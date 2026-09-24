# bpr tour CLI contract

**Proposed public interface, version 1.** These commands are implementation targets, not commands that exist in the supplied source. The runtime and shared contracts own behavior; parsing/printing contains no GitHub business logic. The finite catalog is [command-catalog.json](command-catalog.json).

## Everyday terminal use

```sh
bpr tour open 123
bpr tour edit problem
bpr tour edit solution
bpr tour add src/data_store.cpp:148-169
bpr tour walk
bpr tour preview
bpr tour publish
```

Use a full PR URL outside an unambiguous checkout. Common options are `--profile NAME_OR_ID`, `--draft ID`, `--json`, `--no-color`, and `--non-interactive`. `open` additionally accepts `--root REVIEW_URL_OR_ID` to select among several tours and `--repo HOST/OWNER/REPO` to qualify a PR number. Argument parsing must accept these documented forms consistently on supported OSes; shell examples do not prescribe a shell-based implementation.

Human convenience context is per terminal/worktree and explicitly selected profile, not a process-global PR changed by another client. On ambiguity, offer a short chooser or require a target. Agent calls use `--profile` and `--draft` after open. A PR number alone never chooses a host/account from a different profile. A returned `draft_id` is stable on this profile, not globally across machines; published root and comment IDs remain canonical externally.

`open` may persist local state but performs only authorized remote reads. Opening a PR, including a draft PR, does not prepare server-pending comments, publish a description, or change draft status. Offline status/local editing works with available local material and explicit stale/unknown source states; publishing requires fresh validation.

## Standalone first run

A new CLI-only install can create a default local profile using the existing profile management service; it must not require opening the desktop or copying its database. `open` may initialize that local state, but cannot launch a browser, change global gh configuration, or perform sign-in automatically. If read authentication is missing, show the exact next action:

```sh
bpr tour connect --host HOST --transport native
```

`--transport gh` is available only where the existing profile trust boundary is qualified. `connect` is an explicit, narrowly scoped frontend to current profile/account/auth services, not a new credential implementation. It can run the admitted device/sign-in flow or select an already configured gh identity, with required trust disclosures. It must not extract gh tokens or use another account as a fallback. Noninteractive setup without an approved auth path returns interaction_required with instructions; no surprise browser or hidden desktop launch. Public anonymous reads remain governed by existing policy.

The catalog's `remote_write` flag means **PR/review content mutation**, not all HTTP POST requests: connect may perform explicitly authorized authentication requests, but cannot alter a PR or comment. This setup path is part of standalone CLI acceptance, not a new general account-management project.

## Task operations

| Syntax | Meaning |
|---|---|
| `connect --host HOST [--transport native\|gh]` | Explicit first-run profile/account setup through existing admitted services; no desktop or token extraction. |
| `open PR [--root ROOT]` | Resolve exact identity; recover published content and resume/initialize local work without clobbering local changes. |
| `status [--draft ID]` | Show target, local/server state, revisions, partial observations, unresolved operations, and next useful step. |
| `show problem\|solution\|stop:ID [--source]` | Return a bounded current field or one stop and optional pinned source context. |
| `edit FIELD [--file PATH\|-] [--expected-revision R]` | Open configured editor for a human; alternatively import one exact field from a file/stdin with CAS in machine mode. |
| `add PATH:START-END [--side LEFT\|RIGHT] [--commit OID]` | Add a local stop with a validated source binding; explicit `--path` and `--lines` forms avoid colon/path ambiguity. |
| `move STOP --before STOP\|--after STOP` | Change order of stable stop IDs using the current/expected structure revision. |
| `remove STOP` | Remove from local desired route, preserving published discussion. |
| `ready STOP --expected-revision R` | Deliberate current-content completion; records observed/relayed origin, never proof of attention or permission to publish. |
| `walk [--from STOP]` | Human one-stop loop; noninteractive callers use show/edit/ready instead. |
| `preview [--refresh]` | Current local publication plan; refresh explicitly admits remote reads, never writes. |
| `publish [--resume ATTEMPT]` | Confirm and execute the existing finite new-tour or maintenance plan. No generic `--yes`. |
| `recover [--attempt ID]` | Read-first reconciliation and local journal repair; remaining remote writes require publish/resume. |
| `undo` / `redo` | One shared local committed transaction; display its label and restore stored bytes, not remote actions. |
| `capabilities` | Version/feature/transport/approval-path availability; may describe a limitation without leaking credentials. |
| `hooks show\|edit\|reset HOOK` | Read/change the existing four trusted local hook settings; no invocation occurs automatically. |
| `assist context --hook HOOK --field FIELD [--range-bytes A:B]` | Issue a bounded explicit local assist ticket and effective context; no model launch. |
| `assist complete --ticket ID --input PATH\|-` | Validate parent result contract; apply one in-place local rewrite or return an outline proposal, never publish. |
| `outline use PROPOSAL --indices LIST` | Insert chosen body-free suggestions after the author's selection; all begin Suggested, not Ready. |

`FIELD` is `problem`, `solution`, or `stop:ID`; title/body editing is a documented typed subfield, not an arbitrary object path. `HOOK` is problem, solution, outline, or comment. For the outline hook, omit field/range; context belongs to the selected draft. Hook edit opens the same trusted editor mechanics. The writing defaults stay in RFC-BPR-0043's shared settings, not in this extension.

