# BPR-0970 — Parser and mapping fuzzing

[[BPR-0970]] carries RFC-BPR-0013's fuzzing obligation (acceptance row 7):
fuzzers over diff parsing and mapping, Markdown, settings, themes, API
normalization, Git output parsing and AI structured output. This page is the
card's AC1 inventory, the AC2 registration, and the AC3 campaign record.
RFC-BPR-0013's amendment r4 and its earlier acceptance are unchanged.

- Source: `main`. The inventory and targets were written against `aac225a4e`
  and land in the commit that adds this page; the card records that commit.
- Work taken over from the unmerged `codex/bpr0970-fuzz` branch (`ae8139d1a`,
  six days old): its plan, and six draft harnesses and their seed files, left
  uncommitted in its worktree. The harnesses were rewritten against current
  `main`; the worktree was read and not changed.
- Host: Apple M4 Pro, macOS 26.6.2 (25G83), Apple clang 21.0.0, Qt 6.11.2.

## What counts as an entry point

An entry point here is a function that reads bytes or text from outside the
process, or a record derived from them, and decides what typed value they are:
a document, a protocol reply, a file another build or a person may have
written, or a mapping computed over such input. Third-party parsers the code
calls (glaze, Qt's Markdown reader, SQLite's file format, the JSON readers in
`src/app`'s tour CLI, which use Qt) are not custom, so they are not targets of
their own; the custom checks around them are. Writers are listed only where a
target uses them to check a round trip.

## The fuzz graph

`BPR_BUILD_FUZZERS=ON` adds `tests/fuzz` to a Clang or Apple clang build on
macOS or Linux. Every first-party unit in that graph is compiled with
`-fsanitize=fuzzer-no-link,address,undefined -fno-sanitize-recover=all -O1`,
so libFuzzer steers by the production code's coverage and AddressSanitizer
sees its accesses. Apple clang ships no libFuzzer runtime, so on macOS the
graph links the runtime archive named by `BPR_LIBFUZZER_RUNTIME`
(`/opt/homebrew/opt/llvm/lib/clang/23/lib/darwin/libclang_rt.fuzzer_osx.a`
here); the production code is compiled by the same compiler as the `dev`
graph. On Linux, Clang links `-fsanitize=fuzzer` itself. Windows is refused.

Each target `fuzz_<family>` registers `fuzz_<family>_corpus` (label
`bpr_fuzz_corpus`), a network-isolated row that replays the checked-in seed
corpus `tests/fuzz/corpora/<family>` once, with the campaign's limits, and
writes nothing back. A target's first input byte selects the entry point; the
target's header comment gives the layout. Multi-part inputs are split at
`\n\x1e\x1e\n`.

| Target | Registration | `-max_len` | Families |
|---|---|---|---|
| `fuzz_diff` | `fuzz_diff_corpus` | 8,192 | diff parsing and mapping |
| `fuzz_markdown` | `fuzz_markdown_corpus` | 16,384 | Markdown |
| `fuzz_markdown_view` | `fuzz_markdown_view_corpus` (Qt graph only) | 70,000 | Markdown |
| `fuzz_settings_theme` | `fuzz_settings_theme_corpus` | 20,000 | settings, themes |
| `fuzz_github_api` | `fuzz_github_api_corpus` | 262,144 | API normalization |
| `fuzz_git_output` | `fuzz_git_output_corpus` | 65,536 | Git output parsing |
| `fuzz_agent_output` | `fuzz_agent_output_corpus` | 65,536 | AI structured output |
| `fuzz_review_cycle` | `fuzz_review_cycle_corpus` | 65,536 | AI structured output (Review Cycles) |
| `fuzz_syntax` | `fuzz_syntax_corpus` (Qt graph only) | 65,536 | C++ syntax classification ([[BPR-1441]]) |

A target checks more than the absence of a crash where the production
contract states a property: a published diff validates and stays inside its
work limits; a line mapping is symmetric; an admitted link is printable,
rooted in the repository and never carries a parent segment; a theme
importer's refusals are bounded printable ASCII and an imported palette
exports and reimports equal; a settings store adopts only typography the core
accepts; canonical documents re-encode to their own bytes; a ZIP read writes
back to the same members; protocol and pack parsers stay inside their byte and
packet limits; an MCP answer never carries a file outside the snapshot. A
failed property aborts, and libFuzzer keeps the input.

## AC1: the inventory

Rows give the entry point, its input and who controls it, the deterministic
rows that already exercise it in the `dev` graph, and the target and selector
that fuzz it. "Selector" is the first input byte modulo the target's case count.

### Diff parsing and mapping (`src/core`)

| Entry point | Input | Deterministic rows | Fuzzed by |
|---|---|---|---|
| `compute_file_diff`, `validate_computed_file_diff` (`diff_compute.cpp`): text decoding, line splitting, alignment, pairing, moves, limits | Blob bytes from the pull request's repository | `core_diff_compute`, `application_diff_host` | `fuzz_diff`, mode bit 4 clear |
| `compute_material_diff`, `validate_computed_material_diff` | Bytes a Review Cycle read from a workspace | `core_review_cycle_comparison`, `review_cycle_comparison` | `fuzz_diff`, mode bit 4 set |
| `build_diff_semantic_index`, `map_line`, `move_for_line`, `detail_for_mapping`, `detail_for_line`, `select_line`, `select_group`, `preferred_group_line`, `navigate_change`, `project_changes` (`diff_semantics.cpp`) | A published diff, then line references | `core_diff_semantics` | `fuzz_diff` |
| `normalize_diff_options`, `valid(diff_options)`, `options_digest` (`diff_model.cpp`) | Persisted or requested options | `core_diff_model` | `fuzz_diff`, bytes 1 and 2 |

### Markdown

| Entry point | Input | Deterministic rows | Fuzzed by |
|---|---|---|---|
| `admit_markdown_link` (`core/markdown_navigation.cpp`): credential, scheme, authority and path checks | A link destination in a provider body, and the observed host and repository | `core_markdown_navigation` | `fuzz_markdown`, selectors 0 and 1 |
| `map_tour_markdown`, `map_tour_stop`, `edit_tour_markdown`, `insert_tour_brief`, `tour_markdown_label`, `render_tour_index`, `render_tour_stop` (`core/review_tour_markdown.cpp`) | A pull request description or tour stop someone else wrote | `core_review_tour_markdown`, `application_review_tour_publication_*` | `fuzz_markdown`, selector 2 |
| `qt::markdown_body_view::set_body`: `rendering_limit`, fence and tab repair, image placeholders around Qt's reader (`adapters/qt/markdown_body_view.cpp`), and [[BPR-1543]]'s GitHub-parity preparation and presentation (`adapters/qt/markdown_github.cpp`) | A provider body | `qt_markdown_body_1_0`, `_1_25`, `_2_0`; `qt_markdown_github_1_0`, `_1_25`, `_2_0` | `fuzz_markdown_view` |
| RFC-BPR-0084: `scan_html_images`, `markdown_linked_images`, `served_markdown_images`, `admit_markdown_image_source`, `match_served_markdown_image`, `admit_markdown_image_fetch`, `admit_markdown_image_redirect` (`core/markdown_image.cpp`) | Image markup in a provider body; GitHub's rendered `bodyHTML`; a redirect location | `core_markdown_image`, `github_markdown_image_resolution` | `fuzz_markdown`, selector 3 |
| RFC-BPR-0036 amendment 2: `scan_markdown_fenced_code`, `markdown_code_language_of`, `markdown_diff_marks`, `markdown_suggestion_rows` (`core/markdown_code.cpp`) | A provider body's fenced code, its info strings, and a suggestion's commented lines | `core_markdown_code`, `qt_markdown_code_1_0`, `_1_25`, `_2_0` | `fuzz_markdown`, selector 4 |
| `render_round_markdown` (`core/review_cycle_round_markdown.cpp`) | An admitted round | `core_review_cycle_round_markdown` | `fuzz_review_cycle`, selector 3, on admission |

### Settings

| Entry point | Input | Deterministic rows | Fuzzed by |
|---|---|---|---|
| `appearance_settings_database::open`, `load`: `read_settings`, `read_legacy_settings`, mode and flag decoding, `validate_typography` (`adapters/sqlite/appearance_settings_database.cpp`) | A durable settings file another build or a hand edit may have changed | `sqlite_appearance_settings_storage` | `fuzz_settings_theme`, selector 1: cells rewritten with SQLite, read back through `open` and `load` |
| `workbench_settings_database::open`, `load`, `load_window`, including the writing-hook overrides the tour CLI reads (`adapters/sqlite/workbench_settings_database.cpp`) | The same | `sqlite_workbench_settings_storage` | `fuzz_settings_theme`, selector 2 |

### Themes

| Entry point | Input | Deterministic rows | Fuzzed by |
|---|---|---|---|
| `import_theme`, `export_theme` round trip (`core/theme_exchange.cpp`) | A theme file a user chose, and stored palette documents | `core_theme_exchange`, `application_appearance_host` | `fuzz_settings_theme`, selector 0, and through selector 1 |

### API normalization (`src/adapters/github`)

| Entry point | Input | Deterministic rows | Fuzzed by |
|---|---|---|---|
| `json_preflight` (`github_json_preflight.hpp`; since [[BPR-1433]] a wrapper over `core::json_preflight`) | Every JSON response before glaze decodes it | through `github_read_contract`, `github_mutation_contract` | `fuzz_github_api`, selector 0 |
| `inspect_graphql_operation_policy` (`github_graphql_operation_policy.hpp`) | An outbound GraphQL document | `github_read_contract` | selector 1 |
| `normalize_github_read`, `decode_record` (`github_read_normalizer.cpp`) | The ten read responses | `github_read_contract` | selector 2: one of the ten recorded github.com responses replaced |
| `parse_github_response_metadata` (rate limits, `Link`, `Retry-After`), `github_error_body_is_secondary_rate_limit`, `github_graphql_error_body_is_rate_limited`, `normalize_observed_github_viewer` (`github_paged_read.cpp`) | Response headers and bodies | `github_paged_read`, `gh_cli_transport` | selector 3 |
| `parse_device_code_response`, `parse_device_token_response`, `parse_device_refresh_response` | Device-flow responses | `github_device_authorization_contract` | selector 4 |
| `parse_release_update_metadata`, `parse_gh_cli_auth_status`, `parse_gh_cli_response` with `parse_link_completeness` (`gh_cli_response.hpp`), `parse_tour_permalink`, `parse_review_permalink_body`, `sanitize_capability_error` | Release metadata; `gh auth status` JSON; what `gh api --include` prints; comment bodies; error bodies | `release_update_transport_contract`, `gh_cli_transport`, `gh_cli_git_smart_transport`, `github_review_comment_permalink`, `github_capability_probe` | selector 5 |
| `parse_repository`, `parse_branch`, `parse_commit`, `parse_tree_presence`, `parse_blob_presence`, `parse_created_sha`, `parse_created_commit`, `parse_created_branch`, `parse_pull_request`, `parse_pull_requests` (`github_publication_contract.cpp`) | Publication responses | `github_publication_contract` | selector 6 |
| `parse_target_observation`, `parse_viewer_observation`, `parse_review_comment_response`, `parse_review_comment_observations`, `parse_review_submission_response`, `parse_review_submission_observations`, `parse_review_submission_recovery_target`, `parse_thread_target_observation`, `parse_thread_resolution_response`, `parse_staged_review_response` (`github_mutation_contract.cpp`) | Review mutation responses | `github_mutation_contract` | selector 7 |
| `normalize_capability_probe`, `capability_probe_batch` (`github_capability_probe.cpp`) | The capability probe's GraphQL reply | `github_capability_probe`, `github_capability_content_policy` | selector 8 |
| `perform_paged_github_read` (signed in and signed out), `perform_paged_github_timeline_read`, `perform_personal_discovery_read`: `parse_json_document`, `parse_link_relations`, `parse_url_query`, `parse_rest_actor`, `parse_file`, `parse_commit`, `parse_review`, `parse_issue_comment`, `parse_review_comment`, `parse_comment_observation`, `parse_graphql_comment`, `parse_check`, `parse_pull_ref`, `parse_timeline_timestamp`, `parse_discovery_item`, `parse_requested_reviewers` (`github_paged_read.cpp`) | Each page of a pull request read, timeline and discovery search | `github_paged_read`, `large_review_replay_contract` | selector 9: the payload's parts answer the read's requests in order |

The reader of `gh api --include` output was inline in `gh_cli_transport.cpp`,
reachable only through a running `gh`. This card moves it, unchanged, into
`gh_cli_response.hpp`, which the transport includes and calls, as it does the
Git readers below. `github_capability_probe.cpp`'s `parse_json_object`,
`parse_selected_type` and `parse_rate_limit_response_facts` are reached
through `normalize_capability_probe`. `designated_live_public_observer.cpp`'s
`parse` belongs to the designated live capsule, which is built only for that
capsule and which this card may not run; it is not fuzzed.

### Git output parsing

| Entry point | Input | Deterministic rows | Fuzzed by |
|---|---|---|---|
| `parse_git_commit_oid`, `parse_git_tree_oid`, `parse_git_blob_oid`, `parse_git_entry_oid`, `parse_git_base_branch_ref`, `git_object_format` | Object names, ref names and `core.objectformat` from replies and repositories | `core_git_objects`, `application_git_protocol_v2`, `core_review_cycle_manifest` | `fuzz_git_output`, selector 0 |
| `parse_git_index` (`core/review_cycle_git.cpp`) | A workspace's `.git/index` | `core_review_cycle_manifest` | selector 1 |
| `parse_git_head`, `parse_git_reference`, `find_packed_reference` | `HEAD`, loose refs and `packed-refs` | `core_review_cycle_manifest` | selector 2 |
| `workspace_ignore_rules`, `workspace_attribute_rules`, `wildcard_match` (`core/review_cycle_ignore.cpp`) | `.gitignore`, `info/exclude`, `.gitattributes` | `core_review_cycle_manifest` | selector 3 |
| `parse_git_protocol_v2_advertisement`, `parse_git_protocol_v2_ls_refs` (`application/git_protocol_v2.cpp`) | A smart-HTTP remote's replies | `application_git_protocol_v2`, `native_git_smart_transport_contract` | selector 4 |
| `git_protocol_v2_fetch_parser` (pkt-lines, sidebands, pack stream) | The fetch reply | `application_git_protocol_v2`, `git_cache_contract` | selector 5 |
| `parse_comparison_bases`, `parse_ls_tree_blob`, `parse_rename_pairs`, `parse_blob_size`, `parse_committer_seconds`, `parse_ref_record`, `parse_version`, `parse_object`, `valid_tree_entry_path` (`adapters/git/git_process_output.hpp`) | What the installed Git prints about cache repositories: object names, paths and sizes the repository controls | `git_process_contract`, `git_cache_contract`, `git_sparse_cache_contract`, `application_epoch_host` | selector 6 |
| `verified_commit_tree`, `line_feed_form` (`core/git_objects.cpp`) | Commit objects and blob bytes | through `review_cycle_publisher` and `review_cycle_outgoing` | selector 7 |

These readers were inline in `installed_git_process.cpp`'s member functions,
reachable only through a running Git. This card moves them, unchanged, into
`git_process_output.hpp`, which the adapter includes and calls; the target
calls the same functions. `installed_git_cache.cpp`'s `parse_pack_oid` reads
pack file names in a directory the cache itself wrote and validates, with the
same hexadecimal rule as `parse_object`; it is not fuzzed separately.

### AI structured output

| Entry point | Input | Deterministic rows | Fuzzed by |
|---|---|---|---|
| `decode_agent_finding_payload`, four protocols (`application/agent_launch.cpp`) | An agent's `bprFindings` payload | `application_agent_finding_protocol` | `fuzz_agent_output`, selector 0 |
| `agent_protocol_decoder::push`, `finish`, `acknowledge_followup` | An agent's stdout frames, in arbitrary chunks | `application_agent_launch`, `application_agent_protocol_loop` | selector 1 |
| `decode_writing_result` | A writing hook's JSON result | `application_writing_delivery` | selector 2 |
| `mcp_context_helper::handle_line`, its JSON reader and tool paths (`adapters/mcp/mcp_context_helper.cpp`) | An agent's JSON-RPC requests | `application_mcp_context_helper` | selector 3, against a frozen snapshot with a link out of it |
| `decode_revision_binding`, `read_snapshot_alignment` | The snapshot's binding and alignment files | `application_mcp_context_helper` | selectors 4 and 5 |
| `decode_response_document`, `decode_manual_reply`, `decode_suggestion_wire`, `parse_imported_claim` (`core/review_cycle_response.cpp`, `_suggestion.cpp`, `_policy.cpp`) | A response an agent or reviewer returns to a round | `core_review_cycle_response`, `core_review_cycle_suggestion`, `core_review_cycle_policy` | `fuzz_review_cycle`, selectors 0, 1, 2, 10 |
| `decode_round_envelope`, `decode_round_payload`, `decode_round_selection` | A round handed in, and stored payloads | `core_review_cycle_round` | selectors 3, 4, 5 |
| `parse_cycle_json`, `canonical_cycle_json`, `decode_cycle_base64`, `parse_cycle_sha256` | The strict JSON and encodings under every cycle document | `core_review_cycle_round` | selectors 6 and 7 |
| `read_cycle_zip`, `inflate_cycle_deflate`, `open_round_bundle` (`core/review_cycle_zip.cpp`, `_bundle.cpp`) | A `.bpr-review.zip` bundle | `core_review_cycle_bundle`, `core_review_cycle_bundle_pack_oracle` | selectors 11 and 12 |
| `decode_manifest`, `decode_publication_plan`, `decode_publication_journal`, `decode_import_association` | The cycle's own durable records | `core_review_cycle_manifest`, `core_review_cycle_publication`, `core_review_cycle_import` | selectors 8, 9, 10 |

### C++ syntax classification ([[BPR-1441]])

Added for RFC-BPR-0081, after this card's campaigns. The target stays whatever
that evaluation decides.

| Entry point | Input | Deterministic rows | Fuzzed by |
|---|---|---|---|
| `packaged_syntax_classifier::classify`, `classify_hunk` with the C++ definition: the KSyntaxHighlighting complete-blob pass and `refine_cpp_identifiers` (`adapters/syntax/`) | A file's bytes from the pull request, a Review Cycle workspace or the suggestion composer | `syntax_classification_contract`, `syntax_refinement_cancellation`, `qt_syntax_pane_composition` | `fuzz_syntax`: bytes 1 and 2 name a hunk's first line, byte 3 its line count, the rest is the blob |

It checks that a result other than `available` carries no spans; that
available spans are nonempty, ordered, non-overlapping, inside the blob's
lines and syntax roles; that a request answers the same twice; that a hunk is
exactly the whole blob's spans on its lines; that a span limit below the
answer's count refuses with no spans; that a stop requested before the call
publishes nothing; and that strictly valid UTF-8 is never `unavailable`. A
first 300-second run on macOS (Apple M4 Pro, seed 1441, 66,040 executions)
was clean.

Every entry point above has a target. The two `tests/fuzz/release_*_fuzz.cpp`
harnesses belong to the release policy's own recipe and are not registered
in this graph.

## AC2: seeds

| Corpus | Seeds | Where they come from |
|---|---|---|
| `diff` | 12 | Hand-made pairs: identical, replaced, CRLF against LF, whitespace, NUL bytes, BOM, invalid UTF-8, a moved block, absent sides, the material path |
| `markdown` | 30 | Hostile and ordinary link destinations; review Markdown from `docs/rfcs/assets/bpr-review-cycles-v1/examples` and `bpr-review-tour-v1/examples`; one regression seed; three image-markup and served-HTML seeds and one SVG badge seed (RFC-BPR-0084); four fenced-code seeds (languages, diffs, containers, suggestions; [[BPR-1562]]) |
| `markdown_view` | 21 | Tables, fences with tabs, images, deep nesting, HTML, the tour examples; one regression seed; [[BPR-1543]]'s GitHub-parity bodies (comments, alerts, footnotes, a PR template, edge marks); [[BPR-1544]]'s details, HTML, hostile HTML, emoji and HTML PR bodies |
| `settings_theme` | 15 | The draft branch's five theme documents; cell rewrites of text, integers, NULL, blobs and invalid UTF-8 for both stores |
| `github_api` | 95 | The recorded github.com contract responses (`tests/fixtures/github/contracts/v1/github-com`); capability fixtures, with and without rate-limit headers; the RFC-BPR-0017 signed-out captures; the draft branch's GraphQL documents; device-flow, mutation, staged-review and publication bodies shaped as the contract tests write them; `gh api --include` output; the eleven pages of `github_paged_read`'s success course, captured in request order from an uncommitted dump of its fixture transport, and the signed-out variant; timeline and discovery pages; one regression seed |
| `git_output` | 26 | A real index, commit object and packed-refs from a scratch repository; pkt-line replies as `application_git_protocol_v2` builds them; Git's own output for each process reader; one regression seed |
| `agent_output` | 24 | Finding payloads and frames as the agent protocol tests build them; the writing results in the tour examples; an MCP session including an escape through the link; one regression seed |
| `review_cycle` | 19 | The golden `round.json`, `response.json` and `example.bpr-review.zip`; a manifest, publication plan and journal, round selection, manual reply and suggestion written by the production encoders |

The seeds reach the deep paths, checked with `-print_coverage=1`: the signed-in
and signed-out paged reads, the timeline read and both discovery reasons
complete; all ten normalizer responses normalize; the appearance and
workbench stores load; the golden round, response and bundle are admitted.

## AC3: campaigns

Every campaign ran the `build/fuzz` binaries on a copy of the corpus in a
scratch directory, one process per target, all eight at once on the M4 Pro:

```sh
<target> -seed=<seed> -max_total_time=<seconds> -max_len=<target's> -timeout=10 \
  -rss_limit_mb=2048 -malloc_limit_mb=512 -print_final_stats=1 \
  -artifact_prefix=<scratch>/artifacts/<target>- <scratch>/corpus/<target>
```

Seeds count up from 970001 in the target order of the table, so
`fuzz_diff` is 970001 and `fuzz_review_cycle` 970008; the second campaign
starts at 970101. The scratch corpora, logs and artifacts are local and not
committed; the reproducers of the findings are, as regression seeds.

### Findings

| Found by | Defect | Disposition |
|---|---|---|
| `fuzz_agent_output`, trial, 60 s | `mcp_context_helper.cpp`'s `render_identity` cast a JSON-RPC number identity to `std::int64_t` with no range check: `{"id":5555…551}` (104 digits) is undefined behaviour (UBSan: "5.55556e+103 is outside the range of representable values") | Fixed: an identity outside [−2⁶³, 2⁶³) is refused before the cast. Seed `agent_output/regression-mcp-identity-range` |
| `fuzz_github_api`, trial, 60 s | `parse_gh_cli_auth_status` takes a `std::string_view`, but its glaze options assumed a NUL after the view and read one byte past it (ASan: heap-buffer-overflow, READ of size 1, in `glz::read`). The one production caller passes a `std::string`, which has the NUL, so no shipped path over-reads | Fixed: the options declare `null_terminated = false`, as every other glaze reader in the adapter already does. Seed `github_api/regression-gh-auth-status-view` |
| `fuzz_git_output`, campaign 2, after about 174 s | `wildcard_match` backtracked exponentially in the number of `*`: a 6 KiB `.gitignore` line and path took 13 seconds for one match (libFuzzer timeout). A repository's ignore file is read by every Review Cycle capture of its workspace | Fixed: the matcher stops as Git's `wildmatch` does, when the text runs out with pattern left or a single `*` reaches a `/`. A differential run of 5,000,000 random patterns and names against the old matcher found no disagreement. `core_review_cycle_manifest` gains three cases; with the old matcher the row times out at 20 s. Seed `git_output/regression-wildcard-backtracking` |
| `fuzz_markdown_view`, campaign 1, after about 196 s | `markdown_body_view::refresh_formats` restyled each fragment while a `QTextBlock::iterator` stood on it. A restyle can merge the fragment with its neighbour, and the iterator then read a freed fragment node (ASan: BUS in `QTextBlock::iterator::operator++`). Minimised to the 8-byte body `[_|_)]()` | Fixed: each block's fragments are read first and restyled after; a character format moves no text, so the spans stay valid. Seed `markdown_view/regression-fragment-merge` |

Each reproducer fails at the base and passes with its fix in the same
instrumented build. The matcher's cost shows in any build, so its
deterministic cases discriminate on their own. The other three do not
announce themselves in an uninstrumented build: a regular `dev` run of `[_|_)]()` renders `|)` and passes, the
over-read reads a byte nobody looks at, and an out-of-range conversion's
result depends on the platform. So the regression cases are the corpus seeds,
which the `fuzz_*_corpus` rows replay under the sanitizers; a deterministic
row without them could not tell the fixed code from the broken. Two trial
rows showed it: `gh_cli_transport` passed with its fix reverted, and the
unfixed view rendered the body without a fault. Neither row was kept.

Two harness properties were wrong, not the code, and were corrected:
`fuzz_markdown` required no `%` anywhere in an admitted repository path, but
the production rule refuses `%` only in the path, and keeps a query after it
(`r?c/%2e%eeet`); and `fuzz_agent_output` required no answer to carry the
outside file's text, but an answer echoes the request's identity, which may
be that text. Each is kept as a seed (`markdown/regression-query-after-path`).

Seen and not changed: `render_identity` says it echoes an identity "exactly as
the caller sent it", but a number beyond 2⁵³ comes back rounded
(`9007199254740993` as `9007199254740992`), and `1e2` comes back as `100`.
JSON-RPC clients send small integers or strings; this card's fix is limited to
the undefined behaviour.

### Campaign 1: 15 minutes per target

Source: `aac225a4e` with this change, before the markdown view fix. Started
2026-10-01T18:52:23Z, ended 19:07:24Z.

| Target | Seed | Executions | exec/s | New units | Peak RSS (MB) | Final coverage | Outcome |
|---|---|---|---|---|---|---|---|
| `fuzz_diff` | 970001 | 2,071,952 | 2,299 | 5,166 | 601 | cov 4,028, ft 16,653, 2,407 units | clean |
| `fuzz_markdown` | 970002 | 7,241,685 | 8,037 | 19,047 | 677 | cov 1,582, ft 7,166, 1,175 units | clean |
| `fuzz_markdown_view` | 970003 | 158,829 | 810 | 1,846 | 751 | | crash after about 196 s: the fragment merge above |
| `fuzz_settings_theme` | 970004 | 467,864 | 519 | 4,833 | 416 | cov 8,940, ft 25,111, 934 units | clean |
| `fuzz_github_api` | 970005 | 1,414,782 | 1,570 | 16,680 | 767 | cov 20,047, ft 46,722, 4,261 units | clean |
| `fuzz_git_output` | 970006 | 30,226,139 | 33,547 | 25,643 | 625 | cov 2,076, ft 7,805, 1,882 units | clean |
| `fuzz_agent_output` | 970007 | 12,377,149 | 13,737 | 26,695 | 600 | cov 3,339, ft 13,447, 2,075 units | clean |
| `fuzz_review_cycle` | 970008 | 11,416,802 | 12,671 | 12,457 | 469 | cov 6,162, ft 17,296, 1,785 units | clean |

Before it, two short trials (60 s and 120 s per target) found the MCP and
`gh` defects and the two harness errors.

### Campaign 2: 15 minutes per target, from campaign 1's corpora

Source: as campaign 1, with the markdown view fix. Each target started from
campaign 1's grown corpus plus the checked-in seeds. Started
2026-10-01T19:08:30Z.

| Target | Seed | Executions | exec/s | New units | Peak RSS (MB) | Final coverage | Outcome |
|---|---|---|---|---|---|---|---|
| `fuzz_diff` | 970101 | 1,413,532 | 1,568 | 708 | 538 | cov 4,044, ft 16,971, 1,852 units | clean |
| `fuzz_markdown` | 970102 | 8,702,451 | 9,658 | 5,308 | 527 | cov 1,582, ft 7,248, 1,012 units | clean |
| `fuzz_markdown_view` | 970103 | 587,733 | 652 | 1,549 | 1,041 | cov 729, ft 2,809, 279 units | clean, with the fix |
| `fuzz_settings_theme` | 970104 | 547,417 | 607 | 1,353 | 437 | cov 8,944, ft 25,235, 798 units | clean |
| `fuzz_github_api` | 970105 | 1,567,649 | 1,739 | 6,593 | 826 | cov 21,045, ft 49,968, 4,551 units | clean |
| `fuzz_git_output` | 970106 | 5,999,054 | 34,477 | 1,928 | 455 | | timeout after about 174 s: the wildcard matcher above |
| `fuzz_agent_output` | 970107 | 12,285,003 | 13,634 | 5,746 | 535 | cov 3,363, ft 13,756, 1,853 units | clean |
| `fuzz_review_cycle` | 970108 | 10,509,185 | 11,663 | 3,726 | 457 | cov 6,267, ft 17,998, 1,671 units | clean |

### Campaign 3: 15 minutes per target, the final product code

Source: as campaign 2, with the wildcard matcher fix; every target relinked.
Each target started from campaign 2's grown corpus plus the checked-in seeds.
2026-10-01T19:24:16Z to 19:39:38Z.

| Target | Seed | Executions | exec/s | New units | Peak RSS (MB) | Final coverage | Outcome |
|---|---|---|---|---|---|---|---|
| `fuzz_diff` | 970201 | 1,202,987 | 1,335 | 519 | 605 | cov 4,045, ft 17,186, 1,865 units | clean |
| `fuzz_markdown` | 970202 | 5,999,084 | 6,658 | 4,757 | 558 | cov 1,582, ft 7,264, 1,035 units | clean |
| `fuzz_markdown_view` | 970203 | 413,403 | 458 | 954 | 1,006 | cov 729, ft 2,836, 302 units | clean |
| `fuzz_settings_theme` | 970204 | 593,857 | 659 | 856 | 474 | cov 8,949, ft 25,282, 842 units | clean |
| `fuzz_github_api` | 970205 | 1,785,084 | 1,981 | 5,678 | 968 | cov 22,685, ft 53,792, 5,401 units | clean |
| `fuzz_git_output` | 970206 | 24,888,379 | 27,623 | 4,935 | 588 | cov 2,079, ft 7,931, 1,694 units | clean, with the matcher fix |
| `fuzz_agent_output` | 970207 | 11,515,411 | 12,780 | 3,117 | 563 | cov 3,364, ft 13,855, 1,896 units | clean |
| `fuzz_review_cycle` | 970208 | 10,531,064 | 11,688 | 2,959 | 438 | cov 6,311, ft 18,221, 1,724 units | clean |

After campaign 3 two corrections reached `fuzz_github_api` only. The `gh`
reply reader moved into `gh_cli_response.hpp` and gained a case. And the
harness's header splitter had been taking a JSON body's lines for headers
and keeping the space after each colon, so the device-flow cases and the
capability probe's rate-limit headers saw less than the seeds held in
campaigns 1 to 3. Campaign 4 ran that target again.

### Campaign 4: `fuzz_github_api` again, 15 minutes

Source: the final candidate. Started from campaign 3's grown corpus plus the
checked-in seeds. 2026-10-01T19:40:10Z to 19:55:11Z.

| Target | Seed | Executions | exec/s | New units | Peak RSS (MB) | Final coverage | Outcome |
|---|---|---|---|---|---|---|---|
| `fuzz_github_api` | 970305 | 2,134,878 | 2,369 | 7,774 | 1,055 | cov 24,505, ft 59,727, 6,788 units | clean |

### RFC-BPR-0084: `fuzz_markdown` selector 3, 5 minutes

Selector 3 reads its payload as a body's image markup (`scan_html_images`,
`markdown_linked_images`) and as GitHub's rendered HTML
(`served_markdown_images`), and admits what it finds as a source, a served
match, a first hop and a redirect under a GitHub.com and a GHES policy. It
requires ordered disjoint spans that start at a tag and never decode longer
than their markup, linked images whose link is in the input, HTTPS for every
admitted hop, no GitHub.com credential, and a credential that never reappears
after a hop drops it. Three seeds (`markdown/image-upload`,
`image-linked-picture`, `image-served-html`) start the family; selectors 0 to
2 keep their meaning (`% 4` instead of `% 3`; no seed began with 3).
On 2026-10-16, seed 840084, from the checked-in corpus: 2,034,317 executions in
301 s (6,758 exec/s), 22,713 new units, peak RSS 692 MB, clean.

