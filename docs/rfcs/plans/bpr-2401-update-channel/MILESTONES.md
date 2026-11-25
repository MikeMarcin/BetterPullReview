# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **bpr-2401-update-channel** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## m1-update-channel

A durable application-wide channel preference selects eligible BPR updates and preserves exact package and install authority.

**Work:** `BPR-2401`

**Prerequisites:** None

**Completion:**

- Stable only is the default for existing and new update settings, and both choices persist.
- The Settings course reaches the shipped updater composition and offers published compatible RCs or stable releases according to the selected channel.
- Channel changes invalidate stale checks and staging, and install refresh preserves candidate and native package identity.

**Boundary:** Hand back the locally verified update-channel feature without publishing or replacing any installed copy.
