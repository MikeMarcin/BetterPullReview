---
id: BPR-0453
title: The composed GitHub drawer assertion broke when the map landed
status: Done
rfc: RFC-BPR-0005
depends_on: [BPR-0418]
---

# The composed GitHub drawer assertion broke when the map landed

`qt_composed_github_scale_{100,125,200}` fail at `364c8f6`:

```
FAILED: accepted pull-request content replaces the empty state
```

[[BPR-0418]] put the thread map above the conversation tree, which meant the
drawer's loaded stack page stopped being the `QTreeView` and became the
splitter holding both. `composed_github_tests.cpp:707` asserted
`conversationState->currentWidget() == window.conversation_view()`, and that
identity is no longer the shape of the widget tree.

## Why this got past the closing slice

BPR-0418's focused selector was
`-R 'github_conversation|workbench|thread_map'`. `qt_composed_github_scale_*`
builds the same window through the same drawer and matches none of those
words. The card ran 19/19 green and committed; the full `dev` matrix, run
afterwards out of caution rather than obligation, found this within the hour.

That is the honest lesson and it is not "run the full matrix every time" — the
board deliberately rejects that, and a 29-minute replay per slice buys nothing
a focused selector did not already prove. It is narrower: **a selector chosen
by subsystem name misses tests named after a composition.** A change inside a
widget that the shipped window composes has a blast radius the widget's own
name does not describe. `composed_github`, `application_*_public_path`, and the
window scale suites are all downstream of a drawer edit and only the last one
says so in its name.

## Required final state

- The three composed suites pass.
- The repaired assertion still fails if the drawer shows the wrong page. A
  weaker check that merely tolerates the new structure would trade a real
  regression signal for a green run.

## Verification

```sh
ctest --preset dev --output-on-failure -R 'qt_composed_github|github_conversation|workbench|thread_map'
```

## Done — 2026-08-25

The assertion now requires the loaded page to be a page other than the empty
state **and** to be an ancestor of `window.conversation_view()`. That survives
the structural change without conceding what the check is for: it still fails
if the drawer shows the empty state, and it still fails if the page on screen
is not the one holding the conversation.

The rejected alternative was comparing against the new splitter by name or
object. That would pass for a drawer showing an empty splitter, which is
exactly the state the assertion exists to catch.

### Verification

| claim | command | proves it | result |
|---|---|---|---|
| the three composed suites pass | `ctest --preset dev -R 'qt_composed_github'` | the real GitHub window at three scales | 3/3 pass |
| the repair is load-bearing | same, with the assertion reverted | the three suites fail again at `FAILED: accepted pull-request content replaces the empty state`; build exit 0 in both directions, so it is not a stale binary | **fails without the fix** |
| the drawer suites still pass | `ctest --preset dev -R 'qt_composed_github\|github_conversation\|workbench\|thread_map'` | composition, panel, model property, window and diff scales, public-path courses | 22/22 pass |
| formatting | `cmake --build --preset dev --target coding-format-check` | 425 files | pass |

The full 218-test `dev` matrix that found this was otherwise green apart from
[[BPR-0452]], which is unrelated and predates this run.
