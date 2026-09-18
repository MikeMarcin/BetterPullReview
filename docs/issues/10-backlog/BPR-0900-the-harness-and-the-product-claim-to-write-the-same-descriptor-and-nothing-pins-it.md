---
schema: "implement-rfc.card.v1"
id: "BPR-0900"
title: "The harness and the product claim to write the same owner-private descriptor, and nothing pins that claim"
status: "Backlog"
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
