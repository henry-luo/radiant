# Radiant Issue Ledger — Fixed Issues

Resolved native Radiant issues under D9.1.1v4 and D9.1.3v2. Entries retain
their original IDs, reproduction evidence, fixes and verification limits.
Active issues remain in the [Radiant Issue Ledger](Radiant_Issue_Ledger.md).

Verification for RI-013/RI-014/RI-015 (2026-11-11): Lambda's final
`make test-radiant-baseline` exits 0 with **8,163 passed / 299 partial / 20 skipped**
of 8,482 cases and no failures. Owner `make test-native` passes 902 tests with
one unrelated, reproducible speech-provider failure recorded as RI-016.
The Lambda core gate passes 7,412/7,413, with its separate watercolor timeout
recorded as LR12-45. References, tolerances, deadlines and expected outputs
are unchanged. Full evidence is under `../../lambda/temp/kianhup-style/`.

## Fixed issues

<a id="ri-089"></a>

### RI-089 — Min-content minimum widens floated intrinsic contribution boxes

**Status:** FIXED, 2026-11-14. **Lambda blocker:** cleared. Native ownership
is D9.1.1v4 / D9.1.3v2; measured layout state retains D5.5.1 ownership. The user
explicitly authorized the Radiant source update from the Lambda task.

The unchanged `keyword-sizes-for-intrinsic-contributions` case had two floated
`.test.min-width` boxes at 90px/150px instead of 70px. Their children used
`width:0;min-width:min-content` and correctly laid out at 60px, but contributed
the larger natural max-content width to the containing float. A definite
preferred width skipped the early shortcut when intrinsic limits required
content measurement and was never restored after that measurement.

Intrinsic measurement now retains the definite border-box preferred width
separately from the natural min/max-content bounds. Shared minimum and maximum
clamps constrain its contribution, with a minimum taking precedence over a
smaller maximum. `width:auto` retains the RI-074 repair. `fit-content` limits
keep negotiating available space through their existing contribution path;
the first candidate's fit-content regressions were repaired before delivery.

The new native definite-contribution regression fails before and passes after
repair. It checks nine min/max combinations in three containing widths with
repeated recascade/layout; the additional fit-content control checks both
minimum and maximum limits. Independent Chromium agrees with all nine width
combinations and all four fit-content box measurements. The complete owner
dynamic suite passes **110/110**; layout lint checks **89 casts, zero unmarked**.

The refreshed debug Lambda host passes sizing **776/776**. The standard
`make test-radiant-baseline` exits 0 with **8,170 passed / zero failed /
297 partial / 20 skipped**. The previously repaired Lambda-owned paint and
cascade integration executables remain green at **250/250** and **186/186**.
No external fixture, reference, tolerance or scored baseline was changed.

Source identity: Lambda `09a40dfe2`, Radiant `4ec1e8c` plus this implementation
and test, Input `3e14a05`. Original and final integration evidence is in
`../../Lambda-opus/temp/radiant-three-failures/`, especially
`ri089-radiant-baseline.log`, `sizing-final-results.json`,
`browser-control-final.json` and `ri089-verification-identity.json`.
Owner evidence is in `../temp/ri-089-before.log`,
`../temp/ri-089-dynamic-final.log` and `../temp/ri-089-lint-final.log`.


<a id="ri-087"></a>

### RI-087 — Propagated body overflow clips its own zero-height box

**Status:** FIXED for the selected clipping/coordinate milestone, 2026-11-14.
**Lambda blocker:** cleared for the saved Mymind main-content omission.
Native ownership D9.1.1v4 / D9.1.3v2; declaration reads follow D5.5.1.

