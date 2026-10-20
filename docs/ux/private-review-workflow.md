# Private review history and publication

This guide is for reviewers who want to think through a pull request privately
before saying anything on GitHub. It covers private conversations, choosing
sources for an agent, turning private material into a draft, publishing that
draft, and the saved inline comments you have written.

> [!NOTE]
> Everything described here stays on your machine until you publish a draft.
> Nothing private is added to a published comment, and deleting private data
> never deletes a GitHub comment.

## Open private history

In a signed-in pull request, open **Review > Agent > Private history**. (The
**Notes** tab holds only private notes.) Private history has three tabs:

- **History** shows every revision of each private conversation.
- **Sources** lets you pick exact turns or findings to work from.
- **Write** starts a new private thread.

These conversations are kept apart from GitHub review threads.

## Write and read private threads

In **Write**, start a thread about the whole pull request or about code. A code
thread remembers the exact revision and line it was written against.

**History** shows each revision with who wrote it and whether its code
location is current, stale or can no longer be placed. Revisions never change:
editing a thread adds a new reviewer revision and keeps the earlier text.

Older notes brought over from the previous storage format show an unknown
author (`legacy_unknown`) instead of a guessed author or code location.

## Ask an agent about selected sources

1. Open **Sources** and select the turns or findings you want to use.
2. Choose **Ask agent using only selected sources**.

Selecting sources does not send anything. Only that button sends the selected
private text to the agent.

The run records separately:

- the model and effort you requested,
- the configured defaults,
- what the protocol reported, and
- what the response said about itself.

A missing report stays "unknown", and reports that disagree are shown as
conflicts. A profile name or requested model does not prove which model
actually answered.

## Squash private work into a draft

**Squash to Draft** opens a local composition view.

1. Choose how to write the draft: your own prose, an exact copy of one source,
   or agent-assisted composition that you start explicitly.
2. If your sources point at different places, choose one exact target, or
   choose **Split into exact target drafts** to make one draft per target.
   Sources that can no longer be placed need a new selection.
3. Choose **Accept as Draft** to save a local review draft.

The draft keeps a list of exactly which sources it came from. A stale source
can still inform your reasoning, but its code location is never moved to the
current head without you. **Accept as Draft** does not change anything on
GitHub.

## Publish a draft

1. Open the draft in the ordinary draft editor and review or edit its text.
2. Choose **Publish**.

Before publishing, BetterPullReview checks the author, sources and target
against the current pull request. The published comment contains only the text
you approved; a comment on a file also gets a visible link to that exact file
version. Private thread IDs, how the draft was derived and model details are
never added to the public text.

> [!WARNING]
> After GitHub accepts a comment, the local record can stay "pending" until
> BetterPullReview confirms it with the server. Do not create a fresh draft
> and post again just because the local receipt is missing; that can publish
> the comment twice.

Publishing from private history has been verified with local fixtures and in
the app, but not yet against live GitHub with independent server readback.

## Saved inline comments

Choose **Finish review > Saved comments** to see the comment text you have
kept, including comments written on an earlier revision. For each one the
viewer shows the revision it was written on and whether it is unpublished,
published, discarded or awaiting publication confirmation.

- You can copy earlier text after checking it against the current code. It is
  never moved to the new head automatically.
- If a publication is uncertain, use **Check status** before reusing its text.
- You can **discard** a draft that is still unpublished, including one on an
  earlier head. Its text stays in history and it no longer counts toward the
  active-draft limit. You cannot discard a draft whose publication is
  uncertain, or a comment already confirmed on the server. The command checks
  the revision shown before discarding.

### When comments are saved

Selecting lines does not create a draft. A comment is saved the first time you
edit it, before it can be published.

If saving fails, your text stays in the editor and **Reapply my text**
appears. Fix the problem before leaving the comment.

### Clean up empty drafts

**Free unused empty drafts…** removes old empty rows that were never edited,
after you confirm. Written comments, replies, composition sources and
publication records are kept. A comment you edited and then cleared on purpose
is not removed by this cleanup.

## Limits and storage

| Limit | Value |
|---|---|
| Active drafts you are writing | 64 per account and pull request |
| Draft records kept, including published and discarded | 4,096 per account and pull request |
| Private data budget that drafts share with other private data | 64 MiB |
| Automatic deletion by age | None |

Published and discarded drafts are kept as history and do not use an active
slot. When a write would go over a limit, it is refused; text is never cut
short to fit.

Private history, drafts and where they came from are stored in your profile.
Deleting them, or purging the profile, removes local data only. See
[Storage and diagnostics](../development/storage-and-diagnostics.md) for the
exact limits and what a profile purge removes.
