# RFC-BPR-0028 attended session, v001

Status: **prepared; three decisions pending**.

Three parked cards wait on one run of the signed bundle. Nothing here changes
anything on GitHub: the pull request is opened signed out through Public
preview, which reads and never writes.

- Bundle: `build/app-release/src/app/BetterPullReview.app`, executable
  sha256 `9e8af29a9bf43e5114b16e0fd3e1f2c62a0b0a12444bd91c97a321cb9faafc94`,
  built at f492706b with tests disabled.
- Review page: [offline-package/index.html](offline-package/index.html). Open
  it in a browser, decide each candidate, then use Copy JSON or Download JSON.
- Identity: [session-identity.json](session-identity.json) binds the bundle,
  the page and the three candidate fingerprints.

## The run

1. Quit the BetterPullReview already running. It is the older bundle.
2. Open the bundle above.
3. Stay signed out. From Home choose Public preview and open
   `https://github.com/microsoft/terminal/pull/20607`.
4. Wait for the pull request, then let the review history settle.
5. Look at the window at its default size and again at about 1280x820.
6. Afterwards, read
   `~/Library/Application Support/BetterPullReview/logs/diagnostics.log`.

## What each candidate asks

- **BPR-0646, workbench chrome.** The strip is the title row beside the
  traffic lights with the epoch line beneath and nothing else above the
  comparison; the epoch line draws a node per upload and a handle changes the
  comparison; one drawer collapsed to its rail shows a vertical label and an
  unread count while the other stays open; signed out, the strip's account
  button reaches sign-in without scrolling.
- **BPR-0662, drawer widths.** The drawers are at usable widths, they do not
  move on their own when the window is narrowed and widened again, and a width
  you drag survives a trip to Home and back. The persisted layout file is
  currently empty, so this run starts at the design widths rather than at the
  64-pixel widths the defect wrote.
- **BPR-0663, public preview epoch.** The epoch line shows the updates rather
  than "No updates loaded.", the comparison resolves to an exact pair, and the
  diagnostics log records `epoch_stage=smart_acquire` succeeding with no
  `failure=invalid_request`.

## After the decisions

Hand the exported JSON back and it is imported into the session; the three
cards are then closed or reopened from what you decided. A rejection is as
useful as an acceptance: it names the next card.

## What was verified without you

Every automated criterion on the three cards passes at this head, including
the focused Qt suites at all three scales, the composition and round-trip
contracts with both application digests repinned, the six anonymous
public-preview courses, the native git smart transport contract with its new
seam pin, and both UIforETW courses. What is owed is exactly what a person
has to look at.
