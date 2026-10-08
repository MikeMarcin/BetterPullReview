# Prepare and maintain a review tour

A review tour explains a change through a short Problem, a Solution, and an
ordered set of source comments. Start with **Prepare review tour** in Overview
or the Pull Request menu (`Alt+T`). It opens the Tour page beside Comments and
Notes. Preparation, preview, navigation and local saving work without AI.

The local authoring and fixture publication paths are implemented. Production
Tour writes currently remain unavailable because their exact GitHub.com/GHES
operations have not been qualified. Existing ordinary review commands retain
their own support. The [qualification course](../verification/RFC-BPR-0039-0043.manual-course.md)
and [evidence ledger](../verification/RFC-BPR-0039-0043.md) record the outstanding
server, other-platform and installed-provider/model checks. The seven prepared
Mac native judgments are accepted on their recorded candidates. Do not interpret
an available ordinary comment action as support for a pending tour batch.

RFC-BPR-0039–0043 close only the verified implementation under the owner's
[r2 amendment](../verification/RFC-BPR-0039-0043.owner-closeout-r2.md).
Standalone Blocked BPR-1222–1226 retain all unobserved qualification and original
predecessor acceptance. The eleven live-operation rows remain unavailable;
actual model quality and installed-provider restrictions remain unqualified.

## Write the brief and route

1. Write what was wrong, missing or difficult in **Problem**, then describe the
   central approach and limits in **Solution**. A sentence can be enough. Keep
   durable contracts in source/documentation and change-specific explanation here.
2. Select actual code lines and use **Add tour stop** (`Alt+Shift+T`). Give the
   stop a meaningful title and explain a decision, invariant, tradeoff, uncertainty
   or point that needs scrutiny. The new stop is inserted after the current stop
   and selected; Redo restores that selection. Related source links can supply context.
3. Right-click a stop to **Insert after**, **Duplicate**, **Merge**, replace its
   source or **Remove** it. Shift-click selects a range; Command-click on macOS
   (Control-click elsewhere) selects individual stops. Merge requires two or more
   adjacent stops and combines their explanation bytes in route order, retaining
   related sources. Drag the dotted handles to reorder selected stops; the
   keyboard alternative is `Alt+Shift+Up/Down`. With the outline focused,
   `Cmd+D` (`Ctrl+D` elsewhere) duplicates and Delete removes the selection.
   These actions each have one structural Undo step.
4. Use **Ready and next** (`Alt+Return`) after reading the current explanation
   against its source. This is private preparation state. It does not mark code
   reviewed, resolve a thread or approve a change. Previous, Next and Return to
   tour let you explore code and resume the route.
5. Inspect the brief, explanations and exact source links in Preview. The native
   Markdown reader is read-only and has HTML/disclosure limitations. Raw Markdown
   remains the publication source; preview does not promise GitHub pixel parity.

Each stop has a small status icon before its number: a pencil for Editing, a
circled check for Ready, a sparkle for Suggested / unvisited, and a warning
triangle for Needs attention. The dotted drag handle is separate. Hover a row for
its full status; assistive technology receives the number, title and status.

Local saves are automatic and profile/account scoped. A failed save says that
text is not yet durable; keep the editor open and use the offered retry/recovery.
Conflicting windows retain both versions. Tab and Shift+Tab move between controls
without inserting tabs into prose. The focused control scrolls into view and has
a visible focus outline. Standard Undo/Redo (`Cmd+Z` / `Cmd+Shift+Z`
on macOS) reverses the latest arrangement while its history is available,
including adding a stop or dragging it. Subsequent text editing clears that
bounded structural history and gives Undo/Redo to the native text editor, so
restoring an older arrangement cannot erase newer prose. Undo and Redo work
through the normal shortcuts throughout the Tour panel.
Undo after publication changes only the local draft.

The local ceiling is 256 stops, 64 KiB UTF-8 per field, 32 related sources per
stop and 4 MiB per encoded draft including baselines and metadata. There are at
most 128 saved drafts per profile, with no automatic eviction. A refused oversized
edit remains available for shortening, copying or Undo. Provider limits are
smaller: a pending line batch permits at most 100 comments and 64 KiB of encoded
request data, including JSON overhead. A locally valid tour can therefore be too
large to publish as one review. The application does not silently truncate it or
split a promised single submission.

