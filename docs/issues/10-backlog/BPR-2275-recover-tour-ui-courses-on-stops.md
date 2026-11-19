---
schema: "implement-rfc.card.v1"
id: "BPR-2275"
title: "Re-cover the tour UI courses that ran on the Problem and Solution, on a stop"
status: "Backlog"
rfcs: []
depends_on: ["BPR-2274"]
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"test-debt","target":"0.5.0","reservation":"docs/issues/_reservations/BPR-2275--BPR-2275--1eb0c0ff7e7b.json","found_by":"BPR-2274, 2026-11-18"}
---

# BPR-2275: Re-cover the tour UI courses that ran on the Problem and Solution

## Why

[[BPR-2274]] removed the tour's Problem and Solution. Several courses used
those two fields as the vehicle for behaviour that is not about them: the
side chat's quoting, prompts, Update link, Copy and Resend; the live typing
handoff between the desktop and `bpr tour`; and website reconciliation of
published text. BPR-2274 trimmed what could not be moved in the same change.

## Trimmed

- `qt_tour_side_chat`: the main scenario (sections 1 to 4 and the part chats)
  ran on the Solution's chat; only the clock check remains.
- `application_review_tour_publication_reconciliation`: the course reconciled
  a website edit of the description's brief; removed from the course list
  with `application_review_tour_publication_description`, which tested the
  description write that no longer exists.
- `application_profile_tour_handoff` (`profile_tour_handoff_contract.py`): the
  live typing, caret, input-method and assist checks on the Problem field.
- `application_review_tour_composition_public_path`: the IME, undo and
  restored-editor checks on the Problem and Solution editors.

## Acceptance

- AC1. Each trimmed behaviour that still exists is covered again on a stop's
  text or chat: quoting, prompts, Update, Copy, Resend and Delete in the side
  chat; reconciliation of a website edit to a published stop; typing in a
  stop while `bpr tour` edits it.
- AC2. The restored courses pass on macOS.
