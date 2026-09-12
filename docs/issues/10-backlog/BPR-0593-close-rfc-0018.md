---
schema: "implement-rfc.card.v1"
id: "BPR-0593"
title: "Verify and close RFC-BPR-0018"
status: "Backlog"
rfcs: ["RFC-BPR-0018"]
depends_on: ["BPR-0592","BPR-0678","BPR-0679","BPR-0680","BPR-0681","BPR-0682","BPR-0683","BPR-0684","BPR-0685","BPR-0686","BPR-0687","BPR-0688","BPR-0689","BPR-0730","BPR-0731"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0018"]
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0018

## Current frontier, 2026-09-12 (d25, mandate r16): integrated slices and exact remaining asks

Run `attended-prep-2026-09-10`, dispatch d25, producer
`/root/integrator_d25`, mandate r16, plan `ux-redesign-pack-0028`, milestone
`attended-preparation-pack-head`. The clean baseline was `8302a401` on main,
28 commits ahead of origin. The v009 session handed back at 362 and released
protections at 363. No session host, watch or process remains. Every d25
child result is collected; writers and commands are quiescent.

The owner **did not accept RFC-BPR-0029 as written**. The revised
[RFC-BPR-0029](../../rfcs/RFC-BPR-0029-machine-independent-epoch-identity-update-history.md)
(`2f151966`) carries A2's A→B→A as two evidenced occurrences sharing a
head, A3's labeled GitHub event or consistently selected commit dates, and
the prohibition on treating surviving commits as push boundaries or a
derived comparison base as the original historical base. Existing catalogue
rows remain lossless even when host/event evidence cannot yield a portable
key. The RFC stays **Proposed**; [[BPR-0743]] stays **Blocked** and [[BPR-0646]]
waits behind it. The one remaining owner ask is whether to accept the *revised*
RFC. No epoch product work or RFC-0029 closeout ID was created.

Three slices are **Done**:
- [[BPR-0748]]: owner-approved conditional macro purple, with operators,
  literals, headers, attributes, builtin-test operands and `#define` bounds.
  Source `a8b0394a`, finish `3c25b3a9`. The first red showed
  `__cplusplus` near-grey; independent review found two more counterexamples
  and the same syntax owner reproduced and corrected both. Final five checks,
  format and tidy pass (`build/logs/bpr-0748-d25-correction-*`).
- [[BPR-0749]]: one-pixel rule centered at `(width - 1) / 2` in the native
  handle (macOS 7 → column 3; Fusion 4 → column 1). Source `b00c276e`, finish
  `24f7e2ed`. Style-aware raster red before fix, required Qt scales green
  after (`build/logs/bpr-0749-d25-{red-test,green-tests-2}.log`). Cursor
  delivery remains [[BPR-0746]]'s **Blocked, owner-declined-probe** issue.
- [[BPR-0736]]: real Return and Enter queue key events dispatch the selected
  row once; invalid selection and mouse activation are covered. Source
  `7e53ff78`, finish `d240806a`; red and three-scale green logs are
  `build/logs/bpr-0736-d25-{red-test,green-tests}.log`.

[[BPR-0735]] is **Parked on human-review** (`48296f42` implementation,
`0e37a9c3` partial finish). The three named clangd checks load the dev
compile database and report none of the missing headers/types. Full check
mode exits 3 on unrelated Homebrew clangd code-action tweak self-tests on
two files; the Qt test file exits 0. Its sole remaining ask is the owner's
observation that the editor noise is gone in one named file. No attended
packet is needed.

At source head `d240806a`, the pack-head integration pass is **18/18 green**:
both application composition contracts, workbench and Settings Qt paths,
syntax and architecture boundaries (`build/logs/d25-pack-head-integration.log`).
The documentation gate and `git diff --check` pass. Plan exit 2 is evidenced
at this source head. Exits 1 and 3 still need a current-head app-release
bundle and digest-bound attended packet; exit 4 still holds BPR-0646's exact
human ask. No v010 packet was prepared because BPR-0646 is not yet a
candidate. RFC-BPR-0018 and RFC-BPR-0028 remain open; BPR-0730 and BPR-0731
stay on their Windows/Linux stations.

The next independent large slice is [[BPR-0729]]'s accepted one-pane Read
surface, suitable for a fresh bounded engineering context; it was not
claimed in d25. The next human actions are the revised RFC-0029 acceptance
answer and, independently, BPR-0735's editor observation. The d24 frontier
below remains evidence for prior work not superseded here.

## Intermediate frontier, 2026-09-12 (d25, mandate r16): owner answers received; engineering in progress

Run `attended-prep-2026-09-10`, dispatch d25, producer
`/root/integrator_d25`, mandate r16, plan `ux-redesign-pack-0028`, milestone
`attended-preparation-pack-head`. The clean dispatch baseline was `8302a401`
on main (28 commits ahead of origin). The v009 attended session handed back at
362 and released protections at 363; it has no host, watch or process.

The owner **did not accept RFC-BPR-0029 as written**. A2 says A→B→A is two
occurrences sharing a head, without inventing an occurrence GitHub cannot
evidence. A3 selects GitHub event time where present, a consistently selected
commit timestamp otherwise, explicit labels distinguishing commit dates from
push times, local first-seen kept separate, and deterministic display ordering
that does not claim equal timestamps establish push order. RFC-BPR-0029 stays
Proposed; BPR-0743 stays Blocked and BPR-0646 waits on it. A revised proposal
and its remaining owner ask are in preparation; no epoch product work is
authorized by these answers.

The owner answered **yes** to BPR-0748's separate syntax question, with the
specific operator/literal/operand and `#define` boundaries in mandate r16.
BPR-0748 was promoted and claimed (`a3c9ae4a`, `4a767cef`); its bounded syntax
slice is owned by `rfc_worker_bpr0748_syntax`. BPR-0749 was promoted and
claimed (`6bbb49be`, `90aff668`); its independent seam slice is owned by
`rfc_worker_bpr0749_seam`. Both workers own only their named source/test paths;
the integrator owns documentation, acceptance, commits, checks and push.
No worker result has yet been accepted. Do not start overlapping writers.

The d24 frontier below remains the evidence for prior work until superseded by
this dispatch's completed results.

## Current frontier, 2026-09-12 (d24, mandate r15): the epoch work has its own RFC, and BPR-0742 is Done

Dispatch `attended-prep-2026-09-10` d24 (mandate r15), same milestone
`attended-preparation-pack-head` of plan `ux-redesign-pack-0028`. The owner
answered d23's one question in chat: "It's own rfc". The RFC work changed no
product source. Nothing is pushed.

### RFC-BPR-0029, Proposed and not accepted (`10af12bc`)

[RFC-BPR-0029](../../rfcs/RFC-BPR-0029-machine-independent-epoch-identity-update-history.md)
records BPR-0743's design pass as proposed decisions, and the owner's sentence
("An Epoch is an update to the PR, …") as its motivating requirement.

**How the id was chosen.** RFC ids in this repository have no counter and no
receipt; the receipts under `docs/issues/_reservations` are for card ids only.
0029 is the next free number, and `git log --all -S RFC-BPR-0029` is empty. No
closeout card is allocated while the RFC is unaccepted, as with RFC-BPR-0021
and RFC-BPR-0028.

**What it settles, as proposals:**
- **Relationship.** It supersedes RFC-BPR-0003 in part, and only once accepted:
  what identifies an epoch, and what base OID and observed time mean for an
  imported epoch. RFC-BPR-0003 got a pointer and stays Auto-Verified.
- **A correction to the design pass.** "A digest over the authority scope"
  cannot mean `pull_request_authority_scope_id`, which is random and partitioned
  by account. The inputs are GitHub's numeric pull request id, a host term, and
  the head OID. `git_host_id` is not a provider value in the paths examined.
- **A gap the design pass left.** It asserts a cross-machine order but defines
  no ordering key, and after imports the ordinal order is local.

**What it carries open.** Seven questions:
1. head collision;
2. unobserved base-branch events;
3. paging a large timeline;
4. merge-base validation;
5. the date, and with it the order;
6. the unbuilt site list (a search adds `src/app/src/large_review_fixture.hpp:305`);
7. the host term, and what backfill can compute.

**Three owner asks, one question each.** A1 is acceptance, A2 is the same
commit twice, A3 is an update's date. Nothing is built before A1 is answered
yes, and A2 and A3 fix the identity and the order.

### The board (`17cbd086`, `37f9ba90`)

- [[BPR-0743]]: rfcs `["RFC-BPR-0029","RFC-BPR-0028"]`. It is off human-review
  and **Blocked on `rfc-bpr-0029-owner-asks`**. AC1 stands; AC2 to AC4 are
  unchanged.
- [[BPR-0646]]: `depends_on: ["BPR-0743"]`, now in **Backlog**. On this board a
  parked card cannot hold an unfinished dependency, and nothing about the card
  waits on the owner. Its handle criterion is unchanged, and it returns to
  human-review when BPR-0743 is Done.
- RFC-BPR-0028 got a dated pointer, because its AC-02 waits on this work.
  RFC-BPR-0018 needs no change.
- The v009 packet's two links were repaired in their own commit, paths only,
  following d23's precedent.

Checks at `37f9ba90`:
- `verify_documentation.py` prints DOCUMENTATION_OK.
- Board `validate` is valid across 741 cards.
- `queue --rfc RFC-BPR-0003` is 60 of 60 done again.
- `queue --human-waiting` no longer lists either card.

### BPR-0742 is Done (`7c6e2ca9` the work, `3b80eea2` the finish)

An unreadable workbench layout store now says so. Composition passes each
window its one availability decision (`!WorkbenchPersistenceAvailable`). The
window keeps "The saved workbench layout is unavailable." on its status line
past the projections that clear a host failure. BPR-0718's fallback is
unchanged: composed defaults, and the store left untouched.

- **Reproduced red first**, against the product code at `004ee4ec` with only
  the course's new assertion: `status=No pull request is open in the
  workbench.` (`build/logs/bpr-0742-d24-red.log`).
- **Integration pass, 13 of 13**, on the tree that was committed
  (`build/logs/bpr-0742-d24-integration.log`):
  - `application_composition_contract`, 143.89 s;
  - `application_workbench_composition_contract`, 22.06 s;
  - `application_review_round_trip_composition_contract`, 11.22 s, with its two
    source digests repinned for the `github_application.cpp` change;
  - the layout, restart and multi-window courses;
  - `qt_workbench_window` and `qt_composed_github`, each at three scales;
  - `architecture_boundaries`.

  clang-format is clean on the five touched C++ files. **Both application
  composition contracts stay green.**
- **Not rerun:**
  - The review-restoration course (about 586 s): nothing it asserts reads the
    status line, and its store is readable.
  - The `coding-format-check` target, which d23 recorded red at HEAD for three
    unrelated files.
- **The slice owner stopped at its turn limit** before returning. No process of
  its was left running. The integrator inspected its diff and logs, adopted the
  partial work, and ran the verification above itself.

### Commits, in order

All carry the pack's `Co-Authored-By: Claude Opus 5` trailer. No history was
rewritten.
- `10af12bc`: RFC-BPR-0029
- `17cbd086`: the v009 packet links, paths only
- `37f9ba90`: BPR-0743 and BPR-0646 rebound
- `51dac838`: claim BPR-0742
- `004ee4ec`: the frontier before BPR-0742
- `7c6e2ca9`: BPR-0742's work
- `3b80eea2`: finish BPR-0742
- then this frontier

### Noticed, not created

Two shells running `find / -name bpr-0746-cursor-probe.log`, started by an
earlier dispatch of this coordinator session, were still running throughout.
They write nothing to the repository, and this dispatch left them alone.

### Next, in order

1. **RFC-BPR-0029's three asks, to the owner.** A1 is acceptance, A2 is the same
   commit twice, A3 is an update's date. Nothing epoch-related can be
   implemented before A1. [[BPR-0743]] is Blocked on the asks, and [[BPR-0646]]
   waits on BPR-0743.
2. [[BPR-0749]] when the seam is next touched, and [[BPR-0748]] as its own
   attended candidate.
3. No v010 packet has been prepared, and nothing forces one. Neither
   RFC-BPR-0018 nor RFC-BPR-0028 became closable.

d23's frontier follows. It is still authoritative on everything not corrected
above.

## Frontier, 2026-09-12 (d23, mandate r14): the probe refused, the contract green, BPR-0743 back with one question

Dispatch `attended-prep-2026-09-10` d23 (mandate r14), same milestone
`attended-preparation-pack-head` of plan `ux-redesign-pack-0028`. Four cards
moved, one new card, one design pass recorded and escalated, and the milestone's
exit criterion 2 is green for the first time in three dispatches. No product
source changed; one test course did. Nothing is pushed.

### What the owner's message did, and on what basis

His message, in chat, in full: "I don't care to do any cursor probe, i've
already confirmed that the cursor is working correctly now." He wrote it after
being told that the probe log was never captured, that his v009 accept rested on
a criterion whose objective half was missing, and that d22's reconciliation is
an account and not a diagnosis. The message makes two separate statements, and
they land differently.

**[[BPR-0739]] is Done (`1abfb9c5`), on his word, and the card says so.** AC3
("the owner resizes the conversations drawer on the signed bundle with the
cursor visible throughout") has only ever had one possible witness, and nothing
here can observe his screen. This time the criterion was put to him and he
answered it knowing it was disputed. That is not the v009 aside, which stays
refused as a verdict. The report is also bound to a known binary, and that was
checked:
- The probe instance was still running as pid 72017, and its file hashes to
  `c2381f32…`, the v009 candidate revision exactly.
- `diagnostics.log` showed no other instance.
- `git diff --stat 35438244 aff9fc64 -- src/adapters/qt` is one file, with zero
  lines matching cursor, splitter, handle, `underMouse` or `paintEvent`, so it is
  the same program he rejected in v008.

**What it rests on is written on the card:** his word and nothing else. There is
no log, no measurement and no product change on this path, and the reversal
against v008 is unexplained. BPR-0746's hypothesis 1 predicts exactly what he
described (fresh entries work, a resting pointer decays), so the close does
**not** refute it. One report of the arrow returning reopens the card.

**[[BPR-0746]] does not close. It is Blocked on `owner-declined-probe`
(`0e092e30`).** Closing it on the same sentence would be wrong, because its two
open criteria do not ask him for anything:
- AC1 asks for a log.
- AC2 asks for a check asserted against the hypothesis that log would select.

He has declined the only instrument that produces either, and no agent can stand
in for him: every hypothesis turns on what AppKit does on the next *real*
pointer motion. AC2 is recorded as unobtainable, not pending. What landed at
`bbc4db98` checks the admissibility of the card's evidence, not cursor delivery,
and is not recorded as satisfying AC2. AC4 passes on BPR-0739.

The card leaves human-review because nothing is waiting on him: he answered.
Blocked is the lane that says the work cannot proceed and names why. A new
`parked_on` token would have needed a tracker-rule change, and `human-review`
would have been false. Nothing is refuted. The three hypotheses and the
corrected absolute-path command are kept.

**`handleWidth` is measured, not left open.** `PM_SplitterWidth`, read off the
shipped Qt 6.10.3 in a scratch program under the offscreen platform, is **7 for
the macOS style and 4 for Fusion**, and the offscreen default style is `fusion`.
The "seven pixels" both cards quote is right for the signed bundle, and d22's 4
is Fusion, so there is no contradiction. The consequence is that the Qt suite
measures a style the product does not ship. That, together with his unanswered
"it wasn't drawn in the right location" (`rule_edge()` puts the rule in the hot
zone's boundary column), is **[[BPR-0749]]** (new, Backlog, P3).

### The composition contract ran, and what it settled

The owner quit the probe instance ("I quit the running instance"; shutdown
logged at 17:45:33Z). That was verified before anything ran: pid 72017 was gone,
no BetterPullReview process existed, and nothing held the catalogue or
`activate.sock`.

`application_composition_contract` **passed in 172.22 s on the first attempt**,
with nothing changed. That demonstrates d22's diagnosis: the block was the
profile lease and nothing else.

**[[BPR-0744]] is Done (`dc231174`).** All 11 AC3 tests were rerun at `f953ce9c`
rather than reused, although `git diff --stat bbc4db98 HEAD -- src tests
packaging` is empty.

**Milestone exit criterion 2 is green at the pack head.** Beyond those 11, 13
more passed (`build/logs/d23-exit2-integration.log`):
- `application_workbench_composition_contract`, the second composition contract
- `qt_workbench_window_scale_{100,125,200}` and `qt_composed_github_scale_{100,125,200}`
- `qt_settings_window` and its three scales, and `qt_settings_effect_boundary`
- `sqlite_workbench_settings_storage`

`architecture_boundaries` and the documentation gate also pass.
`coding-format-check` stays red only for the three files already red at HEAD,
none of them touched here.

**[[BPR-0748]]'s broadening question is untouched** and is not implemented on
the strength of the accept.

### BPR-0743: the design pass, and the exact ask

**Parked on human-review (`b61e9c05`), AC1 pass, nothing implemented.** The full
pass is on the card (`f953ce9c`), and a short note is on RFC-BPR-0003.

**The signed-out obstacle does not exist.** The probe was run with no
credential: `GET /repos/NVIDIA/stdexec/issues/1846/timeline` returned **HTTP
200** against the 60-per-hour budget. It names both force pushes with GitHub-side
ids and the head each moved *to*. One of those heads, `6ffbd0ac`, is absent from
the commits list and still fetchable, which demonstrates the "commits aren't
updates" claim rather than asserting it.

The same response exposed three limits:
- The REST event has **no before-commit**. GraphQL has one and is refused
  anonymously.
- Array position is not an ordering key, and `committed` dates are author dates.
- An ordinary push produces no event at all.

**The decisions:**
1. **Derivation.** A digest over the authority scope and the **head OID**,
   carried beside `review_epoch_id`. It is not the event `node_id`, which exists
   only for force pushes and which the catalogue excludes by design. It is not
   the base/head pair either, because the base is time-dependent even for a live
   read. A force push back to an earlier head would give two updates one
   identity; that is open and recorded.
2. **Ordinal.** Not redefined. Redefining it can't buy portability while
   Incarnation stays in the id. Two of its seven serializations are beyond the
   catalogue's reach (git refs on disk, and MCP and agent-launch bindings already
   emitted). Density is an adapter guarantee that paging depends on.
3. **Identity can be made stable; population cannot be made equal.** A head seen
   live and then force-pushed over can't be recovered by any other machine.
4. **The base OID is mandatory and origin-blind** (`epoch_transition.cpp:2199-2218`),
   and the timeline supplies none for a historical update.
5. **Migration.** Neither re-mint nor renumber. Add a column, backfill it from
   the stored head, and let the import append. This works because a historical
   epoch already advances neither live ordinal.

**The conditional fires, on kind rather than size.** Identity, base OID, observed
time and the schema version all move: four of the six things RFC-BPR-0003 pins.
**The ask, one question:**

> The epoch work you asked for changes what RFC-BPR-0003 says an epoch *is* —
> what identifies it, what its base OID means when GitHub only remembers the
> head, and whether its timestamp is when you saw it or when it was pushed.
> Do you want it as an RFC of its own, or as an amendment recorded against
> the closed RFC-BPR-0003?

[[BPR-0646]] stays parked with its handle criterion unchanged, and it can't be
asked until this lands.

### BPR-0741 is Done; BPR-0742 was deliberately not started

**[[BPR-0741]] (`aab99d2b` the work, `e72a1347` the finish).** The red was
reproduced at HEAD before any change. The course now signs in from the landing
card before its first paste, and it passes in 80 s against 180 s. The layout and
restoration courses still pass (the restoration course at 586 s against 1800 s,
its normal runtime). The integrator checked the claim that no pin was dropped,
separately from the slice owner: one assertion was replaced by four, and every
other message is identical and in the same order.

**[[BPR-0742]] is not started.** It is a P3 product change in the composition
path, the same `github_application.cpp` branch the two composition contracts
exercise, and those contracts went green for the first time this dispatch. It
belongs in a fresh context, not the tail of this one. It stays Ready and nothing
blocks it.

### Commits, and attribution

In order:
- `0e092e30`: BPR-0746 Blocked, BPR-0749 carded
- `1abfb9c5`: finish BPR-0739
- `f953ce9c`: the design pass
- `db2f416f`, `4428f162`: protected packet links, paths only
- `dc231174`: finish BPR-0744
- `b61e9c05`: park BPR-0743
- `aab99d2b`: BPR-0741's work
- `e72a1347`: finish BPR-0741
- then this frontier

Every commit here uses `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`,
per the pack's standing rule. No history was rewritten and the attribution
question is not re-raised.

### Next, in order

1. **The owner's answer on BPR-0743**: its own RFC, or an amendment. Nothing
   epoch-related is implementable until then, and [[BPR-0646]] waits behind it.
2. **[[BPR-0742]]**, in a fresh context. Keep both application composition
   contracts green.
3. **[[BPR-0749]]** when the seam is next touched, and [[BPR-0748]] as its own
   attended candidate with two pictures.
4. No v010 packet was prepared and none is forced. Neither RFC became closable:
   nothing touched RFC-BPR-0018, and RFC-BPR-0028 still holds BPR-0743, BPR-0646,
   BPR-0729, BPR-0653, BPR-0736, BPR-0746 and BPR-0749.

d22's frontier follows. It is still authoritative on everything not corrected
above.

## Frontier, 2026-09-12 (d22, mandate r13): the v009 verdicts are consumed

Dispatch `attended-prep-2026-09-10` d22 (mandate r13), same milestone
`attended-preparation-pack-head` of plan `ux-redesign-pack-0028`. The four
v009 verdicts are read, reconciled and landed. Two cards are Done, three keep
their human-pending disposition with the reasons recorded, one new card
carries a question that was not answered, and one product change landed.

**Every verdict was verified before it was used**, not taken on trust: all
four decisions are `actor: human` and each decision fingerprint equals both
the live candidate's and the one in
[session-identity.json](../../verification/reviews/rfc-bpr-0028-attended-v009/session-identity.json).
None is stale.

| Candidate | Outcome | What it did |
|---|---|---|
| `BPR-0734-read-one-pane` | accept, empty note | [[BPR-0734]] **Done** (`80762210`), [[BPR-0745]] **Done** (`51dd2af2`), RFC amendments landed, [[BPR-0729]] rewritten |
| `BPR-0739-seam-hover` | select, "Drop it" | source change landed; [[BPR-0739]] stays Parked, AC3 still `fail` (`b8fb5983`) |
| `BPR-0744-directive-ranges` | accept, empty note | AC4 `pass`; AC3 still blocked; question carried to [[BPR-0748]] (`bdcd563d`) |
| `BPR-0746-cursor-probe` | accept, note | criterion **not** satisfied; stays Parked (`d32a3117`) |

Commits: `bbc4db98` (the work), `6ac7d344` (packet link repair, paths only),
then the five board commits above.

### The contradiction, reconciled

His note on the hover verdict said "now that the cursor is fixed", and
[[BPR-0746]] exists because it is not. Both reports are his and neither is
discarded.

**It is not the binary.** `git diff --stat 35438244 aff9fc64 -- src tests` is
six files, all of it [[BPR-0744]]: the classifier, the appearance roles and
three test or fixture files. `git diff 35438244 aff9fc64 -- src/adapters/qt`
contains no line matching cursor, splitter, handle, `underMouse` or
`paintEvent`. The executable he ran is `c2381f32…`, recomputed on disk. On
this path the v008 and v009 bundles are the same program.

**The escape route was tested and is false.** The obvious way out — that the
lit rule in his v008 screenshot was stale paint and never proved his pointer
was on the handle — was written as a test assertion and went **red**:
`Qt::WA_Hover` is set on the handle, arrival costs one repaint and departure
another, and `underMouse()` follows both, at scales 100 and 200. The
assertion was rewritten to the measurement rather than the measurement
explained away. So BPR-0746's refutation of BPR-0739's cause 3 keeps its
foundation, and that measurement is now in the suite so it cannot rot.

**What is left is the gesture, and it is hypothesis 1.** v007 ("flickers only
for a brief moment and the returns back to the normal cursor") and v008 (a
pointer held still for a photograph) are a pointer **at rest**. v009's
candidate instructed him to "move the pointer **on and off** that rule to see
both states", so he was **re-entering**. Hypothesis 1 predicts exactly that:
Qt sets the cursor once on enter, AppKit reverts it on the next real motion,
and Qt never re-asserts — every fresh entry works and every rest decays to the
arrow. That is an **account, not a diagnosis**. It is what the missing log was
written to discriminate, hypotheses 2 and 3 are not excluded, and nothing was
implemented on it.

### The log still does not exist, and why

He ran the probe. Pid 72017 is the bundle's executable, started 16:06:37Z from
`ttys001`, piped to pid 72018 `tee build/logs/bpr-0746-cursor-probe.log`. Both
were still alive at 16:57Z. `lsof` on 72018 shows cwd
`/Users/jaeger/Documents/GitHub` — one directory above the checkout — and open
descriptors only fd 0 on the pipe and fds 1 and 2 on the tty. **There is no
output file and there never was.** The output exists only in that terminal's
scrollback. His "Dunno about the test you had me run" is what that looks like
from his side.

The packet caused it: the copy label read "run this in a terminal, in the
repository" over a value with an absolute executable path and a **relative**
`tee` path. Fixed at the authoring site — `AGENTS.md` now requires every path
in an attended candidate's command to be absolute, and requires one question
per candidate criterion, which is the other defect this round exposed. The
corrected command, absolute throughout, is on [[BPR-0746]]:

    QT_LOGGING_RULES='qt.qpa.input.mouse*=true' \
      /Users/jaeger/Documents/GitHub/BetterPullReview/build/app-release/src/app/BetterPullReview.app/Contents/MacOS/BetterPullReview \
      2>&1 | tee /Users/jaeger/Documents/GitHub/BetterPullReview/build/logs/bpr-0746-cursor-probe.log

### Two empty notes, read as narrowly as they deserve

**BPR-0734.** An empty-note accept answers no question; it accepts the
candidate's four criteria as written. Two of those criteria named a
consequence and asked him to refuse it, and no refusal came, so **Review shows
one pane too** and the one-pane gutter has **no whole-file overview**. But the
page did not show either: direction v7 has four states and **none of them is
Review**, its own prose offers the alternative reading with the ask, and
neither a pure insertion nor a pure deletion is drawn, so the "one ribbon
rather than two" case is words too. Accepted-as-written, not
accepted-as-shown; [[BPR-0729]]'s rewritten scope carries all three as work
that still needs a drawing or an ask.

**BPR-0744.** The costed question that rode the candidate — whether every
identifier inside `#if`, `#ifdef`, `#ifndef`, `#elif`, `#undef` and
`defined()` should take the macro purple rather than only ALL_CAPS ones — came
back with an empty note for the second round running. It is **not** read into
the accept and **not** implemented. [[BPR-0748]] carries it with its costing,
and records that the next packet must ask it as its own candidate with its own
criterion and with two pictures.

### `application_composition_contract` was not run, and why

The predecessor expected a free profile. It is not free: the owner's own probe
process, pid 72017, has held it since 16:06:37Z. The block is mechanical and
now established rather than assumed — `assert_release_self_test`
(`tests/end_to_end/composition_contract_tests.py:500-531`) runs the freshly
built `BUILD_TESTING=OFF` Release executable with `os.environ.copy()` and
requires a stdout match, and with the lease busy that executable prints only
`PROFILE_ACTIVATION_QUEUED` or `PROFILE_ALREADY_OPEN` and exits
(`src/app/main.cpp:8005-8021`). There is no environment variable for the data
root, and `--test-application-data-root` is compiled out of that build, being
inside `#if defined(BPR_BUILD_TESTING)` (`src/app/main.cpp:7261`,
`:7285-7291`) — which the test itself asserts the absence of. It was **not**
worked around by pointing `HOME` at a scratch directory: that would be a green
under a different environment than the criterion names. Pid 72017 was **not**
killed: it is the owner's process on his own desktop.

### The two asks for the owner, exactly

1. **Quit the BetterPullReview running from the probe terminal** (pid 72017).
   That is one command away from closing [[BPR-0744]]'s AC3 and this
   milestone's exit criterion 2:
   `ctest --preset dev --output-on-failure -R '^application_composition_contract$'`,
   about two to five minutes.
2. **Before quitting it, save that terminal's scrollback** to
   `/Users/jaeger/Documents/GitHub/BetterPullReview/build/logs/bpr-0746-cursor-probe.log`,
   or re-run the corrected command above with the gesture [[BPR-0746]] asks
   for: pointer onto the seam, **held still with only slight motion for about
   three seconds**, then off, then quit. Resting is the half that fails and is
   the variable under test.

Neither is urgent and neither blocks [[BPR-0743]].

### Attribution: the dispatch's premise was wrong, and no history was rewritten

d22 was told that `Co-Authored-By: Claude Opus 5 (1M context)` on `5846630e`
and `cdc00c79` was an anomaly against a repo that otherwise uses
`Claude Opus 5`. The census says otherwise: **533** commits carry
`(1M context)` and **100** carry the plain form, alongside 343 `Claude Fable
5.1`, 255 `Claude Fable 5` and 960 with no trailer. `(1M context)` is the
dominant Opus 5 form and goes back to at least 2026-09-03. d22 followed its
explicit instruction and used the plain form for its own commits, rewrote
nothing, and records the correction here so the owner can rule on which he
wants rather than on a false premise.

### Next, in order

1. **[[BPR-0743]]**, the epoch-identity design pass — Ready, P1, the largest
   piece in the pack and the only route to [[BPR-0646]]. Nothing in d22
   touched it and it is not blocked by either ask above. Take it in a fresh
   context: it is a design pass over `core::review_epoch_id`'s two
   machine-local parts, not a bounded edit.
2. `application_composition_contract` the moment the profile frees, then
   finish [[BPR-0744]].
3. [[BPR-0741]] and [[BPR-0742]].
4. No v010 packet was prepared and none is forced. The next one, when a card
   forces it, must fix the two defects `AGENTS.md` now names: absolute paths
   in every command, and one question per criterion. [[BPR-0746]]'s AC4 and
   [[BPR-0739]]'s AC3 both come back on a bundle carrying whatever the log
   leads to; [[BPR-0748]] needs two pictures; [[BPR-0646]] waits on BPR-0743.

Neither RFC became closable this round. Nothing touched RFC-BPR-0018, and
RFC-BPR-0028 still holds BPR-0743, BPR-0729, BPR-0653, BPR-0736, BPR-0739 and
BPR-0746. Nothing is pushed. d22 added eight commits on `main`, on top of the two
`cdc00c79` and `5846630e` it inherited unpushed.

d21's frontier, still authoritative on everything it is not corrected on
above, follows.

## Frontier, 2026-09-12 (d21, mandate r12): the v009 session ran

Dispatch `attended-prep-2026-09-10` d21 (mandate r12), same milestone
`attended-preparation-pack-head`. The owner asked to launch the attended
review. The host ran, he decided all four candidates in ten minutes and handed
back at `2026-09-12T16:13:55Z`, sequence 362, nothing pending. **The verdicts
are recorded and not consumed:** no card is finished, no acceptance entry is
written and nothing is interpreted here. That is the next dispatch's work. No
card was begun and no product source was touched.

### The four verdicts, verbatim

Every one is `actor: human`, carries no attachment, and its decision fingerprint
equals the prepared candidate's, so each is bound to exactly what was shown.

| Candidate | Revision | Outcome | Note |
|---|---|---|---|
| `BPR-0734-read-one-pane` | `design-v7-2026-09-12` | **accept** | *(empty)* |
| `BPR-0739-seam-hover` | `app-release-c2381f32-2026-09-12.5` | **select**: "Drop it: draw the rule at one constant weight, as the asset does" | "We don't need it anymore now that the cursor is fixed, it wasn't drawn in the right location anyways." |
| `BPR-0744-directive-ranges` | `app-release-c2381f32-2026-09-12.5` | **accept** | *(empty)* |
| `BPR-0746-cursor-probe` | `app-release-c2381f32-2026-09-12.5` | **accept** | "The pointer stuff works correctly. Dunno about the test you had me run." |

Decision ids, in that order: `31462e78-51ea-4ef5-ba85-231a18098b68`,
`9298ad8d-c3a2-4b4d-b58e-ba18a3daa3f5`,
`4f1d1354-c3c1-43d9-af11-358347147453`,
`3e06fb9d-e8d8-4abd-901e-b5dab8cc4d1c`.

### Three things the next dispatch must resolve before finishing any of them

**1. BPR-0746's log does not exist, and the accept cannot stand on its stated
criterion.** That criterion is "the bundle is run from a terminal with the
logging line, the pointer rests on the seam for about three seconds and moves
off, and `build/logs/bpr-0746-cursor-probe.log` exists". He did run the probe —
pid 72017 with `QT_LOGGING_RULES=qt.qpa.input.mouse*=true` confirmed on the
process — but he ran it from `/Users/jaeger/Documents/GitHub`, one directory
above the repository, and the candidate's copied command uses a **relative**
tee path. `/Users/jaeger/Documents/GitHub/build/logs` does not exist, so `tee`
(pid 72018) never opened an output file: `lsof` showed it holding only fds 1
and 2 on `/dev/ttys001`. No `bpr-0746-cursor-probe.log` exists anywhere on
disk. His own note, "Dunno about the test you had me run", is consistent with
that failure. **The objective half of the criterion was never satisfied**, and
his accept was recorded against a candidate whose evidence is missing. The
packet's own wording is at fault: prepare that command with an **absolute** tee
path next time, not a relative one behind a "run this in the repository" label.

**2. He states twice that the cursor works, which contradicts why BPR-0746 and
BPR-0739 exist.** "the cursor is fixed" on BPR-0739 and "The pointer stuff
works correctly" on BPR-0746. Nothing about cursor delivery changed between the
v008 and v009 bundles, so on the recorded state this should not be true. One
untested explanation, offered as a lead and **not** as a finding: no one had
ever run this bundle with a free profile and a real pointer, because the v008
build held the profile until he quit it this morning. That is a hypothesis, not
a diagnosis, and it is exactly what the missing log would have settled.

**3. BPR-0744's third question went unanswered.** The accept carries an empty
note, so the costed question the candidate rode — whether every identifier
inside `#if`, `#ifdef`, `#ifndef`, `#elif`, `#undef` and `defined()` should take
the macro purple, rather than only ALL_CAPS — is still open. An accept of the
implemented behaviour is not an answer to it.

**The renewal was bookkeeping, confirmed rather than assumed.** d20's
`coordinator_handoff.py check` returned reconcile because `BPR-0646:*` moved
under an unchanged milestone definition fingerprint. Read against `6b76f744`,
the whole change to that card is an appended "Owner answer" section plus one
link-path fix (BPR-0743 `10-backlog` → `20-ready`). Frontmatter, `status:
"Parked"`, `parked_on: human-review` and every entry of the `acceptance` array
are byte-identical, and the section says in its own words that the criterion is
**not reworded**. Exit criterion 4 of this milestone already admits "**or** the
exact remaining ask is recorded on the card", which is exactly what landed. No
requirement moved. Rebound at obligation fingerprint
`sha256:b18b2581a41bfffba81a70ac6fc84881c0de0e69bbf015e429e695682d6709a8`,
definition fingerprint `sha256:201de49a…`.

**The packet was verified intact before the host was started**, not taken on
trust: executable sha256 `c2381f32…` recomputed and equal, `codesign --verify
--deep --strict` exit 0, offline page digest `sha256:51da0dd4…` recomputed and
equal, and all four candidates `ready` + `protected` at sequence 357 with
fingerprints equal to
[session-identity.json](../../verification/reviews/rfc-bpr-0028-attended-v009/session-identity.json)
and no decision recorded against any of them. Each candidate's launch and copy
targets were resolved on disk: the design asset and its `read-one-pane` anchor,
the bundle, and a writable `build/logs` with
`build/logs/bpr-0746-cursor-probe.log` **not** yet present, so its existence is
real evidence when the probe runs.

**Host:** ran as pid 70931 on loopback `127.0.0.1:60308`, and was **stopped
through the contract** once he handed back. All four candidates keep their
protection of the Qt, application, SQLite, design and RFC paths, exactly as the
v008 set did, until an explicit `resume-development` precedes the next source
edit — and three of the four verdicts imply source edits, so that release is
the next dispatch's first move, not an afterthought. Reopen with `serve` if
anything needs showing again; the attach information comes back with it.

One practical note for whoever runs the next session: `wait` had to be re-armed
by hand after every single verdict, because a backgrounded shell command on
this harness is capped well below the length of a review. Arm it as a
persistent watch instead, or expect one wake per click.

**Correction to d20's environment note, which is now wrong.** The v008 build,
pid 30378, **is gone** — the owner quit it himself before this dispatch (the
profile directory was last touched 06:47; his `3afdeae7` landed 07:12). No
BetterPullReview process is running and nothing holds the profile. So the
packet's first instruction, and the "quit the running BetterPullReview first"
wording inside BPR-0739's and BPR-0744's launch labels, are **already
satisfied** — stale, but harmless, and not worth invalidating a protected
candidate's fingerprint to reword under his hands.

`application_composition_contract`'s
`test_tests_off_release_graph_omits_public_path_fixture` was **deliberately not
rerun,** and still has not been. It should go green now that the profile is
free, but `assert_release_self_test` launches the executable against the real
environment, so taking that evidence during the session would have opened the
profile and blocked his own launch of the v009 bundle. He may still have the
bundle open from the probe; check for a running process first, then run it. It
is one command and it closes exit criterion 2.

Next, in order. **Release the protections** with `resume-development` before any
source edit. Then consume the four verdicts against their own cards — BPR-0734
and BPR-0744 accepted, BPR-0739 selecting the constant-weight seam, BPR-0746
accepted on a criterion whose log never existed, which is the one that needs a
decision rather than a transcription: either re-take the probe with an absolute
tee path, or rewrite the criterion around what he actually observed. Then run
the composition contract, then [[BPR-0743]], still the largest piece of work in
the pack and the only route to BPR-0646. Neither RFC became closable this
round: nothing here touched RFC-BPR-0018, and RFC-BPR-0028 still holds
BPR-0743, BPR-0729, BPR-0653, BPR-0736 and the four cards these verdicts land
on. d20's frontier, still authoritative on everything it is not corrected on
above, follows.

## Frontier, 2026-09-12 (d20, mandate r11)

Dispatch `attended-prep-2026-09-10` d20 (mandate r11), milestone
`attended-preparation-pack-head` of plan `ux-redesign-pack-0028`. The owner
answered all six outstanding asks in chat. Five close a question; the sixth
reopens the epoch model and is the one that matters.

**The epoch answer.** Asked to choose between importing a pull request's
earlier updates and rewording [[BPR-0646]]'s handle criterion, he answered
neither: "An Epoch is an update to the PR, epochs identity should be stable and
repeatable if I look at the PR on different machines. A purely observed epoch
is not useful and terribly confusing." So the criterion is right as written and
is **not reworded**, and the fix is larger than a missing producer:
`core::review_epoch_id` is `{Authority, Incarnation, Ordinal}`
(`epoch_model.hpp:444-450`) and both variable parts are machine-local — a
random per-catalogue incarnation ([[BPR-0044]]) and an ordinal handed out by the
SQLite scope row under compare-and-swap (`epoch_catalog.cpp:3030-3045`,
`:3147-3156`) in observation order. [[BPR-0743]] is rewritten and promoted to
**Ready** (P1) with the derivation question, the ordinal's coupling points (git
retained refs encode it as sixteen hex digits, the ledger and window
restoration store it, the MCP context surfaces it, history paging is pinned on
it by [[BPR-0056]] and [[BPR-0066]]; the amber last-review tick is *not* on that
list, it matches by head OID), the migration-by-rebuild finding, and the
signed-out problem that every GraphQL-routed operation is refused anonymously
(`github_anonymous_read_admission.hpp:99-108`) so a timeline read has to be
REST. **Correction to d19's diagnosis:** `list_pull_request_commits` does exist
end to end and is admitted anonymously, but a force push rewrites a commit list
rather than appending to it, so commits cannot reconstruct the updates that
matter. RFC-BPR-0003 records the direction without reopening its baseline; if
the design pass finds the change exceeds what an owner direction on a closed
RFC can carry, it comes back for the owner rather than reopening it from inside
a card. **BPR-0646 is not in the v009 packet** and is not asked again on any
bundle until the line carries the pull request's own updates.

