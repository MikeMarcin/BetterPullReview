---
schema: "implement-rfc.card.v1"
id: "BPR-0915"
title: "Consolidate Home and PR setup into shared sign-in"
status: "Done"
rfcs: ["RFC-BPR-0015", "RFC-BPR-0018", "RFC-BPR-0032"]
depends_on: ["BPR-0914"]
priority: "P1"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"cbd50b749971f4fcc66cfb16086f6af7a355a1fd","evidence":{"kind":"observation","detail":"Home and PR links share the reused account panel in a focused dialog; registered device course, Qt window course, and post-setup Settings management pass."}},{"criterion":"AC2","outcome":"pass","revision":"cbd50b749971f4fcc66cfb16086f6af7a355a1fd","evidence":{"kind":"observation","detail":"Public-path fixture verifies first-click CLI discovery, one trust confirmation for a sole relevant account, multiple named account choices, and discarded canceled inventory completion."}},{"criterion":"AC3","outcome":"pass","revision":"cbd50b749971f4fcc66cfb16086f6af7a355a1fd","evidence":{"kind":"observation","detail":"Host and composed tests verify native and CLI exact-host selection, unknown CLI host, single destination opening, cancellation, replacement, cross-host refusal, and native staged rollback; CLI has zero native vault access."}},{"criterion":"AC4","outcome":"pass","revision":"cbd50b749971f4fcc66cfb16086f6af7a355a1fd","evidence":{"kind":"observation","detail":"Four focused CTest checks pass (build/bpr0915-tests.log); app builds; documentation gate and 64 documentation tests pass. Dialog captures inspected. Additional non-fixture stage-0 inventory probe failure documented in card; no full-suite success claimed."}}]
---

# Shared sign-in flow

Owner mandate: clean up the duplicate setup flow, use a focused sign-in dialog
shared by Home and PR links, and eliminate repeated CLI clicks. Direct mode,
no sub-agents. This explicit UI amendment supersedes RFC-0032's requirement
that active sign-in be presented only inside Settings; authentication authority,
credential persistence and per-write approval remain unchanged.

## Bounded milestone and acceptance

- AC1: Home sign-in and PR setup use one focused account surface with the same
  native/device/CLI operations. Settings retains connection management.
- AC2: Use CLI starts discovery immediately. A sole relevant account advances
  to the existing trust confirmation; multiple accounts offer a meaningful
  choice, with no repeated discovery click. Cancellation defeats late results.
- AC3: A pending PR survives exact-host account choice and opens once after
  acceptance; cancellation, replacement and another host cannot consume it.
- AC4: Focused host, Qt and public-path tests and documentation checks pass.

Baseline: `bf45f693`. Prior investigation is recorded in
`build/sign-in-flow-audit.md`. No unrelated working files were present.
Reuse the existing session/device policies, staged native enrollment rollback,
and account widgets rather than introduce another authentication implementation.
Home personal PR discovery is outside this setup cleanup.

## Implementation and verification

Home, PR setup and Settings connection actions now reuse one account panel in a
focused dialog. The dialog returns the same widgets to Settings when it closes.
Management rows and duplicate setup/cancel controls are hidden during sign-in.
CLI discovery starts from the first gesture, filters choices to a pending PR's
host, and advances a sole match to the existing trust confirmation. Discovery
completion is generation-guarded against cancellation or replacement. Explicit
account selection binds the pending destination through the core's exact-host
resolver; native enrollment retains its existing staged rollback path.

Focused verification: `application_setup_destination`,
`application_setup_destination_public_path`, `qt_workbench_window_scale_100`, and
`application_device_authorization_registered_course`. The composed fixture covers
Home CLI, paste-first CLI with an existing native option, an unknown Enterprise
host found through CLI, multiple-account choice, cancellation before discovery
completion, native setup cancellation/replacement/failure, and Settings management
after acceptance. CLI courses perform no native credential-vault operations.
Dialog captures in `build/bpr0915-render/` were inspected locally. Documentation
gate and all 64 documentation script tests pass; changed C++ files are formatted.
Build and test logs: `build/bpr0915-build.log`, `build/bpr0915-tests.log`,
`build/bpr0915-docs.log`, `build/bpr0915-doc-tests.log` (local, not committed).

An additional non-fixture `application_device_authorization_public_path` probe
fails its initial CLI-profile inventory check at stage 0, before any connection
or device request. Its inventory/count gate is unchanged by this slice; the
registered, isolated device course exercises the changed dialog and passes.
This is a verification limitation, not a claim that the entire suite passes.
No live GHE service, release installation, or automatic Home PR discovery was
validated or changed by this slice.
