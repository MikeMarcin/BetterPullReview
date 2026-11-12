# RFC-BPR-0032 account setup in the window, amendment r1

Status: adopted by the owner on 2026-11-12 in attended review session
`dcb3c9f70ca09b5cb2169ff96d811309` (all five look criteria accepted; the three
open questions answered, below). The server step that followed from Q2 is not
yet looked at by the owner. Owner request, 2026-11-11, relayed by the
coordinator: "The initial startup account connection flow is functional but it
feels pretty bad compared with our newer setup flows like the agent tab. My
main issue is that it's several pop-up windows and kind of disorienting when
going through the CLI flow. Maybe we can redesign this flow to be a clearer
progression."

This replaces the [shared sign-in amendment (BPR-0915)](../RFC-BPR-0032-calm-home-and-accounts-surfaces.md#owner-amendment-shared-sign-in-bpr-0915):
the shared sign-in dialog and the GitHub CLI consent question are removed.

## Owner decisions, 2026-11-12

- Look: accepted as one progression in the window; the browser code step; the
  GitHub CLI account choice with its consent in place; the problem states;
  the same language as the Agent tab's setup.
- Q1, answered "Use the setup there too and retire the dialog": a pasted pull
  request link that needs an account, Use another account, Settings and
  GitHub Enterprise Server all use this setup. Owner addition: adding an
  account from Settings uses the same progression, in one surface, with no
  stacked pop-ups; one implementation that Home and Settings both open.
- Q2, answered "Add an Enterprise way card (server, then browser or token)",
  with the note "I think server is the first step, github.com or enterprise":
  the setup begins with the server, then the way.
- Q3, answered "Pick gh's active account first": gh's active account is chosen
  first and still labelled; another may be chosen.

## Grounded behavior before

A signed-out window showed Home's first-launch card
(`src/adapters/qt/src/workbench_landing.cpp`) with **Sign in**, **Use GitHub
CLI** and **Add enterprise server**. **Sign in** opened the shared sign-in
dialog, a separate window borrowing the Accounts and hosts cards, with the
device code in the github.com card's row. **Use GitHub CLI** opened the same
dialog and then, for the activation, a modal consent question over it
(`ask_github_cli_read_consent`). An Enterprise server was added from a draft
card on that page, and its token was typed into a third, modal prompt
(`prompt_for_credential`). The order of what happened was told only by which
window was in front.

## Behavior

The one setup is Home's (`qt::account_setup_view`), in place of the
first-launch card and of Home's lists while it is open. Every way in opens it
there: Home's first run, Settings › Accounts and hosts › **Add account**, a
server card's **Sign in** or **Connect**, the strip's account button signed
out, a refusal's **Sign in**, **Use GitHub CLI** and **Use another account**,
and a pasted link that needs an account. Settings gives way to Home. With an
account already, the setup is titled **Add a GitHub account** and offers
**Close**.

1. **Server.** **GitHub.com**, or **GitHub Enterprise Server** with its
   hostname typed in the card. A hostname with a scheme or path is refused in
   place. An Enterprise server is added (kept, as the draft card kept it) when
   the reviewer continues; a link or card that names the host fills it in and
   waits for Continue.
2. **How to connect.** GitHub.com: **Sign in with your browser** (first) or
   **Use the GitHub CLI**. Enterprise: **Use a personal access token** (first)
   or **Use the GitHub CLI**. A way this build cannot offer is shown with why.
3. **Sign in**, **Choose an account** or **Enter a token**.
   - Browser: the code on its own plate with **Copy code**, the address,
     **Open GitHub**, the time left, a waiting line, and **Cancel and choose
     another way**. Expired, declined or failed codes offer **Get a new code**.
   - GitHub CLI: gh's accounts on that server as cards, gh's active one chosen
     first and labelled. Missing, too old, unreadable, signed out (with
     `gh auth login` to copy) and no usable account each say what to do, with
     **Check again**. The consent is said under the chosen account, with
     RFC-BPR-0015's full text behind **What connecting allows**; **Connect as
     LOGIN** is the consent.
   - Token: a password field, how to create the token, and **Sign in**. The
     field is read and erased in place; the token goes to the host as erasable
     bytes and never enters the core.
4. **Confirm the account.** A refused account or token returns to step 3 with
   the reason, the choice kept.
5. **Ready.** "Connected as LOGIN on HOST.", then **Start reviewing**.

**Change** on a folded step goes back to it (and ends a code still shown)
until the account is being confirmed. A remembered connection validated at
launch ends a setup Home began by itself while it is still at its first two
steps, and Home's Connecting card is shown instead of the setup while it
validates. A setup the reviewer asked for is never ended that way.

The setup holds Home while it is open, unless a pull request or its cached
overview is open. Over an account (Add account) it has Home to itself;
signed out, Recently viewed (a public preview's reading) stays beneath it.

## Boundaries

- `core::transition_account_setup` (`bpr/core/account_setup.hpp`) is the one
  deterministic transition: phases, the step each shows, a closed set of
  problems, which ways a server offers, and typed effect requests
  (`prepare_enterprise_server`, `inspect_github_cli`, `begin_browser_sign_in`,
  `cancel_browser_sign_in`, `connect_github_cli`, `enroll_token`). Every
  answered effect carries a new request identity; a stale answer is refused. A
  device flow begun for a pasted link is followed as the browser way.
- `application::account_setup_host` applies it on the decision lane, marshals
  gh's and the server's answers onto the lane, and stops at shutdown. Qt
  receives only `account_setup_projection`.
- The composition implements the effect port over what exists: the queue's
  saved-server add, gh discovery filtered to the server, the device host's
  begin, cancel and open, the Connect action with recorded consent, and the
  host's manual enrollment (or the link's staged credential) for a token.
- BetterPullReview still never runs `gh auth login`, `auth switch` or
  `logout`, and never reads or stores a gh token (RFC-BPR-0015,
  RFC-BPR-0078).

## Acceptance

- AS-1: equal states and events give equal results; stale and abandoned
  answers change nothing (`core_account_setup`).
- AS-2: the host asks for exactly the requested effects, marshals answers onto
  the lane, survives reentrant ports and publishes nothing after shutdown
  (`application_account_setup_host`).
- AS-3: offscreen, every step of both servers and all three ways, with
  cancel, problems and retry, and Settings' Add account, open no window
  (`qt_account_setup_view`).
- AS-4: over the composed application, the GitHub CLI and browser ways,
  Settings' Add account and an Enterprise server reach their steps with no
  dialog or modal question (`application_account_setup_public_path`), and the
  courses that signed in through the shared dialog sign in through the setup.
  The composition source contract holds the gh read gate on the setup and
  refuses a restored modal question (`application_composition_source_contract`).
- AS-5: the owner accepted the look (attended session above); the server step
  is to be shown to the owner.
