---
schema: "implement-rfc.card.v1"
id: "BPR-1917"
title: "Agent settings' built-in prompt editor gets a real design"
status: "Parked"
parked_on: "human-review"
rfcs: ["RFC-BPR-0092"]
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-06, coordinator chat after using BPR-1900","related_cards":["BPR-1916"]}
acceptance: []
---

# BPR-1917: Agent settings' built-in prompt editor gets a real design

## Owner (2026-11-06)

"having used it for a while I want to do more than just enhance the tour text with my agent. I want to ask questions, get back and forth and have it update it. For example right now I have Solution section where the draft said something I don't understand, but I don't really have a way to ask for clarification. It also feels like the output of the draft is entirely devoid of any formatting. It feels just like big walls of text. I have a skill here for /technical-book-humanizer but I also have a skill elsewhere for technical writing that is slightly more tuned to those purposes. I wonder if I can't have a hook to let me reference a custom skill in my prompts if the skill is available in as a personal user skill or a skill in the repo being reviewed. I also wonder if we can improve the settings agent tab where we're editing the built-in prompts its a bit weak maybe some /frontend-design:frontend-design could improve that."

## Change

Load `frontend-design:frontend-design` first, and say so.

Redesign the Agent settings area where the built-in prompts are edited. Today it's weak. Aim for:
- a clear list of the built-in prompts, with what each is for and where it's used (Tour drafting, Enhance, Agent chat, ...);
- a roomy editor with a monospace-friendly font, line wrapping, and a visible "changed from default" state with Reset to default;
- a preview of what the run receives, showing BPR-1916 `/skill` references resolved (if BPR-1916 has landed; otherwise a placeholder that BPR-1916 completes);
- save state that's obvious, with no surprise loss on close.

Keep the widget free of effects. Persistence goes through the host.

## Acceptance

- AC1. Every built-in prompt is listed with its purpose, and can be edited, saved and reset.
- AC2. A changed prompt is visibly marked, and Reset restores the default.
- AC3. Owner look: attended Accept/Reject on a frozen build of the redesigned settings.
- AC4. The documentation gate passes.

## Implementation (2026-11-06)

Loaded `frontend-design:frontend-design` first. Settings, Agents & context:
a list of every built-in prompt under where it is used (Tour tab, Comments,
Agent tab; the review skill now listed with them and showing its own
editor), each with its use on a second line and Changed or Unsaved on the
right; the chosen prompt's name, what it is for and what uses it; Edit and
Preview; a fixed-pitch, wrapping editor with `/skill` completion
([[BPR-1916]]); Built-in or Changed from built-in; Save and Discard changes
beside the save state and byte count; Reset to built-in (Undo reset brings
the text back unsaved) and Test saved prompt. Unsaved edits are kept per
prompt in the panel until saved or discarded, so moving between prompts or
closing Settings loses nothing. Preview shows what a run receives
(`application::preview_writing_hook`), opened at the prompt's own part, with
named skills brought in. Persistence stays the host's: the panel's Save and
Reset are intents. Fits the 760 by 480 minimum Settings window with no
sideways scrolling. Tests: `qt_settings_window` (listing, marker, unsaved
kept, Save, Reset, Undo reset, Preview with a skill).

## Attended review v001 (2026-11-06)

Parked on `human-review` for AC3. AC1 and AC2 are met by
`qt_settings_window`; AC4 by the documentation gate (`DOCUMENTATION_OK`).

- Frozen build: `bpr-1916-1917-v001` from 354656a39, root
  `/Users/jaeger/bpr-1916-1917-v001.6lrosi2z`, executable sha256
  `deef523682d229b545d788f2a4fcdbbeb77fbcbc6eaf172fa45230037bc99f12`. It
  opens Settings on Built-in prompts with Problem statement changed through
  its own Save.
  Launch: `python3 /Users/jaeger/bpr-1916-1917-v001.6lrosi2z/launch.py`.
- Session `4b8b91fc663e6c51243591fe761ba0f5`, state
  `/Users/jaeger/BPR-attended/review-state-bpr-1916-1917-v001`; candidate
  `BPR-1917-prompt-editor` (one Accept/Reject question).

## Owner question on v001 (2026-11-06)

The owner, on the BPR-1916 candidate of the same session: "why does the review
skill render differently than the other ones?" It was an inconsistency, not a
distinction: the review skill's row showed its own panel as it was built for
the old Settings column, so it repeated its own heading under the pane's title,
had no "Used by" line or Built-in/Changed state, used a proportional editor,
said Restore default where the others say Reset to built-in, and its row never
showed Changed. Fixed: the pane gives it the same title, purpose, use line and
Built-in or Changed from built-in state (and Changed in its row, from the
review skill host's projection); its own heading and introduction are hidden;
its editor is the prompts' fixed-pitch one at their height; its Restore default
reads Reset to built-in. What stays different on purpose: no Preview or Test
(the skill's text is itself what every review request carries, in its brief,
and it runs from the Agent tab), and its version offer (Compare, Use version N,
Keep mine) and "Save my copy to Downloads first", which only the versioned
review skill has. `qt_settings_window` checks it. BPR-1917 has no recorded
decision yet; the fix is not in the v001 build.
