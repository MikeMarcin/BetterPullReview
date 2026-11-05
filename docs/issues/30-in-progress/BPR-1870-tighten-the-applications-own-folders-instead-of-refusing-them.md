---
schema: "implement-rfc.card.v1"
id: "BPR-1870"
title: "The application makes its own folders private instead of refusing them, and never changes anyone else's"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"bug","found_by":"Owner, 2026-11-04, on BPR-1868: \"I dont like refused, i want the app to just work for users. auto fix is probably preferred, ask and fix is okay if its infrequent, failure is not good\"; then: \"cant tightening a folder break other apps using that folder?\"","reservation":"docs/issues/_reservations/BPR-1870--BPR-1870--63ee0a0384ef.json","related_cards":["BPR-1868","BPR-1857","BPR-0817","BPR-0825","BPR-0436"],"frontend_design":"not loaded: no surface the owner looks at changed in layout; status-bar refusal sentences were reworded and diagnostic lines added"}
acceptance: [{"criterion":"AC1 An existing data root that is the application's own (empty, or holding its state/v1) and this user's is narrowed to 0700 in place and starts; a copied data folder with 0755 folders and 0644 files starts and every store narrows its own folders and files","outcome":"pass","revision":"e2b7c00d4","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev -R '^(application_startup_data_root|sqlite_storage_root_contract)$' --output-on-failure","detail":"macOS arm64 dev. A root made with mode 0755 and nothing in it starts (exit 0), stderr has STORAGE_PRIVATE_REPAIRED:registry-root:, the root is 700 with state/v1, and the folder above it stays 700. A private root started once, then loosened with chmod -R go+rX, starts again; the root, state, state/v1, logs and agent-workspace-ownership-v1 end 700, epoch-catalog.sqlite3 and profile-registry.sqlite3 end 600. sqlite_storage_root_contract: tighten_existing_storage_root narrows an empty 0755 root, then answers already_private; a loosened copied catalog root is refused before narrowing and opens after, with state, v1 0700 and the catalog file 0600."}},{"criterion":"AC2 A named folder that holds other files keeps its mode and files; the data is kept in a private BetterPullReview/data inside it, and later starts keep using it","outcome":"pass","revision":"e2b7c00d4","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev -R '^(application_startup_data_root|sqlite_storage_root_contract)$' --output-on-failure","detail":"A 0755 folder holding a 0644 notes.txt starts twice (exit 0), each time printing STORAGE_PRIVATE_NESTED:registry-root:; the folder stays 755, notes.txt 644, its entries are exactly BetterPullReview and notes.txt, BetterPullReview and BetterPullReview/data are 700 and hold state/v1. In the contract, storage_root_placement chooses the nested root, tighten_existing_storage_root answers foreign_content and changes nothing, and the nested root stays chosen after the outer folder is made 0700."}},{"criterion":"AC3 A link is never followed and its target never changed","outcome":"pass","revision":"e2b7c00d4","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev -R '^(application_startup_data_root|sqlite_storage_root_contract|agent_workspace_ownership)$' --output-on-failure","detail":"A data root that is a symbolic link to a 0755 folder is refused (STARTUP_FAILED, STARTUP_FAILURE_SOURCE:registry-root-link:) and the target stays 755 and empty. tighten_existing_storage_root and make_storage_root_owner_private refuse a linked root, target unchanged (make_storage_root_owner_private used a path-based permissions call that followed the link; it now narrows through an O_NOFOLLOW descriptor after an owner check). A store file that is a link to a 0644 file is refused and the file keeps 644. A neighbourhood that is a link to a 0755 folder is refused as neighbourhood-not-private with no repair, and the target keeps 755."}},{"criterion":"AC4 The application's own agent workspace neighbourhood, registry and records are narrowed in place; the folder that holds the neighbourhood is never changed","outcome":"pass","revision":"e2b7c00d4","evidence":{"kind":"command","command":"ctest --preset dev -R '^agent_workspace_ownership$' --output-on-failure","detail":"macOS: a 0755 neighbourhood recovers (recovered=0 live=0 repaired=neighbourhood-not-private) and ends 0700 with its parent unchanged; a 0755 registry recovers with repaired=registry-not-private and ends 0700; a 0644 record of an abandoned ready workspace recovers (recovered=1 live=0 repaired=record-not-private) and the workspace is reclaimed. BPR-0817's shared parents (0775, 1777) are still refused as neighbourhood-parent-shared and keep their modes. The application prints AGENT_WORKSPACE_PRIVATE_REPAIRED:path=omitted:check=<name> per repair."}},{"criterion":"AC5 Windows: build, and the same courses on the station","outcome":"pending","revision":"e2b7c00d4","evidence":{"kind":"command","command":"On bpr-main-win: cmake --preset windows-dev; cmake --build --preset windows-dev --target app_better_pull_review test_better_pull_review_sqlite_storage_root_tests test_better_pull_review_agent_workspace_ownership_tests tool_better_pull_review_network_sandbox; ctest --preset windows-dev -C Debug -R \"^(sqlite_storage_root_contract|agent_workspace_ownership|architecture_boundaries)$\" -V","detail":"Not run: the station was not used (BPR-1869 may hold it). The Windows code is not compiled yet: tighten_workspace_object's descriptor branch in installed_git_process.cpp and windows_private_descriptor::acl(). agent_workspace_ownership's Windows negative-acl case now expects the Everyone-readable record to be narrowed (recovered=1 live=0 repaired=record-not-private)."}},{"criterion":"AC6 Neighbouring courses, format and documentation","outcome":"pass","revision":"e2b7c00d4","evidence":{"kind":"command","command":"QT_QPA_PLATFORM=offscreen ctest --preset dev -j8 -R '^(architecture_boundaries|architecture_boundary_contract|sqlite_.*|application_profile_startup_admission|application_profile_startup_public_path|application_profile_management_public_path|application_startup_failure_source|application_startup_data_root|application_private_draft_restore|application_logging.*|logging_.*|agent_workspace_ownership|application_agent_crash_recovery|application_agent_materialization_crash_recovery)$'; cmake --build --preset dev --target coding-format-check; python3 scripts/verify_documentation.py --root .","detail":"51 of 51 pass after three courses that asserted the old refusal were changed to the new policy: sqlite_review_ledger_storage (a group-readable state/v1 is now narrowed to 0700 by whichever store opens first), sqlite_profile_storage_root (a permissive profile root under its private container is narrowed and admitted; a permissive base is still refused by the store itself, unchanged) and application_profile_startup_public_path (its refused root is now a link to a 0740 folder, which is refused and left unchanged and unwritten). clang-format 22 check passed on 1943 files; DOCUMENTATION_OK."}}]
---

