# Agent Instructions for Radiant

This file contains Radiant-specific agent rules. Use [README.md](README.md)
for setup, build profiles, test targets, fixture requirements, and platform
notes. See [radiant/README.md](radiant/README.md) for subsystem boundaries
and native development guidance.

## Ownership and project boundaries

Radiant owns the native layout, rendering, event capture, hit testing, and
state-store code under `radiant/`; `build_targets.json` is its source
manifest. It depends on lambda-input through the configured input root.
Under D9.1.1v4 and D9.1.3v2, the Radiant archive must remain independent of
the Lambda runtime. The standalone `radiant.exe` may link Lambda as its
optional scripting host.

Preserve D5.5 ownership and D6.3 precise rooting through host adapters. Keep
layout, render, and UI behavior consistent with the accepted design. Do not
change Lambda or lambda-input implementation, headers, tests, or build files
as part of Radiant work. Record Radiant-owned issues in
[Radiant Issue Ledger](vibe/Radiant_Issue_Ledger.md), input-owned issues in
`../lambda-input/vibe/Lib_Input_Issue_Ledger.md`, and Lambda-owned issues in
Lambda's `vibe/Lambda_Issue_Ledger.md`. Include observed behavior,
reproduction, evidence, and whether the issue blocks the task; mark
unconfirmed causes as unconfirmed. If an issue blocks the task, record it and
stop the affected work while continuing independent work where possible.

## Rules for changes

1. Find the root cause before changing behavior. Do not hard-code results or
   add workarounds to hide a defect. Keep a failing test or baseline mismatch
   visible when the cause is not fixed.
2. Write temporary files under this checkout's `./temp/`, never `/tmp`.
3. Follow C++17 and the project C+ conventions. Use project `lib` types
   instead of introducing C++ standard-library containers.
4. In layout code, use `float` for position and dimension values. Do not use
   `int` for layout geometry. If a cast is required for a non-geometry value,
   add `// INT_CAST_OK: <reason>` at the cast.
5. Run the layout integer-cast check for layout changes:
   `make lint ARGS='--rule ^no-int-cast-radiant$'`.
6. Do not use `printf`, `fprintf`, or `std::cout` for debugging. Use
   `log_debug()`, `log_info()`, or `log_error()` from the project logging
   API, with a distinct prefix at the start of each log line.
7. Do not modify `log.conf`.
8. Do not hand-edit generated Premake/Lua files or generated projects. Update
   the owner source manifest `build_targets.json` and use the documented
   build workflow.
9. Do not patch vendored dependencies. Fix defects in Radiant-owned code; if
   the root cause belongs upstream, stop and explain it before proposing an
   upstream patch.
10. Do not duplicate helpers. Search for an existing helper first. Extract a
    shared helper before adding a third near-identical case; promote a shared
    `static` helper through its module header instead of copying it.
11. Add a concise, lowercase-starting comment for a non-trivial fix when it
    explains why the code is needed. Do not add narration.
12. Do not weaken layout tolerances, remove failing cases, or change reference
    results to make a baseline pass. Fix the rendering or layout cause.
13. Use a release profile for performance measurements.

When a design or implementation record is added or changed, cite the relevant
Lambda formal ruling by `S#` or `D#` when it covers the topic. The issue
ledger records evidence and status; it does not change design rulings.

## Build and verification

Use the profiles and commands documented in [README.md](README.md). The native
archive stays runtime-free; the standalone executable is a host that can link
the Lambda runtime.

```sh
make PROFILE=debug                  # build the Radiant archive
make PROFILE=release                # build the release archive
make radiant.exe                    # build the standalone host executable
make test-native                    # run native GTest suites
make test-layout-smoke              # exercise static fixtures without a script host
make test-layout-baseline            # compare configured layout suites
make run-radiant-baseline           # run native and layout baseline gates
```

Use focused checks during implementation and aggregate baseline gates at a
milestone or task completion. Some targets use shared `lambda-test` fixtures,
a configured Lambda host, or platform-specific dependencies; consult the
README before running them. Existing mismatches remain visible and must not be
masked by changing the references or tolerance.

## Entry points

| Area | Start here |
|---|---|
| Build ownership | `build_targets.json`, `Makefile` |
| View, style, and animation data | `radiant/view.hpp` |
| Layout contexts and line boxes | `radiant/layout.hpp`, `radiant/layout_*.cpp` |
| Rendering and display lists | `radiant/render.hpp`, `radiant/paint/` |
| Events and input | `radiant/event.hpp` |
| Application and host API | `radiant/radiant.hpp`, `radiant/main.cpp` |
| Issue records | `vibe/Radiant_Issue_Ledger.md` |
