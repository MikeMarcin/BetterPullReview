---
schema: "implement-rfc.card.v1"
id: "BPR-0932"
title: "Allow attended fixture Resume to replay the recorded pull request"
status: "In Progress"
rfcs: ["RFC-BPR-0024", "RFC-BPR-0026", "RFC-BPR-0032"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"report-followups-20260919","path":"docs/rfcs/plans/report-followups-20260919/milestone-plan.json"},"source_report":"report-followups-run-20260919"}
---

# Allow attended fixture Resume to replay the recorded pull request

## Owner report and provenance

The owner recorded `google/UIforETW#74` under Recently viewed, selected Resume,
and received “GitHub returned data that BetterPullReview could not safely use.”

The read-only source capture is
`/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/codex-clipboard-a281749a-0f58-43ae-b13b-e8349f4ef356.png`
(`sha256:37eb2d1357843027f10cebefa8d460a0d97b91544b001d4a5e7689f16ab6737e`).
The original report path and digest are recorded on [[BPR-0930]]. The
RFC-BPR-0037 attended bundle exposed this issue but did not own it.

## Diagnosed boundary

The signed-out attended process uses the strict UIforETW fixture. Initial load
consumes its one-shot anonymous recording. Recent preserves the exact public
mode, host, destination and saved position and correctly dispatches a second
open, but the exhausted fixture rejects the next `get_repository`. That strict
contract failure is deliberately normalized to the displayed fail-closed
message. Repeat sessions are enabled only for a separate probe, and the rewind
hook currently covers only account recordings, not the anonymous recording in
the capture. No production Recent identity or authorization defect was found.

## Acceptance

- Test-only attended fixture launch enables repeat read sessions, and the
  strict transport can rewind both anonymous and account recordings only at
  the exact `get_repository` session boundary; bootstrap remains one-shot.
- A synthetic attended public-preview course opens UIforETW, returns Home,
  resumes the same Recently viewed row and restores the exact destination and
  saved position without the malformed-response notice.
- Wrong owner/repository/number, request path, query, headers or body after a
  rewind still fails closed and no snapshot is accepted. Mismatched host,
  access mode or identity generation is refused before provider dispatch.
- Production Recent persistence, provider normalization, credential authority
  and publication behavior are unchanged. Focused restoration, anonymous
  preview and UIforETW selected-diff contracts pass with no live service use.

