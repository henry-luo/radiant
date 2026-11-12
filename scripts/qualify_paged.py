#!/usr/bin/env python3
"""Collect paged correctness and provenance receipts without updating references."""
import argparse
import ctypes
import ctypes.util
import hashlib
import json
import os
import platform
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ('layout_paged.cpp', 'view_tree_css.cpp', 'typeset.cpp', 'typeset_regions.cpp')
SUPPORTING = ('layout_containing_block.cpp', 'layout_pass.cpp', 'layout.hpp', 'view_tree_css.hpp', 'layout_flex_multipass.cpp', 'layout_grid_multipass.cpp')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def source_inventory(revision=None, names=SOURCES):
    result = {}
    for name in names:
        path = ROOT / 'radiant' / name
        exists = path.exists()
        if revision:
            snapshot = subprocess.run(['git', 'show', revision + ':' + str(path.relative_to(ROOT))],
                                      cwd=ROOT, capture_output=True)
            exists, data = snapshot.returncode == 0, snapshot.stdout if snapshot.returncode == 0 else b''
        else:
            data = path.read_bytes() if exists else b''
        result[str(path.relative_to(ROOT))] = {
            'exists': exists, 'lines': len(data.splitlines()), 'sha256': sha(data)}
    return result


def sample_pixels(pixels, width, height, witnesses, channels):
    samples = []
    for witness in witnesses:
        x, y = witness['x'], witness['y']
        if not (0 <= x < width and 0 <= y < height):
            raise RuntimeError('reference witness is outside the decoded image')
        offset = (y * width + x) * channels
        rgba = list(pixels[offset:offset + channels])
        if channels == 3:
            rgba.append(255)
        passed = rgba == witness['rgba'] if 'rgba' in witness else rgba != witness['not_rgba']
        samples.append({'x': x, 'y': y, 'rgba': rgba, 'passed': passed})
    return samples


class PngImage(ctypes.Structure):
    # libpng's public simplified png_image ABI, version 1 (png.h).
    _fields_ = [('opaque', ctypes.c_void_p), ('version', ctypes.c_uint32),
                ('width', ctypes.c_uint32), ('height', ctypes.c_uint32),
                ('format', ctypes.c_uint32), ('flags', ctypes.c_uint32),
                ('colormap_entries', ctypes.c_uint32), ('warning_or_error', ctypes.c_uint32),
                ('message', ctypes.c_char * 64)]


class PngReader:
    def __init__(self):
        library = ctypes.util.find_library('png16') or ctypes.util.find_library('png')
        if not library:
            raise RuntimeError('libpng is required for decoded preview comparison')
        self.lib = ctypes.CDLL(library)
        self.lib.png_image_begin_read_from_file.argtypes = [ctypes.POINTER(PngImage), ctypes.c_char_p]
        self.lib.png_image_begin_read_from_file.restype = ctypes.c_int
        self.lib.png_image_finish_read.argtypes = [ctypes.POINTER(PngImage), ctypes.c_void_p,
                                                   ctypes.c_void_p, ctypes.c_int32, ctypes.c_void_p]
        self.lib.png_image_finish_read.restype = ctypes.c_int
        self.lib.png_image_free.argtypes = [ctypes.POINTER(PngImage)]
        self.lib.png_image_free.restype = None

    def read(self, path, limit, witnesses):
        image = PngImage()
        image.version = 1
        try:
            if not self.lib.png_image_begin_read_from_file(ctypes.byref(image), os.fsencode(path)):
                raise RuntimeError(image.message.decode(errors='replace'))
            image.format = 3  # PNG_FORMAT_RGBA: color plus alpha, eight bits per channel.
            size = image.width * image.height * 4
            if not size or size > limit:
                raise RuntimeError('decoded preview exceeds the declared raster-byte budget')
            pixels = (ctypes.c_ubyte * size)()
            if not self.lib.png_image_finish_read(ctypes.byref(image), None, pixels, 0, None):
                raise RuntimeError(image.message.decode(errors='replace'))
            return {'width': image.width, 'height': image.height, 'rgba_sha256': sha(bytes(pixels)),
                    'samples': sample_pixels(pixels, image.width, image.height, witnesses, 4)}
        finally:
            self.lib.png_image_free(ctypes.byref(image))


def command(argv, log, env):
    result = subprocess.run([str(x) for x in argv], cwd=ROOT, env=env, capture_output=True)
    log.write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError('command failed (%d): %s; see %s' % (result.returncode, argv, log))
    return result.stdout.decode(errors='replace')


