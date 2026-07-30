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
- Inherit the application and operating-system palette, style, and font. Do
  not force a palette or style, and do not add QSS. Refresh derived
  presentation values when Qt reports application palette, font, style, or
  theme changes.
- Keep connection, locator, pull-request, and conversation hierarchy compact
  and responsive. Secondary capability, rate-limit, and exact-identity facts
  belong behind disclosures rather than competing with the primary review
  content.
- Preserve local expansion and selection across incremental accepted
  refreshes whenever the semantic identity still exists.
- Custom delegates must retain native selection and focus painting, wrap
  remote literal text without elision, and derive colors and fonts from the
  current style option.
- Announce one combined, polite, deduplicated accessible status update for a
  published presentation change. Do not announce every field mutation.
- Treat offscreen forced-style renders as deterministic layout evidence only;
  they do not verify system appearance integration.
