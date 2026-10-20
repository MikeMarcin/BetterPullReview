# BPR-0822 GCC 15.2 aggregate cleanup survey

## Current frontier

- **Binding.** Run `run-20260916-stations`, dispatch `dsp-001-r2` (station
  `bpr-linux/int-3`), mandate r2, task `task-20260917-lnx-verify`. Station
  `bpr-linux` (`stn-3f7fcc92`, `wsl2`), branch `linux-lane`, authority
  `lane-commit`. Base `c19fa196`; BPR-0822 claimed at `fe5a71d9`.
- **State.** Source fixes, the survey tool and the build note are committed
  in `de528ce3` and pushed. The stock builds, focused courses and the survey
  run again on that tip; the results are under "Final run" below.
- **Card.** Stays In Progress. Its third criterion also names the macOS dev
  build, which this station cannot run.
- **Logs.** `/home/mmarcin/bpr-station-logs/task-20260917-lnx-verify/bpr0822/`
  and the task directory above it. None is committed.

## Host and compiler

JaegAW, WSL2 (kernel `6.18.33.2-microsoft-standard-WSL2`), Ubuntu 26.04 LTS,
20 logical CPUs. GCC 15.2.0 (`Ubuntu 15.2.0-16ubuntu1`) as `/usr/bin/c++`;
clang 22 (`clang++-22`) for comparison. CMake 4.4.2, Ninja 1.13.2, Qt 6.10.3.

## The shape, measured

The station first measured the defect at run time. Each shape counted live
objects of a `tracked` type across an initialization whose later member
throws. A negative net count is a double destroy. The shape sources are
`shapes2.cpp`, `shapes3.cpp` and `shapes4.cpp` (48 shapes); clang 22 gave 0
for every one.

GCC 15.2 double-destroys when all of these hold:

- A member is an aggregate with more than one element and a non-trivially
  destructible subobject.
- It is initialized from braces without naming its type. That covers
  designated or positional elements, `{ }`, and an omitted member, including
  one whose default member initializer names the type (`receipt Receipt =
  receipt{ };`).
- A later member's initializer throws. A throwing default member
  initializer counts.

The context does not matter: a local, `return`, `new`, a function argument,
`push_back`, an initializer list, a lambda capture, an assignment from braces,
`std::optional` initialization, a static local, a base-class subobject and a
class template all fail.

It does not happen for:

- a member initialized from a named-type prvalue (`.Receipt = receipt{ ... }`)
  or a moved local;
- a single-element nested aggregate;
- a non-aggregate member type, `std::optional` or `std::array` members, or C
  array elements;
- a member initialized directly in a constructor's member-initializer list;
- C++20 parenthesized aggregate initialization.

An aggregate braced *inside* a member-initializer list does fail; the
independent review found that case.

**Why.** `-fdump-tree-gimple` shows the mechanism. GCC guards every constructed
subobject with a cleanup flag. For the braced member it arms the member's whole
cleanup (`receipt::~receipt (&v.Receipt)`) while the flag guarding the
subobject inside it (`~tracked (&v.Receipt.ServerCommentId)`) is still 1. For
a named prvalue, GCC clears the inner flag first.

## Method

A grep cannot see an omitted member, and a Clang matcher sees Clang's lowering,
which is correct. The `app-release` preset's `-O3 -Wmaybe-uninitialized`
reported only BPR-0816's two sites. At the base it builds clean with the ten
`src/` sites below still present. So the survey asks GCC.

