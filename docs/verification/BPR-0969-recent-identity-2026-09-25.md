# BPR-0969: same-profile Recent account isolation

## Bound scope and result

Assignment `/root/bpr0969_recent_identity`, mandate
`r4-standalone-recent-identity-20260925`, source/tracker
`/Users/jaeger/Documents/GitHub/BetterPullReview`, baseline
`566b7802a989718de5d5fe3734a9997230a3d612`. The selected MASK
coordinator-implementer recipe revision 2 ran as one native Codex
`rfc-implementer` leaf. No delegation, live account, remote mutation, release,
Claude Code or Windows/Linux qualification occurred. Existing RFC0052 attended
resources were untouched.

The assigned Recent title/destination/saved-reading repair is complete at source
`da3eea5c5691661fa90058735b8cfa119c6be9f9`. The canonical AC2 remains **partial/pending**, and AC3 remains
unfinished: this slice creates no durable PR snapshot or offline account
admission. The prior [AC1 inventory](BPR-0969-offline-inventory-2026-09-25.md)
and RFC0013 owner amendment r4 retain their scope.

## Demonstrated failure and repair

The prior product stored Recent destinations by mode/host/repository/PR in one
profile without an account. Its projection admitted any authenticated account
on that host. Sign-out also published its transition before clearing the old
accepted session; Recent therefore still used that old session for filtering.

The failing-before run used unchanged product files from `a2c65b71`, with only
the new fixture/course, against generated accounts 910001 and 910002. Account A
opened PR17, loaded both source panes and saved a reading. The course drove the
actual Sign out control, waited for both synthetic vault slots to be absent,
the server card to be disconnected and the ready statuses to retire, then
signed in as B. The settled sign-out retained **one** authenticated Recent row;
B's accepted session still exposed A's title and destination. The assertion
`another account cannot see the first account's title or destination` failed in
22.32 seconds. At the failing B observation, snapshot, source and comparison
were all absent; the demonstrated exposure was the persisted Recent metadata.
No live provider was involved.

The core destination now includes the accepted account ID. Its equality keeps
same-PR entries for different accounts distinct, and `matches_review_authority`
requires exact host, mode and account; zero cannot authorize an authenticated
entry. New authenticated accepted reads refuse an unknown account. Composition
supplies the account only from an accepted snapshot/handoff, checks the current
core account during identity retirement, filters before publishing to Qt, and
rechecks both Open and Resume. The index map updates even when visible rows are
unchanged; gestures also check the projected destination still matches the row.

SQLite schema five appends canonical decimal account identity to both window
records and Recent entries. It validates the old schema and bounded payloads
before transactional migration. Versions one through four gain account `0`,
meaning unknown for authenticated data. Old titles, destinations, positions and
tabs remain in private storage but are hidden; loading never assigns them the
current login. A later authenticated read creates a separate known-account row.
Anonymous rows keep account zero and their existing behavior. The shared
profile bound remains ten Recent entries; this is not a per-account quota.

Source/cache authority, epoch keys, anchor OIDs, position bytes and tab payload
formats are unchanged. The existing host still owns coalesced revision-aware
persistence on its effect lane and quit-time draining. Qt retains its narrow
projection/gesture contract and gains no effects or authority decisions.

## Exact public course

`application_recent_account_isolation` uses the real desktop composition,
public controls, provider/host completion lane, installed Git actor and private
SQLite stores over four actual process lifetimes:

1. A opens PR17 and saves an exact reading. A refresh completion is held; after
   settled sign-out B is accepted, and the old completion is released. No A
   title, destination, snapshot or source reappears. B opens PR18 and gets only
   its own row. Switching back to A reveals only A's row; Resume restores A's
   original exact comparison key.
2. Restart revalidates A and shows only A's saved row, without a PR snapshot.
   Offline Resume reaches a wire failure and explicitly unavailable threads and
   source. Reconnection opens exact A content. B then signs in, sees only B's
   retained row, and its offline Resume likewise stays unavailable. A returns.
3. A fully offline restart cannot revalidate its remembered grant. No
   authenticated Recent entry, accepted PR snapshot or source is offered.
