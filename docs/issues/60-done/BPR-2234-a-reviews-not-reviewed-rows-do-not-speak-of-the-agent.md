---
schema: "implement-rfc.card.v1"
id: "BPR-2234"
title: "A review's Not reviewed rows do not speak of the agent"
status: "Done"
rfcs: ["RFC-BPR-0128"]
depends_on: []
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2231--BPR-2235--072fd709068e.json","origin":"BPR-2219's known gaps, 2026-11-16"}
---

# A review's Not reviewed rows do not speak of the agent

## Origin

With the report format of [[BPR-2219]] the real agent's summary is about the change, but its Not reviewed rows still said "I did not" twice (Claude Code, Sonnet, medium effort, MikeMarcin/BPR-Test#185, 2026-11-16). The review skill's own wording asks for what could not be verified in a chat voice.

## Scope and acceptance

- **AC-a.** The review skill and the report format agree on how unchecked things are written: the thing, then why.
- **AC-b.** A real-agent review's Not reviewed rows name things and reasons, with no first person.

## Delivered

`74429be59`. The shipped review prompts (`prompt_macro_shipped.cpp`, Review
this pull request and Review {file}) ask for Not reviewed as "each thing not
read or not checked, named, then why. Write it about the thing, not about
yourself." The report format (`report-templates.md`) says the same for the
review's last section and for What could break's Not checked: the place or
the thing first, then why, no "I"; its stray "a row a thing" is gone.

## Acceptance

| Criterion | Check | Result |
|---|---|---|
| AC-a | `core_prompt_macro` (`test_review_prompts_carry_their_instructions`: no "you did not", asks for the thing, then why); the report format read beside it | Pass at `74429be59` |
| AC-b | One real review: Claude Code 2.1.319, Sonnet, medium effort, `claude -p` with the shipped review body and the report format, on a three-file scratch change (cache, retries, `--delete`); 2 turns, 22 s, $0.10 | Pass: three Not reviewed rows, each a thing then a reason ("Callers of `fetch()` and `AuthError`: ... no callers exist to check."); no first person anywhere in the answer |

The real run went through the CLI with the app's two texts, not through the
app's launch; the app adds the same report format as its brief.
