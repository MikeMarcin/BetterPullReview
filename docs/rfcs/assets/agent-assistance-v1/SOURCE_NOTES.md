# Source notes, 2026-11-10

The owner's words from the authoring session, in order, and the list they
refer to. This file is the preserved original: do not edit a note once it is
here. Each note is numbered only so the RFCs and the decision record can
cite it.

1. The question that started the session:

   > The work going on in the "v0.5.0 prerelease targets implementation" task
   > got me thinking where else can we integrate the agent into this app to
   > add more value.

2. The answer listed, in the author's words and not the owner's:

   - Three agent entry points that core names and no surface calls:
     `analyze_latest_upload` ("what changed since I last looked", next to
     Seen and Approve again), `evaluate_proposed_fix` (an Addressed? check on
     a thread, where the agent reports evidence and the reviewer still
     presses Verified) and `analyze_current_file` (explain this file in
     Review, without a trip to the Agent tab).
   - Comment to exact suggested edit: from a draft comment the agent writes
     the suggestion block, and core checks that it applies to the exact
     lines before the reviewer sees it.
   - Failing check triage: an agent action on a failed check that reads the
     log and says whether this change caused it or it is flaky or stale.
   - Author evidence: the agent proposes checks for a local PR draft, runs
     them, and records the results as reported evidence.
   - Self-review on a local PR draft, with findings as private notes.
   - Acceptance rate in Insights: accepted versus discarded suggestions per
     feature.
   - Held to throughout: every agent output lands as something the
     application already has, nothing runs on refresh or open, and nothing
     reaches GitHub without the reviewer's press.

3. After the local-draft pack had been imported without those ideas:

   > The other agent ideas from the initial ask are part of our final
   > suggestions.

4. Said during that earlier import, about one of the ideas, and used here as
   a precedent only:

   > make sure that self-review gets its own prompt in agent settings

Self-review is already RFC-BPR-0115. This pack holds the rest of note 2.
