# RFC-BPR-0076 verification

Mandate `file-change-navigation-r1`; milestone
`file-change-navigation-v1/review-navigation-entry`; immutable RFC_BASE_SHA
`d9c93c2dbfe29c5fcd6e11b21df4be0a0aead5f1`. The
[live import](../rfcs/assets/file-change-navigation-v1.LIVE_IMPORT.md) retains
the authored draft and amendment; all five criteria remain unchanged.
Product `40639f2f606aec92aaf8f651f4e72735c8429357` and final sibling-reading
assertion `aa3d71fe470627e1e78488eb51a9114a788f33bd` are integrated on main.
The exact verified candidate is `aa3d71fe470627e1e78488eb51a9114a788f33bd`.

The implementer loaded the required `frontend-design` skill from the installed
plugin before UX work. Existing native controls, styles, labels and shortcuts
remain the design basis; the older base/base offscreen source render was inspected.

## Result

The four retained file/change QActions enter Review through its ordinary drawer
transition. The composition restores the originating active tab's exact file and
pair before Qt evaluates the command and applies its ordinary step or bound.
One continuation binds accepted PR authority, generation, tab and exact view;
it is consumed once or cancelled by a later replacement gesture. Empty or
unavailable Review retains the ordinary explanation and visible focus fallback.

| Criterion | Direct evidence | Result |
|---|---|---|
| AC01 | The composed Git/cache review course invokes all four retained QActions from Overview, Discuss and Threads through toolbar clicks, View menu activation and admitted physical key events; each of 36 entries reveals Review and performs its ordinary step once. | pass on `aa3d71fe4` (macOS/offscreen) |
| AC02 | The composed course covers first/last file and change, no-open-comparison, no-changes and no-tab first-file entry; bounds retain exact source/reading position and the existing three-second message, while inactive actions remain disabled. | pass on `aa3d71fe4` (macOS/offscreen) |
| AC03 | The exact older/reverse active-tab pair is restored before navigation. Delayed file/change entry runs once; unavailable material and later command/tab/file/comparison/Home/composer gestures cancel without retargeting. The final sibling assertion restores its unchanged semantic reading position. | pass on `aa3d71fe4` (macOS/offscreen) |
| AC04 | Hidden and remembered Review drawers restore on entry and retain later changes inside Review. The composed summary draft survives the round trip; the full Qt window course verifies discussion selection, reading, filters, replies and existing draft retention. | pass on `aa3d71fe4` (macOS/offscreen) |
| AC05 | Physical key entry focuses visible source after exact asynchronous restoration or the originating visible arrow for empty/unavailable Review. Editable multi-line keys retain their native text/caret behavior; Home, local-cycle and composer behavior retain existing checks. | pass on `aa3d71fe4` (macOS/offscreen) |

## Commands and controls

- [Exact integrated pass](../issues/_checkpoints/file-change-navigation-v1/m1-integrated-checks.json): `application_review_navigation_entry`, `application_header_source_navigation`, `qt_workbench_window_scale_100`, `architecture_boundaries` and `test_network_isolation_manifest` all pass at `aa3d71fe4` (5/5, 74.815 seconds).
- [Original verification](../issues/_checkpoints/file-change-navigation-v1/m1-verification.json): focused composition build, full navigation course, full Qt window course, diff/shared-navigation checks, architecture contract, network manifest, clang-format 22.1.8 and documentation checks pass. This earlier source evidence is retained alongside the final integrated pass; it is not presented as an additional final-candidate run.
- The same record retains the essential-wire negative control: removing only the composition's Review-navigation dispatch binding fails the delayed reverse-pair change assertion with child exit -6. The exact source was restored before the passing candidate was built. An unnecessary extra epoch branch was excluded after its removal control passed.
- [Baseline review](../issues/_checkpoints/file-change-navigation-v1/m1-baseline-review.json) binds the scoped final diff to the immutable baseline, accounts for independently landed title-row edits, and preserves terminal BPR-0924, BPR-1132, BPR-1295 and BPR-1328.
- Closeout documentation commands and results are retained in [gate receipts](../issues/_checkpoints/file-change-navigation-v1/m1-documentation.json) and [verifier-test receipt](../issues/_checkpoints/file-change-navigation-v1/m1-documentation-tests.json).

## Quality and evidence limits

The new window, resident-menu and navigation-course code passes focused tidy.
Full composition and broad Qt test translation units retain 14 and 6 diagnostics
respectively, all confirmed unchanged at RFC_BASE_SHA in
[m1-tidy-limits.json](../issues/_checkpoints/file-change-navigation-v1/m1-tidy-limits.json).
A broad tidy-green verdict is not claimed. Source formatting and whitespace pass.

The evidence uses real Qt actions, physical window-system key events, toolbar
clicks and rendered composition on macOS arm64, Qt 6.11.2, offscreen at 100% scale
and 1280 × 860. It does not qualify native platform appearance, an installed
package or Windows/Linux native behavior. The registered Windows course retains
its existing uncontained DOS-volume-name restriction and is unmeasured here.
The RFC has no required human/native/independent criterion; none was inferred,
waived or substituted. No attended session is required.

## Documentation and upkeep

The [workbench specification](../ux/workbench-specification.md) owns bounded
Review entry, remembered drawers, focus and retained reading/drafts. The
[system architecture](../architecture/system-architecture.md) owns exact-tab
restoration and cancellation. The narrow Qt dispatch API and composition
lifecycle are covered there; the public-path test registration and network
manifest cover build/test impact. No provider/authentication protocol,
credential/publication authority, persistence format, package or launch policy
changed, so those semantic categories need no further document update.

Newer main bookkeeping imports do not change committed navigation product/test
inputs. Concurrent uncommitted C++ work is outside this accepted candidate and
is preserved. RFC, index, card, acceptance and the durable
[continuation](../issues/_checkpoints/file-change-navigation-v1/continuation.md)
agree on the exact verified product. No live mutation, desktop capture, browser
personal data access, release or push occurred.
