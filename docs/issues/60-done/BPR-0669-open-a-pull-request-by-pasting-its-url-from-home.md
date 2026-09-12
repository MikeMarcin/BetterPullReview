---
schema: "implement-rfc.card.v1"
id: "BPR-0669"
title: "Open a pull request by pasting its URL, from Home"
status: "Done"
rfcs: ["RFC-BPR-0028","RFC-BPR-0026"]
depends_on: []
priority: "P1"
acceptance: [{"criterion":"Pasting a pull request link on Home opens that pull request, on GitHub.com and on an enterprise host; owner/repo#123 does the same","outcome":"pass","revision":"c354e08a","evidence":{"kind":"observation","detail":"qt_composed_github_scale_{100,125,200} pass: a link pasted whole into Home's bar offers Open, and Open sends the same open intent Return does, carrying owner, name and number, for a link on a non-GitHub.com host; the existing assertions for a GitHub.com link and for owner/name#number by Return are unchanged and still pass. A paste that names no pull request is answered beside the bar and the text stays. Silencing the bar or always offering Open fails those assertions."}},{"criterion":"No entry field asks the reviewer to split a link by hand","outcome":"pass","revision":"c354e08a","evidence":{"kind":"observation","detail":"The Settings locator fills its owner, repository and number fields from one pasted reference, pinned by qt_composed_github; an owner typed alone is left where it was typed. Home's landing card no longer instructs the reviewer to enter an owner, a repository and a number under Accounts and hosts, and points at the bar instead. check_boundaries, strict tidy on the three touched translation units, clang-format --Werror and verify_documentation.py pass, as do the composition, round-trip and queue contracts."}}]
---


# Open a pull request by pasting its URL, from Home

The owner, 2026-09-07: "we had described the UX as allowing pasting a URL but I
still have to manually split it myself. And for some reason that PR entry
process is in the settings menu instead of on the home."

Both are true. The locator takes an owner, a repository and a number in
separate fields, and it lives on the session panel, which
[BPR-0646](../10-backlog/BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md)
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

## Notes (2026-09-07)

- **What was there.** Home already had an open bar that parsed a link, and
  the owner never found it. It was a bare line edit with a placeholder, no
  label, no button and Return as its only affordance, sitting under a
  landing card whose signed-out text sent the reviewer to Settings and
  whose signed-in text told them to "enter its owner, repository and
  number under Accounts and hosts". The locator there takes three fields,
  so that instruction was the manual splitting the card is about.
- **The bar.** A heading with the bar as its buddy, an Open button beside
  it that is offered only while the text names a pull request, and a line
  under it that answers a paste naming no pull request instead of doing
  nothing and keeping the text. Return still works. The placeholder names
  the two forms without spelling a host, because the Qt boundary gate
  refuses a link literal in this layer, which is the right refusal.
- **The card.** The signed-out card now says a public pull request can be
  opened without signing in by pasting its link below, and the
  choose-a-pull-request card points at the bar rather than at Settings.
- **The locator.** Pasting a whole reference into the first field of the
  Settings locator fills the owner, the repository and the number. An
  owner typed on its own never parses as a reference, so ordinary typing
  is untouched.
- **Evidence.** `qt_composed_github_scale_{100,125,200}` pin the pasted
  link filling all three locator fields, an owner left alone, Open offered
  only for a reference, the refusal beside the bar, and Open sending the
  same intent Return does from a link pasted whole on a non-GitHub.com
  host. `qt_workbench_window_scale_*`, `qt_review_landing_model`,
  `qt_anonymous_public_preview_*` and the composition, round-trip and
  queue contracts pass. `check_boundaries`, strict tidy on the three
  touched translation units, `clang-format --Werror` and
  `verify_documentation.py` pass.
- **Negative mutations.** A locator that ignores a pasted link fails "a
  pasted link fills the owner, the repository and the number"; a silent
  bar fails "a paste that names no pull request is answered beside the
  bar"; an always-enabled Open fails "Open is not offered while the bar
  names no pull request".
- **Left for BPR-0670.** Reading without signing in is still reached only
  through Settings. The bar can now name a pull request from Home, and the
  next card puts the signed-out path beside it.
