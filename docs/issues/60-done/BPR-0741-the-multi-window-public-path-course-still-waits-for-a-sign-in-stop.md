---
schema: "implement-rfc.card.v1"
id: "BPR-0741"
title: "The multi-window public path course still waits for a sign-in stop"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"found_at":"65c47ec6","found_by":"attended-prep-2026-09-10 d17"}
acceptance: [{"criterion":"AC1 application_profile_multi_window_public_path passes in ctest --preset dev on macOS, with the same windows, reads and restarts it pinned before","outcome":"pass","revision":"aab99d2b","evidence":{"kind":"command","detail":"Red reproduced first at b61e9c05: 1.33 s, 'FAILED: first review offers explicit sign in' (build/logs/bpr-0741-d23-head.log). After the change: ctest --preset dev --output-on-failure -R '^application_profile_multi_window_public_path$' Passed in 80.44 s and again in 78.37 s, against TIMEOUT 180 (build/logs/bpr-0741-d23-multiwindow.log, -rerun.log). Run on b61e9c05 plus exactly the one-file diff committed as aab99d2b; the only commit between, 4428f162, changes a packet link and no build input. Same pins, verified by the integrator independently of the slice owner: every expect and wait_for message was extracted from HEAD and from the change and diffed; one assertion ('first review offers explicit sign in') is replaced by four ('first public window lands signed out with no queue', 'Home sign-in reaches an account session', 'an account shows the queue', 'an account opens the first review without a sign-in stop'), and every other message is identical and in the same order. The windows (A; B from New Window; A closed with B surviving; C after A's context is retired), the reads (A->17, B->18, B->17, B->18, B's held refresh across A's close, C->18, C->17), the cross-window fanout and the draining Quit are unchanged. The course performs no process restart; the criterion's 'restarts' are those retirement, replacement and Quit steps, all still pinned. macOS only; Linux and Windows not exercised."}},{"criterion":"AC2 application_profile_window_review_restoration_public_path and application_profile_window_layout_public_path still pass","outcome":"pass","revision":"aab99d2b","evidence":{"kind":"command","detail":"ctest --preset dev --output-on-failure -R '^(application_profile_window_review_restoration_public_path|application_profile_window_layout_public_path)$' is 2 of 2 (build/logs/bpr-0741-d23-siblings.log): layout Passed in 24.28 s against TIMEOUT 500, restoration Passed in 585.93 s against TIMEOUT 1800. The restoration duration was checked rather than left as the slice owner's open uncertainty: it sits at a third of its budget, and earlier logs record it at 563.25 s and 565.78 s on 2026-09-11, so that is its normal runtime and not an effect of this change, which does not touch it. coding-format-check stays red only for the three files already red at HEAD (workbench_drawer.hpp, diff_runtime_ports.hpp, diff_application_host.cpp); this card's file is not in its list (bpr-0741-d23-format.log)."}}]
---

# The multi-window public path course still waits for a sign-in stop

`application_profile_multi_window_public_path` is red at HEAD, in one
second: `submit(*windowA, 17)` pastes the link signed out and the course
then asserts `homeSetupContinue` visible ("first review offers explicit
sign in",
`tests/end_to_end/profile_multi_window_public_path_tests.cpp:190-191`).
Since [BPR-0732](BPR-0732-a-signed-out-home-is-one-paste-field-that-opens-a-public-pull-request.md)
a pasted public link no longer stops at sign-in where preview is composed,
and on macOS `shipped_public_preview_capability()` composes it for every
github.com fixture course (`src/app/src/github_application.cpp:1965-1977`,
`:4654-4674`): the destination resolves to `public_preview`, the button is
hidden by design, and the read goes out anonymously. The `github_com`
fixture answers the anonymous read of pull 17 once and never pull 18
(`src/app/src/github_public_path_fixture.cpp:932`, `:1368-1374`), so the
course cannot pass on the anonymous path. Same cause as
[BPR-0740](BPR-0740-the-layout-public-path-no-longer-opens-the-exact-public-review.md),
whose course was converted to the account path in d17; this one is
outside that card's owned paths. On Linux and Windows the capability is
not shipped and the course still sees `sign_in`, so the red is macOS-only.

