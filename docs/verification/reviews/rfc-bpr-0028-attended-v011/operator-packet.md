# RFC-BPR-0028 attended session, v011: epoch comparison handles

Status: **one candidate ready and protected at sequence 372; owner verdict pending; no host or watch running**.

The tests-disabled, locally signed bundle was built from `16e242a5` and is at
`/Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release-d28/src/app/BetterPullReview.app`.
Its executable sha256 is
`dfd1c4cae5739559b143138c28a0277bc9f5e7c40ef1556bee47d967cff2f187`;
`codesign --verify --deep --strict` passed. The build and page digests are in
[session-identity.json](session-identity.json). This isolated path leaves the
protected v009 and v010 packets and the v010 bundle unchanged.

The one candidate is `BPR-0646-workbench-chrome`, revision
`app-release-d28-dfd1c4ca-2026-09-12.7`, fingerprint
`b664183574e90c340b9717ad0fec1dddb6b6bbc50dc0379927e52e0ebfbf422d`.
It repeats **exactly the one previously rejected criterion**: whether dragging
an epoch-line bracket to another update of NVIDIA/stdexec#1846 changes the
comparison and whether the displayed update count is the pull request's.
[Open the offline review page](offline-package/index.html) to inspect the
candidate and save a verdict. The page is self-contained and retains partial
decisions; import its exported JSON through the attended-review CLI to record
an offline verdict. No owner approval is inferred from automated or driver
checks.

To inspect the running app, quit any running BetterPullReview and launch the
exact bundle:

```sh
open /Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release-d28/src/app/BetterPullReview.app
```

Paste `https://github.com/NVIDIA/stdexec/pull/1846` into Home and open it.
Sign-in is not required. Wait for “Review history is ready” on first open;
historical Git fetches can take time. The evidenced Aug 1 and Aug 9 force-push
events appear beside this install's live observation. Drag the right bracket
from the current node to the Aug 9 node and inspect the exact comparison in
Read. The app explicitly says the history is incomplete and ambiguous. GitHub's
anonymous REST timeline does not enumerate ordinary pushes, so the displayed
nodes are **evidenced updates, not a guaranteed complete upload count**. A
historical update's original base is unknown; the available comparison uses a
derived base. The owner should judge the unchanged criterion with those limits
visible, without treating them as an automatic pass or failure.

On this exact bundle, a live macOS public-preview window loaded that pull
request, showed three evidenced nodes, and a drag from current to Aug 9 settled
the exact pair as “Left available; right available; Ready for diff.” The local
account state in that window was not independently verified. The signed-out
composed fixture course passed separately through the real host, Git cache,
SQLite catalogue, core transition and Qt gesture
(`build/logs/bpr-0743-d28-release-guard-composed.log`). The local review
session has an objective observation of the live interaction, no human
decision.

The candidate is ready in session `cbd48b91000b00f1e72e1ad62c7c384c`.
For a live session, run:

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview serve
```

Then arm the driver watch with `wait --after 372`. No host was started while
preparing this packet. The isolated bundle and exact source paths stay
protected until the decision is consumed or development is explicitly
resumed. The owner accepted [[BPR-0729]] in v010; [[BPR-0735]] and [[BPR-0746]]
remain declined observations and are not asked again here. [[BPR-0743]] AC4
and [[BPR-0646]]'s attended criterion remain pending until the owner's answer.
No step in this packet writes to GitHub.
