---
schema: "implement-rfc.card.v1"
id: "BPR-1308"
title: "Present one native repository addition dialog"
status: "Done"
rfcs: ["RFC-BPR-0062"]
depends_on: ["BPR-1307"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
acceptance: [{"criterion":"C1","outcome":"pass","revision":"8e2f855e9061b4ce9cc150c1668356c8301b9b01","evidence":{"kind":"command","detail":"Qt Home/discovery at 100/125/200: shared Settings/Home native dialog, exact destination, raw draft, correlated progress/refusal/duplicate, Return and Escape with focus restored, accessible input and long wrapped destination; focused integration 8/8 and final profile palette renders pass."}},{"criterion":"C2","outcome":"pass","revision":"8e2f855e9061b4ce9cc150c1668356c8301b9b01","evidence":{"kind":"command","detail":"Composed Home course refuses PR-shaped and foreign-host input, explicit server setup preserves draft, Sign-in Cancel returns without save/search; setup host/core course rejects stale sign-in/server completions and public preview for repository preparation, returns to draft requiring fresh Add; authority-replacement Qt course passes."}}]
---

# Present one native repository addition dialog

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: Home and Settings open the same addition dialog with exact account/host, draft input, progress, typed refusal, keyboard completion and cancel focus restoration.
- C2: Explicit setup/sign-in preserves the draft, cancellation saves nothing, and authority change requires a fresh final gesture; no automatic save or modal.

## Composition reseal review (2026-10-08)

fd96a3876 changed `src/app/src/github_application.cpp` without resealing the
round-trip composition pins, so `application_review_round_trip_composition_contract`
and `application_composition_contract` were red on `origin/main`
(`docs/releases/v0.1.5.md` records this as a prerelease exception). Review of
that file's diff:

- `submit_repository_setup` retires CLI discovery and calls the setup host's
  `submit_repository`. It does not cancel restoration, because preparing a
  repository opens no pull request.
- In `open()`, a repository-kind destination returns `repository_prepared`
  only when the session is authenticated for the exact profile, host and
  hostname, and `unavailable` otherwise, including under public preview. It
  returns before any pending link location, navigation or read, and it saves
  nothing. Adding the repository still needs a new Add gesture.
- `set_repository_addition_dispatch` sends to the queue host's existing
  `apply(repository_addition_intent)`. The composition overwrites `Source`
  with this window's `DraftWindow`, so Qt cannot name another attachment, and
  the queue publishes the gesture result only to that attachment.

The change adds no grant, target, provider, credential, GitHub write, mutation
or publication path. The mutation-branch digest is unchanged
(`e067befe…` at fd96a3876 and at `origin/main`). The full-application and
app-manifest pins were resealed with BPR-1313's and BPR-1316's changes in the
commit that adds this note.