Machine creation/edit parameters use JSON stdin where simple scalar flags are insufficient; source/content arrays are never embedded as unescaped shell strings. A bulk import of an authored local stop set is not a bulk human-acknowledgement operation. Generated outline data cannot contain comment bodies. Machine calls explicitly bind expected revision for all writeable records; interactive calls capture that revision before the input/edit session.

## Structured envelope and data types

```json
{
  "schema": "bpr.tour.cli-result.v1",
  "command": "show",
  "request_id": "req-18",
  "status": "ok",
  "data": {},
  "warnings": [],
  "error": null
}
```

Every response includes these exact top-level keys. `status` is ok, error, conflict, partial, or uncertain. `data` is command-specific; references/identifiers are strings, not JSON floating values. `warnings` are bounded objects with code and human-readable message. `error` is null or a structured code/message/recoverability object. `data` may contain partial safely observed results on an error; do not present absent fields as empty shared content.

One JSON document goes to stdout; no Markdown fences, progress spinner, ANSI escapes, banner, or English preamble. Text/code remains raw UTF-8 inside escaped JSON strings, not terminal-rendered/sanitized content. Human rendering sanitizes control sequences without modifying canonical stored text. Plain prose output never consists of pretty-printed JSON. Error messages go to stderr for human commands; machine error envelopes stay on stdout with useful exit status and no duplicate source-bearing logs.

Use JSON input/result schemas for integration tests, not as a requirement to introduce a runtime JSON Schema engine. The parent Glaze DTO and validation style remain suitable. Common transport framing limits are advertised; use a paged snapshot with explicit continuation/completeness instead of returning an entire private draft history. No silently truncated success. A proposed 1 MiB per frame and 256 KiB one-field payload ceiling are upper bounds, not promises to relax smaller inherited limits; document actual effective bounds in capabilities. Multiple bounded read frames may recover a larger admitted draft.

## Exit codes

| Code | Meaning |
|---:|---|
| 0 | Successful completed read/local operation or reconciled successful publication; warnings may remain. |
| 2 | Invalid input, unsupported command/schema or ambiguous target. |
| 3 | Interactive user/approval needed; nothing unauthorized was sent. |
| 4 | Stale revision, conflict, unknown/unsupported owner protocol or explicit owner busy. |
| 5 | Authentication, host policy or supported-capability refusal. |
| 6 | Uncertain or partially completed mutation; inspect/recover before further writes. |
| 7 | Transport/storage/internal operation failure with no claimed completed mutation. |
| 130 | Cancelled with no uncertain write; after a possible write prefer 6 and retained outcome details. |

A signal does not guarantee server rollback. `recover` may return ok for successful reconciliation while data still explicitly names remaining authorized work; it must not call that plan Published. Failures never rewrite the draft as a convenient reset.

## Exact prose operations

Store canonical raw UTF-8 and line endings as inherited. Ranges are byte offsets [begin,end), not codepoints, graphemes, UTF-16 units or rendered columns. Reject invalid Unicode or a range interior to a scalar. Qt bridges its own coordinate system; a shell tool does not receive native cursor offsets. For a request phrased by exact selected text, the service counts exact current matches; zero/multiple matches are errors, not reasons to broaden scope.

Snapshot ticket creation captures field generation/digest, source/structure context, effective hook/version, caller editing-session identity, and fixed allowed output kind. `assist complete` receives only the parent model result plus a separate known ticket handle. It cannot accept a new range or target in result data. The original full bytes remain during computation. Validate the whole-field generation even for a small selection. An edit then undo to equal bytes invalidates old tickets.

Assist tickets are profile-private bounded semantic-session records, not tied to an individual short-lived CLI process or a publication grant. They can survive the normal context/complete process pair and ownership handoff while their originating editing session remains admitted. Changing target/hook/source, supersession, explicit session closure, deadline expiry, or field editing invalidates them. Owner migration does not cause automatic application; the next explicit completion validates every captured condition. No late result is applied merely on reopening the desktop.

Outline completion returns a local proposal with no comment bodies and no ready bits. `outline use` commits selected suggestions as one structural edit and preserves Suggested status. Per-stop writing assistance is separate. Requesting code for a stop changes neither visit nor readiness; the agent relays an explicit current-content author response through ready.

## Editor integration

Use trusted user configuration with a executable/argument-vector representation. Support the familiar VISUAL/EDITOR environment only under a documented platform-aware parser that does not evaluate shell syntax, command substitution or repository configuration. Ambiguous shell strings are refused with guidance to configure explicit arguments. Launch only on an explicit edit action. Graphical editor preferences do not make the CLI require a GUI; terminal editors remain fully supported.

Create an owner-private field projection outside the repository. Wait for the editor's configured blocking completion, then read with bounded UTF-8 validation and compare the captured revision. Do not assume successful child spawn means the user has finished saving. Conflict/cancellation preserves current draft and offers recovery of the separate edited candidate. Explain that editor plugins/backups/cloud sync are governed by the chosen editor, not a BPR sandbox guarantee. Clean known temp files safely; never follow a substituted link to an arbitrary file.

## Discovery and compatibility

`bpr tour capabilities --json` returns CLI contract version, parent tour format version, local-draft schema compatibility, commands/limits, supported owner protocol, qualified host transports and available approval modes. The skill checks once per new binary/session and rechecks on version mismatch, not every sentence. Unknown capabilities are not inferred from a version number. Failed read never triggers account connection, browser open, executable installation or provider switch without an explicit author operation.
