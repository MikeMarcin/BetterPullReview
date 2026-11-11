---
id: "RFC-BPR-0107"
title: "Share selected agents and prompts"
status: "Proposed"
metadata: {"origin":"absorbed 2026-11-10 from the local PR drafts pack v1, where it was RFC-DRAFT-0001","inspected_revision":"b12269ea9ad1ff07afe8ec31812875b89d870e5a","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"mandate_revision":"v0-5-0-prerelease-targets-r1","baseline":"dc5453afa3ba0e1620fe30bf97b17211e09ba4bd"}
depends_on: []
---

# Share selected agents and prompts

## Problem and grounded current behavior

An author can configure multiple agents, edit review and tour prompts, and name
personal or repository skills in those prompts. Repeating those settings on
another machine or sharing them with a teammate currently requires reconstructing
the configuration. The inspected settings expose no selective portable exchange.
The agent list has local identities, executable paths and explicit launch checks;
these cannot be copied as if they identify the same objects on another computer.
Each entry also carries a host-minted identity that owns its retained native
conversations, and editing an entry's protocol, program, arguments or strict-run
setting mints a new one. See [current behavior, S1–S3 and
S15](assets/v0-5-0-prerelease-targets-v1/local-drafts/CURRENT-BEHAVIOR.md).

The owner explicitly requested agent and prompt import/export as the first
independent slice. It must be useful in the existing application before any local
PR draft or enterprise integration is implemented.

## Proposed behavior and boundaries

### Selective export

Settings > Agents & context gains Import configuration… and Export configuration….
Both act on the profile whose Settings window is open. A build without agent
integrations has no Agents & context page and therefore no exchange.

Placement (2026-11-11, [[BPR-2051]], the owner's choice of the Agents roster,
design [record](../design/BPR-2051-agents-roster-and-footer/README.md)):
the two buttons share one row of the settings group under the agents roster,
between "Stop a run after" and "Remove every agent and set up again". The
row reads "Share agents and edited prompts", with Import configuration… and
Export configuration… at its right. The row is not shown until this RFC is
implemented.

Export lets the user select saved agent definitions and individual saved prompt
overrides. Version one covers the review skill and the writing hooks: tour Problem,
Solution, outline and stop comment, review feedback, and since [[BPR-2051]]
Enhance on a stop, a comment and a note (see "Purposes recognized by this
version"). A prompt the profile has not overridden is the shipped text and is not
an exportable item. A later RFC may add recognized purposes without making this
slice depend on them. Each prompt item names its purpose. An importer that does
not recognize a purpose lists that item as unsupported by its version, applies
none of it and lets the user apply the recognized remainder; it never stores the
item as an inert success. The local authoring prompts of the local-tour RFC are
such later purposes, and the tour-chat suggestions would be if Settings ever
makes them editable. Unsaved editor content must be saved or deliberately
excluded; export never silently substitutes it.

An agent definition carries a portable item identifier, display name, protocol,
preset kind where applicable, fixed argument arrays, check arguments, model,
effort, and requested strict-run-check setting. Neither the profile's numeric
agent ID nor its host-minted identity is portable or exported. A prompt carries
its stable purpose, text, and available built-in version/digest provenance.
Package identity/version and origin labels describe the export; they do not
establish publisher trust.

Use one versioned UTF-8 JSON file, at most 1 MiB, with no executable attachments.
Retain existing per-field admission: at most 16 active agents, names at most 64
bytes, at most 16 fixed arguments of at most 256 bytes each, the current model,
effort and check-argument checks, 8 KiB for each writing hook and 64 KiB for the
review skill. Existing agent and prompt validators remain authoritative. The
selected import must fit the final profile limits, not merely the file's
individual entries. Unsupported versions, unsupported required items, duplicate
portable identifiers and malformed values are reported before changes; do not
silently discard a setting and call the import complete.

The export replaces executable locations with binding requirements. Built-in
agent discovery can satisfy a compatible requirement; custom programs are chosen
on the recipient machine or mapped to an existing agent. Clone/worktree paths,
accounts, tokens, environment snapshots, review history, conversations and local
source do not enter the format. Neither do the profile's other agent settings:
the preferred agent and list order, the run time limit, clone folders and linked
clones, and whether suggestions or feedback guidance are shown. Literal arguments
and prompt text can themselves contain sensitive information: show their exact
export text for review/redaction and make no claim that the absence of credential
fields proves those strings safe.

Named skills remain references with their requested source qualifier. Their code,
supporting documents and installed packages are not copied or installed. Missing
skills are visible prerequisites. Import does not change the existing restrictions
on tools, settings sources or skill loading.

### Inspect, bind and apply an import

Import first presents the package contents, differences and missing local
bindings. For an agent collision, the user can map to an existing compatible
agent, add a separately named entry, or explicitly replace the chosen entry.
Settings offers a popular agent only while the profile has not added it, so an
imported Claude Code or Codex definition meeting an existing one offers mapping
or replacement, not a second preset entry. For a prompt collision, keep the existing text by default and offer comparison and
explicit replacement. Mapping to an existing agent does not update that agent's
configuration.

Replacement shows every changed field. It follows the existing edit rule for the
entry's identity: changing protocol, program, fixed arguments or the strict-run
setting gives the entry a new identity, so its retained conversations stay
readable history and can no longer be continued; changing only name, model,
effort or check arguments keeps them. The preview states that consequence before
the user confirms. A new entry is added at the end of the list with a new
identity and the standing of an agent just added by hand: nothing about it has
been checked on this machine.

Imported settings never silently relax a check. A definition that requests
relaxed run checks is imported strict unless the user accepts the relaxation for
that entry in the preview; popular agents stay strict as they do today.

Incomplete imported definitions remain in the import preview only. They are not
runnable agent entries and are not saved as a pending configuration: the package
file still holds them, and importing it again restarts their binding. The user
can apply a fully bound subset and leave the rest. Applying that selected
set is all-or-nothing for the saved settings it affects; a failure or crash must
not expose a partly replaced active configuration. Revalidate against concurrent
settings edits before application, including those made through the `bpr`
command line. Preserve unrelated settings and the order of existing entries.

Import, preview and application launch no program, sign-in check, skill, network
request or hook. Check agent remains a separate existing action. No harness
configuration is edited. Re-importing the same package offers comparison against
current values and provenance; it never automatically overwrites local changes.
Automatic distribution, update subscriptions and a command-line form of the
exchange are outside this slice.

### Allocation and delivery

Core owns bounded format semantics, validation, mapping and conflict decisions.
The application owns file reading/writing, persistence and serialized completion.
Qt presents selection, differences and local bindings using native controls. The
file is read and written with glaze, the JSON library already approved for core
([S18](assets/v0-5-0-prerelease-targets-v1/local-drafts/CURRENT-BEHAVIOR.md)); this RFC adopts no dependency and
permits no new general-purpose parser. The
[frontend-design guidance and native presentation
constraints](assets/v0-5-0-prerelease-targets-v1/local-drafts/UX-FLOW.md) apply.

This is a proposed design implementing the owner's confirmed scope, not an
already accepted screen design. There are no material product questions preventing
review of this slice independently. Exact control arrangement is implementation
design, with focused owner judgment of the eventual frozen candidate under the
repository's ordinary UX convention.

### Fitted to this pack's other decisions

These rules come from the [reconciliation record](assets/v0-5-0-prerelease-targets-v1/RECONCILIATION.md); its row numbers are given.

- **The outline prompt stays (X06).** [RFC-BPR-0106](RFC-BPR-0106-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md) removes Suggest an outline from
  the application and keeps the outline hook for the command line. The
  outline prompt therefore remains one of the six recognized purposes and
  stays exportable.
- **Two more agent settings (X17; confirmed by the owner, 2026-11-10).**
  [RFC-BPR-0104](RFC-BPR-0104-agent-scratch-folder-and-opt-in-builds.md) adds **Scratch folder starts** and **Build and run tests** to an
  agent's settings. Both join the exported definition as requested values,
  from this format's first version whether or not that RFC has landed.
  Build and run tests imports off unless the user accepts it for that entry
  in the preview, exactly as relaxed run checks do. Scratch folder starts
  imports as requested when it asks for Off, and as the recipient's own
  default otherwise. Neither changes an entry's identity. The scratch choice
  remembered for one pull request is local state and is never exported.
- **Overrides written against earlier guidance (X24).** [RFC-BPR-0100](RFC-BPR-0100-tour-conversation-that-changes-the-tour-in-one-turn.md) and [RFC-BPR-0102](RFC-BPR-0102-agent-questions-say-what-they-point-at.md)
  change built-in guidance and advance its versions. An override written
  against an earlier built-in version imports as before, and the preview
  names the version it was written against.

### Purposes recognized by this version (2026-11-10, [[BPR-2051]])

[[BPR-2051]] added three Enhance prompts as writing hooks in the same list,
store and profile revision as the others (Settings › Agents & context ›
Built-in prompts), so version one recognizes nine purposes, not six: the
review skill and eight writing hooks. Each hook's purpose ID is stable and
independent of its display name, enumeration position, text and version
(`application::writing_hook_purpose`, `writing_hook_of_purpose`):

| Purpose ID | Prompt | Built-in version |
|---|---|---|
| `tour.problem` | Problem statement | `review-preparation-v1` |
| `tour.solution` | Solution summary | `review-preparation-v1` |
| `tour.outline` | Tour outline | `review-preparation-v1` |
| `tour.stop-comment` | Tour-stop comment (Generate comment) | `review-preparation-v1` |
| `review.feedback` | Review feedback | `review-feedback-v1` |
| `enhance.tour-stop` | Enhance a stop | `enhance-tour-stop-v1` |
| `enhance.comment` | Enhance a comment | `enhance-comment-v1` |
| `enhance.note` | Enhance a note | `enhance-note-v1` |
| `review.suggested-comment` | Suggested comments ([[BPR-2059]]) | `suggested-comment-v1` |

[[BPR-2059]] (2026-11-10) added a ninth writing hook, **Suggested comments**:
how an agent writes the comments it suggests for the reviewer to post. It is
listed under the Agent tab group of Built-in prompts and rides every Agent tab
brief as `brief.comment_format.{source,version,text}`; the review skill
(bpr-version 5) defers to it. Version one therefore recognizes ten purposes.

Each built-in's digest is SHA-256 of its shipped text
(`application::default_writing_hook_digest`). The review skill's purpose ID
is this RFC's to assign. The store does not yet record which built-in version
an override was written against, for any prompt; the exchange that names it
in its preview (X24) adds that record with the import and export.

## Acceptance criteria

- **AC01:** A user exports selected custom agents and existing prompt overrides, imports them into another profile, and obtains equivalent configured behavior after explicit local binding without any local-PR or enterprise feature installed.
- **AC02:** The exported bytes contain only selected portable material; local agent IDs and identities, executable/workspace paths, unselected settings and account/history stores do not leak into the format. Exact literal arguments and prompt text are inspectable before export.
- **AC03:** Missing programs/skills, incompatible protocols, duplicate identities, unrecognized prompt purposes and limit/version failures produce specific recoverable results, with no runnable partial definition or silent truncation.
- **AC04:** Importing and re-importing preserve existing settings by default. An explicit replacement shows the actual differences and whether the entry's retained conversations remain continuable; relaxed run checks apply only when accepted for that entry; concurrent edits or storage failure cannot leave a partly applied active selection.
- **AC05:** Selecting, inspecting and applying an import produces no process launch, network operation, credential access or harness-configuration change; an explicit subsequent agent check remains distinguishable.
- **AC06:** Public Settings flows are usable with keyboard and accessibility, survive restart, and retain built-in restoration and prompt skill-reference behavior on supported native platforms.
- **AC07:** An exported agent definition carries both added settings; importing one that requests builds leaves builds off for that entry unless accepted in the preview, a requested scratch Off is applied, the entry's identity is unchanged by either, and no pull request's remembered scratch choice appears in the file.

## Proposed delivery

[Portable exchange](../issues/10-backlog/BPR-2016-portable-agent-and-prompt-configuration-exchange.md), [Settings flow](../issues/10-backlog/BPR-2017-selective-settings-import-and-export.md),
and [closeout](../issues/10-backlog/BPR-2018-close-rfc-0107.md) form the first milestone in the
[canonical plan](plans/v0-5-0-prerelease-targets-v1/milestone-plan.json). No later RFC is a prerequisite.