4. The closed private test store is downgraded to the exact v4 schema. A real
   authenticated restart migrates it and exposes no legacy row. A fresh accepted
   read of PR17 creates an A-owned entry, leaving both old rows unknown.

After every process exits through public Quit and host quiescence, the driver
checks exact persisted account/PR rows and nonempty saved position bytes. It
also compares every retained Git pack and complete epoch authority, scope,
material and anchor row to the seed. They stay unchanged. Synthetic vault
material travels in bounded stdin/stdout pipes; diagnostics and local renders
contain generated fixture data only. Private temporary roots are removed after
all handles close.

The 1500x950 Home raster after B signs in has no Recent row; the later Home
raster shows only B's PR18/title and the visible notice that saved reading could
not be resumed. The legacy authenticated Home also has no Recent row. These
inspected QWidget captures use Qt 6.11.2 offscreen on macOS arm64, not the native
compositor or a human verdict. Restored geometry can change later capture size.

## Verification

The final source candidate passed all **13/13** focused integration tests in
**888.62 seconds**:

| Test | Seconds |
|---|---:|
| `architecture_boundaries` | 20.39 |
| `architecture_boundary_contract` | 99.94 |
| `test_network_isolation_manifest` | 5.32 |
| `core_window_review_restoration` | 0.46 |
| `application_github_window_read_host` | 0.33 |
| `application_window_review_restoration` | 0.32 |
| `sqlite_window_review_restoration_storage` | 0.71 |
| `qt_composed_github_scale_100` | 3.45 |
| `application_profile_two_account_isolation_public_path` | 13.40 |
| `application_profile_window_review_restoration_public_path` | 564.73 |
| `application_document_tabs_restoration_public_path` | 92.98 |
| `application_offline_inventory` | 25.24 |
| `application_recent_account_isolation` | 61.32 |

The core-only configuration/build passed `architecture_boundaries` and
`core_window_review_restoration` (2/2, 25.35 seconds). Canonical formatting passed
for all ten touched C++ files. Documentation validation returned
`DOCUMENTATION_OK`; all 64 documentation regression tests passed (1.913 seconds),
and the scoped board was valid with 1,186 cards. `git diff --check` passed.
No broader suite, native acceptance or release qualification is claimed.

The durable handoff names exact commands and executable/source hashes. Ignored
logs, including the valid failing-before control and the final per-phase
`integration-details.log`, remain in `build/bpr0969-recent-identity/`.

Core coverage includes exact account/host/mode admission, unknown ownership
refusal, same-PR two-account rows, separate titles and positions, and refusal of
a late old-account reading. Store coverage includes v1–v4 migrations,
canonical/full-width account IDs, malformed/overflow account rejection,
anonymous-account refusal, preserved titles/positions/tabs and transaction
cancellation. Cancellation before commit rolls back the old schema and data;
a complete COMMIT may win a late cancellation, in which case retry reads the
entire valid migrated snapshot. No partial schema is accepted.

The fixture setup required three corrections before the valid negative control:
a constant descriptor cannot be reassigned; altered PR titles must match search
results; and sign-in landing state alone is too early to prove sign-out settled.
The initial migration test also incorrectly expected a completed COMMIT to roll
back after cancellation. These were diagnosed harness expectations, not passes.
The bounded repair then passed its direct public course and focused regressions.

## Remaining obligations and continuation

Full AC2/AC3 remain pending: design the durable review snapshot, offline
admission and freshness contract; exercise changed-head/revision reconnection,
broader active Git outage/cancellation combinations, public preview offline
behavior, observed-empty conversations and crash restart as appropriate. Native
and required human evidence remain unsupplied; Windows/Linux are owner-deferred.
Retained Git/catalog data must not be promoted into offline account authority.

The [durable handoff](../rfcs/plans/bpr0969-offline-inventory/recent-identity-handoff.json)
records candidate hashes, commands, ownership and the next action. The shared
checkpoint returns `action=stop` and `engineering_acceptance=not-evaluated` for
the partial `continue-milestone` observation: AC2 remains pending while the four
assigned repair exits pass. This consistency result does not certify whole-AC2
or whole-card acceptance. An initial proposed AC4 was removed
on coordinator direction; the canonical card still has only AC1–AC3.
