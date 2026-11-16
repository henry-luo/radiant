# CSS Shapes implementation

Status: core implementation complete, 2026-11-16; aggregate qualification
retains unrelated native link and flex fixture failures. This record describes
a bounded implementation, not complete CSS Shapes conformance.
Authority: native ownership D9.1.1v4 / D9.1.3v2,
selected-view lifetime D5.5.1 and name identity D5.6.1.

## Delivered behavior

`shape-outside`, `shape-margin` and `shape-image-threshold` participate in the
normal shared parse, cascade, substitution and computed-value pipeline. They
are non-inherited, with initial values `none`, `0px` and `0`. Explicit inheritance
uses the existing declaration-owner walk. Invalid declarations cannot displace
an earlier valid declaration. Thresholds accept numbers and percentages and
clamp to [0,1]; pixels must exceed the threshold to contribute to the contour.

Horizontal left/right floats support circles, ellipses, insets, polygons and
margin/border/padding/content reference boxes. Box shapes include border-radius
curvature, including asymmetric elliptical corners. Basic shapes reuse the
parsed/substituted geometry resolver used by clip-path. Shape margins expand
contours by Euclidean distance; percentages use the containing block's
established content width, before its measured content_width is published.

Local and data URL raster images produce alpha-derived outer row bounds at
their used content-box size. The layout pass copies these rows; no borrowed
pixel pointer escapes image decoding. A retained style change rebuilds the
contour, including threshold and margin changes. Image-set selection uses the
existing CSS image chooser for local/data URL candidates.

Line queries consider the whole line band and clip the result to the float's
margin box. Float placement, clear, containment and independent BFC avoidance
continue to use margin boxes, as required by CSS Shapes 1 section 2. A separate
shape-query flag preserves the existing initial-letter clearance policy for
tall pending inline content. Float translations translate their pass-local
contours with the existing BFC geometry refresh.

## Registration prerequisite

The original parser probe found ID 0 and unsupported declarations for all
three names. With the user's explicit scoped ownership exception, Input now
owns the three IDs, property metadata, grammar validation, the missing
margin-box keyword and parser regressions. The name catalog source remains in
Lambda's `utils/well_known_names_data.py`; its three new identities append to
the stable tail, and the existing generator produces Input's name tables,
property mapping and CSS declaration attributes. Existing NameIds are unchanged.
See [LI-065](../../../lambda-input/vibe/Lib_Input_Issue_Ledger.md#li-065).
No second CSS parser or custom-property alias bypasses the shared registry.

## Qualification

- `test/test_css_shapes_gtest.cpp`: 14/14 geometry/image-alpha cases pass.
  Covers full bands, analytic ellipse offsets, sloped polygon margins,
  concavity, degenerate insets, rounded-corner normalization, strict alpha
  comparison, row padding, buffer bounds, copying and source mutation.
- `test/test_css_shapes_layout_gtest.cpp`: 21/21 native integration cases pass.
  Covers real text line starts, variables and percentage margins, right floats,
  retained shape/threshold changes, local/data images, reference boxes,
  margin-box clipping, invalid declarations, float packing, BFC avoidance and
  refusal of remote alpha extraction without CORS admission, plus offset
  formatting contexts in ordinary flow, grid and flex items.
- Input's CSS parser suite: 117/117 pass, including the new Shapes grammar case.
- Layout integer-cast lint and generated name-table freshness pass.
- Input aggregate baseline: 5,312 passed, three skips across 109 executables.
- The runtime-free archive closure passes with every native member force-loaded.
- The full Radiant gate was run. Native aggregation is blocked by RI-091's
  custom-layout duplicate providers. Building/running the independent suites
  yields 51 passing executables, 1,063 passed tests and one skip; the retained
  display-list target separately has twelve pre-existing provider collisions.
  Both failed links remain explicit in the native results inventory.
- With an isolated current-source host, static layout reports 3,850 successful,
  one failure and six skips; scripted layout passes 630/630. The remaining
  static case, `flex_grow_root_minimized`, has the same 33.3% element match with
  the older existing Lambda host. RI-092 records it without changing references.
- The optional scripting host passes `test/html/css_shapes_mutation.html`:
  camelCase writes for all three properties reflow the text from x=90 to x=60,
  and computed style reports `10px` and `0.75` (`SHAPES_JS_PASS`).

Logs are under `temp/css-shapes/`, including `native-geometry-final.log`,
`flex-offset-final.log`, `input-parser-full.log`, `input-baseline.log`,
`native-suites/results.json`, `native-closure-final-visual.log`,
`layout-final-visual-baseline.log` and `js-mutation-final.json`.
Browser references, layout tolerances and scored baselines are unchanged.

## Visual test page

[`test/html/css_shapes_visual.html`](../../test/html/css_shapes_visual.html)
provides eight self-contained comparisons: rectangular wrapping, a circle,
an ellipse, a polygon, a rounded inset, a larger shape margin, and the same
alpha image at low/high thresholds. It uses no script or network resource.
Render it at 1120 by 1500 pixels:

```sh
./radiant.exe render test/html/css_shapes_visual.html -o temp/css-shapes/visual.png --viewport-width 1120 --viewport-height 1500
```

The first render exposed two native float-context defects (RI-093). Assigned
grid contexts subtracted their parent-space origin from coordinates already
relative to the BFC border frame, and ordinary horizontal flex items lacked
an independent float context. Native geometry now uses one BFC frame and
ordinary flex items initialize their assigned context. Five regressions cover
offset flow/grid/flex and rectangular controls; independent Chromium captures
confirm the shaped grid/flex text starts at x=153. The corrected preview is
`temp/css-shapes/visual.png`; before/after evidence remains alongside it.
This follows native ownership D9.1.1v4 / D9.1.3v2 and selected-view lifetime
D5.5.1; it changes no design ruling.

## Host provenance

The first aggregate host reused shared runtime/adapter objects whose dependency
files referenced private benchmark snapshots. It trapped in script-task cleanup;
that run's 748 static/434 scripted runner errors are not current-source evidence.
An isolated host was rebuilt with current Input/Radiant roots and private
object/archive destinations under `temp/css-shapes/current-host`. All 252 new
dependency files have zero snapshot-root references. Generated build files were
not hand-edited and other active snapshot configurations were preserved.
Lambda's issue ledger records the shared-build identity hazard.

The validated host is `temp/css-shapes/current-host/radiant.exe`; an identical
copy is installed as this checkout's `radiant.exe`. To repeat the layout gate
without consulting a shared host configuration, use:

```sh
make test-layout-baseline RADIANT_EXE="$PWD/temp/css-shapes/current-host/radiant.exe"
```

The isolated build commands, archive/executable hashes and source hashes are
retained under `temp/css-shapes/current-host/`. A later default host build can
consume the shared configuration again; select the pinned host for this evidence.

## Remaining coverage

Vertical/sideways float contours, initial-letter shapes, fragmented and paged
contour continuation, geometry animation, generated/gradient image contours,
SVG/animated image semantics and remote image CORS admission are not qualified
by this increment. HTTP(S) alpha extraction is refused until the resource
provider supplies CORS admission. Basic inset rounding retains the shared
clip resolver's uniform-radius profile; complete four-corner/slash rounding,
path()/shape()/rect()/xywh() grammars and fully computed basic-shape CSSOM
serialization remain extensions. Complex concave polygons crossing the margin
box and exhaustive negative-margin/degenerate cases require further comparison.

Specification: https://www.w3.org/TR/css-shapes-1/.
