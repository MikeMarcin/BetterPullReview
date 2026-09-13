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
| Bar as address field; first-launch card | `workbench_window.cpp` Home construction; `HomeOpenBar`, `HomeOpenAction`; `workbench_landing` states | Changing what the bar accepts or the `public_preview` stage |
| Strip controls on Home (Group, State headers, Filter, refresh) | The strip's existing action groups and the Home/review visibility switch; window-owned view state | Showing the controls signed out; a second toolbar row |
| Panels (**new** `home_panel`) and delegate (**new** `home_row_delegate`) | `HomeRecentList`, the queue landing's `QueueView` and `review_queue_model`; the core attention projection for order and reason | Urgency invented from timestamps; a second queue model; order by anything but state first |
| Group and nesting | A Qt-side grouping over the same model rows keyed by owner, repository or account identity from the projection | A new host query; persisting a grouped copy of the queue |
| Avatars and account chips | The existing avatar cache used by the comment rails; account identities from the session projection | Fetching avatars from Qt; a second cache |
| Filter menu | The surviving typed queue preferences (role, observer reason, quiet, sort) and `queue_presentation_preferences` store | A new preference; keeping Repository and Bucket as filters |
| Per-window view state | The window layout store RFC-BPR-0024 assigns window filters to | A profile-wide grouping preference |
| Status bar session line on Home | The existing status text path and the session projection | Polling; a second status owner |
| Empty-desktop policy (**new** `empty_desktop_policy`) | `profile_desktop.cpp` last-runtime branch; `main.cpp` quit policy; existing desktop Quit and drain | Timers; a tray item; per-platform branches hidden in widgets |
| Reopen from activation, ⌘N, Profiles menu | Existing `open_bpr_profile_intent` and `new_bpr_window_intent`; registry recency | A second runtime owner; touching another profile |
| Failure routing with a live window | The window's existing status projection and Profiles menu status | Presenting the chooser over a live window |
| Profiles sheet | `profile_management_window`, `profile_management_dispatch`, `profile_management_projection`; `qt_profile_management` course | A new intent; a launcher screen; profile deletion |
| Per-theme palettes | `appearance_settings_store` schema bump and migration; appearance host commit, reset, import, export intents scoped to a variant; `appearance_profile` override table keyed by variant | Guessing the variant in Qt; a parallel palette store; losing a customisation in migration |
| Theme strip | `theme_preview_strip` cards in a scrolling well, wider snippet, diagonal by clip; captions from the projection | A drawn diagonal; a separate thumbnail painter |
| `info_glyph` (**new**) | Shared by Appearance and Accounts rows; popover plus accessible description | Tooltip-only text |
| `type_preview` (**new**) | The review window's code painter over accepted typography and the resolved palette | A second renderer |
| `colour_role_list` (**new**) and preview bar | The 47 roles from `appearance_model.hpp`; the platform colour picker; the existing preview, commit and cancel intents | A new role; a hex field as the only entry; commit outside the existing intent |
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

The appearance migration must keep every existing appearance contract and the theme
customisation public contract green; a customisation lost in migration is a failed slice.
