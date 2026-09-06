---
schema: "implement-rfc.card.v1"
id: "BPR-0637"
title: "Migrate authentication and capability JSON to Glaze"
status: "In Progress"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0636"]
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

## Device portion implemented; capability portion remains

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

BPR-0637 remains In Progress. Next migrate `github_capability_probe.cpp`, which
still contains Qt JSON schema/diagnostic/envelope parsing and request/part-merge
serialization. Preserve nine exact query parts, independent rate observations,
schema shape/limit checks, bounded sanitized diagnostics and request bytes;
use capability/provider and synthetic sign-in composition evidence. Do not
repeat unchanged device tests unless capability work changes their inputs.
BPR-0612 full migration and later RFC work remain open; performance waits.

All work was local with synthetic fixtures. No production remembered session,
credentials, GitHub, Actions, push or paid CI was touched. Task build/test logs
are consumed here and removed; fixture wrappers own and clean their private
roots, and reusable build trees remain.
