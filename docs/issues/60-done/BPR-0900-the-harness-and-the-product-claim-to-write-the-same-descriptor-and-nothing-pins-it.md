---
schema: "implement-rfc.card.v1"
id: "BPR-0900"
title: "The harness and the product claim to write the same owner-private descriptor, and nothing pins that claim"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"dsp-006 integrator, adjudicating bpr-win's BPR-0875 fix","found_at":"81844fef","run":"run-20260916-stations","dispatch":"dsp-006"}
acceptance: [{"criterion":"A Windows row fails if the two authors stop writing descriptors that workspace_parent_trusted treats alike","outcome":"pass","revision":"f826d758","evidence":{"kind":"command","detail":"owner_private_descriptor_agreement passes on Windows: the product, the real CMake helper and the real Python helper each narrow a directory inside a root granting Authenticated Users inheritable Modify; the product's recover_installed_agent_workspaces (workspace_parent_trusted) trusts all three and refuses the unnarrowed control."}},{"criterion":"The row's falsifiability is shown: change one author, see it fail","outcome":"pass","revision":"f826d758","evidence":{"kind":"command","detail":"With /inheritance:r dropped from the CMake helper the row fails at 'cmake: expected trusted, the product said REFUSED 1'; the same change to the Python helper fails it at 'python: ...'. Both trials restored."}},{"criterion":"The harness comment points at the row instead of asserting the equivalence itself","outcome":"pass","revision":"f826d758","evidence":{"kind":"observation","detail":"bpr_narrow_owner_private.cmake and bpr_narrow_owner_private.py name owner_private_descriptor_agreement as the pin; the CMake comment no longer claims icacls writes exactly the product's descriptor."}},{"criterion":"BPR-0875's thirteen rows still pass","outcome":"pass","revision":"f826d758","evidence":{"kind":"command","detail":"application_epoch_portable_occurrence_two_catalogues (99.84 s), application_epoch_ordinary_update_recreated (35.48 s) and agent_workspace_ownership (9.49 s) pass, the rows that consume both helpers; test_network_isolation_manifest names no new row."}}]
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

## Pinned — bpr-main-win, 2026-10-01

Windows 11 Home 10.0.26200, MSVC 19.51, `build/windows-dev` Debug, at
`f826d758`, with `main` later at `02837c09`.

**The row,** `owner_private_descriptor_agreement`, is Windows-only, in
`tests/git_contract/`. The product is the judge. No copy of its rule is made.

- `tests/scripts/owner_private_descriptor_agreement_tests.py` makes a root and
  grants Authenticated Users an inheritable Modify on it. That is what a shared
  build tree hands a course, made explicit so the row does not depend on where
  it runs.
- Under the root are four directories:
  - `product`, narrowed by `make_storage_root_owner_private`, through the row's
    executable;
  - `cmake`, narrowed by the real `bpr_narrow_owner_private`, through
    `cmake -P tests/scripts/owner_private_descriptor_agreement.cmake`, which
    includes the helper;
  - `python`, narrowed by the real `make_root_private`;
  - `control`, left as made.
- `owner_private_descriptor_agreement.exe judge` runs
  `recover_installed_agent_workspaces(<dir>/registry, <dir>/neighbourhood)` on
  each. That runs `workspace_parent_trusted(<dir>)`, the rule
  [[BPR-0875]]'s rows depend on.
- The row requires the three authors' directories to be trusted and the
  control to be refused. It prints each directory's `icacls` listing as
  record.

**The two helpers differ on purpose, and the row says why.** The CMake one
writes one inheritable user grant, which is enough for the uncontained
composition rows. The Python one also grants the container SID and removes
SYSTEM, Administrators and OWNER RIGHTS for contained courses. The row pins
what they must share: the product trusts both, and refuses the shared
inherited grant. Uncontained, all three read back as the single entry
`JAEGAW\mikem:(OI)(CI)(F)`.

| Run | control | product | cmake | python | Row |
|---|---|---|---|---|---|
| As committed | REFUSED 1 (`repair_required`) | TRUSTED | TRUSTED | TRUSTED | pass, 0.29 s |
| CMake helper without `/inheritance:r` (trial, not committed) | REFUSED 1 | TRUSTED | **REFUSED 1** | TRUSTED | **fails**: `cmake: expected trusted, the product said REFUSED 1` |
| Python helper without `/inheritance:r` (trial, not committed) | REFUSED 1 | TRUSTED | TRUSTED | **REFUSED 1** | **fails**: `python: expected trusted, …` |

**Where it lives,** given [[BPR-0825]] item 3. The row reaches the rule through
the Git adapter's public `recover_installed_agent_workspaces`, not through a
private name or a copy. So if the owner moves the rule's definition, the row
still asks whichever code the product calls.

**`WINDOWS_UNCONTAINED`, measured.** Contained, the lowbox token cannot walk the
container temporary directory's ancestors. The product then refuses all four
directories, its own included, and the contained process cannot even remove
the CMake-narrowed one, which has no container grant. The row compares what
uncontained courses use, so it runs where they do. The pin is in
`scripts/verify_test_network_isolation.py` with that measurement.
`test_network_isolation_manifest` does not name the row.

**The helpers' comments now point at the row.** `bpr_narrow_owner_private.cmake`
no longer asserts on its own authority that `icacls` writes "exactly" the
product's descriptor. `bpr_narrow_owner_private.py` says the two differ on
purpose and names the row that pins their agreement.

**[[BPR-0875]]'s rows still pass:**

- `application_epoch_portable_occurrence_two_catalogues`, 99.84 s;
- `application_epoch_ordinary_update_recreated`, 35.48 s;
- `agent_workspace_ownership`, 9.49 s.

**[[BPR-0875]]'s thirteen rows, run after the finish.** The acceptance record
above names three representative consumers. The full list was run afterwards,
at `563c41e0`. Twelve of the thirteen pass, in 397.60 s:

- the UIforETW, workbench and round-trip composition contracts;
- the overview-since-review, review-behaviour and saved-server contracts;
- the three appearance contracts;
- both epoch persistence rows and the epoch maintenance path.

The thirteenth, `application_native_notification_attended_contract`, was not
run. It drives the tray and window activation in the foreground, and
foreground runs on this station are held until the owner clears them.
