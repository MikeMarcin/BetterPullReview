# Qt Presentation Adapter Instructions

Qt owns windows, widgets, actions, models, focus, layout, accessibility,
themes, rendering mechanics, and event translation.

- Translate input into semantic intents; do not perform host effects.
- Apply command/status/model projections incrementally by stable identity.
- GitHub presentation code may receive only
  `bpr/application/github_projection.hpp`; provider, host, vault, transport,
  request, accepted-completion, and credential types do not cross into Qt.
- Do not place canonical GitHub, ledger, or semantic policy in Qt objects.
- Keep presentation wording and Qt-local state at this boundary.
- Test QAction translation, focus, accessibility, sizing, high DPI, rendered
  state, and the absence of whole-model resets.
