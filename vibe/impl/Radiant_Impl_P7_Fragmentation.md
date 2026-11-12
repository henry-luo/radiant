# P7 qualification and advanced page fragmentation

**Status:** active, 2026-11-12. Native task authorized after the Lambda paged
export increment; work is scoped to the owning Radiant checkout.
**Authority:** D9.1.1v4 / D9.1.3v2 (native ownership), D5.5.1 / D5.1.4
(published edition and retained resource lifetimes). Page layout follows
`../../Lambda-opus/vibe/radiant/Radiant_Layout_Paged_Media.md` §1.3 and §3.2;
`../../Lambda-opus/vibe/impl/Radiant_Impl_Paged_Integration.md` retains the
historical P7 measurements and extensions.

## Current checkpoint

- Workspace: `/Users/henryluo/Projects/radiant`, initial HEAD `340ad59`, clean.
- Goal: establish reproducible P7 receipts, implement deferred page floats,
  then progress through the approved advanced-fragmentation backlog.
- Constraints: native sources/tests/build stay in Radiant; Lambda and input
  sources are read-only. Preserve reference results, page budgets, exact source
  coverage, physical PDF geometry and retained edition lifetimes.
- Observation: `paged_flow_anchor` reads `float-defer` but only logs that it is
  ignored; current page floats remain beside their anchors.
- First implementation milestone: numeric float deferral for top/bottom page
  floats, correct reservations and queued completion, ordered placement,
  page-budget failure and exactly-once published material.
- Acceptance: compare defer 0/1/2 against authored page controls; verify body
  text stays on its original page when only the figure is deferred; test
  end-of-document material, top/bottom slots, blank sheets and failed admission.
- P7: retain independent visual, geometry, release timing, source-size and
  platform verdicts. Historical `temp/paged_release` artifacts are absent in
  the available Lambda checkouts; do not reconstruct passing evidence from
  the old prose totals.
- Completed first native milestone: 16 focused tests; positive page offsets in
  one-column, one-sequence flows; destination sizing and shared CSS variable
  computation; 3 authored preview/PDF/Poppler controls and a failing old-engine
  control. Owner baseline: 944 native passes, one disabled case; 4,481 layout
  passes and six skips. Lint and whitespace checks pass.
- P7 receipts distinguish source inventory and current core fidelity from the
  unavailable historical baseline. The collector pins the host, decodes pixels,
  verifies literal color witnesses, records every page's PDF geometry and uses
  independent Poppler rasters; it does not write reference data.
- Implemented second native milestone: nested named-page width transitions inside
  an open auto-width ordinary-flow containing chain. The before case rejects
  the change; acceptance includes portrait/wide/portrait restoration, percentage
  children, enclosing padding, source coverage and retained fragment widths.
- Historical replay waiver (user, 2026-11-12): assume the original P7 archive
  was deleted/lost and ignore that comparison. RI-028 is closed by waiver,
  not by passing the old visual/performance/source-reduction measurements.
  Fresh immutable receipts define the current qualification scope.
- Second-milestone focused gates: 19/19 tests, including portrait/wide/portrait,
  percentages, enclosing padding, per-fragment widths and deeper ancestor resume.
  Four authored controls pass exact preview/PDF/Poppler pixels, literal color
  witnesses and physical geometry. Earlier focused failures are retained.
- Fresh optimized baseline: 7 samples each after 2 warmups, 21 and 300 pages,
  fixed ordinary paragraph/block workloads; all page counts and geometry/glyph
  digests agree across samples. Initial medians are 0.890 and 0.796 ms/page.
  These are current baseline timings, not a historical speedup comparison.
- Active qualification: rebuild the integration host; run current native,
  paged-media and full PDF/export gates; retain frozen host/build receipts.
- Next implementation milestone: last-page body-column balancing, followed by
  flex/grid row-level breaks. These remain unimplemented; ordinary page-flow
  width changes retain the bounded auto-width containing-chain profile.

## Ordered remaining milestones

1. Reproducible P7 receipt and source inventory, independent reference controls.
2. Deferred page-float scheduling through shared page-flow hooks.
3. Continuation relayout across changed page widths inside an open flow.
4. Last-page body-column balancing.
5. Flex/grid row-level pagination.
6. Final independent raster classification, optimized composition profiling,
   structural simplification/source-size assessment, and platform qualification.

Only one implementation milestone is active. A native increment requires
focused owner-local checks, layout integer-cast lint and the Radiant baseline.
Release performance uses optimized builds and matched inputs/executables;
correctness and timing receipts remain distinct.
