# RFC-BPR-0032 account setup in the window, amendment r1

Status: proposed, implemented on a branch for the owner's look; not adopted
until the owner accepts it. Owner request, 2026-11-11, relayed by the
coordinator: "The initial startup account connection flow is functional but it
feels pretty bad compared with our newer setup flows like the agent tab. My
main issue is that it's several pop-up windows and kind of disorienting when
going through the CLI flow. Maybe we can redesign this flow to be a clearer
progression."

This amends the [shared sign-in amendment (BPR-0915)](../RFC-BPR-0032-calm-home-and-accounts-surfaces.md#owner-amendment-shared-sign-in-bpr-0915)
for Home's first-run sign-in only. Settings › Accounts and hosts, pasted pull
request links that need an account, and GitHub Enterprise Server keep their
current ways in.

## Grounded current behavior

A signed-out window shows Home's first-launch card
(`src/adapters/qt/src/workbench_landing.cpp`) with **Sign in**, **Use GitHub
CLI** and **Add enterprise server**. Each opens surfaces of its own:

1. **Sign in** opens the shared sign-in dialog, a separate non-modal window
   (`workbench_window::open_sign_in`) that borrows the Accounts and hosts page
   and its server cards. The device code appears inside the github.com card's
   account row (`device_authorization_panel`), and **Open browser** hands the
   address to the system browser.
2. **Use GitHub CLI** opens the same dialog in its CLI-only mode and starts gh
   discovery. One usable account connects at once; the activation then opens a
   second, modal window over the dialog, the consent question
   (`ask_github_cli_read_consent`, a `QMessageBox` with Show Details). Several
   accounts are offered as cards; none or a failing gh is a status line under
   the dialog's buttons.
3. The dialog closes itself when a session is accepted. A refused account is
   said on the server card in the dialog.

So the CLI way stacks Home, a dialog, and a modal question, and the order of
what happens is told only by which window is in front.

## Proposed behavior

Signed out, Home shows the account setup where the first-launch card stood:
one surface, four numbered steps, built from the Agent tab setup's parts
(RFC-BPR-0092): numbered markers that turn into ticks, whole cards to choose,
check marks for running, passed and failed, and a step that folds to one
line once done.

1. **How to connect.** Two cards, **Sign in with your browser** (picked
   first) and **Use the GitHub CLI** (shown disabled, with the reason, where
   gh cannot be inspected). Continue says what it does: **Get a sign-in code**
   or **Check the GitHub CLI**. A link keeps GitHub Enterprise Server's
   existing way in.
2. **Sign in** (browser) or **Choose an account** (CLI).
   - Browser: the code on a plate of its own, large, with **Copy code**; the
     address as the host validated it; **Open GitHub**; the time left; a
     running mark, "Waiting for you to approve on GitHub…"; and **Cancel and
     choose another way**, which ends the code. An expired, declined or
     failed code says what happened and offers **Get a new code**.
   - CLI: a running mark while gh is checked. Missing, too old, unreadable,
     signed out (with `gh auth login` to copy) or no usable account each say
     what to do, with **Check again**. Accounts are cards; gh's active account
     is said ("active in gh") but never preferred, as RFC-BPR-0078 requires an
     explicit choice. One account is chosen without asking. The consent is
     said under the choice, for that account, with RFC-BPR-0015's full text
     behind **What connecting allows**. **Connect as LOGIN** is the consent.
3. **Confirm the account.** "GitHub accepted the code. Checking which account
   signed in…", or "Checking LOGIN with GitHub…". A refused account returns
   to step 2 with the reason and the choice kept.
4. **Ready.** "Connected as LOGIN on HOST.", then **Start reviewing**, which
   leaves the setup for Home.

**Change** on the folded first step goes back (and ends a code still shown)
until the account is being confirmed. Nothing in the setup opens a window.
A session accepted before a way is chosen (a remembered connection validated
at launch) ends the setup without a Ready step.

## Boundaries

The core decides, the host performs, Qt presents:

- `core::transition_account_setup` (`bpr/core/account_setup.hpp`) is the
  setup's one deterministic transition: phases, the step each shows, a closed
  set of problems, and typed effect requests. Every effect that expects an
  answer carries a new request identity; an answer naming another is refused
  as stale, so a gh check or code abandoned by Back cannot move a later
  choice.
- `application::account_setup_host` applies it on the decision lane, queues
  events raised while one is applied, marshals gh's answer onto the lane as a
  typed event, and drops everything after shutdown. Its effect port is
  implemented by the composition over what already exists: gh discovery, the
  device host's own begin, cancel and open, and the Connect action's
  activation. Qt receives only `account_setup_projection`, whose device part is
  the device host's own presentation.
- `qt::account_setup_view` is presentation only. Copying the code is the one
  local action, as on the device panel.
- BetterPullReview still never runs `gh auth login`, `auth switch` or
  `logout`, and never reads or stores a gh token (RFC-BPR-0015,
  RFC-BPR-0078). Pressing **Connect as LOGIN** records the same consent the
  dialog's Connect recorded.

The setup is composed for the interactive application. Inside the automated
test boundary it is composed only for a fixture that sets
`AccountSetupInWindow`, so courses written for the shared dialog keep it until
they are moved.

## Acceptance

- AS-1: equal setup states and events give equal states, effects and refusals;
  stale and abandoned answers change nothing (`core_account_setup`).
- AS-2: the host asks for exactly the effects the core requested, marshals
  gh's answer onto the decision lane, survives a port that publishes from
  inside an effect, and publishes nothing after shutdown
  (`application_account_setup_host`, stubbed gh and device flow).
- AS-3: offscreen, both ways walk all four steps, including cancel, every gh
  problem, an expired code, a refused account and retry, with no window
  opened (`qt_account_setup_view`).
- AS-4: the owner accepts the look, one question per criterion, in an
  attended review of a frozen build.

## Open questions for the owner

- Q1: Should **Sign in** for a pasted pull request link, and **Use another
  account**, use this setup too, retiring the shared dialog?
- Q2: Should GitHub Enterprise Server get its own way card (server, then
  browser or token) instead of the link to Accounts and hosts?
- Q3: With several gh accounts, keep "no account chosen until you choose", or
  pick gh's active account first?
