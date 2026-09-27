# Source-line comments

Select a current/right line in a text file already changed by the pull request,
open the ordinary comment editor, enter prose and choose Publish. Unchanged
lines use the same gesture. The app checks the exact current head, retained
file/line and acting account again before posting. Left-side off-hunk and
arbitrary historical publication are outside this feature.

Native publication is attempted when support is unknown. Current production
has no admitted unsupported-response contract, so a rejection does not trigger
automatic file fallback. A timeout or uncertain result also never starts a
second POST. Keep the draft and report the typed result; a new representation
is not a retry policy.

When an approved contract makes file fallback available, the public body starts
with an immutable code permalink, a blank line and the original prose. Markdown
source links include `?plain=1`. In BetterPullReview a validated link places the
conversation at its exact source line. Collapsed Support details exposes its
honest GitHub file subject. Line-only suggestions cannot be silently degraded.

GitHub owns comments, author identity, replies, edits and resolution. Removing
or corrupting the root link removes semantic placement; the comment remains
file-level. A fresh database can reconstruct a valid link once exact source
material is available. Uninstall does not remove public comments or links.
Fixture evidence establishes reconstruction, not a completed live uninstall
or collaborator judgment.

The [RFC-0022 ledger](../verification/RFC-BPR-0022.md) tracks remaining live
GitHub.com acceptance. Native GHES behavior is unobserved under the owner's
GitHub.com-only amendment. Implementation and milestone integration do not
mean the RFC has met canonical closeout order.

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
