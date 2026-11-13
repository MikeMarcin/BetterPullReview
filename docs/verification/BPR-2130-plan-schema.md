# BPR-2130: pinned plan schema repair

This future-main metadata repair corrects the completed BPR-2120 plan for the
pinned MASK payload `96505b1615cf367eed06153717f8dea0c2f22a51e5aad15fc5080a8e9f928588`.
The actual schema sets `additionalProperties: false` on milestones and permits
the existing required `boundary` string.

The two unsupported milestone keys were removed. Their exact names and values
now appear as proof bindings in that existing boundary string:

- `baseline_source_sha256=8716981f5d46bfdf4462c0a1cd484b7e4394c5b4ea3c1888a6cb19df6aeb27cd`
- `existing_probe_receipt_sha256=4800b5a91d05358c76a2e4332057f3e3063872a4dcda75b959428289391a2486`

Restoring the original boundary and those two keys reconstructs every original
JSON value exactly. Scope, outcome, work selectors, dependencies and exits are
unchanged. The BPR-2120 Done card and acceptance remain byte-identical.

The original plan SHA256 is
`11b4b93a848749959e4fe4e881681bcedabe7c09c166cbc6693be438775841e3`;
the corrected plan SHA256 is
`567dcce0a28c63f121268f931f7e76940389e626f710589d21e49fb935982019`.

## Actual pinned-tool evidence

Commands used the immutable pinned payload's `scripts/milestone_plan.py`
with the explicit canonical repository root, not a newer runtime.

| Public operation | Before | After |
|---|---|---|
| Explicit BPR-2120 plan check | Exit2: unknown proof-hash fields | Exit0: structurally valid |
| Discover target BPR-2120 | Exit2: unknown proof-hash fields | Exit2: independent `amendments` error |
| Discover independent target BPR-2129 | Exit2: unknown proof-hash fields | Exit2: independent `amendments` error |
| Explicit owned BPR-2130 plan check | — | Exit0: structurally valid |

The bare no-target discover command returned `absent`/0; it does not exercise
the affected target index and supplies no global validation credit.

Global target discovery remains failing because
`docs/rfcs/plans/release-v041/milestone-plan.json` contains an unsupported
root `amendments` field. An explicit pinned check of that plan also exits2.
Its SHA256 `15a7bde30938ee4ba63db3e8267798adc72dedd97f84743438716e8d9764b815`
matches the pre-work source exactly. That separate plan was not repaired.

Documentation validation and the owned follow-up plan pass. This correction
does not reopen BPR-2120, upgrade the runtime, change product/tests, or supply
release qualification. It is future main only: frozen Build60 source, cohort,
artifacts and native jobs remain untouched, and this work is excluded from
Build60 intake.