Owner amendment 1 adds SVG admission to the same selector: every payload is
also offered to `admit_markdown_svg`, which must refuse anything without an
`svg` root as unsupported and never admit a document holding a script, an
entity declaration, `@import`, an external `url()` or an `http`/`file` `href`.
One seed (`markdown/image-svg-badge`) holds a badge with fragment references
and one external image. On 2026-10-16, seed 840085, from the checked-in corpus:
2,611,624 executions in 301 s (8,676 exec/s), 26,074 new units, peak RSS
701 MB, clean.

### Campaign 5: `fuzz_markdown_view` after [[BPR-1543]], 15 minutes

The GitHub-parity preparation and presentation (`markdown_github.cpp`) added a
line classifier, a code-span scanner and document rewriting to the reader, and
the target now also requires that no parity mark reaches the rendered document
unless the body holds one. Run on a copy of the 16 checked-in seeds at
`cc305a2ff` (the later bracketing of a reference's number changes only its
text), macOS arm64, Apple clang with LLVM 23's libFuzzer runtime.

| Target | Seed | Executions | exec/s | New units | Peak RSS (MB) | Final coverage | Outcome |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `fuzz_markdown_view` | 1543001 | 360,876 | 400 | 6,047 | 710 | cov 2,181, ft 10,482, 1,260 units | clean |

