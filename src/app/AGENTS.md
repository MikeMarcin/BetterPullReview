# Composition Root Instructions

This scope is the only place that knows the concrete core, host, capability,
Qt, and entry-point pieces.

- Make construction and destruction order visible.
- Keep build/platform diagnostics non-secret.
- Do not add hidden singleton initialization or service discovery.
- Keep self-test and render-evidence paths on the same public composition used
  by the interactive application.
- Keep the selected diff projection alive until diff, epoch, GitHub, and cache-
  actor work is quiescent; drain diff effects before epoch effects.
- A successful smoke test must traverse intent, transition, request,
  completion, and projection rather than calling helpers directly.
