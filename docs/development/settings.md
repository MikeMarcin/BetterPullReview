# Settings

Open Settings from the application menu, the Settings button, or `Command+,`
on macOS (`Ctrl+,` on Windows and Linux). The title identifies the BPR profile
being edited. Windows in the same profile share one modeless Settings window;
opening it from another review brings that profile's window forward and supplies
the invoking review's context. Another BPR profile has separate preferences.

Choose a category in the sidebar. Each page scrolls within the window and keeps
its position when you switch categories. Tab reaches controls below the visible
area. Escape first belongs to an active editor, picker or confirmation; an
unhandled Escape closes Settings and returns focus to the invoking window.

To find a setting, type in **Search settings** at the top of the sidebar, or
press `Command+F` on macOS (`Ctrl+F` on Windows and Linux) ([[BPR-1289]]).
While the field holds text, the page list becomes the matching settings,
grouped by page in sidebar order and by section within a page. Each result
names the setting and, under it in smaller, quieter type, its page and section.
A page or a section that matches is a result of its own, on one line. The
letters the search matched are set heavier; where the section matched, it
rises to the name's ink. A setting is found by its name: as the search's text,
at the start of a word or inside one, or failing that as its letters in order
where each word they enter is entered at its start ("lnum" finds Code line
number text). It is also found by its section, or by its description where the
search starts one of its words; a description, which the list does not show,
counts only when no name, section or page holds the search. With nothing else
found, a small typing slip is forgiven. The best match starts chosen; Up and
Down choose another while the caret stays in the field, and Tab moves into the
list. Return or a click opens the result's page, scrolls the setting into view
and focuses it, and changes nothing; a setting that cannot take the keyboard
is shown and its page takes it. Escape clears the field, or, from the list,
returns to the field; with the field empty, Escape closes Settings as before.
When nothing matches, the sidebar says so and offers Clear. The search reads
the pages as they are shown, so a withdrawn page is never searched. The
ranking is the core's `fuzzy_match`; its provenance is in
[Ported code provenance](ported-code-provenance.md).

Ordinary changes apply immediately after their owning host accepts them; there
is no global Apply button. A refused change returns to the accepted value and
shows its reason. Closing Settings cancels an uncommitted colour preview or
pending import. It does not undo an accepted save or cancel a committed storage
operation. If a store is unavailable, only that domain loses durable changes;
where session changes remain possible, the page labels them session-only.

## General

New windows start on Home. Profile/window restoration is managed by the desktop
lifecycle, not by an editable startup preference on this page. The present
release-update panel is supplied by RFC-BPR-0035 and uses application-wide
preferences even though Settings belongs to a profile. It explains unavailable
platform or installation capabilities. See [Application updates and
recovery](application-updates.md) for the defaults, explicit release-access
account, checks, download, installation and cancellation behavior.

## Accounts & hosts

Each configured server has a card showing its connection state, account and
available access. Details contains capability, rate-budget and diagnostic facts.
Those facts describe the current session; they are not editable authority.
Credentials are never shown or copied by the card.

Use Sign in to open the dedicated sign-in flow. Supported GitHub.com device
sign-in starts the code flow directly; the code and browser action remain
reachable while waiting. Cancel discards that setup request. Manual token entry
remains available, including for enterprise servers whose device flow is
unavailable. GitHub CLI access, where supported, requires its own explicit trust
choice. A pending PR destination resumes only after its exact host accepts
authentication; cancelling setup keeps the existing review usable.

Add enterprise server validates and remembers the hostname and optional account
selection as local configuration. This does not grant access. Forget server
confirms removal of that configuration; it does not remove remote data or sign
out a session. Sign out removes the exact remembered device binding and its
vault token slots. Manual-token sessions are session-only; device grants can
survive orderly Quit and are revalidated on startup. Changing servers never
reuses another host's credentials.

Public preview is available on macOS for GitHub.com only. It has no authenticated
viewer or publishing authority. Linux and Windows report it unavailable; GHES
refuses it. CLI and native-device capabilities also depend on the selected
platform and configured host. The page reports these limitations rather than
silently selecting another transport.

