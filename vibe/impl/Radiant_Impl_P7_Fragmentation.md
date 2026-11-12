# P7 qualification and advanced page fragmentation

**Status:** auxiliary-column reservations, backward page floats and nested
flex/grid content fitting are implemented and qualified on macOS, 2026-11-12.
Broader P7 and advanced fragmentation remain partial; a measured composition
slowdown is open as RI-032.
**Authority:** D9.1.1v4 / D9.1.3v2 (native ownership), D5.5.1 / D5.1.4
(published editions and retained resource lifetimes). Page layout follows
`../../Lambda-opus/vibe/radiant/Radiant_Layout_Paged_Media.md` §1.3 and §3.2;
`../../Lambda-opus/vibe/impl/Radiant_Impl_Paged_Integration.md` §7 retains the
historical P7 measurements. No formal language/design ruling changed.

## Current checkpoint

- Native workspace: `/Users/henryluo/Projects/radiant`, starting from `ead8d2a`
  with a clean worktree. The user authorized the three continuation cores below.
  Native sources, tests, fixtures and documentation stay in this owner checkout.
  No Lambda or input source changes are required; preserve unrelated sheet work.
- Implementation: reservation-aware final-column balancing, bounded backward-float
  reservation replay, oversized/nested row-content fitting, single-column flex
  in either direction, and trimming gaps at exact fragment boundaries.
- Final focused gate: 82 cases pass, including batched discovery of 20 backward
  figures within the default pass budget. Layout integer-cast lint checks 89
  casts with zero unmarked; whitespace checks pass. Original failures are retained.
- Final aggregate: 1,010 native passes and one disabled case; 4,481 layout passes
  and six skips. The explicit profile gate leaves parallel wrapped column-flex
  lines on their previous whole-context path.
- All 16 independent preview/PDF/Poppler controls pass exact pixels, literal
  color witnesses and every physical page's geometry. Preserve the first failed
  control run under `temp/paged-reservations-exports/`; its one-pixel text artifact
  was removed from the new authored input by making the real blue anchor line
  match its blue body box. No engine tolerance or existing reference changed.
- Final Lambda integration: 504/505 pass; selected PDF/FO/native export inventory
  is 84/90. Exact failure messages match earlier controls, and both original and
  private PDF baseline hashes remain unchanged. These are final-source frozen
  builds; the PDF subset does not replace the full 472-case inventory.
- Performance limitation: the held-object optimized comparison attributes about
  6% higher ordinary-flow composition time to the pagination object (RI-032).
  Correctness digests match; no performance no-regression claim is made.
- All new core gates must preserve source intervals, page/pass budgets, exact
  preview/PDF pixels, physical sheet geometry and previously published editions.
  No protected references or tolerances are changed. No staging/commit requested
  for this continuation.
- Historical archive waiver (user, 2026-11-12): assume the original P7 baseline
  archive was deleted/lost and ignore that comparison. RI-028 is closed by waiver,
  not by passing old visual, performance or source-reduction measurements.

## Delivered reservation and nested-formatting cores

1. Final equal body columns can balance with column top/bottom floats and notes.
   The search includes actual reserved extents and per-column indivisible floors;
   monolithic-box retry thresholds include their auxiliary capacity demand.
   Line-less insertion still binds to its next committed box, so concurrent
   reservation acceptance uses an actual line rather than assuming an empty
   box commits the figure's anchor. Text coverage and retained pixels are checked.
2. End-relative page figures can precede their committed body anchor in the
   existing one-column, one-sequence profile. Stable source/sequence identities
   retain reservation plans across bounded reference replay. Fragmentainers
   install those slots before earlier body fitting; later actual anchors bind
   once without double-counting or following moved body boxes. Destination
   sizing, top/bottom placement, source ordering, body text refit and failed
   page/pass admission have native acceptance cases.
3. Oversized flex/grid row items fit their finished ordinary block and inline
   content through the existing line/keep fitter. Nested resolved formatting
   rows use the same machinery. Each concurrent item fits independently; the
   row grows by the largest continuation and advances later rows/following flow.
   Real text remains DOM-backed after formatter scratch retirement. Tests cover
   legal block edges, widows, exact authored intervals, concurrent peers and
   byte-identical retained page pixels after failed admission.
4. Single-column flex items use their resolved physical block order, including
   `column-reverse`. Horizontal flex/grid gaps crossing a fragment boundary are
   discarded before testing the next row, including exact full-row boundaries;
   gap trimming participates in the existing strut undo journal.

These are finite native profiles. They do not assert complete CSS fragmentation
conformance. Supporting standards: CSS Multicol §7.1
https://www.w3.org/TR/css-multicol-1/#cf ; CSS Page Floats §5.1 (a working draft)
https://www.w3.org/TR/css-page-floats-3/#float-defer-property ; CSS Flexbox §10
https://www.w3.org/TR/css-flexbox-1/#pagination ; CSS Grid §12
https://www.w3.org/TR/css-grid-1/#fragmentation . Internal native ownership and
retained-resource decisions remain governed by the formal D# rulings above.

## Current qualification receipts

- `temp/paged-reservations-before.xml` preserves the four original auxiliary
  rejection cases; `temp/paged-backward-before.xml` preserves late-anchor
  rejection; `temp/paged-nested-before.xml` preserves five complex-row failures.
  `temp/paged-nested-neighbors.xml` caught the exact-boundary gap regression.
  `temp/paged-backward-batch-before.xml` reproduces per-figure pass exhaustion.
