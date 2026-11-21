---
schema: "implement-rfc.card.v1"
id: "BPR-2319"
title: "Accounts & hosts says how a connection signs in in one short sentence"
status: "Done"
rfcs: ["RFC-BPR-0032"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","defect":true,"reservation":"docs/issues/_reservations/BPR-2319--BPR-2319--e0d762b9e25d.json","origin":"owner chat 2026-11-20, looking at a GitHub CLI connection in Settings › Accounts & hosts (\"No one is going to read something this long on accounts\")","related_cards":["BPR-0772","BPR-0933","BPR-1430"],"acceptance":["AC1","AC2","AC3","AC4","AC5"]}
---

# BPR-2319: Accounts & hosts says how a connection signs in in one short sentence

## The owner's words (2026-11-20)

"No one is going to read something this long on accounts"

## What was wrong

A server card with a GitHub CLI profile showed RFC-BPR-0015's whole residual
trust disclosure as a paragraph of about 150 words under the account
(`cli_trust_warning_text`, label `githubCliTrustWarning`), on every platform.
Above it, a signed-in GitHub CLI session said it was signed in with a token
for this session only and asked for again after a restart. That sentence is
wrong for gh: BetterPullReview holds no token of its own there and reuses the
connection until sign-out. The grant projection reports every
non-device-grant session as `token`, and the card read it as a typed token.
The account setup's GitHub CLI consent ran to three sentences.

## What changed

| Surface | Before | After |
|---|---|---|
| Server card, GitHub CLI trust line (signed out with the offer, or connected through gh) | The ~150-word paragraph, wrapped under the account | "Uses gh's sign-in for this account; the token isn't saved." on one line, then an info glyph (`githubCliTrustDetails`). Its popover and accessible description hold the full disclosure, unchanged in content and now split into short paragraphs |
| Server card, signed in through GitHub CLI | "Signed in with a token for this session only; it is asked for again after a restart." | No consequence sentence. The trust line says how it signs in. In setup mode, where the trust line is hidden, the trust line's sentence takes its place |
| Server card, signed in with a typed token (native HTTPS, GHES) | "Signed in with a token for this session only; it is asked for again after a restart." | "Signed in with a token for this session only." |
| Server card, signed out, token way in | "Signs in with a token for this session only; it is asked for again after a restart." | "Signs in with a token for this session only." |
| Account setup, GitHub CLI consent under the chosen account | Three sentences: will use the account's existing gh sign-in for reads and won't save its token; reuse until sign-out; connecting doesn't let it publish | "BetterPullReview uses LOGIN's existing gh sign-in and doesn't save its token." Reuse and the publishing boundary stay in What connecting allows, which is unchanged |

The GitHub CLI offer's accessible description is now the short sentence. The
glyph comes after the offer in tab order and carries the full text. The
setup's way-of-signing-in cards, intro and token instructions are one or two
short sentences each and are unchanged. So are the Remember and Forget
glyphs.

RFC-BPR-0032 quoted the old token sentence as the account row's example. It
now quotes the short one, and an owner amendment paragraph records the rule.
RFC-BPR-0015 and RFC-BPR-0079 ask that the residual trust be named and
accessible in the existing disclosure. They quote no wording, and the glyph
keeps every phrase. Neither RFC is amended.

## Acceptance

- AC1. A GitHub CLI server card shows one sentence of at most 80 characters,
  with no line break, about how the connection signs in. The full RFC-BPR-0015
  disclosure (each phrase the earlier course required, per platform) is the
  info glyph's tooltip, popover and accessible description. The glyph is
  keyboard-focusable and appears only on the card with a GitHub CLI profile.
- AC2. Neither the sentence nor the disclosure names the host; the
  disclosure says "this server".
- AC3. A signed-in GitHub CLI card makes no session-only token claim.
- AC4. The token sentences on Accounts & hosts are one short sentence.
- AC5. The account setup's GitHub CLI consent is one sentence naming the
  chosen account, with the full text still behind What connecting allows.

## Evidence

Candidate `c1fb7d2e6` (rebased onto main `814f0a875` with no source change),
macOS. The disk had about 2 GB free, too little for a dev tree of its own.
Instead, the changed translation units (`server_card.cpp`,
`account_setup_view.cpp`, `workbench_window_github_presentation.cpp`, which
allocates the card whose layout changed, and the three test sources) were
compiled with the dev preset's exact flags. They were linked ahead of a
shared dev tree's libraries. The test binaries were checked to hold the new
strings and none of the old.

| Criterion | Check | Result |
|---|---|---|
| AC1, AC2, AC3 | `qt_composed_github_scale_100` (full run), including `test_cli_warning_lives_on_its_servers_card`: the short line, the glyph's tooltip, accessible description and focus, every per-platform disclosure phrase, no host, CLI card only, and a signed-in CLI session with a `token` grant showing no consequence sentence. Also `test_accounts_page_says_each_fact_once` and the per-server warning course | Passed |
| AC1 | `qt_composed_cli_read_consent` | Passed |
| AC4 | `qt_workbench_window_scale_100`: enterprise token sentence | Passed (on the candidate before the one-line `setWordWrap(false)` and a test message change. A relink after that failed because the shared tree's Qt library had moved to another branch's in-progress signature) |
| AC5 | `qt_account_setup_view`: the consent text exactly; What connecting allows still present | Passed |
| Regression | `qt_composed_home_sign_in` | Passed |
| Look | Offscreen render of a GHES card signed out with the CLI offer and signed in through gh: one line of trust text plus glyph, no token claim when connected. The first render showed the wrapping label folded into four lines, which is why the line does not wrap | Checked by the implementer; not committed |
| Boundaries | `architecture_boundaries` (`VerifyArchitectureBoundaries.cmake`) | Passed |
| Standard | `format_check.py` (clang-format 22.1.8) on every changed C++ file; `tidy.py --warnings-as-errors` on `server_card.cpp` and `account_setup_view.cpp` | Passed |
| Docs | `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

Not run: Windows and Linux. The change is portable Qt text and layout, and
the off-macOS disclosure phrases are asserted under `#else`. Also not run:
the 125% and 200% scale variants and a full `ctest`. No owner look was
requested.
