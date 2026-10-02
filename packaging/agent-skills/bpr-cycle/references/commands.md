# Command reference

`bpr cycle capabilities --json` is authoritative for what this binary offers; its `consumer_commands` are the only commands a harness runs. Every result is one `bpr.cycle.cli-result.v1` document on standard output: `outcome` is `ok` with `data`, or `error` with `code`, `message`, `next` and sometimes `detail`. A page with more after it has `complete` false. IDs, revisions, counts and byte sizes are decimal strings. The forms below set the role the POSIX way; in PowerShell set `$env:BPR_CYCLE_ROLE = "consumer"` first, in cmd `set BPR_CYCLE_ROLE=consumer`.

```sh
BPR_CYCLE_ROLE=consumer bpr cycle capabilities --json
BPR_CYCLE_ROLE=consumer bpr cycle round read <cycle> --round <round> --json
BPR_CYCLE_ROLE=consumer bpr cycle round read --round <round> --json
BPR_CYCLE_ROLE=consumer bpr cycle round read <cycle> --round <round> --page-bytes <bytes> --json
BPR_CYCLE_ROLE=consumer bpr cycle round read <cycle> --round <round> --budget-bytes <bytes> --json
BPR_CYCLE_ROLE=consumer bpr cycle round page <cycle> --round <round> --cursor <cursor> --json
BPR_CYCLE_ROLE=consumer bpr cycle round source <cycle> --round <round> --json
BPR_CYCLE_ROLE=consumer bpr cycle round source <cycle> --round <round> --cursor <index> --json
BPR_CYCLE_ROLE=consumer bpr cycle round source <cycle> --round <round> --source <source> --json
BPR_CYCLE_ROLE=consumer bpr cycle round source <cycle> --round <round> --source <source> --range <start>:<end> --json
BPR_CYCLE_ROLE=consumer bpr cycle response append <cycle> --round <round> --file - --json
```

`<cycle>` is `RC-n` and `<round>` is `Fn`. `<cursor>` and `<index>` are only ever the `next` the previous result gave. The form without a cycle runs in the folder the cycle is bound to and finds only that cycle.

## Reading a round

`round read` data names the round: `cycle`, `round`, `payload_sha256`, `project`, `source_revision` and `source_consistency`. It gives the `assignment` (`action`, `goal`, `constraints`, `write_scope`), the counts `items_total`, `suggestions_total`, `sources_total`, `instruction_bytes` and `source_bytes`, the flags `instruction_set_complete` and `intentional_omissions`, and a page: `items`, `first_item`, `items_through` and `next`. `round page` gives the same identities and the next page. Pages default to 1 MiB; `--page-bytes` takes 4096 to 2097152, and the cursor keeps the size it was made with. `--budget-bytes` states how much instruction text you can hold; a larger round is refused with its measure and no items.

Each item has `id`, `revision`, `body`, `action_override`, `anchors`, `related_item_ids` and `suggestions`. An anchor has `kind`, `path`, `source_revision_id`, `start_byte`, `end_byte` and `blob_sha256`; byte offsets are half-open. A suggestion has `id`, `revision`, `intent` (`exact_replacement` or `illustrative`), `state` (`proposed` or `observed_already_applied`), `anchor`, `already_applied_result_revision`, and `original` and `replacement`, each `length`, `sha256` and `base64`. Decode the bytes and check their SHA-256 before you compare or apply them.

## Attachments

Without `--source`, `round source` lists the round's attachments (`sources`, from `first`, at most 256, then `next`), each with its `id` (`S1`), path, revision, kind and digests. With `--source` it returns that attachment's bytes as `base64` with their `sha256`, at most 1 MiB at a time between `start` and `end`; `next_range` is the following `<start>:<end>`, or null. Nothing else of the project is readable this way, and you need nothing else from BPR: read your own checkout with your own tools.

Run commands one at a time and keep each result. Do not build a command line by pasting round text into it.
