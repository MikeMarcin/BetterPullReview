---
schema: "implement-rfc.card.v1"
id: "BPR-0679"
title: "Start sign-in directly and retain exact server recovery"
status: "Done"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0592"]
acceptance: [{"criterion":"One gesture begins selected-host sign-in; running, cancelled, expired and unavailable paths remain exact","outcome":"pass","revision":"947df5029dbec9d9406e4c59bb07a4aefa37f593 plus finish","evidence":{"kind":"observation","detail":"Home and signed-out account action call the same command-projected panel button; public-preview recovery presents the same page. Regression failed before change and passes after: one selected-profile begin, no second begin while awaiting, cancellation remains available, expired retry binds newly selected host. Settings button text is Sign in / Sign in again. Existing remembered-server and session-only token evidence from BPR-0660 at 23e5fe25 is reused: its host/store inputs are unchanged."}},{"criterion":"Selected executable, Qt scaling, authority and code checks","outcome":"pass","revision":"947df5029dbec9d9406e4c59bb07a4aefa37f593 plus finish","evidence":{"kind":"command","detail":"Executable fixture course now clicks Home Sign in. application_device_authorization_panel_course, panel_absent, ghes_refusal: 3/3 pass, 13.40s. qt_composed_github at 100/125/200 and qt_settings_window pass; expanded cancellation/expiry test at 100 and qt_device_authorization at 100/125/200 pass (4/4,17.76s). architecture_boundaries pass. application_composition_contract passed 5/6 including isolated tests-disabled build; sole source-digest drift fixed after inspecting main.cpp diff, then python3 tests/end_to_end/composition_contract_tests.py -k test_async_mutation_host_inventory_rejects_every_direct_bypass passes (1/1). Canonical tidy on four changed translation units passes; formatter and docs gate pass. No live login or server mutation performed."}}]
---

# Start sign-in directly and retain exact server recovery

## Scope

One Sign in gesture starts supported device authorization; cancellation, expiry, refusal and manual tokens remain reachable. Reuse existing saved-server authority.

Owned paths: Qt account entry; app device composition; existing device/GitHub hosts only where required; focused tests.

## Acceptance

AC-04/06/12/13/14: recording public path sees one begin; no second Get code; no wrong-authority requests; remembered servers restore and manual credentials stay session-only.

See [the inventory](../../development/settings-authority-inventory.md) for exact existing selectors and authority.
Closeout: [[BPR-0593]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.

## Evidence

Not run. Created by the authorized BPR-0592 planning pass.
