# RFC-BPR-0013: usable review amendments, 2026-09-20

Assignment: `selected-diff-exact-epoch-pair`, mandate
`r20-usable-review-amendments`, `implement-rfc.engineering.v1`,
coordinator-implementer recipe revision 2. Sole engineering child:
`/root/rfc0013_postreview_r3`. No delegated work or live GitHub mutations.

## Result and exact candidate

The owner independently observed a rendered selected diff for
`EpicGames/UnrealEngine#1773` in repaired r21. That loading observation is
accepted; no repeat owner test is requested. The supplied screenshot was
inspected directly, but remains outside the checkout. Its SHA-256 is
`fdaa14887ac1966c75f7a5fec2b33bcafaf08125626a51d30d6024d40a83c7e3`.
This does not itself establish a source/anchor cross-check for the private file.

Product commits:

- `e21476cc`: apply persisted code font and tab stops to the composed workbench.
- `10e9716d`: semantic added/removed file-drawer ink and accessible operation text.
- `af4e2d38`: per-token contrast-safe intraline foregrounds.
- `ca000e79`: bounded provisional Overview before the complete snapshot.

The final native candidate is
`build/app-release-rfc0013-r22/src/app/BetterPullReview.app`, Release,
`BUILD_TESTING=OFF`, Developer ID signed, on macOS 26.6.2 arm64, Qt 6.10.3,
Apple Git 2.50.1. Its executable SHA-256 is
`859cc687765671f2a1ce0e3e314f9ffe7a92a93da3a184b69ae729ffa036e6b4`.
`codesign --verify --deep --strict` passes. No embedded fixture, self-test,
automatic consent, or credential override was used.

## Visual diagnoses and repairs

The prior intraline overlay replaced an entire edited span with monochrome
foreground when any syntax color lacked contrast; selection always replaced
it. Normal dark fills are now `#104a17` and `#672323`. They retain a distinct
edit surface while preserving readable syntax hues. Foreground-only token
intersections adjust only insufficiently contrasted ink. High-contrast themes
retain their stronger black/white treatment. Exact source, mapping, edit ranges,
and Earlier/Later non-color cues are unchanged. Color-vision audit obligations
explicitly retain those non-color cues; no contrast threshold was lowered.

The tab setting was stored and restored correctly. The concrete application
applied the interface font and colors but omitted `apply_code_font_to`, leaving
Qt's default 80-pixel tab stop. Standalone widget tests had called the missing
helper themselves. The repair applies the accepted profile to the actual
workbench. The public Settings course checks default 4, gestures 2/8/4, and
recreated-application persistence across source, detail, and unified documents.
Rendered cursor geometry checks one, two, and three literal tabs against the
selected width and current code-font space advance, without rewriting source.

The file delegate had only neutral ink. A typed operation role now selects the
same accepted/defect semantic colors as Overview for added/removed files.
Modified files stay neutral. Native highlighted text is retained for selected
rows. Operation words remain accessible, so color is not the only cue.
Dark, light, and both high-contrast themes are covered.
The public application course also paints safe added/removed model rows through
the **actual composed delegate** after each Settings mode change. It never
manually applies a profile to that delegate, so a missing composition handoff
cannot be hidden by the standalone widget fixture.
Disconnecting that concrete application handoff fails the new pixel check;
restoring it passes. This is separate from the earlier standalone delegate test.

Safe rendered evidence, directly inspected:

- [Dark intraline macro/string and selection](rfc0013-r22-rendered/dark-intraline.png).
- [Dark drawer](rfc0013-r22-rendered/file-drawer-dark.png) and
  [light drawer](rfc0013-r22-rendered/file-drawer-light.png).
- [Composed provisional Overview](rfc0013-r22-rendered/progressive-overview.png).

## Latency: connection is not the pull-request read

The read-only live probe used the production provider, CLI transport, bounded
macOS process runner, configured active account, and unchanged pre/post identity
checks. It emitted fixed stage/operation/count/duration/success fields only.
Account comparison and private response content stayed in memory. It performed
no Git acquisition, so its terminal time is explicitly provider acquisition,
not selected-diff readiness.

| Stage | Measured duration |
|---|---:|
| Direct `gh auth status`, output discarded | 0.38 s |
| Bounded discovery version / auth status | 0.216 / 0.396 s |
| Transport construction plus live viewer setup | 0.545 s |
| Nine endpoint-specific capability probes | 13.078 s |
| Repository + PR metadata, first two read responses | 3.920 s |
| Complete PR read, 48 response pages | 85.959 s |

Viewer setup includes a 0.138-second version check; do not add it twice.
Capability probe operations themselves took 0.525–0.916 seconds each. Eighteen
before/after `/user` checks account for roughly 7.1 seconds of the capability
stage. `gh auth status` is not the application's live endpoint-capability and
account-continuity protocol. Discovery plus viewer plus capabilities total
about 14.24 seconds, excluding the user decision and later PR read.

No demonstrated duplicate can safely be removed without changing the current
identity/executable/capability boundary. The second version check occurs at
transport construction after discovery/consent; the identity brackets detect
external account changes around each bounded operation. They remain intact.
The improvement here is useful asynchronous presentation, not a claim that
connection validation or Git acquisition became faster.

