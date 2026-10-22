# Source-line comments

Select a current/right line in a text file already changed by the pull request,
open the ordinary comment editor, enter prose and choose Publish. Unchanged
lines use the same gesture. The app checks the exact current head, retained
file/line and acting account again before posting. A base line off every hunk
is published as a file comment only when its document is the merge base's;
arbitrary historical publication is outside this feature.

Since the owner's 2026-10-22 decision ([[BPR-1735]], RFC-BPR-0022 amendment
r5) the core places the line first. Once the app has read the file at the merge
base GitHub names and at the head, a line more than three lines from every
change of that diff on its side is published straight as a file comment with a
permalink, with no native attempt; the editor says so before Publish, and Start
a review refuses the line. A comment already pending in a review on such a line
is held on Finish your review and on the comment, with Publish as file comment,
and the review waits for it. With no merge base named, or the file unreadable,
the line is published natively as before.

A native comment GitHub refuses with a fully received 4xx, once its complete
comment list holds none of it, is recorded rejected: the editor says it was not
published and why, the draft is kept, and Publish sends a new attempt. The
recorded rejection contract still permits fallback after a native attempt only
in the designated PR2 validation course with its exact source, account and
profile. A timeout or uncertain result never starts a second POST and stays
GET-only; only a comment an earlier version left unconfirmed settles, ten
minutes after the upgrade kept it, when GitHub lists none of it.

The file comment's public body starts
with an immutable code permalink, a blank line and the original prose. Markdown
source links include `?plain=1`. In BetterPullReview a validated link places the
conversation at its exact source line. Collapsed Support details exposes its
honest GitHub file subject. Line-only suggestions cannot be silently degraded.

GitHub owns comments, author identity, replies, edits and resolution. Removing
or corrupting the root link removes semantic placement; the comment remains
file-level. A fresh database can reconstruct a valid link once exact source
material is available. Uninstall does not remove public comments or links.
Fixture evidence establishes reconstruction. The owner accepted the designated
GitHub-only code/prose/author/reply course, and the signed app subsequently read
the external reply at the same semantic line. Actual desktop uninstall remains
unobserved.

The [RFC-0022 ledger](../verification/RFC-BPR-0022.md) records Auto-Verified
closeout under the owner's r4 amendments. App-closed GitHub continuity and
fresh-host reconstruction satisfy its amended AC-10 evidence; actual installed
uninstall remains unobserved. Owner acceptance substitutes for separate
independent review of the exact candidate. Native off-hunk success, native GHES
and Windows qualification remain unobserved.

## Diagnosing an unavailable publication

The profile log records the create-comment operation, native/file representation,
HTTP status when received, and typed transport failure. A complete bounded422
response records only recognized resource/field/code/message words, fixed public
GitHub documentation metadata, and counts of unknown members. Missing, null,
empty, non-string and unrecognized values remain distinct. Recognized message
variants retain exact spelling; they are never normalized into an authority
claim. Unknown service text, request prose, paths, private URLs, tokens and IP
addresses are omitted.
These diagnostic observations do not authorize fallback. The durable journal
still decides whether the attempt may only reconcile through GET; an empty GET
result does not prove non-creation. A log capture can also be unavailable or
truncated, so absent diagnostic data is not evidence that no request was sent.
