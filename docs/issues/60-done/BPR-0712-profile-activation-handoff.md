---
schema: "implement-rfc.card.v1"
id: "BPR-0712"
title: "Route profile activation and new-window launches to the runtime owner"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0711"]
metadata: {"dispatch":"UX-2026-09-08-D4","mandate_revision":1}
acceptance: [{"criterion":"Bounded lease-authenticated native activation protocol","outcome":"pass","revision":"a654669c7475d5b0254e071bbbdd25b3e4dfc20b","evidence":{"kind":"command","detail":"sqlite_profile_activation and sqlite_profile_runtime_lease pass2/2 in4.85s on macOS; independent processes, exact identity/framing, hostile peers, lease authenticity/lifetime, timeout and indeterminate loss. Windows and overlong native addresses explicitly unavailable."}},{"criterion":"Actual owner routes activate and New Window without sender ownership","outcome":"pass","revision":"a654669c7475d5b0254e071bbbdd25b3e4dfc20b","evidence":{"kind":"command","detail":"build/dev/bpr0712-activation-final.log: actual process activation4.93s and owner2.18s pass; strict CLI, exact MRU after management focus, durable extra window, two profiles, no sender runtime/window, lost endpoint, closing refusal, restart/death recovery and zero provider mutation."}},{"criterion":"Existing lifetime and failure isolation preserved","outcome":"pass","revision":"a654669c7475d5b0254e071bbbdd25b3e4dfc20b","evidence":{"kind":"command","detail":"build/dev/bpr0712-integration.log: startup1.71s management1.63s multi-window62.42s restart6.54s management-host0.07s pass. Critical review's synchronous busy reply leak corrected; exact32-slot trigger/recovery has host evidence, composed post-delivery busy sequence unexercised and documented."}},{"criterion":"Available-target architecture and production boundary verified","outcome":"pass","revision":"a654669c7475d5b0254e071bbbdd25b3e4dfc20b","evidence":{"kind":"command","detail":"Exact local-listener architecture exception passes7.25s and four negative/positive cases; full unchanged contract71.03s. Changed format/tidy, tests-disabled Release build/inventory30-required/no-seven-forbidden, doc gate62-regressions and52 composition regressions pass. Full RFC/native/package obligations retained in BPR0595."}}]
---

# Route profile activation and new-window launches to the runtime owner

## Scope and acceptance

Bounded RFC-0024 AC-06/09 continuation: a stable profile identity and only
activate/new-window intents cross an owner-private native activation boundary.
Ordinary repeated launch activates the existing MRU window; explicit New Window
requests another window through the owner's durable management admission.
The sender never changes window registry state or bypasses the runtime lease.
Bound framing, peer authority, pending work and timeout/owner-loss outcomes.
Refuse closing owners; no automatic retry of an indeterminate New Window.

## Owned paths and verification

Application activation contracts, native local transport adapter, desktop/startup
and launch parsing, focused native and real-process tests, startup documentation
and this card/closeout. Preserve RFC-0028 Home/layout and unscheduled file tabs.
Verify real independent processes, exact profile/intent routing, malformed and
unavailable requests, existing owner lifetime, and no competing registry writer.
Run focused composed lifecycle regression, format/tidy and documentation gates.
Independent critical review targets framing, ownership and shutdown/ack races.
Native Windows/package focus evidence remains open unless directly observed.

## Accepted available-target evidence

Implementation candidate: `a654669c7475d5b0254e071bbbdd25b3e4dfc20b`.

- Native `sqlite_profile_activation` and `sqlite_profile_runtime_lease` pass
  2/2 in 4.85s on macOS. The bounded worker exercised independent executed
  clients, exact profiles, fragmented/malformed/overlong frames, stalled and
  disconnected peers, forged/wrong-root leases, socket symlinks, duplicate
  acknowledgements, timeout and dropped-reply ambiguity. Windows returns
  unavailable; no positive Windows/Linux-native claim is made.
- `application_profile_activation_public_path` passes 4.93s and
  `application_profile_owner_admission_public_path` passes 2.18s, serially
  (`build/dev/bpr0712-activation-final.log`). Actual processes prove strict
  CLI identity/intent parsing, ordinary MRU activation after focus was moved
  to management, durable New Window, two independent profiles, zero sender
  runtime/windows, an unavailable exact endpoint with lease retained, owner
  death/restart, closing-owner refusal and surviving unrelated ownership.
  Fixture counters prove zero publication/reply/resolution mutations.
- Integration passes startup 1.71s, management 1.63s, existing live multi-window
  62.42s, durable restart 6.54s and management-host 0.07s. The existing host
  course covers 32 occupied slots, one durable Close still draining, immediate
  New Window busy, exact drainage and successful replacement. The seven-test
  run had one failure: the previous blanket listener prohibition. The exact
  local-listener exception then passes architecture 7.25s; its four positive/
  negative cases pass separately, while the prior full architecture contract
  passed 71.03s. Logs: `bpr0712-integration.log`, `bpr0712-architecture.log`
  and `bpr0712-architecture-local.log` under `build/dev/`.
- Fourteen changed C++ files pass canonical formatting; changed translation
  units pass warnings-as-errors tidy. Parent logs `bpr0712-format.log`,
  `bpr0712-tidy.log` and `bpr0712-final-tidy.log`; the final closing fixture
  recheck is `bpr0712-activation-tidy.log`. The native worker separately
  verified its transport, native-lease and test translation units.
- Tests-disabled Release builds, and the production host inventory finds
  30 required and zero of seven forbidden hosts. Logs remain in
  `build/app-release/bpr0712-build.log` and `bpr0712-host-inventory.log`.
  Documentation gate, 62 documentation regressions and 52 composition-inventory
  regressions pass; `git diff --check` passes.

## Review, limits and durable invariants

Independent native critical review found that ignoring every busy projection
could retain one activation reply forever when the management host synchronously
refused New Window while a closed window still occupied its runtime slot.
The corrected exact submission bracket consumes that immediate refusal while
protecting admitted work from unrelated later busy gestures. The reviewer found
no remaining confirmed source blocker. The 32-slot trigger/recovery has direct
host evidence and the reply correction has source review; the exact composed
post-delivery busy sequence is not claimed. The added public closing course
pauses Qt Hide during command delivery and proves pre-admission refusal only.
The existing read-terminal fixture releases holds during Close, so it cannot
prove the other sequence; no timing-based substitute or new fixture seam was added.

Acknowledgement follows actual owner activation or window opening. Lost replies
are indeterminate and never retried automatically. The endpoint retains a genuine
exact-root/process lease, uses a fixed 16-byte request and EOF, allows one delivered
pending request, backlog eight and two-second client/stalled-peer bounds. No
credential, provider command or general remote operation enters the protocol.
Unavailable/closing owners never authorize competing ownership or sender writes
to window records. Global registry CAS recovery remains explicit Reload.

POSIX paths beyond the native socket address bound and Windows transport remain
explicitly unavailable. Native packaged foreground behavior, full AC-06 and the
RFC's other native/human criteria remain open. Requested native worker routes
were Astra low through explicit default adapters; critical review used Astra
xhigh. Opposite-family review and named integrator adapter were unavailable;
serving telemetry is not inferred from requested routing.
