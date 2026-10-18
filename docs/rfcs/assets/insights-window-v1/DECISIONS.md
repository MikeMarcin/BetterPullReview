# Design record: Insights window

## Scope and provenance

- **Draft.** RFC-DRAFT-0024 revision 2. Revision 1 was written on
  2026-10-17 against `e9b1cee35`; revision 2 (2026-10-18) records the
  answers to Q1 to Q4.
- **Source.** One chat session with the owner. The owner asked about a
  BetterPullReview version of AllTheThings' Insights window. Three rough
  HTML mockups were shown in chat and aren't kept. The owner then asked
  for this pack with `/author-rfc`.
- **Reference app.** AllTheThings 0.14.6 (`4c2965d`).
- **Coverage.** No grilling or attended review was requested.

## Decisions

| Decision | Requirement/acceptance | Status and actual answer | Rationale/consequences | Depends on / affected artifacts |
|---|---|---|---|---|
| D1 Cost and fun stats in one window | RFC "Summary", "Reviews"; AC03, AC06, AC07 | Settled. Owner: "So it shows cost, but it also shows some fun stats. We could track total comments, notes, files accepted/seen. Reviews completed/loaded." | Reviews is a tab, not a separate feature. Summary shows two of its counts | DRAFT-0005, DRAFT-0006 |
| D2 Energy is secondary, judged on idle cost | RFC "Energy"; AC08, AC09, AC12 | Settled. Owner: "energy shouldn't be as important here as it is in ATT but if we're going to have it we should keep its foreground and background split. I probably care more that it's very cheap when I'm not using it" | Tiles lead with background. Foreground lives in the paired fact table. AC12 measures an idle hour | DRAFT-0007, DRAFT-0008, DRAFT-0010 |
| D3 One fixed window size | RFC "The window"; AC01 | Settled. Owner: "We had explicit rules in ATT to make sure all tabs were the same window size so we didn't get that disconcerting resizing motion." | Every page fits one locked size. A test switches every tab and range | DRAFT-0001 and every UI card |
| D4 Chart quality bar is AllTheThings | RFC "Charts"; AC02 | Settled. Owner: "I think the ATT graphs are a bit better than your mockups here." | The RFC spells out AllTheThings' chart behaviour, plus three improvements: maximum labels, consistent gaps, accessible descriptions. The owner judges the result in an attended review | DRAFT-0004 onward |
| D5 Other clients are derived, never fetched | RFC "Summary"; AC01, AC03 | Proposed by the author. Not answered | Insights must not spend the budget it reports. AGENTS.md records that `gh api rate_limit` misreported during BPR-1344 | DRAFT-0003 |
| D6 No time-spent tracking | RFC "Reviews" | Proposed by the author in chat, and the owner continued without objecting. Treat it as proposed, not answered | Active-time tracking reads as surveillance | DRAFT-0005 |
| D7 GraphQL shows no split | RFC "GitHub"; AC04 | Proposed by the author | GraphQL charges by query cost, which BetterPullReview doesn't read | DRAFT-0003 |
| D8 No second deletion path | RFC "Storage"; AC05 | Proposed by the author, following the rule in BPR-1602 | Manage cached repositories opens the existing controls | DRAFT-0002 |
| D9 No "Reset review stats" in v1 | RFC "Data, privacy and lifetime" | Explicitly deferred | Profile purge and BPR-1602's reset everything already clear the store. A separate reset can follow if asked | none |
| D10 Main-thread stalls as the input proxy | RFC "Work"; AC10 | Proposed by the author | No product code measures input latency. Stall detection is cheap, works on every platform, and matches the 100 ms target | DRAFT-0009 |
| Q1 Menu placement | RFC "The window"; AC01 | Settled 2026-10-18. Owner: "go with recommendation". The app menu on macOS and Help elsewhere, with ⌘⌥⇧I or Ctrl+Alt+Shift+I | Only the entry point is affected | DRAFT-0001 entry point |
| Q2 Backfill | RFC "Reviews"; AC06, AC07 | Settled 2026-10-18. Owner: "start from first launch with insights, app isnt distributed to anyone but me yet so its no big deal". No backfill, and the header says "since <date>" | The store's first-run behaviour is affected | DRAFT-0005, DRAFT-0006 |
| Q3 Idle budget figures | RFC "Energy"; AC09, AC12 | Settled 2026-10-18. Owner: "go with recommendation". 2 s of CPU per background hour, and 6 wakeups per minute | The budget line and the AC12 pass mark are affected | DRAFT-0008, DRAFT-0010 |
| Q4 Streaks | RFC "Reviews"; AC07 | Settled 2026-10-18: cut. Owner: "I think we just cut streaks, too hard to get it right with holidays and whatnot." | The Reviews tab has no streak. The calendar and records stay | DRAFT-0006 |

## Open frontier and explicit deferrals

- **Unanswered material choices:** none. The owner answered Q1 to Q4 in chat
  on 2026-10-18.
- **Deferred internal detail:**
  - exact window size;
  - how the process-metrics sampler is split across adapters;
  - the operation-category mapping;
  - store schema;
  - how the stall detector hooks the event loop.

  The observable behaviour of each is defined in the RFC.
- **Coordination.** BPR-1602 (Ready) must classify `insights.sqlite3`
  under its "reset everything" option only. Whichever of the two lands
  second reconciles this.

## Checks and confirmation

- **Structural validity.**
  `python3 .claude/skills/implement-rfc/examples/milestone-plans/validate.py docs/rfcs/assets/insights-window-v1/milestone-plan.json`
  printed "Structurally valid; references and semantic coverage
  unchecked." `MILESTONES.md` is that validator's `--render` output.
  - Not checked: live issue and RFC references resolve only after import.
  - Not checked: whether milestone exits cover every requirement.
- **Design readiness.** Every milestone has its material answers. Internal
  detail stays deferred as listed above.
- **Confirmation.** The answers are the owner's quoted statements in D1 to
  D4 (2026-10-17) and Q1 to Q4 (2026-10-18). Nobody has confirmed the pack
  as a whole. The owner's request
  was "let's /author-rfc a pack for this guy".
- **Execution eligibility.** Not evaluated. An authorized `implement-rfc`
  run checks live mapping, dependencies and the mandate.
- **Documentation gate.** `python3 scripts/verify_documentation.py --root .`
  printed `DOCUMENTATION_OK`.

## Continuation

Hand the pack to `/implement-rfc`. It allocates live IDs, imports the RFC as
the next RFC-BPR number, and adopts this plan.
