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
  content. A chevron disclosure is a `disclosure_button`, which paints its
  bar, title and chevron from the palette's raised surface and text roles. A
  plain `QToolButton` with an arrow is drawn by the platform style, which on
  macOS is a native bezel in the system appearance's grey under the
  profile's text.
- The same bezel takes any auto-raise `QToolButton` or flat `QPushButton`
  outside a real `QToolBar`. Build those as `quiet_tool_button` or
  `quiet_push_button`: name a card's ground with `set_surface(Base)`, a
  ground no role holds (a code block's header) with `set_surface_colours`,
  and a one-colour mark with `set_tinted`. A popup is a window, and Qt gives
  a window the desktop's palette, not its parent's: once a profile is
  applied, every popup with a parent (a `QMenu`, a submenu) wears its
  parent's palette as it is shown (`popup_palette.hpp`, [[BPR-2356]]). A
  popup without a parent takes its opener's palette with every role set, and
  a popup that sets its own palette keeps it. `qt_button_faces` and
  `qt_button_faces_cocoa` measure every such button against its painted face.
- Preserve local expansion and selection across incremental accepted
  refreshes whenever the semantic identity still exists.
- No drawer ever pans sideways (owner, look workflow-v002: "This should be a
  rule by now"). Every drawer tab fits the drawer's narrowest width: labels
  wrap, a row of controls that may not fit is a `detail::wrapping_row`
  (`src/wrapping_row.hpp`), and a button naming something user-supplied
  shortens the name and keeps it whole in its tooltip. Turning a scroll
  area's horizontal bar off is not a fix: a trackpad still pans it.
  `qt_drawer_surfaces_fit_scale_100/125/200` checks every drawer tab of
  every surface at the narrowest and a wide width for a horizontal bar,
  sideways range and content wider than its viewport; a new drawer tab or
  drawer state joins that course in the same change.
- The right drawer is also held to that rule in the application's own
  window (owner, 2026-11-25, of a card that passed the Qt course and still
  panned: "This made the drawer have a horizontal scroll which should have
  been disallowed by our tests."). `application_composed_drawer_fit_scale_100`,
  `_200` and, on macOS, `_cocoa`
  (`tests/end_to_end/composed_drawer_fit_public_path.cpp`) drive the desktop
  composition with the Fixture agent through Comments, the Agent tab and the
  Tour tab's chat column, drag the drawer to 280, 300, 340 and 420 pixels in
  each state, and refuse a horizontal bar, sideways range, content wider than
  its viewport, a control outside the drawer, a button narrower than its
  words, a chip drawn over another, a button's face or a text field cut by
  a part that holds it, and a tool button the platform style draws. It also
  refuses a label or button drawn over what a scroll area scrolls, anything
  between the Agent tab's header row and its transcript, and the words
  RFC-BPR-0130 replaced ("Native continuation", "Transcript replay",
  "Native:", "Private history and runs") on any label, button or menu item,
  and a chat whose header is not the one `chat_header_view` row or that
  still shows a control the header replaced. It opens the side chat about a
  finding as well and judges that column at each width it can have.
  A state a real run
  reaches (an activity line, a suggested prompt, a refused question, a moved
  head) joins that course: a window nothing composed never shows it.
- A state is what the window shows between two gestures, not only where a
  flow ends. A course that presses on to its destination judges and draws
  only the destination: sweep the drawer after each gesture a person can stop
  at (the draft running, its proposal waiting before Apply all, a text being
  edited, a question asked before its answer, one part applied of two). The
  owner, 2026-11-25, of the proposal Draft Tour leaves, its four buttons cut
  to "dit tex" through several rounds of looks: "It still looks exactly the
  same this must be an offscreen vs on screen render issue at this point if
  we're this many iterations deep." It was not a render issue. The course
  had pressed Apply all on its way to the route, so the state the owner sat
  in was never measured or drawn, and every look showed a later one.
- A composed render is the window on screen. Held against the owner's
  screenshot of that proposal (the same state, a 280 pixel drawer, the
  display's own scale), the course's render drawn off screen under
  `BPR_NATIVE_LOOK=1` had every pixel equal but the drawer's one-pixel edge.
  When a look and the owner's screen disagree, find the state, the width and
  the scale the owner is at before suspecting the renderer, and hold the
  render against their screenshot pixel for pixel before saying it matches.
  Three things do differ, and the course settles each:
  - Scale. `QT_SCALE_FACTOR` multiplies the display's own scale, so on a
    Retina display a factor of 2 draws at 400%. Draw natively without it for
    what the owner sees (200% there), and with 0.5 for a 100% display. A
    render's name carries the scale it was really drawn at.
  - The active window. The window a person looks at is the active one: the
    selected tab reads selected and a default button wears its accent. The
    platform never makes a window active that is kept off screen, so the
    course tells Qt it is (`QApplication::setActiveWindow`) before it judges
    or draws.
  - The keyboard. A course presses buttons by program, which leaves the
    keyboard where no pointer would; when that control is hidden or
    disabled Qt hands the keyboard on as if Tab were pressed, and the next
    button wears a focus ring no pointer-driven person sees. The course
    takes the keyboard off its own leftovers (`leave_no_keyboard_behind`)
    and leaves it where the window itself puts it (a question's safe
    answer, a field opened to write in).
- The application hands an agent only the environment names its profile
  allows (`HOME`, `PATH`, the temporary folder), so a `BPR_FIXTURE_AGENT_*`
  variable set in a composed course never reaches the Fixture agent. A state
  that shows only while a turn runs is held open with a file: the course
  runs its own copy of the agent and, while `hold-turns` stands beside that
  copy, every turn waits (`course::hold_turns`, `release_turns`). A look is
  then as long as it takes, at any scale, and the turn ends when the course
  says so.
- A row of a part's quiet choices that ends in its one worded action is a
  `detail::wrapping_row` with `set_last_trails(true)`: the action keeps the
  row's right end on one row and when the row wraps. Icon tools the app
  already uses (the comments' pencil and trash) stand for Edit and Discard
  there, each a `quiet_tool_button` with its name first in its tooltip and
  as its accessible name; a choice that only opens another surface the
  drawer already offers has no button of its own.
- A chat's transcript keeps itself at its end (`stick_to_bottom`), so one
  taller than its viewport cuts a turn at the top, wherever that falls. It
  gets a
  `transcript_edge` as well: a hairline on the viewport's first row while a
  turn is cut there, and only then. Without it the cut turn reads as covered
  by whatever is above (owner, 2026-11-25, of a question cut under a line of
  the header: "partly hidden under the Native continuation line"); nothing
  lay over it. The composed course refuses a transcript that keeps its end
  and has no edge, one with a turn cut and no edge drawn, and an edge over a
  first turn that is whole. What a chat says of its session (carried over,
  cannot be picked up) is a quiet notice on the question field, never a line
  between the header and the transcript.
- A chat with an agent wears the one header row, `chat_header_view`, drawn
  from `core::plan_chat_header` (RFC-BPR-0130, header A): what the chat is
  about on one line that ends in an ellipsis, its place under it, then Show,
  History where the chat keeps sessions or Copy conversation where it keeps
  none, and Close. Which of them a chat has is core's decision; no chat builds
  a header row of its own, shows or hides a tool of the row, or adds a button
  to it (New session is History's first item). A session's cost and the
  revision it reads are in History, never on the chat's face.
- A chat with an agent places the one composer, `chat_composer`
  (RFC-BPR-0130 stage 2): the field, its chips above the words, the words,
  the chat's tools and Send, with Return, Shift+Return, Command+Return, the
  input method and the skill list handled there and nowhere else. Send and
  Stop are drawn from `core::decide_chat_send`; a chat enables Send and says
  in its tooltip what it does, and does not restyle, retitle or add a Stop of
  its own.
- A card the next question waits on, or that reports the turn at the thread's
  end, stands on the question field (`agent_tab_view::dock_banner`), not at
  the head of a thread that keeps itself at its end: the card that asks which
  revision to go on with, and the report of a run that stopped. Content of any
  length on such a card (the stopped run's log) is bounded and scrolls inside
  it, so the field and the card's choices stay in view. While a card asks for
  a choice, everything that would send is held to it the same way: a typed
  question is refused in place and a suggested prompt is greyed with what to
  choose first as its tooltip (`agent_suggestion_list::set_usable`), never
  left enabled to be refused after the press.
- A `QPlainTextEdit` that wraps at its width can still be given a few pixels
  of sideways range (Qt measures a block it lays out again as its widest
  line and some room more), which shows a horizontal scroll bar under words
  that all fit. A drawer's field of wrapped prose is passed to
  `detail::hold_wrapped_text_still` (`src/wrapped_text_field.hpp`).
- A chip that paints its whole rect (`quiet_push_button` with words) is laid
  out by that rect where chips stand side by side or one under another:
  `setAttribute(Qt::WA_LayoutUsesWidgetRect)`. The macOS style lays a push
  button out by its bezel, which is smaller than the widget, so without it
  each chip is drawn over the edge of its neighbour.
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
- A render shown to the owner is made the way main's `qt_button_faces_cocoa`
  draws: Cocoa with `Qt::WA_DontShowOnScreen`. Any Qt test executable of
  `tests/adapters_qt` does that for every window it shows when run with
  `QT_QPA_PLATFORM=cocoa QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM=1
  BPR_NATIVE_LOOK=1` (and `BPR_NATIVE_LOOK_SCHEME=dark` or `light`):
  QMacStyle, the system palette and fonts, nothing on screen
  (`native_look_startup.cpp`, `qt_native_look_*`). The owner's look of
  2026-11-10 ("some kind of theme disparity ... between what I see and your
  offscreen renders") came from offscreen renders: Fusion, always Light,
  another font database.
- A look of a control is drawn from the window that composes it, in a
  built-in theme applied with `appearance_profile::apply_to`, and saved under
  a directory named for the platform and style that drew it. Do not grab a
  part out of a bare panel, add a reference widget to the row being drawn,
  set a made-up palette or force the macOS style under offscreen: the row has
  no layout, a theme's `Highlight` is its pale selection colour and not the
  default palette's strong blue, and offscreen cannot draw Cocoa. The owner,
  2026-11-24: "your screenshot captures look busted, but the frozen build
  looks better than those." `render_drawer_filters` in
  `tests/adapters_qt/github_conversation_panel_tests.cpp` is the pattern. A
  window kept off screen is never made active, so a look that shows keyboard
  focus tells Qt the window is active first.
- A look of the right drawer (Comments, the Agent tab, the Tour tab and its
  chat) is drawn by the composed course above, from the application's own
  window with the Fixture agent: `BPR_COMPOSED_DRAWER_RENDER_ROOT`, under
  `BPR_NATIVE_LOOK=1`. The owner, 2026-11-25, of a tour chat drawn as a
  window of its own with canned turns: "No, doesnt look like your
  screenshots." The real column sits beside the tour in a drawer a person
  drags, and shows what only a run makes: its activity line, its prompts,
  its notices.
- On Windows every Qt test executable of `tests/adapters_qt` runs offscreen
  with the Windows fonts (`QT_QPA_FONTDIR` from `%WINDIR%` unless set) and
  Segoe UI 9 pt (`windows_fonts_startup.cpp`, [[BPR-2200]]). Without them
  offscreen falls back to an unnamed font twice as wide. Do not set fonts per
  test; a layout that only holds in one font is a product bug ([[BPR-2199]]).
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
- Do not name a local, parameter or lambda capture `data` or `d_ptr` in this
  adapter. It hides a Qt base class data member, which MSVC reports as C4458
  and the Windows build refuses, while Clang and GCC say nothing.
  `architecture_boundaries` refuses it on every host. A name that matches only
  a member function (`size`, `width`) does not warn.