# BPR-1870: The application makes its own folders private instead of refusing them, and never changes anyone else's

## Report

The owner, 2026-11-04, on [[BPR-1868]], which kept the rule that an existing
data root that is not owner-private is refused at startup: "I dont like
refused, i want the app to just work for users. auto fix is probably
preferred, ask and fix is okay if its infrequent, failure is not good". Then,
on the first proposal to narrow such folders: "cant tightening a folder break
other apps using that folder?"

## Policy

Only folders and files the application itself creates and names are narrowed:
the data root when it is the application's own, `state`, `v1`, `profiles`, a
profile's root, `logs`, the store files, the agent workspace neighbourhood
(`<data root>-agent-workspaces-v1`), its ownership registry and records.
Nothing else uses them. A folder above them, or a folder the user named that
holds other content, is never changed: a sync client, a backup tool or a
project may depend on its mode.

**What counts as the application's own data root.** An existing folder is
the application's own when it is empty, or when it holds `state/v1`, which
the stores create on the first launch over any root. The empty case is what
`mkdir -p` gives; the `state/v1` case is a copied, restored or loosened data
folder. Any other content (one stray file is enough) makes it someone else's.

**Narrowing is safe only as an exact, unfollowed operation.** Every narrowing
opens the object itself with `O_NOFOLLOW` (relative to its held parent where
there is one), checks that the opened object is a real folder or a
single-link regular file owned by the effective user (and, for a root, the
named one by device and inode, with its own canonical spelling), and then
calls `fchmod` on that descriptor: `0700` for a folder, `0600` for a file. On
Windows it opens with `FILE_FLAG_OPEN_REPARSE_POINT`, refuses a reparse
point, verifies the owner is the token user and writes the exact protected
owner-private descriptor through the handle. A link is never followed, so
nothing outside is ever changed, and nothing is ever made wider for anyone
but its owner.

## Inventory of refusal points and what each now does

