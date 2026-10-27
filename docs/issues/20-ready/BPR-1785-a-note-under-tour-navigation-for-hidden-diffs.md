---
schema: "implement-rfc.card.v1"
id: "BPR-1785"
title: "Each published tour stop says what to do when Next does nothing because the diff is hidden"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1785--BPR-1785--7d1172f3484c.json","kind":"feature","found_by":"owner chat, 2026-10-27","related_rfcs":["RFC-BPR-0040","RFC-BPR-0042"]}
acceptance: []
---

# BPR-1785: A note under each tour stop's navigation for hidden large diffs

## Request

The owner, 2026-10-27, verbatim:

> On the GitHub page for the tour, the the next and prev links don't function
> if the diff is hidden because it's a large file. The workaround is to go to
> the tour overview and then navigate to the next one. Can we add a very brief
> italic note below that line on each tour comment to tell people what to do
> when next doesn't work?

## Grounded current behavior

A published stop's comment ends with a footer line of links:
`[Previous](…) · [Tour overview](…) · [Next](…)`
(`core::render_tour_stop`; parsed back by `core::map_tour_stop`, which takes
the last non-blank top-level line as the footer when it is exactly such a
navigation line; `patch_stop` and the update path replace that footer).
GitHub does not scroll to an anchor inside a diff it has collapsed as too
large, so Next and Previous appear to do nothing there.

## Outcome

- Every published stop's footer is the navigation line followed, on the next
  line, by one short italic line, worded as the owner showed it on GitHub
  (photograph, 2026-10-27):
  `_If Next or Previous doesn't jump (GitHub hides large diffs), use the Tour overview._`
  The owner's own reply: "Something like this". Directly under the links, no
  blank line between, as in the photograph (GitHub renders the line break).
- `map_tour_stop` recognises both footers (with the note, and the old one
  without), so tours published before this still import, map and update; an
  update rewrites an old footer to the new one.
- Nothing else in the body changes; the round trip stays byte-exact for
  everything outside the footer.

## Acceptance (draft)

- Core tests: render includes the note; map finds the footer with and without
  the note and the narrative ends before it; patch/update turns an old footer
  into the new one and leaves the rest byte-identical; import of an old tour
  still succeeds.
- Tour publication fixture course shows the note in each stop body.
- Optional live check on BPR-Test with a large-file stop.