The 48 pages are intentional complete-snapshot acquisition: 16 REST file pages,
16 GraphQL viewed-state/thread pages, three commit pages, six timeline pages,
and metadata/reviews/comments/checks/search. They are neither sparse Git tree
traversal nor an accidental catalogue loop. The old UI waited for the entire
snapshot, making already acquired useful metadata invisible. The prior
76.24-second sample and this 85.959-second sample show network variation, not
an 85-second GUI-thread block. The website's reported seven seconds is an owner
comparison, not a controlled benchmark here.

## Progressive-publication contract

The provider emits one independently bounded metadata observation after the
repository and PR responses. It does not promote a partial snapshot. The host
maps validated repository/PR relationships into a provider-free, read-only
projection on the serialized decision lane. The core admits it only once for
the exact pending window/session/request. The queued callback is capped at one,
even if a provider repeats it 1,000 times. Foreign, stale, cancelled, retired,
post-terminal, malformed, oversized and relationship-mismatched observations
cannot grant authority or revive old content.

Qt shows title, author, branch names, reported counts, description and observed
open/closed/merged/draft state, explicitly marked provisional. Unknown review
metrics remain unknown; the file model is empty and Start review is disabled.
Complete final validation, atomic snapshot publication, pagination limits,
epoch acquisition and mutation admission remain unchanged. Failed/cancelled
initial reads clear the provisional content; refresh retains prior accepted
content rather than replacing it with a weaker observation.

The real composition test holds the first files-page response. It observes a
painted useful Overview while provider completion and all Git acquisition remain
held, then releases the gate and observes normal terminal promotion. The final
captured local sample was **48 ms first useful paint / 63 ms terminal after release**;
the uncaptured final sample was 10/26 ms. These are deterministic recorded transport
measurements, not GitHub network promises. On the live target, metadata was
available 82.039 seconds before terminal read; native first-paint latency for
that private target was not separately timed.

Disconnecting the production Qt publication makes the new public course fail
at “paints useful metadata while the first file page is still held” (25.46 s).
Restoring it passes. The original missing composed tab application and original
monochrome intraline behavior likewise fail their new regressions before repair.

## Native tests-disabled end-to-end proof

The external safe `large-positive` CLI course in
`tests/end_to_end/rfc0015_cli_read_fixture.py` supplied 515 synthetic files,
32 revisions, sparse special Git files, and an actual local Git repository.
Only its CLI endpoint boundary is synthetic. Consent, production account
wrappers, provider pagination, sandboxed installed Git, cache, exact epoch pair,
selected source, syntax and Qt rendering are the shipped composition.

Final app PID 82469 was independently driven through normal CLI consent and
`https://github.com/octo-org/review-repository/pull/17`. Native screenshot/AX
inspection first showed useful provisional metadata with 13 pages received,
no accepted file rows and a disabled review entry. It then showed 515 files,
pair ready, file 4 selected, “Diff ready — 1 semantic changes”, and the complete
eight-line `src/file_0000.cpp`, with earlier `0` and later `2` at the first line.
Review details exposed the exact left/right anchors:

- Safe fixture base: `475b9b6c25dbcbd1d3f9cf559b4cc8ab6a78f986`.
- Safe fixture head: `ab75a4be6969ce0750b56ec9ae594a10ac809e9c`.

Both match `git rev-parse` of the fixture refs, and both rendered source panes
match `git show` at those refs. The selected-change detail matches the same
replacement. The normal quit was reaped with exit 0. An earlier run expired
while inspection was unfinished and its launcher reported a shutdown-bound
failure; no render success was inferred from that exit or from the launcher.

The real-target loading observation is the owner's r21 evidence. The independent
exact source/anchor check above is the safe target through the identical
composition; it does not silently close BPR-0794's private-target cross-check.
No private screenshot, source, response, path, object ID or credential was
copied into this record or the committed rasters.

## Verification and remaining boundary

- Final progressive core/host/pager/provider/public composition: 5/5, 54.41 s.
- Final public composition including Settings-driven drawer ink: 1/1, 40.98 s;
  its missing-handoff negative fails at the actual composed delegate's pixels.
- Visual focused pass: 7/7, 47.21 s, including 1x/1.25x/2x diff rendering.
- CLI transport and smart-Git transport retain identity/cancellation/refusal tests.
- `qt_composed_loading_scale_100` passes. The broader
  `qt_composed_github_scale_100` remains non-green and is tracked as BPR-0940.
  Test-only upkeep repairs retired all-root conversation, pre-0795 Home loading,
  shared-sign-in destination, and unanchored inline-action expectations. The
  next reached failure concerns CLI-card setup-mode wording/actions; no product
  behavior was changed to silence it, and no full-suite pass is claimed.
- Core-only integration: 143/146 passed. The three failures enumerate the
  existing ignored `.claude/worktrees/agent-a17cae88115dbbb4e` as duplicate source.
  A tracked-source materialization, configured separately with the same cached
  dependencies, passes all three unchanged gates: architecture boundaries,
  isolation structure contract and isolation manifest (final 3/3, 6.78 s). The user
  worktree was neither changed nor moved.
- Focused clang-tidy and changed-source formatting pass. Self-review used the
  code-simplifier discipline to keep the optional observation path narrow,
  preserve old port fallback behavior, and avoid general streaming machinery.

BPR-0934 folder collapse remains separate. The RFC and original selected-diff
milestone are not closed by these bounded amendments. No new attended candidate
or review server was prepared; session `cbd48b91000b00f1e72e1ad62c7c384c` remains
at sequence 443 with the old rejection/hand-back intact.