**The other five.** Block order is unified-diff order ("don't care"); the
one-pane choice lives in the View menu "like line numbers", which was read
against the code to mean per window, seeded from a profile default, with no
modality of its own, **so Review takes one pane too** — the opposite of the
position BPR-0734 drew, offered with its flip. The alias italic does not matter
("no") and `CE8AD4` stands as the macro purple ("yes it's acceptable"), so
RFC-BPR-0006's floor holds and neither is repeated. The seam's hover highlight
is **not settled**: "I'm not sure what this is, but leaning towards keep it" is
a leaning and its first clause is the operative one, so nothing changed and
v009 shows him the element in a picture instead of naming it again.

Landed on main: the answers and the redraw (`6b76f744`); BPR-0746's record
(`c0d3c5da`); [[BPR-0745]] finished human-pending (`16eaefb4`) with
`docs/design/RFC-BPR-0028-workbench-direction-v7.html`, state 2b redrawn so the
gutter carries the link map at its limit — each change's struck and added rows
converge on a spine, an empty side draws one ribbon, the whole-file overview
and the strip toggle are gone and a View menu is drawn instead; and
[[BPR-0744]] finished human-pending (`aff9fc64`) with a directive's arguments
taking their own roles. [[BPR-0747]] carded from that slice (P3, Backlog): a
`//` comment after a null directive paints `syntax_error` salmon, upstream
grammar behaviour, identical count at HEAD.

