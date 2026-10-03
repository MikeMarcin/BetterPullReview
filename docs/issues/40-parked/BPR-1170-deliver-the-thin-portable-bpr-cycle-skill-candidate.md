---
schema: "implement-rfc.card.v1"
id: "BPR-1170"
title: "Deliver the thin portable bpr-cycle skill candidate"
status: "Parked"
rfcs: ["RFC-BPR-0058"]
depends_on: ["BPR-1169"]
metadata: {"pack": "bpr-review-cycles-v1", "milestone": "external-consumers", "acceptance": ["RC58-08", "RC58-10"], "pack_alias": "PACK-CYCLE-28", "milestone_plan": {"id": "bpr-review-cycles-v1", "path": "docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"RC58-08","outcome":"pass","revision":"b10a15a86","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Owner accepted BPR-1170-rc58-08-distribution-v001 (session cbd48b91000b00f1e72e1ad62c7c384c, event 861). The package is in AgentSkills (399da0d, then b4a3357 after the frontmatter quoting fix 2b48d25fd that mask install needed) and selected from agentskills in .mask.json by a reviewed mask install; mask status --check is clean. The package, the AgentSkills copy and both MASK links digest to sha256:8b5946f579a0f8bf3de38bfc45113d1b3789f236dccf7dab57caa2d0828a5381. skill_bpr_cycle_package passes with the links present (14 broken copies refused; the 1b8fd4a5c package and a real-directory copy fail). The package half (active harness, no second model or orchestrator, no provider publisher) is unchanged from 1b8fd4a5c; nothing was installed silently."}},{"criterion":"RC58-10","outcome":"pending","revision":"b10a15a86","evidence":{"kind":"command","path":"docs/verification/RFC-BPR-0053-0061.md","detail":"Manual half passes (unchanged from 1b8fd4a5c). Owner accepted BPR-1170-rc58-10-harness-course-v001 (event 862). scripts/run_bpr1170_harness_course.py is that course: setup and the session's default-profile view verified without a model, analyzer checked on a synthetic transcript. The observed session has not run: standalone Claude Code is not signed in (claude auth status loggedIn false; claude -p answers 'OAuth session expired and could not be refreshed'). Needs the owner to run claude auth login once, then one run of the course."}}]
---

Pack alias: **PACK-CYCLE-28** (live `BPR-1170`, allocated at import).

# BPR-1170: Deliver the thin portable bpr-cycle skill candidate

## Outcome and bounded work

Adapt the bundled skill to verified CLI schemas/commands; integrate into canonical shared AgentSkills through separately authorized MASK distribution. Teach read-whole-round, action distinctions, exact suggestions and item-linked replies. Keep the active harness and its existing orchestration.

## Acceptance and direct evidence

Validate portable front matter, short main instructions, progressive references, missing CLI/reachability and policy refusal paths. Run separately authorized harness/manual courses; no silent install, model route changes or provider-specific copied implementations.

Criteria: RC58-08, RC58-10.

## Ownership and constraints

Use the owning RFC and [integration map](../../rfcs/assets/bpr-review-cycles-v1/supporting/INTEGRATION-MAP.md) to resolve live paths; proposed names are not a claim files already exist. Preserve the core/host/Qt allocation. Record the actual owned paths before editing. Reuse shared components and unchanged evidence; no second implementation or global cleanup.

Product evidence is **Not run** at authoring. Follow [PRODUCT-VERIFICATION.md](../../rfcs/assets/bpr-review-cycles-v1/verification/PRODUCT-VERIFICATION.md). No live GitHub/GHES mutation, model invocation, shared-skill installation, credential access or unrelated repository write is authorized merely by importing this card. Record result, revision, environment and limitations; a fixture result is not native/user/service acceptance.

## Implemented: macOS, 2026-10-02

`packaging/agent-skills/bpr-cycle/` is the candidate package: `SKILL.md` and
three references (commands and fields, responses, refusals), nothing to
execute, at `1b8fd4a5c` with package digest
`sha256:040a559b841704b63e3e62b3dfc90d60b6a9176ff2ec9e92506010458ae83cf3`.
It teaches only the consumer commands BPR-1169 shipped, each with
`BPR_CYCLE_ROLE=consumer`: read the whole round before acting, exact and
illustrative suggestions, replies by item ID, and an optional `response
append`. It works from the round's Markdown when `bpr` is missing, and sends a
refusal's `next` to the user rather than running it. It keeps the active
harness, starts no model, publishes nothing and is installed nowhere.

`skill_bpr_cycle_package` pins the text and runs every documented line through
the real `bpr` against a local cycle with a stored grant. The refusals the
package teaches are observed there: no grant, a read-only grant, a denying or
unconfigured policy, revocation, and every reviewer command refused to the
declared consumer. Thirteen broken copies fail the row. Owned paths, decisions,
commands, controls and owed station runs are in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md), under "BPR-1170:
the bpr-cycle skill candidate"; usage is in
[the developer page](../../development/cycle-cli.md).

## Waiting on the owner's authorization

Both criteria have their unattended half done and an authorization left,
asked in attended session `cbd48b91000b00f1e72e1ad62c7c384c` as one
Accept/Reject item each:

- RC58-08, `BPR-1170-rc58-08-distribution-v001`: importing the package
  unchanged into the shared AgentSkills repository and selecting it from
  `agentskills` in `.mask.json`, as that repository's own task.
- RC58-10, `BPR-1170-rc58-10-harness-course-v001`: one observed course in
  which a Claude Code session follows the package, read from the repository
  without installing it, against a disposable fixture profile.

Accepting an item authorizes the work; neither answer is the evidence itself.
Accept on RC58-08 starts the import task. Accept on RC58-10 starts the course,
which is then recorded here. A Reject leaves its criterion open, with the
requested change in the note.

`coding_format_check` fails at main on `src/app/src/tour_cli.cpp`, which this
card did not touch. It is filed as BPR-1268.

## Linux: bpr-main-linux, 2026-10-02

At `adef7a558`, which includes BPR-1268's `tour_cli.cpp` format fix, on WSL2 with
GCC 15.2 and `build/dev-tour-cli`. The build exits 0 with 0 `warning:` lines.
`ctest --test-dir build/dev-tour-cli -j4 --output-on-failure -R
"^skill_bpr_cycle_package$|^skill_pr_tour_package$|^cycle_cli_public_path$|^tour_cli_|^coding_format_check$"`
passes **9 of 9** in 32.18 s:

- `skill_bpr_cycle_package` 4.67 s and `skill_pr_tour_package` 2.12 s
- `cycle_cli_public_path` 13.50 s
- the five `tour_cli_*` rows
- `coding_format_check` 29.67 s

**clang-format 22 agrees with 23 on the hoisted line.** `coding_format_check`
ran `/usr/bin/clang-format` 22.1.8 explicitly ("Checking 1208 file(s) with
/usr/bin/clang-format (22.1.8)"), not the 21.1.8 earlier on this station's
`PATH`. A direct `/usr/bin/clang-format --dry-run --Werror
src/app/src/tour_cli.cpp` is also clean. No remote mutation.

## The owner's answers: 2026-10-03

Both items were accepted with no note (session
`cbd48b91000b00f1e72e1ad62c7c384c`, events 861 and 862, hand-back 869). The
protections were released by candidate name.

**RC58-08 passes.** The package was imported unchanged into AgentSkills
(`399da0d`) and pushed. `mask install bpr-cycle` then refused it: the
description held an unquoted `: `, which is invalid YAML and which MASK refuses.
`2b48d25fd` quotes the description and changes no other byte. That changes
the digest from `040a559b…` to
`sha256:8b5946f579a0f8bf3de38bfc45113d1b3789f236dccf7dab57caa2d0828a5381`.
The lint now catches the problem, with a fourteenth broken copy as its control.
AgentSkills `b4a3357` re-imported the package. A reviewed `mask install`
selected it from `agentskills`, and `b10a15a86` commits that selection. The
package, the AgentSkills copy and both MASK links share the new digest, and the
row passes with the links in place. MASK also rewrote the declared repository
from SSH to HTTPS; the lock depends on that form.

**RC58-10 is still pending.**
[`scripts/run_bpr1170_harness_course.py`](../../../scripts/run_bpr1170_harness_course.py)
is the authorized course, ready to run. It has not run because standalone
Claude Code has no sign-in on this Mac (`claude auth status`: `loggedIn: false`).
This session is authenticated by its desktop host, and nothing here provisions
credentials. The owner needs to run `claude auth login` once. Then run:

```sh
python3 /Users/jaeger/Documents/GitHub/BetterPullReview/scripts/run_bpr1170_harness_course.py /Users/jaeger/Documents/GitHub/BetterPullReview/build/tour-cli/src/app/bpr
```

Record its summary in the ledger and finish this card if the session followed
the package. Details are in the
[execution ledger](../../verification/RFC-BPR-0053-0061.md) under "BPR-1170: the
owner's two answers and what followed".
