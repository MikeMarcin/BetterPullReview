---
id: BPR-0467
status: In Progress
title: "Six card titles are not the YAML they look like"
rfc: RFC-BPR-0013
depends_on: []
---

# Six card titles are not the YAML they look like

Every card opens with a `---` fence, which is how a reader — human or
otherwise — is told the block inside is YAML. For 459 of the 465 cards it is.
Six carry a title with an unquoted colon:

```
title: Host mutation coordinator: safe target, idempotency, retries
title: Core verification evidence: what it may and may not conclude
title: Core: role classification, observer rules, and exclusions
title: Core: attention buckets, reasons, and queue navigation
title: Host: refresh scheduler, limits, offline state, and identity isolation
title: Notifications: metadata-only content and correct deep links
```

A YAML parser rejects each one with `mapping values are not allowed here` and
returns nothing for the whole block — not a wrong title, no front matter at
all.

This is not a convention being proposed. Fourteen other cards already quote a
colon-bearing title — `title: "Core: session identity, revision binding, and
staleness"`, `title: "Qt: extract the session chrome into a composable
panel"` — and the earliest of those was committed on 2026-08-16, the same day
as the earliest of these six. The convention exists and six cards missed it.

## What it actually cost

Nothing in this repository breaks, and that is the trap. `front_matter()` in
`scripts/verify_documentation.py` splits each line on its first colon, so it
reads `title` and the complete title text correctly, and every gate that
depends on it passes. The defect is invisible from inside.

It became visible from outside. A board-inventory pass in this session used a
real YAML parser and reported that `BPR-0164` "exists in `60-done` but is
missing the `id` field". The card has an `id`. The report was false, it was
about front matter, and it was one plausible step from being carded as board
drift — which would have been a second wrong answer built on the first.

That is the whole argument for repairing six lines: the block claims to be
YAML, and anything that believes the claim gets a wrong answer with no
indication that it is wrong.

## Required final state

- The six titles are quoted, so every front-matter block in the repository
  parses under a strict YAML reader.
- `scripts/verify_documentation.py` rejects an unquoted front-matter value
  that a YAML parser would read differently — concretely, an unquoted value
  containing a colon followed by whitespace. The rule lives in
  `front_matter()`, the single place front matter is read, so it covers RFCs,
  issue cards, closeout cards, and operator packets at once.
- The check adds no dependency. It does not import a YAML library, and the
  repository keeps its own parser; the rule only refuses the inputs on which
  the two would disagree.
- Negative proof: the check fails on the six before the repair.
- `tests/scripts/verify_documentation_tests.py` covers an unquoted colon
  rejected, a quoted colon accepted, an ordinary value accepted, and the
  `depends_on: [BPR-0001]` flow sequence still accepted.

## Non-goals

- Not adopting a YAML library, and no change to what `front_matter()` returns
  for input it already accepts. The parser is deliberately small; this narrows
  what it will accept rather than replacing how it reads.
- No card is renamed. Quoting a title does not change the title, and the six
  filenames, headings, and inbound `[[BPR-nnnn]]` links are untouched.
- Not a general YAML-conformance sweep of the repository's other `.md` front
  matter. All 504 front-matter blocks were checked and these six are the only
  ones that fail; if that changes, the new gate is what will say so.