def pdf_receipt(path, prefix, env, witnesses):
    info = command(['pdfinfo', path], prefix.with_suffix('.info.log'), env)
    match = re.search(r'^Pages:\s+(\d+)', info, re.M)
    if not match:
        raise RuntimeError('pdfinfo returned no physical page count')
    count = int(match.group(1))
    if any(not 1 <= w['page'] <= count for w in witnesses):
        raise RuntimeError('PDF color witness names a missing physical page')
    boxes = command(['pdfinfo', '-f', '1', '-l', str(count), '-box', path],
                    prefix.with_suffix('.boxes.log'), env)
    geometry = [line.strip() for line in boxes.splitlines()
                if re.match(r'^Page\s+\d+\s+(size|rot|MediaBox|CropBox|BleedBox|TrimBox|ArtBox):', line)]
    if not geometry:
        raise RuntimeError('pdfinfo returned no physical sheet geometry')
    raster_prefix = prefix.parent / (prefix.name + '-raster')
    command(['pdftoppm', '-r', '96', path, raster_prefix], prefix.with_suffix('.raster.log'), env)
    rasters = sorted(prefix.parent.glob(raster_prefix.name + '-*.ppm'),
                     key=lambda p: int(p.stem.rsplit('-', 1)[1]))
    if len(rasters) != count:
        raise RuntimeError('independent PDF raster inventory is incomplete')
    samples = []
    for page, raster in enumerate(rasters, 1):
        header = raster.read_bytes().split(b'\n', 3)
        if len(header) != 4 or header[0] != b'P6' or header[2] != b'255':
            raise RuntimeError('unexpected Poppler PPM header')
        width, height = map(int, header[1].split())
        if len(header[3]) != width * height * 3:
            raise RuntimeError('incomplete Poppler PPM pixels')
        selected = [w for w in witnesses if w['page'] == page]
        samples.extend(dict(sample, page=page) for sample in sample_pixels(header[3], width, height, selected, 3))
    # Poppler emits canonical uncompressed PPM; compare complete bytes, including dimensions.
    return {'pages': count, 'geometry': geometry, 'poppler_ppm_sha256': [sha(p.read_bytes()) for p in rasters],
            'samples': samples}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, help='the exact native host to qualify')
    parser.add_argument('--composition-exe', type=Path, help='optimized native composition benchmark')
    parser.add_argument('--samples', type=int, default=7)
    parser.add_argument('--manifest', type=Path, default=ROOT / 'test/fixtures/paged/cases.json')
    parser.add_argument('--output', type=Path, default=ROOT / 'temp/paged-qualification')
    parser.add_argument('--max-raster-bytes', type=int, default=512 * 1024 * 1024)
    args = parser.parse_args()
    if not 1 <= args.samples <= 1000:
        parser.error('--samples must be between 1 and 1000')
    output = args.output.resolve()
    if not output.is_relative_to((ROOT / 'temp').resolve()):
        parser.error('qualification artifacts must stay under this checkout\'s temp/')
    if output.exists() and any(output.iterdir()):
        parser.error('use a fresh output directory; existing qualification receipts are immutable')
    output.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(__file__, output / 'collector.py')
    sources = source_inventory()
    supporting = source_inventory(names=SUPPORTING)
    report = {'scope': 'source inventory', 'sources': sources, 'supporting_sources': supporting, 'cases': [],
              'source_relationship': 'observed checkout; executable provenance must be supplied by its build receipt',
              'collector_sha256': sha(Path(__file__).read_bytes()), 'platform': platform.platform(), 'python': sys.version,
              'historical_comparison': {'status': 'waived', 'date': '2026-11-12',
                  'reason': 'user confirmed the original archive is lost; historical values are not replayed'},
              'unqualified': ['matched release composition performance', 'other platforms']}
    failure = None
    try:
        report['head'] = command(['git', 'rev-parse', 'HEAD'], output / 'head.log', dict(os.environ)).strip()
        report['head_sources'] = source_inventory(report['head'])
        report['source_lines'] = {'current': sum(item['lines'] for item in sources.values()),
            'head': sum(item['lines'] for item in report['head_sources'].values())}
        retired = ('PagedFlowNode', 'PagedFrame', 'PagedComposer', 'PagedCheckpoint', 'RADIANT_PAGED_ENGINE')
        report['retired_symbols_present'] = {symbol: any(symbol in (ROOT / path).read_text()
            for path, item in sources.items() if item['exists']) for symbol in retired}
        patch = subprocess.check_output(['git', 'diff', '--binary'], cwd=ROOT)
        (output / 'working.patch').write_bytes(patch)
        report['working_diff_sha256'] = sha(patch)
        if args.composition_exe:
            original = args.composition_exe.resolve()
            bench = output / 'composition-host'
            bench_hash = sha(original.read_bytes())
            shutil.copy2(original, bench)
            if sha(bench.read_bytes()) != bench_hash:
                raise RuntimeError('composition host changed during capture')
            report['composition'] = {'host_sha256': bench_hash, 'frozen_path': str(bench),
                'scope': 'fresh optimized baseline; parsing, export, hashing and retirement excluded',
                'client_sha256': sha((ROOT / 'test/paged_composition_bench.cpp').read_bytes()),
                'build_signature': json.loads((ROOT / 'build/radiant-release-build.json').read_text()), 'workloads': []}
            for pages in (21, 300):
                workload = output / ('composition-%d.html' % pages)
                text = 'A paged document retains its source text, selected widths and physical sheet geometry through ordinary block and paragraph layout. '
                articles = ''.join('<article><h1>Chapter %d</h1>%s</article>' %
                    (i + 1, ''.join('<p>' + text * 2 + '</p>' for _ in range(6))) for i in range(pages))
                workload.write_text('<!doctype html><html><head><style>@page{size:640px 800px;margin:40px}'
                    'html,body{margin:0;font:16px/20px monospace}article{break-after:page}'
                    'article:last-child{break-after:auto}h1{font-size:24px;line-height:28px;margin:0 0 12px}'
                    'p{margin:0 0 10px}</style></head><body>' + articles + '</body></html>\n')
                result = json.loads(command([bench, workload, str(args.samples)],
                    output / ('composition-%d.log' % pages), dict(os.environ)))
                samples = result['samples']
                if result['profile'] != 'optimized-NDEBUG' or len(samples) != args.samples or any(
                        sample['ns'] <= 0 or sample['pages'] != pages for sample in samples):
                    raise RuntimeError('optimized composition returned invalid page/timing samples')
                if len({sample['edition_fnv1a64'] for sample in samples}) != 1:
                    raise RuntimeError('composition samples changed published geometry or glyph material')
                report['composition']['workloads'].append(dict(result, source_sha256=sha(workload.read_bytes()),
                    source=str(workload), expected_pages=pages,
                    median_ms_per_page=statistics.median(sample['ns'] for sample in samples) / pages / 1000000))
            report['unqualified'].remove('matched release composition performance')
            report['composition']['comparison'] = 'no historical speedup claim; archived baseline waived'
        if args.exe:
            original = args.exe.resolve()
            host_hash = sha(original.read_bytes())
            exe = output / ('host' + original.suffix)
            shutil.copy2(original, exe)
            if sha(exe.read_bytes()) != host_hash:
                raise RuntimeError('host changed while its immutable copy was captured')
            report['executable'] = {'path': str(original), 'frozen_path': str(exe), 'sha256': host_hash}
            manifest = args.manifest.resolve()
            data = json.loads(manifest.read_text())
            report['manifest_sha256'] = sha(manifest.read_bytes())
            shutil.copyfile(manifest, output / 'manifest.json')
            report['scope'] = data['scope']
            names = [case['name'] for case in data['cases']]
            if not names or len(set(names)) != len(names) or any(not re.fullmatch(r'[A-Za-z0-9_-]+', n) for n in names):
                raise ValueError('manifest needs a nonempty inventory with unique safe case names')
            reader = PngReader()
            env = dict(os.environ)
            for case in data['cases']:
                if not case['preview_witnesses'] or not case['pdf_witnesses']:
                    raise ValueError('every visual case needs literal preview and PDF color witnesses')
                entry = {'name': case['name'], 'oracle': case['oracle'], 'outputs': []}
                report['cases'].append(entry)
                for kind in ('input', 'reference'):
                    source = (ROOT / case[kind]).resolve()
                    before = sha(source.read_bytes())
                    frozen_source = output / (case['name'] + '-' + kind + source.suffix)
                    shutil.copyfile(source, frozen_source)
                    if sha(frozen_source.read_bytes()) != before:
                        raise RuntimeError('fixture changed during immutable source capture')
                    prefix = output / (case['name'] + '-' + kind)
                    png, pdf = prefix.with_suffix('.png'), prefix.with_suffix('.pdf')
                    for target in (png, pdf):
                        command([exe, 'render', source, '--paged', '--block-remote-resources', '-o', target],
                                Path(str(target) + '.log'), env)
                    record = {'source': str(source), 'frozen_source': str(frozen_source), 'source_sha256': before,
                              'preview': reader.read(png, args.max_raster_bytes, case['preview_witnesses']),
                              'pdf': pdf_receipt(pdf, prefix, env, case['pdf_witnesses'])}
                    if sha(source.read_bytes()) != before:
                        raise RuntimeError('source changed during qualification: ' + str(source))
                    entry['outputs'].append(record)
                actual, expected = entry['outputs']
                entry['preview_equal'] = actual['preview'] == expected['preview']
                entry['pdf_geometry_equal'] = actual['pdf']['geometry'] == expected['pdf']['geometry']
                entry['pdf_pixels_equal'] = actual['pdf']['poppler_ppm_sha256'] == expected['pdf']['poppler_ppm_sha256']
                entry['pages_expected'] = actual['pdf']['pages'] == expected['pdf']['pages'] == case['pages']
                entry['literal_witnesses_pass'] = all(sample['passed'] for item in entry['outputs']
                    for image in ('preview', 'pdf') for sample in item[image]['samples'])
                entry['passed'] = all(entry[k] for k in ('preview_equal', 'pdf_geometry_equal', 'pdf_pixels_equal', 'pages_expected', 'literal_witnesses_pass'))
            if sha(exe.read_bytes()) != host_hash or source_inventory() != sources or source_inventory(names=SUPPORTING) != supporting:
                raise RuntimeError('host or audited native sources changed during qualification')
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        failure = str(error)
    report['error'] = failure
    report['passed'] = not failure and all(case.get('passed', False) for case in report['cases'])
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'scope': report['scope'], 'passed': report['passed'], 'cases': len(report['cases']),
                      'unqualified': report['unqualified'], 'error': failure}))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
