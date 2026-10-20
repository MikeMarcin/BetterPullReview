---
schema: "implement-rfc.card.v1"
id: "BPR-1672"
title: "Save update settings with public release access"
status: "Done"
rfcs: []
depends_on: ["BPR-1669"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Found while preparing BPR-1671's attended review v001, 2026-10-20: the native public release binding is refused by the SQLite update store","reservation":"docs/issues/_reservations/BPR-1672--BPR-1672--3bc0e2141819.json","related_cards":["BPR-1669","BPR-1671"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"bc2f4bcf9","evidence":{"kind":"command","detail":"Candidate is the working tree on bc2f4bcf9, committed as this card's slice. application_release_update_public_path's new public_release_access_persists course starts the desktop with an eligible installation, the real SQLite update store and the native public account's binding and label (fixture lease), and checks durable(), no unsaved-settings notice, and that the binding and the automatic preference persist. With the binding put back to \"public:github.com\" the course fails: first on the binding's form, and with that check removed on \"update settings save with the public releases selected\". Restored, it passes."}},{"criterion":"AC2","outcome":"pass","revision":"bc2f4bcf9","evidence":{"kind":"observation","detail":"The store contract is unchanged (empty or 64 lowercase hex digits). release_update_public_account() in release_update_desktop.cpp derives the binding as the lowercase hex SHA-256 of a fixed label with QCryptographicHash, as github_application.cpp derives an account's, and start_native offers it first."}},{"criterion":"AC3","outcome":"pass","revision":"bc2f4bcf9","evidence":{"kind":"command","detail":"cmake --build --preset dev; ctest --preset dev -R \"release_update\" (10/10), architecture_boundaries and application_profile_multi_window_public_path pass; coding-format-check, clang-tidy on the touched sources and python3 scripts/verify_documentation.py --root . pass."}}]
---

# Save update settings with public release access

[[BPR-1669]] offered the public releases as the account
`public:github.com`. The SQLite update store keeps only an empty binding or
64 lowercase hex digits, so once the desktop selected that account every save
was refused. A self-updating build then showed "Update settings can't be
saved right now…" and forgot automatic checks and skipped versions on quit.
`release_update_public_path` bound 64 hex digits and never met the native
binding, and the [[BPR-1671]] look keeps settings in memory.

## Acceptance

- AC1: With the native public release access selected, update settings save
  and survive a restart. A regression test fails on the old binding.
- AC2: The store contract stays as it is; the public binding takes an
  account binding's form.
- AC3: Updater tests, format, tidy and documentation checks pass.

## Execution

`release_update_public_account()` (declared in `release_update_desktop.hpp`)
builds the public offer with the binding
`sha256("BetterPullReview release access:public:github.com")` in lowercase
hex, and `start_native` offers it first. No stored binding needs migrating:
the old one was never saved. `docs/architecture/release-updater.md` and
`docs/security/release-updater.md` now say the release repository's releases
are public and how the binding is formed.
