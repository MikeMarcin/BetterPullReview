# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **remember-cli-connections-v1** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## connection-metadata

A token-free selected-connection snapshot has bounded deterministic policy and private transactional durability, ready for shared runtime wiring.

**Work:** `BPR-1408`

**Prerequisites:** None

**Completion:**

- Canonical bounded metadata stores no credential or publication authority and enforces one connection per server with stable identity and selected order.
- Pure policy rejects stale selection, supports disabled reuse and Forget, and preserves independent profile state.
- Real SQLite reopen, transactional rollback, stale writes, cancellation and malformed/newer/refused storage preserve complete data or refuse without authorization.
- Profile maintenance inventories and purges the metadata with focused format, architecture and documentation checks.

**Boundary:** Metadata and policy capability only. Application startup, successful-selection admission and composed restart acceptance remain in the next milestone.

## shared-profile-restoration

The shared profile runtime remembers accepted connections and revalidates the exact selected CLI account across launches without repeating discovery or consent.

**Work:** `BPR-1409`

**Prerequisites:** connection-metadata

**Completion:**

- Implement RFC-BPR-0071 AC01–AC04 through shared application startup, exact-host navigation and replacement/sign-out/forget effects.
- Persist successful selection promptly, report session-only or disable-save failure, drain admitted saves on Quit, and reject late completions.
- Restore pinned host/login/numeric account without inventory or trust prompts; failures remain visible and retry exact authority without private-state exposure.

**Boundary:** Full runtime integration; composed qualification and final closeout remain.

## restart-qualification-and-closeout

All six RFC criteria have direct composed macOS evidence and the amended contracts and closeout match the final candidate.

**Work:** `BPR-1410`, `RFC-BPR-0071`

**Prerequisites:** shared-profile-restoration

**Completion:**

- Every RFC-BPR-0071 AC01–AC06 is demonstrated against the RFC baseline through composed macOS fixture courses, including fresh processes and last-window closure.
- Tokenless execution, publication consent, global CLI state, storage/refusal and profile maintenance canaries pass; all owning contracts describe implemented scope.
- Focused integration and documentation evidence cover the exact final candidate; required acceptance remains explicit.

**Boundary:** Full authorized RFC and amendment acceptance; no independent or human acceptance is inferred.
