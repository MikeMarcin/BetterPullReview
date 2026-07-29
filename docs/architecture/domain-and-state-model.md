# BetterPullReview Domain and State Model

## Architectural premise

The functional core owns compact interaction state and deterministic policy. The application host owns authoritative external/local sessions, persistence, caches, and effects. The Qt presentation adapter owns visual and Qt-local state. Large GitHub, Git, diff, and ledger data is observed through stable identities and bounded views rather than copied into one global state tree.

## Strong identities

```text
GitHostId
GitHubAccountId
RepositoryId
PullRequestId
CommitOid
BlobOid
ReviewEpochId
FileMaterialId
GitHubReviewId
GitHubThreadId
GitHubCommentId
LocalDefectId
AiSessionId
RequestId
RequestGeneration
ApplicationSessionId
```

Every persisted row, observation, request, completion, cache key, and asynchronous result includes enough identity to prevent cross-host, cross-account, cross-repository, cross-PR, or cross-revision reuse.

## Authority by state category

| Category | Authority | Core representation | Qt representation |
|---|---|---|---|
| GitHub shared state | GitHub/provider session in host | stable IDs, current revision facts, bounded observations | immutable cards/rows/actions |
| Private durable state | SQLite ledger in host | current relevant receipt/actionability observations | badges, filters, commands |
| Review interaction | Functional core | active PR/file/change, epoch pair, navigation mode, pending semantic action | selected indexes, focus, visible command state |
| Git/GitHub/diff caches | Host | cache identities and availability observations | loading/unavailable projections |
| Layout and focus | Qt adapter, persisted through host settings | only semantic state when behavior depends on it | drawer width, scroll, hover, expanded nodes, animation |
| Pending work | Core and host | request/generation/revision correlation | progress/cancel presentation |

When local and GitHub state disagree about a shared fact, GitHub wins. When a widget and the core disagree about semantic selection, the core wins and the adapter repairs the visual selection.

## Core application state

The core should retain only the state needed to make decisions, for example:

```text
application session identity
active host/account/repository/PR identity
selected left/right review epochs
active file material and change group
filtered navigation context
semantic selection
command state inputs
pending request identities and generations
review interaction mode
AI session/finding staleness state
compact diagnostics and status state
```

The core does not own all PR comments, all Git blobs, all diff rows, all queue entries, or the SQLite database. It receives the bounded records required for the current transition.

## Semantic input/output types

### Intents

```text
OpenPullRequest
SelectComparisonEpochs
SelectFile
SelectChangeGroup
NavigatePreviousFile
NavigateNextFile
NavigatePreviousChange
NavigateNextChange
MarkFileRead
AcceptFile
ExcludeFile
MarkConversationRead
AcceptConversation
CreateDraftComment
PublishReview
ClassifyThreadAsDefect
VerifyDefect
CreateObserverRule
StartAiReview
```

### Bounded observations

```text
CurrentHostCapabilitiesObservation
CurrentPullRequestSnapshotObservation
SelectedFileMaterialObservation
VisibleNavigationSetObservation
ReviewPermissionObservation
ReceiptObservation
ConversationVersionObservation
CurrentHeadObservation
AiProviderPolicyObservation
```

Observations expose facts, not service objects. They carry source identity/revision so the core can reject stale or mismatched decisions.

### Host requests

```text
FetchPullRequestRequest
EnsureGitObjectsRequest
ComputeDiffRequest
PersistReceiptRequest
PublishReviewRequest
ResolveThreadRequest
RefreshQueueRequest
ShowNativeNotificationRequest
LaunchAiSessionRequest
PersistSettingsRequest
```

Requests are semantic. The core does not request a `QMessageBox`, `QNetworkReply`, `QProcess`, or SQL statement.

### Result events

```text
PullRequestLoadedEvent
GitObjectsAvailableEvent
DiffComputedEvent
ReceiptPersistedEvent
ReviewPublishedEvent
ReviewPublishFailedEvent
QueueRefreshedEvent
AiSessionStartedEvent
AiFindingReceivedEvent
RequestCancelledEvent
```

### Semantic deltas

```text
CommandDelta
SelectionDelta
FileReceiptDelta
ConversationDelta
QueueRowDelta
StatusDelta
DiagnosticDelta
```

Deltas describe meaning. Qt-specific row/index updates are derived by the presentation adapter.

## Pull request and epoch

```text
PullRequestKey = host + account + repository node/database identity + PR number
ReviewEpoch = base OID + head OID + observed time + cause + ordinal
```

Epoch causes include initial observation, author upload, force-push, base advance, base-branch change, and explicit historical import. Display labels may say `Branch base`, `Review epoch 4`, `My last review`, or `Latest`, but OIDs remain authoritative.

The host owns epoch persistence and retained Git refs. The core owns the current left/right epoch selection and comparison policy. The Qt adapter owns selectors, labels, popup layout, and unavailable-state presentation.

## File material

A file review receipt attaches to immutable compared material rather than a path alone:

```text
FileMaterialId = hash(left blob, right blob, old path, new path, change kind, comparison policy)
```

This preserves state across pure renames where appropriate and invalidates state when compared content changes. Blob contents and diff caches remain host-owned. The core operates on IDs, summaries, and bounded selected material.

## Private receipt states

Files:

```text
Unseen -> Seen -> Read -> Accepted
                     └── changed material -> ChangedSinceRead
Accepted + changed material -> ChangedSinceAccepted
Excluded is an explicit terminal state for the current material only.
```

Conversations:

```text
Unread -> Read -> Accepted
new or edited comment -> Unread
```

GitHub thread resolution is distinct from private acceptance. Receipt/actionability transitions are deterministic core policy; SQLite storage and queries are host effects.

## Defect projection

```text
Draft -> Published/Open -> FixClaimed -> NeedsVerification
NeedsVerification -> Verified | Reopened
Open -> Withdrawn | AcceptedAsIs
```

The underlying published object remains an ordinary GitHub thread. `FixClaimed` and `NeedsVerification` are local interpretations until represented by normal human-visible GitHub replies. The core owns legal transitions; the host correlates GitHub/thread observations and persists local classification; Qt presents badges and actions.

## Asynchronous identity

Every request carries the identity required for deterministic acceptance:

```cpp
struct request_context
{
    request_id Request;
    application_session_id Session;
    git_host_id Host;
    github_account_id Account;
    pull_request_key PullRequest;
    std::optional<review_epoch_id> Epoch;
    request_generation Generation;
};
```

Not every request needs every field, but no completion may be accepted merely because the originating widget still exists.

## Qt-local state

The following normally stays in the Qt adapter rather than the core:

- focused widget and hover state;
- file-tree expansion;
- scroll positions and horizontal anchors;
- drawer widths and splitter geometry;
- transient selection rectangles;
- animation progress;
- font metrics and text-layout caches;
- native menu/popup placement.

The host may persist user settings, but that does not make the core or database the runtime owner of Qt mechanics.
