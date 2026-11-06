# Radiant

Radiant is the native layout, rendering and UI engine. Under D9.1.1v3 it depends on lambda-input; Lambda supplies optional scripting adapters.

This checkout is being extracted. `radiant/` owns the native sources and `build_targets.json` is the manifest imported by Lambda. The engine still has runtime dependencies and the `lambda` and `lib` migration links point to sibling source trees. It is not yet a standalone build. Complete provider separation, direct lambda-input archive consumption, native tests, and vendor dependency packaging before distribution. See the implementation checkpoint in the sibling Lambda project for current evidence.

`make DEPS_ROOT=../Lambda` compiles the native archives in this project using the existing local dependency headers and temporary runtime link. It does not prove that Radiant is independently linkable.

`make test-native DEPS_ROOT=../Lambda` runs four native host cases directly against the sibling-built Radiant archive. This focused subset does not qualify the full engine or its remaining Lambda imports.

For the current migration, clone the companion repositories as siblings named `Lambda`, `lambda-input`, and `radiant`. The `Lambda` checkout supplies the temporary runtime and vendor build dependencies.
