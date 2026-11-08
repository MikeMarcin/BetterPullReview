---
schema: "implement-rfc.card.v1"
id: "BPR-1908"
title: "Split the window composition out of github_application.cpp"
status: "Done"
rfcs: []
depends_on: ["BPR-1907"]
priority: "P2"
metadata: {"baseline":"3e19d0e87","mandate":"Owner, 2026-11-06: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round, landing on local main without pushing; at most about 2,000 lines per file for this initial pass; subfolders welcome."}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f57ecff2f","evidence":{"kind":"observation","detail":"window_composition.hpp declares the class; sixteen window_*.cpp responsibility sources define it unchanged; probe states in window_probe_states.hpp."}},{"criterion":"AC2","outcome":"pass","revision":"f57ecff2f","evidence":{"kind":"command","detail":"Pinned span stays in the root; the mutation branch is byte-identical after one-tab dedent of 62 lines; pin resealed; live_mutation_isolation and round-trip contract pass."}},{"criterion":"AC3","outcome":"pass","revision":"f57ecff2f","evidence":{"kind":"command","detail":"bpr_composition_source.cmake and composition_source.hpp; defect, UIforETW, workbench, ledger, round-trip, agent chat and finding contracts pass."}},{"criterion":"AC4","outcome":"pass","revision":"f57ecff2f","evidence":{"kind":"command","detail":"Full dev and tests-off builds; format 22; 100/105 on first cut and 12/12 scanner rerun on final bytes; two pre-existing failures recorded. Windows/Linux not built."}},{"criterion":"AC5","outcome":"pass","revision":"f57ecff2f","evidence":{"kind":"observation","detail":"Completed card and journal line accompany the source for the local main fast-forward."}}]
---

# Split the window composition out of github_application.cpp

Round 4 of the `github_application.cpp` refactor. `github_application::implementation`,
one window's composition, is a 19,300-line class: 552 members, 467 member
functions (a 1,564-line constructor among them) and 42 nested types, all
defined in the class.

## Acceptance

- AC1: Declare the class in a private header and define its members,
  unchanged, in responsibility sources: construction, navigation and
  restoration, sign-in, workbench, defects, tours, agent setup, agent chat and
  notes, review drafts, review mutations, review ledger, commit filter, read
  state and Overview, and shutdown.
- AC2: Keep the security-pinned span, `compose_review_mutation` through
  `refresh_review_projection`, in the root, so the live provider's single
  construction site does not move. Reseal the production mutation-branch pin
  only after checking that its bytes differ by one leading tab per line, and
  update only the indentation-dependent markers of the contracts that find it.
- AC3: The composition contracts that scan composition source read the
  composition root's source set; counts keep their meaning (definitions, not
  declarations).
- AC4: Preserve behavior: full dev and tests-off builds; architecture,
  composition (source and Release), round-trip and the composed courses;
  format and documentation gates.
- AC5: Land the change, the completed card and a journal line on local main.

## Result

Source `f57ecff2f`; `github_application.cpp` goes from 35,440 to 17,481 lines.
The class is declared in `window_composition.hpp`; its members are defined,
unchanged, in these sources (lines):

| Source | Lines | Responsibility |
|---|---|---|
| `window_construction.cpp` | 1,644 | the 1,564-line constructor and the destructor |
| `window_agent_chat.cpp` | 1,728 | private notes, agent invocation, comment workshop, side chat |
| `window_agent_setup.cpp` | 1,467 | agent cards, asked conversations, agent and clone setup |
| `window_navigation.cpp` | 1,435 | recents, tabs, search opens, review navigation and restoration |
| `window_workbench.cpp` | 1,400 | workbench intents, diff selection, preparation, classification, appearance |
| `window_tours.cpp` | 1,382 | tour sessions, writing runs and tour navigation |
| `window_review_ledger.cpp` | 1,231 | review drafts on threads, the review ledger and private review |
| `window_sign_in.cpp` | 1,001 | profiles, CLI accounts and setup sign-in |
| `window_review_drafts.cpp` | 966 | review submission, held replies and shared drafts |
| `window_read_state.cpp` | 960 | read state, receipts and the Overview |
| `window_commit_filter.cpp` | 947 | commit filter, whole diff, moved suggestions |
| `window_defects.cpp` | 600 | defects, base numbering, thread anchors |
| `window_device_authorization.cpp` | 591 | device authorization and public preview |
| `window_github_reads.cpp` | 456 | rereads, link location, GitHub host intents |
| `window_review_mutations.cpp` | 432 | review conversations, mutation targets and terminals |
| `window_shutdown.cpp` | 392 | shutdown and cleanup |
| `window_probe_states.hpp` | 712 | test-only probe states the composition courses keep |

Files over 500 lines stay within the owner's 2,000-line ceiling for this pass;
each is one responsibility of the window. One exception remains:
`window_composition.hpp` is 2,373 lines. It is the declaration of one class
with 552 members, 467 member functions and 26 production nested types;
the rationale comments moved to the definitions and the test probe states to
their own header. Going lower means regrouping the window's members into
collaborating objects, which changes construction and destruction order: a
design change, not a behavior-preserving split.

The security-pinned span (`compose_review_mutation` through
`refresh_review_projection`, about 1,370 lines) stays in the root as
out-of-line definitions, so the live provider's single construction site and
its isolation allowlist do not move. The production mutation-branch pin is
resealed: its 62 lines are byte-identical to the previous pin's after removing
one leading tab each, checked mechanically against `240c2d4d6`.

Contracts that scan composition source read the composition root's source set
through `tests/end_to_end/bpr_composition_source.cmake` (round-trip, defect,
UIforETW, workbench and review-ledger contracts) and
`tests/end_to_end/composition_source.hpp` (agent chat and finding courses,
definitions only so call-site counts keep their meaning). The Python
contract's insertion point, provider texts and CLI admission body pattern
follow the out-of-line indentation. Includes that the root guarded, or reached
only through guarded includes, stay guarded in the split files; a first cut
broke the tests-off build on exactly that and the composition contract's
Release self-test caught it.

Checks on macOS arm64: full dev build and tests-off composition build;
clang-format 22; 105-row selection with 100 passing on the first cut plus the
12-row rerun of every changed scanner on the final bytes (composition with
Release self-test, composition source, round-trip, defect, UIforETW,
workbench, review ledger, agent chat and finding, live-mutation isolation,
architecture), all passing. The two remaining failures predate the refactor
(`application_agent_invocation_composition_contract`,
`application_review_tour_publication_manual`). Windows and Linux were not built.
