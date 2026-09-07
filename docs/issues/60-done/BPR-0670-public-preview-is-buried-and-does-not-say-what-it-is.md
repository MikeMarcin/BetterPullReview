---
schema: "implement-rfc.card.v1"
id: "BPR-0670"
title: "Public preview is buried and does not say what it is"
status: "Done"
rfcs: ["RFC-BPR-0028","RFC-BPR-0017"]
depends_on: []
priority: "P1"
acceptance: [{"criterion":"A signed-out reviewer reaches a public pull request from Home without opening Settings, and the control says what it does before it is pressed","outcome":"pass","revision":"a74db881","evidence":{"kind":"observation","detail":"qt_composed_github_scale_{100,125,200} pin the control on Home while signed out, its text and its explanation that comments and review actions stay read-only, the single activate_host_profile_intent in public-preview mode it sends for the selected profile, the focus it leaves in the open bar, and its absence once a session is accepted. Offering it while signed in, or sending the authenticated mode, each fail a named assertion. Sixty-nine Qt, public-preview, composition, device-binding and RFC-BPR-0017 selectors pass."}},{"criterion":"The owner accepts the name and the wording","outcome":"pass","revision":"a74db881","evidence":{"kind":"observation","detail":"The owner chose \"Read without signing in\" on 2026-09-07 when asked. The control, its accessible name, the Settings button and the mode banner all use it; the banner reads \"Reading without signing in\". How it reads in the running application belongs to the attended session BPR-0646 still owes."}}]
---


# Public preview is buried and does not say what it is

The owner, 2026-09-07: "The public preview is buried deep in menus and looks
innocuous, I could barely find it. I'm not even sure if this is meant to be a
'continue without an account' option or a 'staged testing data' option."

It is the first: reading a public pull request signed out, over the
credential-free authority RFC-BPR-0017 built. Today it is a small button on the
session panel, which lives on Settings > Accounts and hosts, next to Connect,
and its name says nothing about signing in or not signing in.

A reviewer with no account has exactly one way to see anything, and it is
behind two navigations and a word that reads like a build channel.

## Scope

- The signed-out path is offered where a signed-out reviewer lands: on Home,
  beside signing in, in words that say what it does and what it costs ("read a
  public pull request without signing in; comments and review actions stay
  read-only").
- The name is decided with the owner. "Public preview" is not it.
- What the mode cannot do stays explained where the reviewer meets the refusal,
  as it already is.
- Owned paths: `src/adapters/qt` Home page and session panel, the mode's
  strings, their tests.

## Acceptance

- A signed-out reviewer reaches a public pull request from Home without opening
  Settings, and the control says what it does before it is pressed.
- The owner accepts the name and the wording.

## Notes (2026-09-07)

- **The name.** The owner chose "Read without signing in" (2026-09-07).
  It replaces "Public preview" on the control, on its accessible name, and
  on the banner that names the state, which now reads "Reading without
  signing in". The build-unavailable line says the same thing. The mode
  itself, its authority and its refusals are unchanged: only what the
  reviewer reads changed.
- **Where it is.** Home's landing card, while signed out, offers it beside
  signing in, with the cost stated: comments and review actions stay
  read-only. Pressing it sends the same `activate_host_profile_intent` in
  public-preview mode that the Settings button sends, for the host the
  profile picker names, and then puts the cursor in the bar that names a
  pull request, so the whole way in is one card. Signed in it is not
  offered, because it would be a way to read less than the reviewer
  already can.
- **What was wrong.** The only entry was a small button named "Public
  preview" beside Connect on Settings > Accounts and hosts. A reviewer
  with no account had to find a Settings page to reach the one thing they
  could do, and the word read as a build channel rather than as an
  account-free read.
- **Evidence.** `qt_composed_github_scale_{100,125,200}` pin the control
  on Home, its wording, the single activation it sends for the selected
  profile, the focus it leaves in the bar, and its absence once a session
  is accepted. Sixty-nine Qt, public-preview, composition, device-binding
  and RFC-BPR-0017 selectors pass, including
  `application_anonymous_public_preview_*` unchanged.
  `check_boundaries`, strict tidy on the touched translation units,
  `clang-format --Werror` and `verify_documentation.py` pass.
- **Negative mutations.** Offering the control while signed in fails
  "signed in, reading without an account is not offered"; entering the
  authenticated mode instead fails "it enters the signed-out read for the
  selected host, and nothing else".
- **Owner acceptance.** The wording is the owner's own choice, recorded
  above. Whether it reads well in the running application is part of the
  attended session that BPR-0646 still owes.
