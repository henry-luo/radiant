# Radiant

Radiant is the native layout, rendering and UI engine. Under D9.1.1v4 it depends on lambda-input; Lambda supplies optional scripting adapters.

`radiant/` owns the native sources and `build_targets.json` is the manifest imported by Lambda. Input headers and archives resolve through `INPUT_ROOT` (default `../lambda-input`); Radiant does not link whole `lib` or `lambda` source directories. Radiant owns platform event capture, hit testing, layout, rendering and the state store; Lambda supplies event handling and scripting through native host interfaces (D9.1.1v4, D9.1.3v2).

`make PROFILE=debug` or `make PROFILE=release` builds the native archives against the matching input profile. `make test-radiant-baseline` builds and runs the owner-local native GTest suites against the archives; `make test-native` is an equivalent entry point. The shared layout and media fixtures use the sibling `lambda-test` checkout. Device-specific cases may require local hardware and are reported as skipped when unavailable.

On macOS, `make test-native-closure` force-loads the complete Radiant and input archives without Lambda runtime or DOM libraries. Its client initializes native UI, lays out HTML, and hit-tests HTML and SVG. It checks native linkage separately from the unit tests.
