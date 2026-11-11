# Radiant Issue Ledger — Fixed Issues

Resolved native Radiant issues under D9.1.1v4 and D9.1.3v2. Entries retain
their original IDs, reproduction evidence, fixes and verification limits.
Active issues remain in the [Radiant Issue Ledger](Radiant_Issue_Ledger.md).

## Fixed issues

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
