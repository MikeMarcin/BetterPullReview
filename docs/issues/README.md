# BetterPullReview issue board

Use MASK-managed implement-rfc and `.mask/skill-config/implement-rfc/` context.
Cards use `schema: "implement-rfc.card.v1"`, `id`, `title`, `status`, `rfcs: []`, and `depends_on: []`. Closeout associations use `closeout_for: []`; descriptive data belongs in `metadata`. Each field occupies one line with a JSON value. Markdown bodies retain acceptance and evidence.

Common lanes are 00-inbox, 10-backlog, 20-ready, 30-in-progress, 40-parked, 50-blocked and 60-done. Folder and status agree. `platforms: ["win"]` (or mac/linux) filters eligibility; unmet dependencies belong in Backlog. Human waits use `parked_on: "human-review"`; `awaiting-live-github` remains a service wait. Required human acceptance remains open.

```sh
python3 .agents/skills/implement-rfc/scripts/issue_board.py --repo-root . queue --rfc RFC-BPR-0008 --platform mac --json
python3 .agents/skills/implement-rfc/scripts/issue_board.py --repo-root . queue --human-waiting --json
python3 scripts/allocate_issue_id.py --count 1 --reserve-for local-main --json
```

Allocation occurs on main and returns counter/immutable receipt paths to commit together. Workers consume reserved IDs; never recycle a reservation. Use shared `claim` and acceptance-driven `finish` for narrow explicit commits. See the shared bookkeeping contract at `.agents/skills/implement-rfc/references/bookkeeping.md`.

Prepare attended sessions with shared attended-review. Review ready items immediately, save partial decisions and pause when needed. Pause retains relevant protection; explicit resume-development releases only selected resources. Prefer local HTML and practical agent-captured observations; pure HTML copy/paste remains supported. Wizard guides manual steps.

Ordinary slices use focused checks, format touched C++ files, and reuse evidence whose inputs are unchanged. Pack/milestone completion normally includes one integration-focused pass; full relevant suites and critical review are appropriate when scope warrants them. There is no default exhaustive matrix, hermetic replay or manual gate.

Two selector traps are recorded because both have cost a day. A selector chosen
by *subsystem* name misses the suites named after the *composition* that holds
it — [[BPR-0418]] ran `github_conversation|workbench|thread_map` green and broke
`qt_composed_github`, which builds the same window through the same drawer. And
a diff that adds or changes a `LOOPBACK_PORT_COUNT` must run
`test_network_isolation_manifest`, because no ordinary selector matches it and
[[BPR-0452]] sat red for a day between the registration and the next full
matrix.

## Programs and history

RFC and card dependencies determine closeout eligibility; the RFC index (`../rfcs/README.md`) records status and explicit owner scheduling decisions. Index position is not a gate. Required real-platform evidence remains valid. Split portable and platform work only when the portable portion can be verified independently. [HISTORY.md](HISTORY.md) retains the historical narrative; it is not a duplicate live ledger.

Closeout runs `python3 scripts/verify_documentation.py --root .` and records direct criterion/check/result/revision evidence. Specialized RFC-0016/17 historical contracts remain repository verification tools under `scripts/verification/rfc0016_attended_contract/`.

RFC-0014 owner amendment r4 accepts only the implemented release foundation.
RFC-0038/BPR-0973 now owns complete native and release-security qualification;
unchanged-criteria BPR-0958–0962 remain Blocked as standalone follow-ups.
Historical M8 has partial native candidates, not platform-matrix acceptance.
BPR-0958's original source/map and maintenance questions are accepted only for
their exact candidate. A reproduced initial-load failure now returns to Home;
its exact revised recovery question was accepted at hand-back 460. BPR-0958
is Blocked on clean-account Keychain/authentication, full purge and residual
evidence; the three scoped human accepts do not complete native qualification.
BPR-0959 needs a native Intel Mac; BPR-0960 needs clean ordinary-user Windows
11 desktop/auth/vault evidence on JaegAW; M9 completed exact PE mechanics but not
those native courses. BPR-0961 now needs Ubuntu 26.04 WSL2/WSLg ordinary-user
desktop/vault and exact-package evidence on JaegAW under the owner's 2026-10-11
and 2026-10-16 amendments (confirmed by BPR-1743); Ubuntu 24.04/RHEL are outside
the selected set. Windows 10 and other Linux environments are untested, not
refused. The
[M8 ledger](../verification/RFC-BPR-0014.md#milestone-8-native-candidates-and-pending-acceptance)
retains the observed fixes and exact limitations. The
[successor ledger](../verification/RFC-BPR-0038.md) preserves Qt 6.11.2,
separate source assets and raw 32/30 Grype failures. The Windows
licensing-v002 question, unanswered at cursor 478, was accepted for its exact
inventory on 2026-09-24 (decision `afa8af40`, hand-back 589). No release
approval is implied.