`scripts/survey_gcc_aggregate_cleanups.py`
([build-and-test](../development/build-and-test-reference.md#gcc-15-can-destroy-a-braced-aggregate-member-twice)):

1. **Calibration.** It compiles an embedded reproducer of 25 shapes with the
   build's compiler and runs it. It then requires the detector to flag exactly
   the shapes whose run double-destroys: 16 under GCC 15.2. A disagreement
   stops the survey with status 2.
2. **Survey.** It recompiles every translation unit of each compile database
   with the recorded flags plus `-fdump-tree-gimple-lineno
   -fdump-tree-eh-lineno`. Objects and dumps go to a temporary directory.
3. **Detector.** For each destructor call in a `finally` or `catch` handler,
   it takes the protected `try` body and the statements at which all of the
   call's guard flags are 1. It pairs calls whose object paths nest (`P` and
   `P.S`). A pair is a finding when a call statement is protected by both,
   and the EH-lowered function destroys both objects on an exception path.
   An unguarded handler counts as active over its whole body.

**How false negatives are bounded.**

- **Every GCC-built unit.** It covers every unit of both Linux presets: `dev`
  (412, including `tests/`) and `app-release` (202, with its
  `!BPR_BUILD_TESTING` code). That is GCC's own lowering, with templates as
  instantiated and headers as included. Code under `_WIN32` or `__APPLE__`
  is never compiled by GCC.
- **Calibration.** The committed calibration agrees with the run on 25
  shapes. The station's exploratory shapes agreed on all 48. The independent
  reviewer built about 60 more counterexample shapes and found no run-time
  double destroy that the detector missed.
- **Positive control.** With BPR-0816's fix reverted in a source snapshot, the
  tool flags exactly its three sites: `github_mutation_contract.cpp:986` and
  `:1016`, and `review_ledger_database.cpp:1815`.
- **Conservative parts.** The detector is conservative where it parses:
  unrecognised guards widen the active region. The EH filter drops only pairs
  that GCC's EH lowering does not destroy on any exception path; a run that
  cannot throw is still flagged. The reviewer's shape with a later call that
  cannot throw was flagged with a run-time net of 0.

**Iterations.** The first station-local detector keyed on `D.N` flags only. It
reported 82 sites, including false pairs guarded by `cleanup.N` flags, for
example `installed_git_cache.cpp:2142`. Generalising the guards gave 59. The
EH filter removed sites with no exception path, such as
`review_ledger_database.cpp:1447`, whose later initializer is a
`local_note_revision` built from a value already read, and a value-initialized
`github_wire_request`. That left 46. The committed tool is that last
version.

## Base survey: `fe5a71d9`, source identical to `c19fa196`

The tool ran over a `git archive` snapshot with both presets' compile
databases: 614 units, 0 compile failures, 280 s at `--jobs 10`. Status 1: 46
sites, 10 under `src/` (`tool-base.log`, `tool-base.json`).

### `src/` sites and their dispositions

The exception edge is real at each site. In practice only an allocation
failure reaches it: the later initializers copy strings, construct a
`std::stop_source`, or call an evidence builder that allocates. Each site now
names the type, and values, order and behaviour are unchanged. The independent
review checked that `profile_registry_snapshot{ }` still has one `Default`
profile with the same id, label, root mode and root identity and no windows,
and that it compares equal to the old form under GCC and clang.

| Site (base line) | Built into | Nested member | Later initializer that can throw | Disposition |
|---|---|---|---|---|
| `src/application/include/bpr/application/profile_registry.hpp:29` | both (reported here in dev units, at the using lines in others) | `profile_registry_entry::Profile` in the default `Profiles` entry | `RootIdentity` from `"legacy"` | Fixed: `core::bpr_profile_record{ ... }` |
| `src/adapters/sqlite/src/profile_registry_store.cpp:223` | both | the same default member initializer, used by `profile_registry_snapshot{ }` | as above | Fixed by the header |
| `src/adapters/sqlite/src/profile_registry_store.cpp:411` | both | the same | as above | Fixed by the header |
| `src/app/src/github_application.cpp:12487` | both | the same | as above | Fixed by the header |
| `src/app/src/github_application.cpp:8283` (the `SharedDrafts->apply` call at 8274) | both | `revisioned_review_draft_command::Command` | `ReplyToThread` copy, `Scope` | Fixed: `core::review_draft_command{ ... }` |
| `src/app/src/github_application.cpp:17881` and `:17885` | dev (round-trip probe) | `Command`, twice | `Scope` | Fixed: `core::review_draft_command{ ... }` |
| `src/application/src/github_application_host.cpp:1006` | both | `session_candidate::Viewer` (`{ }`) | `anonymous_capability_evidence(aAuthority)` | Fixed: `github_viewer_record{ }` |
| `src/application/src/review_mutation_application_host.cpp:798` | both | `request_record::Anchor` (`{ }`) | `Stop` (`std::stop_source` allocates) | Fixed: `review_mutation_anchor_binding{ }` |
| `src/app/src/github_public_path_fixture.cpp:615` | dev (fixture) | `wire_request::Semantic` (`{ }`) | `Response.Headers = response_headers()` | Fixed: `semantic_read_request{ }`. `Request` is named too (`wire_request{ ... }`); the review showed that naming `Response` or adding a throwing sibling would otherwise expose it. |

**Not flagged.** `src/core/include/bpr/core/profile_window.hpp:81` has the same
text shape as the header site, but its later members are value-initialized
vectors that cannot throw. The reviewer confirmed that at run time, and it is
left as it is.

### `tests/` sites and their disposition

**Disposition for every row: left unchanged.** Each site builds fixture
values in a test. The later initializers are string, vector, path or optional
copies whose only exception is an allocation failure, and none calls a helper
that throws on purpose. A double destroy there can only follow an allocation
failure in a test process. The tool lists `tests/` and gates only `src/`.

Six base sites were default-`Profiles` uses and went away with the header fix:

- `profile_registry_tests.cpp:207`;
- `profile_storage_authority_tests.cpp:23` and `:46`;
- `profile_registry_storage_tests.cpp:98` and `:198`;
- `profile_storage_root_tests.cpp:50`.

The other 30 remain. The workbench site moved from line 6464 to 6514 with
BPR-0824's edit.

| Site | Nested member(s) |
|---|---|
| `tests/adapters_qt/composed_github_tests.cpp:5957` | `Preferences`, `Session` |
| `tests/adapters_qt/review_landing_model_tests.cpp:638`, `:640` | `Destination` |
| `tests/adapters_qt/settings_window_tests.cpp:129` | `Preferences` |
| `tests/adapters_qt/workbench_window_tests.cpp:103`, `:6514` | `Preferences` |
| `tests/application/agent_invocation_host_tests.cpp:214` | `Workspace` |
| `tests/application/epoch_application_host_tests.cpp:3095` | `Workspace` |
| `tests/application/github_contract_tests.cpp:217` | `PullRequest`, `Repository` |
| `tests/application/profile_registry_tests.cpp:209` | `Profile` |
| `tests/application/profile_startup_admission_tests.cpp:28` | `Profile` |
| `tests/application/profile_storage_authority_tests.cpp:25` | `Profile` |
| `tests/application/review_mutation_application_host_tests.cpp:223` | `SelectedDiff.Request` |
| `tests/application/review_queue_enrollment_tests.cpp:73` | `Session` |
| `tests/core/revisioned_review_draft_tests.cpp:39` | `Command` |
| `tests/end_to_end/profile_activation_public_path_tests.cpp:82` | `Profile` |
| `tests/end_to_end/profile_owner_admission_public_path_tests.cpp:82` | `Profile` |
| `tests/end_to_end/profile_startup_public_path_tests.cpp:184` | `Profile` |
| `tests/github_contract/github_provider_tests.cpp:729` | `Context` |
| `tests/github_contract/github_provider_tests.cpp:1602` | `Viewer` |
| `tests/github_contract/github_read_contract_tests.cpp:142` | `Request`, `Request.Semantic` |
| `tests/github_contract/github_read_contract_tests.cpp:237` | `PullRequestRequest` |
| `tests/github_contract/github_read_contract_tests.cpp:307` | `PullRequest`, `Repository` |
| `tests/github_contract/live_review_submission_provider_tests.cpp:135` | `Proposal` |
| `tests/sqlite_contract/epoch_catalog_behavior_tests.cpp:1842` | `Destination` |
| `tests/sqlite_contract/profile_storage_root_tests.cpp:52`, `:53` | `Profile` |
| `tests/sqlite_contract/review_mutation_recovery_storage_tests.cpp:113` | `Anchor` |
| `tests/sqlite_contract/review_submission_recovery_storage_tests.cpp:66` | `Proposal` |
| `tests/sqlite_contract/window_review_restoration_storage_tests.cpp:98` | `Destination` |

## The reproducer

There are two reproducers:

- **The tool's calibration.** It runs every time the survey does. Under GCC
  15.2 each named-type shape nets 0, and each unnamed shape of the affected
  kind nets −1.
- **The station's ASan reproducer.** It carries the two real shapes BPR-0816
  fixed, with `std::string` members so that AddressSanitizer can see the
  double free. Its source follows.

```cpp
#include <cstdint>
#include <cstdio>
#include <expected>
#include <optional>
#include <stdexcept>
#include <string>
static bool throwOnCopy = false;
struct fragile { std::string Value; fragile() = default; fragile(const fragile& aOther) : Value(aOther.Value) { if (throwOnCopy) throw std::runtime_error("copy"); } fragile(fragile&&) noexcept = default; };
struct receipt { std::string ServerCommentId; bool Duplicate = false; };
struct observation { receipt Receipt; std::uint64_t DatabaseId = 0U; fragile NodeId; };
struct supplement_note { std::optional<int> File; std::optional<int> Comparison; std::string Text; };
struct note_record { std::uint64_t Id = 0U; std::optional<int> Attachment; supplement_note Note; fragile Revision; };
enum class failure { malformed };
[[gnu::noinline]] auto make_observation(const std::string& aId, const fragile& aNode) -> std::expected<observation, failure>
{
#if TYPED
	return observation{ .Receipt = receipt{ .ServerCommentId = aId, .Duplicate = false }, .DatabaseId = 1U, .NodeId = aNode };
#else
	return observation{ .Receipt = { .ServerCommentId = aId, .Duplicate = false }, .DatabaseId = 1U, .NodeId = aNode };
#endif
}
[[gnu::noinline]] auto make_note(const char* aText, const fragile& aRevision) -> note_record
{
#if TYPED
	auto note = note_record{ .Id = 1U, .Note = supplement_note{ .Text = std::string{ aText } }, .Revision = aRevision };
#else
	auto note = note_record{ .Id = 1U, .Note = { .Text = std::string{ aText } }, .Revision = aRevision };
#endif
	return note;
}
int main(int aArgc, char**)
{
	const auto longText = std::string(64, 'x');
	auto node = fragile{ }; node.Value = longText;
	throwOnCopy = true;
	if (aArgc == 1) {
		try { auto value = make_observation(longText, node); } catch (const std::runtime_error&) { std::puts("observation: caught"); }
	} else {
		try { auto value = make_note(longText.c_str(), node); } catch (const std::runtime_error&) { std::puts("note: caught"); }
	}
	return 0;
}
```

The ASan build was `g++ -std=c++23 -O0 -g -fsanitize=address -DTYPED=N`; the
abort build was `g++ -std=c++23 -O2 -DTYPED=N`. Run with no argument it builds
the observation shape; with `note` it builds the ledger shape.

| Build | Shape | `TYPED=0` (pattern present) | `TYPED=1` (pattern avoided) |
|---|---|---|---|
| `-O0` ASan | observation | exit 1, `ERROR: AddressSanitizer: attempting double-free` | exit 0, `observation: caught`, no report |
| `-O0` ASan | note | exit 1, `attempting double-free` | exit 0, `note: caught`, no report |
| `-O2` | observation | exit 134, `free(): double free detected in tcache 2` | exit 0 |
| `-O2` | note | exit 134, `free(): double free detected in tcache 2` | exit 0 |

The logs are `asan-typed{0,1}-{observation,note}.log` and
`o2-typed{0,1}-*.log`.

## The durable guard, and why

- **No compiler version gate.** GCC 15.2 is the Linux toolchain, so a gate
  would stop every Linux build. In this code base the defect needs an
  allocation failure to fire.
- **No `-O3` warning build.** It is measured insufficient: the `-O3 -Werror`
  preset built clean with ten `src/` sites present.
- **No tidy or clang-query rule.** Clang's AST does not carry GCC's lowering.
  An omitted member and "a later initializer can throw" cannot be expressed
  without heavy noise.
- **No style rule in `_coding-standard/STANDARD.md`.** It cannot cover omitted
  members without asking every aggregate initialization to name every nested
  member. The hazard is recorded where compiler-specific classes already live,
  in [build-and-test](../development/build-and-test-reference.md#gcc-15-can-destroy-a-braced-aggregate-member-twice).
- **The guard is the survey.** `scripts/survey_gcc_aggregate_cleanups.py` is
  the only method that finds these sites. It calibrates itself against the
  compiler it runs with, so a GCC upgrade that changes the lowering or fixes
  the defect cannot pass silently. It fails on `src/` findings, and it takes
  about five minutes on the Linux station. It is run by hand after a GCC
  change and on a stock build of a base that adds aggregate initializers under
  `src/`. It is not a ctest registration, because it recompiles the whole
  tree.
- **No separate canary test.** The tool's calibration is the reproducer. It
  runs whenever the survey does, which is when it matters. A separate ctest
  would only restate that GCC 15.2 is still defective.

## Independent review

An `rfc-reviewer` reviewed the candidate read-only.

**Blocking.** Its one blocking finding was that this record was still a stub.
This section and the tables above answer it.

**Non-blocking, each resolved in `de528ce3`:**

- The claim that member-initializer lists are unaffected was too broad.
- A finding's line can be the enclosing function's closing brace.
- A compile database configured from another checkout would have passed
  silently. It now fails with status 2.
- A compiler launcher was treated as "not GCC", with status 0. It now fails
  with status 2; Clang still skips with status 0.
- The fixture's `Request` relied on GCC's lowering. It is now named.

**Non-blocking, noted:**

- The detector is conservative about calls that cannot throw.
- `profile_window.hpp:81` has the same text shape and is safe.
- The calibration counts objects rather than using ASan. The ASan evidence is
  the reproducer above.

## Final run

**Revision.** `de528ce38a914d47b0e62a031f3eef38602704a8`, pushed.

**Builds (the card's third criterion, Linux half).** Both presets were
configured with `--fresh` (empty `CMAKE_CXX_FLAGS`) and built with
`--clean-first`; no class is demoted.

| Preset | Result | Steps | Duration | Diagnostics |
|---|---|---|---|---|
| `app-release` | rc 0 | 429 of 429 | 100 s (10:30:00Z–10:31:40Z) | none from first-party code; the vendored SQLite amalgamation's one `-Wstringop-overread`, as before |
| `dev` | rc 0 | 1241 of 1241 | 159 s (10:31:40Z–10:34:19Z) | none |

**The survey.**

```sh
python3 scripts/survey_gcc_aggregate_cleanups.py --build-dir build/dev --build-dir build/app-release --jobs 10
```

It passed calibration (25 shapes; 16 double-destroy; the detector flags 16).
It covered 614 units with 0 compile failures and found 30 sites, none gated.
It printed `GCC_AGGREGATE_SURVEY_OK` with status 0 after 281 s. Every `src/`
site is gone, and the 30 `tests/` sites are the ones listed above.

**The positive control** was rerun with the committed tool, with BPR-0816's fix
reverted in a snapshot. It exited with status 1, gating
`github_mutation_contract.cpp:986` and `:1016` and
`review_ledger_database.cpp:1815`.

**The tool's refusals** were checked by feeding it prepared compile databases:

| Input | Status |
|---|---|
| A launcher prefix | 2 |
| A Clang database | 0, skipped |
| A database outside `--root` | 2 |

**Focused courses.** The Secret Service `login` collection read `(<false>,)`.
The 16 names cover every touched site:

- the profile registry and storage-authority users of the header;
- the public-preview session;
- the review mutation host;
- the shared draft and round-trip probe;
- the public-path fixture.

The command started at 10:35:40Z and exited with ctest rc 8.

| Test | Result |
|---|---|
| `application_profile_registry`, `application_profile_storage_authority`, `application_profile_startup_admission` | Pass |
| `sqlite_profile_registry_storage`, `sqlite_profile_storage_root` | Pass |
| `application_profile_startup_public_path` | Pass (1.73 s) |
| `application_profile_multi_window_public_path` | Pass (9.64 s) |
| `application_github_host`, `application_anonymous_read_host` | Pass |
| `application_anonymous_public_preview_enabled_probe` | Pass |
| `application_review_mutation_application_host`, `application_review_mutation` | Pass |
| `application_shared_draft_application_host` | Pass |
| `application_threaded_reply_contract` | Pass (2.47 s) |
| `application_review_round_trip_composition_contract` | **Fail, by construction.** See below. |
| `application_anonymous_public_preview_public_path` | **Fail, stale regex; not caused here.** See below. |

**The round-trip contract.** It pins LF-normalized digests of
`github_application.cpp` and of the production app sources (BPR-0819's
reseal procedure). This card and BPR-0824 both change those files, so the pins
fail at their first check.

- **Values.** `-DBPR_PRINT_PINNED_DIGESTS=ON` prints:
  - full application `27cfb35d06023575b86f7c6bf9a3af753a2081a3192fdf8c805af87067f42cf5`
    (was `fed42059…`);
  - production app manifest `c2fd7749a7523c5b94550f88f655074bc09c4eb971d46ed2cdcf040a2d0cb3c9`
    (was `b4425715…`);
  - production mutation branch `a7e2081e…`, unchanged.
- **The rest of the course passes.** A scratch copy of the script outside the
  checkout, with only those two values replaced, printed
  `REVIEW_ROUND_TRIP_COMPOSITION_CONTRACT_OK`.
- **The reseal is left to main.** It follows the documented procedure and
  names the reviewed changes, as `99712ec3` did after BPR-0817's Linux change.
  The reviews it would cite are recorded here and in the BPR-0824 evidence.

**The public-preview course.** Its pass regex
(`tests/end_to_end/CMakeLists.txt:2172`) expects
`PUBLIC_PREVIEW_HISTORY_INCOMPLETE:true` to be followed directly by
`PUBLIC_PREVIEW_SMART_ACQUISITIONS:1`. The application has printed a
`PUBLIC_PREVIEW_EPOCH_LINE_HISTORY:` line between them unconditionally since
`26fd7f7c` (2026-09-12, `main.cpp:4161`).

- **Acquisitions vary.** Across four runs `PUBLIC_PREVIEW_SMART_ACQUISITIONS`
  read 2, 1, 2 and 2.
- **A/B.** Under one build-lock hold, the five BPR-0822 source files were set
  back to their pre-change contents and the application was rebuilt. Three
  runs failed identically: the line was present and acquisitions read 2 each
  time. The files were then restored and the tree rebuilt; `ninja -n` reported
  no pending work.

**Tidy.** Over the touched translation units, `tidy.py --warnings-as-errors`
reports only findings that predate this card (see the BPR-0824 evidence). The
header is checked through its includers.

**Format and diff.** `clang-format` 22 `--dry-run --Werror` passes, and so
does `git diff --check`.

**clang 22 proxy.** These compile clean with `clang++-22` and the project's
clang warning set under `-Werror`:

- `github_application.cpp`, `github_public_path_fixture.cpp`;
- `github_application_host.cpp`, `review_mutation_application_host.cpp`;
- `profile_registry_store.cpp`, which includes the header.

The independent review also compiled the header's default against GCC and
clang. Neither AppleClang nor MSVC compiled the change.

**Portable sources touched.**

- `src/application/include/bpr/application/profile_registry.hpp`
- `src/app/src/github_application.cpp`
- `src/app/src/github_public_path_fixture.cpp` (tests-enabled fixture)
- `src/application/src/github_application_host.cpp`
- `src/application/src/review_mutation_application_host.cpp`

`scripts/survey_gcc_aggregate_cleanups.py` is new. None has a platform branch.

**Against the card's acceptance.**

- **Criterion 1, the method and every site with its disposition, listed on
  the card: met.** See the card and the sections above.
- **Criterion 2, the reproducer: met.** Under GCC 15.2 the ASan reproducer
  above reports nothing with the pattern avoided and `attempting double-free`
  with it present. The tool's calibration, the committed reproducer that runs
  with every survey, nets 0 for each named shape and −1 for each unnamed one.
- **Criterion 3, stock builds: met on Linux** (`dev` and `app-release`).
  **The macOS dev build was not run**; this station cannot run it.

**Logs.** `/home/mmarcin/bpr-station-logs/task-20260917-lnx-verify/`:

- `bpr0822/`: `tool-final.log` and `.json`, `tool-base.*`, `tool-control-final.log`,
  `survey.summary`, the shapes and ASan logs, and the snapshots;
- `final-0822.ctest.log`, `final-0822-preview-rerun*.ctest.log` and `ab-0822-*`;
- `roundtrip-print-final.log` and `roundtrip-scratch-final.log`;
- `stock.summary` and `clang-proxy-*.log`.

None is committed.
