# Initial boundedness and performance contract

These are initial product budgets to implement and measure, not universal platform or
model limits. Use existing stricter admitted limits where required. Changes need a
named configuration/reason, compatible schema and tests; never silently truncate.

| Resource | Initial budget / behavior |
|---|---|
| source enumeration | 250,000 admitted entries per capture; paged observations, bounded core views |
| single captured file | 128 MiB; larger files remain visible as unavailable unless explicitly admitted by a supported policy |
| text/suggestion edit view | 16 MiB source display, 256 KiB replacement per suggestion; file-level feedback remains possible |
| per-item prose | 64 KiB UTF-8; edits beyond it refuse before commit |
| round feedback | 4,096 items, 16 MiB combined instruction/suggestion decoded payload |
| selected extra source context | 8 MiB default, 64 MiB admitted maximum; full instructions are never evicted for optional context |
| read page | 256 KiB target, 1 MiB maximum bounded page; expose total counts/digest/completeness |
| bundle | 10,000 entries, 512 MiB decoded bytes, 100:1 expansion ratio; reject encrypted/unsupported members |
| capture retry | at most three instability retries per explicit request, then `capture_unstable` |
| local retained-content budget | 2 GiB default soft quota with inventory; pins may force explicit refusal/adjustment, never hidden evidence loss |
| suggestion batch | at most 1,024 edits to one original blob; full conflict/precondition pass before any application |
| JSON nesting | at most 32; duplicate keys and nonfinite/floating values refused in canonical payloads |

Budgets apply to decoded bytes, not merely compressed/base64 size. Each refusal names
what exceeded the limit and preserves existing work. Cancellation is bounded by the
current chunk/operation; no partially published catalog entry. Export never quietly
shrinks the selection. Large scopes are browsable through pagination; this is not a
requirement to eagerly load every file into Qt or the functional core.

Measure cold/warm open, input latency, freeze/export, stable and changed-file recapture,
comparison and peak memory on named native machines. Use a synthetic 1-million-line
corpus with many small files, a 100,000-file tree and a stress round within the admitted
limits. Report hardware, build mode, bytes/files, warm/cold state and operation latency
p50/p95. Target p95 under 100 ms for note typing/navigation commands while capture
runs; operations longer than 250 ms need nonblocking progress and cancellation.
These are acceptance targets pending native measurement, not claims of current BPR
performance. Do not hard-code wall-clock unit tests to unstable CI timing.

Assess the user's benefit separately: compare the same eight-item/two-suggestion
assignment sent manually through chat with BPR's round. Count human steering turns,
copy/reconstruction steps, preparation time, omitted constraints, reviewer corrections
and rework. Keep model/harness settings, source/task and initial context recorded.
This is a small evaluation, not an experiment framework or a guarantee of token savings.
