# P7 qualification and advanced page fragmentation

**Status:** the requested column-balance, flex/grid row-break and wider-profile
core milestones are implemented and qualified on macOS, 2026-11-12. Broader P7
qualification and the remaining advanced profiles are partial.
**Authority:** D9.1.1v4 / D9.1.3v2 (native ownership), D5.5.1 / D5.1.4
(published editions and retained resource lifetimes). Page layout follows
`../../Lambda-opus/vibe/radiant/Radiant_Layout_Paged_Media.md` §1.3 and §3.2;
`../../Lambda-opus/vibe/impl/Radiant_Impl_Paged_Integration.md` §7 retains the
historical P7 measurements. No language or formal design ruling changed.

## Current checkpoint

- Native workspace: `/Users/henryluo/Projects/radiant`; this continuation started
  from `ad33423`. Native sources, tests, controls and collector changes stay here.
  Lambda-owned integration acceptance is in `test/test_paged_media_gtest.cpp` in
  Lambda-opus. Preserve unrelated Lambda sheet work and existing ledger changes.
- All three requested core milestones below are delivered. Native page fitting,
  strut replay and formatter results remain shared; there is no second layout
  engine. Page/pass budgets, source coverage, physical PDF geometry and retained
  published editions remain acceptance constraints.
- Final gates: 60 focused passes; 988 owner-native passes and one disabled case;
  4,481 layout passes and six skips; ten independent exported controls pass.
  Integer-cast lint checks 89 casts, zero unmarked. Whitespace checks pass.
- Lambda integration: 504/505 pass, with the unchanged RI-030 table assertion.
  The affected PDF/FO/native subset plus six known-failure controls is 84/90;
  all six failure profiles match the earlier pristine control (RI-031/LR12-50).
  This is a subset rerun, not a new full 472-case PDF qualification.
- Original and isolated PDF baseline hashes remain unchanged. No protected
  references or tolerances were rewritten. Qualification artifacts stay under
  `temp/`. An external Lambda commit captured integration acceptance during this
  run; the native feature changes are committed separately in their owner repo.
- Historical archive waiver (user, 2026-11-12): assume the original P7 baseline
  archive was deleted/lost and ignore that comparison. RI-028 is closed by waiver,
  not by passing the old visual, performance or source-reduction measurements.

## Delivered behavior

1. Explicit `column-fill:balance` balances the final equal-column native body
   page. Earlier physical pages stay full. Bounded whole-flow replay finds a
   legal capacity through existing fits, indivisible-unit floors and text keeps;
   failed speculative trials cannot append an auxiliary tail or relax the page
   budget. The original body height remains the sizing reference. Tests cover
   auto versus balance, ordinary blocks, text/spans, preceding pages, indivisible
   units, page/pass limits, retained editions and combined row fragmentation.
   Equal native body columns are the finite core, rather than a claim of complete
   CSS multicol support. See CSS Multicol §7.1:
   https://www.w3.org/TR/css-multicol-1/#cf .
2. Horizontal flex lines and grid rows enter the common page-fit/strut machinery
   as row units. Visual order, connected grid row spans, explicit before/after
   breaks, fitting keep prefixes and oversized slicing are preserved. Anonymous
   formatter wrappers contribute real DOM text before their scratch storage
   retires. Tests check gaps, source coverage, following flow, explicit avoidance,
   retained page pixels and failed admission. Independent formatting inside each
   item still uses the existing formatter.
3. Named-page width transitions support open ordinary-flow chains with fixed
   absolute widths as well as the earlier auto-width profile. Fixed ancestors
   retain their measure; auto ancestors resume the page measure when the fixed
   child closes. Tests include portrait/wide/portrait, percentage descendants of
   a fixed ancestor, enclosing padding and later auto-ancestor continuation.
   Percentage-containing chains and independent formatting contexts remain
   diagnosed outside this profile.
4. Page-reference `float-defer` accepts negative offsets and `last` in the existing
   one-column, one-sequence profile. A bounded warmup observes the body end before
   auxiliary placement, consuming the reference budget without inventing tail
   pages. Negative offsets outside the flow fall back to the committed anchor.
   A destination before that anchor is diagnosed; a line-less terminal insertion
   keeps existing next-box binding. Positive out-of-range clamping retains its
   prior partial behavior. Tests cover computed values, tight page/pass budgets,
   source coverage and retained editions. See CSS Page Floats §5.1:
   https://www.w3.org/TR/css-page-floats-3/#float-defer-property .

## Qualification receipts

- `temp/paged-advanced-final.xml`: 60 focused cases, zero failures.
  `temp/paged-advanced-final-lint.log`: integer-cast lint.
