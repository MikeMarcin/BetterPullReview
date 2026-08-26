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
- Keep worker-executor infrastructure standard-C++ and free of concrete
  provider or Qt ownership. An executor may run a host-owned effect closure,
  but it must never interpret, retain outside that queued closure, or mutate
  Qt objects, widgets, models, or presentation state.
- A successful smoke test must traverse intent, transition, request,
  completion, and projection rather than calling helpers directly.
- An automated-run stage waits on a condition it does not control; it does not
  fail on the first refusal. Readiness here is established by effects landing
  on the completion lane, so one tick cannot tell "not yet" from "wrong". The
  run budget is the bound, and a stage that waits must record what it is
  waiting for so an expiry names the condition rather than the stage.
