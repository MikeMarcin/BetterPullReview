# RFC-BPR-0107 verification

Mandate `v0-5-0-prerelease-targets-r1`, milestone `share-agent-prompts`
([[BPR-2016]], [[BPR-2017]], [[BPR-2018]]). RFC_BASE_SHA:
`dc5453afa3ba0e1620fe30bf97b17211e09ba4bd`. Product candidate `7ae9b389c`.
Frontend-design was loaded before the Qt surface work. Owner amendments in
force: "If you need it you need it go build it."; "main should be the source
of truth for the tracker"; "Continue working unattended, you can file reviews
but dont block on their results"; and the owner's decision of 2026-11-11 to
export shipped defaults too, recorded in the
[RFC](../rfcs/RFC-BPR-0107-agent-and-prompt-sharing.md#owner-decision-export-the-shipped-defaults-too-2026-11-11).
Debug timings are nongating; nothing below is a timing claim.

## Criteria

| Criterion | Evidence | Result |
|---|---|---|
| AC01 export, import into another profile, equivalent behaviour after binding | `sqlite_agent_configuration_storage` (two profile roots, real stores, host): agents and prompts exported from one, imported into a fresh one after binding, read back after a restart equal in arguments, model and settings. Composed app courses below: the sender's Settings › Export configuration… writes the file; a second, fresh profile's Import configuration… binds both custom agents through Choose program… and imports them and three prompts | Pass (fixture and composed public path); owner look accepted |
| AC02 only selected portable material; exact text inspectable | `core_agent_configuration` and the storage course: no program folder, identity, numeric ID, run limit or preferred agent in the bytes; `qt_agent_configuration_dialogs`: the dialog shows the exact bytes it writes; the composed sender course checks the written file holds no program path | Pass |
| AC03 specific recoverable results | `core_agent_configuration`: too large, not UTF-8, malformed, unknown field, duplicate key, foreign format and newer version refuse the whole file by name; unsupported protocol, unrecognized purpose (`local.description`), duplicate portable ID or purpose, a value the agent validator refuses, invalid prompt text each listed per item and not applicable; an unbound custom agent or relative program is refused; the profile's 16-agent bound and duplicate names apply to the final list | Pass |
| AC04 preserve by default; replacement differences and conversations; relaxation per entry; concurrency and failure | Dialog defaults: a colliding agent and an edited prompt are left as they are (`qt_agent_configuration_dialogs`); Replace lists every changed field and says whether conversations stay continuable, with the identity rule of Settings' Edit (`core::keeps_agent_identity`, core and Qt tests); relaxed checks and builds off unless ticked for that entry; an edit through `bpr` between preview and application answers `changed_since` and changes nothing; a crash between the two settings files is undone from the journal (storage course) | Pass |
| AC05 no launch, network, credential or harness change | The host's effects are the file and the two stores; identities are minted locally. The storage course counts only identity mints; the composed recipient course shows both imported agents "Not checked". Detection after import reads metadata only, as before | Pass |
| AC06 keyboard, accessibility, restart, built-in restoration, skill references, native platforms | Every control is a native focusable widget with accessible names; Space operates the share row and a prompt's choice (`qt_agent_configuration_dialogs`); restart read-back (storage course); importing the shipped text leaves the default and Reset to built-in is unchanged; a missing `/skill` named in an imported prompt is said in the preview. macOS only; Windows and Linux not run | Pass on macOS, owner look accepted; Windows and Linux not run |
| AC07 the two RFC-BPR-0104 settings | Exported as requested (core round trip); builds off unless accepted, scratch Off applied, identity unchanged by either (core); no per-pull-request scratch choice is in the format (it has no field for one) | Pass |

## Owner look and closeout (2026-11-11)

Attended session `629d64c72746157219d2473892973d7f` was handed back with all
three candidates accepted and no notes: `BPR-2017-export-selection`,
`BPR-2017-import-preview-bind` and `RFC-BPR-0107-default-prompt-round-trip`.
Its host was stopped and its frozen root removed. BPR-2017 is Done; BPR-2018
closes the RFC as Auto-Verified on macOS evidence. AC06's Windows and Linux
runs were not made: they are recorded as not run, not as passes.

## Composed public path

Built `better_pull_review` (dev, tests on) at the candidate, offscreen, fixture
`nested-folders-v1`, each run in a fresh `CFFIXED_USER_HOME`:

```sh
BPR_ATTENDED_SHARE_CHECK=1 BPR_ATTENDED_CONFIGURATION_FILE=/abs/rt.json \
  better_pull_review --github-fixture nested-folders-v1 --attended-launch \
  --attended-open-file README.md --attended-share-look
# ATTENDED_SHARE_CHECKED:agents=2:said=Imported 0 agents and 1 prompt.

BPR_ATTENDED_SHARE_CHECK=recipient BPR_ATTENDED_CONFIGURATION_FILE=/abs/rt.json \
  BPR_ATTENDED_CONFIGURATION_PROGRAM=/usr/bin/true better_pull_review ... --attended-share-look
# ATTENDED_SHARE_RECEIVED:agents=2:rows=Work Claude / Opus (newest) · high effort / Not checked|Lint bot / /usr/bin/true / Not checked:feedback=imported:said=Imported 2 agents and 3 prompts. ...
```

The first adds two custom agents and edits two prompts through Settings' own
surfaces, exports every agent and prompt, improves the shipped Review feedback
in the file, imports it back, and checks the prompt is now the edited one
("Changed from built-in"). The second is a second profile receiving that file.

## Runs

`core_agent_configuration`, `sqlite_agent_configuration_storage`,
`qt_agent_configuration_dialogs` (new); `core_agent_entry`,
`application_agent_setup_host`, `application_review_skill_host`,
`application_workbench_preferences_consumers`, `sqlite_agent_setup_storage`,
`sqlite_workbench_settings_storage`, `qt_settings_window`,
`qt_agent_setup_view`, `application_composition_source_contract` (app pin
resealed; no grant change), `architecture_boundaries`,
`test_network_isolation_manifest`; `coding-format-check`; clang-tidy on the
new core, application, Qt and SQLite sources (naming warnings fixed). Native
renders (Cocoa off screen, light and dark) of the share row and both dialogs.
Not run: the whole dev suite, the core-only preset, Windows and Linux.

## Decisions made here

- Writing-hook provenance lives in `agent-setup.sqlite3` keyed by the SHA-256
  of the override it describes, so the workbench settings schema did not
  change; an override saved by an older build reads as unrecorded.
- A prompt the recipient has not edited takes the file's text by default; an
  edited one is kept until chosen. A colliding agent is left out until chosen.
- An agent that meets one already here is offered Replace and Use existing,
  never a second entry of the same name or popular agent.
- The scratch folder limit is a profile setting since RFC-BPR-0104's
  2026-11-11 amendment and is not exported, like the run time limit.

## Known limits

- Another open window of the same profile keeps its own agent setup in memory;
  a later edit there can write its older list over an import (true of any
  agent edit today).
- The composed courses are run by hand, like the earlier attended-look checks;
  they are not ctest rows.