A zero-height body with overflow scroll/hidden/auto clips its absolutely
positioned content to an empty box. Its overflow instead belongs to the
viewport when the HTML root has visible overflow on both axes. The source
body then uses visible overflow locally ([CSS Overflow §3.3](https://www.w3.org/TR/css-overflow-3/#overflow-propagation)).
Root/body containment disables this transfer; ordinary boxes keep their clips.
The existing iframe viewport selector also missed those containment guards.

One shared `view_viewport_overflow_source` selects the root or first displayed
body with those guards. Viewport overflow resolution reuses it. Shared clip
geometry/axes, raster scroll scopes and scrollbar paint, native hit testing
and coordinate scroll scopes skip the source body's local overflow. Declared
CSS remains intact. No page condition, stylesheet modification, changed frame,
threshold or budget is used; no public structure layout changes.

The new native propagation case fails before repair; five focused native
cases and the existing clip/hit regression pass afterward. Exact PNG controls
change from5/8 to8/8, with all eight independently confirmed by Chromium.
Cases include scroll/hidden/auto transfer, explicit root overflow, root style
containment, body layout/paint containment and ordinary-box clipping. The
unchanged saved Mymind source now displays its heading, copy and actions.
RI-086 alone repaired root geometry but left the pixel control blank; that
intermediate host was held. RI-088 remains an independent unresolved query error.
Evidence: Lambda `temp/online-visual-round38/`, propagation native/PNG before
and final logs, browser-body-overflow.json and source freezes. Integer lint
passes. The later live canonical capture remains blank and is not cleared;
its initialization state is a separate unresolved cause. Full baselines and
catalog capture are deferred under focused checking. The user cleared the goal
while the completed repair was being wrapped up.

<a id="ri-086"></a>

### RI-086 — Empty-child root collapse erases resolved minimum height

**Status:** FIXED for the selected final root calculation, 2026-11-14.
**Lambda blocker:** cleared for root geometry; RI-087 separately fixes painting.
Native ownership D9.1.1v4 / D9.1.3v2; scalar sizing follows D5.5.1.

Mymind's min-height100% resolves to800px, but the late all-self-collapsing-child
shortcut replaces its already-constrained root height with0. A border-box
control similarly falls from800 to14px. The shortcut now reuses the existing
`layout_apply_min_max_axis` resolver and retains the larger content overflow
extent. It does not replace collapsed-margin handling or make auto height definite.

The native regression fails five height assertions before repair. Six live
states then agree with independent Chromium geometry: percentage and fixed
minima, conflicting calculated minimum/maximum, zero minimum, border-box
padding/border and restoration. The empty-body margin control retains20px
root height and10px body Y. Both source styles and capture clocks are unchanged.
Evidence: Lambda `temp/online-visual-round38/`, native-before/native-focused,
browser-root-min.json and the minimum-only held host/source pin. The final
combined qualification includes the additional RI-087 viewport controls.


<a id="ri-085"></a>

### RI-085 — Wide SVG Gaussian kernels exhaust the work budget

**Status:** FIXED for the existing wide Gaussian path, 2026-11-14.
**Lambda task blocker:** cleared for Overflow's missing colored SVG background;
other page residuals are reported separately. Native ownership D9.1.1v4 /
D9.1.3v2; temporary window storage and work admission follow D5.5.1.

Overflow's two single-node Gaussian graphs use standard deviations80 and130.
Their5285×3322 and6229×2896 raster grids exhaust the unchanged805,306,368-unit
work limit while convolving each varying pixel over every kernel tap. RI-082's
exact flat-run reuse cannot remove that radius-dependent work on gradients.
Failure provenance now records the node, grid, density and used/allowed work.

For physical sigma>=2, the existing SVG path now uses the three sliding box
passes permitted by [Filter Effects §9.14](https://www.w3.org/TR/filter-effects-1/#feGaussianBlurElement).
Odd windows remain centered; the first two even windows balance their half-pixel
offsets. Virtual line halos extend the original none/duplicate/wrap input once
and survive all three passes. X and Y deviations remain independent. Smaller
kernels and the public exact Gaussian reference routine retain their previous
math. Fully transparent input has an admitted scan and returns its existing
zero surface. No budget, capture frame, resource admission or page condition
changes; this is an algorithm change within an existing filter feature.

One shared strided box-window span now serves both SVG and the existing CSS
shadow passes. The original CSS edge renormalization and integer truncation
are preserved, with widened sums. Window scratch is line-sized and memory/work
admitted before evaluation. SVG accounting includes extension, initial windows,
sliding updates, output writes and line copies.

The new native regression fails before repair at sigma40 under a linear work
budget. After repair it covers18 combinations of deviation, edge and axis,
compares against the independent exact Gaussian within the specified3% envelope,
and retains low-budget rejection. Literal span values verify asymmetric windows,
strides, truncation, renormalization and untouched output. Nine initial focused
tests pass; the milestone extends this once to21 filter, shadow and budget tests,
all passing. Four independent Chromium controls (including even windows and
anisotropic blur) differ by at most3 channel values, with mean errors0.027%–0.098%.
Layout integer lint passes. No public structure layout changes.

The identical saved Overflow source restores its colored hero with no filter
failure and completes in1.65s versus the diagnostic failing2.64s run. These are
capture observations, not a broad performance benchmark. Evidence: Lambda
`temp/online-visual-round37/`, native-before/native-first/native-milestone logs,
Chromium control comparison, diagnostic and repaired source pins, and selected
capture audit. Full native/layout/JS/Input baselines and catalog recapture remain
deferred; original Chromium references and D10.1.7 clocks are retained.


<a id="ri-084"></a>

### RI-084 — Static effect contexts lose positioned descendants and empty-box backdrops

**Status:** FIXED for the selected block-context traversal, 2026-11-14.
**Lambda task blocker:** the reduced opacity defect is cleared; no online page
clearance is inferred. Native ownership D9.1.1v4 / D9.1.3v2; display-list resource
copies and temporary borrows follow D5.5.1.

An absolute child beneath a static opacity parent uses an external containing
block. The zero-level paint list followed only that block's `first_abs_child`
chain, painting the child after the opacity group closed. SVG export confirms
the colored rectangle outside the empty opacity group. Paint ownership now
chooses the nearest block stacking context before the containing block. The
original containing block still supplies coordinates. Nested contexts and
positive/negative layers retain their existing traversal. Inline effect-context
ownership is outside this repair's acceptance scope.

Raster/export/hit consumers share the sorted positioned visitor and reuse their
existing ancestry replay. Raster's former bridge ignored the walker's coordinates;
using its existing direct stacking path preserves the containing-block basis.
Overflow and scrolling below an external absolute/fixed containing block do not
apply, while containing-block and outer clips remain. Chromium independently
confirms that escape rule; the initial local clip expectation was corrected
before promotion, without weakening an existing tolerance.

A separate zero-height group had no backdrop admitted at begin, so its descendant
ink composited at full opacity. Completed ink bounds now admit the required
backdrop prefix before replay. Shared `dl_prepend_tail` reuses existing resource
copy/replacement helpers and also replaces PaintIR's duplicated staging shape.
No public structure layout, page-specific condition, capture frame, budget or
admission change is introduced.

The previous qualified binary passes two and fails six of the final eight PNG
controls. The final binary passes all eight, including transparent and half
opacity, overlapping children composited once, fixed/positive-z children, a local
relative containing block and ordinary overflow escape. Four native ownership,
paint-order, clip and hit tests pass; three focused display-list resource/mask
checks and all eight independent Chromium controls pass. Layout integer lint
has no violations. Two intermediate candidates were held after focused pixel
failures and were not promoted.

Identical saved Metronome and Highnote sources have unchanged pixels before/after;
their visible residuals are not assigned to this repair. No canonical captures,
scores or references were changed. Evidence: Lambda `temp/online-visual-round36/`,
source hash freeze, native-before/final, png-before-final/png-final, browser-opacity,
rdt-focused and lint-final logs. Full baselines and catalog capture are deferred.

<a id="ri-083"></a>

### RI-083 — Early flex intrinsic widths omit deferred scalar gaps

**Status:** FIXED for the selected declared-length path, 2026-11-14.
**Lambda task blocker:** cleared for Affinity button-row/nav sizing; independent
card animation/font differences remain. Native ownership D9.1.1v4 / D9.1.3v2;
bounded owner-aware declaration queries follow D5.5.1.

A wrapping row inside an automatic-width flex column sized two100px items at
200px, omitting its16px variable gap. Final layout resolves that gap and wraps
the second item, producing96px height instead of40px. A literal16px control
already uses216px and stays on one line. Affinity has the same root: its parent
uses344px (the sum of its buttons), omits16px, and stacks the actions.

`layout_flex_declared_length` accepted only a raw CSS length and silently
ignored `var()` and math functions. Its font-relative lengths also used the
caller's font. It now reuses `resolve_element_definite_length_value` to consume
the declaring element's substituted value and font. The existing definite-only
contract excludes cyclic percentages. One nested measurement call now retains
its available context rather than passing null. No duplicate variable/math
resolver, page override, public ABI or structure change is introduced.

The native regression fails20 assertions before the fix. Four focused native
cases pass afterward, including existing nested-wrap, ratio-height and margin
controls. Chromium confirms all five cold/live states:16px variable,24px variable,
calc(6px +10px),.8em at20px, and restoration. Layout integer lint has no violations.
The identical saved Affinity HTML/CSS now lays out the actions in one row and
stops clipping the navigation labels at the top. Additional media loaded from
cache during the later run is not attributed to this sizing fix. Font and card
motion differences remain; no broad page-fidelity completion is claimed.
Evidence: Lambda `temp/online-visual-round35/`, gap-control/fixed views,
native-before.log, native-focused.log, browser-gap.json and affinity-static/final
PNGs. Full baselines and catalog recapture are deferred under focused verification.


<a id="ri-082"></a>

### RI-082 — Gaussian work accounting charges skipped flat-run convolutions

**Status:** FIXED for accounting and transparent runs, 2026-11-14.
**Lambda task blocker:** Overflow's complete filter graph remains unresolved.
Native ownership D9.1.1v4 / D9.1.3v2; filter-run borrows and scratch D5.5.1.

The Gaussian kernel already shares an exact convolution across flat pixel
runs, but its caller precharged every output pixel times every kernel tap.
A constant 128×128 image with sigma1.5 fails under16 work units per pixel even
though the exact optimized kernel fits. The private budgeted kernel now charges
linear channel/run scans, output writes and kernel setup separately, then each
convolution immediately before executing it. The public API remains unchanged.
No cap, admission profile, image bounds, frame clock or approximation changes.

A related redundant-work path treated transparent image edges as boundaries
that always require convolution, even when zero extension makes every sample
zero. The existing flat-run optimization now joins transparent boundary runs
with zero extension and skips the exactly-zero kernel. Nonzero and duplicate/
wrap edge math retains its prior behavior. This also handles a transparent
128×128 image with sigma80, wider than the image, under the same linear budget.

Five focused native tests pass. The new test compares unlimited and budgeted
pixels byte for byte across none/duplicate/wrap modes, checks the wide zero
kernel, and confirms that noisy input still rejects the same budget with an
existing shared charge. Existing independent Gaussian pixels, channel/pitch
and edge-extension checks, shared graph accounting, and memory reclamation/
oversized-grid rejection pass. Evidence: Lambda `temp/online-visual-round34/`,
native-before.log, native-focused.log, native-final.log and source hash freezes.

The saved Overflow page still reports two generic filter memory/work failures
with both candidates, and its PNG is unchanged between them. The remaining
rejection phase has not been isolated; this repair does not clear the page.
Stored logs show filter failures on Overflow, Calendly, FramerAuth and Pitch,
which are candidates rather than claimed beneficiaries. No canonical capture
or comparison score is refreshed without demonstrated page improvement. Full
baselines and catalog captures are deferred under focused verification.


<a id="ri-081"></a>

### RI-081 — Wrapped flex height queries omit item cross margins

**Status:** FIXED for the selected intrinsic query, 2026-11-14.
**Lambda task blocker:** cleared for Betterway hero placement. Native ownership
D9.1.1v4 / D9.1.3v2; scalar queries retain geometry under D5.5.1.

Betterway's heading started 80px too high, partially hidden behind its header.
A local reduction has three 40px full-width flex items with 20px bottom margins
inside a wrapping row with 10px padding, nested in a stretched parent. The
container eventually reaches the correct 200px height, but its items retain a
30px upward offset. Their individual hypothetical border/outer cross sizes
are already correct (40/60px). The scalar wrapping-height query omitted the
margins, causing the parent to allocate only 140px and center the lines in too
little space before late content growth.

The wrapping query now includes each child's non-collapsing vertical margins.
It reuses the existing signed/percentage/auto resolver; the existing fieldset
and ordinary-flow fallback are folded into that resolver to avoid a third
copy. No alternate alignment rule, site CSS, source mutation, horizontal-size
change, budget or capture-clock adjustment is introduced.

The new native regression fails three positions before the fix and passes
afterward across live 20px, 10%, -10px and restored 20px margins. It also checks
read-only intrinsic queries and the unchanged 40px item height. Chromium
independently agrees with all four states. Four focused native cases pass,
including existing nested wrapping, flexed-width and RI-080 controls; layout
integer lint has zero violations. The identical saved Betterway HTML renders
its complete hero heading at Chromium's position. Evidence: Lambda
`temp/online-visual-round33/`, native-before.log, native-focused.log,
browser-control.json and betterway-current/final.png. No ABI/structure change;
full baselines and catalog recapture are deferred under the user's focused
verification instruction.


<a id="ri-080"></a>

### RI-080 — Nested ratio height queries consume parent free space and stale SVG heights

**Status:** FIXED for the selected preferred-size queries, 2026-11-14.
**Lambda task blocker:** no after repair. Native ownership is D9.1.1v4 /
D9.1.3v2; scalar queries preserve selected geometry and scratch lifetime under
D5.5.1.

Isomeet's 16px ratio icon made a nested flex button approximately 176px high,
versus Chromium's 48px reduction. Height queries applied preferred/natural ratios
before resolving the child's declared width, using the parent's free space.
The existing width-expression/min/max resolution now runs before ratio and
replaced-size contributions, after the definite-height fast return. Assigned
used widths retain their existing priority.

The non-square control exposed two connected SVG paths: a percentage geometry
height could be replaced by cached used height or a natural-ratio fallback,
and a ratio-transferred parent height was still treated as cyclic. The shared
SVG percentage-axis helper now consumes a definite height before those
fallbacks, retains automatic sizing for an indefinite basis, and preserves CSS
precedence and existing box/min/max handling. Cyclic fallback applies only when
no definite child-height basis exists. No structure or public ABI change.

Qualification: seven new exported PNG controls improve from 3/7 to 7/7; all
seven independently pass Chromium. Seven existing SVG percentage controls also
pass. The first 6/7 renderer was held for the non-square failure. Four selected
native cases pass, including live 16/24/16px mutations, a 2:1 ratio, alternate
query widths and geometry preservation, plus the existing width-expression,
computed-inheritance and SVG percentage regressions. Integer lint checks 89
casts with zero unmarked. The saved Isomeet page restores the launch badge,
normal form control size and hero placement. Evidence: Lambda's
`temp/online-visual-round32/`; tracked controls are
`test/online/nested_ratio_height_fixtures.cjs` in Lambda. Broad layout/paint/JS/
Input baselines were deferred under the user's focused-verification instruction.


<a id="ri-079"></a>

### RI-079 — Unmatched stylesheet icons exhaust resource discovery

**Status:** FIXED for selected-cascade discovery, 2026-11-14.
**Lambda task blocker:** no after repair. Native ownership is D9.1.1v4 /
D9.1.3v2; temporary substitution values retain D5.5.1 lifetimes.

Conductor's 11 stylesheets contain over 700 URL declarations, mostly icon
classes absent from its 1,124-element document (36 icon classes). The loader
prefetched every raw stylesheet declaration before cascade, including unmatched
and inactive rules. It reached the unchanged 500-resource limit before the
hero, logo images and script consumers loaded. Storetasker also has recorded
capacity rejections among the remaining high-severity pages.

Discovery now reads selected element and pseudo-element style trees after
cascade publication, including live selector/CSSOM changes. It retains source-file
URL bases and the existing variable, fallback, inline-edit and image-set paths.
The URL walker is read-only and substitution scratch dies after discovery.
Premature raw-sheet discovery is removed from stylesheet completion and loader
setup. Resource limits, network waits and D10.1.7 snapshot clocks are unchanged;
Input LI-036 separately preserves reuse of existing resources at capacity.

Qualification: five new loopback HTTP PNG/request controls improve from 0/5 to
5/5, covering 600 unmatched icon rules, inactive conditions, stylesheet-relative
variables, pseudo backgrounds and live selector activation. Five existing inline
CSS image controls also pass, including video and style mutations; three input
capacity/cache lifetime units pass. Focused integer lint checks 89 casts with zero
unmarked. The exact saved Conductor source now displays the large hero and brand
marks, without a capacity warning. Evidence: Lambda's
`temp/online-visual-round31/`; tracked integration controls are
`test/online/render_css_resource_discovery.test.cjs` in Lambda.

This remains speculative discovery of URLs in selected cascade declarations;
visibility pruning and exhaustive resource selection are not claimed. Full
layout/paint/JS/Input baselines and catalog recapture were deferred.


<a id="ri-078"></a>

### RI-078 — Absolute percentage math keeps a provisional containing height

**Status:** FIXED for late preferred-height resolution, 2026-11-14.
**Lambda task blocker:** no after repair. Native ownership is D9.1.1v4 /
D9.1.3v2; scalar declaration queries preserve D5.5.1 owner/scratch lifetime.

Statamic's script-free hero contains an absolute image with
`height:calc(100% + 3rem)`. It retained 48px after its auto-height parent reached
952px, omitting the landscape. The late absolute-child pass revisited only the
bare percentage scalar, ignoring calculated percentage dependencies.

The existing grid percentage-preferred-size resolver is now shared through
`layout_resolve_percentage_preferred_size`. It classifies after substitution,
borrows the final containing axis through `LayoutContainingBlockScope`, resolves
with the child's own units and variables, and returns only a scalar. Grid keeps
its existing behavior; the absolute-height pass reuses the helper and its
existing min/max, box-size and ratio handling. Fixed/automatic heights are not
replaced by percentage guesses. No public structure layout changes.

Qualification: new native `AbsolutePercentageMathUsesTheFinalContainingHeight`
checks images, blocks, own font/variables, a maximum, fixed and bare-percentage
controls through live 160/240/160px parent changes, plus query/geometry isolation.
It and three existing grid/absolute sizing cases pass. Six PNG controls improve
from 2/6 to 6/6 and independently pass Chromium. The focused integer-cast lint
checks 89 casts with zero unmarked. Evidence: Lambda's
`temp/online-visual-round30/`; tracked reductions are
`test/online/abs_percentage_height_fixtures.cjs` in Lambda. Full layout/paint
baselines and catalog captures were deferred under the user's focused budget.


<a id="ri-077"></a>

### RI-077 — SVG inherited reads repeat shared ancestor declaration queries

**Status:** FIXED for the selected cache path, 2026-11-14. Native ownership
**D9.1.1v4 / D9.1.3v2**, cache-pool lifetime **D5.5.1**, unchanged snapshot frame
**D10.1.7**. This is a partial cost repair; Dock's capture blocker is not cleared.

A release CPU sample of Dock localizes repeated full-sheet declaration queries
inside SVG transition baselines. The existing per-document computed-property
cache retained the requesting child's result, but inheritance walked common
ancestors again for each sibling. The resolver now stops at an existing parent
snapshot and fills the visited ancestry with that same computed value. The
walk stays iterative, and direct hits allocate nothing. Existing document,
selector-state, stylesheet/query, font, layout, animation-generation, base-value
and inheritance guards are retained; scoped use-instance and incomplete SMIL
reads remain uncached. Values stay owned by the existing SVG cache pool, with
no borrowed declaration payload or native/public structure-layout addition.

The new native regression covers sibling inheritance, variables, source owners,
initial/unset, explicit inheritance of non-inherited opacity and ancestor class
and attribute changes. It and the neighboring SVG percentage case pass. Five
existing SVG inheritance, animation-base/retargeting, text-hit invalidation,
paint invalidation and cache-teardown tests pass. The same saved script-free
Dock page produces a byte-identical PNG before/after. Single release controls
observe 9.83 CPU seconds before and 9.12 after; these are limited observations,
not a stable performance guarantee or page-wide improvement claim.

Matched live release profiles both reach the unchanged 150-second cutoff:
23 completed layouts before and 25 after. Repeated script-turn full recascade
and layout remains the dominating capture cost (RI-031). No canonical page
capture or reference is replaced from these diagnostic profiles. No URL, site
condition, admission, time/grace budget, clock or broad verification gate changes.
Only the two native and five existing SVG cases were selected; full baseline
and catalog capture were deferred under the user's instruction.

Evidence: Lambda `temp/online-visual-round29/`, `qualified-source-sha256.json`,
`native-final.log`, `rdt-focused.log`, `dock-{static-profile,static-final,live-profile,live-final}.json`,
identical static PNGs and `dock-live.symbolicated.txt`. The linker-map image's
text-section hash was checked equal to the sampled pre-change release image
before resolving its addresses. Final release pin SHA256
`c82dd7955d760cbb7e101bc062f9d049c4164bd3e2e2a7fe51fc3dbae3412f49`.



<a id="ri-076"></a>

### RI-076 — SVG percentage viewport attributes inflate intrinsic height

**Status:** FIXED for the selected height-at-width path, 2026-11-14. Native
ownership **D9.1.1v4 / D9.1.3v2**, measurement rollback **D5.5.1** and snapshot
frame **D10.1.7**. User authorization permits sibling changes.

Contentsquare's saved script-free blog gives the lead card 757.65px native
height versus Chromium 501.61px. The media's final dimensions agree; removing
the tiny Read More SVG removes 256px of excess card height. A network-free
reduction gives a 10×10 final SVG but a 300px intrinsic contribution, inflating
its card to 761.65px versus Chromium 481.64px. Equivalent CSS percentages work.

SVG percentage geometry attributes were applied only in final viewport layout.
Intrinsic width queried them as unresolved and selected the 300px default;
the containing height query reused that width even with a definite narrow
containing block. The existing percentage-attribute getter is now shared with
final layout. Height-at-width queries resolve the attribute against their own
containing width, apply existing width constraints and transfer the ratio.
Authored CSS precedence and cyclic/indefinite width handling are preserved.
The child containing-block scope also retains definite zero instead of silently
borrowing the outer width. No authored style, site condition, admission limit,
resource grace or capture time is changed.

Seven PNG controls improve from 2/7 to 7/7; all independently pass Chromium.
They cover partial/oversized percentages, nonsquare ratios, definite zero,
min/max clamps and CSS precedence. The first candidate's 6/7 result was held
until zero propagation was repaired. The native regression additionally checks
alternate-width queries preserve selected geometry and a live width-attribute
edit changes the card's size. All 98 selected layout/cache unit cases pass;
integer lint reports 89 casts and zero unmarked. Same saved full-page geometry
now gives a 502.05px card, agreeing within 0.5px with Chromium. Small text/baseline
residue and cookie/interactive differences are outside this bounded repair.

A public read-only helper was added; runtime/native structure layouts and Node
state ABI are unchanged. Broader layout/paint baselines and catalog recapture
are deferred under the user's focused-verification instruction. Canonical page
qualification is recorded in Lambda's round28 online comparison audit/report.
Evidence: Lambda `temp/online-visual-round28/`, original/repaired card captures,
`svg-percentage-{before,final}.log`, `svg-percentage-browser.json`,
`native-final.log`, `lint.log`; native
`DynamicLayoutTest.SvgPercentageViewportDimensionsDoNotInflateIntrinsicRows` and
Lambda `test/online/render_svg_percentage_intrinsics.test.cjs`.



<a id="ri-075"></a>

### RI-075 — Canvas general matrix operations are unavailable to the DOM host

**Status:** FIXED, 2026-11-14. Native provider **D9.5.6**, ownership
**D9.1.1v4 / D9.1.3v2**, synchronous matrix borrow **D5.5.1**. The selected
Origin callback failure is cleared; page-wide hero parity remains unresolved.

Origin's DOMContentLoaded particle initializer throws at `context.setTransform`.
An unchanged extracted local script reproduces this exactly. Canvas already
retains a CTM and supports scale/rotate/translate, but has no general set/compose
provider operation and the DOM bindings publish none of these matrix methods.
The native provider now accepts six affine coefficients and set/compose mode.
Existing matrix application is shared with scale/rotate/translate; the setter
preserves clip, path and other drawing state. Runtime bindings supply transform,
setTransform (six numbers or DOMMatrix2DInit aliases/defaults) and resetTransform.
Receiver, arity, conversion, alias and nonfinite checks run before native state
changes. No site condition, alternate source, animation time or budget changes.

All 10 new PNG controls fail before and pass after; 10 independent Chromium
controls agree. They cover composition/order, off-diagonal coefficients,
replacement, reset, clip/style preservation, dictionary/error behavior,
save/restore, resize and actual load-callback initialization. Existing Canvas
context and drawImage goldens pass; the rebuilt DOM graphics-provider contract
case passes and layout integer lint reports zero unmarked casts. The saved
Origin page callback completes after repair. Other visibility/Worker gaps remain.
The provider table gains an operation; affected providers/clients are rebuilt.
Runtime/DocState layout and Node state ABI are unchanged. No playback, WebGL,
getTransform or complete Canvas conformance claim is made.

Evidence: Lambda `temp/online-visual-round27/`, new
`test/online/canvas_transform_fixtures.cjs` / `render_canvas_transform.test.cjs`,
`canvas-before.log`, `canvas-after.log`, `canvas-browser.json`,
`canvas-app*-output.log`, `origin-*-output.log` and `provider-test.log`.
The selected matrix contracts follow HTML Canvas transformations and Geometry
Interfaces DOMMatrix2DInit validation; no Lambda formal ruling is changed.

<a id="ri-070"></a>

### RI-070 — Inherited percentage width retains the parent's used width

**Status:** FIXED, 2026-11-14. Native ownership **D9.1.1v4 / D9.1.3v2**,
computed-value lifetime **D5.5.1**. No full online page is attributed to this
adjacent defect without a fresh page-specific comparison.

`resolve_css_axis_size` copied the parent's mutable given size for `inherit`,
losing percentages, keywords and the difference between a preferred size and
its clamped used size. A 600px card with 20px padding gave its width-inheriting
child 600px instead of 560px; the image-bearing reduction could enlarge the
card to 640px. The resolver now follows the parent's winning physical/logical
size declaration and inheritance chain. Existing variable and owner-context
helpers substitute the parent's value; lengths are normalized in the parent's
font context, percentages remain for the child's containing box, and the
existing axis resolver consumes the temporary owned value before it retires.
No persistent field, API or native layout structure is added.

One focused native regression fails before and passes after. Seven width cases
cover percentages, parent max-width clamping, auto, em, percentage calc with
different parent/child fonts and variables. A percentage-sized image and live
style edits are included. Height and grandchild inheritance additionally check
percentage and calc values. The neighboring min-width regression also passes.
Broader verification is deferred under the user's focused-check instruction.

Evidence: Lambda `temp/online-visual-round26/inherit-before.log`,
`inherit-after.log`, `inherit-controls.log`; native
`DynamicLayoutTest.InheritedAxisSizesKeepComputedPercentagesAndLengths`.
The original image-bearing reduction remains in Lambda
`temp/online-visual-round24/width-inherit-before.html`.

<a id="ri-074"></a>

### RI-074 — A min-content minimum incorrectly caps the preferred width

**Status:** FIXED, 2026-11-14. Native ownership **D9.1.1v4 / D9.1.3v2**,
geometry ownership **D5.5.1**. The selected sizing blocker is cleared; Python
page-wide comparison remains unresolved.

`measure_element_intrinsic_widths` set max-content equal to min-content for
`min-width:min-content`. A minimum is not a cap on preferred width. This also
breaks fieldsets, whose UA minimum is min-content. Python.org's search fieldset
loses the combined width of its inline controls and wraps them vertically.
A reduced 30px span + 220px input + 50px button becomes 220x120 native versus
300x40 Chromium; replacing fieldset with an ordinary block without that minimum
already gives 300x40. The repair removes the incorrect min-width cap while
preserving max-width:min-content and min-width:max-content behavior.

One focused native regression fails before and passes after. It covers fieldset
and ordinary block wrappers, zero/min-content/max-content live transitions,
a definite preferred width and a max-width:min-content control. On identical
saved Python HTML, form width improves from 239.4px to 303.381px and height from
114.6px to 98.6px; Chromium is 348.859px by 57.375px. Separate icon visibility
and slider initial-frame differences remain; full-page clearance is not claimed.
Aggregate verification is deferred under the user's focused-check instruction.
No capture clocks, admission rules, references or thresholds change.

Evidence: Lambda `temp/online-visual-round26/minwidth-before.log`,
`minwidth-after.log`, `fieldset-before*`, `fieldset-browser*`,
`python-before*` / `python-after*`; native
`DynamicLayoutTest.MinContentMinimumPreservesPreferredInlineWidth`.

<a id="ri-073"></a>

### RI-073 — Cyclic percentage invalidation discards replaced media height

**Status:** FIXED, 2026-11-13. **Lambda blocker:** cleared for Pivot271's selected
zero-height media defect. Native ownership **D9.1.1v4 / D9.1.3v2**, region/state
ownership **D5.5.1**, unchanged capture frame **D10.1.7**. User authorized edits.

The same saved script-free Pivot source loads its MP4 and reports natural
506x720 with all 180 resources complete. The enclosing column is about 619.94px
high in both engines, but native video/immediate wrapper height is zero versus
619.72px in Chromium. Authored CSS is `width:100%;height:100%` in an auto parent.
A network-free 200x100 poster video in a 320px auto parent reproduces native 0
versus Chromium 160, while its natural ratio remains correctly available.
Posterless video also loses its 150px default height.

The default/natural replaced-size and ratio stage runs before cyclic percentage
height handling, which clears its result. The stage now follows invalidation;
only the invalidated height percentage is treated as automatic. Existing type
conditions and helpers supply dimensions/constraints, the obsolete iframe
fallback exception is removed, and the repeated ratio call is shared. A natural
ratio also describes content when specified sizing uses border-box; the common
ratio helper now preserves that distinction. No page condition, geometry
constant, CSS rewrite, new decoder or capture/readiness change is added.

The native live-style test fails before and passes after with correct intrinsic
facts, auto/definite/zero parent transitions, 50%/100% percentages, fallback
retention and decorated border-box media. Thirteen PNG controls improve from
6/13 to 13/13; independent Chromium passes all 13. Author versus natural ratios,
content/border-box decorations and the image neighbor are included. All 97
native dynamic and 13 refreshed custom-layout cases, 206 online cases, 247
rebuilt paint/ownership cases, 117 UI assertions (10 runner cases), 26 independent
SVG/PDF rasters, and integer lint (89 casts, zero unmarked) pass. All 17 layout
categories retain 5609 full passes and 295 recorded partials, zero regressions,
skips or errors. No API/header/structure/state layout changes. Input source
and Node ABI are unchanged; prior independent LI-001/002/012 aggregate failures
remain recorded. Release performance and broader replaced-size conformance
remain unqualified.

The unchanged saved Pivot video/wrapper is now 619.72888px high (Chromium
619.71875px), while the overall hero stays 984.699px. The final canonical
full-page capture shows the large animated artwork, resolving the major
missing-artwork annotation. The remaining shape difference reflects movie
phase, alongside smaller background/header/spacing residue. The quick
script-free diagnostic captures the blank initial poster/frame, while native
wall-clock playback advances during full-page loading; D10.1.7 does not freeze
that playback. The actual H.264/avc1 asset decodes through AVFoundation. An
initial sandboxed Swift probe fails but the same probe outside it succeeds;
this is not attributed to the engine or an unsupported codec. Capture clocks
are unchanged, no diagnostic frame is substituted, and playback synchronization
remains unqualified.

Evidence: Lambda `temp/online-visual-round25/`, root
`test/online/video_percentage_fixtures.cjs`, `render_video_percentage.test.cjs`,
`VideoCyclicPercentageHeightRetainsNaturalAndFallbackSize`, same-source
`pivot-media*` / `pivot-fixed*`, original poster/MP4 headers and SHA256 records,
and independent time0/time1 decoded frames. Final pin SHA256:
3fb7114ad21abb7a6c90ce36ee6bfb3573d94370694c7817df60d2a20643e882.


<a id="ri-072"></a>

### RI-072 — Atomic inline alignment loses enclosing relative Y offsets

**Status:** FIXED, 2026-11-13. **Lambda blocker:** cleared for RI-071's inline
control. Selected geometry **D5.5.1**, native ownership **D9.1.1v4 / D9.1.3v2**.

An inline `span{position:relative;left:40px;top:40px}` containing a 100px
inline-block marker places it at (40,0) in the preserved host; Chromium gives
(40,40). Removing the overlapping background reproduces before RI-071. Final
line alignment overwrites atomic Y without retaining enclosing inline offsets,
although the existing text and BR branches already share
`layout_applied_inline_relative_y`. The atomic branch now uses the same helper
before applying its own relative offset. No alternate geometry path is added.

The unchanged positioned-inline PNG control passes both witnesses. Native
`InlineAtomicAlignmentPreservesRelativeAncestors` covers nested offsets and
alternating child relative/static styles without accumulation. The joint final
qualification below covers all 96 dynamic, 13 custom-layout, 193 online and
247 raster/ownership tests, 117 UI assertions, 32 SVG/PDF controls and unchanged
17-category layout inventory. Evidence: Lambda
`temp/online-visual-round24/inline-flow*`, `inline-order*`, `inline-after.log`
and the native regression. No expected pixel or tolerance is weakened.


<a id="ri-071"></a>

### RI-071 — Absolute and relative zero-level siblings paint in separate passes

**Status:** FIXED, 2026-11-13. **Lambda blocker:** cleared for the selected
block-owned sibling paint/hit sequence. Native ownership **D9.1.1v4 / D9.1.3v2**;
geometry **D5.5.1**, unchanged capture frame **D10.1.7**. User authorized edits.

Pivot's saved script-free hero has an early absolute background followed by a
relative foreground container. Both prior hosts paint that background over the
headline/actions, while Chromium shows them. A 600x400 relative parent, absolute
navy background and later lime relative marker reproduce without network,
scripting, fonts or media. The shared flow walk paints relative zero-level
children before the separate absolute containing-block chain. Reverse hit
traversal follows those split passes and also targets the background.

The block-owned collector now combines its in-flow relative/sticky/containment
children with its absolute chain and sorts equal stack levels in DOM order.
The existing selection order helper is promoted to the shared stacking module;
a deep-DOM control exposes its old 64-ancestor cap, now replaced by an unbounded
ancestor walk. The render walk dispatches generic Views so inline boxes retain
their producer. Hits consume exactly the same list in reverse. Explicit phase
ownership on child walkers preserves standalone and inline calls; normal block
callers defer their relative siblings to the combined sequence. Existing signed
layers and custom-layout sequencing keep their owners. Two child-walk signatures
extend with this flag; consumers and all four Node module targets are refreshed.
Public structure/Node state layout is unchanged.

Sixteen PNG controls improve from 7/16 before to 16/16 after. They cover both
source orders, auto/explicit zero, positive/negative layers, sticky and inline
foreground, absolute descendants under static/deep wrappers, overflow clipping,
static owners and a once-only opacity group. All 16 independent Chromium
controls and all 32 independent SVG/PDF rasters pass. The native paint/hit test
fails before and passes after under live z-index changes; all 96 dynamic and
13 custom-layout cases pass. Joint final qualification: 193 online cases,
247 rebuilt raster/ownership cases, 117 UI assertions (10 runner cases), and
integer-cast lint (89 casts, zero unmarked). All 17 layout categories retain
5609 full passes and 295 recorded partials with zero regressions/skips/errors.
The final layout/UI gates run against the exact restored pin after the RDT
client target incidentally relinks the working host. Input source is unchanged;
prior independent LI-001/002/012 aggregate failures remain recorded.

The unchanged saved Missive customer page also restores its large right-card
quotation and Canny logo. The preserved host and RI-069-only host hide both;
the final host and Chromium show them. The final canonical capture agrees,
resolving its manual major omission while retaining smaller font/logo-size and
spacing residue. Same-source evidence is `missive-static-before*`,
`missive-static-after*` and `missive-width-only*` under the round24 directory.

The unchanged saved Pivot source now shows its headline, copy, actions and
customer logos. RI-069's closer hero height remains 984.70px versus Chromium
991.58px. Background/media/header residue remains; exact parity is not claimed.
This finite repair does not implement zero-level descendant lifting through
arbitrary contexts, negative inline stacking or full flex/grid isolation rules.
Evidence: Lambda `temp/online-visual-round24/`, root
`test/online/positioned_order_fixtures.cjs`, `render_positioned_order.test.cjs`,
and native regressions. Final pin SHA256
4648e3a3abaec9cd3eb443e4962ddf164bd43c37d39cdf8d65272e6dfe6a69eb.
Release performance remains unqualified. Canonical review is recorded separately
in online comparison round 24.


<a id="ri-069"></a>

### RI-069 — Height queries omit width expressions and constraints

**Status:** FIXED, 2026-11-13. **Lambda task blocker:** cleared for the selected
Maple static sizing defect; its independent animated-WebP title gap remains
LI-030. Ownership **D9.1.1v4 / D9.1.3v2**, scalar query/selected geometry
**D5.5.1**, unchanged capture frame **D10.1.7**. User authorized sibling edits.

The same saved script-free Maple hero is 843.4px high instead of about 600px,
while the title image and text have correctly sized selected boxes. A reduced
600px column flex, a border-box card with 20px padding and a child
`width:var(--size)` or `calc(100% - 360px)` produces a 600px card instead of
240px. The selected image is correctly 200px square. Literal width controls
agree, isolating the measurement path rather than final layout or media.

The generic height query recognizes only literal lengths/percentages when
resolving its root content width. It omits functions, variables and width
constraints, and conflates the percentage containing block with the supplied
content width and border-box edges. Descendants contribute a height at the
outer width, which the enclosing flex retains as its basis.

The repair reuses element-owned CSS resolution and size classification, existing
content/border-box conversion and min/max clamps. Automatic/intrinsic keywords
and unresolved cyclic percentages retain their existing policy. Assigned
flex/grid query mode still consumes the supplied content width directly. No
second CSS evaluator, style rewrite, page condition, capture-clock change or
public API/structure-layout change is added.

`IntrinsicHeightResolvesWidthExpressionsAndLimits` fails before and passes
after, covering live variable/style/constraint changes, alternate available and
assigned width queries, and preservation of selected image geometry. Sixteen
PNG controls improve from 3/16 to 16/16; independent Chromium passes all 16.
They cover variable fallbacks, calc/min/max/clamp, width limits, percentage and
content/border-box edges, and measured-element font units. All 94 native dynamic
cases, 177 online cases, 247 refreshed raster/ownership cases, 117 UI assertions
(10 runner cases), and integer-cast lint pass. All 17 layout categories retain
5609 full passes plus 295 recorded partials, with zero regressions/skips/errors.
Input source was unchanged; its aggregate was not repeated and prior independent
LI-001/002/012 failures remain recorded. Node module ABI is unchanged.

The unchanged static Maple hero shrinks from 843.4px to 605.77px (Chromium about
600px), restoring the following statistics section to the viewport. Smaller
button/font/spacing residue remains outside the major-gap milestone. The title
still fails because the actual response is animated WebP, which the input codec
explicitly rejects (LI-030); no title repair or exact page parity is claimed.
Canonical refresh/review is recorded separately in round 24's comparison.
Evidence: Lambda `temp/online-visual-round24/`, root
`test/online/intrinsic_width_value_fixtures.cjs` and
`render_intrinsic_width_value.test.cjs`, plus the native regression. The host
SHA256 is a377e8444c548c0a37be721e1967c377402f9835ec78c8e05e1439a29d37db26.
Release performance and broader intrinsic-sizing conformance remain unqualified.


<a id="ri-068"></a>

### RI-068 — Grid row contributions reuse provisional whole-grid widths

**Status:** FIXED, 2026-11-13. **Lambda task blocker:** cleared for the selected
horizontal grid/flex sizing defect. Ownership **D9.1.1v4 / D9.1.3v2**, query
rollback **D5.5.1**, unchanged capture frame **D10.1.7**. The user authorized
sibling edits.

Monese's saved script-free home page has a 1392px native hero versus Chromium's
800px. Its side grid item is finally 280px wide but contributes 1136px of
height. A reduction with a 600px three-column grid, a central 280px minimum and
200px content, and a square image in a percentage-width side item produces a
600px row instead of 200px, independently of scripts and external resources.

Grid row contributions select the provisional given width before the solved
144px column area. That percentage was resolved against the whole grid, and
the height query then applies preferred width again. The generic grid-height
estimator independently guesses equal-width columns, promotes a percentage
minimum using the whole grid, and leaves the inflated size in the enclosing
flex measurement. Fixing only final row contributions exposes this second path.

The repair promotes the existing grid-placement border-size resolver, adding a
read-only mode for measurement, and uses the solved area as the percentage
containing block. Height queries receive the assigned content-width contract.
Explicit zero preferred sizes retain zero. Horizontal grid height-at-width
queries now reuse the existing placement and track-sizing algorithm under
`LayoutMeasureScope`, replacing the guessed column calculation. The existing
width-resolution block is shared by grid and ordinary-flow queries. No page
condition, authored-style rewrite, second track algorithm or clock change is
introduced. Public functions were added; there is no structure-layout change.

The native regression fails before and passes after, including live
600/900/600px resizing and alternate-width scalar queries that preserve selected
grid/image/wrapper geometry. Twelve PNG reductions improve from 3/12 to 12/12;
all independently pass Chromium. The first candidate's 11/12 result remains
recorded and was rejected until the enclosing-query path was fixed. All 93
native dynamic tests, 161 online cases, 247 refreshed raster/ownership cases,
117 UI assertions (10 runner cases), and integer-cast lint pass. Four Node DSOs
were refreshed. All 17 layout categories retain 5609 full passes plus 295
recorded partials, with zero regressions/skips/errors. Input aggregate was not
repeated; prior independent LI-001/002/012 failures remain recorded.

The unchanged static Monese hero shrinks from 1392px to 800px; its heading moves
from y533.90 to y237.90, agreeing with Chromium's y237.90625. Smaller grid image
height/spacing differences remain (385.8px grid versus 324.19px). The live page's
initial hidden-state script is a separate unresolved cause; this fix does not
claim to clear its blank canonical capture or establish exact page parity.
Vertical intrinsic measurement, full grid conformance and release performance
remain outside the milestone. Evidence: Lambda `temp/online-visual-round23/`,
`test/online/grid_row_width_fixtures.cjs`, `render_grid_row_width.test.cjs`, and
`DynamicLayoutTest.GridRowHeightUsesTheSolvedPercentageWidth`. The qualified
host SHA256 is d0aae7d46c8943d80bc05f1445aee9bd13d01b2c78380560297193b0b88aef2c.
Canonical refresh/review is recorded separately in round 23's report.


<a id="ri-067"></a>

### RI-067 — Row-flex cross heights reuse the preferred width after flexing

**Status:** FIXED, 2026-11-13. **Lambda task blocker:** repaired for the selected
horizontal row-flex cross-height defect. Native ownership **D9.1.1v4 /
D9.1.3v2**, query ownership **D5.5.1**, selected capture frame **D10.1.7**.
The user authorized sibling edits.

Panxo 481's headline/copy/actions sit below a large empty hero, pushing its
product panel outside the viewport. Identical saved script-free HTML reproduces
the gap: the native top row is 508px high instead of Chromium's 163.19px. The
right item occupies its correct 405px flexed width but retains a 508px height;
the correctly sized left item aligns to that inflated row's bottom. A reduced
600px row with a 200px fixed neighbor assigns the other item 400px, yet uses
124px of height measured at its preferred 100px width instead of 52px.

The cross-height fallback passes the assigned content width into the generic
height query, whose descendant branch reapplies the root's preferred CSS width.
Intrinsic preferred cross sizes also read the earlier unflexed height cache;
intrinsic min/max clamps can retain that value after main-axis allocation.
Thus growing and shrinking items both wrap against the wrong width during
cross sizing, even though final text layout uses the assigned width.

Height queries now carry an explicit used-width mode, defaulting to the existing
available-width contract. Only that query root skips preferred-width resolution;
descendant width/percentage resolution keeps its own containing block. The mode
is included in the immutable height-cache key. Row-flex intrinsic block sizes
and keyword clamps resolve against the allocated main content width, and the
ordinary cross fallback forwards the same contract. Existing sizing helpers
perform measurement; no authored CSS rewrite or second formatter is added.
The C++ query signatures changed and native/header consumers were rebuilt;
no public structure layout changes were needed.

The focused native RowCrossHeightUsesTheFlexedWidth test fails before and passes
after. It covers width/style changes, shrink/restore, distinct available/used
query results for equal numeric widths, and preservation of selected geometry.
Twenty-two real PNG controls improve from 4/22 to 22/22 and independently pass
Chromium: auto/min/max/fit-content cross sizes, intrinsic min/max clamps,
padding/border, percentage descendants, growth/shrinkage, fixed/zero height and
a row inside a column. All 92 native dynamic cases, 149 online cases, 247
refreshed raster/ownership cases, 117 existing UI assertions (10 runner cases)
and layout integer-cast lint pass. Four Node DSOs were refreshed. The 17 recorded
layout categories retain 5609 full passes plus 295 recorded partials, with zero
baseline regressions, skips or runner errors. Input's aggregate was not repeated.

The same static Panxo row shrinks from 508px to 172px, restoring the headline
and product panel to the viewport. Chromium measures 163.19px; the small residual
text/spacing difference is outside this major-gap milestone. Live canonical
qualification and manual-review resolution are recorded in round 22's report.
Evidence: Lambda temp/online-visual-round22/ preserves the saved document,
failing native/PNG controls, repaired captures, pin and gate logs. Browser
references, URLs, admission, finite resource grace, deadline and virtual time
remain unchanged. Vertical/orthogonal, broader intrinsic-sizing conformance and
release performance are not qualified by this repair.


<a id="ri-066"></a>

### RI-066 — Auto flex columns measure empty wrappers and unlaid-out child boxes

**Status:** FIXED, 2026-11-13. **Lambda task blocker:** repaired for the selected
centered auto-column regression exposed during RI-065 qualification. Native
ownership **D9.1.1v4 / D9.1.3v2**, measurement ownership **D5.5.1** and capture
frame **D10.1.7**. The user authorized sibling edits.

The first RI-065 pin restores Comand but shifts Exactly 471's heading upward
88px inside a clipped centered flex column. Identical saved script-free HTML
confirms the regression: Chromium and the previous pin position the headline
at y298 with height 120; the first repair pin uses y210 with height 120. Trial
line formatting itself returns the correct 120px. The first capture/report
candidate was rejected during visual review and is preserved as evidence.

Two auto-height approximations lose the same collected content. The multipass
extent walk visits source display:contents wrappers rather than collected flex
items. Its auto-basis fallback then initializes descendant boxes to zero and
recurses over those boxes without measuring text. Exactly's heading and action
therefore contribute zero; only 32px gap plus 80px padding gives a provisional
112px column. Centering 176px of actual items plus 32px gap in 32px content space
moves them upward 88px. The previous wrong heading estimate happened to trigger
a later corrective content pass; accurate inline measurement removed that
incidental second pass. An experimental cache-key guard did not fix the cause
and was removed.

Auto extents now reuse the existing collected-item walker. A live column's auto
and content bases both use calculate_flex_basis, which already owns constrained
text measurement and preferred-size rules. No page condition, alternate line
algorithm, cache bypass or clock change is introduced. Exactly's unchanged
static headline returns to y298, and Comand's RI-065 restoration remains intact.

The new CenteredAutoColumnMeasuresFlattenedTextItemsBeforeAlignment native test
covers center/start/center recascade, full heading height, outer auto height and
action placement. The new centered PNG control fails on the first pin and passes
with the companion repair; all 11 PNG controls independently pass Chromium.
Final qualification passes 91 native dynamic cases, 127 online cases, 247 refreshed
raster/ownership cases and layout integer-cast lint. The 17 layout categories
retain 5609 full passes plus 295 recorded partials, with 0 baseline regressions,
skips or runner errors. Live capture refresh and visual qualification are in
the round 21 online report. No release performance claim is made.

Evidence: Lambda temp/online-visual-round21/ keeps failing first captures,
script-free reductions, generic traces, rejected v2/v3 experiments, final v4 pin,
source records and gate logs. No diagnostic logs or experimental cache changes
remain in engine source. Original browser references, authored inputs, admission,
finite resource grace and deadlines are unchanged.


<a id="ri-065"></a>

### RI-065 — Structured inline breaks understate nested flex height

**Status:** FIXED, 2026-11-13. **Lambda task blocker:** repaired for the selected
horizontal text/BR intrinsic-height defect. Native ownership **D9.1.1v4 /
D9.1.3v2**; selected fragment ownership **D5.5.1** and capture frame **D10.1.7**.
The user authorized sibling edits.

Comand 524 places its headline 129.6px too low and clips hero copy/action. The
same saved script-free HTML/CSS reproduces it in native20 and agrees with the
original discrepancy against independent Chromium. Its headline finishes at
194.4px, but the outer flex column measures 355.6px rather than 420.4px. Replacing
fonts, resolving typography variables, removing hidden variants and flattening
contents wrappers do not remove the difference. A reduced monospace fixture
with text/span + BR + text/span measures the column 298px instead of 334px.

intrinsic_text_br_content_height admitted direct text/BR only (except balance).
The complex-inline fallback maxed independently measured inline children,
losing forced-line boundaries when a heading also contained spans. Eligible
horizontal text/BR descendants now reuse the normal inline formatter in an
isolated measurement scope. Counter state, caller context, selected scalar
boxes and text fragments are restored. Mixed fonts are not flattened into one
font and no separate line-layout algorithm is introduced. Direct text keeps
its existing fast path; vertical/paged/atomic estimators are not expanded.

The new DynamicLayoutTest.NestedColumnIntrinsicHeightRetainsInlineForcedBreaks
fails before and passes after, including hide/show recascade, repeated 120/240px
queries and selected text-geometry preservation. All 91 native dynamic cases
pass. Ten root PNG cases improve 2/10 to 10/10 and independently pass Chromium:
direct/nested breaks, normal/contents wrapper, mixed font sizes, preserved and
nonwrapping whitespace, consecutive breaks, direct-text and hidden-break
controls. The complete online suite passes 127 cases; layout integer-cast lint
passes. The unchanged saved Comand hero now measures 420.4px and positions its
headline at 333.6px, within 0.02px of Chromium. Its missing live background video
remains a separate unresolved difference; no complete Comand repair is claimed.

Evidence: Lambda temp/online-visual-round21/ retains reductions, failing tests,
qualified host, logs and comparison refresh. Capture admission, finite resource
grace, 150s deadline, authored pages, browser references and virtual clock stay
unchanged. Broader gate and canonical refresh qualification is recorded with
the round 21 online report.


The first pin was held after Exactly visual review exposed RI-066. Its companion
repair and the eleventh centered PNG control are required for final qualification.

<a id="ri-064"></a>

### RI-064 — Inline CSS image URLs never enter snapshot resource discovery

**Status:** FIXED, 2026-11-13. **Lambda task blocker:** repaired for inline URL
discovery. Native ownership **D9.1.1v4 / D9.1.3v2**, declaration scratch lifetime
**D5.5.1**, selected-frame capture **D10.1.7**. The user authorized sibling edits.

Maki222's white hero omits its authored video CSS background, leaving white
heading/copy unreadable. The inline URL survives parsing and the image paints
in isolated controls; literal local pixels also paint in the complete page.
Network-managed paint reads only ready resource bytes. The loader prefetches
stylesheet URLs before inline attributes are parsed, and document discovery
previously queued img/use/video-poster resources but no inline CSS URLs. Thus
all77 tracked requests could complete while the required background was never
requested. This is distinct from RI-061's finite readiness grace.

Document image discovery now walks parsed inline declarations and reuses the
existing recursive CSS URL visitor, promoted through resource_loaders.h. CSSOM
edits borrow their owned declaration blocks; untouched HTML attributes parse
through the existing declaration parser in one traversal-owned scratch pool.
The scheduler copies URL ownership before that pool retires. The shared walker
retains custom-property/function/list behavior without rewriting consumer URL
spelling. Requests use the existing scheduler, priorities, admission and bounded
snapshot wait. Paint still does not start a blocking HTTP fetch.

Five reduced HTTP PNG/request cases in Lambda's existing render_session.test.cjs
fail0/5 before and pass5/5 after: div/video background, inline variable,
image-set and script style mutation. The complete online suite passes116 cases.
Fresh raster/paint/ownership247 cases,117 existing media/CSSOM UI assertions
(10 runner cases), and17 layout categories pass with5609 full passes/295
recorded partials and0 regressions/0 skips. Both unchanged saved Maki script-free
and full-script controls regain the hero media; full external source/time
freezing and exact sizing/video frame parity are not claimed.

Evidence: Lambda temp/online-visual-round20/ retains failing controls, final
pin, source patches, logs and the canonical comparison refresh. The existing
1000ms optional-resource grace,150s parent deadline, original browser references,
URLs, authored content and virtual clock are unchanged. No broader media
readiness, mask consumer, platform decoder or SPA support is claimed.


<a id="ri-063"></a>

### RI-063 — Decoded video overwrites foreground page content

**Status:** FIXED, 2026-11-13. **Lambda task blocker:** repaired for the selected
video paint-order defect. Native ownership **D9.1.1v4 / D9.1.3v2**, immutable
render ownership **D5.5.1**; the selected-frame contract **D10.1.7** is unchanged.
The user granted sibling implementation updates.

Bizzabo656's full saved HTML renders its primary heading/copy/action and header
under the hero video. At capture the video is playing with a decoded1920x1080
frame, while heading/ancestors have visible styles, opacity1 and valid geometry.
The absolute video has z-index:-1. `render_video_frames` and its cached update
path nevertheless blit the video over the completed page. This violates the
existing [CSS painting order](https://www.w3.org/TR/CSS22/zindex.html#painting-order).

The native `DecodedFramePreservesFollowingForegroundPaint` regression records
a decoded video then a red foreground rectangle. Before repair the post-paint
blit replaces the red4278190335 pixel with decoded4281808966. The repair captures
decoded pixels and controls once in an immutable owned ImageSurface, then
records it through the ordinary image painter in CSS paint order. Shared
object-fit/object-position, content clips, transformations and effect scopes
apply. The video marker retains decoder-epoch invalidation. Playback wakes
repaint the page through the same ordered stream; the out-of-order cached blit
and unused placement cache are removed. Worker replay cannot borrow a mutable
decoder buffer. This does not add a decoder/backend or force a later frame.

Qualification:34 native video cases,247 raster/paint/ownership cases,
111 online CLI/PNG/capture/comparator cases and117 assertions in seven existing
media/CSSOM UI replays pass. Seventeen layout categories retain630 JS and4979
static passes plus295 recorded partials, with0 regressions/0 skips. The native
regression also checks contain/cover and retained pixels after a decoder seek.
The complete saved Bizzabo HTML restores the foreground; external resources
remain live, so that screenshot is not a fully frozen-source or video-time
comparison. Full-catalog canonical refresh/report evidence is recorded in
Lambda `temp/online-visual-round19/` and `test/online/`.

Limits: live video now replays ordered page content rather than the previous
placement-only optimization. No release playback performance qualification or
new GPU video composition claim is made. SVG/PDF video frames and platform
backends remain separately qualified; browser references, URLs, admission,
deadlines, comparison thresholds and virtual clocks are unchanged.


<a id="ri-057"></a>

### RI-057 — Sizing and noscript snapshot regressions in the Lambda layout gate

**Status:** RESOLVED, 2026-11-13. **Lambda task blocker:** repaired. Native
ownership: D9.1.3v2. Initial reproduction: Lambda `e5d5e848e`, Radiant `ea6ec14`.
The user explicitly approved Radiant edits and overriding the sibling boundary.

The recorded `wpt-css-sizing` baseline had two regressions and `wpt-css-lists`
had one. All reproduced individually; no fixture, reference, tolerance or scored
baseline was changed.

- `svg-intrinsic-size-001`: the 40x40 viewport with a 4:1 viewBox was measured
  correctly, then treated as an automatic cross size because its attribute had
  no numeric BlockProp slot. The shared automatic-size predicate now recognizes
  SVG viewport hints when CSS does not override them. Cross sizing reuses the
  existing explicit-size helper and publishes measured column width before
  alignment. Late line reconciliation reuses the existing stretch predicate;
  it no longer overwrites attribute dimensions through a second numeric check.
- `keyword-sizes-on-abspos`: intrinsic min/max keywords did not make the parent
  query cyclic. Descendant percentage canvas widths therefore contributed the
  provisional containing width instead of zero/20px min/max-content values.
  A shared preferred/min/max predicate now governs contribution measurement,
  cyclic percentage/cache basis and ratio transfer.
- `counter-set-002`: all geometry already matched. The unrendered snapshot
  confused the suppressed layout role with computed `display:none`, shifting
  comparison child paths. It now uses the existing computed-display serializer
  while retaining zero geometry.

Final qualification, debug split host at the commits above plus these changes:

- `make test-radiant-baseline`: exit 0, **8,165 passed / 299 recorded partial /
  20 skipped**, zero failures out of 8,484 cases.
- Sizing: 776/776. Lists: 52 passing plus eight recorded partial results.
- Selected UI baseline, including the independent Lambda-owned LR12-54 iframe
  rooting fix: 329/329.
- Owner `test_layout_dynamic_gtest`: 87/87, including two new regressions for
  all six intrinsic canvas limits and independent SVG axes through row/column
  flex, CSS auto and retained attribute mutation.
- Layout integer-cast lint: 89 casts checked, zero unmarked exceptions.

Evidence is under `../../lambda/temp/failed-radiant-tests/`, especially
`radiant-baseline.log`, `native-dynamic-final.log`, `radiant-lint.log` and the
original focused mismatch logs. [Implementation record](../../lambda/vibe/impl/Lambda_Impl_Radiant_Baseline_Repairs.md).
The extra Lambda sweep remains incomplete after its missing optional WebRTC
prerequisite; ASan host linkage is a separate Lambda LR01-25 limitation.


<a id="ri-055"></a>

### RI-055 — Paint engine lacks the M3 wet-media kernels

**Status:** FIXED, 2026-11-12. Native ownership: **D9.1.1v4 / D9.1.3v2**; engine contract: PE9.

Initially the paint engine had no `paint_wet_layout`, `paint_wet_step`, or
`paint_wet_deposit` implementation. PE9 calls for a caller-owned planar sheet,
in-place Curtis steps, activity bounds, and transfer into pigment tiles. At the initial report Lambda could not connect the native watercolor path or
establish parity and the 512-by-512 step budget without those kernels and their API.
This is an unimplemented milestone, not a diagnosed defect in existing kernels.

Reproduce from Lambda-opus: inspect `../radiant/radiant/paint/` and run
`rg -n 'wet_step|wet_deposit|PaintWet|wet_layout' ../radiant/radiant/paint`;
at the initial report there were no matches. `lmd/package/procart/watercolor.ls` supplies the 21-plane
reference and `vibe/Lambda_Pkg_Paint.md` section 6 gives M3 acceptance: parity,
the wet-step budget, and the unchanged procart watercolor demo on the native
step. Implement and test the kernels in Radiant, then expose the owner-defined
interface through Lambda's adapter and validate those acceptance cases.

Continuation: the user authorized Radiant changes. `paint_wet_step` now has a
three-pigment, caller-owned 21-plane implementation with validation and focused
native tests. A ten-step, 40-by-32 two-pigment comparison against the Lambda
reference differed by at most 1.2e-7 across the planes; this is a focused
check, not full M3 qualification. The continuation now adds dry-to-pigment
transfer and Lambda adapters for both f32 and float64 sheets. An 80-step
reference comparison passes on the three Lambda tiers with forced GC, and five
30-step runs against a pinned release archive give medians of 6.04 ms (f32)
and 6.98 ms (float64) for a fully active 512 x 512 sheet. The watercolor replay
passes 64/65; its one mobile layout assertion fails identically with the original
Lambda step. At that checkpoint the wet layout was still fixed to three pigments; the
continuation below resolves that engine limitation. Evidence: Lambda-opus `temp/paint_m3/`.

Resolution: `paint_wet_layout` now describes 1–4,096 pigments with `12 + 3M`
planes, and the shared f32/float64 kernels step, track activity, dry and transfer
every pigment. The bridge checks table/shape agreement and charges work by count.
Native tests cover one, four and seven pigments, the last pigment's activity and
drying, span rejection and transfer; a 4,096-pigment Lambda probe passes. The
three-pigment outputs remain byte-identical to the prior implementation in six
checked cases. A source-matched isolated release archive passes 36 native paint
tests and the 512 x 512 wet budget (6.79 ms f32, 7.47 ms float64 locally).
Lambda's isolated baseline passes 7,457/7,457 and Test262 passes 40,261/40,261;
five wet fixtures pass on three tiers under forced GC. Both the native-step and
reference-step watercolor replays still fail the same one mobile layout assertion
(64/65 each); its ownership is unconfirmed and it is not claimed as fixed here.
Evidence: Lambda-opus `temp/paint_m3_general/`.

<a id="ri-054"></a>

### RI-054 — Owner-held font fixture lookup under Lambda integration

**Status:** FIXED, 2026-11-12. Ownership: **D9.1.1v4 / D9.1.3v2**.
`radiant_test_fixture_path` chose Lambda's `test/fixtures` merely because the
directory existed, even when the requested `fonts/cmunrm.woff2` belonged to
Radiant. This made `FontMetricTest.RasterPreservesNativeGrayscaleCoverage` fail
with a null loaded font. The shared helper now tests the requested local file
before resolving the owner-held fixture; isolated runs use `RADIANT_ROOT`.
The same font test fails before and passes after correction. Native metric
expectations and grayscale comparisons are unchanged. Evidence in Lambda-opus:
`temp/paged-p7-integration/{paged.xml,updated-focused.xml,paged-final.xml}`.


<a id="ri-053"></a>

### RI-053 — Historical P7 comparison artifacts are unavailable in the current checkouts

**Status:** CLOSED by explicit user waiver, 2026-11-12. Native ownership:
D9.1.1v4 / D9.1.3v2. No layout ruling changes.

The historical P7 record in Lambda's `vibe/impl/Radiant_Impl_Paged_Integration.md`
requires the archived 113-input source inventory, release executables/receipts,
PNG controls and source-size snapshot under `temp/paged_release/`. That directory
and its qualification/source-size/raster scripts or archives are absent from the
available Radiant, Lambda and Lambda-opus temp trees. The source commit
`1a9921fe9` still exists in Lambda history, but the generated input hashes and
exact captured artifact set are unavailable. A source reconstruction alone must
not be reported as a replay of the original measured corpus.

Reproduction: from Radiant, inspect `temp/paged_release/`,
`../Lambda/temp/paged_release/` and `../Lambda-opus/temp/paged_release/`, then
search the three temp inventories for `paged*qualification.json`,
`paged*source_sizes.json`, `qualify.py`, `raster_controls.py` and paged archives.
The required historical set is not present. Cause of its loss is **unconfirmed**.

On 2026-11-12 the user confirmed that the archive location is unknown and
instructed us to assume it is deleted/lost and ignore its replay. This closes
the missing-artifact blocker by waiver; it does not make the old raster,
performance or source-reduction comparisons pass. Fresh source/core-fidelity
receipts under Radiant's `temp/paged-core-*` establish the current scope.
`scripts/qualify_paged.py` records the waiver separately from its current gates.
No reference results or historical measurements are reset.


<a id="online-visual-review-20261112"></a>

### Online visual qualification — 2026-11-12

RI-022 and RI-024–RI-027: 68 native dynamic layout tests, the document activation
regression, 38 StyleEpoch tests and 11 owner input-value tests pass (118 focused
cases). Online tools pass 14 checks. The aggregate Radiant gate retains the
existing JS03-L2 forced-GC iframe failure: 8,163 pass, 299 partial, 20 skip,
one fail. The final CSS images suite passes all 56 cases, including size
containment of video posters. No references, URLs, deadlines or comparison
thresholds changed. Evidence and the full comparison are under Lambda
`temp/online-visual-round2/` (`radiant-baseline-qualified.log`).

<a id="ri-027"></a>

### RI-027 — Mobile viewport metadata shrinks desktop layout independently of its host

**Status:** FIXED, 2026-11-12.
**Lambda task blocker:** no. Ownership: **D9.1.1v4 / D9.1.3v2**; scale axes
remain distinct under the scale design's **RSC6/RSC7**.

Cap'n Proto declares `width=480`. A 1200×800 desktop capture keeps a1200-pixel
surface and JS window but uses 480 pixels for the native root and CSS media
queries, leaving a large dark region. Chromium's desktop root is 1200 pixels.
The cause is the direct use of raw mobile meta dimensions in `layout_init`,
layout publication inputs and CSS media setup.

A shared host-viewport resolver now supplies all three stages. Parsed metadata
remains available, while desktop layout follows the host's CSS viewport.
Registered viewport-unit lengths also use that active CSS environment instead
of raw meta hints. Native regression covers width480/height320 metadata,
1200→800 host resize, CSS media queries, vw/vh, registered vw lengths and
oversized2000×2000 metadata. It fails before and passes after repair. Mobile
viewport adaptation/zoom remains outside this correction. Evidence: Lambda
`temp/online-visual-round2/capnproto-tree.json`, `test-viewport-{before,after}.log`.

Verification: see the [online visual qualification](#online-visual-review-20261112).


<a id="ri-026"></a>

### RI-026 — Background color resets paint opaque black

**Status:** FIXED, 2026-11-12.
**Lambda task blocker:** no. Ownership: **D9.1.1v4 / D9.1.3v2**.
OCaml's documentation hero declares `background-color:initial` above its SVG
background. Native layout sends that keyword to generic color conversion, whose
invalid-color fallback is opaque black. `unset` fails in the same way. The
background is non-inherited and its initial color is transparent:
[CSS Backgrounds §2.2](https://drafts.csswg.org/css-backgrounds-3/#background-color),
[CSS Cascade §7.3](https://drafts.csswg.org/css-cascade-5/#initial).

A shared background-color resolver now handles wide reset keywords before
generic color conversion in direct declarations and ordered background
composition. Explicit inheritance remains intact, and a color reset retains
the image layer. The reduced `background-initial.html` shows initial/unset alpha
255 before the fix, versus alpha0 for authored transparent. Native regression
`BackgroundColorWideKeywordsUseTransparentInitialValue` covers these cases,
shorthand reset and inheritance. Evidence: Lambda `temp/online-visual-round2/`.

Verification: see the [online visual qualification](#online-visual-review-20261112).


<a id="ri-025"></a>

### RI-025 — Headless exports never queue ordinary network image dependencies

**Status:** FIXED, 2026-11-12.
**Lambda task blocker:** no. Native ownership: **D9.1.1v4 / D9.1.3v2**;
finite snapshot frame selection: **D10.1.7**.

Loader-stage HTTP documents have a resource manager. `load_image_resource`
correctly consumes only ready bytes during layout/paint, but export preparation
never discovers ordinary `img` requests. Curl's main SVG logo is absent and
laid out with fallback dimensions; DEBUG logs say `Network image not ready
without blocking` with no corresponding request. The same logo and NOAA's photo
decode/render correctly from local files. This is a missing integration step,
not an unsupported image codec.

`radiant_prepare_snapshot_resources` now queues selected image sources and
external SVG uses before layout, binds the existing native resource manager,
and shares the layout CLI's existing bounded wait with export. It delivers
native completions without executing JS timers or advancing the virtual frame.
No budget increases or synchronous HTTP fetches were added to layout/paint.
The existing wait defaults to1000ms, with the existing 0–5000ms environment
override. Slow/pending resources can still be absent; this is not a claim of
full lazy-media/video support.

Offline regression `ExportQueuesOrdinaryNetworkImagesBeforeInitialLayout`
starts from a cached73×41 SVG available only through its HTTP request identity.
Before repair it has fallback height16 and no image; after repair it has the
correct natural geometry/image and virtual time0. The shared native image/use
discovery walker is also reused by interactive resource discovery. Evidence:
Lambda `temp/online-visual-round2/test-image-{before,after}.log`, `curl-debug.log`,
`assets-test.png` and live probes.

The followup also queues `video[poster]` as an opaque image dependency, without
binding it to the video as an `img` owner. Video width/height hints now survive
style resolution. When video metadata is unavailable, the existing replaced
intrinsic facts use a ready poster's natural dimensions before assigning the
generic 300×150 fallback. Automatic axes use the existing replaced-size
constraint solver so a maximum height scales the width with it. Size containment
keeps its substituted dimensions and suppresses the poster's natural ratio.
The cached-SVG export regression covers ordinary images, authored video sizes,
automatic poster sizes, max-height and containment. Automatic video size fails
before with 300×150 and passes after with 73×41. This is the existing poster
and intrinsic-sizing path; it does not implement video timeline parity.


Verification: see the [online visual qualification](#online-visual-review-20261112).


<a id="ri-024"></a>

### RI-024 — Multi-keyword link relations omit entire stylesheets

**Status:** FIXED, 2026-11-12.
**Lambda task blocker:** no. Native ownership: **D9.1.1v4 / D9.1.3v2**.
`vite_docs` and `rollup_docs` use `rel="preload stylesheet"`. Initial CSS
prefetch, linked-sheet loading and dynamic resource discovery compared `rel`
against the complete string `stylesheet`, omitting these sheets. Vite's banner
image consequently expands across the unstyled document. The HTML link relation
is a set of case-insensitive ASCII-whitespace-separated keywords:
[HTML links](https://html.spec.whatwg.org/multipage/links.html#linkTypes).

Promoted the existing sandbox token walker to `radiant_html_has_token` and
reused it in all three paths. The newly recognized titled alternate sheets stay
loaded but initially disabled; existing CSSOM can enable them. Native focused
regressions pass for keyword boundaries, case, HTML whitespace (excluding VT),
actual stylesheet application, and alternate activation. Live Vite/Rollup
captures now retain their document CSS. Qualification and residual details are
under Lambda `temp/online-visual-round2/`.

Verification: see the [online visual qualification](#online-visual-review-20261112).


<a id="ri-022"></a>

### RI-022 — Downloaded dynamic stylesheets lose their link media condition

**Status:** FIXED for the confirmed sheet activation defects, 2026-11-12. **Lambda task blocker:** no; this affects one
remaining large-difference class. Native ownership: **D9.1.1v4 / D9.1.3v2**.

`lambda.exe render https://webassembly.org/ -o temp/online-visual/wasm.png
-vw 1200 -vh 800` can export the dark stylesheet despite light media preferences.
The site writes both theme links from a `noscript` template. Their `media`
attributes select mutually exclusive color schemes. `process_css_resource` in
`radiant/resource_loaders.cpp` parses completed dynamic CSS and assigns its owner,
but leaves `CssStylesheet::media` null. Both sheets can enter the cascade, making
the result depend on loading. The initial linked-sheet collector already checks
media; the asynchronous completion path does not. The native cause is confirmed;
additional custom-element behavior on this site is not qualified.

Evidence in Lambda: `temp/online-visual/webassembly.html`, the preserved
`refreshed-before/webassembly-lambda.png`, and
`probes/webassembly.png` plus its log. Expected: retain the link's media
condition on the associated sheet and evaluate it with the shared CSS engine.
Do not force a particular theme or choose sheets by their filenames.
The selection requirement is also stated in the
[HTML link media processing rules](https://html.spec.whatwg.org/multipage/semantics.html#processing-the-media-attribute).

Implemented the completion-path correction: copy the owner's media query into
the document pool and initialize its disabled flag before appending the sheet.
The native `DownloadedStylesheetsRespectLinkMediaAndDisabledState` regression
covers completed/cached downloads and active/inactive/disabled sheets; the full
11-case input-value suite passes. Three final WebAssembly.org captures still
select dark, so this site is not claimed fixed. Its custom element changes
media and disabled state during loading; the remaining association/mutation
cause is unconfirmed. Evidence: `temp/online-visual/wasm-final-{0,1,2}/`.
The separately built pristine-current-source control produces the identical
61.45% mean error against the browser image (`pristine-wasm/webassembly.png`),
so this remaining mismatch is not introduced by the three repairs.

**Confirmed followup and repair (second review):** `link.media` and `style.media`
were absent from IDL reflection; `link.disabled` used the style element's
sheet-only setter and lost pending-link state. Associated sheets did not receive
owner attribute edits. Lambda's shared CSSOM owner-attribute hook now updates
the retained sheet condition/disabled state, invalidates it, and preserves
unrelated CSSOM state. Initial and dynamically parsed inactive sheets remain
associated with their media conditions instead of being discarded.

The native fast, reference, layer-rank and single-element cascade paths also
ignored root-sheet media; they now share an active-sheet predicate. The new
`StylesheetOwnerMediaAndPendingDisabledRetainTheirAttributes` integration test
fails before and passes after (pending-link booleans, media on/off, attribute
set/remove, immediate computed style and sheet identity). The final
WebAssembly.org capture selects light: mean color error 61.4%→0.6%. Its shadow
control rendering remains partial under Lambda JS13-L2. The earlier unconfirmed
association/mutation cause above is now resolved; WASM execution was not the
cause of this theme mismatch. HTMLLinkElement's disabled IDL is reflected,
whereas HTMLStyleElement's disabled IDL controls its associated sheet:
[HTML link/style interfaces](https://html.spec.whatwg.org/multipage/semantics.html#the-link-element).


Verification: see the [online visual qualification](#online-visual-review-20261112).


<a id="ri-021"></a>

### RI-021 — Grid placement declarations suppress actual flex items

**Status:** FIXED, 2026-11-12.
**Lambda task blocker:** no. Computed/view storage follows **D5.5.1**;
native ownership follows **D9.1.1v4 / D9.1.3v2**.

Chrome, Firebase, Google Cloud and web.dev documentation pages wrap ordinary
description text into tiny columns or give it zero-sized boxes. A local
reduction requires a column flex parent and a child with `grid-area: content`.
Resolving the declaration provisionally allocates the parent-item union's grid
arm. Flex initialization refuses to overwrite that arm, so the flex algorithm
cannot size the item. The property's declaration order incorrectly determines
participation in its parent's formatting context.

`layout_set_parent_item_kind` now releases the provisional owning prop, adopts
the actual parent role and invalidates the computed storage before applying
its CSS again. Flex and grid initialization share this transition. It also
replaces grid's previous discriminant-only conversion, which abandoned the
old owning prop. The authored CSS remains in the cascade; no grid declarations
are removed to match a page. Native regressions cover text geometry and four
flex/grid parent switches. All 63 dynamic-layout checks and integer-cast lint
pass. The reduced Chrome description grows from zero to the browser's 564px.
Both aggregate integration runs have 8,163 passes, 299 partials, 20 skips and
one existing forced-GC iframe failure (Lambda JS03-L2). A separate pristine
current-source host reproduces that failure 3/3, as does the repaired host.
The gate remains red; its failure is not suppressed. Broader live recapture
is recorded in the Lambda online-visual checkpoint.

Evidence in Lambda: `temp/online-visual/min-rules.json`, `prop-no_area.json`,
`chrome-static-fixed.json`, `test-root-causes.log`, and `test-dynamic-all.log`.

<a id="ri-020"></a>

### RI-020 — An initially disabled linked theme participates in the cascade

**Status:** FIXED, 2026-11-12.
**Lambda task blocker:** no. Native ownership: **D9.1.1v4 / D9.1.3v2**.

Chrome documentation links its dark theme with the boolean `disabled`
attribute. `collect_stylesheets_in_document_order` loaded the sheet but did not
initialize its existing `disabled` flag, so its dark declarations replaced the
active light theme. Firebase, Cloud and web.dev share this failure.

The collector now retains the sheet for CSSOM access while marking the root
sheet disabled. Its imports stay inactive through the existing sheet-tree
traversal. Native tests cover the empty boolean attribute, `disabled='false'`,
imported rules and enabling/disabling the retained sheet across recascades.
All focused and neighboring native tests pass. Aggregate qualification has
the same existing iframe failure documented above under RI-021.
The minimal before fixture incorrectly gives a light page a dark
background and changes a 600px block to 8px. Evidence in Lambda:
`temp/online-visual/disabled-before.json` and `test-root-causes.log`.

Live before/after coarse mean image error against the unchanged Chromium image:
Chrome 79.5%→7.3%, Firebase 75.8%→1.5%, Cloud 67.1%→4.1%, web.dev 57.0%→6.0%.
These improvements combine RI-020 and RI-021; remaining icon/image details are
outside the major-difference milestone.

Verification evidence: `../../lambda/temp/online-visual/radiant-baseline-final.log`,
`test-dynamic-all.log`, `test-media-final-all.log`, `layout-lint.log` and
`pristine-build.py`. The online comparison scripts and saved before/after
images retain the remaining differences; none are filtered out to qualify these repairs.

<a id="ri-018"></a>

### RI-018 — Online exports exhaust memory before viewport PNG encoding

**Status:** RESOLVED for the four original reproductions, 2026-11-11.
**Lambda task blocker:** cleared for APR, Azure, F# and Leaflet captures.
Ownership **D9.1.1v4 / D9.1.2v2 / D9.1.3v2**; precise carrier ownership
**D6.3**, map shape mutation **D3.4.3**. Coordinated sibling edits authorized
by the user. The original observations had separate allocation causes.

- Leaflet's collapsed-cell borders and fragmented-simple backgrounds were
  lowered without clearing the pending PaintIR list. Later cells replayed
  every earlier border. Both raster boundary paths now use the existing
  `paint_record_lower_pending` gateway. Leaflet's display list fell from
  4,452,044 commands to 8,862 and the 1200x800 PNG exports successfully.
- Azure/F# exposed Lambda String boxing that reflagged every indexed property
  through copied shape prefixes. The Azure allocation snapshot showed
  1.77GB/30M allocations in the transition arena; a native allocation stack
  identified `mvp_lmd_box_primitive` / `type_tree_retype_target`. Lambda now
  constructs the complete unpublished index/length shape once, preserving
  subclass identity, UTF16 units and immutable descriptors. Azure post-script
  allocator reservation fell from 1,925,873,344 to 160,050,837 bytes. APR,
  Azure and F# now have canonical loaded-document PNGs.
- Follow-up Studio capture exposed input's legacy all-frame GIF loader
  bypassing the process memory policy. It now admits its existing preflight
  estimate through `memtrack_allow_allocation` before giflib expansion. GIFs
  that exceed the shared budget retain the independent still-frame path;
  the configured budget is unchanged. Studio's image category fell from
  1,059,694,508 to 711,756,076 bytes and PNG encoding succeeds. Its selected
  initial frame is mostly blank; this is load/export qualification, not a
  claim of full visual fidelity.

Verification: `RadiantDocumentTest.BoundaryFragmentsAreConsumedOncePerBox`
checks exact linear command counts and empty pending lists for 32 collapsed
border and fragmented-background flushes; it and collapsed-table geometry
pass 2/2. Required Radiant integer-cast lint passes. The full Lambda JavaScript
native executable passes 859/859 tests, including long-wrapper memory and
UTF16/descriptor checks. Input image I/O passes 35/35, including budget
rejection and retained still decoding. The final Radiant baseline passes 8,164 with
299 partial / 20 skipped of 8,483 and no failed gate. The final Lambda gate
passes 7,415/7,415, including 859 JS and 368 GC stress cases; Lambda
LR03-46 records the later private-object fallback repair and Ko-fi PNG.

Evidence in Lambda `temp/online-recovery/`: `azure-memory.json`,
`azure-string-fixed-memory.json`, `azure-arena-trace-output.log`,
`leaflet-dl-trace-output.log`, `leaflet-border-fixed.log`,
`border-fragment-tests.log`, `js-mvp-suite.log`, `radiant-baseline.log`,
`studio-image-trace3-output.log`, `studio-budget-fixed-output.log`;
input `temp/online-gif-tests.log`. Canonical PNGs/logs are under
Lambda `test/online/snapshots/`. Temporary allocator instrumentation was removed.


<a id="ri-017"></a>

### RI-017 — One-shot exports drain recurring animation before capture

**Status:** RESOLVED, 2026-11-11. **Lambda task blocker:** cleared for finite
HTML file/source exports. Ownership **D9.1.1v4 / D9.1.3v2**; snapshot contract
**D10.1.7**. The user authorized coordinated sibling source updates.

`render_export_load_html_document` and `render_export_load_html_source`
left `auto_close_event_loop` and `virtual_clock_enabled` false. Recurring rAF
and timers therefore kept the host in its long-lived drain loop before PNG
encoding. Native layout suspends the script CPU watchdog, so this can also
outlive its wall-time allowance. A stack sample on Boldare located the loop
under `js_event_loop_drain_impl` / `js_animation_frame_drain` / layout checkpoints.

Both loaders now use one shared export configuration helper with finite
virtual-clock capture. The selected frame runs load handlers and queued rAF,
without advancing a future timer. Scripts and assertions remain enabled.
`RadiantViewTest.FileExportsCaptureRecurringFramesWithoutAdvancingDelayedTimers`
verifies PNG, SVG and PDF output, load/rAF execution and exclusion of a delayed
1000ms timer. It and `StaticSnapshotDoesNotAdvanceTimersDuringNativeLayout`
and `FailsUnsettledSnapshotAndContinuesLayoutBatch` pass (3/3). Boldare now
loads and exports its canonical Lambda PNG; a full failed-side corpus retry
is recorded in Lambda's ignored capture manifest. Not every former timeout
is attributed to this cause.

Evidence: Lambda `temp/online-recovery/ri017-tests.log`, original
`saas_landing_003/sample.txt`, and
`test/online/snapshots/saas_landing_003-lambda.png` with its capture logs.


<a id="ri-013"></a>

### RI-013 — Downloaded CSS overrides later document styles

**Status:** RESOLVED, 2026-11-11. **Lambda blocker:** cleared for the public
Kian Hup login rendering task. Ownership: **D9.1.1v4 / D9.1.3v2**.

`process_css_resource()` appended a downloaded sheet without its link owner
and bypassed inline styles' document-order sorting. A dynamically inserted
Bootstrap link could therefore override the template and page styles after
its download completed. The handler now sets `owner_element` and uses the
promoted `radiant_append_document_stylesheet()` helper; the duplicate append
and array-growth implementation is removed.

The owner regression
`RadiantInputValueTest.DownloadedStylesheetsUseOwnerOrderAcrossCompletionOrder`
loads completed/cached resources in reverse order around an inline style and
checks all three owners. It and the existing inline mutation/order test pass.
The Lambda host separately repairs `link.rel` reflection and parser-blocking
head insertion order under **D8.4.4 / D6.3**.

<a id="ri-014"></a>

### RI-014 — A zero-height body loses its canvas background image

**Status:** RESOLVED, 2026-11-11. **Lambda blocker:** cleared for login rendering.
Ownership: **D9.1.1v4 / D9.1.3v2**.

`lambda.exe view https://kianhup.com/login` loaded the 6x6 tile, but all body
content is absolutely positioned and the body's height is zero. Native canvas
propagation handled only color, leaving the image's painting area empty.
The shared background recorder now extends the selected canvas image layer's
painting area to the viewport. Canvas source selection also checks root
images before propagating a body background, rather than checking color alone.
Image positioning remains based on the existing box geometry; transformed,
contained and fragmented canvas backgrounds are outside this repair's checks.
The relevant CSS rule is [CSS Backgrounds 3 §2.11](https://www.w3.org/TR/css-backgrounds-3/#special-backgrounds).

Lambda integration fixture `canvas_body_background` passes its viewport pixel
assertions beyond a zero-height body and verifies that a root background color
suppresses body propagation. PNG and SVG exports retain the tile through the
shared recorder. Live comparison evidence is in
`../../lambda/temp/kianhup-style/native-final.png` and `browser.png`.

<a id="ri-015"></a>

### RI-015 — Authored inline buttons bypass atomic button layout

**Status:** RESOLVED, 2026-11-11. **Lambda blocker:** cleared for login rendering.
Ownership: **D9.1.1v4 / D9.1.3v2**.

With `button { display:inline; font-size:14px; height:2.4em }`, inline dispatch
bypassed the existing HTML button used-display transformation in
`layout_block.cpp`. Inline decoration supplied a 30px box instead of the
authored 33.6px height; the surrounding login panel was 11.68px too short.
The helper is now shared through `layout.hpp` and applied before dispatch,
so inline buttons reach their existing atomic layout and paint code.

The `canvas_body_background` integration fixture checks the 33.6px button
height. The live panel measures 231.800px versus Chromium's 231.78125px and
the button 33.600px versus 33.59375px. A 0.574px button text width difference
remains as font-metric residue; heading and input widths match. Integer-cast
layout lint passes. Geometry and replay logs are under
`../../lambda/temp/kianhup-style/`.

<a id="ri-010"></a>

### RI-010 — Layout baseline mismatches after the host-path repair

**Status:** RESOLVED, 2026-11-11, macOS arm64. **Lambda blocker:** cleared.
The normal integration entry point passes all 18 categories: **5,631 passed /
293 partial / 0 failed / 12 skipped**, all 5,936 cases present. Authority:
**D9.1.1v4 / D9.1.3v2**, preserving **D5.5 / D6.3**. The user explicitly
approved the scoped 16-file macOS reference correction as an exception to
AGENTS.md rule 12. Original captures, fixture source, scored baselines and
tolerances remain intact; the native and reference repairs are described below.

**Original report and intermediate qualification:**

The reported 5,904 layout startup failures are caused by Lambda adapters using
the shared runner's default `radiant.exe` in workspaces containing `lambda.exe`.
A two-site explicit-host patch, tested in an isolated review checkout, executes
all 5,936 selected cases: 5,621 pass, 293 retain partial fidelity, 10 fail and
12 input-event cases retain their skips. The user explicitly approved the two-file ownership exception, and that
integration repair is applied; it is not a layout workaround.

Initial ten cases:

- `wpt-css-inline/text-box-trim-multicol-{002,004,006,007,008}`.
- `wpt-css-text/text-spacing-trim-end-001` and `writing-system-line-break-001`.
- `wpt-css-writing-modes/available-size-{022,023}`.
- `wpt-css-lists/list-marker-position-crash`.

Reproduce from this checkout with `node test/layout/test_radiant_layout.js --exe
../Lambda/lambda.exe --host-cwd ../Lambda -c <category> -t <case> --json`.
The exact original reports, isolated integrated host, ten retained view trees,
full comparisons and fresh Chrome captures are under
`temp/radiant-failed-20261111/`. `remaining-layout-comparisons.json` compares the
original references; `fresh-comparisons.json` compares temporary fresh captures.

Fresh capture repairs fragment ordering in the 006/008 observations without a
native change; those two references therefore need an owner-side capture audit.
For 002/004/007, remaining continuation text positions differ by 20px despite
matching element boxes. The orthogonal 022/023 cases also require standards
review: the native constrained height follows their WPT reftest's scrollport
constraint, whereas current Chrome selects the larger viewport fallback under
containment. See [CSS Writing Modes §7.3.1](https://www.w3.org/TR/css-writing-modes-3/#orthogonal-auto)
and the fixtures' `available-size-022-ref.html` target. The CJK/font and list-marker
causes are not yet confirmed.
No reference, baseline score, fixture selection or tolerance was changed.

The native replay-summary repair separately passes 326/326 reported UI cases,
278 view cases with one existing speech skip, and the owner-native/layout gate.

**Native layout follow-up, 2026-11-11:** `text-spacing-trim-end-001` now fits
closing punctuation using its line-end half-space before wrapping, and
`list-marker-position-crash` now preserves float source order across an empty
list-item block. Both reach 100% element/text agreement with the stored and
fresh browser captures. Three multicol trim/flow corrections bring all five
reported multicol cases to 100% element/text agreement with fresh captures.
Japanese hyphen-like punctuation now respects the content writing system,
including explicit script subtags and inline/intrinsic boundaries.

The unchanged full integration contract now reports **5,623 passed / 293 partial /
8 failed / 12 skipped**, all 5,936 cases present. The dependencies at that checkpoint were:

- Five multicol cases still fail their stored text-fragment references; their
  fresh captures fully match the repaired native geometry. Reference ownership
  and capture audit block further work on these comparisons in Radiant scope.
- `writing-system-line-break-001` retains stored font/line differences. The
  repaired Japanese box matches fresh Chrome; Chrome's `ja-Hang` box still breaks
  before U+301C, contrary to the fixture assertion and the writing-system rule
  in [CSS Text 3 §5.2](https://drafts.csswg.org/css-text-3/#line-break-property).
  Native Hangul-script text retains the prohibited boundary. Reference/font and
  browser behavior need owner review; no incorrect native break is introduced.
- `available-size-{022,023}` retain the standards/browser disagreement. Both
  native outputs have aside heights `[280,280,280,280,280,680,680,680,680]`, exactly
  matching a fresh capture of their shared WPT reftest target. Chrome captures of
  the tests instead use 680px for all nine boxes. The scrollport sizing rule is
  preserved pending resolution of that disagreement.

Qualification: **901 native tests plus static smoke**, all **4,481 owner geometry
comparisons**, seven runner contracts and layout lint pass. Seven new native
regressions cover trim geometry, flow height, punctuation, script tagging,
inline/intrinsic breaks and float ordering. See the
[implementation record](Radiant_Impl_Remaining_Layout_Repairs.md) and
[pinned receipt](../temp/radiant-layout-20261111/verification-summary.json).
No sibling source, corpus reference, scored baseline, selection or tolerance
changes are included (D9.1.1v4 / D9.1.3v2).

**Reference audit follow-up, 2026-11-11:** the remaining eight comparisons have
independently generated macOS reference corrections prepared under
`temp/radiant-reference-20261111/proposal/`. Five use ordinary browser recaptures;
three use explicit, source-hashed standards controls through the existing corpus
mechanism. The two orthogonal controls reproduce all nine aside geometries and
line contents from a separate capture of the fixtures' shared WPT reference.
The CJK control applies strict breaking only to the Hangul-script string, giving
the required boundary without changing source text or prescribing geometry.
Unmodified Chrome outputs are rejected by all three corrected standards oracles.

Full/compact parity and the unchanged 5,936-case integration inventory pass in
a temporary overlay: **5,631 passed / 293 partial / 0 failed / 12 skipped**.
The 16-file additive patch preserves every existing reference and affects only
macOS selection. At that checkpoint, applying it awaited an explicit exception
to AGENTS.md rule 12, and the real corpus still had eight failures. The user then
approved the reviewed scope; exactly those 16 additions were applied and their
hashes verified without altering other dirty corpus files. See the
[review and patch](../temp/radiant-reference-20261111/REVIEW.md).

The standards review additionally completed native Hanb/Bopo/Hira/Kana script
recognition and added an orthogonal containment sizing regression. The focused
script checks fail before and pass after the mapping repair. The final owner
gate passes **902 native tests plus smoke, all 4,481 geometry comparisons and
seven runner contracts**; layout lint and its five contracts pass. This native
follow-up needs no reference exception (D9.1.1v4 / D9.1.3v2).

**Final applied qualification:** the real Lambda `test/test_run.js` layout
integration group now exits zero with the same **5,631 passed / 293 partial /
0 failed / 12 skipped** result as the isolated proposal. This run uses ordinary
reference resolution, with no temporary overlay. Every newly added file matches
its reviewed SHA-256; the corpus status delta contains exactly those 16 paths.
The eight corrected cases have 100% element/text agreement in both full and
compact formats. Existing partial cases and skips remain visible. Native source
and executable hashes still match the passing owner gate described above.

Final reports, explicit authorization and applied-reference identities are in
`temp/radiant-reference-20261111/applied-integration/`, `authorization.json` and
[verification-summary.json](../temp/radiant-reference-20261111/verification-summary.json).
The unrelated pre-existing clone-capacity mismatch remains open as RI-011.
Changes remain unstaged and uncommitted.

<a id="ri-004-pointer-event-protocol"></a>

### RI-004 — Lambda pointer-event adapter expects an absent native event protocol

**Status:** RESOLVED, 2026-11-10, macOS arm64. **Lambda blocker:** the original
release adapter and complete host now build and link from current sources. Authority:
**D9.1.1v4 / D9.1.3v2**. This report retains its original ID separately from
the older layout RI-004 below; its descriptive anchor preserves unambiguous links.

**Original reproduction:** Lambda starting HEAD `aee8f3242`, adapter last changed
in `41dc5ccc5`, Radiant `558eb3e`. From Lambda, after normal Premake generation:
`make -C build/premake config=release_native -f lambda-radiant-adapter-cpp.make LDDEPS= -j2`.
`lambda/module/radiant/event.cpp:8594–8624` expected `pointer_type`, pressure,
pen axes, `samples`, `RdtPointerSample`, `rdt_pointer_sample_of`,
`RDT_POINTER_MOUSE` and `RDT_POINTER_SAMPLE_CAPACITY`, but the owner header's
`MousePositionEvent` carried only position/movement. Compilation stopped after
20 missing-field/type/constant errors. The runtime archive built, while host
linking and execution qualification stopped. Original evidence remains in
Lambda's `temp/mvp_full_mir_retirement/adapters.log` and
`vibe/impl/Lambda_Impl_MIR_Full_Retirement.md`.

**Confirmed cause and repair:** the owner checkout lagged its native pen
protocol. Radiant commit `2a86f24` adds the complete protocol, platform capture,
bounded coalescing and replay; merge `7106a48` brings it into this checkout.
The existing Lambda consumer agrees with those definitions and its original
release build now succeeds. No further ABI edit or adapter fallback is needed.
Samples remain borrowed only during synchronous dispatch; Radiant's archive
continues to depend on lambda-input, with scripting supplied by the host
(D5.5.1, D6.3.3, D9.1.1v4, D9.1.3v2).

**Regression coverage:** five native cases in
[`test_pointer_input_gtest.cpp`](../test/test_pointer_input_gtest.cpp) verify
mouse initialization after pen input, pen axes and chronological samples,
separate device/pointer batches, bounded overflow/reset and borrowed samples
during reentrant submission. The owner Makefile discovers the suite with the
other native tests. All five pass; a fresh debug host links and its HTML replay
passes eight pen/eraser/mouse axis assertions through the unchanged adapter.
The canonical full release build also succeeds against Lambda `fd07a893f`:
`make -C ../lambda build-lambda-split SPLIT_PROFILE=release SPLIT_OUTPUT=../radiant/temp/ri004/lambda-release.exe JOBS=4`.
That release host passes the same eight assertions with each of
`JS_EXEC_BACKEND=ast` and `JS_EXEC_BACKEND=mir` (16/16). Both hosts were built
under `temp/ri004/`; the public Lambda executable was not replaced.

**Qualification limits:** `make -j4 PROFILE=debug run-radiant-baseline` passes
823 native tests plus the static corpus smoke test, with 45 existing skips.
The aggregate exits 2 because the shared corpus checkout `a64ed85dc` lacks the
`--host-cwd` option and related runner contracts expected by this Radiant tree:
six of seven runner tests fail, and geometry comparisons do not execute.
The existing Lambda-script pen fixture separately exits 255 on parser E100 at
`test/demo/paint/tests/_pen_input.ls:48`, before event delivery. Both failures
are retained; no fixture, assertion or reference was weakened. Broader Lambda
MIR retirement correctness and performance qualification remain separate.

Evidence: [`temp/ri004/`](../temp/ri004/), including `adapter-release.log`,
`pointer-tests.xml`, `host-debug.log`, `pointer-adapter-result.json`,
`build-release.log`, `release-link.log`, `release-replay-summary.json`,
`pen-replay.log`, `radiant-baseline.log`, `baseline-summary.json` and
`verification.json` (source and artifact identities).

<a id="ri-003"></a>

### RI-003 — Startup fragment navigation is followed as a document redirect

**Status:** FIXED in Radiant, 2026-11-09; full Lambda source rebuild remains
blocked independently. **Observed:** 2026-11-09, macOS, debug Lambda host.
**Lambda blocker:** the fragment loop is resolved for the two same-document
timer replays (D9.1.1v4, D9.1.3v2); retain the original Lambda unit cases.

From the Lambda checkout, create an HTML file under `temp/` containing
`<div id="result"></div><script>setTimeout(function(){document.getElementById('result').setAttribute('data-timer','yes')},0);location.replace('#result');</script>`.
Run `./lambda.exe view <file> --headless --event-file <events.json> --event-result <result.json>`
with an event that asserts `#result` has `data-timer` equal to `yes`. The host
exits 255 without a result; it logs `load_html_doc: too many document redirects`.
The equivalent in-process Lambda tests for relative and absolute same-document
navigation pass. The retained reproduction and log are under
`Lambda/temp/radiant-adapter-test-migration/fragment.{html,json,log}`.

**Confirmed cause and repair:** both redirect loops in
[`cmd_layout.cpp`](../radiant/cmd_layout.cpp) treated startup fragment navigation
as a replacement document. They now share `document_resolve_navigation_redirect`,
which uses input's `url_equals_without_fragment` and updates the existing URL
carrier in place. It clears the consumed request so the next viewer turn cannot
reload it. Document, DOM, pool and host-owned script/timer state remain retained
(D5.5.1, D6.3.3). Query/path changes and same-URL requests without a fragment
still redirect or reload.

**Verification:** the two original relative/absolute CLI replays failed with
exit 255 before the repair. All eight
[`test_navigation_cli.py`](../test/test_navigation_cli.py) cases pass afterward:
relative/absolute/repeated/empty fragments, cross-document and query redirects,
and relative/absolute `layout` output retaining timer-produced text. The six
`view` replays pass 12 assertions. Eight native loader regressions verify retained
ownership and redirect decisions. `make test-radiant-baseline` passes **770**
tests across **45** runners with **45 existing skips** (RI-001, RI-002 and the
hardware-dependent MIDI case); `make test-native-closure` also passes without
Lambda runtime or DOM libraries. Input facade and diff checks pass.

**Host qualification limit:** `make -C ../Lambda build` still fails on ambiguous
`direct_call`/`expression` overloads in Lambda's `mvp_mir.cpp`. Before/after CLI
checks used separately linked debug executables with current Radiant/input
archives and the same last-successful Lambda runtime/adapter archives. A full
current-source host rebuild remains pending. Reproduce the integration checks
with `make test-navigation-cli LAMBDA_HOST=../Lambda/lambda.exe` after that
independent build issue is repaired. Logs, binary/archive hashes and the
isolated executables are under `temp/ri-003/`; replay artifacts are under
`temp/navigation-cli/`.

<a id="ri-004"></a>

### RI-004 — Dynamic baseline exposes native anonymous-table and match-parent mismatches

**Status:** FIXED in Radiant, 2026-11-09; the three match-parent comparisons
are confirmed unsupported-browser references. **Observed:** 2026-11-09, macOS
debug Lambda integration, shared corpus `lambda-test` at `a64ed85dc`.
**Lambda blocker:** the native defects in this entry are resolved; the complete
integration suite still has independent failures, including those references.
Radiant retains native layout and generated-content ownership; Lambda remains
the optional scripting host (D9.1.1v4, D9.1.3v2, D5.5.1).

**Original reproduction:** from Lambda, run `LAMBDA_ROOT="$PWD" node
test/layout/test_radiant_layout.js -c baseline-js --baseline-only -j 3
--global-concurrency 3 --batch-size 100 --json`. Initially it reported
598/630 passing; the separate Lambda `classList` fix raised this to 599/630.
The native cases were `table-anonymous-objects-117`–`120`, `169`–`170`,
`185`–`188`, `193`–`196`, and `205`–`208`. Case `117` matched only 8/11
elements; `text-align-match-parent-02`–`04` matched only 13/17 text nodes.
The fixtures had moved from `baseline` to `baseline-js` without content changes.
Retained initial evidence: [full reproduction](../../Lambda/temp/radiant-failed-tests/baseline-js-reproduction.json),
[fresh-reference comparison](../../Lambda/temp/radiant-failed-tests/fresh-reference-comparison.json),
and [previous shared-runner results](../../Lambda/temp/radiant-failed-tests/final-reported-gates/test_radiant_layout_baseline_results.json).

**Confirmed causes and repairs:**

- [`intrinsic_sizing.cpp`](../radiant/intrinsic_sizing.cpp) did not recognize
  an inline outer display with table inner display as atomic before its first
  layout. Text lookahead descended into its cells, interpreted them as a forced
  line boundary, and removed the preceding space. Treating this display pair
  as atomic preserves the separator and adjacent-node geometry.
- A generated table-cell pseudo is reparented under anonymous table fixups.
  On retained reflow, [`layout_block.cpp`](../radiant/layout_block.cpp) looked
  up its content on that wrapper and replaced it with empty text. It now uses
  the existing `dom_source_parent()` helper to find its originating element.
  [`layout_list.cpp`](../radiant/layout_list.cpp) uses the same source ancestry
  for the pseudo's counter operations. Retained nodes and property ownership
  remain in their existing view-tree epoch (D5.5.1).
- [`view_pool.cpp`](../radiant/view_pool.cpp) now omits anonymous table wrappers
  below inline parents as it already did below block parents, preserving
  authored nodes and zero-size table markers in exported snapshots.

**Match-parent classification:** Headless Chrome 148.0.7778.97 reports
`CSS.supports('text-align', 'match-parent') === false`. Its original captures
therefore inherit logical start/end alignment and use the child's direction.
Radiant correctly resolves against the parent's direction, as required by
[CSS Text 3 §6.1](https://drafts.csswg.org/css-text-3/#valdef-text-align-match-parent).
With Chrome's supported `-webkit-match-parent` spelling applied only in a
temporary browser control, all 16 text positions in each of the three fixtures
agree with Radiant within 0.2 CSS pixels. Native tests cover parent-direction,
child-class and parent-width mutations. No alignment implementation, corpus,
reference, threshold or baseline expectation was changed. The three original
browser-reference comparisons remain failed pending corpus-owner correction.

**Verification:**

- All 18 named table fixtures now match **100% elements and text** against
  unchanged references. Native `207` rendering was visually checked against
  Chrome: the overlaid text is green, including the retained generated cell.
- All six [`DynamicLayoutTest`](../test/test_layout_dynamic_gtest.cpp) cases
  pass, including whitespace geometry, marker/snapshot structure, generated
  content and counter ownership across reflow, and the alignment mutations.
  Build with `make build/test/test_layout_dynamic_gtest`, then run
  `./build/test/test_layout_dynamic_gtest`.
- The native GTest and static-layout smoke portions pass: **794 tests,
  45 skips, no native failures**. The standard aggregate now also runs the
  geometry comparisons below, whose remaining failures keep it red.
- Full browser-geometry comparisons, using three host processes and batches
  of up to 100, improve `baseline-js` from **599/630 to 617/630** and static
  `baseline` from **3,786/3,851 to 3,800/3,851** (six static skips). No case's
  element or text score regressed. The remaining **13 dynamic / 51 static**
  failures are retained; these complete comparison suites are not green.
- Input-facade and diff checks pass. Direct `no-int-layout-decl` scanning has
  zero findings, and the changes add no integer casts. The documented
  `no-int-cast-radiant` rule is absent from the current Lambda lint configuration;
  its wrapper was also rejected by automatic approval review for sibling-temp
  writes. The optional type-aware checker lacks Python `clang` bindings.

Detailed results, unchanged-host comparisons, binary hashes and browser controls
are in [`temp/ri-004/qualification.json`](../temp/ri-004/qualification.json),
[`browser-alignment.json`](../temp/ri-004/browser-alignment.json),
[`radiant-baseline.log`](../temp/ri-004/radiant-baseline.log), and
[`lint-result.json`](../temp/ri-004/lint-result.json). Integration used an isolated
debug host relinked with the repaired Radiant archive and existing Lambda
runtime/adapter archives; a full Lambda source rebuild was not part of this
native repair. Initial invocations from Radiant lacked Lambda's package/WPT
roots; final comparisons launch from Lambda with all scratch/cache/log output
redirected into Radiant's `temp/ri-004/`.

**Standard test integration:** `make test-layout-baseline` now invokes the
existing shared `test/layout/test_radiant_layout.js` directly for both suites;
`make test-radiant-baseline` includes it. The shared runner's `--host-cwd` option
lets the external host resolve its package/WPT resources while all scratch files
stay in Radiant's `temp/`. Combined-suite reporting now propagates unscored
geometry failures and missing suites to the CLI exit code. Five maintained
runner-contract tests pass. The [standard report](../temp/layout-baseline/results.log)
retains the **51 static / 13 dynamic** failures and the aggregate exits nonzero;
the [full log](../temp/layout-integration/standard-radiant.log) also records the
passing native portions. The temporary RI-004 wrapper is no longer needed.

**Follow-up:** the historical 51 static / 13 dynamic failures above are now
resolved and the standard aggregate is green; see [RI-005](#ri-005).


<a id="ri-005"></a>

### RI-005 — Native static layout baseline needs execution qualification

**Status:** FIXED, 2026-11-09, macOS arm64 debug. The original **51 static /
13 dynamic** failures now pass through the existing standard layout suites.
**Lambda blocker:** none for these comparisons; the external host remains a
separate integration dependency. RI-006 remains independent.

The original qualification ran the static smoke corpus successfully but left
`baseline` at **3,800/3,851** and `baseline-js` at **617/630**. The standard
aggregate correctly returned failure. This follow-up repairs the underlying
layout and reference defects without changing fixtures, thresholds or coverage.

**Native repairs:** preserve aspect-ratio transfers and assigned sizes through
late absolute/flex/grid passes; retain fractional grid tracks and use the grid
area as the percentage containing block. Correct intrinsic percentage padding,
negative-indent forced breaks, emergency wrapping during balanced text probes,
float fit and clearance, and the multicolumn leading-margin simulation and
balanced-height floor. Table row minima exclude cyclic percentage cell heights;
empty-cell baselines and auto-inset static positions follow content alignment.
Inline bounds include decorated empty children, broken-image fallback text
preserves following spaces, bidi preserves visible interior spaces, and emoji
clusters retain their variation-selected font metrics. Counter operations ignore
boxless elements; reversed counters include their originating element and
following sibling scope, replacing same-name counters across intervening
sibling scopes.

The existing [`DynamicLayoutTest`](../test/test_layout_dynamic_gtest.cpp) suite
now contains **31 passing cases**, including 25 new focused regressions. No new
runner was introduced. `make test-layout-baseline` continues to run the shared
`test/layout/test_radiant_layout.js` for `baseline` and `baseline-js`, and
`make test-radiant-baseline` combines those comparisons with native tests and
the runtime-independent static smoke pass (D9.1.1v4 / D9.1.3v2).

**Reference qualification:** 23 full/compact reference pairs were recaptured
with the existing extractor after checking fixture fonts, WPT resources,
post-mutation geometry, source-order multicolumn fragments and bidi ranges.
Each was compared against native output before installation. Ten additional
full/compact standards-control pairs retain the original fixtures and browser
captures while explicitly compensating for Chrome 148 gaps: unprefixed
`text-align:match-parent`, prohibited atomic GL/WJ/ZWJ wrapping, and reversed
custom-counter initialization across sibling scopes. Controls record the source
SHA-256, specification, transformation and reason; the existing runner rejects
stale controls. Full/compact parity passes for all **33 pairs**. Comparison
normalization now preserves every fragment when differently wrapped source runs
cannot be partitioned, reporting differences instead of throwing. Browser
capture scratch honors `LAMBDA_TEMP_DIR`.

**Verification:**

- `caffeinate -is make PYTHON=/usr/bin/python3 test-radiant-baseline -j8
  LAMBDA_HOST=<qualified external host> LAYOUT_BASELINE_CONCURRENCY=3` exits 0:
  **819 native/smoke tests passed, 45 existing native skips; 3,851 static and
  630 dynamic geometry comparisons passed, zero failures, six existing static
  skips**. Coverage and element/text thresholds are retained; tolerance is 5px.
- All seven maintained runner/reference contract tests pass. Input-facade and
  both repository diff checks pass. Direct `no-int-layout-decl` scanning and
  the existing libclang `int-cast-type-aware` checker report zero findings
  across the changed layout files. Existing multiline `INT_CAST_OK` markers
  were moved onto the cast lines so the checker recognizes them. The legacy
  `make lint --rule ^no-int-cast-radiant$` wrapper is not a qualification gate:
  that rule is absent and its unconditional fixture helper tries to write into
  sibling scratch; the relevant existing checks were run directly with local
  scratch instead.

The [standard report](../temp/layout-baseline/results.log),
[full aggregate log](../temp/layout-repairs/standard-radiant.log), and
[qualification receipt](../temp/layout-repairs/qualification.json) retain the
counts, fixture identities, browser controls and executable provenance. The
isolated debug host was relinked with Radiant and rebuilt existing Lambda
runtime/adapter archives; no Lambda runtime source changes were required.
All scratch, logs and browser profiles used Radiant's `temp/`.

**Current workflow, 2026-11-10:** the standard target is now
`make run-radiant-baseline`. It builds and runs `radiant.exe` for both geometry
suites; that executable links the existing Lambda scripting adapters, while the
native Radiant archive remains runtime-independent. The full target again
passes 3,851 static and 630 dynamic comparisons with six existing static skips;
the native/smoke portion passes 819 tests with 45 existing skips. See
[`temp/radiant-standalone-baseline-final.log`](../temp/radiant-standalone-baseline-final.log).


<a id="ri-001"></a>

### RI-001 — HTML/CSS integration cases skip missing fixtures

**Status:** OPEN — test coverage gap. **Observed:** 2026-11-09, macOS,
`make run-radiant-baseline` (debug). **Lambda blocker:** not established.

The owner-local `test_html_css_gtest` run skipped 19 cases because fixture files
could not be loaded. For example, `LoadSimpleBoxTestHTML` could not load
`test/html/simple_box_test.html`, and `LayoutData_Flex_NestedContent` reported
a missing layout file. The remaining 30 cases passed. Reproduce with
`./build/test/test_html_css_gtest` after building the baseline. Determine
whether the fixtures should be supplied or the test paths corrected, and verify
the affected cases execute; this report does not establish a rendering defect.

**Resolution (2026-11-11):** FIXED under D9.1.1v4 / D9.1.3v2.
The 19 skips reproduce before the change. Most named fixtures had moved into
`baseline` (one into `page`); their test paths now follow the actual corpus.
The absent `simple_box_test.html` is supplied in Radiant's `test/html/`, and
the missing float/clear page now uses the existing static clear-float fixture.
Required single-file loads now assert rather than skip. No corpus file or
reference was changed. `test_html_css_gtest` passes **49/49, zero skips**.
Before/after receipts: `temp/ledger-html-{before,after}.{log,json}`.


<a id="ri-002"></a>

### RI-002 — DOM integration cases skip unsupported standalone node creation

**Status:** OPEN — test coverage gap. **Observed:** 2026-11-09, macOS,
`make run-radiant-baseline` (debug). **Lambda blocker:** not established.

`test/css/test_css_dom_integration.cpp` contains 25 existing skipped cases
that report standalone node creation is no longer supported. Reproduce with
`./build/test/css/test_css_dom_integration` after building the baseline.
Review the supported document-owned creation path and restore equivalent
coverage where applicable, without removing or hiding the skipped cases.
The separate MIDI live-device skip is conditional on hardware and is not
classified as a defect here.

**Resolution (2026-11-11):** FIXED under D5.5.3, D9.1.1v4 and
D9.1.4v2. All 25 skips reproduce before the change. The same named cases now
create native-backed text/comment/doctype nodes in the fixture's document,
using the document-owned detached factories or backed parent append helpers.
Text updates and insertion use the supported backed mutation APIs, preserving
both the Mark list and sibling chain. All original coverage families execute;
additional assertions check ownership, backward links, tails and backing order.
`test_css_dom_integration` passes **139/139, zero skips**. No input-owned source
or test changed. Before/after receipts: `temp/ledger-dom-{before,after}.{log,json}`.


<a id="ri-007"></a>

### RI-007 — Column flex measures a nested wrapping row too short

**Status:** OPEN — reproduced in static HTML; native cause unconfirmed.
**Observed:** 2026-11-10, macOS debug `lambda.exe view --headless`.
**Lambda blocker:** no. The paint demo (`test/demo/paint/`) gives its toolbar
rows that do not depend on this case.

A `flex-direction: column` container holds a wrapping row (`flex-wrap: wrap`).
One child of that row is itself a wrapping row limited by `max-width`. The
column sizes the flexible item after it as if the inner row did not wrap.
The outer row is then laid out taller, so the items after it overflow the
column by the difference.

Reproduce from the Lambda checkout with
`./lambda.exe view temp/radiant-issues/flex_col4.html --headless --event-file temp/radiant-issues/flex_rects.json`
(the rect assertions are expected to fail; their messages print the rectangles).
In a 1400 × 960 viewport, the toolbar `#bar` is laid out at 102 px. The
`flex: 1` stage gets 858 px, which is 960 − 74 − 28: the column used a 74 px
toolbar, the height without the inner wrap. The 28 px footer starts at y = 960,
outside the container. Expected: a 830 px stage and the footer at y = 932.
`flex_col6.html`, which is the same page with the outer row set to `nowrap`,
lays out correctly (96 + 836 + 28 = 960). So does the page without the inner
wrapping row (74 + 858 + 28). The hypothetical main size of the outer row
probably comes from a measurement pass that does not wrap the inner row at its
`max-width`. That cause is unconfirmed. No Radiant source was changed.

**Resolution (2026-11-11):** FIXED under D9.1.1v4 / D9.1.3v2
(native layout ownership). Confirmed cause: the wrapping-row intrinsic-height
walk measured each child using the parent's full width, even after computing
a narrower hypothetical child width from `max-width`. The inner row therefore
reported a single-line height during column flex sizing. The query now receives
the child's content width derived from its hypothetical border box; percentage
resolution retains the parent's containing-block basis.

`DynamicLayoutTest.ColumnFlexMeasuresNestedWrappingRows` first reproduces the
reported **102 / 858 / footer y=960** mismatch, then verifies **102 / 830 /
footer y=932** after repair. It also checks retained recascade/reflow after
narrowing the inner maximum. The nowrap-toolbar and nowrap-child controls pass.
The complete dynamic native suite passes **35/35**. Receipts:
`temp/ledger-flex-before.{log,json}`, `temp/ledger-layout-after.{log,json}`.
The restored owner-local layout-cast gate passes (see RI-009). Aggregate
qualification is recorded below.


<a id="rad22-7"></a>

#### RAD22-7

**GIF finish leaves a dangling `surface->pixels` briefly.** `gif_animation_finish` frees the frames, after which `surface->pixels` points at freed memory until it is nulled two lines later (`gif_player.cpp:55-64`) — a narrow window, but any interleaved read during teardown would be a use-after-free.

**Resolution (2026-11-11):** FIXED under D5.5.1 / D9.1.1v4.
The current `GifAnimation::finish` still freed `GifFrames` before switching
`surface->pixels` away from the borrowed frame. Teardown now restores the
cache-owned still (including pitch, dimensions, alpha mode and generation),
or detaches pixels, before releasing frames. This closes the transient dangling
reference; no concurrent-reader failure was reproduced or claimed.
`test_gif_player_gtest` passes **23/23**, including finish/cancellation controls.
Receipt: `temp/ledger-gif-after.{log,json}`.


<a id="rad01-5"></a>

#### RAD01-5

**Class-name handling is still a single-token stub.** The deprecated `RDT_VIEW_MATH` enum span has been removed, but the class-name path in `view_pool.cpp` still assumes a single class ("TODO: split on whitespace for multiple classes").

**Reverified (2026-11-11):** RESOLVED by existing code under D9.1.1v4.
`view_pool.cpp` now shares `append_class_list_json` across dump paths and walks
whitespace-separated tokens; selector-name output also splits the class list.
The reported single-token TODO no longer exists. This is a source audit of the
reported stub, not a claim about every DOM class mutation path.


<a id="rad02-4"></a>

#### RAD02-4

**No named colors in the HTML-attr parser.** `parse_html_color` (`resolve_htm_style.cpp:90`) handles hex/`rgb()` only, with a TODO for named colors (`resolve_htm_style.cpp:100`). Because presentational attributes use this parser and CSS uses `resolve_color_value`, a named color works in a stylesheet but not in a `bgcolor` attribute.

**Reverified (2026-11-11):** RESOLVED by existing code under D9.1.1v4.
`parse_html_color` delegates to input's `css_parse_color`, including named
colors. The added native `HtmlColorAttributesUseNamedCssColors` control passes
for body/table-cell backgrounds and mixed-case font color attributes.
Receipt: `temp/ledger-layout-after.{log,json}`.


<a id="rad02-6"></a>

#### RAD02-6

**Two color parsers.** `parse_html_color` (htm) vs `resolve_color_value` (css) — see item 4. Consolidating would remove a class of "works one place, not the other" bugs.

**Reverified (2026-11-11):** RESOLVED by existing code under D9.1.1v4.
The HTML attribute adapter uses the shared CSS color parser; it no longer
contains an independent hex/rgb parser. The RAD02-4 native color control passes.
This closes parser duplication, not full legacy HTML color-algorithm coverage.


<a id="rad05-1"></a>

#### RAD05-1

**Dead `IntrinsicSizeCache` struct.** The header struct (`layout.hpp`), including its four-slot per-width `HeightCacheEntry` height cache, has zero references anywhere. The real cache is the four `DomElement` fields (`dom_element.hpp:434-437`) and caches **width only**. *Improvement:* delete the struct, or wire up its height cache to fix issue 2.

**Reverified (2026-11-11):** RESOLVED by existing code under D9.1.1v4.
`IntrinsicSizeCache` and its unused `HeightCacheEntry` are absent from current
Radiant sources. The live intrinsic-height query has its own owner-scoped cache;
its scope/remaining measurement correctness is separate from this dead-struct
report. No production edit was needed.


<a id="rad06-8"></a>

#### RAD06-8

**Legacy `@font-face` cruft in the font bridge.** `font_face.cpp:341` `parse_font_face_rule_OLD` is a dead lexbor path (fonts are RAD07's domain, cross-linked here only because it is reached from the inline text path via `setup_font`).

**Reverified (2026-11-11):** RESOLVED by existing code under D9.1.1v4.
`parse_font_face_rule_OLD` and its commented lexbor implementation are absent
from `font_face.cpp`. This duplicate report and RAD07-2 are closed together;
no new font capability is claimed.


<a id="rad07-2"></a>

#### RAD07-2

**Legacy dead code in `font_face.cpp`.** `parse_font_face_rule_OLD` (`font_face.cpp:341-385`) is an entire lexbor-dependent function, including a hardcoded "Liberation Sans" registration and a `rule == NULL` "hardcoded @font-face" path, left inside a block comment. It is dead but clutters the file and encodes a stale design. *Improvement:* delete it.

**Reverified (2026-11-11):** RESOLVED by existing code under D9.1.1v4.
The reported dead legacy function and hardcoded registration block are absent
from current `font_face.cpp`; the native archive builds without them.
See the duplicate RAD06-8 record.


<a id="rad08-1"></a>

#### RAD08-1

**Fixed-size global measurement cache.** `static MeasurementCacheEntry measurement_cache[1000]` with `static int cache_count` (`layout_flex_measurement.cpp:555`) is a process-global, capacity-capped array ("Fixed size for simplicity") — not per-document. Overflow logs an error and drops the entry (`:571`); a document with more than 1000 measured nodes silently loses caching, and there is a theoretical cross-document bleed risk mitigated only by the generation counter. *Improvement:* a per-`ViewTree` hash-map cache with real eviction.

**Reverified (2026-11-11):** RESOLVED by existing code under D5.5.1 /
D9.1.1v4. Flex measurement entries, capacity and generation now belong to
`ViewTree`. `ensure_measurement_cache_capacity` uses `lam::mem_grow_array`
and logs allocation failure; the global 1000-entry array is absent.
This source audit closes the fixed-global-cap report, not every retained-cache
invalidation concern (RAD04-4R remains open).


<a id="rad08-4"></a>

#### RAD08-4

**`has_flex_item_prop` duplicated across all three files.** It is re-defined as a `static` in `layout_flex.cpp:39`, `layout_flex_measurement.cpp:438`, and `layout_flex_multipass.cpp:268`, and the measurement file additionally carries its own `flex_measurement_direction_is_row` (`:68`) parallel to `is_main_axis_horizontal`. Divergence between copies is a live risk. *Improvement:* hoist the shared predicates into `layout.hpp`.

**Reverified (2026-11-11):** RESOLVED by existing code under D9.1.1v4.
`has_flex_item_prop` has one shared inline definition in `layout.hpp`, consumed
by all three flex translation units. The duplicate direction predicate is also
absent. The archive and all three nested-flex controls build and pass.


<a id="rad14-4"></a>

#### RAD14-4

**Fixed clip-stack depth in the ThorVG backend.** `RDT_MAX_CLIP_DEPTH` is hard-coded to 8 (`rdt_vector_tvg.cpp:1461`); overflow is logged and the clip dropped (`:1476`). Deeply nested SVG clip paths beyond depth 8 silently lose clipping.

**Reverified (2026-11-11):** RESOLVED by existing code under D9.1.1v4.
The ThorVG backend uses a thread-local inline clip stack that grows on demand
via `ensure_clip_capacity`; `RDT_MAX_CLIP_DEPTH` and the depth-eight clip-drop
branch are absent. Allocation failure is logged. This is source verification of
the reported fixed cap, not qualification of every nested SVG clip combination.


<a id="rad16-2"></a>

#### RAD16-2

**Hard parser caps silently truncate.** `parse_keyframes_content` caps at 64 stops and 32 declarations per stop (`css_animation.cpp:343`/`347`), with 64-byte name and 256-byte value buffers (`css_animation.cpp:413`/`434`). Overflow is dropped without a warning. *Improvement:* at least `log_warn` on overflow, or grow dynamically.

**Reverified (2026-11-11):** RESOLVED by existing code under D9.1.1v4.
`parse_keyframes_content` grows stop/property/offset arrays with
`lam::pool_grow_array` and duplicates names/values at their actual lengths.
The 64-stop, 32-declaration and fixed-buffer truncation paths are absent.
This source audit closes those specific hard-cap reports; it does not qualify
all CSS animation syntax or property coverage (RAD16-3 remains separate).


<a id="rad17-3"></a>

#### RAD17-3

**`s_in_batch` is a file-static global**, not a `DocState` field. It gates whether mutation-time asserts fire, so it is not per-document and is unsafe if multiple documents or threads mutate state concurrently. *Improvement:* move it into `DocState` alongside `active_cascade_depth`/`transition_depth`.

**Reverified (2026-11-11):** RESOLVED by existing code under D9.1.1v4.
The batch guard is `DocState::state_batch_depth`; `state_begin_batch` increments
it and `state_end_batch` publishes only at the outermost boundary. The added
`BatchesNestIndependentlyAcrossDocuments` control verifies nested/interleaved
batches on two documents and an unmatched end, without cross-document changes.
The state-store suite passes **38/38**. Receipt: `temp/ledger-state-after.{log,json}`.


<a id="ri-009"></a>

### RI-009 — Required standalone layout lint target is absent

**Status:** OPEN — verification tooling gap. **Observed:** 2026-11-11,
macOS arm64. **Lambda blocker:** no. Authority: D9.1.1v4 / D9.1.3v2.

Reproduce from Radiant with
`make lint ARGS='--rule ^no-int-cast-radiant$'`: Make exits 2 because the owner
Makefile has no `lint` target. The sibling Lambda pattern-rule inventory also
lacks that exact rule. An attempted type-aware fallback through
`../Lambda/utils/lint/tidy/explicit_cast_check.py radiant/intrinsic_sizing.cpp`
exits before scanning because this Python environment lacks `clang.cindex`.
Expected: an owner-local, runnable layout-cast gate rather than a silent empty
rule selection. This blocks lint qualification of RI-007, not its focused
geometry checks. No integer geometry or integer casts were added by that fix.
Evidence: `temp/ledger-layout-lint.log`, `temp/ledger-layout-type-lint.log`.

**Resolution (2026-11-11):** FIXED under D9.1.1v4 / D9.1.3v2.
Radiant's Makefile now owns `lint` and `test-lint`. The owner-local ast-grep
rule restores the same `no-int-cast-radiant` pattern and file scope formerly
in Lambda (`59ebad8066^:utils/lint/rules/c-cpp/no-int-cast-radiant.yml`),
without depending on sibling scripts or `clang.cindex`. The runner fails on
an empty rule selection, missing scanner, scanner errors or unmarked casts.
Same-line `INT_CAST_OK` exceptions require a reason.

The audit adds reasons to seven existing byte/count casts and the type-erased
integer node-budget payload, and moves the existing grid-count explanation
onto its cast line. No layout arithmetic changed. The required command passes:
**89 cast matches checked, zero unmarked**. Five gate contract tests pass,
including rejection of an unmarked geometry cast, a reasonless marker, a
marker on another line and an unknown rule selector.
Receipts: `temp/ledger-layout-lint-after.log`, `temp/ledger-lint-contracts.log`.


<a id="ri-006"></a>

### RI-006 — Property providers still live outside the unified Radiant archive

**Status:** RESOLVED — verified 2026-11-09 against Radiant `558eb3e`.
**Observed:** 2026-11-09, macOS arm64. **Lambda blocker:** cleared for the
current-source MVP release build and validation.
Authority: **D9.1.1v4 / D9.1.3v2**; the current Lambda
[split implementation checkpoint](../../Lambda/vibe/impl/Lambda_Impl_Radiant_Split.md#current-checkpoint)
requires both property providers in `libradiant.a`, with no separate property
archive target.

Lambda HEAD `1540d9228429f3385c1526d3bfabc85d715a423e`; Radiant HEAD `0c664052c753b66016e34267e65fe59bd11ce6bd`.
From the Lambda checkout, reproduce with:
`make build-lambda-split SPLIT_PROFILE=release SPLIT_OUTPUT=temp/mvp_untyped_tuning_next/starting-release.exe JOBS=2`.
The native archives build, but the host link fails with undefined
`BLOCK_PROP_DEFAULT`, `BOUNDARY_PROP_DEFAULT`, `EMBED_PROP_DEFAULT`,
`GRID_ITEM_PROP_DEFAULT`, and numerous property-provider symbols.

The cause is confirmed in the owning build: `Makefile`'s `libradiant.a` rule
archives only `NATIVE_OBJECTS`; `PROPERTY_OBJECTS` remain in the separate
`libradiant-properties.a` rule. `build_targets.json` still declares the separate
`radiant-properties` target. `ar t build/lib/release/libradiant.a` contains neither
`view_prop_defaults.cpp.o` nor `view_prop_ensure.cpp.o`, while the property
archive contains both. The merged Lambda integration deliberately removed the
separate provider link and its checks require each provider once in Radiant.
Expected: the owner manifest/build produce the unified archive containing both
providers once, so the existing Lambda consumer links without restoring the
retired separate dependency.

Evidence in the Lambda checkout:
`temp/mvp_untyped_tuning_next/starting-tree.json`, `starting-build.log`,
`libradiant.a.provider-members.txt`, `libradiant-properties.a.provider-members.txt`;
full link diagnostics: `temp/build_lambda-exe-split.log`.
No Radiant source/build edits or Lambda link workarounds were made. MVP
implementation/timing was stopped under the project-boundary rule pending the
owner repair. No baseline suite was run.

**Resolution verification:** the updated owner manifest removes the separate
property target and includes both providers in the native source list. Rebuilt
`build/lib/release/libradiant.a` contains `view_prop_defaults.cpp.o` and
`view_prop_ensure.cpp.o` exactly once; the four default symbols listed above
are defined. The original Lambda split release build/link now exits zero with
no dependency workaround. Typed and untyped Fibonacci goldens pass under MVP
and Full MIR (4/4 smoke runs). This closes the archive/link blocker, not broader
language or performance qualification (D9.1.1v4, D9.1.3v2).

Verification: `Lambda/temp/mvp_untyped_tuning_next/ri003-verification.json`,
`ri003-rebuild.log`, `ri003-successful-link.log`, and `ri003-recheck-tree.json`.
The private host SHA256 is
`cda6b0f4985ac9e440494d8b86badbbbe2c7d386e86e6483e1e45b7a4fb14363`.
The original failing link log is preserved as `ri003-failed-link.log`.


<a id="ri-008"></a>

### RI-008 — font face source includes a missing alloca header

**Status:** RESOLVED — Radiant now includes the selected input header at
`lib/alloca.h`.
**Observed:** 2026-11-10, macOS arm64 debug `make build`.
**Lambda blocker:** yes — the split build cannot compile `libradiant.a`
(D9.1.1v4, D9.1.3v2).

Reproduce from the Lambda checkout with `make build`. The build reaches
Radiant's `radiant/font_face.cpp` and fails at line 11 with
`fatal error: 'lib/lambda_alloca.h' file not found`. The command supplies
`-I/Users/henryluo/Projects/lambda-input`, but that checkout has
`lib/alloca.h` and no `lib/lambda_alloca.h`; a search across the Lambda, input
and Radiant checkouts found no file with the requested name. The current input
header defines `LAMBDA_ALLOCA`. Expected: Radiant compiles against the current
selected input headers and the Lambda split build proceeds.

Radiant's three consumers now include `lib/alloca.h`, preserving the existing
`LAMBDA_ALLOCA` call sites and behavior. This updates `font_face.cpp`, the
reported blocker, and the same stale include in `render_border.cpp` and
`rdt_vector_tvg.cpp`. A focused debug compilation of all three translation
units passed.


## 2026-11-11 owner qualification

Under D9.1.1v4 / D9.1.3v2, `make -j8 run-radiant-baseline` passes:
**875 native tests plus one static corpus smoke test**, with one hardware-only
MIDI skip; **7/7 runner contracts**; **3,851 static and 630 dynamic geometry
comparisons**, with six pre-existing static corpus skips and no failures.
The 44 formerly skipped integration cases now execute. No baseline tolerance,
reference, corpus file or sibling implementation was changed.

`make lint ARGS='--rule ^no-int-cast-radiant$'` passes with 89 checked casts;
`make test-lint` passes 5/5; the input-facade and diff whitespace checks pass.
The final follow-up to the aggregate changes only cast-reason comments,
owner lint tooling/tests, build target/documentation wiring and ledger records;
production behavior remains that of the qualified aggregate. Broader imported
feature/refactoring work and source-audit-only limitations remain explicit.

Receipts: `temp/ledger-radiant-baseline.log`,
`temp/layout-baseline/results.log`, `temp/ledger-layout-lint-after.log`,
`temp/ledger-lint-contracts.log`, and the focused before/after logs above.


## 2026-11-11 continued owner repairs and re-verification

<a id="rad03-2"></a>

#### RAD03-2

**Hard-coded float-width estimate.** `layout_block.cpp:4123` uses `float float_width = 100.0f; // Conservative estimate` in place of a real float measurement, so float space reservation can be wrong for floats whose actual width differs materially from 100px.

**Resolution (2026-11-11):** removed the obsolete 100px float-width and
8px-per-character prescan gate from `prescan_and_layout_floats`. The placement
walk already stops at preceding in-flow content and lays each admitted float
out through `layout_block`; a guessed combined width cannot improve that
decision. The preliminary scan still rejects floats behind a preceding block
and avoids work when no float exists. This is removal of redundant estimation,
not a claimed newly reproduced pixel mismatch. The unchanged original controls
and repaired source both pass `FloatPrescanUsesActualGeometryAndFlowOrder`,
covering leading floats, empty preceding inlines, preceding blocks and retained
width changes through 180px, 40px and 220px. Duplicate report: RAD04-2.
Authority: **D9.1.1v4 / D9.1.3v2**.

<a id="rad04-2"></a>

#### RAD04-2

**Hard-coded float-width estimate.** `layout_block.cpp:4123` uses `float float_width = 100.0f; // Conservative estimate` instead of measuring the float, so a float's effect on available width is guessed. *Improvement:* run `layout_measure_intrinsic` for the float and use its max-content width.

**Resolution (2026-11-11):** the same obsolete prescan gate as
[RAD03-2](#rad03-2) was removed. Actual float layout remains the sizing authority;
no replacement estimate or hard-coded measurement was added. The original
geometry controls pass before and after the cleanup. Authority:
**D9.1.1v4 / D9.1.3v2**.

<a id="rad04-7"></a>

#### RAD04-7

**`KnownDimensions` counts only positive givens as known.** `layout_known_dimensions_from_block` (`layout_pass.cpp:185`) treats a `given_width` of exactly 0 as *unknown*, so a legitimately zero-width element cannot key slot 0. This is usually benign but is an implicit contract worth documenting at the call sites.

**Resolution (2026-11-11):** reproduced a real cache-key collision:
`layout_known_dimensions_from_block` returned an unknown axis for a specified
zero width or height, and the resulting entry was incorrectly reusable for an
`auto` query. It now uses the same nonnegative-given contract as the context
helper and reads properties without mutable access. Negative givens remain
unknown. `LayoutCacheTest.ZeroDimensionsRemainDistinctFromAutoCacheKeys`
fails on the original source for both axes and passes after the repair,
including actual cache store/query isolation. Receipts:
`temp/ledger-next-layout-before.{json,log}` and
`temp/ledger-next-layout-final.{json,log}`. Authority:
**D9.1.1v4 / D9.1.3v2**.

<a id="rad07-1"></a>

#### RAD07-1

**Duplicated CSS→`FontStyleDesc` mapping.** The weight/slant mapping exists twice: inline in `setup_font` (`font.cpp:109-129`) and in `font_style_desc_from_prop` (`view.hpp:437`), and a third variant maps CssEnum→`FontWeight` in `register_font_face` (`font_face.cpp:438-447`). They can drift (e.g. `font.cpp` handles `bolder`/`lighter` keywords; `register_font_face` handles only `bold`/`normal` + numeric range). *Improvement:* have `setup_font` and `register_font_face` both route through one shared mapping helper.

**Resolution (2026-11-11):** `setup_font` already called
`font_style_desc_from_prop`, but that helper still duplicated the conversion
used by font-face registration. More seriously, the font-face helper interpreted
`CSS_VALUE_NORMAL` (318) as numeric weight 318 because CSS keyword IDs overlap
the numeric interval. The shared weight helper now decodes keywords before
numeric weights; `font_style_desc_from_prop` delegates both weight and slant
to the same helpers as `register_font_face`. Explicit numeric-property weights,
including 950, retain precedence. `FontStyleTest` reproduces the original
normal/bolder/lighter and encoded-numeric discrepancies; the native
`NormalFontFaceRegistersTheNormalWeight` case checks the real registry entry is
400. Receipts: `temp/ledger-next-font-before.{json,log}`,
`temp/ledger-next-font-after.{json,log}` and
`temp/ledger-next-layout-final.{json,log}`. Authority:
**D9.1.1v4 / D9.1.3v2**.

<a id="rad15-5"></a>

#### RAD15-5

**`event_sim_print_results` uses `fprintf(stderr,...)`.** Scoped to the test harness, but a deviation from the project no-`fprintf` guidance.

**Resolution (2026-11-11):** replaced the result banner's direct
stderr writes and the utility `SIM_EVENT_LOG` write with project logging,
each prefixed `event_sim:`. Passing summaries use `log_info`; a failing summary
uses `log_error`. The result JSON schema, counts and file-writing path are
unchanged. A source check finds no remaining `fprintf` or direct console writes
in `event_sim.cpp`; string-buffer `snprintf` calls remain. `log.conf` was not
changed. Authority: **D9.1.1v4 / D9.1.3v2**.

**Correction (2026-11-11):** that cleanup incorrectly treated the human replay
result as debug output. With filtered/disabled logging, the seven view-oriented
UI fixtures and many Radiant view controls lost their required `Assertions:`
and `Result:` lines despite passing machine results. `event_sim_print_results`
now formats its existing user-facing banner into a project string buffer and
writes it independently of logging. Diagnostics still use the logging API;
the JSON result schema and counts remain unchanged. The native
`EventSimResults.HumanAndMachineResultsRemainAvailableWithLoggingDisabled`
regression fails before the repair and passes afterward for success, failure
and no-assertion cases. Evidence lives under
`temp/radiant-failed-20261111/result-contract-{before,after}.{json,log}`;
integrated-host qualification is recorded separately when complete.

<a id="rad01-3"></a>

#### RAD01-3

**Incremental relayout correctness is implicit.** The skip-clean optimization depends on `layout_height_contribution` being exactly recomputed on every non-skipped child (`layout_block.cpp`). There is no assertion tying the cached contribution to the actual advance, so a bug there manifests as silently mis-placed siblings rather than a crash.

**Reverified (2026-11-11):** the current
`layout_can_shift_clean_block` rejects reuse unless the cached height
contribution matches the finite used height within its existing 0.1px check.
It also excludes margins/collapse, floats, vertical flow and unstable subtrees.
The normal path recomputes the contribution from the actual advance delta.
The historical implicit-only precondition is therefore already explicit in
the reuse gate; an assertion is not needed to reject that invalid reuse.
This source audit does not establish every incremental-layout combination.
No production change was needed. Authority: **D9.1.1v4 / D9.1.3v2**.

<a id="rad03-4"></a>

#### RAD03-4

**BFC line-break detection uses a hard-coded inline tag whitelist.** `layout_block.cpp:7562-7578` decides whether a block should suppress the parent line break by matching a long list of inline/inline-block tag ids (`HTM_TAG_SPAN`, `HTM_TAG_A`, …) *before* styles are resolved. This is fragile: a new inline-level element, or an unusual tag defaulting to inline via CSS, will be mis-classified. *Improvement:* defer the decision until after `dom_node_resolve_style`, or drive it from the UA stylesheet's default `display` rather than a tag list.

**Reverified (2026-11-11):** the tag whitelist no longer exists.
The current `layout_block` line-break decision uses its resolved display,
inline-atomic/table classification, float state and out-of-flow state, rather
than a span/a/etc. tag inventory. Animated `display:none` also restores the
saved parent context. This closes the named obsolete whitelist report, not
all possible block/inline interactions. No production change was needed.
Authority: **D9.1.1v4 / D9.1.3v2**.

<a id="rad04-5"></a>

#### RAD04-5

**`max_iterations` caps truncate silently.** The float-avoidance / find-y loops in the block driver and `block_context_find_y_for_width` cap at 100 iterations with no error logged when the cap is hit (see RAD03); a pathological float stack silently stops converging. Mentioned here because the sizing services (`compute_shrink_to_fit_width`, percentage resolution) feed those loops.

**Reverified (2026-11-11):** all four current capped float searches
already log exhaustion: `RAD_CAP_FLOAT_FIND_Y` in `block_context.cpp`,
`RAD_CAP_FLOAT_PLACE` in `layout_positioned.cpp`, and `RAD_CAP_FLOAT_LINE_Y` /
`RAD_CAP_BFC_FLOAT_AVOID` in `layout_block.cpp`. This closes the report that
the caps truncate silently; the limits themselves remain. Source verification
only; no cap was weakened and no pathological convergence claim is made.
Authority: **D9.1.1v4 / D9.1.3v2**.

<a id="rad08-6"></a>

#### RAD08-6

**space-evenly workaround.** `space-evenly` is routed through a custom `x-justify-content` property in style resolution rather than a first-class CSS value (`layout_flex_multipass.cpp:831` note). This is a resolution-layer patch that should be normalized.

**Reverified (2026-11-11):** standard `justify-content:space-evenly`
is parsed by the shared content-alignment parser, stored as
`CSS_VALUE_SPACE_EVENLY` and consumed by native space distribution. The old
`x-justify-content` route is absent. The native
`StandardSpaceEvenlyDistributesEdgeAndInteriorGaps` control verifies equal
edge/interior gaps at widths 400px and, after retained mutation, 200px.
No production change was needed. Authority: **D9.1.1v4 / D9.1.3v2**.

<a id="rad13-2"></a>

#### RAD13-2

**PDF success paths emit `log_error` noise.** Multiple non-error PDF outcomes log at error level — e.g. "fallback effect group rendered as passthrough content" (`render_pdf.cpp:1033`) and the raster-fallback effect-group notice (`:598`) fire on the *success* path where a fallback is the intended behavior. This makes real PDF errors hard to spot. *Improvement:* demote intended fallbacks to `log_debug`.

**Reverified (2026-11-11):** `render_pdf.cpp` now delegates PDF
writing through `radiant_pdf_export_write` and has no `log_error` calls or
the reported effect-fallback success notices. The former native implementation
is gone. This source-level result concerns the Radiant wrapper only; it does
not qualify diagnostics in an optional external PDF writer. No production
change was needed. Authority: **S2.6.6v3 / D9.1.1v4 / D9.1.3v2**.

<a id="rad22-9"></a>

#### RAD22-9

**`WebViewProp.src`/`srcdoc` are borrowed pointers.** They are documented as borrowed DOM-attribute storage in `radiant.hpp`; their lifetime is tied to the DOM node and is not retained through a `view_pool_reset_retained` (RAD01) — a relayout that frees attribute storage while a webview holds `src` would dangle. No `PersistentFieldRef` guards them.

**Reverified (2026-11-11):** source admission already uses
`radiant_retain_webview_src` / `radiant_retain_webview_srcdoc`, which store
pool-domain pointers through `PersistentFieldRef`. The historical claim that
those guards are absent is obsolete. The owning document/pool lifetime remains
the contract; this source audit is not a platform-webview mutation/teardown
stress test. No production change was needed. Authority:
**D5.5 / D9.1.1v4 / D9.1.3v2**.

## 2026-11-11 continued owner qualification

Under **D9.1.1v4 / D9.1.3v2**, `make -j8 run-radiant-baseline` passes on the
complete repaired source: **882 native tests plus one static corpus smoke
test**, with one existing hardware MIDI skip; **7/7 runner contracts**;
**3,851 static and 630 dynamic geometry comparisons**, with six existing static
fixture skips and no failures. The focused native layout suite passes **41/41**
and the font suite **4/4**. No baseline tolerance, reference, fixture corpus or
sibling implementation was changed.

`make lint ARGS='--rule ^no-int-cast-radiant$'` passes with **89 checked casts
and zero unmarked casts**; `make test-lint` passes **5/5**. The input-facade
and whitespace checks pass. The grid baseline entries remain explicitly
PARTIAL, including flex, spanning-row and orthogonal qualification limits.
The six source-reverified historical reports above retain their narrower
verification limits.

Receipts: [verification summary](../temp/ledger-next-verification-summary.json),
[aggregate log](../temp/ledger-next-radiant-baseline.log),
[geometry results](../temp/ledger-next-layout-baseline.log),
`temp/ledger-next-layout-final.{json,log}`, `temp/ledger-next-lint.log`, and
`temp/ledger-next-lint-contracts.log`. The verification summary pins the host
and changed source/header/test hashes. Only ledger documentation changed after
the passing aggregate.


## 2026-11-11 flex, media and layout-cache delivery

<a id="rad22-6"></a>

#### RAD22-6

**Poster image is not plumbed.** The idle/loading branch of `render_video_frames` (`render_video.cpp:452-457`) comments that a poster "would need to be passed through" and falls back to the play button; `poster` attribute support is missing.

**Resolution (2026-11-11):** poster images are admitted independently of a
playable source and rendered through the normal image path before playback.
Object-fit/position, ancestor/content clips and retained image ownership share
existing image mechanisms. Removing, replacing or failing a poster load clears
its old borrow. Display-list and PaintIR placeholders carry a poster-presence
bit, without another resource pointer. The compositor rechecks playback so a
live frame wins over a retained pre-play recording; ready/idle cached media
keeps the poster. Missing/broken images preserve the existing fallback.

**Evidence:** the original CLI captures in `temp/ledger-impl/` show only the
video background. Native pixel tests now cover no source, preload:none,
contain letterboxing, cover/ancestor clipping, replacement/removal and a broken
poster. The macOS decoder test also reproduces the retained-placeholder bug
before its compositor repair and verifies live-frame pixels afterward. Its
sandboxed audio-decoder start failure is retained separately from the passing
native rerun. Receipts: `temp/ledger-impl-layout-final.{json,log}` and
`temp/ledger-impl-retained-poster-{before,after}.{json,log}`. The
[implementation record](Radiant_Impl_Ledger_Repairs.md) states the delivered
scope. Authority: **D5.5 / D9.1.1v4 / D9.1.2v2 / D9.1.3v2**.

### Core delivery qualification

Under **D9.1.1v4 / D9.1.3v2**, the final `make -j8 run-radiant-baseline`
passes **891 native tests plus one static corpus smoke test**, with one
existing hardware MIDI skip; **7/7 runner contracts**; **3,851 static and 630
dynamic geometry comparisons**, with six existing static skips and no failures.
The final focused layout suite is **49/49**. The earlier aggregate's one
`align_baseline_nested_column` failure was repaired and retained as a regression;
fresh Chrome and the unchanged corpus reference agree. Its failed aggregate
remains in `temp/ledger-impl-radiant-baseline-before-nested.log`.

Layout-cast lint passes **89 checked / zero unmarked**, lint contracts **5/5**,
and input-facade/diff checks pass. Native poster pixels, the macOS playback
transition and PNG/SVG visual controls pass. No tolerance/reference/corpus or
sibling implementation changed. Broader baseline, whitespace and decomposition
reports remain PARTIAL as described in the implementation record.

Receipts: [verification summary](../temp/ledger-impl-verification-summary.json),
[aggregate log](../temp/ledger-impl-radiant-baseline.log),
[geometry results](../temp/ledger-impl-layout-baseline.log), and
[implementation record](Radiant_Impl_Ledger_Repairs.md). The summary pins the
host and changed source/header/test identities; only documentation changed
after the passing aggregate.

## Continued native clip and default-size repairs — 2026-11-11

**Final owner qualification:** `make -j8 run-radiant-baseline test-native-closure`
passes 893 native tests plus static smoke, 7 runner contracts, all 3,851 static
and 630 dynamic geometry comparisons and native archive closure. One hardware
MIDI test and six static fixtures retain their existing skips. Layout-cast lint
checks 89 casts with zero unmarked; its five contracts, the input facade and
diff checks pass. The [receipt](../temp/ledger-next-verification-summary.json)
pins source and executable/archive hashes; only documentation changed afterward.
Six PNG/SVG clip states independently match fresh Chrome pixels. The original
failing native clip report remains in `temp/ledger-next-clip-before.{json,log}`.

<a id="rad02-3"></a>

#### RAD02-3

**`clip: rect()` not parsed.** `resolve_css_style.cpp:9908` has a TODO to parse `rect()` values into scroller clip bounds; currently ignored.

**Resolved (2026-11-11):** the native dispatcher now resolves rectangle lengths and auto edges into view-owned position data. Shared border-box geometry clips the positioned HTML owner, descendant paint, vector output and native hits, including retained updates. Two native controls pass; six raster/vector states match fresh Chrome pixels. See the [implementation and scoped limits](Radiant_Impl_Ledger_Clip_Defaults.md#rad02-3--legacy-rectangle-clipping) and [qualification receipt](../temp/ledger-next-verification-summary.json). Authority: **D5.5 / D9.1.1v4 / D9.1.3v2**.

<a id="rad03-3"></a>

#### RAD03-3

**Replaced-element 300×150 defaults are duplicated.** The HTML default replaced size appears as scattered literals — e.g. `layout_block.cpp:5849` (`... : 300.0f`) and `5864` (`given_height = 150.0f`), plus the `layout_measure.cpp` intrinsic path (RAD05). Correct per spec but duplicated rather than centralized in one constant, so a change must be made in several places.

**Resolved (2026-11-11):** replaced width/height and audio height now have one native constant definition. Inline SVG, HTML hints, sizing, SVG metadata and picture fallbacks share those constants or the existing `layout_replaced_default_size` provider. Full geometry qualification preserves existing sizes; see the [implementation record](Radiant_Impl_Ledger_Clip_Defaults.md#rad03-3--rad04-3--replaced-defaults). Authority: **D9.1.1v4 / D9.1.3v2**.

<a id="rad04-3"></a>

#### RAD04-3

**Hard-coded replaced defaults are duplicated.** The 300×150 (and 300×54 audio) intrinsic defaults live both in `layout_measure.cpp:105`+ and scattered through `layout_block.cpp` (SVG/iframe/hr paths). Centralizing them into one table would remove the drift risk.

**Resolved (2026-11-11):** the duplicate replaced defaults are consolidated with [RAD03-3](#rad03-3); the existing tag-based sizing provider remains authoritative. See [implementation and qualification](Radiant_Impl_Ledger_Clip_Defaults.md#rad03-3--rad04-3--replaced-defaults). Authority: **D9.1.1v4 / D9.1.3v2**.

<a id="rad16-5"></a>

#### RAD16-5

**`radiant_frame_clock_start` has dead trailing statements.** The `#else` fallthrough stores `running = 0` and returns `false`, but the two statements after the `#endif` (`frame_clock.cpp:369`-`370`) are unreachable on every platform — harmless but confusing.

**Resolved (2026-11-11):** removed the shared trailing store/return after the platform selection. The macOS defensive mode rejection remains inside its own branch, and all branches now return explicitly. Native animation/scheduler regressions remain in the aggregate gate. See the [implementation record](Radiant_Impl_Ledger_Clip_Defaults.md#rad16-5--frame-clock-control-flow). Authority: **D9.1.1v4 / D9.1.3v2**.

<a id="ri-028"></a>

### RI-028 — Auto-height flex columns shrink natural content to their minimum

**Status:** FIXED, 2026-11-12. **Lambda blocker:** cleared for the reduced
ImageMagick header. Ownership **D9.1.1v4 / D9.1.3v2**. ImageMagick uses a column
body with `min-height:100vh` and an overflow-hidden hero. The initialization
used that minimum as available main size before natural line sizing. Flex
shrink then reduced the hero to its padding and clipped its heading/actions.
A reduced 800px-minimum column shrank a 340px header to 40px and a 100px footer
to 0px despite 1000px of main content; the auto container should grow to 1440px.

The existing natural column-size pass now selects the larger of the minimum
used size and natural hypothetical lines when the main size is not definite.
A new regression fails before and passes after, including short-content
minimum/growth controls. All 69 native dynamic tests pass; all 3851 recorded
static layout baselines pass with 6 skips and no changed tolerances. The layout
integer-cast lint passes. The repaired offline full ImageMagick document's
hero grows from 105px to 604.4px; exact font/spacing parity is not claimed.
Evidence: Lambda `temp/online-visual-round3/{flex-before.log,dynamic-after.log,
baseline-layout.log,imagemagick-local.json,imagemagick-after.json}`.


<a id="ri-030"></a>

### RI-030 — Grid row growth leaves stale scroll clips that hide item contents

**Status:** FIXED for the isolated defect, 2026-11-12. **Lambda blocker:**
cleared for the reduced paint case; full DigitalOcean content remains under
investigation. Ownership **D9.1.1v4 / D9.1.3v2**. DigitalOcean's missing content
led to a small two-row grid reproduction: an overflow-auto main item has final
height 1400px, but `setup_scroller` clips its children at a cached height 0px.
An adjacent sidebar's cached clip is 18px while its final used height is 800px.
`layout_final_grid_content` published scroll bounds before content-driven row
growth and final alignment changed those used boxes.

The existing publication now runs after row growth/alignment, before sticky
positioning. No new clip implementation or page-specific rule was added.
`GridScrollBoundsUseFinalContentGrownRows` fails before on both stale-state
assertions and passes after; the reduced PNG's interior pixel changes from
parent yellow to child red. All 70 native dynamic tests and all 3851 recorded
static layout baselines pass (6 skips); integer-cast lint passes. The full
DigitalOcean offline page still loses main content after this repair, so this
entry does not claim its entire visual mismatch is solved.
Evidence: Lambda `temp/online-visual-round3/{grid-before.log,dynamic-grid-after.log,
baseline-grid.log,grid-scroll.html,grid-scroll.png,grid-scroll-after.png,
do-after-debug.log}`. The final 142 grid-using candidates are refreshed separately.

<a id="ri-032"></a>

### RI-032 — Flex SVG automatic cross sizes ignore the flexed main size

**Status:** FIXED, 2026-11-12. **Lambda blocker:** cleared for the reduced
Opus announcement. Native layout ownership follows **D9.1.1v4 / D9.1.3v2**.
An SVG with `viewBox="0 0 20 20"` and CSS `width:14px` inside a flex row
receives a provisional intrinsic height of 1,184px from available space.
Cross-size finalization used only authored aspect ratios, so it never
transferred the SVG's natural ratio from the final 14px main size. The icon
made the announcement 1,200px tall and pushed the hero below the viewport.

Finalization now uses the existing preferred/natural ratio query and existing
border/content-box transfer helper. The regression fails before and passes
after for physical/logical widths, non-square viewBoxes, padding/borders,
explicit independent height and width mutation. The full static Opus document
now has a 32.8px announcement and its main begins at 101.8px, rather than an
announcement above 1,100px. This does not add a new SVG feature or page rule.

Qualification: all 73 dynamic-layout tests pass; all 3,851 recorded static and
630 JavaScript layout baselines pass (six static skips). Evidence: Lambda
`temp/online-visual-round4/{svg-before.log,svg-after.log,dynamic-final.log,
svg-physical.html,saas_landing_254-static-after.json,radiant-baseline.log}`.
Online capture qualification is recorded separately in the round-four audit.

<a id="ri-033"></a>

### RI-033 — Propagated body backgrounds cover negative stacking layers

**Status:** FIXED for the isolated ordering defect, 2026-11-12.
**Lambda blocker:** cleared for the reduced Vogelhaus sky. Ownership
**D9.1.1v4 / D9.1.3v2**. The renderer selected the body background as the canvas
background, then painted it again during the ordinary body boundary visit.
Root negative-z descendants were already painted, so the repeated body fill
covered the hero's gradient and made white text disappear. CSS Backgrounds 3
§2.11.2 gives a propagated body's background its transparent initial used value.

The shared background-source walk now records that selected source during the
root boundary visit and suppresses its later body visit. Both paint drivers
admit canvas recording when the root has no own background, and optimized
boundary paths use the same source selection. Existing layer accessors and
canvas selection moved to the lightweight boundary module to preserve native
recorder test linkage; they were not copied or replaced by test stubs.
Existing image positioning and the canvas viewport extension remain shared.

The native pixel regression fails before and passes after for solid and
gradient body sources and an explicit root-background control. It samples both
a narrow negative layer and exposed canvas. All 73 dynamic tests pass, local
PNG/SVG controls agree after headless-shell rasterization, and the unchanged
`canvas_body_background` integration fixture passes all six assertions,
including zero-height tiled body images and a later explicit root color.
A local extension also passes all 12 assertions while changing body colors
and restoring the image, checking retained canvas-source freshness.
Containment and fragmented canvas backgrounds remain outside these checks.
Evidence: Lambda `temp/online-visual-round4/{canvas-before.log,dynamic-final.log,
export-controls.json,ui-canvas-final.log,canvas-body.html}`.
Spec: https://www.w3.org/TR/css-backgrounds-3/#special-backgrounds

<a id="ri-034"></a>

### RI-034 — Positioned intrinsic sizing leaks its query mode to following blocks

**Status:** FIXED, 2026-11-12. **Lambda blocker:** cleared for the reduced
Vogelhaus width. Ownership **D9.1.1v4 / D9.1.3v2**. `layout_abs_block_internal`
selects min/max-content available space for an intrinsically sized positioned
box. Its public wrapper returned without restoring that state. A fixed
`width:max-content` header therefore made a following ordinary block choose
max-content width: 3,000px instead of its 1,200px containing width. Vogelhaus's
archived main grew to 4,482px and centered the hero outside the viewport.

The positioned wrapper now saves and restores the parent's available space.
The regression fails before and passes after with fixed max-content,
absolute min-content, a definite-size control and relayout. Overflowing child
content keeps its 3,000px width, while both normal-flow siblings stay 1,200px.
The archived full Vogelhaus main now stays 1,200px. All 73 native dynamic,
3,851 recorded static and 630 JavaScript layout baselines pass (six skips).
Evidence: Lambda `temp/online-visual-round4/{space-before.log,abs-space.html,
abs-space-after.json,reduce-width-2.log,saas_landing_476-static-after.json,
dynamic-final.log,radiant-baseline.log}`.


<a id="ri-036"></a>

### RI-036 — Nested column flex intrinsic heights omit main-axis gaps

**Status:** FIXED, 2026-11-12. **Lambda blocker:** no. Ownership
**D9.1.1v4 / D9.1.3v2**. `FlexIntrinsicAccumulator::add_width` counted only row
children; the subsequent intrinsic contribution added only column gaps to row
widths. Nested auto columns therefore omitted every row gap from their height,
then an enclosing column shrank fixed-height contents to fit that underestimated
budget. A reduced 736px hero, 6000px following section, 120px gap and 40px padding
produced a 6776px page instead of 6896px and compressed the hero to 722.9px.

Count contributions for either orientation and add the main-axis gap through
`layout_flex_style_info`, reusing the existing style resolver. The regression
`NestedColumnIntrinsicHeightIncludesMainAxisGaps` fails before repair and passes
with both 120px and 40px gaps across recascade/relayout. All 77 dynamic-layout
checks, 3851 static and 630 JavaScript layout cases pass (six static skips), as
does layout integer-cast lint. Evidence: Lambda
`temp/online-visual-round5/{gap-before.log,dynamic-final.log,craft-min.html}`.
Craft's full-page residual is separately open as RI-039.

<a id="ri-037"></a>

### RI-037 — Authored display bypasses scripting-enabled noscript suppression

**Status:** FIXED, 2026-11-12. **Lambda blocker:** no. Ownership
**D9.1.1v4 / D9.1.3v2**. The existing scripting-mode guard operated in inline
layout, so explicit `display:contents`, block, flex or grid roles could bypass
it. Raw noscript markup became text or a 400px box could enter a flex/grid.
`layout_resolved_display` now applies the existing
`layout_noscript_content_suppressed` predicate before selecting a used layout
role. CSSOM still reads `resolve_display_value`, preserving authored computed
CSS and DOM contents. No public style is rewritten.

`ScriptingEnabledNoscriptCannotCreateBoxesThroughCssDisplay` covers native
scripting-disabled fallback followed by enabled-mode recascade and relayout.
Lambda `test/ui/dom/noscript_used_display` verifies computed CSS, zero DOMRects,
retained DOM text and normal following flow through the real script host (five
assertions). Its old-host control fails. Qualification is the same complete
layout gates as RI-036. Evidence: Lambda
`temp/online-visual-round5/{noscript-before.log,noscript-before-ui-result.json,noscript-final-ui-result.json}`.

<a id="ri-038"></a>

### RI-038 — Intrinsic ratio heights ignore inline min/max constraints

**Status:** FIXED, 2026-11-12. **Lambda blocker:** no. Ownership
**D9.1.1v4 / D9.1.3v2**. Intrinsic height transferred its provisional inline
query width through `aspect-ratio` before applying inline min/max constraints.
A grid item with `width:100%;max-width:56px;aspect-ratio:1` reserved an 856px auto
row; its percentage-height child remained 205px despite its final 56px square.
Both an empty item and an image-containing flex item reproduce this defect.

Before ratio transfer, constrain the candidate using the existing border-box
conversion and `layout_apply_min_max_border_box_axis` helpers. This implements
CSS Sizing 4 §4.2/§4.4's constrained ratio-determining size without duplicating
constraint logic. `GridRatioHeightUsesConstrainedInlineSize` fails before and
passes after; it checks empty/image items, 56px→112px mutation and preserved
explicit 90px height. All 77 native dynamic checks and both recorded layout
gates pass, with integer-cast lint. Evidence: Lambda
`temp/online-visual-round5/{ratio-before.log,ratio-after.log,grid-ratio.html}`.
The reduced Craft grid/image now measure 56px; RI-039 retains the distinct outer
flex-height residual. Do not claim this repair alone resolves Craft's hero.


<a id="ri-039"></a>

### RI-039 — Intrinsic height recursion borrows an outer percentage-height basis

**Status:** FIXED, 2026-11-12. **Lambda blocker:** no. Ownership
**D9.1.1v4 / D9.1.3v2**. Recursive intrinsic height queries installed each
child's inline basis but retained an outer block's definite height through
auto-height ancestors. In a script-free nested flex/grid reduction, a 100%
image borrowed viewport/outer query heights (800px, then 1152px and 1392px in
LLDB) instead of its own containing box's auto/ratio size. A 136px following
card acquired a 1392px wrapper and squeezed an earlier 736px hero to 540px.

Install the current element's own explicit or ratio-derived content height,
or an indefinite basis for auto height, through the existing
`LayoutContainingBlockScope`. `IntrinsicDescendantPercentHeightUsesItsOwnContainingBlock`
fails before and passes after, including 56px→112px square mutations and
relayout. All 77 native dynamic tests, 3851 static and 630 JavaScript layout
cases pass (six static skips); integer-cast lint passes. The full frozen
Craft hero now measures 736px. The separate nested-height failure in RI-040
still obscures live hero content and reproduces on the previous renderer
when the hero is fixed at 736px. This is an isolated sizing repair, not a
claim that the entire page matches.

Evidence: Lambda `temp/online-visual-round5/{percent-before.log,dynamic-qualified-final.log,craft-fivefix.json,lldb-height.log,craft-thin.html}`. Captures use
`lambda-fivefix.exe` SHA-256
`6993045526db37b5a4b9b1b4ce504ede4e9c6eaafa582105ac4d52572425c0a4`.


<a id="ri-040"></a>

### RI-040 — Maximum constraints erase definite flex main sizes

**Status:** FIXED, 2026-11-12. **Lambda blocker:** no. Ownership
**D9.1.1v4 / D9.1.3v2**; size/percentage behavior follows
[CSS Flexbox §9.8](https://www.w3.org/TR/css-flexbox-1/#definite-sizes).
In init_flex_container, the maximum-size branch replaced has_definite_main
with a comparison against the maximum. An explicitly 736px-high container
with max-height:1100px therefore became indefinite, even though no clamping
occurred. Percentage-height descendants then used their content minima:
Craft's nested wrapper became600px and its padded inner flex box120px
instead of736px. The product image overpainted the heading. The code handles
either main axis.

Preserve an already definite main size while retaining the previous rule
for an automatic size that reaches its maximum. The standalone reduction
temp/online-visual-round6/percent-max.html reproduces without scripts or
site CSS. MaximumConstraintPreservesDefiniteFlexMainAxis fails before and
passes after repair: inactive1100px maximum, clamped600px maximum and removal
of the maximum, with nested percentages, positioned bottom child and a
neighboring percentage-basis row across recascade/relayout.

All78 native dynamic tests,3851 static and630 JavaScript layout cases pass
(six static skips), and layout integer-cast lint passes. Full frozen Craft
keeps both outer and inner content736px; the live heading/button/product
image are restored. URL-masked decorative geometry remains RI-041, so the
page is not declared a complete visual match. Prior fixed-height controls
remain valid evidence that this defect predates RI-039.

Evidence: Lambda temp/online-visual-round6/{max-before.log,dynamic-max.log,
percent-matrix.log,percent-ancestors.log,craft-max.json,craft-live.png}.
Qualified host: lambda-max.exe, SHA-256
34795256482fbda34c9e677bc079475ae48963d43fb5733c9748e8694fc24891.


<a id="ri-042"></a>

### RI-042 — CSS math gaps are dropped before layout

**Status:** FIXED for length-valued math, 2026-11-12. **Lambda blocker:** no.
Ownership **D9.1.1v4 / D9.1.3v2**, document style contract **D5.5.1**;
[CSS Box Alignment §8](https://www.w3.org/TR/css-align-3/#gaps) defines
nonnegative gap lengths. Craft uses gap:calc(var(--spacing) * 10) with
--spacing:.25rem. The variable and computed serialization are correct, but
the gap shorthand excludes FUNCTION values and resolve_gap_property handles
only literal lengths/numbers/percentages. Every corresponding layout gap
therefore becomes zero. The root cause is unrelated to the unsupported URL
masks (RI-041) and is not a missing Tailwind theme variable.

Admit math-valued shorthand components and resolve their substituted definite
lengths through the existing length/math resolver. Clamp negative computed
lengths to zero. The shared consumer feeds flex, grid and multicolumn geometry;
no page-specific stylesheet or spacing is substituted. Pure percentages retain
their existing deferred flags. Percentage-containing math expressions still
lack a retained gap expression and used-container basis; this limited repair
does not claim that separate incomplete consumer is supported.

MathGapsUseOwnerUnitsAcrossLayoutModes fails before the repair in all three
layout modes and passes afterward. It checks calc/min-max-family math,
custom-property substitution, rem/em owner units, two-value shorthand,
nonnegative clamping, pure-percentage controls and 16px to20px root-font
mutation with recascade/relayout. Chromium headless-shell independently
returns the same16 expected relative coordinates. All79 native dynamic
layout tests pass. Final integration qualification and online capture counts
are recorded in Lambda test/online/README.md.

Evidence in Lambda temp/online-visual-round6/: gap-before.log,
dynamic-gap.log, gap-browser.json, craft-spacing-probe-output.log.
Final host lambda-final.exe SHA-256
60f757cadefdbc438344b5826696c6b1536e5c81f4cd777ff2d531a9529b1c60.


<a id="ri-043"></a>

### RI-043 — Unsubstituted variables incorrectly trigger cyclic intrinsic sizing

**Status:** FIXED, 2026-11-12. **Lambda blocker:** no. Ownership
**D9.1.1v4 / D9.1.3v2**, document contract **D5.5.1**;
[CSS Sizing §5.2](https://www.w3.org/TR/css-sizing-3/#intrinsic-contribution)
requires the appropriate min/max constraints in intrinsic contributions.
measure_element_intrinsic_widths classifies authored width and max-width with
layout_css_value_has_percentage before variable substitution. That predicate
conservatively treats var() as potentially percentage-valued. A definite
max-width:var(--cap), with --cap:800px, therefore enters the cyclic-percentage
policy and loses its cap when a flex parent measures an auto-width child.
This enlarges Maven Central's text column and shrinks its promo image.

Use the existing layout_css_size_facts query to classify the substituted
owner value. No new resolver or page-specific geometry is introduced. A
literal 800px override on the frozen page already restores the correct
contribution, proving that font noise and script readiness are not this cause.
The first reduced promo has browser text/image widths600.64/327.36 versus
old native678.8/249.2; this repair gives600.6/327.4. That reduction uses a
436x347 natural image to separate the independent image-hint defect RI-044.

IntrinsicVariableMaximumConstrainsNestedFlexContributions checks literal,
local-variable, owner-em and variable-math maxima, recascade after cap300->200,
and a zero-percentage math neighbor against an unconstrained control. Chromium
independently confirms that calc(300px + 0%) retains a cyclic dependency and
must not be forced to behave like a pure length. All81 native dynamic tests
pass with RI-044. Final recorded-layout and online qualification is in Lambda
test/online/README.md. No percentage basis or tolerance is substituted.

Evidence in Lambda temp/online-visual-round7/: maven-matrix.json,
constraint-before.log, constraint-browser-before.json, dynamic-twofix.log.

<a id="ri-044"></a>

### RI-044 — Intrinsic images restore HTML width hints overridden by CSS

**Status:** FIXED, 2026-11-12. **Lambda blocker:** no. Ownership
**D9.1.1v4 / D9.1.3v2**, document contract **D5.5.1**;
[CSS2 §6.4.4](https://www.w3.org/TR/CSS2/cascade.html#preshint) puts presentation
hints below author CSS. The intrinsic IMG branch correctly avoids a provisional
resolved percentage width, then falls back to the raw HTML width attribute.
That fallback revives a hint which authored width:auto or a cyclic percentage
already overrode. Maven's image has natural972x613 dimensions but HTML436x347
hints; width:100%;height:auto must keep the natural intrinsic contribution.

Only use the raw HTML fallback when no winning physical size declaration
exists. Resolved definite CSS sizing remains authoritative; bare HTML hints
remain available. The shared physical declaration selector includes logical
inline-size, so it follows the same cascade without a copied special case.
IntrinsicImageCssOverrideDoesNotRestoreHtmlWidthHint fails before repair and
passes after: percentage, automatic, variable percentage and logical widths,
plus untouched bare-hint/fixed-CSS controls. Row width400->500 and HTML hint
100->80 mutations verify that overridden hints stay overridden. Chromium's
independent six-case geometry control includes an automatic image retaining
its natural300px width while its flex wrapper shrinks to200px.

All81 native dynamic tests pass. With both repairs, the frozen real-dimension
Maven promo has text/image widths419/509px and height327px; Chromium returns
418.97/509.03px and327.02px. The missing client-created search input remains
unresolved; this is not a claim of full application execution support.
Evidence in Lambda temp/online-visual-round7/: image-before.log,
image-browser.json, dynamic-twofix.log, maven-natural-browser.json,
maven-facts.json, maven-final-summary.json. Final pinned host lambda-final.exe
SHA-256 76e79482164f6192fee0b148cfa929e56b4d8cd129730ef36518b369bca67c9c.


<a id="ri-045"></a>

### RI-045 — Headless document hosts omit the browsing profile

**Status:** FIXED, 2026-11-12. **Lambda blocker:** cleared for cookie startup;
remaining visual omissions require their own evidence. Ownership
**D9.1.1v4 / D9.1.3v2**, headless parity **D9.2.1v2**, snapshot policy **D10.1.7**.
Twenty-six remaining high-severity captures report document.cookie setter
SecurityError. Export initializes a native UI context but no BrowsingSession;
view and js --document already have one. The loader therefore receives no
cookie jar, loses initial response cookies, reports cookieEnabled=false and
throws on script cookie writes. Layout and legacy PNG batches share this gap.

UiContext now owns an ephemeral session only when its caller supplied none.
Exports ensure it before loading; the common hosted loader also ensures it
from explicit/TLS UI configuration before HTTP and passes the same jar to
subresources. Existing window profiles remain borrowed. UI retirement releases
its owned session after the document and loader tasks retire. A shared batch UI
retains the profile across jobs; independent exports do not persist it.
No synthetic cookie success value or setter exception suppression is used.

The real local HTTP renderer regressions in Lambda
 test/online/render_session.test.cjs cover response/DOM cookies, HttpOnly
visibility, initial and script-created image requests, redirects, independent
exports, layout and both batch paths. The initial renderer fails all three
export cases; the intermediate export-only repair leaves layout/legacy batches
failing; all six pass with the common-loader repair and input LI-018/LI-019.
OwnedEphemeralBrowsingSessionEndsWithUi and
BorrowedBrowsingSessionSurvivesUiCleanup verify profile lifetime and isolation.
All83 focused native cases and574 AST baseline tests pass. A restricted-sandbox
media failure reproduces on old/new renderers and passes on both with native
macOS services; no media implementation or expectation was changed.

Input transport fixes LI-018/LI-019 are necessary companions: enabling a jar
exposed corrupt terminal cookie attributes and port-bearing cookie domains.
Final aggregate/capture counts and limitations are recorded in Lambda
 test/online/README.md. These exception repairs do not establish that every
cookie-using page now visually matches. Evidence in Lambda
 temp/online-visual-round8/: cookie-before.log, session-before.log,
 session-frontends-before.log, session-qualified.log, native-qualified.log,
 js-baseline-ast-qualified.log, media-controls.json, media-native-services.json.
Final host lambda-final.exe SHA-256
ff06c819f1cfa2a8e3bc1d1a7e20ae6e2bc9e6f153e9f10b8b1b2f5cbc8ea209.


<a id="ri-046"></a>

### RI-046 — Pending style text reparses discard later CSSOM edits

**Status:** FIXED, 2026-11-12. **Lambda blocker:** cleared for dynamic sheet
retention; unrelated omissions remain explicit. Native/adapter ownership
**D9.1.1v4 / D9.1.3v2**; finite frame capture remains **D10.1.7**.

A script connects an empty style node and calls sheet.insertRule. Its CSSOM
rule list reports the insertion, but the post-script native rescan reparses the
queued DOM text update and replaces the sheet with its empty text source.
Computed color can report the CSSOM edit before the final image loses it;
geometry reads can trigger the stale rescan sooner. Libraries that insert
rules without serializing them into text children lose their entire stylesheet.
The element.sheet getter also returned the old sheet after textContent changed.

radiant_sync_mutated_inline_stylesheets now consumes each successfully synced
owner once, retaining the batch count for reconciliation. The Lambda CSSOM
adapter flushes pending text changes before returning an inline sheet, so later
CSSOM edits survive the final rescan. An actual later text replacement still
creates its new sheet. Explicit native radiant_sync_inline_stylesheet calls
retain their force-parse contract. CSSOM rules are not copied back into text,
and no site-specific selectors, opacity overrides or timer advances are used.

Lambda test/online/render_stylesheet.test.cjs covers new empty-sheet insertion,
existing rule declaration edits, text replacement followed by a sheet read and
CSSOM insertion, and later text superseding CSSOM. Two of four cases fail with
the prior qualified renderer; all four pass with the repair, including exported
solid-color pixels. Chromium independently confirms the reduced insertion and
replacement behavior. The new native input-value regression checks repeated
synchronization retains CSSOM state and a new text update still replaces it.
The real window/event fixture dom_stylesheet_cssom_retention fails3 of4
assertions with the previous renderer and passes4 of4 with the repair. All3
recent DOM integration fixtures pass15 assertions.
All12 native input-value cases,574 AST baseline cases,3851 recorded static layout
cases (6 skips),630 JavaScript layout cases,6 HTTP/session cases and5 comparator
tests pass. The native aggregate has928 passes,1 skip and the existing RI-035
unavailable-voice failure. Initial AST worktree failures use stale/missing
fixture/module links; the qualified current fixture/module environment passes
all574 without crypto/media source or expectation changes.

Diagnostic captures restore Northstar home/about layout and Greenlist styling;
Greenlist's hero remains absent and must remain high even below the metric
threshold. Final per-page results and qualification limits are in Lambda
 test/online/README.md and temp/online-visual-round9/. Reduced source, PNGs,
stylesheet-before.log,render-qualified.log,native-input-value.log,
cssom-browser.json,js-baseline-ast-qualified.log and layout/native logs retain
the controls. Final host SHA-256:
044ca5945f52565491a82c67ad85437473eb8489ad981f4bc51b69f75875f71b.


<a id="ri-047"></a>

### RI-047 — Raw basic-shape parsing drops computed clip geometry

**Status:** FIXED, 2026-11-12. **Lambda blocker:** cleared for the confirmed
menu overlay; unrelated visual differences remain. Native ownership follows
**D9.1.1v4 / D9.1.3v2**, transient value ownership **D5.5.1**, selected-frame
snapshot policy **D10.1.7**.

Hive Streaming uses a fixed menu at1200px and closes it with
clip-path:inset(0 0 calc((1 - var(--menu-open,0)) * 100%) 0).
render_clip.cpp read raw declaration text with strtof, so math did not advance
and the shape became an unclipped rectangle. A whole-property variable also
failed. The open menu covered the main hero although HTTP loading succeeded.
Separately, one/two-value inset percentages were resolved once on the vertical
axis and copied to horizontal edges; circle percentages used the shorter side
instead of the normalized diagonal. Relative lengths were read as raw numbers.

Basic shapes now consume parsed, substituted CssValue trees. Their shared
length sink uses the existing native CSS numeric/font/unit/math resolver with
an explicit reference basis; its original three-argument API retains its prior
behavior. Inset shorthand expands syntax before each physical-axis resolution.
Circle/ellipse center positions reuse the existing CSS position grammar;
closest/farthest-side radii and a default circle use their box distances.
Only resolved coordinates enter the clip scratch scope, including polygon
vertices. Empty/overconstrained insets stay empty. Raster and vector consumers
share this geometry, including CSS zoom. No math parser is copied, no vendor
code edited, and no site selector, clipping override or timer advancement added.

The frozen original Hive HTML/CSS and a control differing only by the equivalent
literal inset(0 0 100% 0) reproduce the menu/hero difference with the previous
renderer. The repaired original matches the old literal control exactly in
pixels, as well as its repaired literal control. A live diagnostic capture
restores the hero and changes coarse mean error from59.7% to1.6%; full-round
measurements retain consent/content/readiness caveats.

Eighteen real PNG cases cover closed/partial clips, variable fallback and whole
values, shorthand axes, nonlinear math, font units, circle/ellipse/polygon,
script mutation, zoom, empty shapes and invalid dimensions. The previous pin
fails15 of18; the repaired pin passes18. Independent Chromium agrees with all18
expected pixel sets, and18 exported SVGs rasterized by Chromium pass the same
checks. The window/event regression fails4 of6 assertions before and passes6
of6 after variable changes and a used-box resize. All4 recent DOM fixtures pass
21 assertions. All83 focused native cases,574 full AST cases,3851 recorded
static layout cases(6 skips),630 JS layout cases,28 real CLI regressions,8
existing clip-render corpus cases,5 comparator cases and geometry lint pass.
The native aggregate has928 passes,1 skip and the same RI-035 voice failure.
Input implementation is unchanged in this round.

Full inset corner/slash rounding, polygon fill rules, complete shape() grammar,
URL clips and geometry boxes remain incomplete. Existing bounded path()/shape()
command parsing is retained. The HTML/CSS support matrix records these limits.
No high-severity page is cleared solely from a metric threshold.

Evidence in Lambda temp/online-visual-round10/: clip-probe.html,
clip-before.png,clip-browser.json,clip-tests-before.log,clip-tests-after.log,
clip-browser-cases.json,vector-controls.json,hive-frozen.html,hive-literal.html,
hive-*-before.png,hive-*-after.png,ui-before.log,ui-qualified.log,
render-qualified.log,render-clips.log and layout/native qualification logs.
Pinned final host SHA-256:
bbea00e3ff61c45ba59b981ee38781c0bdb46c700d4fcb1d351cdda770965fb5.


<a id="ri-048"></a>

### RI-048 — Grid-item percentage sizes use the whole grid instead of their area

**Status:** FIXED, 2026-11-12. **Lambda blocker:** no. Native ownership
**D9.1.1v4 / D9.1.3v2**, temporary query scope **D5.5.1**.
[CSS Sizing §5.2.1](https://www.w3.org/TR/css-sizing-3/#cyclic-percentage-contribution)
and [CSS Grid track sizing](https://www.w3.org/TR/css-grid-1/#algo-content)
require cyclic percentage contributions to be separated from used item sizing.

Nova Benefits' hero is a 1160px, two-column `1fr 1fr` grid with a 64px gap.
Its first item is a flex column with `width:100%`. Intrinsic measurement used
1160px as that percentage's containing block, producing tracks of 1160px/380px.
The loaded, visible 560px hero image consequently began at x1244, outside the
viewport. Chromium instead gives 548px/548px tracks and image x632. Separately,
percentage-containing math bypassed the explicit-size exclusion and retained
its provisional whole-grid preferred size after track placement.

`calculate_grid_item_intrinsic_sizes` now uses the existing containing-block
scope for the unsolved contribution axis and the shared substituted CSS size
facts for percentage classification. Solved row-axis column geometry remains
available. Placement reevaluates percentage math with the existing owner-aware
length resolver against the solved area; border-box conversion remains shared.
No page selector, guessed size, min-width override or alternate CSS is added.

The same frozen original HTML/CSS restores 548px/548px tracks and the hero.
A diagnostic control changes only the first item's width:100% to auto; it works
on the old renderer, and the repaired original matches that control exactly
in pixels. The live page also restores the illustration. A frozen Zapier customer-story
page reproduces its displaced narrow hero on the previous engine and restores
the two-column heading/photo after the same native repair, with identical
HTML/CSS and no scripts. These controls isolate native layout; live score
movement can additionally include consent/content.

`GridPercentageContributionsUseTheUnsolvedArea` covers nine width/constraint
variants at 1160px and 1080px, with independent Chromium geometry. The previous
engine fails 16 assertions; all 18 case/phase geometry sets pass after repair.
Ten real PNG fixtures add owner-em and substituted mixed math: old 5 pass/5 fail,
new 10 pass, and independent Chromium confirms all 10 pixel/geometry sets.
`grid_percentage_area` tests live resize, font and variable changes: old 2/8,
new 8/8 assertions. Current milestone gates pass 84 native dynamic cases,
42 real CLI cases, eight DOM UI fixtures with 58 assertions, 574 AST cases,
3,851 recorded static layout cases (six skips), 630 JS layout cases and geometry
lint. The native aggregate retains 929 passes, one device skip and the known
RI-035 unavailable-voice failure. Input implementation is unchanged.

Evidence in Lambda `temp/online-visual-round12/`: `grid-before.log`,
`grid-after.log` (intermediate scope-only repair; final is native-dynamic.log),
`grid-browser.json`, `grid-png-before/after.log`, `grid-browser-png/results.json`,
`ui-before/final.log`, `nova-frozen-comparison.json`, `nova-browser-boxes.json`,
`nova-original-after-lambda-output.log`, and milestone logs. Final online
qualification and remaining severity counts are in Lambda `test/online/README.md`.

<a id="ri-049"></a>

### RI-049 — Root font-size evaluation leaves descendants on the initial rem basis

**Status:** FIXED, 2026-11-12. **Lambda blocker:** no. Native style/layout
ownership **D9.1.1v4 / D9.1.3v2**, query scratch **D5.5.1**; capture virtual time
remains **D10.1.7**. [CSS Values 4 font-relative lengths](https://www.w3.org/TR/css-values-4/#font-relative-lengths)
uses the initial font for the root's own font-relative font-size and the
computed root font for descendant rem lengths.

Zammad's company page sets the root to
`clamp(.875rem,.25rem + .8333vw,1rem)` at 1200px. While computing that value,
the general rem resolver primed root_font_size with the initial 16px. The
actual root font subsequently became 14px, but the cache was only published
when negative. Descendant layout retained 16px: the 4rem heading used 64px and
wrapped to two lines, while CSSOM correctly reported 56px. Root restyles could
also reuse the previous root basis for their own rem font-size.

The existing owner-aware length resolver now evaluates the root's own rem
font-size against the initial font without priming the descendant cache. The
font prepass publishes the computed root size before other root geometry;
root layout also publishes it on cached-style paths. The stored basis stays
unzoomed, so zoom is applied once by existing consumers. No clamp expression,
page selector or input stylesheet is rewritten by the repair.

The same frozen HTML/CSS, with scripts removed, now matches Chromium's 14px
root, one-line 56px heading at y252 and 52.5px paragraph at y336. The repaired
original and both old/new renderers of a diagnostic one-declaration 14px
control match exactly in pixels. The control is not a catalog or source edit.

`RootRelativeFontSizesPublishTheComputedRemBasis` covers nine font/variable/math/
shorthand/viewport/zoom variants with repeated forced recascades. The native
dynamic suite passes 85 cases. Eight real PNG cases change from one pass/seven
failures to eight passes; independent Chromium validates their geometry and
pixels, plus the native fixture's nine geometries. The live DOM UI fixture
changes from two of six pixels to six of six after root-variable mutations.
All 50 real CLI cases, nine UI fixtures/64 assertions, 574 AST cases, 3851 static
layout cases (six skips), 630 JS layout cases, five comparator tests and integer
cast lint pass. The native aggregate has 930 passes, one device skip and the
independent known RI-035 voice failure. Input implementation is unchanged.

Evidence: Lambda `temp/online-visual-round13/` contains the frozen original,
literal control, before/after PNGs, rem-controls.log, rem-png-before/after.log,
root-rem-browser.json, native-dynamic-final.log and qualification logs. The
initial native probe included an incorrect global-coordinate assertion against
parent-relative positions; its failure count is not used as repair evidence.
The corrected source checks root boundary padding directly. The compatible
integration host uses exact repaired root sources and current sibling archives;
the independent main-checkout paint API mismatch remains outside this repair.

<a id="ri-058"></a>

### RI-058 — Absolute grid placement sizes contents against the whole container

**Status:** FIXED, 2026-11-13. **Lambda blocker:** cleared for the selected
absolute-grid sizing cases. Native ownership **D9.1.1v4 / D9.1.3v2**, pass
scratch **D5.5.1**. [CSS Grid §9.1](https://www.w3.org/TR/css-grid-1/#abspos)
requires the resolved grid area to establish the positioned containing block;
out-of-flow placement does not create tracks.

Stripe's blog hero assigns its absolute figure `grid-area:image`. Native grid
placement only inspected numeric fields, whereas the current CSS projection
stores named references. Numeric diagnostic placement moved the outer box to
the right column, but its descendants had already been laid out against the
whole card. The later grid-specific constraint equation also overwrote authored
sizes and min/max constraints. A static grid or ordinary grid with a fixed child
could wrongly replace the actual containing block. Empty grids did not register
their named line/area aliases before resolving their positioned children.

The existing authored-edge resolution is extracted into `grid_resolve_item_edges`
and reused by in-flow and positioned consumers, retaining their distinct auto
edge rules. Named aliases are registered before the empty-grid branch. The
resolved area is borrowed through `AbsChildLayoutState` and `LayoutContextScope`
while the ordinary positioned solver computes style, size and descendants.
Late percentage passes reuse committed track geometry. The duplicate post-layout
grid constraint equation is removed; the actual containing-block check preserves
static-grid and viewport-fixed behavior. Inset-solved automatic heights now
provide a definite percentage basis through the existing percentage helper.

Nine independent Chromium geometry/pixel cases cover named/numeric placement,
percentage insets, explicit and maximum sizes, child box edges, mixed math,
static grids and viewport-fixed controls. Real PNG results change from 0/9 to
9/9. The live track/area/size fixture changes from 3/9 to 9/9 pixels. The native
case loads the same nine fresh inputs and repeats forced recascades; it passes.
No existing assertion, reference, admission cap or capture time is weakened.

The exact saved Stripe original now restores its 524px-wide image beside the
heading, author, copy and action; RI-059 separately repairs the card height.
Combined qualification: 89 native dynamic cases, 64 real CLI cases, eleven
DOM UI fixtures/81 assertions, 574 AST cases, 3851 static layout cases (six
skips), 630 JS layout cases, five comparator tests and integer-cast lint pass.
Native aggregate:1083 passes, one device skip and the existing RI-035 voice
failure. The current main host builds successfully; the older paint API build
mismatch no longer applies to this revision.

Evidence: Lambda `temp/online-visual-round14/` contains immutable before,
absolute-only and combined hosts; grid-absolute-before/after.log,
grid-absolute-browser.json, native-after-inset-basis.log, ui-before/final.log,
the identical stripe-frozen.html and before/after PNGs. An early native test
mutation sequence and a pre-link stale-binary invocation are explicitly marked
diagnostic/invalid in the checkpoint and are not final before/after counts.

<a id="ri-059"></a>

### RI-059 — Nested grid publication replaces formatted height with a flow cursor

**Status:** FIXED, 2026-11-13. **Lambda blocker:** cleared for the selected
Stripe card and nested-grid sizing cases. Native ownership
**D9.1.1v4 / D9.1.3v2**, formatter scratch **D5.5.1**.

After RI-058 restores the image column, Stripe's white card still has only its
64px vertical padding while its text extends beneath it. In
`layout_grid_item_final_content_multipass`, the nested grid formatter computes
its own geometry, then the common footer overwrites its content height using
the parent's unadvanced flow cursor. Non-stretch alignment subsequently uses
that padding-only height. Independently, every nested grid is marked as having
an assigned block size, even when start/center alignment requires natural size;
this also stretches the card's automatic rows incorrectly.

The nested grid now keeps its formatter-owned dimensions and baselines. The
existing detailed self-alignment resolver is promoted through the module header,
not copied, to distinguish an authored or stretched block extent from natural
layout. The native card height becomes460px and its rows24/96/48/140/24px; the
image becomes524×428px. These sizes match Chromium for the same saved HTML/CSS;
the remaining9px global vertical offset belongs to other page geometry. Live
capture also restores the full card. No site CSS or input document is changed.

Five independent Chromium geometry/pixel cases cover automatic parent size,
start/center alignment, stretch and authored height. Real PNG cases change
from2/5 to5/5; the focused native case changes from10 assertion failures to a
pass across repeated recascades. The live alignment/size fixture changes from
6/8 to8/8 pixels. Combined native/CLI/UI/layout qualification and known failure
are recorded in RI-058. Evidence: Lambda `temp/online-visual-round14/`
grid-nested-before.log, grid-nested-browser.json, native-nested-before/after.log,
ui-nested-before.log, ui-final.log and stripe-grid-after PNG/geometry logs.
Broader orthogonal, intrinsic and rare alignment combinations remain governed
by the existing partial-support matrix; this is a bounded common-page repair.


<a id="ri-062"></a>

### RI-062 — Cyclic percentage flex children inflate the content minimum

**Status:** FIXED, 2026-11-13. **Lambda blocker:** clipped text in online
Azure cards is repaired; the page retains independent header and capture
omissions. Native ownership **D9.1.1v4 / D9.1.3v2**; bounded substituted facts
follow **D5.5.1**; selected capture timing **D10.1.7** is unchanged.

A 270px outer flex box with 16px padding and a nested row containing a fixed
64px icon, 16px gap and `width:100%` text area expanded the row to452.7px.
Chromium uses236px and wraps the text in156px. The intrinsic-child loop in
`layout_flex_measurement.cpp` overwrote the measured min-content contribution
with max-content times the authored percentage. This made a cyclic100%
preferred width act as a nowrap minimum. CSS Sizing3§5.2.1 and Flexbox§4.5
require the cyclic preferred percentage to behave as auto during this query.

The loop now retains the existing content-only measurement. The shared
`layout_axis_size_is_percentage` in `intrinsic_sizing.cpp` consumes existing
`layout_css_size_facts`, so substituted variables, math and logical dimensions
retain their percentage dependency rather than using provisional pixels.
Definite substituted lengths and authored nowrap minima remain effective.
No page-specific rule or alternate intrinsic measurement is added.

Thirteen real PNG controls in Lambda's
`test/online/render_flex_percentage.test.cjs` share inputs from
`flex_percentage_fixtures.cjs`: before8/13, repaired13/13. They cover percentages,
math, variables, logical size, auto/zero-minimum controls, fixed and nowrap
minima, and mutation after a layout read. Independent Chromium agrees with all
13 pixel witnesses. Initial witnesses intersected text ink in both engines;
final witnesses sample the background below the glyphs.

The identical saved Azure HTML/CSS with scripts removed isolates the repair:
its four media rows change from308.9/370.3/451.9/541.5px to236px each; titles
wrap instead of clipping at card edges. A small auto cross-height discrepancy
remains in the reduction and is not claimed repaired. The main navigation is
separate; the page is not declared visually cleared from this control alone.

Qualification at Lambda0c5c0ba0d / Radiant eb39862 / Input4d1ad20 with these
two Radiant source edits:89 native dynamic cases,91 online CLI/capture/comparator
cases,630 recorded JS layouts and16 static suites with4979 passes and295
recorded partials,0 regressions and0 skips. Layout integer-cast lint passes.
A mistakenly named focused `wpt-css-flexbox` category does not exist and is not
counted; the subsequent17 existing-category run is the accepted baseline.
Evidence is Lambda `temp/online-visual-round17/`, including before/final hosts,
`flex-before-final.log`, `flex-percentage.log`, `flex-pixel-browser.json`,
`azure-*-layout.json`, saved PNGs and `layout-results.json`. Qualified host SHA256
`10fae5cc038c9f09a06c06b189b5f604bb648b6e30a628e9f2c52edc96c3da25`.
