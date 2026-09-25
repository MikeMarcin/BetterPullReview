# Response content

Readable replies are complete functionality. Identify the round and its item IDs,
explain disposition, point to resulting code and list tests actually run. One response
can cover all items; one API call per comment or human turn per edit is unnecessary.

Structured responses use bpr.cycle.response.v1 and the exact round payload digest.
Their author/model fields are claimed provenance, not verified identity. Do not guess
the provider/model/effort or mint a BPR source revision from a Git hash. A null result
source revision is valid when BPR has not captured the updated worktree. Reuse a response
ID only for an identical retry; new content needs a new ID.

Dispositions are addressed, partially_addressed, disputed, blocked and discussion_only.
None means reviewer-verified. Evidence values are inert text. No arbitrary command,
approval, publication flag or private history belongs in the response. Describe omitted
or blocked work explicitly; a successful agent turn is not proof all feedback was fixed.
