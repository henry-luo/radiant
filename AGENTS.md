# Radiant agent instructions

D9.1.1v3 and D9.1.3v2 require Radiant to depend on lambda-input, not on the Lambda runtime. Preserve D5.5 ownership, D6.3 precise rooting through host adapters, and the user-visible rendering behavior. Follow `radiant/README.md` for the development workflow and the sibling Lambda `AGENTS.md` during migration.

Use C++17 and project lib types, not C++ standard-library containers. Do not edit vendored dependencies, generated Lua, or log.conf; do not write temporary files outside ./temp/. In layout code use float dimensions and mark necessary integer casts with INT_CAST_OK. Fix root causes and preserve test coverage.