### Campaign 6: `fuzz_markdown_view` after [[BPR-1544]], 15 minutes

The HTML recognizer and sanitizer (`markdown_github_html.cpp`), `<details>`
regions and emoji passes joined the reader. Run on a copy of the 21
checked-in seeds at `fbdb893e2`, macOS arm64, Apple clang with LLVM 23's
libFuzzer runtime, while an attended build compiled beside it (hence the low
rate). The target's own fix for this tree (its helper renamed apart from
`bpr::application`) is in the same commit range.

| Target | Seed | Executions | exec/s | New units | Peak RSS (MB) | Final coverage | Outcome |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `fuzz_markdown_view` | 1544001 | 67,351 | 74 | 2,944 | 577 | cov 3,775, ft 17,662, 1,596 units | clean |

### Campaign 7: `fuzz_markdown` selector 4 after [[BPR-1562]], 10 minutes

The fenced-code scan, the language table, GitHub's diff grammar and the
suggestion rows joined the core target as selector 4, with the property
checks in `tests/fuzz/markdown_fuzz.cpp` (blocks in order and bounded, lines
inside the body, marks inside their line and ordered, rows matching both
sides with changed words inside each row). Run on a copy of the checked-in seeds
(29 then; RFC-BPR-0084's SVG seed makes 30) on branch `bpr-1562-fenced-code`, macOS arm64, Apple clang with LLVM
23's libFuzzer runtime, `-max_len=16384`.

| Target | Seed | Executions | exec/s | New units | Peak RSS (MB) | Final coverage | Outcome |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `fuzz_markdown` | 1562 | 574,890 | 956 | 2,207 | 641 | cov 6,577, ft 35,573, 2,236 units | clean |

The reader's placement of those blocks (`adapters/qt/markdown_code.cpp`) runs
under `fuzz_markdown_view` through `set_body`; no Qt-graph campaign was run
for this change.

### What the campaigns do not show

Three 15-minute campaigns per target, and a fourth for `fuzz_github_api`,
found four defects, and each target's last campaign ran clean. That is evidence about the inputs
the campaigns reached, not a claim that no other defect exists. Coverage is
reported by libFuzzer's own counters over each target's instrumented code;
no line coverage report was made. The campaigns ran on macOS only. The
Windows and Linux builds of the changed sources are the stations'.
