# Implementation integration checklist (not a guessed CLI grammar)

Intake records the actual parent-pack IDs and service/task names. RFC-BPR-0051
requires the following narrow capabilities through shared code:

| Capability | Boundary and expected result |
| --- | --- |
| Catalog | Return all twelve stable IDs and guidance; visibility settings never change support. |
| Parse/preview body | Read-only exact-body annotation; plain, malformed, unknown and limited states are explicit. |
| Explicit draft-prefix edit | Existing shared draft ID + expected revision; one local edit or honest conflict, no publication. |
| Existing-data counts | Exact scope/columns/resolution mode and accepted coverage, no new per-label fetch. |
| Existing native preflight | Own published/pending context, where the inherited review interface already exposes it; no new publisher. |

The proposed annotation JSON schema specifies stable public facts without imposing
an internal C++ class layout. Follow the inherited CLI's versioning envelope; do
not replace it just to use this schema verbatim. Human-readable output is default,
JSON explicit, and additive changes retain old commands. Describe actual commands
in the delivered live CLI guide after examining the imported implementation.

Record a table of actual feedback/workshop/promotion/tour task IDs, canonical
source, effective default, override, provenance and Settings owner. Add the supplied
fragment only at relevant default hooks. Do not append to overridden prompts or
repurpose problem/solution/outline hooks. An agent running the thin pr-tour skill
uses the current session for requested prose work; it does not start another model.

The catalog, parser, draft operation and counter population must be shared with
the desktop C++ services. No GUI subprocess, duplicated regex, direct provider
mutation, second recovery store or autonomous background labeling is permitted.
