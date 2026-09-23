# Scoped result protocol

The JSON Schema defines the proposed logical result body, independent of the existing agent transport envelope. Glaze DTOs plus explicit validation may implement it; this proposal does not require a runtime general-purpose JSON Schema engine. The host owns request IDs, source eligibility, exact raw byte interval, current generations and permissions. Echoed model data cannot acquire those authorities.

`rewrite/ok` has replacement text and bounded private context notes. Notes reference only supplied IDs and are not inserted into the public body. `rewrite/insufficient_context` leaves the editor unchanged and shows a small reason. `outline` contains at most 24 **suggestions per invocation** as a work bound, not a target route length; retained manual tours have their own larger safety ceiling. Suggestions cannot contain a comment body or Ready status.

Schema character limits are not byte limits. The host also enforces UTF-8 byte bounds, aggregate result bounds, valid supplied reference membership, no invalid Unicode, duplicate-response rejection and current target/generation. A syntactically valid JSON result with an unknown source reference or a stale field is refused. No model-provided range or target is accepted. A received plain string/code-fenced JSON wrapper is not silently reinterpreted as some other protocol when a strict structured result is required.

Whitespace selection remains selection; no-selection plus whitespace-only field is empty drafting. Equal replacement produces no undo entry. Cancellation/failure never clears the field. The transport can stream bytes internally, but the editor changes only after a complete validated response.

`examples/text-transactions.json` is an offline reference-splice corpus. Production acceptance still needs actual native undo/redo, UTF-16 cursor mapping, event sequencing, generation invalidation and application persistence tests. Passing the pack validator does not prove those product properties.