## Reviews & queue

These choices change local review behavior, not GitHub assignments or
subscriptions. Saved preferences belong to the current BPR profile.

| Group | Default | Reset scope |
|---|---|---|
| Modality rules | Open on Overview; Continue enters Read | These two choices |
| Reading | Mark read by hand; return comparison Whole PR; comments All | These three choices |
| Drawers | Drawer controls Hidden | Drawer-control preference |
| Comment labels | Label helper off; Label marks Icons (or Letters, the two-letter shortcodes; [[BPR-1083]]), with a six-mark preview beside the choices; Overview counts Open; no Overview chips ([[BPR-1088]]) | These four choices |
| Queue defaults | Any role, observer reason, attention bucket and repository; any quiet/current state; core priority order | Queue filters and sort only |

Comment labels ([[BPR-1088]]). **Label helper** turns on the comment
editor's label chip completion and its leading `:shortcodes:` in every window
of the profile once the choice is saved; a label typed out in full is
recognized and counted either way. **Overview counts** is where a pull
request's Overview starts, Open (unresolved conversations) or All; switching
on an Overview changes only that view. **Overview chips** lists the twelve
labels: tick one to give it its own count in the Overview's Comments column,
and drag a ticked one, or press Alt+Up or Alt+Down (⌥↑ ⌥↓ on macOS), to set
the order. **Mark colours** opens Appearance › Colours with the filter set to
"label". The Comments drawer's own first filter, under Reading, is separate
and never changed by these rows. If the saved chips name a label this build
does not know, or one label twice, the row says how many were left out; the
rest keep their order, and changing a chip or Reset saves the list as shown.

Observer rules match local repository, author or path facts. Add/remove changes
the local rule and its matched membership. **Add repository** opens the same
native dialog as signed-in Home. Enter an HTTPS repository root or owner/name
for the displayed host, check the exact account and host, then choose Add
repository. The record belongs to that BPR profile and account. It starts the
bounded authored/requested discovery owned by RFC-BPR-0026; it changes no GitHub
subscription or review and opens no pull request. Saving succeeds only after
local persistence accepts the record. A duplicate has its own confirmation;
a refused save keeps the draft and accepted list for an explicit retry.

A different repository host requires explicit account/server preparation.
Preparation retains the draft; cancellation saves nothing, and returning from
sign-in requires checking the destination and choosing Add repository again.
Cancel or Escape discards the draft and restores the invoking focus. The list
and Remove remain in Settings. Removing affects the local saved record only;
failed changes preserve the accepted list. The open-PR Save Repository shortcut
continues to save its known repository directly through the same persistence
and result path.

## Appearance

System is the default mode. Explicit Light, Dark, High contrast system,
High contrast light and High contrast dark choices are also available. Interface and code
fonts start from the platform's respective native defaults; code tabs default
to four spaces, ligatures off, and bold/italic syntax emphasis on. Type controls
set family, size and weight independently for interface and code, plus interface
line height and code options. The preview reports the accepted values.

Each theme keeps its own customized semantic colours. Filter the colour-role
list and choose a swatch to preview. Keep changes saves that theme's palette;
Cancel or closing Settings discards the preview. Import validates a bounded
theme document before previewing it. Export saved theme writes the accepted
palette for the theme on screen, excluding unsaved previews. Theme files contain
colour roles, not credentials, review content or an application-data archive.

Reset appearance mode restores System. Reset type restores the platform-derived
font defaults and code options. Reset all colours clears only the current
theme's overrides. Other themes and unrelated preferences remain intact. Dif is
shown by default with System motion; reduced or unobservable system motion uses
the static pose. Its separate reset restores visibility and motion preferences.
Appearance storage failure permits labelled session changes and returns to
defaults after restart under the same failure.

## Notifications

Review notifications default enabled and not paused. These durable preferences
are separate from operating-system permission. Reset notification preferences
changes only enablement and pause; Reset queue defaults does not change them.

