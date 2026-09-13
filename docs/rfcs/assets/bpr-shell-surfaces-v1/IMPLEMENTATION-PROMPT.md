# Implementation prompt

Use the installed MASK-managed `implement-rfc` skill to import and implement this
BetterPullReview shell-surfaces pack in the current BPR checkout.

Read the repository's current AGENTS and read-first documents and
`.mask/skill-config/implement-rfc/project.md`. Read `supporting/SOURCE-AUDIT.md` once;
inspect only the live seams needed to confirm its findings, since line numbers drift and
function names do not. Read `design/DESIGN-NOTES.md` and open
`design/shell-surfaces-v1.html`; treat them as the picture of the accepted layout, and
treat the two RFCs as the contract.

Use **`milestone-plan.json` as the authored execution seed**. Validate the pack, allocate
live issue IDs with BPR's allocator, map every `PACK-SHELL-*` alias in cards and plan,
and resolve RFC-number collisions before import. Preserve the immutable pack and write
the live mapping and plan lineage in the repository's supported import record. If the
skill's import interface has changed, adapt representation, not outcomes or grouping.
Do not manufacture an import receipt or invoke guessed commands.

Implement RFC-BPR-0032, RFC-BPR-0033 and RFC-BPR-0034 as additive revisions. They implement the
owner-accepted RFC-BPR-0028 Settings and Home statements and RFC-BPR-0018's row
language; they supersede the compositions of BPR-0646, 0652, 0660, 0650 and 0696 only
to the extent each RFC names, and no RFC text. Settled RFCs and historic attended
evidence stay immutable. Honor the live sequencing and frontier rules; technical
completion and canonical formal closeout are distinct.

**The core decides, the host performs, Qt presents.** Every card consumes projections
the hosts already publish and dispatches intents the hosts already accept. No widget
reaches a store, a vault, a provider or a process. No stylesheet, no hard-coded palette:
read `QPalette` roles and the accepted appearance. The code font is used for the device
code only. Where the design shows a fact the projection lacks, derive it or omit the
phrase; persist nothing new for presentation.

**Repoint before you delete.** The Accounts page's retired objects (host picker row and
its buttons, locator, status banner, public-preview strip, servers combo editor) and
the chooser's retired controls are selected by existing composed courses. Move every
course to its new target (the bar, the card, the strip, the sheet) and keep
`qt_composed_github`, the device public-path course, the saved-server contract and the
BPR-0696 management course green; then delete the widgets.

**Each theme keeps its own colours.** RFC-BPR-0034 bumps the appearance settings store
schema to one customised palette per variant. Migrate the existing single palette under
its recorded variant with nothing lost, key the profile's overrides by variant, and scope
commit, export, import and reset to the variant the composition resolves; Qt never guesses
it. Paint the six tiles and the type preview with the review window's own painter from the
real palettes; the diagonal is a clip, never a stroke.

**A close is a close.** Remove the last-runtime presentation in `profile_desktop.cpp`
and add one injected, testable empty-desktop policy: resident on macOS, quit through the
existing drain and Quit path elsewhere. Reopen from activation, ⌘N and the Profiles menu
through the existing intents with one runtime owner. Route failures with a live window
to that window; present the chooser for a failure only when no window exists.

Ordinary slices get focused checks. At each milestone reuse still-valid evidence and run
one relevant integration pass. Batch the explicit human criteria (A8, B5, C6) through the
shared attended-review workflow with absolute paths and one question per criterion;
preserve partial answers and keep independent work moving. A mockup is not a rendering
and a rendering is not a verdict. Windows and Linux quit behaviour is platform work on
the existing lanes; record it as not run until observed. No live GitHub or GHES
mutation; recording ports and fixtures suffice.

The completion report must distinguish, per surface: composed and reachable in the
normal executable; rendered evidence captured; keyboard and screen-reader walked;
attended verdicts bound to an executable digest; per-platform observations tested or
honestly unsupported; and any formal closeout wait.
