---
schema: "implement-rfc.card.v1"
id: "BPR-0669"
title: "Open a pull request by pasting its URL, from Home"
status: "Backlog"
rfcs: ["RFC-BPR-0028","RFC-BPR-0026"]
depends_on: []
priority: "P1"
---


# Open a pull request by pasting its URL, from Home

The owner, 2026-09-07: "we had described the UX as allowing pasting a URL but I
still have to manually split it myself. And for some reason that PR entry
process is in the settings menu instead of on the home."

Both are true. The locator takes an owner, a repository and a number in
separate fields, and it lives on the session panel, which
[BPR-0646](../40-parked/BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md)
adopted onto Settings > Accounts and hosts. RFC-BPR-0028 AC-06 says Home
funnels to the queue or the locator when no pull request is open; the locator
is not on Home at all.

## Scope

- One field that accepts what a reviewer has on the clipboard: a full
  `https://github.com/owner/repo/pull/123` URL, the same on an enterprise
  host, and `owner/repo#123`. It splits them; the reviewer never does.
- The field lives on Home, where AC-06 puts it, and stays reachable from the
  strip while a review is open. Settings keeps host and account, not the way
  in.
- What a paste that is not a pull request URL says, in one line, without
  clearing what was typed.
- Owned paths: `src/adapters/qt` Home page and session panel locator, the core
  decision that parses the reference, their tests.

## Acceptance

- Pasting a pull request URL on Home opens that pull request, on GitHub.com and
  on an enterprise host; `owner/repo#123` does the same.
- No entry field asks the reviewer to split a URL by hand.
