---
id: BPR-0108
title: Implement safe theme import and export
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0107]
---

# Implement safe theme import and export

Implement theme import and export over the semantic role vocabulary.

Import must reject unknown fields, URLs, includes, oversized files, and executable or style payloads. Each rejection needs its own case; a single catch-all that refuses everything would pass a weak test while accepting nothing useful.

## Implementation

`theme_exchange` supplies import and export over the closed role vocabulary,
decided in the Qt-free core and taking no third-party dependency.

The format is line-oriented and self-describing: a signature naming format and
version, then one line per role in vocabulary order, each an identifier and an
eight-digit lower-case hexadecimal colour with alpha. Export writes every role,
so an exported document is by construction a total palette.

Each refusal the RFC names is its own typed rejection carrying the offending
text and line, rather than one catch-all. That distinction matters: a validator
that refused everything would satisfy every refusal test while making import
useless, which is why a valid document round-tripping is a mandated check
sitting alongside the refusals rather than an afterthought.

## Refusals, and one classification worth keeping

Unknown keys are refused by name. A near-miss identifier is an unknown field
rather than a case-insensitive match, so a document cannot smuggle a value past
the vocabulary by changing case. A partial document is refused with each
unassigned role named in vocabulary order, rather than silently defaulted,
because the palette is total. Duplicate keys are refused rather than resolved
last-one-wins, since silent precedence is how a document hides a value behind
an innocuous one.

The external-reference scan runs **before any key is interpreted**, and a
reference hidden in a key position is refused as an external reference rather
than as an unknown field. That ordering is deliberate and worth preserving:
classifying a URL as merely unknown would be technically true and would lose
the security signal, and a scheme without an authority is still caught.

## Direct verification

- The theme-exchange suite passes with 68 assertions; the full Qt-free core
  suite passes 38/38 in 81 seconds; both architecture boundary suites, the
  format gate, and strict `clang-tidy` with `--warnings-as-errors` pass.
- Non-vacuity, unknown key: silently ignoring an unrecognised identifier fails
  with "a key outside the closed vocabulary is refused by name, not silently
  ignored".
- Non-vacuity, round trip: this one needed isolating. Shifting the exported
  colours tripped a format assertion first, because the harness exits on first
  failure and the expected-document literal is checked earlier. Perturbing a
  single alpha bit on the *import* side instead leaves the format valid and
  fails precisely on "an exported palette imports back to an equal palette".
  The weaker break would have proved only that something was wrong somewhere.

## Retained risks

- The format is BetterPullReview's own rather than an existing interchange
  format. That is a deliberate consequence of the core taking no third-party
  dependency, and the signature line carries a version so a future format can
  be distinguished rather than guessed at.
- Import takes bytes and returns a result; reading files, enforcing the byte
  bound against a real file, and choosing where themes live are host concerns
  that no slice owns yet.

## Result

Themes import and export over the closed vocabulary with each refusal typed and
named, and a valid document round-trips to an identical palette so the
validator cannot pass by refusing everything. BPR-0108 is complete.
