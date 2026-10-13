# Smooth diff touchpad panning continuation

Assignment: `/root/smooth_panning_flush_reveal`, non-delegating `rfc-implementer`, coordinator/implementer recipe, milestone `smooth-comparison-panning`.

Mandate: `/implement-rfc use coordinator/implementer to Implement the RFC at /Users/jaeger/Documents/GitHub/BetterPullReview/docs/rfcs/drafts/smooth-diff-touchpad-panning/RFC-DRAFT-0012-smooth-diff-touchpad-panning.md.`
Mandate revision: `b9a019b63aee9ddcf27e3381564b95f0aa78cef4173b57ce880d216d075eee4d`.
RFC base: `268b87049592ca4643e5303ab77114e116e7b5f3` on `main`.

## Current result

Candidate `d97c8d105dcb57bbff49a1a5d948abf7f4012417` makes a clipped keyboard target meet the actual displayed bottom (or top for Up) within one device pixel. The owner's v003 rejection explicitly requires this. It supersedes rounding every reveal to a top source-line boundary: a nonintegral viewport height needs a fractional origin to meet the bottom exactly. Consecutive arrows still move one source row, repeat and Shift remain discrete, and an in-view cursor move stays put. Page destinations align to source boundaries and Home/End retain exact endpoints.

Two mechanisms caused the rejected result: extra source-boundary rounding left slack, and `visibleRegion()` included overscan behind the native horizontal scrollbar. Captures now intersect the native viewport mask. The regression independently requires the mapped source bottom to equal scrollbar top minus one, then checks exact selected-row edge geometry after real key events. It exercises both split panes, a 4:7 replacement, unified view, repeat, alternating Shift, in-view stability, shared ribbon geometry and resumed one-pixel touchpad motion. The accepted adaptive axis discriminator and fractional-placement code are unchanged.

All seven affected checks pass on the exact frozen candidate at 100%, 125% and 200%, including architecture, in 48.23 seconds. Format22, both tidy units and the documentation gate pass. [m4-verification.json](m4-verification.json) and the [ledger](../../../verification/RFC-BPR-0073.md) retain the initial failing exact-flush control, unsuccessful raw-origin-only trial and actual clipping diagnosis. A stricter main-lane negative rerun failed compilation on unrelated concurrent dirty header/source-navigation files; it is not counted as regression proof. Candidate bytes were restored. Exact-candidate integration instead used the frozen build's generated CTest registrations in a writable external control tree, changing only render output paths. Source, executable, fixture, assertions, scales and network isolation remain frozen.

The required frontend-design skill was loaded from `/Users/jaeger/.claude/plugins/marketplaces/claude-plugins-official/plugins/frontend-design/skills/frontend-design/SKILL.md`. The native typography, palette and surface are retained. The plan's grouping, scope and exits remain unchanged; live RFC/semantic documents explicitly bind receiving-presentation alignment to the latest edge-flush amendment. Historical source packets and acceptance stay intact.

**AC07 has actual owner acceptance on v002. Only the separate AC08 exact-flush keyboard judgment remains pending.** The RFC/card remain open; no later milestone exists. Controller evidence and 67 documentation verifier tests reuse unchanged [M2 evidence](m2-verification.json). Historical [M1](m1-verification.json), [M2](m2-verification.json) and [M3](m3-verification.json) results retain their original scope.

## Review ready

Frozen root: `/Users/jaeger/bpr-1413-smooth-panning-v004.5_4cr5ir`. Source and build are inside it, exported from `d97c8d105dcb57bbff49a1a5d948abf7f4012417` and read-only. Official freeze and launcher's `--check` passed, reaching `ATTENDED_FILE_SHOWN:src/ui/conversation_cards_view.cpp`, fixture head `89df5e592e5db97a526a05a445735733395e4832` (`inline-drawer-v1`). Executable SHA-256: `9245818af6c3d45a5bd9f382263b8cd1239b248e5b3ad4617fa603d142de6a1f`.

