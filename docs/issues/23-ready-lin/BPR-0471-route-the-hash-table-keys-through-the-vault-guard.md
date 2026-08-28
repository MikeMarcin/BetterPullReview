---
id: BPR-0471
title: Route the vault's hash-table keys through the null-termination guard
status: Ready
rfc: RFC-BPR-0013
depends_on: []
---

# Route the vault's hash-table keys through the null-termination guard

The RFC-BPR-0002 closeout's capability-critical review read [[BPR-0444]]'s
guard and found the three sites it does not cover.
`src/adapters/github/src/linux_secret_service.cpp` still passes
literal-backed `std::string_view` values through `data()` into GLib
hash-table calls whose tables libsecret then consumes: the
`g_hash_table_insert` of the `authority` attribute key, and the two
`g_hash_table_lookup` calls over the attribute names. Each reads to a
terminator it is not given a length for.

They are correct today because every view is `constexpr` over a string
literal — the same "correct today, unguarded tomorrow" shape BPR-0444
existed to eliminate at the direct libsecret arguments. The architecture
document's guard claim was scoped to direct libsecret arguments in the
closeout, so no document overclaims meanwhile; this card makes the guard
total instead.

## Required final state

- The three hash-table sites take their pointers through the same
  `consteval` `c_str` guard the direct libsecret arguments use (the views
  are literal-backed, so the calls compile), or a recorded reason names why
  a site cannot.
- The architecture document's scoping sentence is restored to the total
  claim once the guard is total.
- `coding-tidy` stays clean on the file, the boundary suites stay green,
  and `linux_secret_service_contract` passes — Linux, where the file
  compiles.

## Non-goals

- No behavior change: `c_str` returns the identical pointer `data()`
  returns for these views.
- Not the native Secret Service trio, which stays [[BPR-0310]]'s.
