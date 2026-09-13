# Implementation seams and ownership

Names marked **new** are suggested focused helpers, not assertions that those classes
already exist. Reuse a live equivalent if later work supplied one.

| Change | Existing owner / narrow integration seam | Keep out |
|---|---|---|
| Ten-clip source/canon | `scripts/dif_assets.py`; Qt asset manifest/source/generated tree; current asset tests | Runtime/build image generation, broad dependency changes. |
| Incoming-source cleanup | Shared updater with new BPR consumer project.md; maintainer intake/edited source records | Exact production inventory until approved; generated runtime strips as edit authority. |
| Body geometry | Extend existing runtime-geometry validator to separate body anchors and content envelopes | Relaxing all travel tolerances or recentering on a waving paw. |
| Compile-time clip descriptor | Existing generated header and Qt CMake/resources | Runtime JSON, directory scans, parallel asset framework. |
| `dif_sprite_catalog` / `dif_mascot_widget` / `dif_sequencer` (**new**) | `src/adapters/qt/`, one retained image cache and one widget per workbench | Domain state, effects, singleton. |
| `dif_cue_router` (**new**) | Qt-local projection consumer of already accepted compact facts | Raw model output, prose matching, finding validation, status authority. |
| Typed motion/visibility | `src/core/include/bpr/core/appearance_model.hpp` or the live typed appearance preference records | Clip/frame/timer values in core. |
| Persistence/fan-out | `appearance_preferences_application_host`, existing appearance runtime/store ports and SQLite migrations | Second preference owner, per-window conflicting copies, generic settings map. |
| Motion observation (**new where absent**) | Qt/platform read-only adapter, injected through real composition | Polling system commands, broad accessibility scan, silent OS setting mutation. |
| Home slot | `workbench_landing.cpp` and actual `workbench_window.cpp` Home composition | Replacing paste-to-open/queue/Recently viewed or clearing retained review. |
| AI slot | Existing Side Chat/Findings region of conversations drawer | Shared-comment avatar, persistent toolbar/status mascot or new AI launcher. |
| Process/run cue | `composition_agent_invocation_projection` in `github_application.cpp`, preserving existing `start`/`deliver_findings`/`finish` order and identity | A second process path, stream-token triggers. |
| Reply/proposal cue | Existing accepted chat delivery and `composition_comment_workshop_projection` | Treating stdout/process success as a presentable reply. |
| Local acknowledgment | Accepted workshop/draft outcome already presented to review editor, correlated to originating visible AI operation | Premature optimistic save confirmation or remote publication inference. |
| View comparison gesture | Accepted Qt activation of existing Findings view, once per eligible exposure | Every diff scroll/line selection event. |
| Shared cache/lifetimes | `profile_desktop.cpp` and concrete `github_application.cpp` composition; inject immutable resources to windows | Cross-profile cue/selection state or process-wide hidden global. |
| Build verification | Existing `verify_dif_asset_boundaries.py`, Qt/app resource graph, tests-disabled target and core-only graph | Blanket disabling symbol/resource checks. |

## Bridge shape

Consume accepted projections through composition, not by polling a window/model. The
new adapter receives the existing strong identity and only facts needed to select a
pose: explicit run started, accepted presented reply, accepted triage batch, terminal
failure/refusal, accepted local draft result, or a current continuation choice.
Continue using the existing host's identity and verdict, not a reconstructed substitute.

The bridge does not need review text, source code, credentials, prompts, tokens, or a
copy of finding contents. When possible pass bounded booleans/enums derived from the
existing accepted projection and borrowed identities. Map them to clip IDs only in Qt.
Do not emit/store a new domain event simply to drive a mascot.

A signature/type that lacks the necessary correlation is a real implementation gap:
carry that existing operation identity through its public projection, with focused
negative tests. Do not invent identity from row order, tab title, head OID alone, or
"latest result". No semantic cue is safer than a cue for the wrong request.

## Targeted regression boundaries

Motion/visibility changes must not rerun syntax classification; frame ticks must not
reset models, rebuild alignment/scroll mappings, relayout the window or reproject the
whole workbench. Keep source-pane, comment/provenance and scroll behavior tests focused
on the actual selected composition. Pixel changes inside the mascot rect do not by
themselves prove status correctness, and unchanged broad screenshots do not prove zero
background timers.