## Publish only a qualified plan

On an exactly qualified host/transport, Prepare publication shows the acting
account, current head, each source/body, proposed remote operations and separate
optional description edit. **Publish** authorizes only that captured finite plan.
Native supported line/range stops are staged pending, bound to returned comment
IDs, wired and verified before one explicit COMMENT submission. Reviewers receive
ordinary comments and a Start here index with Previous/Next/overview links; they
need no BPR installation, hidden marker or special syntax to read and reply.

Historical, unchanged or unsupported source subjects may require normal file or
PR conversation comments with exact commit-pinned links. The preview must state
that compatibility publication has several visible writes. Pending file-thread
support is a separate qualification from standalone file comments. Unknown,
denied or ambiguous outcomes do not authorize switching representation.

A description edit changes only the body and preserves unrelated template
content. It updates mapped Problem/Solution sections, or previews inserting both
at the beginning when neither exists. Partial or duplicate sections require
mapping before publication. It is separate from review submission; failure can leave a published
tour with a description update still needing attention. Opening or publishing a
tour does not make a draft PR ready, assign reviewers, merge or close it.

## Reopen, update and recover

Reopening reads current GitHub content. To recover from a fresh local profile,
select an available root explicitly in the import choices. Its index supplies
order and returned links supply comment identities. Custom or ambiguous Markdown
needs explicit mapping; unknown private preparation/AI history stays unknown.
Merely importing or previewing sends no normalization edits.

Website edits are compared with the last observed baseline and your local text.
Overlapping changes show **Baseline**, **Local** and **Website** versions; resolve
them explicitly. Unrelated description/template bytes are retained. A missing
page or network failure leaves stale text visible and does not prove deletion.
Preflight reads and reconciliation cannot eliminate a simultaneous website edit;
these endpoints have no evidenced conditional body-write guarantee.

Reorder updates affected navigation and the index while keeping comment IDs and
replies. Insertions create new destinations before updating the root. Removing a
stop changes membership and keeps the original discussion. Retargeting an immutable
subject creates a linked replacement and preserves the old source and replies.
After a changed head, inspect the original and proposed mapping and explicitly
reaffirm or retarget; identical selected bytes do not establish unchanged context.

**Stop** revokes remaining authority and retains confirmed effects. Restart reads
and reconciles what may have happened; inspect a fresh preview and use **Resume**
for remaining writes. An unknown outcome is not proof of failure and must not be
reposted blindly. During partial maintenance, each stop keeps an overview escape
link and the existing root remains the route until its final update.

## Optional writing assistance

[Settings → Agents & context → AI assistance · Writing instructions](settings.md#review-preparation-writing-hooks)
holds four independent tour hooks: Problem statement, Solution summary, Tour outline
and Tour-stop comment. A fifth, Review feedback, is for comment workshop requests,
not tour fields. Each has a versioned effective source, Restore default and
an explicitly invoked nonpublishing Test. Changes are profile scoped.

The field button rewrites the captured selection, including whitespace, or the
whole nonempty field when nothing is selected. Empty brief fields first recover
an unambiguous description section without inference. Other drafting needs bounded
approved context. One successful result is one undoable local edit; Redo reuses
received bytes. Editing, navigation, IME, account/window changes or a stale result
cannot redirect the replacement to newer text. Failure leaves the field unchanged.

**Suggest tour outline** supplies titles, source suggestions and private reasons,
without explanation bodies. Insert selected skeletons, visit each stop with its
source, write manually or explicitly Generate comment, then choose Ready and next
for that current text. There is no bulk generation or bulk completion command.
No action publishes AI text automatically or retrieves linked tickets or runs
repository instructions. Review unsupported claims yourself; schema validation
and canned-process tests do not prove model quality. Installed-provider restrictions
and actual model quality remain unqualified; an unsupported selected protocol
leaves manual editing available.

## Retention and removal

Private drafts, editable remote baselines, conflict versions and generated-stop
provenance are retained in the admitted tour store. The existing publication
recovery store keeps bounded frozen operations and returned identities. Profile
purge includes these known stores; unsent work and private history can be lost.
Published comments stay on GitHub. Uninstalling program files is not profile
purge. External backups may retain copies; keep them under the same privacy
controls as source and review text. See [Storage and diagnostics](storage-and-diagnostics.md).
