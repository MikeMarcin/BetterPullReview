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
  TeX math library; the owner asked for MicroTeX to be evaluated (below).

## MicroTeX evaluation, 2026-10-27

Candidate: `NanoMichael/MicroTeX`, MIT, C++17, its `openmath` branch (the
OpenType-math rewrite) at `086f4eb74`, built statically against the
product's Qt 6.12 in a scratch tree. It would live in `src/adapters/qt` as an
inline text object. Nothing was added to the repository.

For it:

- Builds cleanly in about four seconds; 2.1 MB static library plus a 49 KB
  Qt backend. FiraMath (SIL OFL 1.1) with its metrics file is about 450 KB.
- Good typesetting of fractions, roots, sums and scripts, inline in a
  `QTextDocument` through a `QTextObjectInterface`, light and dark.
- Fast: under 0.05 ms a formula after a 2 ms initialisation; two renders of
  each sample were pixel-identical.
- A formula reads no file: `\input` and `\includegraphics` draw as text.
  Only font loading opens files, from paths the caller gives.

Against it:

- Unmaintained: last push August 2024, no release beyond a `v0.0.1` tag,
  49 open issues.
- Crashes on hostile input: about 8,000 nested braces, or 4,000 nested
  `\frac`, overflow the main thread's stack (status 139). The product
  would have to cap length and depth before calling it, and fuzz it.
- `\color{…}`, the form GitHub templates use, is ignored by design outside
  array cells (`macro_env.h`); only `\textcolor` colours. The case that
  started this card still needs our reader, or a patch to a fork.
- Malformed input draws partially (`\frac{a}{` shows `a` over nothing)
  instead of being refused, unlike MathJax's error mark; an unknown
  command draws as text.
- `\rule{100000em}{100000em}` yields a 1.6-million-pixel-square box; the
  product would cap the size.
- One process-wide static context (`MicroTeX::init`/`release`).
- Inline placement needs baseline work: a text object sits on its bottom
  edge, so a formula with depth rides high.
- The `.clm` metrics come from `prebuilt/otf2clm.py`; a pinned
  acquisition must regenerate or vendor them.

Recommendation: do not adopt now. Keep the narrow reader, which covers what
templates write and leaves everything else as written. If drawn formulas
become wanted, adopting it is one card: pin `openmath` at a commit in a
fork, patch `\color`, wrap it in the Qt adapter behind length, depth and
size caps, render on the GUI thread, add a fuzz target, and ship FiraMath.