## Scope

- Convert the course to the account path the restoration course uses
  (`profile_window_review_restoration_public_path_tests.cpp:333-341`):
  sign in from the landing card's primary button, wait for
  `session_ready` and `!PublicPreviewSignedOut`, then paste; an account
  opens the destination without a stop, so the Continue assertion becomes
  "hidden". Keep every multi-window criterion the course pins.
- Owned paths: `tests/end_to_end/profile_multi_window_public_path_tests.cpp`
  only; the accepted BPR-0732 behaviour is not reopened.

## Acceptance

- `application_profile_multi_window_public_path` passes in `ctest --preset dev`
  on macOS, with the same windows, reads and restarts it pinned before.
- `application_profile_window_review_restoration_public_path` and
  `application_profile_window_layout_public_path` still pass.

## Converted to the account path, 2026-09-12 (d23)

**Reproduced first.** At `b61e9c05`, before any change,
`ctest --preset dev --output-on-failure -R '^application_profile_multi_window_public_path$'`
failed in 1.33 seconds with `FAILED: first review offers explicit sign in`,
exactly as this card describes (`build/logs/bpr-0741-d23-head.log`).

**The change is the one this card prescribes, in the one file it owns.**
`tests/end_to_end/profile_multi_window_public_path_tests.cpp`, +22/−2. Window A
now signs in from the landing card's primary button before its first paste,
waits for `session_ready` with `PublicPreviewSignedOut` false, and the
`homeSetupContinue` assertion becomes hidden rather than visible. It follows
the layout course converted under
[BPR-0740](BPR-0740-the-layout-public-path-no-longer-opens-the-exact-public-review.md):
only the first window signs in, and it signs in before its first paste,
because a sibling that signed in later would enrol a second grant whose
identity change retires every window's session.

**No multi-window criterion was dropped, and that was checked independently of
the slice owner rather than taken from its report.** Every `expect` and
`wait_for` message was extracted from `HEAD` and from the change and diffed.
One assertion — "first review offers explicit sign in" — is replaced by four:
"first public window lands signed out with no queue", "Home sign-in reaches an
account session", "an account shows the queue" and "an account opens the first
review without a sign-in stop". Every other message is identical and in the
same order. The slice owner counted 105 before and 108 after; the integrator's
extraction counted 108 and 111. The absolute totals differ only by how a
multi-line call's literals were collected, and both agree on the part that
matters: one removed, four added, the rest unchanged and unreordered. The
windows (A; B from New Window; A closed with B surviving; C after A's context
is retired), the reads (A→17, B→18, B→17, B→18, B's held refresh across A's
close, C→18, C→17), the cross-window fanout and the draining Quit are all still
pinned.

**Results.** The course passes in 80.44 s and again in 78.37 s against its 180 s
timeout (`bpr-0741-d23-multiwindow.log`, `-rerun.log`).
`application_profile_window_layout_public_path` passes in 24.28 s (timeout
500 s) and `application_profile_window_review_restoration_public_path` in
585.93 s (timeout **1800 s**) (`bpr-0741-d23-siblings.log`). The slice owner
flagged the restoration course's duration as unchecked against its timeout;
it was checked: it sits at a third of its budget, and it ran 563.25 s and
565.78 s on 2026-09-11 in earlier logs, so that is its normal runtime and not
an effect of this change, which does not touch it.
`coding-format-check` stays red only for the three files it was already red
for at HEAD — `workbench_drawer.hpp`, `diff_runtime_ports.hpp`,
`diff_application_host.cpp` — and this card's file is not in its list
(`bpr-0741-d23-format.log`).

Two things recorded rather than done. The unchanged message "first review opens
through public setup" now describes the account path; it was left as it is so
the pinned set stays byte-identical, and renaming it is cosmetic. And this ran
on macOS only; the red was macOS-only and the layout and restoration courses
already sign in from the landing card on every platform, but Linux and Windows
were not exercised here.