| Check | Where | Policy now | Why |
|---|---|---|---|
| Data root exists, not owner-private: empty or holds `state/v1`, this user's | startup host (`github_application.cpp` `load`), `tighten_existing_storage_root` | **Auto-fix**: narrowed in place, `STORAGE_PRIVATE_REPAIRED:registry-root:` | The application's own folder; nothing else uses it |
| Data root exists, holds other content, or is another account's | `storage_root_placement` (composition constructor) | **Fallback**: the stores use `<root>/BetterPullReview/data`, created 0700; the named folder is untouched; `STORAGE_PRIVATE_NESTED:registry-root:` each launch | Changing the folder could break other programs; a private child protects the data as well, and its own private parent keeps the agent workspace neighbourhood out of a shared folder. Once the nested root exists it is always chosen, so later changes to the outer folder never strand the data |
| Data root is a link, or not a folder | startup host | Refused as before (`registry-root-link`, `registry-root-not-directory`) | A link may name anything; following it to change modes is exactly what must not happen. The message names the cause |
| Data root not owned by this user (race between placement and narrowing) | startup host | Refused: `registry-root-owner`, says to name a folder you own or a new one | Only the owner can narrow it |
| `state`, `v1`, `profiles`, profile roots wider than 0700 | `ensure_private_child` (SQLite, POSIX) | **Auto-fix** through the opened child under its held private parent | Adapter's own folders |
| Store files wider than 0600 | `admit_private_store_file`, the catalog's `validate_database_file` (POSIX) | **Auto-fix** through `openat` relative to held `state/v1`; a file already private is only examined, never opened, because closing a descriptor drops the process's POSIX record locks, SQLite's included | Adapter's own files. A link or a file with more than one name is still refused |
| `logs` folder and log files wider than private | `session_files.cpp` (POSIX) | **Auto-fix** through the opened descriptors | Adapter's own |
| `neighbourhood-not-private` | `prepare_workspace_neighbourhood` | **Auto-fix**; `AGENT_WORKSPACE_PRIVATE_REPAIRED:path=omitted:check=neighbourhood-not-private`. A link or another account's still refused, message says so | Application-named folder |
| `registry-not-private` | `prepare_workspace_registry` | **Auto-fix** of an existing registry | Application-named folder |
| `record-not-private` | `recover_workspaces_locked` | **Auto-fix** of a single-link record of this user's | Application-named file; identity checks that follow are unchanged |
| `data-folder-not-private` | `prepare_workspace_registry` | Unchanged in the Git adapter; the message now says a restart makes it private | Startup already narrows the application's own data root; the Git adapter cannot tell it from a folder other programs use |
| `neighbourhood-parent-shared` | `workspace_parent_trusted` | **Unchanged refusal**, never changed | The parent is not the application's (owner, 2026-11-04). The security reason stands: an account that can write the parent can rename the neighbourhood and substitute its own between check and use, and the agent would then run Git in a folder that account controls (hooks). The nested fallback gives the neighbourhood an application-made private parent, so a user-named shared folder no longer reaches this. What remains is a default layout under a shared parent (Linux `~/.local/share` at 0775, a Windows parent with a foreign write grant); see Open |
| `make_storage_root_owner_private` (POSIX) | callers' helper | Hardened: `O_NOFOLLOW` descriptor, owner check, `fchmod` | It followed a link through `std::filesystem::permissions` |

No "ask and fix" prompt was added: every case the application may change is
safe to change silently, and every case it may not change has either a
fallback (nesting) or a precise message.

## Built

- **SQLite adapter** (`storage_root.hpp`, `catalog_database.cpp`):
  `storage_root_placement` (read-only), `tighten_existing_storage_root`
  returning `already_private`, `tightened`, `not_owned`, `foreign_content` or
  `refused`, `nested_storage_folder`/`nested_storage_leaf`,
  `private_file_at`, narrowing in `ensure_private_child`,
  `admit_private_store_file` and `validate_database_file`, and the hardened
  `make_storage_root_owner_private`.
- **Logging adapter**: `logs` and log files narrowed through their descriptors.
- **Git adapter**: `tighten_workspace_object` / `admit_workspace_object`
  (POSIX and Windows), `workspace_recovery_refusal::Repaired`.
- **Agent adapter**: `windows_private_descriptor::acl()`, the same platform
  descriptor's list, for writing onto an existing object.
- **Composition**: the constructor places the registry root; `load` narrows an
  existing root and prints the repair or nesting line; the workspace recovery
  prints one line per repair; refusal sentences say what is left and what to do.
- **Tests**: `application_startup_data_root` (empty 0755 root narrowed, copied
  loosened root, foreign-content nesting twice, linked root),
  `sqlite_storage_root_contract` (`prove_existing_root_tightening`),
  `agent_workspace_ownership` (`check_own_objects_narrowed`; Windows
  negative-acl expectation updated).

## Verification

macOS arm64, `dev` preset, at `e2b7c00d4`: the AC1 to AC4 courses and the
51-course neighbouring selection in AC6 pass, the clang-format 22 check
passes and the documentation gate reports `DOCUMENTATION_OK`. Linux was not
run; the POSIX code paths are shared with macOS except `stat`'s format in the
end-to-end contract, which picks `-c %a` off Apple.

Three courses asserted the old refusal and now assert the new policy (AC6).
One of them, the ledger's, carried the argument against narrowing: "whoever
set 0755 may already have looked". Narrowing cannot undo a look that already
happened, and refusing could not either; narrowing stops the next one, and the
owner chose working over refusing.

## Open

- **Windows (AC5)**: the descriptor branch of `tighten_workspace_object` is
  uncompiled; the station runs are pending. On Windows the SQLite children
  (`ensure_private_child`) and store files keep their existing rules (a file
  with an inherited private grant is made explicit; anything wider refused).
- **Shared parent of the default layout** (`neighbourhood-parent-shared`): the
  default Linux data root `~/.local/share/BetterPullReview` under a 0775
  `~/.local/share`, or a Windows `%LOCALAPPDATA%\BetterPullReview` parent with a
  foreign write grant, still refuses agent workspaces (startup is unaffected).
  A fix is a layout change: keep the neighbourhood under an application-made
  private parent (as the nested fallback already does), with a migration of
  the recorded neighbourhood identity. Not done here.
- A Windows root owned by `BUILTIN\Administrators` (first run elevated) is not
  re-owned; taking ownership needs `WRITE_OWNER` and was not attempted.
- Internal private-folder checks below the stores (Git caches, release
  maintenance files) were not audited for copied folders beyond the AC1 start.
