# RFC-BPR-0013 current attended candidate

Status: **three current candidates ready and protected at sequence 404; owner
verdicts pending; no host or watch running**.

Open the [offline review page](offline-package/index.html), not the old dsp3
packet. It has the three ready questions and can export any partial decisions
using **Download JSON** or **Copy JSON**. Return that export for intake; browser
storage alone is not a durable verdict. Skip or stop whenever useful.

Source: `7b591faae6c1a746998ade8b49f87d88145fcf6b`.
Candidate: `rfc0013-r17-7b591faa-ad50fe5d`.
Session: `cbd48b91000b00f1e72e1ad62c7c384c`, sequence 404.

The bundle is
`/Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release-rfc0013-r17/src/app/BetterPullReview.app`.
Its executable SHA-256 is
`ad50fe5d3d12b2056ded5bbbee4ccd4a9289850fe8ffa05c051b5b0408ae0657`.
It is tests-disabled Release, stable Developer ID signed and deep-strict verified.

The old source omitted later CLI discovery, connected-session routing and
shared-sign-in fixes. This bundle's actual synthetic public UI course reached
CLI directly from signed-out Home, retained the account through a permission
refusal, and then opened a readable exact-pair diff without signing in again.
That is not evidence of the private target's content or anchors.

The HTML asks separately about the readable private-target diff, its prompt
loading, and the retained eight-hour device-session obligation. The device item
uses only an existing grant with known timing; skip it if those prerequisites
are absent. No sign-out, repeated device flow, global gh account change or live
write is requested. Notes must contain no private source, paths, IDs or secrets.

[The evidence identity](session-identity.json) records exact results and limits:
15 focused passes plus the dedicated loading pass, the portable integration
pass, and the remaining unrelated broad-Qt layout and stage-0 self-test failures.
No full-suite or RFC-closeout pass is claimed. There is no review server or watch;
this is a prepared offline handoff, not an active hosted session.
