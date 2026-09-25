# Capability isolation, removable adapters and security

## Agent-free is a product configuration

Produce a supported **agent-free desktop package** and **agent-free CLI package**.
Candidate build switches are `BPR_BUILD_AGENT_INTEGRATIONS=OFF` and independently
`BPR_BUILD_CYCLE_AUTOMATION=ON|OFF`; intake must adopt existing naming conventions
where present and document exact switches. An agent-free build omits provider launch
adapters, agent supervisors, provider protocol code/assets and agent-only MCP helpers.
Do not merely hide a pane or suppress launch while still requiring those libraries.
No model credentials/provider install/agent executable may be needed at startup.

Core review, source capture, notes/suggestions, immutable rounds, Copy/Save, manual
revision capture, comparisons, ordinary responses, reviewer verification, clean
publication preparation and independently permitted GitHub publication remain usable.
Generic local automation is independently controllable, not classified as a model
integration simply because an agent can invoke it. Remove that too and GUI plus manual
file/clipboard iteration still work. Removing direct AI integrations must not delete
existing cycles, round histories, suggestions, responses or verification records.

The supplied source's `src/app/CMakeLists.txt` links AdapterAI, AdapterAgent and
AdapterMcp into composition. This is an observed **coupling to change**, not evidence
that the requested removable configuration already exists. Split dependency ownership,
composition factories, resources and packaging manifests; test actual installed files
and dynamic dependency closure in both builds. Native syntax highlighting and ordinary
Markdown libraries are not “AI” merely because agents also consume text.

## Capability matrix

| Capability | Core GUI | Agent-free CLI | External skill | Direct agent adapter |
|---|---|---|---|---|
| inspect/capture local source | yes, selected root | yes, local authority | only when separately permitted; not needed to read round | optional approved workspace observation |
| draft/freeze round | yes | yes, human-authoring authority | not implicitly granted | not implicitly granted |
| Copy/Save selected round | yes, export policy | yes, export policy | consumes only allowed output | uses same payload |
| read prepared round | yes | yes, permitted scope | yes when reachable and permitted | same scope |
| append response | yes/import | optional response grant | optional response grant | correlated report only |
| verify feedback | explicit reviewer | explicit reviewer | no under response grant | no |
| launch model process | no | no | harness already owns its session | explicit installed/allowed adapter |
| create branch/PR | separate publication grant | separate publication grant | not implicit in skill | not implicit in delivery |

An action permitted to a human is not automatically allowed to an automated caller.
Within the same OS user, application identity checks are defense-in-depth against
accidental cross-target access, not containment of a malicious same-user process.
Real corporate restrictions must also control filesystem/process/network privileges.
Do not claim a renamed executable, self-reported provider string or skill declaration
certifies a harness. A denied export stays denied across GUI, CLI and alternate formats.

## Disclosure and unknown origin

The user chooses source scope and handoff content. Retain known enterprise origin or
classification; opening a local checkout does not wash away a known GHES restriction.
For an unclassified plain folder, allow local review, but do not infer that every
external destination is approved. Channel availability follows configured policy.
Showing Share, indexing a folder, saving a draft or freezing a round makes no network
request. Clipboard and local file export disclose to their OS destinations; BPR cannot
retract clipboard history or police a later manual upload outside its control.

Round access grants are scoped to an exact ready digest and expire/revoke according to
local policy. Grant references in commands are nonsecret identifiers; never put bearer
secrets in argv, copied prompts or ordinary logs. Credentials stay in existing vault/
transport services; snapshots, exports, context objects and response reports have no
credential fields. No generic get-file endpoint, arbitrary command execution or access
to all local notes is added. No hosted service or always-on listener is required.

## Untrusted material

Treat repository text, comments, Markdown, imported reports, proposed code, filenames
and model output as data. Never execute hooks, filters, textconv/external diff, test
commands, editor commands from the repository, auto-install instructions or shell
strings supplied by a payload. Render with the existing safe native Markdown policy:
no remote fetch on display, active HTML, local-file reads or embedded browser.

Validate archive and path boundaries described in ROUNDS-AND-PORTABILITY. Bound every
list, string, parse depth, decoded byte count, file count and decompression ratio.
Validate UTF-8/scalar boundaries and exact offsets before applying edits. A partial
round cannot appear complete. Candidate application to a checkout is a separate
explicit workflow; importing a round/report never writes project source.

Agent instructions should tell a consumer to treat source snippets and imported
reports as data and respect its own security policy. That is guidance, not a prompt-
injection-proof security mechanism. No output claims that a skill alone prevents
exfiltration or malicious tools.

## Storage, IPC and diagnostics

Keep local round/source data in owner-private application storage with existing
platform ACL and retention rules. Do not claim application encryption where none
exists. A .bpr-review.zip file can contain confidential source; default to a deliberate
user destination and no automatic cloud sync. Memory/crash dumps, OS clipboard history
and external-harness logs remain separate residual risks; document them.

Extend single-owner IPC only through a finite operation registry with bounded frames,
peer checks, request correlation and revocable scope. Reuse the tour owner semantics;
do not quietly claim activation-only IPC already supports source reads. An unavailable
platform transport remains unavailable until native qualification. Diagnostics use
IDs/counts/reason codes by default, with no source, suggestions, round prose, secrets
or entire command inputs. Any deliberate diagnostic content export has its own preview.

## Optional integrated delivery

A direct adapter selects an admitted existing provider profile and sends the exact
frozen payload as one assignment. Where the harness supports continuing an existing
session, bind the exact session; otherwise clearly disclose starting a new session.
Do not create an unrelated agent silently. Existing process lifecycle, credentials,
sandbox and GHES profile policies remain authoritative. Delivery success and model
completion are observed separately; failed/uncertain delivery can be recovered without
resending blindly. Do not auto-fallback to another provider or to an export channel.

Removing an adapter mid-cycle preserves the round and any confirmed response; capture
and next-round authoring continue manually. No adapter-only artifact is required to
interpret the feedback. Optional MCP compatibility, if later useful, remains a thin
adapter to the same bounded interface, not the sole gateway and not an HTTP daemon.

## Qualification

Run the full manual course with integrations compiled out and provider executables
absent. Inspect package contents/link graph and startup process/network observations;
repeat after disabling local automation. Test stale/wrong-owner IPC, over-limit
rounds, malicious archives, traversal, raw path collisions, injected Markdown, denied
enterprise disclosure and imported claimed verification. Record real native results
and residual risks. Pack fixtures do not certify this security boundary.
