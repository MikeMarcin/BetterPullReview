---
id: BPR-0398
title: Superseded operator packets still advertise themselves as live
status: Done
rfc: RFC-BPR-0013
depends_on: []
---

# Superseded operator packets still advertise themselves as live

Three of the board's sixteen frontier roots send the owner to a packet that has
been superseded, and every superseded packet still opens with a status line
saying it is pending. An owner who follows the card gets the wrong procedure,
and the packet they land on agrees with the card rather than correcting it.

This is not a cosmetic drift. `40-parked/` exists so that the only open cards a
person could act on today are the ones at the front of the graph. Two of those
cards currently point at the wrong procedure, and one of the two points at a
procedure [[BPR-0391]] measured as **humanly impossible**.

## What is actually wrong

Each row was reproduced directly rather than inferred from the commit trail.

| Card | Lane | Says | Live packet |
|---|---|---|---|
| [[BPR-0114]] | `10-backlog/` | "The live procedure is now ... `appearance-scheme-foreground-v004`" (`:411-413`) | `appearance-scheme-foreground-v007` |
| [[BPR-0168]] | `40-parked/` | "The live procedure is now ... `native-notification-delivery-v002`" (`:159-161`) | `native-notification-delivery-v005` |
| [[BPR-0220]] | `40-parked/` | "**Run only** the digest-bound procedure in `appearance-scheme-foreground-v004`" (`:174-175`) | `appearance-scheme-foreground-v007` |

All three sentences are forward-pointing instructions, not history. BPR-0220's
is the worst of them: v004 is the procedure the owner attempted twice and could
not complete, because the dark-side measurement fired before a person could
return focus. That is exactly the defect [[BPR-0391]] opened and repaired, and
v007 is the packet that repair produced.

The cause is narrow and worth naming, because it predicts recurrence. A packet
family serves more than one card. When BPR-0391, BPR-0392, and BPR-0100
regenerated their packets they each updated **their own** card and no other:

| Family | Cards it serves | Updated to the live version |
|---|---|---|
| `appearance-scheme-foreground` | BPR-0114, BPR-0220, BPR-0391 | BPR-0391 only |
| `native-notification-delivery` | BPR-0168, BPR-0392 | BPR-0392 only |
| `native-visual-acceptance` | BPR-0100 | BPR-0100 — correct, and it is the family with one card |

The family that stayed correct is the family with a single card. Nothing checks
the others.

## The packets do not correct the mistake either

Of the sixteen packet directories carrying an `operator-packet.md`, eleven are
superseded and every one of them still opens `Status: **pending operator
run**` or `Status: **pending operator decision**`:

| Family | Superseded, still claiming pending | Live |
|---|---|---|
| `appearance-scheme-foreground` | v002, v003, v004, v005, v006 | v007 |
| `native-notification-delivery` | v001, v002, v003, v004 | v005 |
| `native-visual-acceptance` | v001, v002 | v003 |

The convention for saying otherwise already exists and was used exactly once.
`appearance-scheme-foreground-v001` opens `Status: **stale; do not run or
approve...**` and adds a short paragraph explaining that no response to that
version can satisfy its cards. Whoever wrote it stopped there, and the next
eleven regenerations did not repeat it.

## Two packets must not be touched, and the reason is the point

`rfc0016-github-app-registration-v001` and `rfc0016-prerequisite-exception-v001`
also still read `Status: **pending operator decision**`, and both were in fact
answered — each has an adjacent `operator-response.md` and a
`validation-record.json` recording `OPERATOR_RESPONSE_VALID`.

They stay exactly as they are. Their `validation-record.json` binds
`operator-packet.md` itself under `reviewed_packet`, by `sha256` and byte
length. Editing the status line would break the digest and, with it, the record
of what the owner was actually shown when they gave that authority. A frozen
packet reading "pending" is the honest artifact: it is the question as asked,
and the answer sits beside it in `operator-response.md`.

That distinction has to be enforced rather than remembered, which is why the
verifier below checks the digest instead of trusting the next editor.

## Required final state

- BPR-0114, BPR-0168, and BPR-0220 name the live packet for their family.
- Every superseded packet opens with a stale banner naming its superseding
  version, in the form `appearance-scheme-foreground-v001` established.
