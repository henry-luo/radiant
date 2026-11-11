# Radiant

Radiant is the native layout, rendering and UI engine. Under D9.1.1v4 it depends on lambda-input; Lambda supplies optional scripting adapters.

`radiant/` owns the native sources and `build_targets.json` is the manifest imported by Lambda. Input headers and archives resolve through `INPUT_ROOT` (default `../lambda-input`); Radiant does not link whole `lib` or `lambda` source directories. Radiant owns platform event capture, hit testing, layout, rendering and the state store; Lambda supplies event handling and scripting through native host interfaces (D9.1.1v4, D9.1.3v2).

Run `./setup-mac-deps.sh`, `./setup-linux-deps.sh`, or
`./setup-windows-deps.sh` to prepare input and install Radiant's graphics
packages (D9.1.1v4, D9.1.3v2). Set `LAMBDA_INPUT_ROOT` or `INPUT_ROOT` to
select the input checkout. Add `--dry-run` to inspect the steps; Linux and
Windows setup still need native platform verification.

`make PROFILE=debug` or `make PROFILE=release` builds `libradiant.a` against the matching input profile. `make radiant.exe` builds the standalone layout/render executable. It links Lambda's existing runtime and scripting adapters as the executable host, while the Radiant archive remains independent of the runtime. The executable supports `layout`, `render`, and `render-batch`; use `./radiant.exe layout --help` or `./radiant.exe render --help` for options.

`make run-radiant-baseline` runs the owner-local native GTest suites and the layout baseline, even if one group fails. `make test-native` runs only native GTest suites. `make test-layout-smoke` loads every static fixture without a scripting host and checks for a produced view tree. The shared layout and media fixtures use the sibling `lambda-test` checkout. Device-specific cases may require local hardware and are reported as skipped when unavailable.

`make lint ARGS='--rule ^no-int-cast-radiant$'` runs the owner-local layout
integer-cast rule using `ast-grep` (`brew install ast-grep`). Integer exceptions
require a same-line `// INT_CAST_OK: <reason>` comment. `make test-lint` checks
the gate's rejection and suppression behavior. The rule belongs to Radiant
under D9.1.1v4 / D9.1.3v2 and needs no Lambda runtime or Python clang module.

`make test-layout-baseline` builds `radiant.exe` by default, runs the native smoke pass, and compares the existing `baseline` and `baseline-js` suites against browser geometry using `test/layout/test_radiant_layout.js`. The executable's script host uses the configured runtime and adapter archives in `LAMBDA_ROOT` (default `../Lambda`), which also supplies package/WPT resources; it does not invoke `lambda.exe`. Select a different executable in the runner with `--exe <path>` (also `--radiant-exe`), or set `RADIANT_EXE` for the make target. The native archive and smoke target remain independent of Lambda runtime (D9.1.1v4, D9.1.3v2). Existing mismatches fail the target. No scored baseline or reference is weakened to make it pass.

For example, `./radiant.exe layout page.html --view-output temp/page.json` writes a view tree, and `./radiant.exe render page.html -o temp/page.png` renders an image. To run a selected fixture with a specific host, use `node test/layout/test_radiant_layout.js --exe ./radiant.exe --host-cwd ../Lambda -c baseline -t baseline_801_display_block.html`.

Use `LAYOUT_BASELINE_SUITES=baseline` for static geometry alone, `LAYOUT_BASELINE_CONCURRENCY=3` to change the shared process limit, and `ARGS=--json` for the existing runner's JSON summary. The default geometry tolerance remains 5 CSS pixels, matching the RI-004 checks. The comparison report is saved to `temp/layout-baseline/results.log`; host scratch/cache/log files stay under that directory, and corpus-runner scratch stays under `temp/`.

On macOS, `make test-native-closure` force-loads the complete Radiant and input archives without Lambda runtime or DOM libraries. Its client initializes native UI, lays out HTML, and hit-tests HTML and SVG. It checks native linkage separately from the unit tests.

`make test-navigation-cli LAMBDA_HOST=../Lambda/lambda.exe` checks startup fragment navigation and timer retention. Its layout cases use `radiant.exe`; its view cases use the selected Lambda host because the standalone executable does not provide `view`. The native build and baseline do not require the external executable. Replay inputs, results and logs stay under `temp/navigation-cli/`.
