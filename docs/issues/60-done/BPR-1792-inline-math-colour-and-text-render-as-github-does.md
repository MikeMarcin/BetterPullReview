---
schema: "implement-rfc.card.v1"
id: "BPR-1792"
title: "Inline math that reads as styled text renders as GitHub does"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1792--BPR-1792--c780aa619633.json","kind":"bug","found_by":"owner chat, 2026-10-27","related_rfcs":["RFC-BPR-0036"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "9c92233215e5d3f574df865e22fff7ff27798e5f", "evidence": {"kind": "command", "detail": "qt_markdown_github_1_0, _1_25 and _2_0 (check_math on fixture github-parity-v1/math, with GitHub's Markdown API rendering beside it): GitHub and the reader take the same six spans for math; a coloured text formula reads as its words in the named colour, a summary's too; textcolor with textbf is bold in the hex colour; the dollar-backtick form reads the same; math mode drops spaces and sets letters italic; a fraction stays as written; math in a code span stays code; a price is not math; no mark is left; a negative control shows Qt's import prints the TeX."}}, {"criterion": "AC2", "outcome": "pass", "revision": "9c92233215e5d3f574df865e22fff7ff27798e5f", "evidence": {"kind": "command", "detail": "Every qt_markdown and qt_pull_request_overview course passes (21), including qt_markdown_github_html and qt_markdown_body."}}]
---

# BPR-1792: Inline math that reads as styled text renders as GitHub does

## Request

The owner, 2026-10-27, with a photograph of a pull request template whose
first line and a `<details>` summary were written as inline math that sets
coloured text:

> There's a formatting issue with our markdown rendering compared with
> GitHub this text shows as formatted. Fix this do not record this exact
> text in our repo but reproduce a similar situation

## Cause

GitHub renders `$…$` (and `` $`…`$ ``) with MathJax. The reader had no
inline math, so it showed the TeX source.

## Built

- `markdown_github_math`: GitHub's inline delimiter rules, and a reader for
  the TeX that is styled text: `\color`, `\textcolor`, `\text`, `\textbf`,
  `\textit`, `\texttt` and their kin, `\mathrm`-style commands, groups,
  spacing and escaped characters, and letters (italic), digits and
  punctuation in math mode. Anything else is refused whole and stays as
  written.
- Preparation turns a formula it reads into its text between the reader's
  inline marks, every character a Markdown escape. A colour mark carries the
  colour, kept through the summary styling.
- The fixture `math.md` reproduces the situation in new words, with
  GitHub's own rendering `math.github.html`.

## Not done

- Fractions, scripts and symbols still show as written. Drawing them needs a
  TeX math library (MicroTeX, MIT, is the candidate), which is the owner's
  call.
