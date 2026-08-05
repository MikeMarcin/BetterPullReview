---
name: bpr-attended-review
description: Prepare, collect, validate, and record attended human-approval verdicts through deterministic digest-bound artifacts: offline review pages with exact per-subject decisions and batch-bound verdict export, and markdown operator packets for single bounded decisions. Use when the agent needs to run an attended review checkpoint, build a human approval page for captured image, video, or audio evidence, present visual or UI acceptance-criterion evidence for sign-off, review diff-rendering or workbench interaction states, write an operator review packet for a terms or dependency-acquisition decision, validate a returned verdict against its exact batch manifest, or record verdicts as repository evidence.
---

# BPR Attended Review

Turn an attended checkpoint into a deterministic, digest-bound artifact the
user can judge, and turn the user's response into validated repository
evidence. This skill never decides; it makes the human decision exact.

## Core Rule

An attended verdict is made only by the user. Never fabricate, infer, or
placeholder one — not from partial page state, screenshots, conversational
shorthand, or an older batch whose bytes changed. Every decision binds an
exact subject identity plus content digest; a changed byte reopens the gate.
The page and the packet cannot write repository evidence: the agent records
the returned verdict, and only after validation.

## Choose The Form

- One bounded yes/no decision over facts and evidence (third-party terms
  acceptance, dependency acquisition, declared-versus-observed deltas,
  repair authorization): write a markdown operator packet. Follow
  `references/operator-packet.md`.
- Multiple subjects or media to inspect (UI acceptance criteria,
  diff-rendering states, captured image, video, or audio batches): build
  the offline review page. Follow `references/offline-review.md`.

## Offline Review Page Loop

1. Write the `bpr.attended_review_batch_input` manifest: gate, scope, the
   declared false-only downstream claims, and one subject per decision with
   its media groups, criteria, and identity files.
2. Build the package:

   ```sh
   python3 .agents/skills/bpr-attended-review/scripts/build_attended_review.py \
     build --input <input.json> --out <package-dir>
   ```

3. Present `<package-dir>/index.html` to the user and stop. Open it for them
   where the runtime can; a path to paste is friction at a decision point.
   Images open at native resolution on click, so evidence too fine to judge at
   grid size is still judgeable. Do not create placeholder decisions or an
   empty verdict when nothing awaits review.
4. Ask the user to resolve every subject and paste back the complete
   exported verdict JSON.
5. Save the paste to a temporary file and validate it:

   ```sh
   python3 .agents/skills/bpr-attended-review/scripts/build_attended_review.py \
     validate-verdict --manifest <package-dir>/attended-review-manifest.json \
     --verdict <pasted.json>
   ```

6. Apply only accepted exact identity/digest pairs to the current gate's
   normal integration procedure. Rejected identities remain evidence.

## UI And Criterion Reviews

For interaction and rendering acceptance, the decision subject is the
acceptance criterion, and its media are the state captures that prove or
falsify it. Follow `references/ui-review.md` for the mapping, the capture
provenance conventions, and the reference-image disclaimer.

## Recording Evidence And Card Lifecycle

Record the input, package, pasted verdict, and validation evidence
immutably, and drive the issue-board lifecycle for pending checkpoints, per
`references/integration.md`.

## Related Skills

- `betterpullreview-implement-rfc` defines when attended checkpoints occur
  during RFC work and how they batch at run boundaries.

## Validation

```sh
python3 -m unittest discover -s .agents/skills/bpr-attended-review/scripts -p 'test_*.py'
```

Verdicts validate against the exact generated batch manifest before any
integration, and `git diff --check` stays clean.
