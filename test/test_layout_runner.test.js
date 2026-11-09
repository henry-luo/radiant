'use strict';

const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs');
const { spawnSync } = require('node:child_process');
const { once } = require('node:events');
const { test } = require('node:test');
const { RadiantLayoutTester } = require('./layout/test_radiant_layout');

const root = path.resolve(__dirname, '..');
const hostRoot = path.join(root, 'test');

test('layout runner selects a Radiant executable and validates its CLI option', () => {
    const tester = new RadiantLayoutTester({ projectRoot: root });
    assert.equal(tester.radiantExe, path.join(root, 'radiant.exe'));
    const script = path.join(root, 'test/layout/test_radiant_layout.js');
    const help = spawnSync(process.execPath, [script, '--exe', process.execPath, '--help'], { encoding: 'utf8' });
    assert.equal(help.status, 0);
    assert.match(help.stdout, /--exe, --radiant-exe/);
    for (const flag of ['--exe', '--radiant-exe']) {
        const missing = spawnSync(process.execPath, [script, flag], { encoding: 'utf8' });
        assert.notEqual(missing.status, 0);
        assert.match(missing.stderr, /requires an executable path/);
    }
});

test('standalone Radiant layout and render commands', { skip: !process.env.RADIANT_TEST_EXE }, () => {
    const exe = process.env.RADIANT_TEST_EXE;
    const dir = path.join(root, 'temp/radiant-cli-test');
    fs.mkdirSync(dir, { recursive: true });
    const fixture = path.join(root, 'test/layout/data/baseline/baseline_801_display_block.html');
    const tree = path.join(dir, 'layout.json');
    const image = path.join(dir, 'render.png');
    const opts = { cwd: root, encoding: 'utf8', env: {
        ...process.env, LAMBDA_TEMP_DIR: dir, LAMBDA_CACHE_DIR: dir, LAMBDA_LOG_DIR: dir
    } };
    for (const command of ['layout', 'render']) {
        const help = spawnSync(exe, [command, '--help'], opts);
        assert.equal(help.status, 0, help.stderr);
        assert.match(help.stdout, /Usage:/);
    }
    const layout = spawnSync(exe, ['layout', fixture, '--view-output', tree, '--no-log'], opts);
    assert.equal(layout.status, 0, layout.stderr);
    assert.ok(JSON.parse(fs.readFileSync(tree)).layout_tree);
    const render = spawnSync(exe, ['render', fixture, '-o', image, '-vw', '320', '-vh', '200', '--no-log'], opts);
    assert.equal(render.status, 0, render.stderr);
    const png = fs.readFileSync(image);
    assert.equal(png.subarray(1, 4).toString(), 'PNG');
    assert.equal(png.readUInt32BE(16), 320);
    assert.equal(png.readUInt32BE(20), 200);
    for (const [extension, signature] of [['svg', '<svg'], ['pdf', '%PDF-'], ['jpg', '\xff\xd8']]) {
        const output = path.join(dir, `render.${extension}`);
        const exported = spawnSync(exe, ['render', fixture, '-o', output, '-vw', '320', '-vh', '200', '--no-log'], opts);
        assert.equal(exported.status, 0, exported.stderr);
        assert.ok(fs.readFileSync(output).includes(Buffer.from(signature, 'latin1')), extension);
    }
    const invalid = spawnSync(exe, ['render', fixture, '-o', image, '--scale', '0', '--no-log'], opts);
    assert.notEqual(invalid.status, 0);
});

test('external layout hosts use their checkout while artifacts stay in Radiant', async () => {
    const tester = new RadiantLayoutTester({
        projectRoot: root, radiantExe: process.execPath, hostWorkingDir: hostRoot
    });
    const child = tester.spawnLayoutProcess(['-e', 'process.stdout.write(process.cwd())']);
    let output = '';
    child.stdout.on('data', chunk => { output += chunk; });
    const [code, signal] = await once(child, 'close');
    assert.equal(code, 0);
    assert.equal(signal, null);
    assert.equal(output, hostRoot);
    assert.equal(tester.outputFile, path.join(root, 'temp/view_tree.json'));
    assert.equal(tester.processRegistry.size, 0);
});

async function categoryResult({ failed = 0, fullSuiteFailures = failed, missing = false } = {}) {
    const tester = new RadiantLayoutTester({
        projectRoot: root, hostWorkingDir: hostRoot, baselineOnly: true
    });
    tester.prepareCategoryPlanInput = async category => {
        if (missing) throw new Error('missing fixture directory');
        return category;
    };
    tester.buildTestPlan = async categories => ({
        categories: new Map(categories.map(category => [category, {}]))
    });
    const original = RadiantLayoutTester.prototype.testCategory;
    const log = console.log;
    const messages = [];
    console.log = message => { messages.push(message); };
    RadiantLayoutTester.prototype.testCategory = async function(category) {
        assert.equal(this.hostWorkingDir, hostRoot);
        this._categorySummaries = { [category]: {
            category, total: failed + 1, successful: 1, failed, skipped: 0,
            baselineRegressions: 0
        } };
        this._fullSuiteFailures = fullSuiteFailures;
        return Array.from({ length: failed + 1 }, () => ({}));
    };
    try {
        await tester.testCategories(['baseline']);
    } finally {
        RadiantLayoutTester.prototype.testCategory = original;
        console.log = log;
    }
    return { tester, messages };
}

test('unscored geometry failures reach the combined layout gate and report', async () => {
    const { tester, messages } = await categoryResult({ failed: 51 });
    assert.equal(tester._fullSuiteFailures, 51);
    assert(messages.includes('LAYOUT_CATEGORY_RESULT|baseline|FAIL|1|51|0'));
});

test('passing layout categories leave the combined gate successful', async () => {
    const { tester, messages } = await categoryResult();
    assert.equal(tester._fullSuiteFailures || 0, 0);
    assert(messages.includes('LAYOUT_CATEGORY_RESULT|baseline|PASS|1|0|0'));
});

test('accepted scored-baseline partials keep their existing classification', async () => {
    const { tester, messages } = await categoryResult({ failed: 51, fullSuiteFailures: 0 });
    assert.equal(tester._fullSuiteFailures || 0, 0);
    assert(messages.includes('LAYOUT_CATEGORY_RESULT|baseline|PASS|1|0|0'));
});

test('a missing layout category fails the combined gate', async () => {
    const { tester, messages } = await categoryResult({ missing: true });
    assert.equal(tester._fullSuiteFailures, 1);
    assert(messages.includes('LAYOUT_CATEGORY_RESULT|baseline|FAIL|0|1|0'));
});
