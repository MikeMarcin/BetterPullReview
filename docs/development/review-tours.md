# Review Tours

A Review Tour walks reviewers through a pull request: a **Problem**, a
**Solution**, and an ordered route of stops, each a comment on selected lines.
Reviewers need nothing installed; they see ordinary GitHub comments and a
**Start here** comment with **Previous** and **Next** links. AI is optional.

Start one with **Prepare review tour** (`Alt+T`) and add stops with **Add tour
stop** (`Alt+Shift+T`).

## Before you publish

- Nothing reaches GitHub until you confirm the publication plan. **Ready** on
  a stop is a private checkmark for you.
- The Markdown you see is exactly what is published.
- GitHub accepts at most 100 line comments and 64 KiB per review.
  BetterPullReview never truncates a tour to fit, so a large tour may not
  publish.
- Editing the pull-request description changes only its Problem and Solution
  sections.
- Not yet tested against live GitHub: file-level stops, stops outside the
  diff, edits to the **Start here** comment, and GitHub Enterprise Server.

## Updating a published tour

Reopening a tour reads it back from GitHub. If someone edited it on the
website, you choose between your version and theirs. Removing a stop leaves
its old comment and replies on GitHub.

> [!WARNING]
> If a publish ends with an unknown result, choose **Resume** rather than
> publishing again, or comments may be posted twice.

## Writing assistance

The assist buttons rewrite a field or selection as one undoable edit; their
instructions live in [Settings](settings.md#review-preparation-writing-hooks).
AI text is never published without you, so check its claims.

Drafts stay on this computer; see
[Storage and diagnostics](storage-and-diagnostics.md). To work with tours from
a terminal, see the [tour CLI](tour-cli.md).