- `temp/paged-advanced-native-results/`: 52 XML files, 989 registered cases,
  988 executed passes and one disabled case. `temp/paged-advanced-owner-baseline.log`
  records this owner baseline and 3,851 ordinary plus 630 JS layout passes, six
  skips and no failures. Intermediate column/row milestone logs remain retained.
- `temp/paged-advanced-final/report.json`: ten exact preview/PDF/Poppler controls.
  It freezes hosts, inputs, manifest, collector and the actual working patch;
  records decoded RGBA pixels, literal color witnesses and every PDF page's
  physical geometry; and verifies audited/supporting source hashes stay fixed.
  Controls use explicit authored column rectangles, absolute row boxes on forced
  sheets and existing top-level named-page paths, with no new policy under test.
- `temp/paged-advanced-negative-control-verified/report.json`: the frozen prior
  host passes the four existing controls, rejects negative/last deferral and real
  native column balancing, then stops at the flex-row witness outside its shorter
  output. This partial negative inventory must not be reported as ten cases.
  Separate `temp/paged-advanced-negative-extra/grid-rows/` and `fixed-width/`
  runs also reject: the grid output lacks the third sheet witness, and the old
  host explicitly rejects the fixed-width named-page transition. Together these
  receipts demonstrate rejection of each newly introduced feature control.
- Release composition uses an optimized, debug-refusing client with unchanged
  benchmark source. `temp/paged-advanced-final/report.json` has seven samples
  after two warmups on fixed 21/300-page ordinary-flow workloads: medians about
  0.868/0.771 ms/page. Parsing, export, digest calculation and retirement are
  outside the measured interval.
- `temp/paged-advanced-paired/report.json`: four alternating before/after pairs,
  28 samples per host/workload, frozen executables, matched input/client hashes
  and exactly equal published geometry/glyph digests. Median ms/page:
  21 pages, 0.88338 before / 0.88117 after; 300 pages, 0.783795 / 0.791503.
  The observed changes are within about 1% on these ordinary-flow workloads;
  they do not establish statistical equivalence or historical speedup targets.
- The four historically audited source paths total 9,250 raw lines versus 8,895
  at `ad33423` (+355). Supporting headers and flex/grid hooks are inventoried
  separately. Retired engine selectors/types remain absent. No source-reduction
  claim is made.
- Lambda-opus `temp/paged-advanced-integration/` holds frozen integration hosts,
  native source hashes, paged/PDF XML and logs, a private PDF baseline copy and
  the combined `summary.json`. Its paged run is 504/505; the selected PDF run is
  84/90. The known failure messages agree with the earlier final paged result
  and pristine PDF control. Both PDF baseline hashes remain unchanged.

## Earlier P7 checkpoint

The preceding native increments implemented positive page-float deferral and
nested auto-width named-page transitions. Their final scope was 19 focused cases,
four authored export controls, 947 native passes plus one disabled case, and
4,481 layout passes plus six skips. `temp/paged-p7-final/` and
`temp/paged-p7-native-results/` preserve those receipts. An external commit
`ad33423` captured that native implementation without source drift.

Lambda-opus `temp/paged-p7-integration/` retains the full 472-case PDF/export
inventory: 466 pass, six fail. The initial run stopped after 468 cases at hidden
WebGL context creation; all four GPU cases passed separately with normal macOS
access. Exact failures were reproduced with initial pagination source `340ad59`
and otherwise identical linked objects/flags. RI-030's table assertion has the
same independent control. The interrupted raw run was preserved. These known
red gates remain open; the current focused controls do not close them.

## Remaining profiles and resume point

1. Column balancing with auxiliary reservations, unequal explicit column
   rectangles and additional sequence/master transitions. Preserve the bounded
   replay and original sizing reference when widening the delivered core.
2. Flex/grid fragmentation beyond resolved horizontal row groups: vertical,
   nested and variable-inline-measure reflow, and detailed gap-edge conformance.
   Do not claim exhaustive flex/grid fragmentation from the current row cases.
3. Earlier-page reservation replay for floats before their committed anchor,
   column-attached/multi-sequence auxiliary scheduling, and named-page width
   transitions through percentage-containing or independent formatting contexts.
4. Owner-side root-cause resolution of RI-030/RI-031 and Lambda-owned LR12-50,
   followed by broader P7 integration qualification. Keep references and failure
   assertions intact until evidence supports the repair.
5. Platform qualification beyond this macOS host. The fresh release receipts do
   not replace the user-waived historical archive or prove its old targets.

No implementation milestone is currently active. Resume with one finite profile
and explicit acceptance cases, then focused native checks, integer-cast lint and
owner baseline at its feature boundary. Use optimized pinned executables and
matched workloads for performance; correctness and timing receipts stay distinct.
