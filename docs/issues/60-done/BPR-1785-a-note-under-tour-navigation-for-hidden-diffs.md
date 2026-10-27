---
schema: "implement-rfc.card.v1"
id: "BPR-1785"
title: "Each published tour stop says what to do when Next does nothing because the diff is hidden"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1785--BPR-1785--7d1172f3484c.json","kind":"feature","found_by":"owner chat, 2026-10-27","related_rfcs":["RFC-BPR-0040","RFC-BPR-0042"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "0453fa92f4f1cbcb8fc5b1067220bcda603f8e89", "evidence": {"kind": "command", "detail": "core_review_tour_markdown navigation_note: with LF and CRLF, a three-link stop ends with the links then the note on the next line; map_tour_stop's footer holds both and the narrative ends before them; a stop without the note still maps and with_tour_navigation_note adds only the note; a Tour-overview-only stop has none; the note alone is not a footer."}}, {"criterion": "AC2", "outcome": "pass", "revision": "0453fa92f4f1cbcb8fc5b1067220bcda603f8e89", "evidence": {"kind": "command", "detail": "core_review_tour_update: a five-stop tour published without the note plans five stop edits, each the old body plus the note line, and no index edit; equal-body routes still plan zero writes. core_review_tour_import, core_feedback_* and core_review_tour pass."}}, {"criterion": "AC3", "outcome": "pass", "revision": "0453fa92f4f1cbcb8fc5b1067220bcda603f8e89", "evidence": {"kind": "command", "detail": "application_review_tour_publication_native and _compat, _stop, _description, _conflict, _maintenance, _reconciliation, _manual, _unsupported, cold import and CLI courses pass; the native course checks every staged stop body ends with its links then the note."}}]
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

## Acceptance

- Core tests: render includes the note; map finds the footer with and without
  the note and the narrative ends before it; patch/update turns an old footer
  into the new one and leaves the rest byte-identical; import of an old tour
  still succeeds.
- Tour publication fixture course shows the note in each stop body.
- Optional live check on BPR-Test with a large-file stop.

## Built

- `core::tour_navigation_note` holds the owner's wording; `render_tour_stop`
  puts it on the line under the links when they include Previous or Next. A
  stop with only Tour overview has nothing to explain and gets no note.
- `map_tour_stop` takes the footer as the links line, optionally followed by
  the note, so tours published before this import unchanged.
- `plan_tour_update` passes each stop body through
  `with_tour_navigation_note`, so the next update of an older tour adds the
  note and nothing else. Reconciliation is untouched: an older stop is not
  reported as locally edited.
- RFC-BPR-0040's stop format names the note.

## Not verified

- No live check on BPR-Test with a hidden large diff; GitHub's rendering of
  the line break was taken from the owner's photograph.
- An older tour's next update rewrites every stop once to add the note, even
  when nothing else changed in it. Because the update plan is no longer
  empty, the author of an older imported tour may see Publish update offered
  before they edit anything; publishing it once adds the notes.
