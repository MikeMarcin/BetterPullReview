# Application Host Instructions

The application host owns effects, bounded observations, request execution,
serialized completions, cancellation, recovery, and lifecycle.

- Define narrow ports by capability and ownership boundary.
- Do not duplicate command legality, authority, navigation, or stale-result
  policy from the core.
- Keep effect execution separate from the serialized decision executor. A
  worker/effect callback may post a typed completion to the decision lane, but
  must not run blocking platform, process, network, or persistence work there.
- Workers and effect callbacks may not mutate core or Qt state directly.
- Marshal completions onto the decision lane as typed events.
- Treat the decision lane as a required live service through host quiescence:
  accepted posts run exactly once, provider/vault submissions produce exactly
  one terminal, and cancellation does not suppress that terminal.
- A production host owns its capability cache. Do not inject caller-populated
  capability entries or treat caller-provided capability facts as
  observations.
- Credential resume accepts a profile only. Select an exact account authority
  from a trusted host-owned binding established by authenticated `/user`;
  never use login text, settings, or a caller-supplied account as a vault key.
- Keep shutdown order explicit and test pending work, cancellation, failure,
  reordering, and late completion.
