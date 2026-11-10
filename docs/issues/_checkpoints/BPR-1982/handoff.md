# BPR-1982 legacy destination repair checkpoint

Assignment `legacy_destination_fix`, milestone `legacy-destination-fix`, owner `/root/legacy_destination_fix`, mandate `tour-destination-repairs-r1-legacy-destination-fix`. Baseline `992ebba9c6ea35822a24ea6c315676a865fb5f65`. Scope: one saved-receipt P2 upgrade regression.

Plan: claim follow-up; add actual SQLite/session regression; demonstrate failure; implement bounded compatibility and future-retarget preservation; focused tests/static/docs; explicit source and guarded finish commits. No remote writes, desktop effects, server, protection or worktree. Foreign `.codex-attachments/` untouched. All commands currently exited.

Legacy state: Destination A, Original B, no Mapped, exact_original, confirmed Remote at B. New receipts retain independent Original/Mapped and confirmed Destination. Centralize effective published subject and retain legacy confirmed subject before a local mapping transition.

Independent reviewer reproduction: `/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr1981-review-o69kane5`; previous BPR-1981 acceptance stays historical. Next: claim and bind, then regression.
