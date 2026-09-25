# Frozen feedback rounds and portable delivery

## Compose once; share through any channel

A round draft collects the overall action (`discuss`, `plan`, `implement`), goal,
constraints, ordered selected feedback revisions, suggestions and explicitly included
context. Ordinary source notes can be added without creating a round first. `Share
feedback` opens a deterministic preview and freezes on the selected delivery action;
there is no separate mandatory “compile prompt” ceremony. Preparation uses normal
code and text transformations, not a model or lossy summary.

The prepared content contains every selected feedback item, verbatim prose and exact
suggestion bytes, the relevant source identities and excerpts, requested relationships
and one round-level assignment. Shared context is referenced once. Order follows the
reviewer's deliberate conceptual order, not a mandatory filename sort. The recipient
must have the complete instruction set before implementation, but can retrieve more
permitted source and run its own tools afterward. “One turn” means one human handoff,
not one inference request or a prohibition on tool calls.

No label alone authorizes an edit. Plain discussion and illustrative examples remain
valid within an implement round. Include per-item action exceptions when the reviewer
explicitly supplies them. Preserve evidence and uncertainty, not invented acceptance
criteria, rationale, performance promises or test results.

## Canonical payload and identity

`schemas/round.schema.json` defines the first wire shape; [examples](../examples/README.md)
show a small complete payload. The immutable payload identifies the cycle, round,
source revision, item revisions, selected source attachments and exact suggestions.
Its `selection` section declares feedback/context counts and any intentional omissions.
Every included source attachment has a byte length and SHA-256 and is bound to a
source revision/path/range; snippets do not pretend to be complete files.

Use the profile `bpr.cycle.canonical-json.v1`: validated Unicode scalar strings,
lexicographically sorted object keys, preserved array order, UTF-8, no insignificant
whitespace, no floating-point values, and no trailing newline in the hashed payload.
Identifiers/counters are strings; bounded offsets/counts may be exact JSON integers.
Do not Unicode-normalize authored strings. Base64 encodes raw source/replacement bytes.
The envelope stores `payload_sha256` outside the payload. This is this product's
specified canonicalization, not a claim of RFC 8785 compliance. Reject duplicate JSON
keys, unsupported schema versions, invalid base64, mismatched sizes/hashes and lone
surrogates before admitting an imported round.

Frozen content never changes. Re-freezing identical draft inputs reuses the artifact.
Changing content, selection, source bindings or requested action produces a new round
or explicit amendment with a new digest. A revoked local access grant cannot revoke
bytes already copied, saved or retrieved; the UI must not imply otherwise.

## Delivery menu

**Copy to Clipboard** emits the full readable Markdown representation. **Save to File**
emits Markdown by default and offers a portable `.bpr-review.zip` bundle for exact
structured round data and selected attachments. **Use in external agent** exposes the
specific ready round through the provider-neutral local interface and shows a concise
invocation referencing it. **Send to Agent** appears only when an admitted adapter is
installed and permitted. There is no default external recipient or automatic launch.

All routes are projections of one frozen payload. Format-dependent checksums differ;
the canonical payload digest and item identities stay the same. An adapter may add
its own transport envelope but may not summarize away notes, alter replacements,
include other private history or silently change the assignment. Clipboard rich text
is optional; plain text is the complete baseline. Record clipboard write success only
when the platform reports it; never claim read/receipt. Explain that clipboard history
and cloud-synced export folders may retain sensitive source under OS policy.

The share preview names destination/channel, scope, item count, source inclusion,
bytes and limitations. It exposes exactly what will leave the local review. This is
not mandatory metadata form-filling or an extra per-item approval loop. Enterprise
export restrictions can disable a channel; a different channel is not a bypass.

## Readable Markdown contract

The document begins with the assignment, logical project/revision information and
scope. Each ordered item has a stable ID, raw authored body, human-readable source
locations and related references, followed by exact original/replacement text when
supported. Use dynamically sized fences longer than any run in the enclosed text;
include whitespace/encoding notes when they matter. A code fence supplied by the
reviewer must not terminate the surrounding export structure unexpectedly.

A document with CRLF, invalid text or non-text replacement cannot promise a byte-exact
copy/paste patch. Clearly label the rendered view, include exact base64/byte metadata
when necessary, and offer the bundle. Do not silently normalize a patch to make the
Markdown look cleaner. Human readability does not imply executable applicability.
The feedback is intelligible without BPR, a model, internet access or internal IDs.

Default exported locations are project-relative, not usernames/absolute local paths.
Do not automatically include remote credentials, other discussions, model session
logs, classification internals or prior rejected attempts. Selected context can reveal
source; the user sees that in preview. A file export is not a full local-history backup.

## Bundle and import contract

A bundle contains `review.md`, `round.json` (the canonical-payload envelope), and
optional content-addressed objects under `objects/<sha256>`. A bounded file manifest
lists each non-manifest member, type, size and digest. The manifest itself is the
sole unlisted member; its schema and exact member set are validated, avoiding a
recursive self-hash. Suggestion bytes remain inside or explicitly
referenced by the authoritative round; readable derived files are not competing edits.
Opening a bundle imports an immutable external round into a private quarantine/read
view. Editing it forks local authoring with provenance; it never mutates the sender's
round. Attaching it to an existing cycle requires explicit logical-project and source
mapping, not a path selected by the archive.

Reject absolute/drive/UNC paths, traversal, encoded traversal after decoding, NUL,
symlinks, duplicate entries, case-fold/normalization aliases, decompression bombs,
unsupported compression/encryption and unlisted/mismatched members. Extract objects
only to generated private names with no executable behavior. Never overwrite source
files or auto-apply patches on import. Do not load external URLs or linked local files
from Markdown. Failed validation leaves no admitted partial round.

A response file is a different schema with claimed author and item-linked replies;
import it as data, validate round digest and item membership, then preview attachment.
A colleague can instead send ordinary prose. The reviewer may attach that manually;
no custom protocol or JSON requirement blocks subsequent revision capture.

## Completeness and oversized reviews

Expose total item/context counts and completeness. A read or export over budget
returns a typed limitation; it cannot omit trailing items and report success. A CLI
consumer may follow bounded pages within the same handoff, checking the final digest
and counts before acting. If its usable context cannot contain the full assignment,
it reports that limit and asks for an explicit partition rather than implementing the
first page. BPR may suggest groups, but it does not secretly turn one approved round
into several independent assignments. Token estimates, when offered, are estimates
for the chosen consumer and never a guarantee of available model context.
