---
schema: "implement-rfc.card.v1"
id: "BPR-2270"
title: "Comment labels: an editable Feature, so drafted comments carry Conventional Comments labels again"
status: "Done"
rfcs: ["RFC-BPR-0129"]
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2270--BPR-2270--0a26e2f42bfc.json","found_by":"owner, 2026-11-18","related_cards":["BPR-2264","BPR-2267","BPR-2059","BPR-1861","BPR-2258"]}
---

# BPR-2270: Comment labels, an editable Feature, so drafted comments carry Conventional Comments labels again

## Observed

Owner, 2026-11-18: "Make sure when we do the suggested comments fixes that
we're still adding the conventional comment tags. I'm seeing a lot of
comments I draft from a review right now not have any." Refined: "Maybe the
comment tag is another editable feature defaulted to our conventional
comments", and "I don't think we need that safety net, could be a user just
blanks that out because they want all plain comments".

## Acceptance

- AC1. A second editable Feature, `/bpr:comment-labels` ("Comment labels"),
  edited under Settings > Prompts > Features and carried by the settings
  exchange, ships the Conventional Comments convention with BPR-2267's
  decoration rule.
- AC2. The one Feature helper attaches it to every request that writes a
  comment meant to be posted, once however often it is named; an override
  changes what is sent; a blank override sends nothing.
- AC3. The suggested-comment format asks for a label only when Comment
  labels is sent. A drafted comment is the agent's text: its label kept and
  canonicalized, no label added by the app.
- AC4. RFC-BPR-0129 notes the amendment.
- AC5. Focused tests, format and tidy on the changed files, and
  `python3 scripts/verify_documentation.py --root .` pass.

## Cause

Since BPR-2059 the shipped suggested-comment format told the agent to write
no label: "the reviewer sees the finding's severity beside it and chooses a
label in the comment editor". No draft path sets one. The report's Draft
comment and Draft all (BPR-2258), the finding card's Draft, Keep local and
Draft comment on an answer (thread reply, line comment, pull request
comment) all draft the agent's text through
`core::read_agent_comment_suggestion` (BPR-1861), which reads a label the
agent wrote and adds none. BPR-2264 did not cause it; it only drops a
weight decoration the label implies.

## Fix

`/bpr:comment-labels` (`feature.comment-labels`, schema 35, column
`writing_label_convention`) ships the Conventional Comments convention.
`application::prompt_features_for` attaches it, before the Writing style,
to every turn that may write a comment to post. Explain this file, Suggest
the change (which writes code, not a comment) and tour runs do not get it.
A blank override (`core::blank_feature_override`) sends nothing.
`suggested-comment-v3` asks for a label only when Comment labels is sent.
Following the owner's second refinement, the app adds no label of its own
and has no severity-to-label fallback.

## Verification

`core_skill_references`, `core_agent_comment_suggestion`,
`application_writing_delivery`, `application_agent_launch_policy`,
`sqlite_workbench_settings_storage`, `sqlite_agent_configuration_storage`,
`qt_settings_window`, `qt_prompts_page`, `tour_cli_composition`,
`application_agent_invocation_composition_contract` and
`application_profile_tour_handoff_public_path` pass. Format and tidy pass on
the changed files, and so does the documentation gate. In the wider related
run, three tests still expect a weight decoration that BPR-2264 now drops
(`application_agent_chat_conversation_contract`,
`application_agent_finding_workshop_guidance_custom_contract`,
`application_agent_finding_workshop_injection_contract`). They are left for
a follow-up.
