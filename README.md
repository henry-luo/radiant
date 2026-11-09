# Radiant

Radiant is the native layout, rendering and UI engine. Under D9.1.1v4 it depends on lambda-input; Lambda supplies optional scripting adapters.

This checkout is being extracted. `radiant/` owns the native sources and `build_targets.json` is the manifest imported by Lambda. Lambda supplies the event controller through native host interfaces; Radiant owns platform event capture, hit testing, layout, rendering and the state store (D9.1.1v4). The full Radiant and input archives link without Lambda runtime or DOM symbols. Vendor dependencies still come from the sibling Lambda checkout and need independent packaging before distribution. See the implementation checkpoint in the sibling Lambda project for current evidence.

`make DEPS_ROOT=../Lambda` compiles the native archives in this project using the existing sibling dependency headers. This archive build alone does not exercise the complete link closure.

`make test-native DEPS_ROOT=../Lambda` runs native host cases and the video and speech provider tests directly against the sibling-built Radiant archive. This focused subset does not qualify the full engine.

For the current migration, clone the companion repositories as siblings named `Lambda`, `lambda-input`, and `radiant`. The `Lambda` checkout supplies the temporary runtime and vendor build dependencies.

On macOS, `make -j8 test-native-closure DEPS_ROOT=../Lambda` force-loads the complete Radiant and input archives without Lambda runtime or DOM libraries. Its client initializes native UI, lays out HTML, and hit-tests HTML and SVG. This checks the native boundary while using the current sibling-built vendor dependencies.
