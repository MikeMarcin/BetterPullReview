---
schema: "implement-rfc.card.v1"
id: "BPR-0900"
title: "The harness and the product claim to write the same owner-private descriptor, and nothing pins that claim"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"dsp-006 integrator, adjudicating bpr-win's BPR-0875 fix","found_at":"81844fef","run":"run-20260916-stations","dispatch":"dsp-006"}
---

# BPR-0900: The harness and the product claim to write the same owner-private descriptor, and nothing pins that claim

## Defect

There are two authors of the Windows owner-private security descriptor, and
that is **deliberate**:

- `adapters::sqlite::create_storage_root_owner_private` narrows a root the
  product owns.
- `tests/end_to_end/bpr_narrow_owner_private.cmake` narrows a directory the
  product deliberately refuses to touch — which is the whole point of
  `workspace_parent_trusted`. Collapsing the two would have the harness reach
  for a product function to prepare a directory the product would never prepare
  itself.

The problem is not that there are two. It is that the harness's own comment
asserts they agree — it says `icacls` writes "exactly that", and "it is the same
descriptor the contract reads back on the root the application narrows"
([[BPR-0436]]) — and **nothing checks it**. The claim is load-bearing and
unpinned.

[[BPR-0875]]'s fix makes that claim carry more weight than before: thirteen
Windows rows now depend on the harness-written parent being the shape
`workspace_parent_trusted` accepts. If the product's definition moves — and
[[BPR-0825]] item 3 is an **open owner question** about exactly where that
definition should live — the harness's `icacls` line diverges silently. The
thirteen rows then either start failing for a reason nobody will connect to the
move, or keep passing while proving a different shape than the product uses.
The second is worse.

This is the same drift class this run has already found twice: [[BPR-0867]]'s
helper requiring a newer interpreter than the tree declared, and the round-trip
digest pins going stale without saying so.

## Scope

- Pin the equivalence rather than asserting it in a comment. The natural shape
  is a Windows row that has the product create an owner-private root, has the
  harness create one, and compares the resulting descriptors — not string
  equality of `icacls` output, but the properties that matter: inheritance
  disabled, and no allow entry granting write to a principal outside the set
  `workspace_parent_trusted` skips.
- Prefer a check that would fail for the *right* reason. A test that merely
  runs both and passes proves nothing; it must fail when one author changes and
  the other does not.
- Decide where that row lives so it survives [[BPR-0825]] item 3's answer,
  since that answer may move the product's definition.
- While there: `bpr_narrow_owner_private`'s comment should say the equivalence
  is pinned by <row>, rather than asserting it on its own authority.

## Acceptance

- A Windows row fails if the two authors stop writing descriptors that
  `workspace_parent_trusted` treats alike.
- The row's falsifiability is shown: change one author, see it fail.
- The harness comment points at the row instead of asserting the equivalence
  itself.
- [[BPR-0875]]'s thirteen rows still pass.

## There are three authors, not two — dsp-008

Found while answering `bpr-win`'s question about where a tests-owned Windows
narrowing helper should live for the Python courses. The answer turned out to
be that it already exists, and it does not agree with the CMake one.

`make_root_private` is **byte-identical** in
`tests/end_to_end/epoch_ordinary_update_public_path.py:23-44` and
`tests/end_to_end/epoch_portable_occurrence_public_path.py:18-39`, with
`appcontainer_sid` beside it in both. So the harness has two Windows
descriptor authors of its own, and the product a third.

The two harness authors write **different descriptors**, and the Python one is
strictly stronger:

| | `bpr_narrow_owner_private.cmake` | `make_root_private` (Python) |
|---|---|---|
| `icacls /inheritance:r` | yes | yes |
| `/grant:r <user>:(OI)(CI)F` | yes | yes |
| `/grant:r *<AppContainerSID>:(OI)(CI)F` when contained | **no** | yes |
| `/remove:g *S-1-5-18 *S-1-5-32-544 *S-1-3-4` | **no** | yes |

The Python docstring says it is matching the product's
`make_storage_root_owner_private`, and cites [[BPR-0834]]; the CMake comment
says `icacls` writes "exactly that" and cites [[BPR-0436]]. **Both claim to
match the product. They cannot both be right, because they differ from each
other.** That is this card's subject, with one more author than it was written
against, and it is now a concrete discrepancy rather than an unpinned claim.

Two consequences for the acceptance above:

- The row this card asks for should compare **all three** authors, or state
  which pair it pins and why the third is out of scope. A row pinning the
  CMake helper against the product would leave the two Python copies free to
  drift, and they are the ones a contained course uses.
- The promotion this discovery triggers — both copies into
  `tests/end_to_end/bpr_narrow_owner_private.py`, named to match the `.cmake`
  helper — reduces three authors to two before the row is written, which makes
  the row cheaper. It is assigned to `bpr-win`, whose scope owns those files.

**Do not treat the table above as the cause of `bpr-win`'s six remaining
container-temp rows.** It is a lead, and a cheap one, but that station measured
`application_profile_owner_admission_public_path` failing **uncontained** as
well, which rules the AppContainer grant out for that row specifically. If the
container grant does turn out to be what the family needs, that belongs in its
own commit with its own sentence rather than inside a six-row fix.

## The promotion is done, and the divergence is now measured in both directions — dsp-009

Recorded from `bpr-win`'s CHECKPOINT `cbf6d920` (windows-lane), at that
station's request; the station itself edits only `tests/**`.

**Three authors are two again.** `make_root_private` and `appcontainer_sid`
now live once, in `tests/end_to_end/bpr_narrow_owner_private.py`, named to
match the `.cmake` helper beside it, and both epoch courses import it. Both
pass on Windows at that tip: `application_epoch_portable_occurrence_two_catalogues`
90.38 s, `application_epoch_ordinary_update_recreated` 31.23 s, ctest exit 0.
No third spelling was written. The CMake helper's descriptor was deliberately
not changed.

**Neither helper is wrong; they answer different questions**, and that is the
sentence this card was missing:

- the CMake descriptor — one inheritable user grant, inheritance removed — is
  **enough** for the two composition rows `df54d00b` fixed;
- the Python descriptor — the same plus the process's AppContainer SID and the
  SYSTEM / Administrators / OWNER RIGHTS removals — is **not enough** for the
  contained container-temp courses. `application_profile_owner_admission_public_path`
  still refuses at the registry (`SEED_REGISTRY_REFUSAL:0`) when contained,
  even with the container SID granted on the leaf. The refusal is upstream of
  the leaf: `open_state_directory` → `validate_ancestor_chain`, and the
  ancestors of the container's `AC\Temp` are not openable by the lowbox token.
  No descriptor written on the leaf can change what the token can open above
  it. **So the contained question is not a descriptor question at all**, and
  the AppContainer-grant lead dsp-008 recorded is closed for that row: measured,
  not the missing piece.

Uncontained, the promoted descriptor *does* get that row's seed accepted where
it was refused, and the course then fails later and elsewhere (its own
"bounded public course condition"), so the family has split into two obstacles
and a `WINDOWS_UNCONTAINED` pin alone would not make the row pass. That is
`bpr-win`'s next measurement, not this card's.

Consequence for the acceptance row: pin the two remaining harness authors
against the product, and say in the row that they differ on purpose, with the
two facts above as the reason each is shaped as it is.
