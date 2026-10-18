---
schema: "implement-rfc.card.v1"
id: "BPR-1600"
title: "The Refresh key refreshes Home too"
status: "Done"
rfcs: ["RFC-BPR-0026"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"owner decision 2026-10-17","owner_note":"⌘R (QKeySequence::Refresh / Ctrl+R) should refresh Home too, reversing BPR-0990's choice that Home's refresh has no key.","governing_rfc":"RFC-BPR-0026","related_cards":["BPR-0990","BPR-1567","BPR-1575","BPR-1580"],"reservation":"docs/issues/_reservations/BPR-1600--BPR-1600--5a60520b8335.json","review":"lands without an owner review once the tests prove it (a key binding for an existing action)"}
acceptance: [{"criterion":"BPR-1600:home","outcome":"pass","revision":"db2cd82d138436ea3fd6f58fc8dcbabf36997ea5","evidence":{"kind":"observation","detail":"Home key dispatches one existing discovery refresh and reuses reviewer feedback; in-flight press is refused."}},{"criterion":"BPR-1600:pull-request","outcome":"pass","revision":"db2cd82d138436ea3fd6f58fc8dcbabf36997ea5","evidence":{"kind":"observation","detail":"PR refresh remains PR-specific; returning Home transfers the binding back without double dispatch."}},{"criterion":"BPR-1600:typing","outcome":"pass","revision":"db2cd82d138436ea3fd6f58fc8dcbabf36997ea5","evidence":{"kind":"observation","detail":"Typing behavior matches PR refresh; Home Open bar text/focus stay intact."}},{"criterion":"BPR-1600:negative","outcome":"pass","revision":"db2cd82d138436ea3fd6f58fc8dcbabf36997ea5","evidence":{"kind":"observation","detail":"Signed-out press refreshes nothing; removing key handoff fails the catalogue test, restoring exact bytes passes."}},{"criterion":"BPR-1600:catalogue","outcome":"pass","revision":"db2cd82d138436ea3fd6f58fc8dcbabf36997ea5","evidence":{"kind":"observation","detail":"Catalogue drift passes at all three scales, exactly one Refresh binding in each window state."}},{"criterion":"BPR-1600:gates","outcome":"pass","revision":"db2cd82d138436ea3fd6f58fc8dcbabf36997ea5","evidence":{"kind":"observation","detail":"14 focused tests, restored control, format22, focused tidy22 with warnings-as-errors, documentation and diff checks pass."}}]
---

# BPR-1600: The Refresh key refreshes Home too

The owner decided on 2026-10-17 that ⌘R (`QKeySequence::Refresh`, Ctrl+R or
F5 off macOS) should refresh Home too, reversing [[BPR-0990]]'s choice that
Home's refresh has no key. The amendment is recorded against
[RFC-BPR-0026](../../rfcs/RFC-BPR-0026-personal-review-home-pr-url-navigation.md),
which governs Home's explicit Refresh action.

## Scope

- One window shows Home or a pull request; the Refresh key does whichever
  refresh is showing. On Home it triggers Home's existing
  `refreshQueueAction`, the same path as its strip button, with
  [[BPR-1575]]'s feedback ("Finding my pull requests…", the result note,
  failure and Retry). On a pull request it stays `refreshPullRequestAction`.
- The key is on one action at a time (`workbench_window::apply_refresh_key`),
  taken off one before it is put on the other, so no press is ambiguous.
- The pull request's binding does not step aside while typing, so Home's does
  not either: ⌘R in Home's Open bar refreshes Home and leaves the text alone.
- Help → Keyboard Shortcuts ([[BPR-1567]]) lists "Refresh my pull requests"
  (On Home) beside "Refresh the pull request" (With a pull request showing).
  [[BPR-1580]]'s View menu and Home's refresh tooltip do not mention a key and
  are unchanged.

## Acceptance

- `BPR-1600:home`: on Home, signed in, ⌘R sends exactly one discovery
  refresh, the reviewer's, with the note, a failure with Retry and a result;
  a second press while it runs starts nothing.
- `BPR-1600:pull-request`: on a pull request ⌘R refreshes the pull request
  once and not Home; with Home over the review the key is Home's again.
- `BPR-1600:typing`: neither key steps aside while typing; ⌘R from the Open
  bar refreshes Home and leaves the bar's text and focus.
- `BPR-1600:negative`: signed out on Home, ⌘R refreshes nothing.
- `BPR-1600:catalogue`: the catalogue drift check passes in Review and on
  Home; no key is bound twice.
- `BPR-1600:gates`: clang-format 22, tidy on the changed lines, the
  documentation gate.

## Implementation and landing evidence — 2026-10-17

Resumed the existing uncommitted card and implementation, preserved them in
`324653f18`, and merged main `93ebf4eef` at `db2cd82d1`. Qt moves the
existing Refresh binding between the two existing actions; core, host, request
and feedback semantics are unchanged. The catalogue describes both conditional
bindings. The owner reversal is recorded in RFC-BPR-0026 and the UX specification.

- Built the workbench, composed GitHub and refresh-feedback Qt targets. All
  14 focused rows pass (30.69 s): shortcut/catalogue, Home discovery, focus
  order and composed GitHub at 100%, 125% and 200%, plus typing and refresh
  feedback. They cover signed-out refusal, in-flight refusal, Home over an
  open review, PR refresh, and the Open bar's unchanged text/focus.
- Negative control: removed only `apply_refresh_key(home)` from the Home/PR
  transition, rebuilt, and `qt_keyboard_shortcuts_scale_100` failed at
  "on Home the Refresh key is bound once". Restored the exact original bytes,
  rebuilt, and that row passed (1.66 s). No control edit remains.
- clang-format 22 passes all six touched C++ files; clang-tidy 22 with
  warnings as errors passes all four touched translation units; documentation
  and diff gates pass. No `src/app` delta, so no composition reseal is needed.
- The owner explicitly requires no new visual review. Frontend-design was
  loaded for this queue; this change binds an existing action. The one full
  suite and repository format/tidy remain scheduled at handoff item 9.
