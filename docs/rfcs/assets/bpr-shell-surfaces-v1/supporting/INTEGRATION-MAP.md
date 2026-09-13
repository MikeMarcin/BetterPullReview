# Implementation seams and ownership

Names marked **new** are suggested focused helpers, not assertions that those classes
already exist. Reuse a live equivalent if later work supplied one.

| Change | Existing owner / narrow integration seam | Keep out |
|---|---|---|
| Accounts page composition | `workbench_window.cpp` Settings composition; `settings_window::adopt_panel` | Composing the page inside `settings_window`; a second Settings authority |
| `server_card` (**new**) | `src/adapters/qt/`; consumes `account_card` facts, `device_authorization_panel` state, `github_session_panel` connection/capability/rate/diagnostic projections and their dispatch closures | Store, vault, provider or process access; a stylesheet; a hard-coded palette |
| Connect on a non-active card | Existing host-profile selection intent then the existing connect intent, dispatched through the composition closure | A new intent; selecting a host by string |
| Device sign-in in the row | `device_authorization_panel` state machine and `DeviceDispatch`; RFC-BPR-0016 public-path course | A second begin path; changing what authorization proves |
| Enterprise server cards | `saved_server_intent`, the queue enrollment host, `application_saved_server_contract`; existing confirmation path | Credential enrollment from a hostname; making tokens durable |
| Load refusal under the bar | `HomeOpenNotice` and the existing diagnostic text; the pull-request stack's empty page stays the review surface's | Showing the stack's empty page on Home; a second diagnostic source |
| Home column and bar heading | `workbench_window.cpp` Home construction; `HomeOpenBar`, `HomeOpenAction`; `workbench_landing` states | Changing what the bar accepts or the `public_preview` stage |
| Rows and delegate (**new** `home_row_delegate`) | `HomeRecentList`, the queue landing's `QueueView` and `review_queue_model`; the core attention projection | Urgency invented from timestamps; a second queue model |
| Filter menu and chips | The six existing typed queue preferences and `queue_presentation_preferences` store | A new preference; losing a preference behind the menu |
| Status bar session line on Home | The existing status text path and the session projection | Polling; a second status owner |
| Empty-desktop policy (**new** `empty_desktop_policy`) | `profile_desktop.cpp` last-runtime branch; `main.cpp` quit policy; existing desktop Quit and drain | Timers; a tray item; per-platform branches hidden in widgets |
| Reopen from activation, ⌘N, Profiles menu | Existing `open_bpr_profile_intent` and `new_bpr_window_intent`; registry recency | A second runtime owner; touching another profile |
| Failure routing with a live window | The window's existing status projection and Profiles menu status | Presenting the chooser over a live window |
| Profiles sheet | `profile_management_window`, `profile_management_dispatch`, `profile_management_projection`; `qt_profile_management` course | A new intent; a launcher screen; profile deletion |
| Retired widgets | Delete after every composed course names its new target (`qt_composed_github*`, device public-path, saved-server, management courses) | Deleting first and repointing later |

## Bridge shape

Every card consumes projections the hosts already publish and dispatches intents the
hosts already accept. Where a signature lacks a fact the design shows (for example the
"resets in 38 min" of the rate limit, or a profile's window count), the implementer
either derives it from the existing projection or omits the phrase; nothing new is
persisted for presentation.

## Targeted regression boundaries

Repointing courses is the risk: the selector traps on the board (composition, not
subsystem) apply. `qt_composed_github` builds the same window through the same
drawer, so every Accounts change runs it. The device public-path course must prove
the same authorization facts after the move. The management course of BPR-0696 must
keep passing with the last-runtime presentation removed and the policy injected.