Preview for selected Home review uses the existing notification path and reports
whether a request was submitted. Submission does not prove that the system
displayed a banner. Payloads contain repository, review number, attention category
and unread count, without titles, source code or comment text. A notification
can activate only its exact accepted review. Native delivery and activation may
be unavailable or limited; the current tray implementation retains at most the
first actionable payload in a process because the platform callback carries no
per-message identity.

## Agents & context

Open a review to see the agent profiles its host permits. The profile, selected
private notes, private draft and existing-working-copy choice apply to the next
explicit request in that review; they are not saved as global defaults. Private
context is included only when selected. Start and cancel remain in the review's
agent panel. Settings cannot create an agent credential, broaden host policy or
publish an agent's output. Unsupported launch capabilities remain explained.

Under **Claude**, **Process wrapper** is saved for the profile ([[BPR-1301]]):
the full path to a program run in place of `claude` for every Claude request,
with Claude's own arguments, folder and environment and never through a shell.
Set it to the `claude` you installed (for example the path `which claude`
prints) or to a program that prepares your provider and then runs Claude.
Left empty, a launch runs the first `claude` it finds on the application's
`PATH` or, after that, in the usual install folders (`~/.local/bin`,
`~/.claude/local`, `/opt/homebrew/bin`, `/usr/local/bin`, `~/.npm-global/bin`;
on Windows `%USERPROFILE%\.local\bin\claude.exe`), and the row says which in a
read-only line: "Using /opt/homebrew/bin/claude (found on PATH)." or "claude
not found on PATH or in the usual install folders; set a wrapper." A saved
wrapper always wins ([[BPR-1301]], owner 2026-10-07). **Choose…** picks the file;
**Save** or Return saves the typed path; an empty field saved clears it. The
path is checked before it is saved and again before every launch, and a
refused path is not saved: the row says why in one line (not absolute, starts
with `~`, nothing there, a folder, not executable, and on Windows not an `.exe`
or a `.bat`/`.cmd`, which only `cmd.exe` runs). The **Sign-in** line says sign-in
is left to Claude and the wrapper; BetterPullReview never asks you to sign in
to Claude. See the [AI agent boundary](../architecture/ai-agent-boundary.md#claude-process-wrapper).

## Storage & privacy

The local data location is read-only. It contains repository caches, retained
history, drafts, notes, receipts and preferences. Relocation, all-profiles purge
and automatic retention editing are not available here.

Prune selected epochs and Delete repository cache use the current review's exact
accepted scope and require confirmation. Cancel is the default choice; accepted
maintenance follows its own cancellation boundary. Pruning can make an older
comparison unavailable. These operations do not change GitHub or a developer
working copy. They are separate from resetting preferences.

Help → Diagnostics raises this page while signed out. Profile capture controls
show Standard/Verbose and independent Anonymized/Include locally choices as
paired buttons. On/Off and the accepted Open logs/Clear controls remain nearby;
About local diagnostics explains recent context and recording limits.

Help also offers **Export anonymized diagnostics…** and **Export with identifying
details…** directly, without opening Settings. Each opens a compact frozen
preview with **Save…** and **Cancel**. Export options and exact records expand
on request. The identifying preview discloses irreversible capture omissions;
**Save with identifying details…** confirms that policy before choosing a file.
Nothing is uploaded. See the
[diagnostics guide](storage-and-diagnostics.md#preview-and-export-diagnostics).

Expand **Profile maintenance** for the destructive preparation controls below.

**Prepare diagnostic cleanup…** and **Prepare profile purge…** first warn that
the selected profile and all its producers will stop before an exact inventory
is captured. Preparation deletes nothing. The corresponding **Clear prepared
diagnostics…** or **Purge prepared profile data…** action requires confirmation
of that inventory. **Cancel maintenance** does not restart a stopped profile,
and cannot reverse an operation past its commit boundary. Shared registry and
updater data, sibling profiles, exports and documented empty roots/locks remain.
Purge is not uninstall or secure erasure.

The [storage, diagnostics and removal guide](storage-and-diagnostics.md) gives
the exact scope, recovery rules and residuals. Native vault/crash/full-removal
qualification remains with RFC-BPR-0038; visible controls are not evidence of
those pending courses. Uninstalling the application retains local state and
system credential entries. Sign out of remembered device sessions through
Accounts & hosts before removal if you want those bindings removed.

## Ownership and verification

The [authority inventory](settings-authority-inventory.md) identifies the typed
host, store, schema, reset and failure boundary for each group. The
[RFC-0018 closeout](../issues/60-done/BPR-0593-close-rfc-0018.md) records
which automated, native and attended claims have actually been verified. Later
Accounts/Appearance and update work has its own acceptance; this guide does not
turn those RFCs into closed work or imply release approval. The RFC-0014
foundation dependency is accepted; its remaining qualification is RFC-0038.
Owner amendment r2 closes the recorded Settings scope while BPR-0974 remains
open for the Windows complete native course and current inventory; on
2026-09-25 the owner chose its Defender disposition (lower the image's static
signals) and it is Ready. BPR-0975 is Done by a 2026-09-25 owner amendment
that accepts WSLg's refusal of application activation, and BPR-1122 (Blocked,
`upstream-wslg`) reruns the Linux full focus/lifetime course once WSLg ships
the fix. The finding is not declared a false positive, and neither the Linux
inventory-only pass nor the WSLg diagnosis course is a complete native course.

### Review preparation writing hooks

Review preparation instructions live under Agents & context → AI assistance.
Problem statement, Solution summary, Tour outline and Tour-stop comment have
separate profile overrides, Restore default, and effective source/version labels.
Save and Restore do not run an agent. Test saved hook uses the selected allowed
agent for one explicit, nonpublishing request in the current review; its result
is private and changes no tour field. An unavailable agent or source context
leaves manual preparation available. Overrides contain text instructions only;
they grant no tool, publication or broader editing authority.

#### Tool-free review-preparation delivery

The writing buttons use the currently selected agent profile. The bounded writing
path currently supports **Approved Claude stream JSON** with explicit empty tools,
a deny-all tool rule, an empty strict MCP configuration, no settings-source discovery,
bare mode and disabled hooks/session persistence. The response protocol must confirm
an empty tool/MCP list; tool blocks and finding payloads are refused. Other profiles,
including the Codex profiles, report writing assistance unavailable until their
protocol has an enforceable and qualified tool-exclusion boundary. Ordinary manual
writing and the separately explicit agent controls retain their existing behavior.
The application's flags do not change the developer's installed global configuration.
The CLI flag contract is documented in the [provider's CLI reference](https://code.claude.com/docs/en/cli-reference);
actual installed-provider and model-quality qualification is still required.

A request captures one selection (including whitespace), otherwise its whole field.
Empty Problem/Solution fields first recover an unambiguous existing description
section, byte for byte, without inference. Other empty requests need approved
evidence; an empty stop also needs its exact currently available pinned source.
Context is collected only on that gesture: bounded author description/comments,
private brief text, surrounding prose and the selected stop's code. No linked
resource is fetched. Input selection is capped at 6,000 UTF-8 bytes, supplied
evidence at 8,192, and the complete existing agent message at 16,384. Over-limit
requests ask for a smaller selection. Returned replacement text is capped at
16,384 bytes and the complete result at 32,768; extra keys, invalid provenance,
malformed UTF-8, duplicate or stale responses cannot alter the field.

One accepted result is one native undo transaction. Undo restores raw Markdown
and selection; redo uses received bytes and never runs the agent again. Private
Recovered/Inferred/Unclear notes stay outside the tour body. A saved-hook test
uses a synthetic sample bound to the open review and has no draft destination.
The outline hook returns body-free suggestions. Insert selected skeleton stops
from the Tour stops page, then visit and complete each current explanation against
its source. See [Prepare and maintain a review tour](review-tours.md).
