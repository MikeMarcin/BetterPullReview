---
schema: "implement-rfc.card.v1"
id: "BPR-1597"
title: "Settings by task: Home & queue, Review and Comments replace Reviews & queue"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"owner-decision","found_by":"Owner pick in attended-review session 0e5dbaff (decision 3229df02), 2026-10-17, and the owner's amendment naming the middle page Review","governing_rfc":"RFC-BPR-0048","related_rfcs":["RFC-BPR-0018"],"amendment":"RFC-BPR-0048 owner amendment 3 (2026-10-17)","related_cards":["BPR-1088","BPR-1289","BPR-1566","BPR-1568","BPR-1584"],"reservation":"docs/issues/_reservations/BPR-1597--BPR-1597--2851f28bd65b.json","acceptance":["BPR-1597:pages","BPR-1597:workshop","BPR-1597:search","BPR-1597:kept","BPR-1597:gates","BPR-1597:owner"]}
acceptance: []
---

# BPR-1597: Settings by task: Home & queue, Review and Comments replace Reviews & queue

## Owner decisions

- BPR-1568 v002 (a Comment workshop group on Reviews & queue) was rejected in
  session fbb0fdf5 (decision 97a50021), verbatim: "I think reviews & queues is
  too much of a big grab bag of settings and we should find a better
  organization."
- From three Settings organisations shown as mockups in Light and Dark, the
  owner picked **A, by task** in session 0e5dbaff (decision 3229df02), with no
  note. Then, through the coordinator: name the middle page **Review**, not
  Reviewing.
- This card folds in [[BPR-1568]] (the label helper's move) and closes it.
  Recorded as RFC-BPR-0048 owner amendment 3.

## Design

Designed with the `frontend-design` skill in the existing Settings row grammar.

- **Sidebar:** General · Accounts & hosts · Repositories · Home & queue ·
  Review · Comments · Appearance · Notifications · Agents & context · Storage &
  privacy. Repositories stays right after Accounts & hosts (owner direction
  2026-10-07); the mockup had shown it after Home & queue, and the final
  question says so.
- **Home & queue** (Reviews & queue's remainder): queue defaults, observer
  rules, the way to Repositories. Home's Filter menu says "Open Home & queue".
- **Review:** Opening (A pull request opens in; On return, compare), Reading
  (Mark a file read; Comments drawer opens on; and BPR-1584's Reading width
  once it lands), Layout (Drawer controls; Line details; Center ribbon width).
- **Comments:** Labels (About Conventional Comments; Label marks; Overview
  counts; Mark colours; Overview chips), Comment workshop (Feedback guidance:
  "Send with comment workshop requests" with an info glyph; Guidance text: a
  link to Agents & context › Review feedback), Descriptions and comments
  (Images).
- Every group's Reset covers its own rows. The writing hooks panel keeps only
  its On/Off line, naming Comments › Comment workshop; Restore default there
  restores only the text.
- Stored preferences and keys are unchanged. The `settings_page` value is the
  sidebar row and is not stored.

## Acceptance

- **BPR-1597:pages** The sidebar lists the ten pages in order; each page's
  title is its row; Review, Comments and Home & queue host their rows; each
  group's Reset covers its rows (negative control: the old page title fails the
  sidebar check).
- **BPR-1597:workshop** The switch sits on Comments › Comment workshop only,
  follows the published record, writes the stored value, is named for
  assistive technology; Guidance text opens the Review feedback hook; the
  composed courses still carry the guidance only while it is on.
- **BPR-1597:search** Settings search finds every moved row on its new page and
  section, and nothing under Reviews & queue; Return opens the row's page and
  changes nothing.
- **BPR-1597:kept** No store, schema or key change; `bpr tour hooks` unchanged.
- **BPR-1597:gates** Focused rows, clang-format 22, clang-tidy, the
  documentation gate and one full dev ctest before landing.
- **BPR-1597:owner** The owner accepts the built Settings in Light and Dark.

## Evidence

Recorded at finish.