**BPR-0744's one non-obvious invariant, so it is not undone:** the
`syntax_other` → `syntax_macro` substitution is gated on `is_macro_case`, and
that condition is load-bearing. `dsOthers` has one user in `isocpp.xml` and
none in `c.xml`, but the format stream also carries `dsOthers` from `gcc.xml`,
`doxygen.xml` and `modelines.xml` through includes, so a blanket substitution
paints `&amp;` inside a Doxygen comment and `on`/`off` inside a kate modeline the
macro purple. Probed, not reasoned. The card carries the evidence and the two
recorded limits (`__clang__` grey, `__cplusplus` near-grey), both pinned by the
contract so they cannot drift, and the costed context-aware rule that would
remove them, which is asked in v009 rather than taken.

**BPR-0746 made no product change, deliberately.** Its three hypotheses differ
only in what AppKit does on the next real pointer motion after Qt's enter, and
nothing separates them without a real pointer on the seam. Synthesising that
motion is forbidden here and is exactly what made
`build/logs/bpr-0739-cocoa-diag-1.log` worthless. The check is prepared rather
than deferred: one terminal line with `QT_LOGGING_RULES`, riding its own v009
candidate so one trip to the seam yields both the log and the hover verdict.
The category and the four literals were read off the shipped
`libqcocoa.dylib`.