```sh
/opt/homebrew/Frameworks/Python.framework/Versions/3.14/bin/python3.14 /Users/jaeger/bpr-1413-smooth-panning-v004.5_4cr5ir/launch.py
```

Fresh session: `a300291de66a8fb8963c182932d117d4`; saved event cursor **3**. One ready/protected candidate, no decision:

| Candidate | Fingerprint | One Accept/Reject question |
|---|---|---|
| BPR-1413-AC08-v004 | `3b0c1d5a789dfb53ca620e2415ea0c393005d800caeb86f9b7f93cc6f5312642` | After Down scrolls, is the bottom of the selected row flush with the bottom of the source view? |

[attended-manifest-v004.json](attended-manifest-v004.json) binds source, executable and protected paths. State: `/Users/jaeger/.mask/attended-review-sessions/bpr-1413-smooth-panning-v004-20261012`. Registry: `/Users/jaeger/.mask/attended-review-resources`. Full token-bearing URL is private in `handoff.json` and returned to the coordinator. `offline-review.html` is the fallback; import its export before treating offline decisions as authoritative.

Handed-off server: PID **93561**, command session **32588**, port **58371**, loopback only. All build, test, tidy, freeze and launcher-check commands finished. Only the review server remains. Repository writer ownership is released; no app, recorder, waiter, poller or other task command remains.

## Reconciliation and continuation

The v003 durable human rejection was consumed at cursor4: session `12930e4afcde5660e890f36745df8f70`, candidate `BPR-1413-AC08-v003`, fingerprint `d0fbf577e3862a33df5b65614549a83ac12b3e98474ab00de7e3afcc69428ec2`, decision `8751a593-c7dc-45cd-a5a1-92a32375a048`: “The bottom of the selected row should be flush with the bottom of the view area.” The user explicitly handed it back in chat; the session's hand-back field was null. Both screenshot attachments were inspected privately. Relevant protection was released at cursor5; the exact state helper stopped PID46999. The v003 executable hash was verified before official removal of its task-owned root. Original screenshots, decisions and intake remain private.

AC07's retained accept is session `c595e5c955c2cd527f9f4a351d3ab175`, candidate `BPR-1413-AC07-v002`, fingerprint `def1bc09d1cf931783cd1059720d1593954933c7b1ff317a314e0afb75e0234b`, decision `0a0ba98a-3e76-4cb8-b404-fb0196dbd48c`, source `59a2fa10c147fd1dc976eb5eef161137da5077fe`. V002 keyboard reject and earlier v001 rejection, their released protections and removed roots remain in [checkpoint.json](checkpoint.json). The stopped scoped Sublime wizard recorded 5,596 precise events and 4,113 numeric viewport samples; original traces stay private, replay equivalence is unproven, and all diagnostic processes are stopped. No repeat touchpad judgment is required by this keyboard-only correction.

The [current binding](m4-binding.json) is renewed after live upkeep/card parking; earlier bindings preserve provenance. Final tracker HEAD, observation and check are private in the v004 state to avoid recursive identities.

Automatic wait is unqualified. After **Finish and hand back**, the owner sends **Resume the attended review**. The coordinator owns intake and any future wait. Read only new events, verify session/fingerprint/source/executable, then persist the consumed cursor before a fresh engineering leaf. AC08 accept permits closeout using retained AC07 acceptance; rejected/unanswered AC08 stays pending. Query/release only relevant protections before another source change.

```sh
/opt/homebrew/Frameworks/Python.framework/Versions/3.14/bin/python3.14 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview --state /Users/jaeger/.mask/attended-review-sessions/bpr-1413-smooth-panning-v004-20261012 --registry /Users/jaeger/.mask/attended-review-resources status --after 3
```

Preserve unrelated `.gitignore`, `.mask.json`, `.mask.lock.json`, untracked drafts and concurrent header/source-navigation work. No stash, push, desktop capture, personal browser-data read, unrelated process stop or external-task message was used.
