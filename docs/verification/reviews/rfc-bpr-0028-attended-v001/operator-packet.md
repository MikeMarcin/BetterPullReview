# RFC-BPR-0028 attended session, v001

Status: **prepared; three decisions pending**.

Three parked cards wait on one run of the signed bundle. Nothing here
changes anything on GitHub: the pull request is opened signed out, which
reads and never writes.

- Bundle: `build/app-release/src/app/BetterPullReview.app`, executable
  sha256 `a066705f24e9ea527e21f1eba4ee765ca90eeedc8b8a76747a01235789622b76`
  as recorded in [session-identity.json](session-identity.json), built at
  8ba0cb52 with tests disabled.
- Review page: [offline-package/index.html](offline-package/index.html), or
  ask for the live URL. Each candidate offers the command that opens the
  build and the pull request link, both with Copy buttons. The page keeps
  your note, takes a pasted screenshot, lets you change or clear an answer,
  and has "Finish and hand back" when you are done.

## The run

Open the build with the command on the page, or:

    open build/app-release/src/app/BetterPullReview.app

1. Quit the BetterPullReview already running. It is an older build.
2. Stay signed out. On Home press "Read without signing in".
3. Copy the pull request link from the page and paste it into the bar above
   the queue, then press Open.
4. Wait for the pull request, then let the review history settle.
5. Look at the window at its default size and again at about 1280x820.
6. Afterwards, if the history fails again, read
   `~/Library/Application Support/BetterPullReview/BetterPullReview/logs/diagnostics.log`.

## What each candidate asks

- **BPR-0646, workbench chrome.** The strip is the title row beside the
  traffic lights with the epoch line beneath and nothing else above the
  comparison; a handle on the epoch line changes the comparison; one drawer
  collapsed to its rail carries a vertical label and an unread count; signed
  out, sign-in is reachable without scrolling.
- **BPR-0662, drawer widths.** The drawers are at usable widths, they do not
  move on their own when the window narrows and widens, and a width you drag
  survives a trip to Home.
- **BPR-0663, public preview.** The epoch line shows the updates rather than
  "No updates loaded.", and the comparison resolves to an exact pair.

## What changed since the rejected session

- The review history failed because the acquire refused any pull request
  whose base branch had moved, which is nearly all of them. Fixed.
- Home takes a pasted link and has an Open button; the signed-out path is
  on Home as "Read without signing in" rather than buried in Settings as
  "Public preview".
- The pull request summary has left the files drawer for the overview.
- A failed comparison names what is open and lists the files instead of
  claiming nothing is open.
- The page itself keeps notes, takes images, and lets an answer be changed
  or cleared.
