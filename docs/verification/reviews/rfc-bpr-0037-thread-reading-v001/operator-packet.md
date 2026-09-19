# RFC-BPR-0037 thread-reading native review

Status: **ready for native review; three protected candidates are awaiting
explicit human verdicts**.

This queue contains the three remaining human judgments for the
`thread-reading-integration` milestone. The implementation revision is
`8939de7195c7de6c2dc4299c3f5e1f0d342bf115`; the exact development executable
has SHA-256 `5b8b23dcd7ed735c544b7bd78d81f1404b910b4cd09e9e610a787dacd15a362a`.

The two fixtures are local, recorded/synthetic and network-isolated. They do
not use a GitHub account, make a live GitHub/GHES request or publish anything.
Each candidate has one cohesive question and its own absolute launch command.
Quit the fixture instance between Overview and Discuss; Compose may reuse the
Discuss instance. Do not activate a publication control in the Compose item.

## Durable session

- Session: `c44d1fe1c8245aa53959afc9fc9f4b27`
- Sequence after preparation: `15`
- External state:
  `/Users/jaeger/.mask/attended-review/rfc-bpr-0037-thread-reading-v001`
- Protected bundle:
  `/Users/jaeger/Documents/GitHub/BetterPullReview/build/attended/rfc-bpr-0037-thread-reading-v001/better_pull_review.app`
- Offline queue:
  `/Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/reviews/rfc-bpr-0037-thread-reading-v001/offline-review.html`
- Manifest:
  `/Users/jaeger/Documents/GitHub/BetterPullReview/docs/verification/reviews/rfc-bpr-0037-thread-reading-v001/candidate-manifest.json`

Open the offline queue in a browser and work through any ready item. Accept or
reject only the question shown for that item. Partial work is valid: use Copy
JSON or Download JSON before closing, then give the exported JSON/file to the
integrator. Browser storage is not the durable record until that export is
imported into the session.

The import command is:

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/.agents/skills/attended-review/scripts/attended_review.py --repo-root /Users/jaeger/Documents/GitHub/BetterPullReview --state /Users/jaeger/.mask/attended-review/rfc-bpr-0037-thread-reading-v001 import --file /absolute/path/to/exported-decisions.json
```

After import, the integrator must reconcile each explicit verdict with its
card acceptance record. Preparation, observation or hand-back alone does not
approve a card. BPR-0924, BPR-0928 and BPR-0929 remain parked until every
applicable criterion is satisfied.
