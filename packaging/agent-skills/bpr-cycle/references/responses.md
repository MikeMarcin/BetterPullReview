# Responses

A prose reply by item ID is a complete answer. A structured response is optional: append one only when the user asks for it and the grant allows it (a read-only grant refuses it). It records what you claim; BPR verifies nothing, changes no note, wording or round, and publishes nothing.

Write one `bpr.cycle.response.v1` document for the whole round and pass it on standard input with `response append <cycle> --round <round> --file -`. Copy `cycle_id`, `round_id` and `round_payload_sha256` from the read; take each `item_revision` from that item's `revision`.

```json
{"schema": "bpr.cycle.response.v1", "response_id": "<response-id>", "cycle_id": "<cycle>", "round_id": "<round>",
 "round_payload_sha256": "<payload-sha256>",
 "claimed_author": {"display_name": "<harness name>", "kind": "agent", "provider": null, "model": null, "effort": null},
 "result_source_revision_id": null,
 "items": [{"item_id": "<item>", "item_revision": "<item-revision>", "disposition": "addressed", "body": "<what you did, by item>",
            "evidence": [{"kind": "path", "value": "<changed path>"}]}]}
```

- `disposition` is `addressed`, `partially_addressed`, `disputed`, `blocked` or `discussion_only`. None of them means verified; there is no verified disposition.
- List only items you have something to say about; say plainly what you left undone.
- `evidence` entries are inert text with `kind` `text`, `path`, `commit`, `url` or `test_report`, at most 256 per item. A test report you name is a claim, so name only tests you ran.
- `claimed_author` is a claim. Fill `provider`, `model` and `effort` only with what you actually know; null otherwise. `kind` is `agent`, `human` or `unknown`.
- `result_source_revision_id` stays null: BPR names its own captures, and a Git commit is not one.
- `response_id` starts with a letter (letters, digits and `.`, `_`, `:`, `-`). Reuse it only to retry exactly the same document; new content needs a new ID.

A response is at most 16 MiB, each item `body` at most 256 KiB. Through a running desktop, standard input carries up to 4 MiB.

The answer's `appended` is true when stored and false when the identical document was already there. After an `uncertain` result, append exactly the same document again; it is never stored twice.
