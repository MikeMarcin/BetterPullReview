---
schema: "implement-rfc.card.v1"
id: "BPR-0711"
title: "Admit one native runtime owner per profile"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0710"]
metadata: {"dispatch":"UX-2026-09-08-D3","mandate_revision":1}
acceptance: [{"criterion":"Exclusive native profile admission before services","outcome":"pass","revision":"3e7a26d61c1fed4d146f88d8c88b08f0e489213b","evidence":{"kind":"command","detail":"build/dev/bpr0711-integration.log: all11 pass174.06s; native lease and actual six-process fixture course prove owner_busy, independent profiles, unsafe/cancelled refusal, process-death release and macOS exec non-inheritance. Windows/Linux and activation transport remain explicitly unaccepted."}},{"criterion":"Contention preserves durable windows and one-shot startup","outcome":"pass","revision":"3e7a26d61c1fed4d146f88d8c88b08f0e489213b","evidence":{"kind":"observation","detail":"Actual process course2.53s compares settled owner window rows after competing launch and synchronous retry during refusal. Startup callback count is exactly one. Critical review's retry bypass and libc++ moved-function callback defects corrected; no remaining source blocker."}},{"criterion":"Lease outlives profile consumers and safe reacquisition","outcome":"pass","revision":"3e7a26d61c1fed4d146f88d8c88b08f0e489213b","evidence":{"kind":"command","detail":"Final strengthened startup public course1.76s in build/dev/bpr0711-lifetime.log proves exact owner_busy while runtime alive and while drained facade retained, then successful reopen after disposal. Existing multi-window67.69s and restart6.79s pass."}},{"criterion":"Integrated available-target production boundary","outcome":"pass","revision":"3e7a26d61c1fed4d146f88d8c88b08f0e489213b","evidence":{"kind":"command","detail":"Changed-file format/tidy pass; tests-disabled Release build and30-required/no-seven-forbidden host inventory pass;52 composition and62 documentation regressions pass. Documentation gate passes. BPR0595 preserves full AC06 activation, native/human and canonical dependency obligations."}}]
---

# Admit one native runtime owner per profile

## Scope and acceptance

Bounded AC-06/09 safety foundation: retain an exclusive native lease before
profile domain stores or services start. Same-root competing runtime startup
must fail explicitly; distinct profile roots remain independently admissible.
Keep the lease until profile consumers and effects are drained. Process death
releases native ownership without PID-age or timeout-based lock stealing.

Desktop Open Profile must acquire admission before changing that profile's
durable window generations. Existing owned-profile New Window continues through
the management host. Rejected admission preserves the existing owner's durable
windows and services. Root/symlink/permission failure remains fail-closed.

Cross-process activation and explicit new-window transport remain a required
subsequent slice, not an accepted claim of this foundation. This card does not
close AC-06 or infer native package evidence from fixtures.

## Ownership and verification

Owned paths: application admission contracts, native storage-root lease adapter,
profile startup/desktop integration, focused native and composed process tests,
owning architecture/startup documentation and this card/closeout.

Verify actual competing processes, same-process duplicate admission, independent
roots, release/crash recovery, unsafe paths, cancellation and desktop contention
before window mutation. Run focused startup/management/lifecycle integration,
changed-file format/tidy and documentation gates. Independent critical review
targets ownership lifetime and admission ordering.

## Accepted available-target evidence

Source/test candidate: `3e7a26d61c1fed4d146f88d8c88b08f0e489213b`.

- `ctest --preset dev -R '^(architecture_boundaries|architecture_boundary_contract|application_profile_(startup_public_path|management_public_path|multi_window_public_path|owner_admission_public_path|window_restart_public_path|startup_admission|management_host|registry)|sqlite_profile_runtime_lease)$' --output-on-failure`
  passes all 11 checks in 174.06s (`build/dev/bpr0711-integration.log`). The
  actual-process owner course passes 2.53s; existing sibling-window course 67.69s
  and durable restart course 6.79s pass.
- The owner course launches six actual processes including registry seed. It
  proves exact `owner_busy`, one startup terminal, unchanged owner window rows,
  concurrent distinct profiles, synchronous Open retry during refusal publication,
  settled registry state before inspection, orderly release and forced-death
  reacquisition. The native lease test additionally covers independent roots,
  same-process/shared-handle contention, unsafe roots, cancellation and exec
  non-inheritance on macOS. No PID-age override or stale-lock deletion is used.
- Final strengthened `application_profile_startup_public_path` passes 1.76s
  (`build/dev/bpr0711-lifetime.log`): duplicate admission remains exactly busy
  while live and after shutdown completion while the runtime facade is retained;
  normal facade disposal permits reopen. This closes the live-process lifetime
  evidence gap identified during review.
- Changed-file formatting passes for 17 C++ files (`bpr0711-format.log`). Native
  lease worker tidy and final composition/host/test tidy pass; integrator logs
  `build/dev/bpr0711-tidy.log` and `bpr0711-final-tidy.log` contain the seven-TU
  pass and three final rechecks. Documentation gate and 62 documentation
  regressions pass (`bpr0711-docs.log`, `bpr0711-documentation-tests.log`).
- Tests-disabled production build passes (`build/app-release/bpr0711-build.log`)
  and composed-host inventory finds 30 required/no seven forbidden hosts
  (`bpr0711-host-inventory.log`). The 52 composition-inventory regressions pass
  (`build/dev/bpr0711-bypass.log`). No live provider request/mutation was run.

## Review and corrected failures

Independent native Codex critical review found two source failures: a second Open
could bypass admission while a failed startup entry awaited timer retirement, and
moving a small `std::function` did not guarantee clearing its source on libc++, so
shutdown could deliver cancellation after owner contention. The final guard refuses
closing/unadmitted entries and callback consumption uses explicit exchange. Direct
regressions cover both. Review found no remaining source blocker; opposite-family
review was unavailable. Requested review route was Astra xhigh; serving telemetry
is not independently established by that banner.

Earlier course failures are not acceptance: the first exposed the duplicate
startup terminal, and the later shutdown timeout came from a test waiting on Open
button enablement instead of the registry-idle Reload button. The final course waits
for actual registry settlement. No intermediate failed run is reported as passing.

## Remaining requirements

This is the native admission safety foundation, not full AC-06 acceptance.
Activation/new-window transport and stable-profile command-line launch selection
remain next. Ordinary duplicate launch currently emits `PROFILE_ALREADY_OPEN`;
Open Profile presents an explicit owner-contention message. Concurrent registry
changes retain existing explicit Reload/CAS recovery. Native Windows/Linux and
packaged interaction are unobserved; Windows code is guarded implementation, not
target-native proof. No new UI layout or subjective/native focus claim is made.
