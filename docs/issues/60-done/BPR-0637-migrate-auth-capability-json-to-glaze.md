---
schema: "implement-rfc.card.v1"
id: "BPR-0637"
title: "Migrate authentication and capability JSON to Glaze"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0636"]
acceptance: [{"criterion":"Authentication wire contract","outcome":"pass","revision":"5a7494ab0ef6ef5c67096f8c1a6226eb29368c4c","evidence":{"kind":"observation","detail":"Device source 1211a9f0: exact duration/string parsing, form bytes, secret/error categories; device/public/native loopback tests pass as recorded on card."}},{"criterion":"Capability wire contract","outcome":"pass","revision":"5a7494ab0ef6ef5c67096f8c1a6226eb29368c4c","evidence":{"kind":"observation","detail":"Nine exact outgoing request bodies, GitHub/GHES facts, shape/duplicate/aggregate limits, invalid raw JSON and sanitized error tests pass in github_capability_probe (4.22s)."}},{"criterion":"Provider and public composition","outcome":"pass","revision":"5a7494ab0ef6ef5c67096f8c1a6226eb29368c4c","evidence":{"kind":"observation","detail":"Final local network-isolated integration 4/4 pass in 29.12s: provider, capability, registered sign-in and renewal courses. Development app and focused test targets build."}},{"criterion":"Architecture and scope","outcome":"pass","revision":"5a7494ab0ef6ef5c67096f8c1a6226eb29368c4c","evidence":{"kind":"observation","detail":"Both owned production files no longer use Qt JSON; representations stay private. Documentation gate and diff check pass. No production credentials, live GitHub, Actions or paid CI used. Full migration remains BPR-0612."}}]
---

# Migrate authentication and capability JSON to Glaze

## Bounded acceptance and ownership

Own github_device_authorization_contract.cpp and github_capability_probe.cpp
under src/adapters/github/src, with their focused tests. Replace Qt JSON with
typed Glaze contracts; preserve secret handling, sanitized failures, exact
request bytes, nine-query capability probing, optional/required fields and
resource bounds. Verify device/capability/provider and synthetic public sign-in
composition paths without touching production credentials or sign-in.

Migration anchor: BPR-0612. Migration base `4e6a6013f3589711f614fba7b9c46b8967313796`.
Preserve core/host boundaries and pinned dependency provenance. All execution
is local with synthetic fixtures; no Actions, push, paid CI or production
account/process operations. Full migration acceptance remains on BPR-0612.

## Device authorization implementation and evidence

Device source `1211a9f0bb36f49cd3a4aec46df22d04bb06fbe7`. Selection `a9d3aad3`; claim `d6c9eda6`.
No Qt JSON remains in `github_device_authorization_contract.cpp`. Glaze indexes
the bounded flat envelope as raw views into the caller-owned response, then
decodes strings and positive bounded uint32 durations. This preserves exact key
inventories and existing failure-category ordering without retaining a Qt DOM or
normalizing numbers through double. Views stay inside the synchronous call;
only existing typed grants/errors leave it. Outbound form bytes, content-type
and status rules, URI checks, token limits and failure-only enums are unchanged.

The new raw tests cover fractional duration spellings (including values that
would round to an integer), overflow, scalar type errors, exact bounded integer
values/exponents, trailing documents, malformed UTF-8 and unpaired surrogates in
unknown fields, and issuance/renewal lifetime parity. Existing device duplicate
policy remains last-member-wins, including escaped equivalent keys; tests record
this explicitly rather than silently adopting mutation-parser duplicate policy.
The full 64-KiB response ceiling remains before parsing. Glaze validates the
whole envelope and supplies its bounded recursion handling; no parser text or
response bytes become a diagnostic.

Local Mac evidence:

- Device contract, native device fixture and development app targets build.
- `github_device_authorization_contract`: pass, 0.41s; existing recorded and
  documented response inventories and exact form-byte assertions also pass.
- Public application device path / registered course / refresh course: 3/3
  pass, 1.02/5.54/7.65s, through the composed synthetic flows.
- `native_https_device_authorization_contract`: pass, 0.48s.
- Initial native fixture and refresh runs failed their lifecycle assertion:
  the old fixture called `resume_refresh` without the lifetime metadata now
  required by the host, so it correctly refused renewal before a wire request.
  The contained test correction supplies a valid synthetic lifetime and a
  deterministic clock/timer, checks that renewal is scheduled before material
  is spent, then fires that deadline. Real adapter/transport/socket and exact
  rotated vault/access enrollment assertions remain intact.
- Corrected native fixture / refresh contract: 2/2 pass, 15.22/5.33s, under
  their loopback-only isolation wrappers. No product host policy was changed
  to make the stale fixture pass.
- Touched C++ formatted; documentation gate and `git diff --check` pass.
  Owning architecture documentation describes the migrated parsing boundary.

## Capability implementation and evidence

Capability source `5a7494ab0ef6ef5c67096f8c1a6226eb29368c4c` completes this slice. No Qt JSON
remains in either owned production file. Outgoing queries use a typed Glaze
record; the type-dependent introspection tree uses a private Glaze value with
full-width integer storage and existing shape validators. No JSON representation
crosses inward. The nine query documents, exact request bytes, required/null
semantics, preflight and aggregate limits, rate observations, and promotion only
after the complete batch remain intact. Batch serialization preserves explicit
null members and sorted keys. Sanitized diagnostics distinguish missing from
null fields and remove credential reflections before length checks.

New raw tests reject malformed UTF-8, lone surrogates, trailing documents and
invalid explicit null diagnostics. Every one of the nine request bodies matches
the independent prior Qt serializer; BPR-0640 will replace this temporary test
oracle with independent literal fixtures. Existing recorded GitHub/GHES schema
facts, duplicate/member/wrapper/limit cases and encoded reflection checks pass.

During implementation, the aggregate contract check caught an initializer-list
construction that wrapped the merged object in an array. Explicit object
construction fixes it; explicit null serialization is also retained. Only the
rebuilt final binary's passing results below are acceptance evidence.

Local Mac evidence:

- Capability, provider and development app targets build successfully.
- Final focused integration: 4/4 pass, 29.12s total. `github_provider` 10.71s;
  `github_capability_probe` 4.22s;
  `application_device_authorization_registered_course` 6.19s;
  `application_device_authorization_refresh_course` 7.97s. All use registered
  network-isolated synthetic paths; public composition exercises sign-in and
  renewal with migrated capability activation.
- Documentation gate, formatting and `git diff --check` pass. Owning architecture
  documentation describes the migrated capability boundary.
- Earlier device/native-loopback evidence above remains applicable; capability
  changes do not alter the device parser or native transport inputs.

BPR-0612 remains open. Five production/composed-fixture files still use Qt JSON.
Next is BPR-0638 for read contracts, normalization and pagination, then BPR-0639
composition and BPR-0640 test helpers/final inventory. Performance work waits
for full migration completion.

All work was local with synthetic fixtures. No production remembered session,
credentials, GitHub, Actions, push or paid CI was touched. Task build/test logs
are consumed here and removed; fixture wrappers own and clean their private
roots, and reusable build trees remain.
