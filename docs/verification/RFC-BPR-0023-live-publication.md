# RFC-BPR-0023 designated live publication

Observed 2026-09-27 UTC against the owner-designated safe repository
`MikeMarcin/BPR-Test`, pull request 1. This is one bounded source-line review
comment, not an approval, review submission, branch update, or release.

## Bound course

- PR database ID `4246446933`, node `PR_kwDOTyvof879G6NV`; open base
  `e2a44aa8fef704de7373463222b1872e796918c6`, head
  `17990ba0c2fb50338141f35c0717f0f99639c7ba`.
- Fresh tests-disabled Release capsule source digest
  `ffd472125cda85867b458f61d216c3ca34f75cfa978b74d5800abe73702a9cd7`,
  image digest `694b7d4b3a1276d13e2eedaa11cc80a98f423382ecc9eaadf7b4015df6bb00cb`.
  The one-shot, owner-private approval bound this exact build and PR/head. The
  first attended mount ended without mutation; a fresh approval and mount were
  used for the successful course. Neither approval file nor transient logs are
  committed.
- The separate opt-in `live_review_mutation_designated_target` CTest passed
  1/1 in 550.01 seconds. The signed application exited normally; the mount
  parent was empty afterward.

## App path and server readback

In the running app, the reviewer selected the current right-side line 1 of
`bpr-live-review-fixture.txt`, created a private reviewer turn, selected exact
revision 16, and opened `Squash to Draft`. The editor displayed one immutable
source, composition 18, the explicit right-side line 1 target at the bound
head, reviewer-authored mode, and the exact proposed prose. `Accept as Draft`
saved local draft 20; the draft editor then displayed a separate enabled
`Publish` action, actor MikeMarcin and the same PR/head. One Publish gesture
produced one new server comment. The preflight independent server list had
two review comments; the postflight list had three.

An independent `gh api repos/MikeMarcin/BPR-Test/pulls/comments/4116400082`
read returned:

| Field | Observed value |
|---|---|
| Comment ID / node | `4116400082` / `PRRC_kwDOTyvof871W0fS` |
| Author | `MikeMarcin` |
| Body | `This test change is ready for the private composition and publication check.` |
| Path / side / line | `bpr-live-review-fixture.txt` / `RIGHT` / `1` |
| Original and accepted commit | `17990ba0c2fb50338141f35c0717f0f99639c7ba` |
| Reply-to | null; this is a new root review comment |
| Permalink | https://github.com/MikeMarcin/BPR-Test/pull/1#discussion_r4116400082 |

The public body is exactly the reviewer-authored sentence, with no appended
private source, provenance marker or AI label. The app's returned conversation
rendered the same body. After the app exited, a read-only local SQLite query
found one `private_publication` row for scope 2: publication 21, draft 20,
content 19, with server node `PRRC_kwDOTyvof871W0fS`. The same scope retains
composition 18 with source revision 16 and draft 20 at revision 3. This is
the local-to-server join; the independently read node ID matches exactly.

The local crash, failed-mutation, deletion and read-only repair checks remain
the separately recorded deterministic evidence. This course supplies the
previously pending designated target and independent public body/actor read
for BPR-0584 and RFC AC-12/13/17. It does not claim native parity on other
platforms or release qualification.
