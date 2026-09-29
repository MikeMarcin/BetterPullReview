---
schema: "implement-rfc.card.v1"
id: "BPR-1213"
title: "Keep Overview folders flat and collapse only their files"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"Owner clarification during BPR-1199 review, 2026-09-26","related_rfcs":["RFC-BPR-0037"],"related_issues":["BPR-0934","BPR-0972","BPR-1199"],"scope":"Tracking-only later Overview presentation correction; not a new performance requirement or private-comparison diagnosis","session":"2c2e72e3af2e2fd7fa1a566ec49bc5ca","candidate":"BPR-1199-private-comparison-v001","candidate_revision":"c982586fbfc774614c6f4993c4e6c81d450d15be; executable sha256 4c4adf38b82511e9d2b77e7fc85f6f8fd3a4a327e15341ca65e23aecdd23bf58","fingerprint":"ef60b7926f20db034da29907f0d9e30c4d5bf2f7aeb48e4981f3083599d0d212","decision_id":"a74d529b-4aff-4662-b97a-a78d0277b255","mandate_revision":"r4-with-owner-compose-20260925-r3; coordinator tracking addendum 2026-09-26"}
---

# Keep Overview folders flat and collapse only their files

## Owner clarification

On 2026-09-26, the owner supplied this unrelated Overview clarification in
[[BPR-1199]]'s review, verbatim:

> Unrelated: screenshot shows something got lost in translation on the guidance to make folders collapsable on the overview page. folders should still be flat how they were before so that folders without files underneath are a single row and files are always aligned with the left column. I just wanted to be able to hide the files. I didn't want to break out the folder hierarchy.

Source: session `2c2e72e3af2e2fd7fa1a566ec49bc5ca`, candidate
`BPR-1199-private-comparison-v001`, final Reject decision
`a74d529b-4aff-4662-b97a-a78d0277b255`, saved
2026-09-26T18:29:43.795421+00:00. The screenshot's candidate was
`c982586fbfc774614c6f4993c4e6c81d450d15be`, executable SHA-256
`4c4adf38b82511e9d2b77e7fc85f6f8fd3a4a327e15341ca65e23aecdd23bf58`,
review fingerprint `ef60b7926f20db034da29907f0d9e30c4d5bf2f7aeb48e4981f3083599d0d212`.
Only the Overview paragraph is carried here. The private-comparison diagnosis
and review attachment stay with BPR-1199 and its runtime evidence.

The desired presentation stays flat: folders that have no files directly under
them take a single path row, and every file aligns with the left column.
Disclosure hides or shows the files without breaking the path into an indented
folder hierarchy. [[BPR-0934]] implemented a nested tree from the earlier
collapse request; this later correction preserves that terminal card's history
and records the owner's clarified intent separately.

[[BPR-0972]]'s current performance work preserves the existing tree, flat,
changed-first and compact modes. Its implementer confirmed that the scaling
patch does not introduce the hierarchy. This card therefore owns the later
presentation correction; it does not make that independent scaling work or
the private comparison repair responsible for a redesign.

## Intended work and acceptance

Activate this card separately and inspect the then-integrated Overview rows and
mode choices. Preserve the owner's flat presentation while retaining the
existing file identities, changed-file metadata, selection, keyboard operation
and bounded incremental refresh. Keep Qt responsible for layout and local
expansion; no provider, Git, storage or publication effects arise from disclosure.

- AC1: The actual Overview uses the owner's flat folder presentation: no expanded indented ancestry, folder paths without intervening files remain one row, and file labels align at the same left column. The evidence explicitly compares this result with the quoted request.
- AC2: Accessible pointer and keyboard disclosure hide only the associated files and restore their identities and order. Collapse and same-authority refresh retain sensible focus/selection without an unintended file activation; authority replacement clears the former expansion state.
- AC3: Focused Qt and composed read-only fixtures cover nested paths with empty intermediate folders, direct and nested files, sibling folders, collapse/reopen, incremental refresh and mode transitions. Capture the real Overview at representative widths and fonts, retain the existing scaling bounds, and prove disclosure causes no remote mutation.
- AC4: Record the owner's judgment of the built flat rows and collapse behavior against the exact candidate, asking independent visual criteria separately. Preserve unavailable native-platform evidence without inventing acceptance or rewriting BPR-0934's historical result.

## Registration state

Backlog for a later authorized implementation round. No source, fixture, model,
mode default or RFC criterion was changed when this card was registered. The
future course uses synthetic or recorded public fixtures and read-only navigation;
this card grants no live private-PR inspection or remote mutation authority.
