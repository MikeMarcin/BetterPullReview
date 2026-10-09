---
schema: "implement-rfc.card.v1"
id: "BPR-1092"
title: "Expose deterministic convention operations through the shared headless CLI"
status: "In Progress"
rfcs: ["RFC-BPR-0049", "RFC-BPR-0051"]
depends_on: ["BPR-1088", "BPR-1089"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-18","milestone":"hooks-cli","acceptance":["R36","R38","R39","R41"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"},"sequence_after":["BPR-1091"],"coordinator_amendment":{"date":"2026-10-08","decision":"BPR-1091's landed workshop wiring satisfies the dependency; its parked owner look does not block this card"}}
acceptance: [{"criterion":"R36","outcome":"pass","revision":"2f997516a","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0047-0051.md","command":"ctest --test-dir build/dev-tour-cli -R '^(tour_cli_labels|tour_cli_composition)$'","detail":"bpr tour labels adds no model, agent, network authority, publication or background work; counts use the existing reader's one read and are refused beside an owner (tour_cli_composition label_courses; control K11)."}},{"criterion":"R38","outcome":"pass","revision":"2f997516a","evidence":{"kind":"command","path":"tests/scripts/tour_cli_labels_tests.py","command":"ctest --test-dir build/dev-tour-cli -R '^(tour_cli_labels|tour_cli_composition)$'","detail":"The real bpr's catalog, 48 pack parser cases and eight label edits, and the fixture CLI's counts in three coverages and four requests, equal what the desktop composition's own core and application functions decide on the same fixture (--desktop-services); human text is the default and JSON explicit; cli_contract stays 1 (additive). Linux x86_64 Debug and GCC 15 -O3."}},{"criterion":"R39","outcome":"pass","revision":"2f997516a","evidence":{"kind":"command","path":"tests/scripts/tour_cli_labels_tests.py","command":"ctest --test-dir build/dev-tour-cli -R '^(tour_cli_labels|tour_cli_composition)$'","detail":"No display variables and the no-display platform; catalog and parse create no data root; edits and counts leave the profile's files unchanged except its own settings store on first read; edits go through the stop's owner (forwarded beside an owner process); comment drafts are capability_unavailable; no publisher. Control K1: a pattern of its own fails the pack cases."}},{"criterion":"R41","outcome":"pass","revision":"2f997516a","evidence":{"kind":"command","path":"tests/application/tour_cli_tests.cpp","command":"ctest --test-dir build/dev-tour-cli -R '^(tour_cli_labels|tour_cli_composition)$'","detail":"A label chosen against an older revision is revision_conflict (exit 4) with the newer text kept, standalone, after the other interface's edit and through an owner; a lost answer is outcome_unknown and its retry applies nothing twice; the same ID with another edit is command_refused; plain restores the bytes and undo is one step; partial coverage is exact false with limits, never a clean zero."}}]
---

# BPR-1092: Expose deterministic convention operations through the shared headless CLI

Pack alias: **PACK-CC-18** (live `BPR-1092`, allocated at import).

## Outcome and work

Inspect and extend the actual imported CLI grammar minimally for catalog, parse/preview, exact shared-draft edits and label-resolution breakdowns of already available data. Use current C++ services/owner handoff. Document actual command and schema additions; keep human output default and JSON explicit.

## Coordinator amendment (2026-10-08)

The coordinator ruled that [[BPR-1091]]'s landed workshop wiring satisfies
this card's dependency. BPR-1091 stays parked only for its owner look, which
does not block this card. BPR-1091 moved from `depends_on` to
`metadata.sequence_after`, as [[BPR-1089]] and BPR-1091 did. BPR-1088 and
BPR-1089 are Done.

## Acceptance

- CLI and desktop produce identical annotations/counts/draft results for the same fixture identity.
- Headless operation opens no GUI, creates no shadow store and uses no independent publisher/regex.
- Cross-interface edit conflicts, missing capabilities and unknown coverage return honest structured/human errors.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Implementation (2026-10-08, Linux)

