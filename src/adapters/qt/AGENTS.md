# Qt Presentation Adapter Instructions

Qt owns windows, widgets, actions, models, focus, layout, accessibility,
themes, rendering mechanics, and event translation.

- Translate input into semantic intents; do not perform host effects.
- Apply command/status/model projections incrementally by stable identity.
- GitHub presentation code may receive only
  `bpr/application/github_projection.hpp`; provider, host, vault, transport,
  request, accepted-completion, and credential types do not cross into Qt.
- Device-authorization presentation receives only
  `bpr/application/device_authorization_projection.hpp`, which is a second
  narrow contract rather than an exception to the one above: it carries a
  stage, a user code, an already twice-validated verification address, a
  remaining validity, and the fate of the last browser handoff. Qt never
  constructs, parses, or opens an address — `QUrl` and a literal scheme are
  both refused here by `VerifyArchitectureBoundaries.cmake`, so the
  system-browser capability is host-owned and lives outside this adapter.
  The sole inert exception is the unnamed `const QUrl&` parameter required by
  `markdown_document::loadResource`: its exact guarded implementation returns
  a valid empty byte array for every resource and never inspects the address.
  Link destinations remain untrusted text gestures for application admission.
  RFC-BPR-0084's images keep that rule: the reader sends image sources as
  untrusted text and draws only the decoded pixels of
  `application::markdown_image_projection` through its own text-object
  handler; it never loads a resource, builds an address or receives one. The
  decoder (`markdown_image_decoder`) reads bytes the host hands it, by
  signature, and performs no effect.
- Public-preview presentation receives only
  `bpr/application/public_preview_projection.hpp` in addition to the ordinary
  GitHub projection. Its explicit unavailable facts and semantic gestures must
  not become provider, credential, transport, or effect access.
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
- The selected diff workbench admits results only for its exact current loading
  context. Failure, invalidation, and selection replacement clear measured
  geometry before any late publication can render.
- Retain one semantic index per accepted immutable diff. Ordinary scroll may
  project cached axis geometry and visible paths, but must not validate the
  full result, scan pane text, rebuild an unchanged axis, or sort all changed
  entries.
- Treat offscreen forced-style renders as deterministic layout evidence only;
  they do not verify system appearance integration.
- Treat `assets/dif/source/` plus the manifest as the editable Dif authority;
  never hand-edit `assets/dif/generated/` or use a generative prompt as the
  regeneration source.
- A Dif source, manifest, generator, generated-product, or preview change must
  regenerate into an empty directory, pass `dif_assets_contract` in dev and
  core-only, pass the selected-graph boundary verifier, and receive a new
  digest-bound attended verdict before runtime selection.
- Keep Dif resources, clip/frame state, and animation mechanics in Qt. Do not
  move them into core or the application host. Preserve the owner's requirement
  that the plus/minus prop remain separate from Dif's core base representation.
- Do not name a local, parameter or lambda capture after a `QWidget` or
  `QObject` member (`data`, `size`, `pos`, `rect`, `font`, `width`, `height`,
  `geometry`, `palette`, `style`, `layout`, `parent`, `children` and the like)
  inside a member function of a widget class. It hides the member, which MSVC
  reports as C4458, and the project builds with warnings as errors, so Windows
  stops compiling ([[BPR-1090]]'s `data` in `writing_hooks_panel.cpp`).
