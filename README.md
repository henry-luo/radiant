# Radiant

Radiant is the native layout, rendering and UI engine. Under D9.1.1v4 it depends on lambda-input; Lambda supplies optional scripting adapters.

`radiant/` owns the native sources and `build_targets.json` is the manifest imported by Lambda. Input headers and archives resolve through `INPUT_ROOT` (default `../lambda-input`); Radiant does not link whole `lib` or `lambda` source directories. Radiant owns platform event capture, hit testing, layout, rendering and the state store; Lambda supplies event handling and scripting through native host interfaces (D9.1.1v4, D9.1.3v2).

`make PROFILE=debug` or `make PROFILE=release` builds `libradiant.a` against the matching input profile. `make test-radiant-baseline` runs the owner-local native GTest suites and the layout baseline, even if one group fails. `make test-native` runs only native GTest suites. `make test-layout-smoke` loads every static fixture without a scripting host and checks for a produced view tree. The shared layout and media fixtures use the sibling `lambda-test` checkout. Device-specific cases may require local hardware and are reported as skipped when unavailable.

`make test-layout-baseline` runs that native smoke pass and compares the existing `baseline` and `baseline-js` suites against their browser geometry references using `test/layout/test_radiant_layout.js`. It requires an external Lambda host built against the current Radiant archive; set `LAMBDA_HOST` to select it and `LAMBDA_ROOT` to locate its package/WPT resources (defaults: `../Lambda/lambda.exe` and `../Lambda`). This is a test integration dependency; the native archive and smoke target remain independent of the Lambda runtime (D9.1.1v4, D9.1.3v2). Existing mismatches fail the target. No scored baseline or reference is weakened to make it pass.

Use `LAYOUT_BASELINE_SUITES=baseline` for static geometry alone, `LAYOUT_BASELINE_CONCURRENCY=3` to change the shared process limit, and `ARGS=--json` for the existing runner's JSON summary. The default geometry tolerance remains 5 CSS pixels, matching the RI-004 checks. The comparison report is saved to `temp/layout-baseline/results.log`; host scratch/cache/log files stay under that directory, and corpus-runner scratch stays under `temp/`.

On macOS, `make test-native-closure` force-loads the complete Radiant and input archives without Lambda runtime or DOM libraries. Its client initializes native UI, lays out HTML, and hit-tests HTML and SVG. It checks native linkage separately from the unit tests.

`make test-navigation-cli LAMBDA_HOST=../Lambda/lambda.exe` checks startup fragment navigation and timer retention through an external host's `view` and `layout` commands. The native build and baseline do not require this host. Replay inputs, results and logs stay under `temp/navigation-cli/`.
