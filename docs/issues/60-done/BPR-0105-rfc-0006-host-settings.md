---
id: BPR-0105
title: Implement host appearance persistence and caches
status: Done
rfc: RFC-BPR-0006
depends_on: [BPR-0104]
---

# Implement host appearance persistence and caches

Persist and restore appearance mode, semantic colors, and typography through a narrow settings port, and hold the host-side caches the RFC requires.

A theme or color change must invalidate presentation caches without touching review semantics. Token spans cache by blob and definition version independently of the active color theme, so switching theme cannot re-run lexical parsing.

## Implementation

The appearance host persists and restores mode, palette, and both font records
through a narrow settings port, holds the token-span cache, and publishes
bounded presentation invalidations.

The cache key is exactly blob identity plus definition version. The active
theme is deliberately not part of it, which is what makes the RFC's "switching
theme or colour does not re-run lexical parsing" criterion structural rather
than behavioural: a theme change cannot invalidate the cache because the theme
was never in the key. That is a stronger guarantee than remembering not to
evict.

A persisted record the core rejects is refused whole. The host neither keeps
the legal half of an illegal record nor substitutes a clamped value, and it
forwards the core's exact typed rejections to presentation rather than
flattening them to a generic failure. Silently clamping a persisted value would
leave the user with settings they never chose and no indication why.

## Direct verification

- The appearance host suite passes 3/3; the full application suite passes 25/25
  in 237 seconds; both architecture boundary suites pass. Format and strict
  `clang-tidy` with `--warnings-as-errors` pass.
- Non-vacuity, theme independence: making a palette change clear the cache
  fails with "an appearance change evicts nothing". This is the acceptance
  criterion's mechanism, so it needed to fail for the right reason rather than
  merely fail.
- Non-vacuity, rejected persistence: adopting a record the core rejected fails
  with "a record the core rejects is refused whole".
- Both were reproduced independently after delivery, each with the build
  confirmed to have recompiled the changed object rather than merely
  succeeded. The first build of the untouched tree reported `ninja: no work to
  do`, which would have run a stale binary, so the file was touched to force a
  genuine recompile before any result was believed.

## Retained risks

- Lexical parsing is stubbed by a test fake; BPR-0110 supplies the real
  classifier. The cache keying and its parse counter are proven here, but the
  parse-count assertion over a real parser belongs to that slice.
- Persistence is proven through the port with a fake durable store, inheriting
  the same open item as the workbench host: real durability needs a backing
  adapter.

## Result

Appearance settings persist and restore with rejections surfaced exactly, and
the token-span cache is keyed so a theme change cannot invalidate it by
construction. BPR-0105 is complete.
