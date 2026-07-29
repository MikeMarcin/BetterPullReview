# Application Host Instructions

The application host owns effects, bounded observations, request execution,
serialized completions, cancellation, recovery, and lifecycle.

- Define narrow ports by capability and ownership boundary.
- Do not duplicate command legality, authority, navigation, or stale-result
  policy from the core.
- Workers and effect callbacks may not mutate core or Qt state directly.
- Marshal completions onto the decision lane as typed events.
- Keep shutdown order explicit and test pending work, cancellation, failure,
  reordering, and late completion.
