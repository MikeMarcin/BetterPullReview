# Product verification and evidence boundaries

Every requirement in REQUIREMENTS.json is initially `not_run_product`. The pack
validator verifies this archive and normalized examples, not BPR implementation.
Intake maps these rows to canonical live cards and the current test/issue system.

## Focused verification layers

| Layer | Required cases | Direct evidence |
| --- | --- | --- |
| Pure catalog/parser | All twelve; optional/custom decorations; case/strong forms; malformed and unknown syntax; code/quote/HTML/list/heading false positives; Unicode, CRLF, prefix budget and adversarial nesting | Actual Qt/provider-free C++ tests using parser-cases.json, plus generated bounds cases |
| Header edits | Label add/change/remove; original suffix and native suggestion preservation; incomplete editing; stale revision; byte-offset conversion; undo | Actual core edit tests plus editor/workshop integration; header-edit-cases.json |
| Identity and counts | Exact roots versus replies; missing/deleted roots; duplicate/overlapping provider pages; unknown resolution; outdated/unanchored; every column subset | Independent C++ normalized core tests and application normalization/projection, not reuse of the Python oracle as production code |
| Settings/drafts | Old schemas, missing fields, corrupt/duplicate IDs, failed writes, reset/cancel, same-profile fanout, other-profile isolation, desktop/CLI conflicts | Actual store/host tests using current profile and draft ports |
| Native GitHub contracts | Same decoded approved Markdown/anchor; explicit native dispositions; supported/unsupported suggestion context; no new labels/types/checks | Existing fake-provider/wire tests extended to exact request and recovery payloads |
| Private/tour integration | Track action versus public label; no automatic verification; available promotion paths; no private metadata leaks; plain/labelled tour roots; managed segment fidelity | Existing actual defect/promotion/tour application compositions and public-path fixtures |
| Hooks | Default/custom/disabled/missing-agent; relevant task selection; no extra model; candidate uncertainty/patch/provenance; prompt injection as data | Existing invocation/workshop envelopes with synthetic outputs; no live model required |
| CLI parity | Catalog, parse, exact draft edit and existing-data counts use same core; no-GUI shared-owner handling; human/JSON errors | Actual inherited headless command/service composition, with documented additive grammar |
| Native UX | Selected all-label icons and real controls, both resolution modes, uncertainty, exact drill-down, source/copy, native suggestion content, focus/undo/scroll | Actual rendered application captures and automated/observed interaction, not only models/mockups |
| Resource cost | Current large-review fixture; 1k normalized units and a 10k synthetic core set when within justified budgets; 0/3/12 columns; long bodies; single edit and refresh | Target/build/workload-bound timings, allocations and operation counts with no per-label network traffic |

Choose current test selectors by the composition they exercise. Paths in the source
audit identify existing coverage, not promised future selector names. Use current
network isolation and native-platform lanes. Focused checks per slice and one
relevant integration pass per milestone are the normal plan. Reuse unchanged valid
results; do not default to an exhaustive matrix after every card.

## Exact state transitions to demonstrate

A root label edit moves one bucket without changing the total. A reply label edit
changes only that reply's annotation. A provider resolve removes one unit from
known-unresolved and adds it to resolved; an all-state total is unchanged. A
reopen reverses that. An outdated flag or read receipt changes neither. A public
body edit can change classification without a new Git head. A label parse failure
must be readable, with classification coverage explicit.

Changing enabled columns reuses cached label/state buckets, makes no provider
request and does not alter drafts or shared bodies. Incomplete pages or a missing
root never become an exact empty result. Anonymous REST groups retain unknown
resolution even when their label is known. Top-level comments/review summaries
remain non-applicable to native resolution. An anchor loss does not erase its
conversation from PR-wide visibility or move it to a guessed file.

## Native UI course

Use the selected RFC0050 design, real Qt application and synthetic content. Exercise
no columns, Issue+Question, and all twelve; zero/single/multi-digit counts; default
unresolved and all states; unknown/partial coverage; root/reply label differences;
plain/tour comments; and native replacement blocks. Verify light, dark and
high-contrast treatment at representative 100%, 125% and 200% scales, narrow/wide
windows and larger text. Record actual native target results separately from
offscreen/fixture results. Reuse existing supported-platform evidence when truly
unchanged rather than asserting new coverage.

Check keyboard-reachable labels/help, accessible count names, focused drill-down,
source copying, draft undo, inherited multi-caret/virtual space where available,
scroll behavior and selected-thread stability on refresh. One clean screenshot
does not prove navigation or fix a nested-scroll performance problem. Icon-only
headers require more than hover-only help.

## Authorized website interoperability course (card 21)

No safe remote target is named in this pack. Prepare the course now, but perform
writes only after the user has authorized the exact safe repository/PR and scope.
Reuse existing designated-live and explicit publication mechanisms. No production
private-PR content, credentials or model credentials enter fixtures or logs.

On the authorized target, use original synthetic code and feedback. Publish a
labelled ordinary comment and a labelled native suggestion on a supported exact
anchor. Inspect both through GitHub without BPR. Demonstrate that the suggestion
is native, not just a colored code fence. Application of a suggestion is a separate
explicitly authorized commit action; if that action is not authorized, its actual
application evidence remains open rather than inferred from an available button.

Use the native review decision course with explicit Comment/Approve/Request changes
as authorized, not inferred from labels. Edit the root's prefix on the website,
reply with another label and resolve/reopen the thread, refreshing BPR between
accepted observations. Verify counts/root labels, native event identity, tour
links/order where present and the absence of automatic private verification.
Record permissions/GHES support exactly; a GitHub.com result is not proof for an
unavailable Enterprise version. Unsupported hosts keep safe readable behavior.

If the service/target/permission is absent, park this exact criterion and continue
independent core, documentation and local UI work. Do not relabel the course as
passed by using recorded synthetic responses. Remote publication/review/suggestion
application is never an implicit side effect of running a test suite.

## Recovery and security

Use existing contained crash/restart tests. Lose a reply after native comment
transmission and before local confirmation; attempt a label edit while recovery
is unresolved. Prove the prior attempt is reconciled before a new exact-body
request can run, with no duplicate publication or reused old approval. Cross-window
and desktop/CLI edits preserve current data or explicit conflict candidates.

Treat Markdown, decorations, URLs, code and received hook context as untrusted.
No resource loading, shell command, arbitrary script, provider mutation or model
launch is authorized by classification, a source file, or an apparent instruction
inside a comment. Preserve bounded work and raw-source access on refusal. Settings
and annotation cache changes never grant new scopes or bypass the current owner.

## Completion record

Each criterion records its exact producing revision, configuration, target,
command/gesture, observed result and limitations. Link selected visual artifacts
to the implemented resources; retain rights and required human/native decisions.
Run the current documentation/issue-graph gates and focused public path before
formal closeout. Independent review checks semantics and authority, not merely
file presence. R43–R48 are mandatory shared delivery obligations. Never rewrite
settled predecessor evidence, claim inaccessible tour manifests were verified,
or use an RFC status as evidence that a feature was exercised.
