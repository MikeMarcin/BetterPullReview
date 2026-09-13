# Current-source audit and planning assumptions

Source-derived facts below come from the September 12 BPR dump. This file separates
those facts from the new design. Exact dump content offsets and hashes are in
`references/SOURCE-BASELINE.json`; relative line numbers below are within the named
repository source file. Code and later issue decisions sometimes outrun narrative
README/RFC prose; such differences are recorded, not silently rewritten.

| Inspected source | Observed fact | Required implication |
|---|---|---|
| `scripts/dif_assets.py`, especially constants, `_validate_manifest`, `_validate_runtime_geometry`, `_validate_asset_inventory` | The compiler admits exactly `calm_idle`, `greet`, `rest`; fixed 256-square cells, source square multiples of 256, finite limits, exact inventory. | Ten clips require a deliberate schema/catalog/validator revision. Placing ten files in the directory will fail. |
| `src/adapters/qt/assets/dif/dif-sprite-manifest.json` | Current clips have 3, 5, and 1 source frames, not the newly proposed 12, 8, and 1. | Do not confuse current manifest timing/counts with the new artwork's counts. |
| Same root `README.md`; RFC-0020 Accepted prop-separation note | The older raster pack still contains the diff card, and its accepted note requires a prop-free base at runtime. | These new prop-free images need a new derivative revision; do not erase the card during paint or carry forward old verdicts by filename. |
| `scripts/dif_assets.py`, `_differing_generated_products` and `_digest_inventory_is_stale` (about lines 1118–1186) | PNG regeneration compares decoded canonical pixels, not compressor-specific PNG bytes. Other deterministic text compares byte-for-byte; output byte hashes bind the committed PNG payloads separately. BPR-0378/BPR-0408 settled this. | Preserve this distinction. Older prose and the prompt pack's blanket byte-for-byte regeneration language are stale on this point. No new DEFLATE/zlib pin. |
| `scripts/verify_dif_asset_boundaries.py`, marker policies | Current dev/core-only/release boundary assumes no product Dif resources anywhere yet. | Amend selected Qt dev/release checks to require only accepted compiled art, while core-only still excludes it and ordinary builds still exclude art tools. Do not delete the boundary test. |
| `src/adapters/qt/CMakeLists.txt`, widget inventory, `workbench_landing.cpp` | Asset tooling exists; the inspected selected Qt sources do not yet compose a Dif widget/cache/player. | This is real application composition work, not just swapping an already wired sprite. Re-inventory the live tree to reuse later work. |
| `workbench_landing.cpp` (`sign_in`, `choose_pull_request`, `back_to_review`); `workbench_window.cpp` Home construction | Home has a paste/open flow and may cover a still-open review; returning to review preserves position. | Visibility depends on the actually presented page, not `PR == null`. Preserve all existing controls, Recent items, focus and reading state. |
| `appearance_preferences_application_host.hpp`, `profile_desktop.cpp`, current app composition | Appearance preferences and fan-out are profile-owned; observations/publications are per attachment. Multiple windows/profile lifetimes already exist. | The old single-window/application-global wording cannot justify a new singleton or a duplicate preference owner. |
| `appearance_model.hpp`, `appearance_runtime_ports.hpp`, current settings/SQLite sources | The inspected appearance contract has no Dif/motion implementation; database migrations already reach version 5. | Introduce typed motion/visibility using the actual live schema next version; never assume the old version-3 baseline is current. Preserve existing palette, syntax, and font migrations. |
| `agent_invocation_host.hpp` | The projection has `admit`, `start`, `stream`, `finish`, and `deliver_findings`; identities include app session, authority, agent session, invocation generation. Delivery is already validated and occurs at most once per run before finish. | Reuse these exact identities/events; no polling logs, matching English strings, inferring a reply from process exit, or second provider integration. |
| `github_application.cpp`, `composition_agent_*_projection`, `composition_comment_workshop_projection` | Real invocation, findings, chat and workshop projection fan-outs exist. Side Chat and Findings are mounted through the conversations drawer. | A new presentation adapter can observe accepted facts here without moving domain policy into Qt. Exact callbacks are identified at import. |
| `side_chat_panel.hpp`, `side_chat_projection` | Session, provider, Git host, workspace, scope and continuation are explicit; the panel does not retarget to the latest head itself. | Dif is neither the chat author nor a replacement for those disclosures. Continue-against-old-revision stays visibly old. |
| `AGENTS.md`, live MASK project context | Focused slice checks, one integration pass at milestone/pack boundaries, reuse of valid evidence, no default exhaustive matrix/manual approvals. | This plan asks for explicit art and actual-app visual acceptance only where needed, not per-frame/per-card approval bureaucracy. |
| `docs/rfcs/plans/rfc-bpr-0029-closeout/milestone-plan.json` | The current repository uses `implement-rfc.milestone-plan.v1`, complete scope, work issue refs, dependencies, exit criteria and boundary. | The pack uses that observed shape with aliases that must be remapped to allocated live IDs before native validation. |

## New design, not an observed implementation

RFCs 0030/0031 propose the ten-clip catalog, 24-MiB pixel ceiling, two eligible
placement slots, profile-owned show/hide control, presentation cue policy, and the
new normalization/review tasks. The source does not prove these already exist.
The accompanying image audit is a measurement performed for this pack, not a BPR
runtime test or an art-approval verdict.

No live repository fetch/build/native app run was performed while authoring this
pack. The source dump does not identify the live checkout commit. The intake card
must resolve that once, including any new RFC allocations or in-flight ownership.
