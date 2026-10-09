---
schema: "implement-rfc.card.v1"
id: "BPR-1093"
title: "Verify syntax, ownership, migration and mutation adversaries"
status: "In Progress"
rfcs: ["RFC-BPR-0047", "RFC-BPR-0048", "RFC-BPR-0049", "RFC-BPR-0051"]
depends_on: ["BPR-1084", "BPR-1085", "BPR-1086", "BPR-1089", "BPR-1092"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-19","milestone":"verification","acceptance":["R40","R41","R42","R43"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"d154f5631","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0047-0051.md","command":"ctest --test-dir build/dev -R '^(core_feedback_.*|application_feedback_counters|application_label_authoring_contract|application_label_publication_recovery|application_agent_finding_workshop_injection_contract|application_overview_feedback_public_path)$'","detail":"All 78 pack fixture scenarios (48 parser cases, 4 generated families, 5 header edits, 2 counter cases with 28 units, 19 integration cases) map to a named product row and case in the card's traceability table; 5 also carry a recorded limitation (BPR-1095 authorized target x3, no headless comment-draft owner, whole-read coverage). Linux x86_64 Debug."}},{"criterion":"AC2","outcome":"pass","revision":"d154f5631","evidence":{"kind":"command","path":"tests/end_to_end/setup_destination_public_path_tests.cpp","command":"ctest --test-dir build/dev -R '^(application_overview_feedback_public_path|application_agent_finding_workshop_injection_contract|application_feedback_counters)$'; ctest --test-dir build/dev-tour-cli -R '^tour_cli_labels$'","detail":"Settings chips, drill-downs and a read receipt make no provider read or write (control K11); an agent candidate instructing publish/approve/resolve/setting/command/images stays a byte-exact candidate and acceptance is one local edit (K5, K6); hostile bodies are classified as data at the adapter and in bpr labels parse, over-bound refused never cut (K10); the core recognizer has no effect dependency (core_dependency_boundary)."}},{"criterion":"AC3","outcome":"pass","revision":"d154f5631","evidence":{"kind":"command","path":"tests/end_to_end/label_publication_recovery_tests.cpp","command":"ctest --test-dir build/dev -R '^(application_label_publication_recovery|application_feedback_draft_store|application_review_mutation_application_host)$'","detail":"Composed native session: a lost labelled publication cannot be relabelled while unresolved and Retry reconciles one stored comment (native_posts=1, duplicate=1, prepared bytes; K1, K2); a Publish during a pending relabel sends nothing and the next sends exactly the relabelled body (K3 shows the stale body otherwise); storage and the shared owner refuse a Publish bound to the pre-relabel revision (K4); a grant for an earlier request cannot carry a relabelled body (BPR-1082)."}},{"criterion":"R40","outcome":"pass","revision":"d154f5631","evidence":{"kind":"command","path":"tests/scripts/tour_cli_labels_tests.py","command":"ctest --test-dir build/dev-tour-cli -R '^tour_cli_labels$'; ctest --test-dir build/dev -R '^(application_feedback_counters|application_agent_finding_workshop_injection_contract)$'","detail":"This card's share: bounded untrusted parsing through the real bpr (64 KiB pathological body within 1,024 inspected bytes with the digest of every byte; over-bound, non-UTF-8 and NUL refused), 1 MiB headers at the provider adapter, and injected agent text shown as written with no browser, launch, setting or provider effect; rendering reuses qt_markdown_body (images become text)."}},{"criterion":"R41","outcome":"pass","revision":"d154f5631","evidence":{"kind":"command","path":"tests/sqlite_contract/workbench_settings_storage_tests.cpp","command":"ctest --test-dir build/dev -R '^(sqlite_workbench_settings_storage|application_comment_range_draft_store|application_feedback_counters|application_feedback_draft_store)$'","detail":"The v12 label migration under a second writer changes nothing and under interruption at every stage rolls back exactly, then migrates once keeping everything (K8); a label keeps a v2 range draft's record and suggestion bytes and a v1 draft's version (K9); an account swap reuses no annotation (K7c); cross-window stale Publish refused (K4)."}},{"criterion":"R42","outcome":"pass","revision":"d154f5631","evidence":{"kind":"command","path":"tests/end_to_end/label_publication_recovery_tests.cpp","command":"ctest --test-dir build/dev -R '^(application_label_publication_recovery|application_feedback_draft_store|live_review_mutation_provider|live_review_mutation_restart_recovery|application_private_review_retry)$'","detail":"See AC3: no duplicate post across a lost answer and a refused relabel; no stale-body publication after a label edit, in the composed path and in storage. Review events: application_review_submission_composition_public_path and live_review_submission_restart_recovery (reused)."}},{"criterion":"R43","outcome":"pass","revision":"d154f5631","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0047-0051.md","command":"ctest --test-dir build/dev -R '^(core_feedback_.*|application_feedback_counters)$'; ctest --test-dir build/core-only -R '^core_feedback_'","detail":"This card's share: the traceability table shows every label, syntax negative, identity and coverage case of the pack fixtures exercised by real C++ and composition tests, with the adversarial identity and coverage cases (repeated and contradictory pages, missing and deleted roots, account swap) added at the application adapter."}}]
---

# BPR-1093: Verify syntax, ownership, migration and mutation adversaries

Pack alias: **PACK-CC-19** (live `BPR-1093`, allocated at import).

## Outcome and work

Exercise real core/host/provider/store boundaries for malformed inputs, unknown roots/resolution, duplicate paging, stale edits, account swaps, migration conflicts and interrupted publication. Include prompt injection as untrusted data and native suggestion preservation. Reuse existing isolation harnesses.

## Acceptance

- Every fixture scenario maps to a real product test or a specifically recorded capability limitation.
- No unsafe effect is possible from parsing, settings, preview or local classification.
- Recovery tests prove no duplicate post/event and no stale-body authorization after a label edit.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Verification (2026-10-08, Linux)

Station bpr-main-linux (WSL2 x86_64, GCC 15.2.0, Debug `-Werror`), checkout
`/home/mmarcin/BetterPullReview` on `main`, claimed at `102b9ea21`. Engineering
owner `rfc-implementer`, model `claude-opus-5-5`, effort `high`. Test commit
`d154f5631`; no product source changed. The
[execution ledger](../../verification/RFC-BPR-0047-0051.md#verification-bpr-1093-syntax-ownership-migration-and-mutation-adversaries)
carries the same record. No live GitHub, no model call, no remote mutation.

### Acceptance evidence

| Criterion | Result | Evidence |
|---|---|---|
| AC1: every fixture scenario maps to a real product test or a recorded limitation | Pass | The traceability table below: 78 of 78 scenarios name a product row and case; five also name the limitation of the part no test here can show. |
| AC2: no unsafe effect from parsing, settings, preview or local classification | Pass | Composed: Settings' chips, drill-downs and a read receipt read and write nothing at the provider (control K11); an injected agent candidate stays a byte-exact candidate and acceptance is one local edit (K5, K6). Adapter and CLI: hostile bodies are classified as data, over-bound input refused, never cut (K7, K10). The recognizer is core with no effect dependency (`core_dependency_boundary`). |
| AC3: recovery proves no duplicate post or event and no stale-body authorization after a label edit | Pass | `application_label_publication_recovery`: one write across a lost answer, a refused relabel and Retry (K1, K2); a Publish during a pending relabel sends nothing and the next sends exactly the relabelled body (K3). `application_feedback_draft_store`: a Publish bound to the pre-relabel revision is refused by storage and the shared owner (K4). Reused: the BPR-1082 host grant row, the BPR-1085 submission course and `live_review_submission_restart_recovery` for events. |
| R40 | Pass (this card's share) | AC2's parsing and injection rows; rendering reuses `qt_markdown_body_*`. |
| R41 | Pass (this card's share) | The version-12 label migration under a second writer and interruption (K8); labels on range and single-line draft records (K9); account swap (K7c); cross-window stale Publish (K4). |
| R42 | Pass (this card's share) | AC3. |
| R43 | Pass (this card's share) | The traceability table, with adversarial identity and coverage cases added at the application adapter. |

### Pack fixture traceability

Every scenario in the pack's four fixture files maps to a real product test.
Five also carry a recorded capability limitation for the part no product test
here can show. Rows marked **new** were added by this card; the rest are reused
unchanged evidence from the card named.

| Fixture scenario | Product test (row: case) | Limitation |
|---|---|---|
| `parser-cases.json`, the twelve labels: `label-praise`, `label-nitpick`, `label-suggestion`, `label-issue`, `label-todo`, `label-question`, `label-thought`, `label-chore`, `label-note`, `label-typo`, `label-polish`, `label-quibble` | `core_feedback_convention`: the pack corpus loop, status, label, expectation and diagnostics per case (BPR-1076); `tour_cli_labels`: the same 48 cases through the real `bpr` against the desktop recognizer (BPR-1092) | — |
| Decorations and expectations: `blocking`, `non-blocking`, `conditional`, `custom`, `conflict`, `conditional-conflict`, `duplicate`, `note-guidance-conflict` | same two rows, required diagnostics included | — |
| Read forms: `strong-header`, `strong-label`, `strong-simple`, `ascii-case` | same two rows | — |
| Bytes: `crlf`, `bom-blank`, `unicode-subject` | same two rows; `core_feedback_header_edit` round trips keep CRLF and Unicode | — |
| `unknown-label` | same two rows | — |
| Malformed: `empty-subject`, `empty-decoration`, `empty-token`, `nested-decoration`, `unclosed-decoration` | same two rows; `core_feedback_convention` 20 malformed shapes with their diagnostics | — |
| False positives: `quoted`, `fenced`, `suggestion-fence`, `indented`, `tabbed`, `list`, `heading`, `html`, `later`, `inline-code`, `link`, `confusable`, `unsupported-strong`, `untagged` | same two rows; `core_feedback_convention` 24 wrappers per label | — |
| `root-with-different-body-line` | same two rows | — |
| Generated: prefix budget at the limit and one past it, no hidden truncation | `core_feedback_convention` budget cases and the 60 KiB subject; **new** `tour_cli_labels` adversarial: a 65,536-byte pathological body is read within 1,024 inspected bytes with the digest of every byte, and 65,537 bytes are `input_too_large`, never cut | — |
| Generated: large decoration counts and pathological delimiters, bounded work | `core_feedback_convention` ten 1 MiB shapes; **new** `application_feedback_counters` (`test_adversarial_provider_snapshots`): a 1 MiB header from the provider is unclassified, counted once | — |
| Generated: inherited tour and off-hunk spans, missing or ambiguous spans | `core_feedback_convention` and `core_feedback_header_edit` with the real `map_tour_stop` and the permalink offset; `core_feedback_tour_coexistence`; `qt_feedback_label_threads` (`test_a_tour_stop_is_plain_unless_its_author_labelled_it`); `tour_cli_composition` `label_courses` (unmapped envelope is `classification_unavailable`) | — |
| Generated: UTF-8 multibyte span conversion, stale draft revision refusal | `core_feedback_header_edit` (code point boundaries, `stale_revision`); `qt_feedback_label_controls` (editor index conversion); `core_feedback_draft_edit` (`stale_draft`); `tour_cli_labels` (`revision_conflict`) | — |
| `header-edit-cases.json`: `change-label-with-native-patch`, `remove-prefix-with-native-patch`, `change-decoration-empty-native-patch`, `unicode-prefix-edit`, `plain-noop` | `core_feedback_header_edit` ("pack header-edit cases: 5", every byte after the header); `core_feedback_suggestion_preservation` (the five through four edit paths, BPR-1084); **new** `application_comment_range_draft_store` (`a_label_edit_keeps_the_range_and_its_record_version`): a CRLF suggestion block in a stored range draft survives a label, a relabel, Plain and a restart | — |
| `counter-cases.json`: `cache-unresolved-issue-question`, `cache-all-issue-question`, and its 28 unit records | `core_feedback_partition` (all 28 records, the repeated page collapsing, both cases on every field, 32,768 subset projections, BPR-1078); `core_feedback_conversation` ("pack counter units: 27"); `application_feedback_counters` (BPR-1087 oracle rounds; **new** adversarial snapshots) | — |
| `native-integration-cases.json` `parse-preview-no-effects` | **new** `application_overview_feedback_public_path`: Settings' chips, pointer and keyboard drill-downs and a read receipt change no provider read count and make no submission, target observation, comment or resolution; **new** `application_label_publication_recovery`: writing and labelling ask GitHub nothing; `tour_cli_labels`: parse creates no data root; `application_feedback_draft_store`: compose, label, save and restart publish nothing and run no agent; `application_defect_correlation`: labels and resolutions never verify a defect | — |
| `issue-with-suggestion` | `application_label_authoring_contract`: the issue with a patch reaches the fixture server with its exact decoded body, path, side and line (BPR-1084); `core_suggestion_applicability` | The website's own rendering of a genuine native suggestion needs the authorized target: BPR-1095 |
| `suggestion-without-patch` | `application_label_authoring_contract` (exact prose, no mark); `core_suggestion_applicability` | — |
| `unsupported-native-patch` | `application_label_authoring_contract` (refused inline, one attempt on the exact line, no file post, text kept, marked text only); `core_suggestion_applicability` | — |
| `native-request-changes` | `application_review_submission_composition_public_path`: Request changes chosen after reading the list submits `request_changes` once (BPR-1085); `live_review_submission_restart_recovery`: a lost submission answer is recovered without a second event | The website's review state needs the authorized target: BPR-1095 |
| `approve-with-open-feedback` | `application_review_submission_composition_public_path`: Approve with my open blockers submits exactly the native approval, once, and is not overridden | — |
| `website-resolve` | `application_label_authoring_contract` phase 4 (the defect stays `needs_verification`, nothing submitted); `application_overview_feedback_public_path` (resolve through Refresh); `application_defect_correlation` | The resolve on github.com itself: BPR-1095 |
| `website-root-edit` | `application_label_authoring_contract` phase 3; `application_overview_feedback_public_path` (a website label edit moves the conversation, never in both buckets); `application_feedback_counters` (`test_a_website_edit_updates_its_own_scopes`) | — |
| `reply-label` | `core_feedback_conversation` (root identity, replies keep their own badges); `core_feedback_partition` (a reply relabel changes no count); **new** `application_feedback_counters`: a labelled reply under a root never observed is not promoted | — |
| `tour-coexistence` | `core_feedback_tour_coexistence`; `application_label_authoring_contract` phase 5 (BPR-1086) | — |
| `private-promotion` | `core_feedback_private_promotion` (BPR-1086) | — |
| `indeterminate-retry` | **new** `application_label_publication_recovery` `lost-relabel-retried`; `application_review_mutation_application_host` (`test_a_label_changed_after_preparation_needs_a_new_gesture`, BPR-1082); `live_review_mutation_provider` (`idempotency_and_indeterminate_attempts_never_duplicate`); `live_review_mutation_restart_recovery`; `application_private_review_retry` (three processes) | — |
| `no-agent` | `application_agent_finding_workshop_guidance_no_agent_contract` (BPR-1091) | — |
| `hook-override` | `sqlite_workbench_settings_storage` (`test_feedback_hook_override_migrates_from_fifteen_and_survives`); `application_writing_delivery` (version pinned to the default's digest); `application_agent_finding_workshop_guidance_custom_contract` (BPR-1090, BPR-1091) | — |
| `desktop-cli-conflict` | `tour_cli_labels` owner race and `tour_cli_composition` `label_courses` for the tour stop, the shared draft `bpr` owns (`revision_conflict`, newer text kept, BPR-1092); `application_feedback_draft_store` (`conflicting_edits`; **new** `a_publish_chosen_before_a_label_edit_never_prepares_the_stale_body`) for two windows and two store connections | A review comment draft has no headless owner, so `bpr tour labels edit comment:ID` is `capability_unavailable` (BPR-1092's recorded limit) |
| `rest-unknown` | `core_feedback_partition` (all-REST); `application_feedback_counters` (`test_coverage_is_explicit`); `tour_cli_labels` REST coverage | — |
| `partial-body-coverage` | `core_feedback_partition` (missing root, partial collection); **new** `application_feedback_counters` (a thread with no observed root is unclassified and its file's counts are not exact); `tour_cli_labels` partial coverage; `github_paged_read` scenarios `missing_cursor`, `repeated_cursor`, `truncated_total`, `unknown_completeness` and the partial GraphQL errors refuse the whole read | A page truncation or rate-limit cut is not a separate read-model fact: an incomplete read is refused whole, and "partial" means a collection was not read (BPR-1087's recorded limit) |
| `read-outdated-not-resolved` | **new** `application_overview_feedback_public_path` (a read receipt on the open issue leaves it open, counted and in its bucket); **new** `application_feedback_counters` (an outdated, unresolved thread keeps its bucket, state and file); `core_feedback_partition` (outdating changes nothing) | — |
| `privacy-authority` | **new** `application_agent_finding_workshop_injection_contract`; **new** `tour_cli_labels` instructing body (plain, nothing else happens); **new** `application_feedback_counters` instructing body (an issue like any other); `core_feedback_private_promotion` (no private metadata appended); `qt_markdown_body_*` (images become text, no resource is loaded) | — |

Counts: 78 scenarios (48 parser cases and 4 generated families, 5 header
edits, 2 counter cases with their 28 units, 19 integration cases), all 78
mapped to a product test. Five carry a recorded limitation beside their test:
`issue-with-suggestion`, `native-request-changes` and `website-resolve` for
BPR-1095's authorized target, `desktop-cli-conflict` for the comment draft's
missing headless owner, and `partial-body-coverage` for the read model's
whole-read coverage.

### New and extended tests

- **`application_label_publication_recovery`** (new row,
  `tests/end_to_end/label_publication_recovery_tests.cpp`). The real
  composition over the recorded GitHub.com fixture, signed in natively, with
  the durable private draft, the publication host, the GitHub adapter's
  review mutation session and its SQLite recovery journal. The fixture server
  stores what it is sent and can lose the first answer.
  - `lost-relabel-retried`: `issue (blocking): …` is labelled with the chip
    (nothing asked of GitHub), published, stored and its answer lost. The draft
    is held `publishing` with the prepared body. The chip's relabel to
    `question` is refused and the body cannot be typed into. Retry reconciles:
    `native_posts=1 submissions=2 stored=1 duplicate=1`, the stored body is the
    one prepared before the relabel attempt, and no `question:` body was ever
    sent. After publication a relabel sends nothing.
  - `relabel-before-publish`: Publish pressed in the editor and in the drawer
    while the chip's relabel waits for its owner sends nothing and says "Not
    published: your last change to this draft is still being saved." Once the
    relabel is accepted, a new Publish sends exactly `question: …`, once.
- **`application_agent_finding_workshop_injection_contract`** (new course in
  `agent_finding_composition_public_path.cpp`; the canary answers Check the
  premise with a candidate that tells the product to publish, approve, resolve
  every thread, turn the label helper off, run `gh api -X DELETE …` and load
  two remote images). The press carries the shipped guidance; the candidate is
  shown byte for byte with "Proposed by: Check the premise" and is read only for
  its label (`Label added: issue (blocking)`); nothing changes before
  acceptance; acceptance is one local edit of exactly its bytes, still
  drafting; one launch; the label helper stays on; no browser opens; no
  submission, target observation, comment or resolution.
- **`application_feedback_draft_store`**
  (`a_publish_chosen_before_a_label_edit_never_prepares_the_stale_body`): a
  Publish chosen from a view before another window's relabel landed is refused
  by storage and by the shared owner, the draft stays drafting with the
  relabelled body, and a new Publish prepares exactly the relabelled bytes.
- **`application_feedback_counters`** (`test_adversarial_provider_snapshots`),
  through the shared adapter and the kept model: an overlapping page repeating
  a thread counts once; a page disagreeing about a post's body is refused as
  `duplicate_post` and about a thread's resolution as `contradictory_duplicate`,
  with the kept counts neither replaced nor advanced; a thread whose root was
  never observed is unclassified, its labelled reply not promoted, its file's
  counts not exact; a deleted root leaves its issue bucket for the unclassified
  catch-all; another account on the same pull request reuses no annotation,
  nor does switching back; an outdated thread keeps its bucket; a 1 MiB header,
  a NUL-and-0xFF body and an instructing body are classified as data and the
  observation handed in is unchanged.
- **`sqlite_workbench_settings_storage`**
  (`test_label_migration_from_twelve_meets_a_second_writer_and_an_interruption`):
  while another connection holds the version-12 file for writing, opening it
  migrates nothing and every table, column and value is unchanged; stopped at
  each of eleven migration stages it rolls back to the exact version-12 file;
  the migration that runs keeps every other field and the window row, takes
  the label defaults, saves a label choice, and a second opener migrates
  nothing again.
- **`application_comment_range_draft_store`**
  (`a_label_edit_keeps_the_range_and_its_record_version`): BPR-1212's codec
  version 2 range draft with a CRLF suggestion block takes `issue (blocking)`,
  a stale relabel is refused by storage, Plain restores it byte for byte,
  it stays version 2 throughout and after a restart; a single-line draft stays
  version 1. Labelling publishes nothing.
- **`tour_cli_labels`** (adversarial section): 65,537 bytes are
  `input_too_large` with empty data; 65,536 bytes of a pathological header are
  `limited` or `malformed` within 1,024 inspected bytes with the SHA-256 of
  every byte; Latin-1 bytes and a NUL are `invalid_utf8`; a body telling its
  reader to run commands, publish and fetch an image is `plain`; no data root
  is created.
- **`application_overview_feedback_public_path`**: from the first Overview
  until the website edit, Settings' chips, the pointer and keyboard
  drill-downs, a read receipt and listing again leave `AccountReadCount`
  unchanged with no submission, target observation, comment or resolution; the
  read issue stays open and counted.
- **`test_network_isolation_structure_contract`** was red since BPR-1092's
  `tour_cli_labels` pin (`test_every_pinned_exemption_names_a_measured_cause`
  did not know BPR-1343's unmeasured pin; confirmed failing on a detached
  `102b9ea21` worktree). It now accepts an unmeasured pin that names Linux or
  macOS and still compares every measured cause exactly.

### Failing-before controls (each applied to the product source, built, run and reverted)

| # | Mutation | Fails at |
|---|---|---|
| K1 | `plan_review_publication`: a possibly transmitted write is attempted again instead of reconciled | `application_label_publication_recovery`: "the drawer shows the one posted thread in place of the draft" (`duplicate=0 posted=0`) |
| K2 | `review_draft_editor`: the body stays editable while publishing | same row: "the label chip refuses to relabel a draft whose publication is unresolved" (the editor showed `question (blocking): q…` while the accepted draft kept `issue (blocking): …`) |
| K3 | `github_application` `dispatch_review_draft`: a Publish no longer waits for the draft's change in flight | same row, `relabel-before-publish`: "a Publish pressed while the relabel waits for its owner sends nothing, and says so": the stale `issue: …` body reached the fixture server ahead of the relabel |
| K4 | `review_composition_application_host::change_draft`: a Publish is rebased onto the current revision | `application_feedback_draft_store`: "storage refuses the Publish chosen against the revision before the relabel" (the composed row still passes on the window's own guard, K3) |
| K5 | `agent_comment_workshop_host`: the candidate is cut to 96 bytes | `application_agent_finding_workshop_injection_contract`: `candidate-shown-as-written`, `accepted-draft-is-exactly-the-candidate`, `accepted-draft-still-drafting` |
| K6 | `github_application`: a delivered candidate is accepted at once | same row: `nothing-applied-before-acceptance` |
| K7a | `observe_feedback_sources`: a comment's deletion is dropped | `application_feedback_counters`: "a deleted root leaves its issue bucket for the unclassified catch-all, still unresolved" |
| K7b | `observe_feedback_sources`: a repeated thread is dropped silently | same row: "two pages that disagree about one post refuse the observation, naming why" |
| K7c | `feedback_counter_model` and the core refresh both ignore the account (two guards, both mutated) | same row: "another account on the same pull request reuses no annotation, and neither does switching back" |
| K8 | `workbench_settings_database`: an interrupted migration commits instead of rolling back | `sqlite_workbench_settings_storage`: the logical snapshot after the first interrupted stage cannot even be read ("complete schema and rows captured") |
| K9 | `core::apply` (revisioned draft): an ordinary edit drops the range | `application_comment_range_draft_store`: "issue (blocking) is written before the prose; the range, the anchor and every byte of the block stay" |
| K10 | `read_tour_text_input`: an over-bound body is truncated instead of refused | `tour_cli_labels`: the 65,537-byte body returned `recognized`, not `input_too_large` |
| K11 | `github_application`: an Overview chip change triggers Refresh | `application_overview_feedback_public_path`: "Settings' chips, the pointer and keyboard drill-downs reads nothing from the provider and asks it for nothing" |

K4 and K3 show the stale-Publish race has two independent guards: the window
refuses a Publish while the draft's change is in flight, and storage refuses
a Publish bound to an older revision. With only the window's guard removed
(K3), the race published the stale body, so that guard is the one standing
between a pending relabel and an old body.

### Gates

- Dev rows, 36 of 36 in `build/dev` at `d154f5631` (with the structure-contract
  fix): `application_label_publication_recovery`, the five
  `application_agent_finding_workshop_*_contract` courses and
  `application_agent_finding_composition_contract` (the canary changed),
  `application_feedback_counters`, `application_feedback_draft_store`,
  `application_comment_range_draft_store`, `sqlite_workbench_settings_storage`,
  `application_overview_feedback_public_path`,
  `application_label_authoring_contract`,
  `application_review_mutation_application_host`,
  `live_review_mutation_provider`, `live_review_mutation_restart_recovery`,
  `live_review_submission_restart_recovery`, `application_private_review_retry`,
  `application_review_submission_composition_public_path`, the ten
  `core_feedback_*` rows, `core_suggestion_applicability`,
  `core_review_preflight`, `architecture_boundaries`,
  `test_network_isolation_manifest`, `test_network_isolation_structure_contract`,
  `application_review_round_trip_composition_contract` and
  `github_recorded_fixture_contract`.
- `build/dev-tour-cli`, 6 of 6: `tour_cli_labels`, `tour_cli_composition`,
  `tour_cli_process_commands`, `tour_cli_public_entry` and both isolation
  contract rows.
- `core-only`, 16 of 16: `type_read_binding_contract`, the ten
  `core_feedback_*` rows, `core_suggestion_applicability`,
  `core_review_preflight`, both core boundary rows and
  `architecture_boundaries`.
- `coding-format-check` with `/usr/bin/clang-format` 22.1.8 (1,358 files).
- Pins: no pinned `src/app` source changed and nothing was resealed; the
  round-trip contract passes with every digest as it was; the mutation-branch
  pin did not move. The two new uncontained rows,
  `application_label_publication_recovery` and
  `application_agent_finding_workshop_injection_contract`, carry
  "unmeasured (registered on Linux)" in `WINDOWS_UNCONTAINED_TESTS`
  ([[BPR-1343]]); Windows must measure them contained and record the refusal,
  or `test_network_isolation_manifest` stays red there.

### Limits

- Linux only. macOS and Windows runs are owed; the Windows station must also
  measure the two new rows (above).
- The composed recovery course runs a native session; the GitHub CLI session's
  per-publication confirmation is macOS-only code (BPR-1329), so its window
  between confirmation and dispatch is not exercised here.
- A process restart between the lost answer and the retry is shown by
  `application_private_review_retry` for an unlabelled body; the labelled
  course's retry runs in one process with a fresh adapter session and a reopened
  SQLite journal.
- No fuzz target was added; the generated and pathological corpora above
  (40,000 recognizer bodies, 30,000 header edits, ten 1 MiB shapes) stand for
  it.
- Website behaviour (native suggestion rendering, review state, resolve) is
  BPR-1095's authorized-target course.
- Publish stays pressable while a relabel waits for its owner, because the
  editor's enablement follows the accepted projection; the composition
  refuses it with a reason (observed, not a defect).

Next: the coordinator's board move. No attended review is required by this
card's criteria.
