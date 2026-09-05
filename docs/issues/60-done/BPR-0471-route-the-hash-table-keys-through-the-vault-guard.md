---
schema: "implement-rfc.card.v1"
id: "BPR-0471"
title: "Route the vault's hash-table keys through the null-termination guard"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: []
completed: "2026-08-27"
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

## Result — 2026-08-27

All three routed. There was no site that could not be, so no exception is
recorded.

| Site | Was | Is |
|---|---|---|
| `make_attributes`, the inserted key | `const_cast<char*>(authority_attribute.data())` | `const_cast<char*>(c_str(authority_attribute))` |
| `exact_attributes`, authority lookup | `g_hash_table_lookup(..., authority_attribute.data())` | `g_hash_table_lookup(..., c_str(authority_attribute))` |
| `exact_attributes`, schema lookup | `g_hash_table_lookup(..., schema_attribute.data())` | `g_hash_table_lookup(..., c_str(schema_attribute))` |

No `std::string_view` in this file now reaches a libsecret-consumed pointer
through `data()`. The two remaining `.data()` calls are on
`null_terminated_authority_account`, a `std::array<char, N + 1>` whose
terminator is part of its type, and on `text` inside the guard itself.

### The review was right, and about the claim as much as the code

[[BPR-0444]] wrote into `github-host-implementation.md` that no fixed string
reached a libsecret call through `data()`. That was an overclaim: three did.
The closeout review caught it, narrowed the sentence to what was true, and
carded the rest here rather than letting the document assert a guard that was
not total. The sentence is now restored to the total form, and it is true.

### Pinned, or it would not stay total

Routing the three calls does not by itself keep them routed — reverting one
compiles and passes every suite. `VerifyArchitectureBoundaries.cmake` now
pins all three alongside the four direct libsecret arguments, and the
boundary fixture carries the guarded shapes with a removal case for each, so
the pins are exercised rather than merely present.

| Claim | Command | Result |
|---|---|---|
| the file stays clean | `tidy.py --build-dir build/dev --warnings-as-errors --paths src/adapters/github/src/linux_secret_service.cpp` | clean |
| the new pins bind | revert the schema lookup to `.data()`, then `ctest --preset dev -R '^architecture_boundaries$'` | Failed, as required |
| the pinned lifecycle still holds | `ctest --preset dev -R '^architecture_boundaries$'` | Passed |
| the pins are exercised | `ctest --preset dev -R '^architecture_boundary_contract$'` | Passed, 130 cases |
| behaviour preserved | `ctest --preset dev -R '^linux_secret_service_contract$'` | Passed |
| the real adapter still behaves through libsecret | `--expect-unavailable`, `--expect-locked`, `--expect-bus-redirect-rejected` | 3 of 3 |
| format | `format_check.py --clang-format /usr/bin/clang-format` | passed |
| documentation | `verify_documentation.py --root .` | DOCUMENTATION_OK |

RFC-BPR-0002 is already `Auto-Verified`; this card is RFC-BPR-0013 and
reopens nothing. The registered native trio still fails on this host and is
still [[BPR-0310]]'s.