- No packet bound as a `reviewed_packet` is edited; the two answered rfc0016
  packets keep their exact committed bytes.
- `scripts/verify_documentation.py` fails when a superseded packet claims to be
  pending, when a digest-bound packet's bytes no longer match its recorded
  `sha256`, and when a card's "live procedure" statement names a packet that a
  later version of the same family supersedes.
- `.agents/skills/bpr-attended-review/references/operator-packet.md` records the
  superseded state and the frozen-answered-packet rule, so the next regeneration
  has a convention to follow rather than a precedent to rediscover.

## Non-goals

- Do not regenerate any packet. Nothing in this card changes a bound executable
  digest, and the three live packets must remain runnable exactly as they are.
- Do not answer, infer, or placeholder any operator verdict.
- Do not rewrite the historical narrative inside a superseded packet. The banner
  goes at the top; the body is preserved as the procedure that was asked for.

## Verification

```sh
python3 scripts/verify_documentation.py --root .
python3 tests/scripts/verify_documentation_tests.py
ctest --test-dir build/core-only --output-on-failure \
  -R '^(documentation_structure|documentation_issue_graph_contract)$'
```

The verifier check must fail before the fix and pass after it; a check that only
passes proves routing, not the claim.

## Result — 2026-08-19

Base `8a44fb6`. Three commits: `8ffb581` claimed the card, `db8ff1f` repaired the
packets and the three cards, `07b7b34` added the guard and its tests.

### The guard, and what each rule was proven against

Every rule was run against a tree where its claim is false, not only against the
repaired one. Each negative control is a `git archive` of a real commit extracted
outside the working tree, so none of them touched a shared checkout.

| claim | command | proves it | result |
|---|---|---|---|
| a superseded packet cannot keep claiming pending | `verify_documentation.py --root <archive of 8ffb581>` | the pre-repair tree is the real failing case | fails: `appearance-scheme-foreground-v002 ... still claims to be pending` |
| an open card cannot name only a superseded packet | archive of `db8ff1f` with BPR-0220's pointer reverted to v004 | reproduces the exact defect this card opened on | fails: `names appearance-scheme-foreground-v004 but not the live ...-v007` |
| an answered packet's bytes cannot be edited | archive of `db8ff1f` with one word changed in `rfc0016-prerequisite-exception-v001` | the digest is the operator's authority | fails: `binds reviewed_packet at sha256:4a58fa... but ... is now sha256:6eeb60...` |
| the repaired tree passes all three | `python3 scripts/verify_documentation.py --root .` | — | `DOCUMENTATION_OK` |
| the rules hold in isolation | `python3 tests/scripts/verify_documentation_tests.py` | 8 new cases, including both accept cases | 86 tests, OK — was 78 |
| the registered gates still pass | `ctest --test-dir build/core-only -R '^(documentation_structure\|documentation_issue_graph_contract)$'` | the board's own contract suite | 2/2 pass |
| no whitespace damage | `git diff --check 8a44fb6..HEAD` | — | clean |

No C++ changed, so no `clang-format` was owed. The repository has no Python
format or lint gate to run.

### A digest sweep, and the one drift that is not a defect

Checking every artifact bound by a `validation-record.json` found all ten
packet-local bindings intact — which is the direct evidence that leaving the two
answered rfc0016 packets alone was right, rather than merely cautious.

One binding does not match: `official_validator` names
`.agents/skills/bpr-attended-review/scripts/validate_operator_packet.py` at 20,829
bytes, and the file is now 21,401. That is not drift to repair. Both records pin
`validated_commit: e36c2b4`, and the validator was deliberately changed afterwards
at `d348be9`. The record attributes a past validation to the tool as it stood
then. The guard therefore freezes only artifacts inside a record's own review
directory, and this case is why the boundary is drawn there rather than at "every
bound path".

### What this does not close

Nothing attended. The three live packets — `appearance-scheme-foreground-v007`,
`native-notification-delivery-v005`, `native-visual-acceptance-v003` — are
unchanged and still pending, and their bound executable digest is untouched:
every commit here is documentation, tests, or the verifier script, so no card in
this run could rebuild the binary. [[BPR-0114]], [[BPR-0168]], [[BPR-0220]],
[[BPR-0100]], [[BPR-0391]], and [[BPR-0392]] all remain open on the owner.