- `temp/paged-reservations-final.xml`: 82 focused passes.
  `temp/paged-reservations-lint.log`: required integer-cast check.
- `temp/paged-reservations-native-results/` and
  `temp/paged-reservations-owner-baseline.log`: first owner aggregate run.
  Final post-batching aggregate outputs are
  `temp/paged-reservations-batch-native-results/` (52 XML files, 1,011 registered,
  1,010 executed passes, one disabled) and
  `temp/paged-reservations-batch-owner-baseline.log`.
- `test/fixtures/paged/cases.json` adds six independent controls, total sixteen:
  auxiliary column balance, backward top/bottom figures, nested flex/grid and
  column flex. Controls are separately authored absolute rectangles on forced
  physical sheets; they contain no reservation, balance or row policy under test.
  Existing controls and protected external references remain unchanged.
- `temp/paged-reservations-negative/<case>/report.json`: all six new controls
  reject the previously qualified frozen host from `temp/paged-advanced-final/`.
  Earlier hosts reject auxiliary/backward policy explicitly, differ on nested
  preview/PDF pixels, and omit the third column-flex sheet witness. These are
  negative controls, not six completed successful old-engine exports.
- `temp/paged-reservations-final/report.json`: final frozen host, all sixteen
  complete exported controls, audited/supporting source hashes and working patch.
  Its optimized composition client records seven samples after two warmups per
  unchanged 21/300-page ordinary-flow workload, medians 0.968/0.829 ms/page.
  Published geometry/glyph digests match the previous qualified host. Debug
  clients refuse timing; parsing, export, digest and retirement stay outside the
  timer. `temp/paged-reservations-final-release-build.log` records the build.
  Interleaved archived-host comparison is under `temp/paged-reservations-paired/`;
  no historical P7 speedup or source-reduction target is claimed. The stronger
  `temp/paged-reservations-causal-control/` replaces only pagination source from
  `ead8d2a`, verifies every other archive object is identical, and alternates
  28 samples per host/workload. Median ms/page is 0.901476/0.955524 before/after
  for 21 pages and 0.781150/0.827102 for 300 pages, about 6% slower. Exact published
  digests agree. RI-032 retains this open performance qualification issue.
- The four historical source paths total 9,403 raw lines versus 9,250 at
  `ead8d2a` (+153). Supporting sources remain independently inventoried; retired
  engine selectors/types are absent. This is source accounting, not a reduction.
- Lambda-opus `temp/paged-reservations-integration-final/` retains pinned final
  executables, native source hash, private PDF baseline, XML/logs and combined
  `summary.json`. Its final results remain 504/505 paging and 84/90 selected PDF;
  RI-030/RI-031/LR12-50 remain open. The earlier initial-source run is preserved
  separately in `temp/paged-reservations-integration/`.

## Earlier completed increments

The original increments implemented positive page-float offsets and nested
auto-width named-page changes (`ad33423`), followed by equal-column balancing,
horizontal flex/grid row units, fixed-width named-page continuation and negative/
`last` float deferral (`ead8d2a`). Auto/fixed ancestors retain their existing
width behavior and definite sizing references. Positive out-of-range clamping
retains its earlier partial behavior; negative out-of-range offsets fall back to
the committed anchor. First-page/last-page sizing and budgets are not waived.

`temp/paged-advanced-final/` retains 60 focused and ten exact exported controls;
`temp/paged-advanced-native-results/` retains 988 native passes plus one disabled
case; the layout gate had 4,481 passes plus six skips. Its optimized paired
21/300-page ordinary-flow workloads had exact geometry/glyph digests and observed
changes within about 1%; these are earlier receipts, not current measurements.
The four historical source paths totaled 9,250 raw lines at that point, with
supporting headers/hooks inventoried separately. No size-reduction claim was made.

Lambda-opus `temp/paged-p7-integration/` retains the full 472-case PDF inventory:
466 pass, six fail. The raw run stopped after 468 cases at hidden WebGL context
creation; all four GPU cases passed separately with normal macOS access. Each
exact failure was independently reproduced with pagination source `340ad59` and
otherwise identical linked objects/flags. The original raw run was preserved.
`temp/paged-advanced-integration/` retains the subsequent 504/505 paged run and
84/90 selected export run. RI-030/RI-031 and Lambda-owned LR12-50 stay open.

## Remaining profiles and resume point

1. Unequal explicit column rectangles, page-wide reservations across multiple
   body columns and more sequence/terminal-master combinations. Current balance
   support uses equal native columns and their own auxiliary slots.
2. Flex/grid reflow that renegotiates inline tracks on differently sized pages,
   orthogonal writing modes, parallel wrapped column-flex flows, more complex
   spanning/alignment combinations and complete nested forced-break propagation.
   The new core fits finished content at its existing inline measure.
3. Column-attached/multi-sequence deferred-float scheduling and named-page width
   transitions through percentage-containing or independent formatting contexts.
   Backward reservation convergence remains bounded by the caller's pass budget.
4. Owner-side root-cause resolution of RI-030/RI-031, the measured RI-032
   composition slowdown, and Lambda-owned LR12-50, then wider P7 qualification. Keep their reference/assertion
   evidence intact until the owning cause is repaired.
5. Platform qualification beyond this macOS host. Fresh release receipts do not
   replace the lost historical archive or establish its old targets.

No implementation milestone is active. Resume with one finite remaining profile
and explicit acceptance cases, then focused/lint checks and owner gates at that
feature boundary. Preserve known red integration profiles. Keep performance,
historical and platform claims separate from focused native delivery.
