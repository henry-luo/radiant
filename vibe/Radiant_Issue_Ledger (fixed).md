# Radiant Issue Ledger — Fixed Issues

Resolved native Radiant issues under D9.1.1v4 and D9.1.3v2. Entries retain
their original IDs, reproduction evidence, fixes and verification limits.
Active issues remain in the [Radiant Issue Ledger](Radiant_Issue_Ledger.md).

## Fixed issues

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