Bundle and packet. The tests-disabled signed bundle is rebuilt at `aff9fc64`
(executable sha256 `c2381f32…`, `build/logs/app-release-aff9fc64.log`,
`codesign --verify --deep --strict` exit 0, `BUILD_TESTING=OFF`). **No store
moves schema this round**, so there is no second palette reset. The v009 packet
is prepared under
[rfc-bpr-0028-attended-v009](../../verification/reviews/rfc-bpr-0028-attended-v009/operator-packet.md):
four candidates ready and protected at **sequence 357**, revision
`app-release-c2381f32-2026-09-12.5` (`BPR-0734-read-one-pane` carries
`design-v7-2026-09-12`) — `BPR-0744-directive-ranges` (carrying BPR-0720's and
BPR-0738's shared criterion as one ask rather than two),
`BPR-0734-read-one-pane`, `BPR-0739-seam-hover` and `BPR-0746-cursor-probe`. No
host started, no wait armed: `attended_review.py serve`, then `wait --after 357`.
The v008 candidates were released with `resume-development` at sequence 349
before any source edit; their verdicts stay bound to their own revisions.

**Environment, not a regression.** `application_composition_contract`'s subtest
`test_tests_off_release_graph_omits_public_path_fixture` fails
`PROFILE_ALREADY_OPEN`. Verified at HEAD by restoring HEAD content over all six
changed files: identical failure. `assert_release_self_test`
(`composition_contract_tests.py:508`) launches the executable with the real
environment so it opens the real profile, and `src/app/main.cpp:8019` prints
that for `owner_busy`; the owner's v008 bundle has been running since 21:18
(pid 30378) and holds it. It was not killed. Everything else in the
integration-focused pass is green: 25 of 26, including
`qt_composed_github_scale_100`, `qt_workbench_window_scale_{100,125,200}`,
`qt_settings_window*`, `qt_diff_workbench_scale_*`, `qt_appearance_*`,
`syntax_classification_contract`, `architecture_boundaries` and
`application_review_round_trip_composition_contract`. The documentation gate
and `git diff --check` are clean. The check goes green when he quits that
build, which is the packet's first instruction.

Closeout state: unchanged, both RFCs stay open. **RFC-BPR-0018 is not
closable**: 17 of 20 cards Done, this card still waits on [[BPR-0730]] and
[[BPR-0731]] (the Windows and Linux packaged inventories for AC-11, on their own
stations), and the checklist below still holds AC-01, AC-07, AC-08 and AC-11
open. Nothing in d20 touched RFC-BPR-0018. **RFC-BPR-0028 is not closable**: 41
of 51 Done, with BPR-0646, BPR-0734, BPR-0739, BPR-0745 and BPR-0746 parked on
human-review, BPR-0743 Ready and carrying the epoch work, BPR-0729 waiting on
BPR-0734, BPR-0653 on the Windows station and BPR-0736 in Inbox.

Not reached this dispatch, unchanged: [[BPR-0741]], [[BPR-0742]], [[BPR-0735]]
(clangd), [[BPR-0736]], [[BPR-0729]]. Next, in order: run the v009 session and
consume its four verdicts, including the cursor probe's log, which is what
unblocks BPR-0746's diagnosis; then [[BPR-0743]], the largest piece of work in
the pack and the only route to BPR-0646; then BPR-0741 and BPR-0742. d19's
frontier follows.

## Frontier, 2026-09-11 (d19, mandate r10)

Dispatch `attended-prep-2026-09-10` d19 (mandate r10). The owner ran the
v008 session and handed back at sequence 348 with all six candidates
decided; every decision matches its candidate's fingerprint in
[the v008 identity file](../../verification/reviews/rfc-bpr-0028-attended-v008/session-identity.json),
so no verdict is stale. His four screenshots are in that packet's `captures`
directory (the fifth attachment was the v007 Visual Studio reference
re-attached byte-identical). Landed on main: [[BPR-0737]] finished
(`24e1fef8`), the epoch row accepted; the five rejections recorded on their
own cards with the four follow-ups they become (`d0ae88fa`); RFC-BPR-0028
and RFC-BPR-0006 updated (`9a63931c`); IDs BPR-0743..BPR-0746 reserved
(`d960f945`). No product source changed in this dispatch, so the v008
bundle and both composition digests still stand at `35438244` /
`e4fdb0ea…`.

What the rejections mean, one line each. **BPR-0646**: the handle criterion
is unjudgeable on any pull request's first open, because a review epoch is
this install's own observation and nothing enumerates a pull request's
earlier updates — the local catalogue held one epoch for NVIDIA/stdexec#1846,
minted five minutes before the verdict. [[BPR-0743]] (Backlog) holds the
owner's decision between importing history under RFC-BPR-0003 and rewording
the criterion; **this is the ask that has to reach the owner before another
bundle is prepared for BPR-0646.** **BPR-0720 and BPR-0738**: one
requirement in two notes, a directive line paints grey end to end where the
reference greys only the directive; [[BPR-0744]] (Ready). **BPR-0734**: the
one-pane map is the link map at its limit, not a new element; the RFC's
non-goal is relaxed and [[BPR-0745]] (Ready) redraws the state. **BPR-0739**:
the seam is accepted, the cursor is not, and the lit highlight refutes the
d17 cause — the defect is cursor delivery on macOS after Qt's enter;
[[BPR-0746]] (Ready) carries the hypotheses and the discriminating check.
BPR-0739's AC1 evidence is corrected there and on its own card: the cocoa
probe log it cited has the pointer 1164 pixels from the handle and prints
`FAILED`.

Four owner answers are still outstanding and none was synthesised: BPR-0734's
block order and where the one-pane choice lives; BPR-0738's alias italic and
the `CE8AD4`-for-`BD63C5` macro purple ("most things are good now" names
neither); BPR-0739's hover highlight; and BPR-0743's import-or-reword
question.

Closeout state: unchanged by this round and both RFCs stay open.
**RFC-BPR-0018 is not closable**: 17 of its 20 cards are Done, this card
waits on [[BPR-0730]] and [[BPR-0731]] (the Windows and Linux packaged
inventories for AC-11, deferred to their stations, "after we sort all the
MacOS and platform agnostic stuff out"), and the checklist below still holds
AC-01, AC-07, AC-08 and AC-11 open for target-native, screen-reader and
packaged evidence. Nothing in the v008 round touched RFC-BPR-0018;
[[BPR-0683]] remains its only attended criterion and it was accepted back in
v006. **RFC-BPR-0028 is not closable**: 40 of 50 Done, with BPR-0646,
BPR-0734 and BPR-0739 parked on human-review, BPR-0729 waiting on BPR-0734,
BPR-0743 in Backlog behind the owner, BPR-0653 on the Windows station,
BPR-0736 in Inbox, and BPR-0745 and BPR-0746 Ready. BPR-0646 is the one that
changed character: its criterion can no longer be closed by preparing another
bundle.

The v008 candidates keep their protection of `src/adapters/qt`,
`src/application`, `src/adapters/sqlite` and the design and RFC paths until
an explicit `resume-development` precedes the next source edit; this
dispatch made no source edit and released nothing. The host (pid 29635) was
stopped through the contract after the decisions were consumed. Next, in
order: [[BPR-0744]], [[BPR-0745]] and [[BPR-0746]] on main (all three need
`resume-development` first, since all three touch protected paths or design
assets); then [[BPR-0741]], [[BPR-0742]]; then a rebuilt bundle and a v009
packet re-asking BPR-0720, BPR-0734, BPR-0738 and BPR-0739 with the four
open questions attached. BPR-0646 is not in that packet until BPR-0743 has
its answer. Outside the pack: [[BPR-0735]] (clangd), Ready. d18's frontier
follows.

## Frontier, 2026-09-11 (d18, mandate r9)

Dispatch `attended-prep-2026-09-10` d18 (mandate r9). Landed on main:
[[BPR-0740]] finished (`35438244`): finding 2 settled by instrumentation
as course composition, not a product regression (window B's landing
sign-in enrolled a second grant whose identity change retired every
window's session; A had read anonymously because `PublicPreviewSignedOut`
is false before the first paste activates the signed-out session); the
course signs in the first window of each phase before its first paste,
and its broken phase now pins BPR-0718's composed-defaults fallback for an
unreadable layout store, the lost user-visible report carded as
[[BPR-0742]] (Ready, P3). No product change; `github_application.cpp` and
both digests unchanged since the v007 bundle head `2cdc0e57`. The
tests-disabled signed bundle rebuilt at `35438244` (executable sha256
`e4fdb0ea…`, `build/logs/app-release-35438244.log`, codesign clean) and
the v008 packet prepared under
`docs/verification/reviews/rfc-bpr-0028-attended-v008/` (six candidates
at revision `app-release-e4fdb0ea-2026-09-11.4`, ready and protected at
sequence 341: BPR-0646 and BPR-0720 re-asked on NVIDIA/stdexec#1846,
BPR-0734, BPR-0737, BPR-0738, BPR-0739). No host started, no wait armed:
`attended_review.py serve`, then `wait --after 341`. Next after the
verdicts: [[BPR-0741]] (the multi-window course; its `:236` holds on the
account path once the course signs in first), BPR-0729 from the accepted
one-pane state, BPR-0742 when a slot allows. d17's frontier follows.

## Frontier, 2026-09-11 (d17, mandate r9)

Dispatch `attended-prep-2026-09-10` d17 (mandate r9). Landed on main:
[[BPR-0739]] diagnosed and fixed (causes 1, 2 and 4 excluded by a Qt
test that passed at HEAD and by a cocoa probe; the cause is the invisible
seven-pixel handle beside the comparison's scroll bar, so the central
splitter handles now draw the asset's one-pixel drawer edge rule in `Mid`,
`Highlight` under the pointer; parked human-review, asked in v008);
the harness-regenerated `native-roles.lock.json` recorded (claude role
hashes match the installed `.claude/agents` files); [[BPR-0741]] carded
(the multi-window course red at HEAD on macOS, same cause as BPR-0740).
[[BPR-0740]] is In Progress with its course converted to the account path
(the stale sign-in stop was BPR-0732's; confirmed) and one open finding:
after window B opens pull 18, window A's review is closed
(`invalidate_now` in the workbench host); course composition or a
product regression in the window-session lifecycle, undetermined, with
the deciding instrumentation named on the card. The course is committed
in that state and fails at "last accepted layout change belongs only to
A" with A's status in the failure output. Not done: the tests-disabled
bundle and the v008 packet, held because BPR-0740 may still change
`github_application.cpp`; a manifest generator and packet draft for v008
are in d17's return (six candidates: BPR-0646 and BPR-0720 re-asked, and
BPR-0734, BPR-0737, BPR-0738, BPR-0739, on NVIDIA/stdexec#1846; the run
notes say the appearance store moves to schema 5 and older builds refuse
it). No host started, no wait armed. Next: settle BPR-0740's finding 2
(instrument `github_window_read_host::retire`/`release` and
`WorkbenchInvalidations` at the moment B opens), or park it to a named
blocker so the bundle can be built; then the bundle, then the packet.
d16's frontier follows.

## Frontier, 2026-09-11 (d16, mandate r9)

Dispatch `attended-prep-2026-09-10` d16 (mandate r9). The owner answered
d15's two asks: NVIDIA/stdexec#1846 is the multi-update pull request for
the BPR-0646 and BPR-0737 asks (recorded on both cards), and BPR-0738's
reading is corrected, functions orange `FF8000`, class/struct types gold
`FFD700`, macros purple `BD63C5` with a distinct macro role "necessary"
(per-token resample on BPR-0738; BPR-0720 and RFC-BPR-0006 point at it).
Landed in d16, all on main: [[BPR-0734]]'s round (workbench states v6,
state 2b "Read, one pane", the drafted Modalities and AC-05 amendments and
three open questions on the card; parked human-review); the ten v007
candidates released through `resume-development` (sequence 323, no
candidate protected); [[BPR-0738]] (a `syntax_macro` role, the dark
values types `FFD700`, functions `FF8000`, variables `BDB76B`, attributes
`4EC9B0`, macros `CE8AD4`, the appearance store at schema 5 with no second
reset; parked human-review); [[BPR-0737]] (the epoch row is the review
surface's first row above the splitter, full client width in every
modality; parked human-review; RFC-BPR-0028's chrome paragraph amended).
Found red at HEAD and not any card's: the three round-trip composition
pins that BPR-0733 left stale (repinned under BPR-0738's commit) and
`application_profile_window_layout_public_path` ([[BPR-0740]], Ready).
Still to do, in order: [[BPR-0739]] (the resize cursor; diagnosis first);
the rebuilt tests-disabled bundle; the v008 packet written last, on
NVIDIA/stdexec#1846, re-asking BPR-0646 and BPR-0720 and asking BPR-0734,
BPR-0737, BPR-0738 and BPR-0739 (BPR-0738's ask also carries the include
path colour and the alias italic; BPR-0734's carries its three
questions). No host is started and no wait armed. d15's frontier follows.

Dispatch `attended-prep-2026-09-10` d15 (mandate r8). The owner reviewed
all ten v007 candidates on the 2cdc0e57 bundle (executable sha256
`64d9e46a…`, revision `app-release-64d9e46a-2026-09-11.3`) and handed back
at sequence 322; every decision was consumed against its own candidate and
fingerprint (all ten match the identity file of
[rfc-bpr-0028-attended-v007](../../verification/reviews/rfc-bpr-0028-attended-v007/operator-packet.md);
no stale verdict).
Accepted and Done: [[BPR-0663]], [[BPR-0676]], [[BPR-0725]], [[BPR-0726]],
[[BPR-0727]] (with d11's judgement), [[BPR-0728]] (its note is a new
defect, [[BPR-0739]]), [[BPR-0732]] and [[BPR-0733]] (with d13's change);
[[BPR-0716]] finished on BPR-0732's verdict. Rejected and still parked:
[[BPR-0646]] (the epoch line's placement in the centre pane, [[BPR-0737]];
the handle criterion needs a pull request with several updates) and
[[BPR-0720]] (macro and function colours against the Visual Assist
reference, [[BPR-0738]]). The owner's five screenshots are in the v007
packet's `captures` directory. The host (pid 55182) was stopped through
the contract; the ten candidates keep their protection of `src/core`,
`src/application`, `src/adapters/qt`, `src/adapters/sqlite`,
`src/adapters/syntax` and `src/app` until an explicit
`resume-development` precedes the next source edit.

Closeout state: RFC-BPR-0018 is not closable; this card still depends on
[[BPR-0730]] and [[BPR-0731]] (Windows and Linux packaged inventories,
"after we sort all the MacOS and platform agnostic stuff out"). RFC-BPR-0028
is not closable: BPR-0646 keeps one criterion parked, BPR-0729 waits on
[[BPR-0734]], BPR-0653 waits for the Windows station, and BPR-0737 and
BPR-0739 are new Ready work. Next, in order: [[BPR-0734]] (design only, no
product source, needs no release); then `resume-development`, [[BPR-0737]]
and [[BPR-0739]] under RFC-BPR-0028 and [[BPR-0738]] under RFC-BPR-0006 on
main; a rebuilt tests-disabled bundle; a v008 packet re-asking BPR-0646
(on a public pull request with more than one update) and BPR-0720 plus the
three new asks. Outside the pack: [[BPR-0735]] (clangd noise), Ready, not
started. Detail below under "Continuation after the v007 hand-back".

## Objective

Reconcile the complete accepted behavior in [RFC-BPR-0018](../../rfcs/RFC-BPR-0018-robust-settings-center.md)
against direct evidence, all linked issue cards, and the final public path.
This closeout depends on the entry card now and must acquire all subsequently
allocated required slice dependencies before it can become startable.

RFC base: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

Requested RFC set: RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025, RFC-BPR-0026; workstream position: 1 of 4.
Planned cards: BPR-0592, BPR-0593. Allocate additional implementation cards only
when the later bounded-slice planning pass needs them, and update this list and
closeout dependencies before implementation. The RFC index remains the order
authority; this requested subset waives no intervening eligibility/closeout gate.

The 2026-09-04 registration was subsequently activated by the owner's
2026-09-07 implementation direction in the canonical index. This closeout stays
in Backlog until its required acceptance is complete. The immutable base remains
the pre-registration commit; registration commits are planning history.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01: The shipped application exposes a native Settings action and one singleton, modeless window per available BPR profile through platform-appropriate menus and shortcuts, with the edited profile visibly identified. | `qt_settings_window` menu, singleton, Default label and close course | Automated Default environment passes; target-native inventory pending | 68e0a40f, a881d9f9 |
| AC-02: All seven initial categories are present when their capabilities are composed, with stable sidebar/page/row identities and truthful unavailable states. | Seven-page composed matrix and agent/storage offered-model checks | Automated pass; unsupported capabilities remain explained | 51c24f6b, 6c0ba02b |
| AC-03: Appearance is migrated from the embedded workbench surface to the settings center with no duplicate authority; existing mode, typography, palette, import/preview/commit/cancel/reset, and persistence behavior remains intact. | Appearance host/store/Qt and application theme customization course | Pass, including export, scoped reset and actual Settings close | 03a8a02e, a881d9f9 |
| AC-04: Every setting gesture becomes one typed domain intent, reaches only its owning host, and updates the control only from an accepted authoritative projection. | Domain host refusal/late-result checks; queue field merge and composed intent checks | Pass on changed settings paths; medium review found no additional state defect | dd64c098, 51c24f6b, a881d9f9 |
| AC-05: Existing domain stores migrate and reopen from prior schemas; future, malformed, interrupted, and unavailable cases fail at their exact scope without corrupting another domain. | SQLite appearance v1/v2 and queue v5 migrations/reopen; malformed/future checks | Canonical migrations and pre-write malformed/future refusal pass | 03a8a02e, dd64c098, 22e30024 |
| AC-06: Credential, privacy, cache, notification, and agent actions retain their existing capability and secrecy boundaries. | Device flow, metadata-only notification preview, offered private opt-ins and exact maintenance course | Pass on fixture public paths; no live credentials/provider mutations | 01a3ada4, dd64c098, 51c24f6b |
| AC-07: Keyboard-only and screen-reader courses reach every sidebar item, row control, reset, confirmation, status, and close action in a natural order. | Qt keyboard, focus-return, page scroll, reset and Cancel courses | Automated checks pass; native screen-reader course pending | 68e0a40f, a881d9f9 |
| AC-08: Layout and rendered evidence pass at 100%, 125%, 150%, and 200% scale in Light, Dark, High-contrast Light, and High-contrast Dark, including long text and minimum window size. | `qt_settings_window` plus scale 125/150/200, each four appearances and seven pages; 112 captures | Automated layout pass at 760x480 with long metadata; attended visual verdict pending | 6c0ba02b |
| AC-09: One-setting updates preserve sidebar selection, page scroll position, focus, unaffected persistent indexes, and all unrelated projections without routine resets. | Stable page/scroll/focus and incremental workbench projections | Pass for one-setting changes and Settings reopen | 68e0a40f, dd64c098, a881d9f9 |
| AC-10: Hostile catalog, profile, rule, and row floods stop at declared limits with typed refusal; one window materializes at most one page per category; rapid previews coalesce; cancelled or superseded loads publish nothing late; and instrumentation observes zero database, vault, filesystem, network, process, or blocking-wait work on the Qt thread. | Appearance host cancellation/cache/coalescing, one page per category, architecture boundary checks | Collection limits, stable models, coalesced previews and cancellation pass; focused Settings capability-port instrumentation passes; exhaustive OS/native instrumentation remains unproven | 143abbc9, e354b45e, 29bb2980, a881d9f9, 6c0ba02b |
| AC-11: Tests-disabled packaged applications on macOS, Windows, and Linux expose the same setting inventory except for explicitly documented platform capability rows. | Tests-disabled target-native packaged inventory | Pending macOS/Windows/Linux current-candidate inventory; old isolated build is not this criterion | None |
| AC-12: Sign in starts supported device authorization directly and shows its code and browser action without Details, another Get code gesture, or pane scrolling; cancellation, expiry, and supported manual-token alternatives remain reachable. | Application device panel course from Home, GHES/absent refusal and Qt duplicate/cancel/expiry | Pass; supported flow starts directly, manual/session-only alternatives retained | 01a3ada4 |
| AC-13: User-configured servers and account selections survive restart through typed host persistence, while remembered grants reuse the existing vault path and session-only manual credentials are never presented as durable. | Saved server typed store/restart and existing device vault binding cases | Existing BPR-0660 and BPR-0679 evidence retained; manual tokens stay session-only | 01a3ada4 |
| AC-14: Configuration leaves the primary review surface compact, preserves tuned diff/appearance behavior, and exposes direct reconnect plus an exact pending destination handoff without changing credential or provider authority. | Compact workbench, composed GitHub, theme and direct reconnect paths | Pass: visible unknown-server Add/sign-in preserves exact target; cancel/stale stages preserve prior review, unavailable locations are explicit | 554564f8, 01a3ada4, a881d9f9 |

## Completion rules

- Replace planned checks with exact executed commands/courses, results, and
  producing commits as implementation completes; retain not-run rows honestly.
- Inspect all cards linked to this RFC, not just the initial planned pair.
  Every required card must be done and every canonical criterion satisfied.
- Exercise the selected public path and check critical changed-surface findings.
  Use focused evidence; do not infer a full matrix requirement from this card.
- Record target-native or attended evidence in the project's existing workflow.
  Park pending attended work and continue independent work; no approval or live
  GitHub authority comes from registration.
- Run the documentation structure gate and `git diff --check`, reconcile RFC
  and index status, and close only when canonical order and all dependencies
  permit Auto-Verified. No status changes are authorized by this registration.

## Resume

The owner activated the broader UX work. BPR-0678 through BPR-0682, BPR-0684
through BPR-0688 are implemented. BPR-0689 retains exact setup destinations
and stages authentication without disturbing an existing review. BPR-0683 is
parked for required native/attended evidence, with its automated passes retained.
Continue RFC-0024 planning after the automated Settings integration boundary,
while preserving pending target-native/human criteria and canonical closeout order.
The current authorized workstream continuation is retained on [[BPR-0595]].

## Resumed implementation, 2026-09-07

The owner activated the broader UX frontier. [The authority inventory](../../development/settings-authority-inventory.md)
reconciles the integrated 0028 work and maps every criterion to BPR-0678 through
BPR-0683. These join BPR-0592 and BPR-0593 as the planned cards. Existing
partial implementation is not full criterion acceptance; not-run rows above
are superseded by the criterion ledger as each selected path is exercised.

BPR-0678: default-profile shell now uses stable per-page scroll containers,
retains panel identity and category scroll, identifies its default profile and
handles unconsumed Escape. Composed Qt checks pass at 100/125/200%; actual
760x480 offscreen Accounts/Reviews/Appearance captures inspected. This is
partial AC-01/02/07/08/09/10 evidence, not a full platform/accessibility verdict.
Two affected attended candidates (chrome and widths) were released by the
owner's resume-development authorization and need preparation after integration;
public-preview was subsequently released for BPR-0679 and also needs preparation.

BPR-0679: Home and the signed-out account action now start the existing device
flow directly. The executable panel course exercises Home, and the GHES
negative retains its no-request refusal. Running-flow duplicate, cancellation,
expiry and selected-host retry checks pass. Saved server authority/store and
session-only manual-token evidence from BPR-0660 remain unchanged. The
isolated tests-disabled build passed; one digest assertion was updated for
the reviewed course change and its bypass test passed separately. See the
card for commands and revision. Next: BPR-0680 appearance export/reset.

BPR-0684 repairs the Settings-close preview/import lifecycle discovered during integration preparation.

## Current integration evidence

At `3d36ce70` plus bookkeeping, both architecture checks and
`qt_composed_github_scale_100` passed (116.96s total). The actual application and
composed Qt target built successfully. Reused unchanged evidence: full core-only
125/125 at BPR-0681, selected domain host/store checks, device public course,
queue restart/pause course, theme customization 39.16s and maintenance 66.12s.
BPR-0685 adds all four scales/appearances with zero horizontal page scrolling;
112 deterministic offscreen rasters were generated and representative corrected
surfaces inspected. This does not stand in for screen-reader or target-native
packaged-app evidence.

One independent review was initially dispatched with the stale native role's
Astra high setting. It was interrupted on the owner's correction, and the same
bounded review continued with explicit Astra medium routing. The finding was
preserved: appearance legacy migration must validate before writing (BPR-0686).
The medium review reported no additional material finding in queue preference
merges, scoped resets, exports or close/import cancellation. No second review
loop is required; the repair receives its direct regression checks.

BPR-0686 fixed the migration finding (22e30024); malformed and future legacy
stores are refused before mutation. BPR-0687 bounds queue collections and keeps
accepted models and removal controls usable after refusal (e354b45e). Host/store
checks and Qt 100/125/200 passed, including maximum admitted collections.
BPR-0688 coalesces a burst to the latest preview and invalidates queued work on
close/reset (29bb2980); the panel and actual theme course passed. Documentation
verifier regression suite: 62 tests passed during this integration.

## Attended preparation, 2026-09-10

BPR-0683's three remaining macOS asks (AC-07 VoiceOver order, AC-08 the four
appearances at the real window, AC-11 the tests-disabled packaged inventory)
are one candidate in
[rfc-bpr-0028-attended-v002](../../verification/reviews/rfc-bpr-0028-attended-v002/operator-packet.md),
prepared under the adopted pack plan `ux-redesign-pack-0028`. Windows and
Linux packaged inventory stay pending on their stations. This closeout
remains in Backlog until BPR-0683 is Done.

## Attended preparation, 2026-09-10 (v003)

Prepared again, unchanged in substance, in
[rfc-bpr-0028-attended-v003](../../verification/reviews/rfc-bpr-0028-attended-v003/operator-packet.md)
on the bundle built at the BPR-0715 fix head (executable sha256
`9334b13a…`); the v002 packet is historical.

## Continuation after the v003 hand-back, 2026-09-10

Session `cbd48b91…` was handed back at sequence 141 with one decision, the
rejection of `BPR-0646-workbench-chrome` (decision `65f3f881`; `b1348a9e`
withdrawn and superseded). It is consumed on
[BPR-0646](BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md)
and on RFC-BPR-0028, and became four cards: BPR-0719 (comparison work limit
refuses a file), BPR-0720 (dark palette comment and include colours, held
for the owner's confirmation), BPR-0721 (setting to hide the drawer controls
and the stray heading row above each rail) and BPR-0722 (identity hidden in
the strip below 1,400 pixels). `BPR-0683-settings-native-macos`, this
closeout's own candidate, and the eight other candidates carry no decision;
they stay ready and protected in the session, and their bundle inputs
(`src/adapters/qt`, `src/app`, the app-release bundle) stay guarded until
the owner says to resume development. Observations without verdicts are
recorded on BPR-0663 (no acquire stage in the restored run) and BPR-0715
(files and epoch line reached; log half pending).

Next, in order: ask the owner the three open questions (what was wrong in
Read mode; the extent of the BPR-0720 palette change; whether to resume
development now, releasing the nine undecided candidates for a rebuilt
bundle, or to review them on the current bundle first). Then, once released,
BPR-0719, BPR-0722 and BPR-0721 in that order as one milestone, a fresh
tests-disabled bundle, and a v004 packet regenerated last with the same ten
candidates plus the BPR-0719/0721/0722 asks; the packet is written only
after no active card can rebuild the binary.

## Continuation after the v003 follow-ups, first milestone, 2026-09-10

Dispatch `attended-prep-2026-09-10` d5 (mandate r2, "stop the attended
review session and fix the issues raised"). Session `cbd48b91…` was
stopped through the contract: `resume-development --all` released the
ten candidates (the nine undecided ones are `waiting`, unprotected, and
still pending on their cards; the BPR-0646 rejection `65f3f881` stays
recorded) and `stop` ended the loopback host (pid 24325). No watch is
armed.

Landed on main, in the recommended order:

- [BPR-0719](../60-done/BPR-0719-a-large-file-with-a-small-change-is-refused-by-the-comparison-work-limit.md)
  (`901a9c06`): the core sheds the common prefix and suffix, charges the
  exact matrix for the changed region only, and degrades through the
  ordered-unique alignment and patience anchors instead of refusing;
  `computed_file_diff::AlignmentApproximate` names a degraded alignment
  and the Qt status line says "(approximate alignment)". Parked for the
  owner's check on ScreenBufferTests.cpp in microsoft/terminal#20607.
- [BPR-0722](../60-done/BPR-0722-the-strip-hides-the-pull-request-identity-below-1400-pixels.md)
  (`035b5322`): the strip identity yields title, owner, modality
  shortcuts and positions before its number, elided to the room the
  other items leave; the compact modality holds to 1100. Measured fact
  the card did not have: the floor is 900, not 860, and at 100% scale the
  other strip items alone lay out about 900 pixels, so below about 960
  the number has no room and the identity yields whole. Parked for the
  owner's check at the default size and at 1280x820.
- [BPR-0723](../60-done/BPR-0723-the-uiforetw-selected-diff-contract-fails-at-startup-on-head.md)
  (`6de81ee8`): `application_uiforetw_selected_diff_contract` prints
  `STARTUP_FAILED` 0.5 s into its first capture at `8ff3b664`,
  reproduced on a clean worktree build of that head. Not attributed to
  the follow-ups; it blocks re-running the 1280 compact-row and rail
  assertions that BPR-0722 and BPR-0721 touch.

Not started: [BPR-0721](../60-done/BPR-0721-a-setting-hides-the-drawer-side-controls-and-a-collapsed-rail-shows-only-its-rail.md).
Scouted facts for it: the heading row is a nested `QHBoxLayout`
(`workbench_drawer.cpp:399-419`) that `adopt()` (`:501-534`) never hides,
so `Heading` and `Collapse` must be toggled explicitly; the settings store
is already schema version 5 (`workbench_settings_database.hpp:31`), so the
flag is a version 6 column; a profile-level flag fits
`review_behaviour_settings` (`workbench_runtime_ports.hpp:478-488`) and
rides `combined_layout` unchanged, with the hand-built literal at
`src/app/main.cpp:6452` to extend; the Reviews & queue page is built by
`build_behaviour_editor` (`workbench_window.cpp:3614-3740`); the drawer
shortcuts are Alt+Shift+1/2/3, not ⌘1-4 as the card says; the UIforETW
contract asserts the 18-pixel rails, so a zero-width collapsed drawer by
default amends that contract as well as RFC-BPR-0028 AC-03 (`:86`,
`:90-91`, `:129-130`, `:168-169`, `:290-293`).

Next, in order: BPR-0721 as one milestone (defect first, then the setting
through store, projection, window and Settings page, then the RFC and
contract amendments); a fresh tests-disabled bundle; the v004 packet
regenerated last with the same ten candidates plus the BPR-0719/0721/0722
asks, written only after no active card can rebuild the binary. Owner
questions still open and carried forward verbatim: what specifically was
wrong in Read mode ("Even in read mode viewing experience is not great");
for BPR-0720, whether the whole dark syntax palette moves to the Visual
Assist scheme or only comments and includes, and whether the saved custom
palette is reset or left; and, new from BPR-0722, which strip control
should give way below about 960 pixels so the identity's number can stay
to the floor, or whether the floor moves to 960.

## Continuation after the v003 follow-ups, second milestone, 2026-09-10

Dispatch `attended-prep-2026-09-10` d6 (mandate r2). Landed on main:
[BPR-0721](../60-done/BPR-0721-a-setting-hides-the-drawer-side-controls-and-a-collapsed-rail-shows-only-its-rail.md):
the heading row no longer stays above a rail, and
`review_behaviour_settings::DrawerControls` (hidden by default, the owner's
direction) removes the rails and Hide buttons so the strip toggles are the
drawers' only controls; schema version 6; RFC-BPR-0028 AC-03 amended. Parked
for the owner's acceptance of both states. Measured facts: a panel that
leaves and rejoins a `QSplitter` is granted the room it last had regardless
of its fixed width, so the window re-applies `central_sizes()` (which now
works before a layout is published); BPR-0675's attended criterion now
needs the setting on. `application_workbench_composition_contract` stays
red at head (BPR-0718) and `application_uiforetw_selected_diff_contract`
was not rerun (BPR-0723); neither is attributed to this milestone.

Next: a fresh tests-disabled bundle and the v004 packet with the same ten
candidates plus the BPR-0719/0721/0722 asks, written only after no active
card can rebuild the binary. The three owner questions stand as recorded
above.

## Attended preparation, 2026-09-10 (v004)

The bundle was rebuilt at 3c9a3f1c (`cmake --preset app-release`, tests
disabled, executable sha256 `9261509a…`, signature verified) after
BPR-0721 was parked, so no active card could rebuild it. The session
`cbd48b91…` carries the ten v003 candidates at revision
`app-release-9261509a-2026-09-10.3` plus `BPR-0719-large-file-diff`,
`BPR-0721-drawer-controls` and `BPR-0722-strip-identity`: thirteen ready
and protected at sequence 182, none decided, in
[rfc-bpr-0028-attended-v004](../../verification/reviews/rfc-bpr-0028-attended-v004/operator-packet.md).
No host is running and no watch is armed; the packet says how to start
both. BPR-0683's candidate is this closeout's, unchanged in substance.

Next: on the owner's say-so, `serve` and a persistent `wait --after 182`;
then consume the verdicts card by card. Until then the only independent
work is BPR-0723 (the UIforETW startup failure), BPR-0718 (the red
workbench composition contract) and BPR-0720 once the owner answers; the
bundle inputs (`src/adapters/qt`, `src/app`, `src/core`, `src/adapters/sqlite`,
`src/application`) are protected by the thirteen candidates until the
owner says to resume development.

## Continuation after the owner's answers, 2026-09-10 (v005)

Dispatch `attended-prep-2026-09-10` d7 (mandate r3: the owner answered the
three open questions). The thirteen v004 candidates were released through
`resume-development --paths src/adapters/qt src/adapters/sqlite
--resources install:…` (all thirteen protect those paths; sequence 183)
before any source edit. Landed on main:

- [BPR-0724](../60-done/BPR-0724-read-mode-leaves-too-little-horizontal-room-for-the-code.md)
  (`25e1525d`): "it was difficult to read the code because of a lack of
  horizontal area for the text display". Read collapses the repositories
  drawer as well as the files drawer (300 pixels back to the comparison);
  what Read does with the conversations drawer (375-pixel minimum) or a
  single-pane layout is an options candidate, because the RFC's Read
  definition and AC-05 settle it the other way today. Parked.
- [BPR-0720](../40-parked/BPR-0720-comments-are-green-and-includes-are-grey-on-the-dark-palette.md)
  (`c5bd59f8`): "the whole palette, yes reset". The dark syntax roles
  follow Visual Studio dark / Visual Assist; three values lightened to the
  4.5:1 floor on the diff fills (the audit and ratchet pass); the
  appearance store moves to schema 4 and clears `palette_stored` once on
  the way through (the owner's store holds a light-bound customisation at
  schema 3, so that is what the reset removes). Parked.
- [BPR-0722](../60-done/BPR-0722-the-strip-hides-the-pull-request-identity-below-1400-pixels.md):
  "leave it be for now" recorded as the decision to keep the 900-pixel
  floor and the whole-identity yield below about 960; its attended
  criterion is unchanged.
- [BPR-0723](../60-done/BPR-0723-the-uiforetw-selected-diff-contract-fails-at-startup-on-head.md):
  `application_appearance_theme_customization_contract` shares the
  `STARTUP_FAILED` signature at cc31119b on a clean worktree build, before
  this milestone's changes; recorded there, not attributed here.

The bundle was rebuilt after both cards were parked (executable sha256
`d238842d…`, signed, tests disabled; a rebuild at 25e1525d is a no-op) and
the session `cbd48b91…` carries the thirteen v004 candidates at revision
`app-release-d238842d-2026-09-11.1` plus `BPR-0720-dark-palette` and the
options group `BPR-0724-read-width`: fifteen ready and protected at
sequence 228, none decided, in
[rfc-bpr-0028-attended-v005](../../verification/reviews/rfc-bpr-0028-attended-v005/operator-packet.md).
No host is running and no watch is armed; the packet says how to start
both.

Next: on the owner's say-so, `serve` and a persistent `wait --after 228`;
then consume the verdicts card by card (a BPR-0724 selection other than
"leave Read as it is" is a new bounded slice; the first option amends
AC-05). Until then the only independent work is BPR-0723 (now also the
theme customization contract) and BPR-0718; the bundle inputs
(`src/adapters/qt`, `src/adapters/sqlite`, `src/app`, `src/core`,
`src/application`) are protected by the fifteen candidates until the
owner says to resume development.

## Continuation after the two red contracts, 2026-09-11 (v006)

Dispatch `attended-prep-2026-09-10` d8 (mandate r4: "Fix the 2 failing
open work before we do the next round of reviews"). The fifteen v005
candidates were released through `resume-development --paths src/app`
(sequence 229) before the composition edit. Landed on main:

- [BPR-0718](../60-done/BPR-0718-the-workbench-composition-contract-is-red-at-the-pack-head.md)
  (`7c636594`): not a bisect; the probe failed its session-fallback check
  because BPR-0708's desktop path marked workbench persistence available
  without opening the store. Composition now opens it once and falls back
  to session defaults; per-window durable layouts are installed only when
  it opened. `application_workbench_composition_contract` passes.
- [BPR-0723](../60-done/BPR-0723-the-uiforetw-selected-diff-contract-fails-at-startup-on-head.md)
  (`d1ba791f`): the one word was the registry refusing a pre-created
  0755 root, since BPR-0695 made `--epoch-fixture-root` the registry root;
  eight contract scripts now narrow their roots (the theme, overview,
  saved-server, review-behaviour, epoch-maintenance and native-notification
  scripts had the same omission). Behind it, BPR-0390's displayed-comparison
  flag read empty after shutdown since BPR-0697/BPR-0705 release the read
  host and its comparison; the automated-run status is captured at the
  shutdown transition. The BPR-0646/0651 rail block was amended for
  BPR-0721's no-rails default. Both digests of
  `application_review_round_trip_composition_contract` repinned.

Re-run for the earlier dispatches: the UIforETW compact row at 1280 passes
(BPR-0722), the rail assertions could not be re-run as written because the
default has no rails and are amended (recorded on BPR-0721), and the theme
customization contract passes for BPR-0720. Also seen:
`application_native_notification_attended_contract` gets past startup now
and refuses at `platform_tray_not_composed` under the cocoa platform; not
attributed, not chased, no card. Gates at d1ba791f: composition contract
(140 s), both boundary scripts, strict tidy on the two touched TUs,
clang-format, `git diff --check`, documentation gate.

The bundle was rebuilt at d1ba791f (executable sha256 `cc2afa7f…`, signed,
tests disabled) and the session `cbd48b91…` carries the same fifteen
candidates at revision `app-release-cc2afa7f-2026-09-11.2`: fifteen ready
and protected at sequence 259, none decided, in
[rfc-bpr-0028-attended-v006](../../verification/reviews/rfc-bpr-0028-attended-v006/operator-packet.md).
No host is running and no watch is armed; the packet says how to start
both (`serve`, then `wait --after 259`).

Next: on the owner's say-so, `serve` and a persistent `wait --after 259`;
then consume the verdicts card by card. No independent unattended work
remains in this scope; the bundle inputs (`src/adapters/qt`,
`src/adapters/sqlite`, `src/app`, `src/core`, `src/application`) are
protected by the fifteen candidates until the owner says to resume
development.

## Continuation after the v006 hand-back, 2026-09-11

Dispatch `attended-prep-2026-09-10` d9 (mandate r5). The owner reviewed
all fifteen v006 candidates and handed back at sequence 278; events 262
and 263 were a pause and a review start with no decision. Each decision
was consumed against its own candidate and fingerprint (all fifteen
fingerprints match the prepared candidates; no stale verdict):

- Accepted and finished Done: [BPR-0662](../60-done/BPR-0662-drawer-widths-survive-home-and-a-squeezed-splitter.md),
  [BPR-0667](../60-done/BPR-0667-the-epoch-refuses-any-pull-request-whose-base-branch-moved.md)
  (the log half rests on the 2026-09-07 20:56 live acquire; the v006
  launches were cache hits, session observation at sequence 280),
  [BPR-0674](../60-done/BPR-0674-overview-is-starved-by-the-drawers.md)
  (headings accepted; policy chosen),
  [BPR-0675](../60-done/BPR-0675-a-drawer-cannot-be-collapsed-by-anyone-who-has-not-read-the-code.md)
  (the owner retracted the rail finding from BPR-0646),
  [BPR-0677](../60-done/BPR-0677-the-comparison-stays-unavailable-after-the-history-is-ready.md),
  [BPR-0683](../60-done/BPR-0683-settings-integration-and-native-acceptance.md)
  (AC-07, AC-08 and the macOS half of AC-11; AC-10 recorded on the
  focused effect-boundary fixture, the ledger's "exhaustive" wording
  unchanged),
  [BPR-0719](../60-done/BPR-0719-a-large-file-with-a-small-change-is-refused-by-the-comparison-work-limit.md),
  [BPR-0721](../60-done/BPR-0721-a-setting-hides-the-drawer-side-controls-and-a-collapsed-rail-shows-only-its-rail.md),
  [BPR-0722](../60-done/BPR-0722-the-strip-hides-the-pull-request-identity-below-1400-pixels.md),
  [BPR-0724](../60-done/BPR-0724-read-mode-leaves-too-little-horizontal-room-for-the-code.md)
  (two options chosen).
- Rejected, parked, follow-up carded: [BPR-0646](BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md)
  (strip accepted; epoch line not judgeable; rails retracted; sign-in
  "technically yes"), [BPR-0663](../60-done/BPR-0663-public-preview-refuses-the-epoch-smart-acquire.md)
  and [BPR-0676](../60-done/BPR-0676-the-epoch-line-does-not-show-whether-it-is-working.md)
  (all three the empty epoch line on a cache hit, [BPR-0725](../60-done/BPR-0725-the-epoch-line-shows-no-updates-when-the-catalogue-already-holds-the-head.md));
  [BPR-0720](../40-parked/BPR-0720-comments-are-green-and-includes-are-grey-on-the-dark-palette.md)
  (roles never emitted for C++, [BPR-0726](../60-done/BPR-0726-functions-types-and-macros-are-uncoloured-in-c-and-c-plus-plus.md)).
- Selections: [BPR-0727](../60-done/BPR-0727-overview-refuses-a-drawer-opened-by-hand.md),
  [BPR-0728](../60-done/BPR-0728-the-conversations-drawer-goes-narrower-than-375-pixels-in-read.md),
  [BPR-0729](../30-in-progress/BPR-0729-a-single-pane-read-layout-in-the-manner-of-p4merge.md).
- Platform split of AC-11: [BPR-0730](BPR-0730-windows-packaged-settings-inventory.md),
  [BPR-0731](BPR-0731-linux-packaged-settings-inventory.md); this closeout
  depends on both.

Root causes were scouted, not fixed: on a cache hit the core prepares
the revision with no mutation and starts the comparison without emitting
an accepted epoch or a history page, and nothing else requests the
newest page (`src/core/src/epoch_transition.cpp:1302-1309`); the C++
grammar KSyntaxHighlighting ships emits no function role and leaves user
types and macros normal, which the classifier drops. The historical v004
and v005 packets had three links repaired to the moved cards (paths
only). No product source changed in this dispatch.

Next, in order, on main: BPR-0725 (with its application-host and Qt
pins and the composed restore course), BPR-0726, then BPR-0727 and
BPR-0728 as one drawer milestone; a rebuilt tests-disabled bundle; a
v007 packet with the four parked candidates (BPR-0646 narrowed to "a
handle on the epoch line changes the comparison"), written last. BPR-0729
waits for an RFC-BPR-0028 design round. Open to the owner: what on Home
is "very messy" (BPR-0716).

## Continuation after the owner's answers, 2026-09-11 (d10)

The previous frontier paragraph (d9, after the v006 hand-back) is
retained in substance under "Continuation after the v006 hand-back".
The owner's answers, verbatim on the cards:

1. What on Home is "very messy" (on BPR-0716): the queue and its filters
   have no use signed out; open by link does not work without an account;
   "Read without signing in" leads to another confusing page; open the
   link anonymously from the first paste field; the slow automatic
   restore of the last pull request is disorienting, a recently viewed
   list would be better. Two screenshots of the signed-out dark Home were
   attached; their visible text, transcribed: (A) the "Connect a GitHub
   account" card ("BetterPullReview reviews pull requests from your GitHub
   accounts. Sign in once with a device code; the session is remembered
   on this Mac. To open a public pull request without signing in, paste
   its link in the bar below."), buttons "Sign in to GitHub…", "Read
   without signing in", "Accounts & hosts settings"; "Open a pull request"
   with the field "Paste a pull request link, or write owner/name#123" and
   "Open"; "Review queue" with "Review queue is empty. Local rules never
   change GitHub assignments." over an empty list; combos "Role: any",
   "Observer: any reason", "Bucket: any", "Repository: any", "State: any",
   "Sort: core priority"; status "No pull request is open in the
   workbench." (B) after "Read without signing in": the card reads "Open a
   pull request / Paste a pull request link in the bar below, or pick one
   from the review queue." with "Choose from the queue" and "Paste a link";
   a centred "Open a pull request / Enter an owner, repository, and pull
   request number above." with "Copy message"; the same field, focused;
   the same empty queue and combos ("Role: any" disabled); the same status.
   Carded as [BPR-0732](../60-done/BPR-0732-a-signed-out-home-is-one-paste-field-that-opens-a-public-pull-request.md)
   and [BPR-0733](../60-done/BPR-0733-home-lists-recently-viewed-pull-requests-instead-of-restoring-the-last-one.md).
   The "open by link doesn't work" claim was verified in code before
   carding: the Home bar resolves through `core::resolve_setup_destination`,
   which is `ready` only for an authenticated profile, and a public-preview
   session has no credential authority, so the bar stops at "Sign in and
   continue" before and after "Read without signing in"; the Settings
   session panel is the only working signed-out open. BPR-0716's earlier
   "the bar does open a pasted link" is corrected there.
2. BPR-0729's design round: "Card it, can happen after the verdict" —
   [BPR-0734](../60-done/BPR-0734-a-design-round-for-the-single-pane-read-layout.md),
   after the v007 verdicts; BPR-0729 depends on it.
3. Windows/Linux station time for BPR-0730/0731: "After we sort all the
   MacOS and platform agnostic stuff out" — recorded on both cards; they
   follow every macOS and platform-agnostic card above.

Home cards are placed after the drawer milestone and before the bundle
because BPR-0732 changes the Home the four parked candidates are reached
through and BPR-0733 changes what the bundle does at launch; both belong
in the v007 bundle so the owner sees one Home. IDs BPR-0732 to BPR-0734
were reserved on main (`b1d4e14b`).

BPR-0725 landed in this dispatch (see its verification record); the
maintenance public path was the one course it disturbed, repaired in the
probe. Mandate r7 added one card, [BPR-0735](../40-parked/BPR-0735-clangd-cannot-find-the-compile-database-and-reports-every-header-missing.md):
clangd reads no compile database because the project's lands in
`build/dev` and there is no `.clangd`; filed, not started. Next: BPR-0726.

## Landing record, 2026-09-11 (d13, BPR-0733 landed)

Dispatch `attended-prep-2026-09-10` d13 (mandate r7). Landed on main at
`91383cc4`, parked on human-review for the v007 verdict: [[BPR-0733]]. The
app no longer opens the last pull request at startup: `window_review_restoration::load`
selects the startup authority only, and a window's `open_recent` gesture
runs the same exact-position course the automatic restore used to. The
core keeps a per-profile `Recent` list (ten, most recent first, title once a
snapshot was seen, the saved position tracked from `accepted_position`);
the host persists it beside the per-window records (`recent_reviews`,
sqlite `user_version` 3, migrated from 2 with the old rows seeded newest
window first); the composition publishes a per-window
`recent_reviews_projection` filtered for the window's session (signed out:
public entries only; an account: its host's entries; Resume only when the
session matches) and routes "open" through the Home bar's setup path and
"resume" through `window_review_restoration_host::open_recent`; Qt shows
"Recently viewed" between the setup row and the queue heading, hidden when
empty, with Return/activation opening and a "Resume reading" action.
Invariants to know: a refused startup *authority* (missing grant, unavailable
capability) is now silent because nothing was pending; a refused
*destination* or store still speaks. Verified at `91383cc4`: the three
restoration selectors, `qt_composed_github_scale_*`,
`application_profile_window_review_restoration_public_path` (about 9.5
minutes now; timeout 1800 s), full dev build, `check_boundaries`, strict tidy
and clang-format on touched files, DOCUMENTATION_OK. Follow-up carded:
[[BPR-0736]] (queue rows ignore Return on macOS; pre-existing, Inbox).
The d11/d12 landings ([[BPR-0726]], [[BPR-0727]], [[BPR-0728]],
[[BPR-0732]]) are unchanged and still parked; [[BPR-0716]] stays in Backlog
on BPR-0732. Next, in order: a rebuilt tests-disabled bundle, then the v007
packet written last, covering BPR-0646 narrowed to the epoch line, BPR-0663,
BPR-0676 and BPR-0720, plus the asks for BPR-0725, 0726, 0727, 0728, 0732 and
0733 (0733's ask: restart the signed bundle with a review saved, see Home at
once with the entry, open it by hand), the ask for BPR-0732 (paste a public
link while signed out and it opens in one gesture), and a visible ask for
d11's stale-pin judgement that `infer_modality_from_drawers` no longer
infers Overview from the files-only shape. After the v007 verdicts:
[[BPR-0734]]; on their stations later: [[BPR-0730]] and [[BPR-0731]], on
which this closeout depends. Outside the pack: [[BPR-0735]] (clangd noise),
Ready, not started. No host is running and no watch is armed. Detail below
under "Continuation after the owner's answers".

## Continuation after the v007 hand-back, 2026-09-11 (d15)

The d14 frontier (v007 prepared at sequence 311) is retained in substance
by the packet. Decisions, events 312 to 321, then the hand-back at 322:

- Accepted and finished Done from their own acceptance: [[BPR-0663]]
  (the cache-hit launch showed the updates; the `smart_acquire` half rests
  on the 2026-09-07 live acquire recorded on BPR-0667), [[BPR-0676]] (the
  packet's side question about naming the default pair drew no answer;
  nothing carded), [[BPR-0725]], [[BPR-0726]], [[BPR-0727]], [[BPR-0728]],
  [[BPR-0732]], [[BPR-0733]]; [[BPR-0716]] promoted, claimed and finished
  on BPR-0732's verdict, since BPR-0732 took its scope.
- Rejected, parked, follow-up carded: [[BPR-0646]] became [[BPR-0737]]
  (the epoch control is the centre pane's first widget, so its labels
  move with the drawer widths and modality; the owner wants it and the
  "Comparing …" menu on one full-width row under the strip); [[BPR-0720]]
  became [[BPR-0738]] (macros are classified as the preprocessor role and
  paint its grey; functions paint Visual Studio's `DCDCAA`; the owner's
  reference is Visual Assist's purple and gold, sampled on the card).
- Accepted with a note: [[BPR-0728]]'s cursor finding became [[BPR-0739]]
  (no cursor or handle code exists on the central splitter; four
  candidate causes are listed, none confirmed).

No product source changed in this dispatch. The v007 packet's BPR-0733
link was repaired to Done (path only). RFC-BPR-0028 and RFC-BPR-0006 carry
the verdict paragraphs.
