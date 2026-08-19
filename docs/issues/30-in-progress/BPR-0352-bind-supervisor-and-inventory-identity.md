---
id: BPR-0352
title: Bind supervisor and inventory identity into the approval subject
status: In Progress
rfc: RFC-BPR-0008
depends_on: [BPR-0351]
---

# Bind supervisor and inventory identity into the approval subject

[[BPR-0262]] fact_5: the approval subject binds the stage-one supervisor and the
in-image inventory/resource identities.

`designated_review_approval_subject` is a free function rather than a struct
(`src/application/src/review_mutation_execution_authority.hpp:116-120`, defined
`src/application/src/designated_review_authority_bootstrap.cpp:1461-1535`); it
renders a `key=value` text subject. It emits `schema_name`, `schema_version`,
`approval_statement`, `application_sha256`, `source_sha256`, `approval_id`,
`build_configuration`, `build_testing`, the three replay fields, and the fixed
target identity. None of them names a supervisor or an inventory.
`designated_review_bootstrap_request`
(`review_mutation_execution_authority.hpp:83-93`) has no field to carry one.

So an approval authorizes an application digest without saying which supervisor
or which capsule contents it was granted against.

## Required final state

- The subject binds supervisor identity and in-image inventory/resource
  identity, and the request carries both.
- `schema_version` advances, and an approval bound to the old subject is
  refused rather than silently reinterpreted.
- A raw image pathname hash may be recorded as release evidence but is not
  claimed as an FD-atomic DiskImages attach identity — fact_5 is explicit, and
  the packet records the `/dev/fd/N` limit that forced it.
- `application_designated_review_authority_bootstrap` covers the new binding.