Station bpr-main-linux (WSL2 x86_64, GCC 15.2.0, Debug `-Werror`), checkout
`/home/mmarcin/BetterPullReview` on `main`. Product commit `2f997516a`, run as `15c332485` (the landing rebase over
`44be70476` brought only BPR-1334's Qt line-details commits).
Engineering owner `rfc-implementer`, model `claude-opus-5-5`, effort `high`. The
[execution ledger](../../verification/RFC-BPR-0047-0051.md#cli-bpr-1092-bpr-tour-labels)
records the decisions and the evidence in full; the grammar and JSON are on
[the tour terminal page](../../development/tour-cli.md#feedback-labels).

### Commands and schema added

One subcommand family on the existing `bpr` executable, envelope
(`bpr.tour.cli-result.v1`, `cli_contract` still 1: additive) and owner channel
(protocol unchanged):

```sh
bpr tour labels [catalog] [--json]
bpr tour labels parse --file PATH|- [--published] [--json]
bpr tour labels edit stop:ID [--label ID|plain] [--expectation blocking|non-blocking|if-minor|none | --decoration TOKEN | --remove-decoration TOKEN] --expected-revision N --profile P --draft D [--request-id ID] [--json]
bpr tour labels counts [--columns ID,ID|none] [--mode open|all] --profile P --draft D [--transport native|gh] [--json]
```

- `catalog`: the twelve labels (`id`, `description`, `guidance`, `icon_role`), the
  three decorations, `catalog` `bpr.feedback-label-catalog.v1`, `catalog_revision`
  1, `parser_revision` 1. No profile.
- `parse`: `status`, `label`, `guidance`, `expectation`, `form`, `raw_label`,
  `header`, `subject` (each `begin`, `end`, `text`), `user_body`, `discussion`,
  `decorations` (`text`, `kind`, `begin`, `end`), `diagnostics`, `body_sha256`,
  `body_bytes`, `inspected_bytes`, `segment`. No profile.
- `edit`: the usual draft snapshot plus `label_edit` (`stop_id`, `replaced`,
  `replacement`, `annotation`) and `changed`.
- `counts`: the draft identities plus `counts` (`unit`, `columns`,
  `columns_source`, `mode`, `mode_source`, `generation`, `epoch`, `collection`,
  `exact`, `limits`, `tour_index_references`, rows `pull_request`,
  `not_on_a_file`, `folders`, `files`, and `quiet_files`, `quiet_folders`).
- `capabilities` lists `labels` and a `labels` object.
- New error codes: `invalid_label`, `invalid_decoration`, `invalid_edit`,
  `invalid_columns` (2); `unsupported_header`, `restructuring_required`,
  `capability_unavailable`, `counts_unavailable` (5). Existing ones reused:
  `revision_conflict` (4), `read_only`, `command_refused`, `revision_required`,
  `target_ambiguous` (2), `interaction_required` (3), `outcome_unknown` (6).

### Acceptance evidence

| Criterion | Result | Evidence |
|---|---|---|
| AC1: CLI and desktop give identical annotations, counts and draft results for the same fixture identity | Pass | `tour_cli_labels`: the real `bpr`'s catalog, 48 pack parser cases and eight successive label edits of one CRLF stop equal what the desktop's own functions decide (the chip catalog, `recognize_feedback_prefix`, `decide_feedback_composition_edit`), printed by the fixture CLI's independent `--desktop-services` mode; counts for three coverages and four chip/mode requests equal the desktop window's `feedback_counter_model` + `project_overview_feedback` projection row for row. |
| AC2: no GUI, no shadow store, no independent publisher or pattern | Pass | Every row runs with `QT_QPA_PLATFORM=bpr-no-display` and no display variables; `bpr` composes no GUI (unchanged `QCoreApplication` entry). Catalog and parse create no data root; edits and counts leave the profile's file list unchanged except the profile's own settings store on its first read. Edits go through the stop's owner (`tour_runtime::mutate`, forwarded beside an owner); no publication path or provider write is added. Control K1 (a pattern of its own) fails the pack cases. |
| AC3: conflicts, missing capabilities and unknown coverage are honest, structured and human | Pass | A label against an older revision, standalone, through an owner process or after the other interface's edit, is `revision_conflict` (exit 4, status `conflict`) with the newer text kept; a lost answer is `outcome_unknown` and its retry applies nothing twice; `comment:ID` is `capability_unavailable` (exit 5) naming the desktop; counts beside an owner are `interaction_required` (exit 3) and read nothing; a partial read or unknown resolution is `exact: false` with `limits`, a `counts_not_exact` warning and "not exact" in text, never a clean zero. |
| R36 | Pass | No model, agent, network authority, publication or background work is added; counts use the existing reader's one read. |
| R38 | Pass | AC1; human text is the default and JSON explicit; the additions are additive and documented. |
| R39 | Pass | AC2; owner handoff, request IDs and the tour store are the existing ones. |
| R41 | Pass | AC3; the label edit is one ordinary undoable `edit_stop` at the exact revision; plain restores the bytes. |

### Failing-before controls (each applied, built, run and reverted)

| # | Mutation | Fails at |
|---|---|---|
| K1 | `parse` reads the label with its own pattern | `tour_cli_labels`: pack case `strong-header` |
| K2 | `--published` reads from the first byte | `tour_cli_composition`: "a published stop's narrative label is read" |
| K3 | Catalog drops the icon roles | `tour_cli_labels`: catalog differs from the desktop's |
| K4 | Edit ignores the revision read | `tour_cli_composition`: "the stale label is refused and the author's text is kept"; `tour_cli_labels`: stale edit |
| K5 | A recorded request ID replays any edit | `tour_cli_composition`: "the same ID with another edit is refused" |
| K6 | `comment:ID` is an ordinary invalid field | both: `capability_unavailable` |
| K7 | Catalog admits the profile | `tour_cli_labels`: "the catalog created no profile" |
| K8 | Counts ignore the mode | `tour_cli_labels`: All row differs; `tour_cli_composition`: Settings All |
| K9 | Quiet zeros always exact | `tour_cli_labels`: "a file's zero under a partial read is not exact" |
| K10 | Counts ignore Settings | `tour_cli_composition`: "the Settings chips, in their order, and All are the defaults" |
| K11 | Counts read the provider beside an owner | `tour_cli_composition`: "counts beside a running owner are refused and read nothing"; `tour_cli_labels` |

### Gates

- Dev rows, 31 of 31 in `build/dev-tour-cli`, before the rebase and again at
  `15c332485`: the seven `tour_cli_*` rows
  (with the new `tour_cli_labels` and `tour_cli_loopback_provider`),
  `cycle_cli_public_path`, `cycle_cli_documentation`, `skill_pr_tour_package`,
  `skill_bpr_cycle_package`, `sqlite_tour_owner_channel`, the ten
  `core_feedback_*` rows, `application_feedback_counters`,
  `application_overview_feedback`, `application_overview_feedback_public_path`,
  `application_feedback_draft_store`, `application_label_authoring_contract`,
  `architecture_boundaries`, `architecture_boundary_contract`,
  `test_network_isolation_manifest`,
  `application_review_round_trip_composition_contract`.
- `core-only`: 15 of 15, `type_read_binding_contract` among them.
- Optimized tree: `app-release` with `BUILD_TESTING=ON` and the tour CLI (GCC 15.2.0 `-O3
  -DNDEBUG -Werror`), built at `37154853b` before the rebase (the same product
  sources): all 2326 targets, whose only warning is the vendored SQLite's; the
  eight new or extended rows (`tour_cli_labels`, `tour_cli_composition`,
  `tour_cli_process_commands`, `tour_cli_public_entry`, `skill_pr_tour_package`,
  `sqlite_tour_owner_channel`, `application_feedback_counters`,
  `application_overview_feedback`) pass there; the tree was deleted.
- `coding-format-check` with `/usr/bin/clang-format` 22.1.8; `DOCUMENTATION_OK`.
- Pins: no pinned source changed and nothing was resealed; `tour_feedback.cpp`
  and `.hpp` entered the tree inventory with a dated comment; the mutation pin
  did not move. `tour_cli_labels` is pinned "unmeasured (registered on Linux)"
  in `WINDOWS_UNCONTAINED_TESTS` ([[BPR-1343]]); `test_network_isolation_manifest`
  on Windows fails until the station measures it contained and records the
  refusal. The skill packages stay truthful unchanged (`capabilities` is
  their authority), so they were not edited.

### Limits

- Review comment drafts have no headless owner: `labels edit` labels tour stops
  only and says so for `comment:ID`.
- Counts beside a running desktop are refused, not forwarded to its window;
  standalone counts read the pull request once, as refresh does. No drill-down
  list is exposed.
- Desktop parity is against the core and application functions the desktop
  composition calls, on the same fixture, not a driven Qt window.
- Linux only. macOS and Windows runs are owed; no live GitHub, no model call.

### Station commands

macOS (arm64, the Mac station's checkout):

```sh
cmake --preset dev -B build/dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON
cmake --build build/dev-tour-cli
ctest --test-dir build/dev-tour-cli --output-on-failure -R '^(tour_cli_labels|tour_cli_composition|tour_cli_process_commands|tour_cli_public_entry|skill_pr_tour_package|sqlite_tour_owner_channel|application_feedback_counters|application_overview_feedback|core_feedback_partition|architecture_boundaries|test_network_isolation_manifest|application_review_round_trip_composition_contract)$'
```

Windows (bpr-main-win, PowerShell in the checkout):

```powershell
cmake --preset windows-dev -B build/windows-dev-tour-cli -DBPR_BUILD_TOUR_CLI=ON
cmake --build build/windows-dev-tour-cli
ctest --test-dir build/windows-dev-tour-cli -j4 --output-on-failure -R "^(tour_cli_labels|tour_cli_composition|tour_cli_process_commands|tour_cli_public_entry|skill_pr_tour_package|sqlite_tour_owner_channel|application_feedback_counters|application_overview_feedback|core_feedback_partition|architecture_boundaries|test_network_isolation_manifest|application_review_round_trip_composition_contract)$"
```

On Windows, first run `tour_cli_labels` contained (registered without
`WINDOWS_UNCONTAINED`, as BPR-1343 describes) and replace its
"unmeasured (registered on Linux)" pin in `scripts/verify_test_network_isolation.py`
with the measured refusal, or the manifest row stays red there. The other tour
CLI rows stop contained at "profile_unavailable" (`dos-volume-name`); this one
is expected to as well. On Windows `tour_cli_labels` skips its owner-process race (it uses POSIX
`select` on the owner's pipe); `tour_cli_composition` covers the owner race
in-process there, over the named pipe.

Next: the coordinator's board move. No attended review is required by this
card's criteria.
