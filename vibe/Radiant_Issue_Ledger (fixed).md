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

<a id="ri-028"></a>

### RI-028 — Historical P7 comparison artifacts are unavailable in the current checkouts

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
