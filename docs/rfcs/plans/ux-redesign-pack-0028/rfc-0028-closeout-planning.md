# RFC-0028 closeout planning — 2026-09-23

Run `bpr-main-win-20260923` on the `bpr-main-win` station (native Windows 11,
the main checkout on `main`). The work was done by a non-delegating
`rfc-implementer` under the coordinator-implementer recipe; the mandate binding
is `mac-integrator-relay-20260923`. The owner's words, relayed by the Mac
integrator: "implement available unattended work, focus on closing out rfcs.
You can revise and update the cards and rfcs if they have gone stale."

The immutable RFC base is `58a853dd23cbbb7a7e4d28ff0ec39d405593d838`. That is
the commit before `08a621f7`, which locked the direction and carded its first
slices on 2026-09-06; no base had been recorded for RFC-BPR-0028 before.

## Why a milestone was needed

RFC-BPR-0028 is the next RFC in canonical index order: every RFC before it is
Auto-Verified or Deferred. The plan had no milestone that closes it, and no
closeout card existed. The main integrator reserved BPR-1020 to BPR-1022 for
this station (`81927622`, receipt
`docs/issues/_reservations/BPR-1020--BPR-1022--f3b33a48cfc4.json`); BPR-1021 is
the closeout, [[BPR-1021]].

## What the milestone binds

`close-rfc-0028` follows `windows-strip-title-row` and binds two cards:

- [[BPR-0746]], the resize cursor over the seam. Its card names this plan, but
  no milestone held it. It is Blocked on `owner-declined-probe`: the owner
  declined its cursor probe on 2026-09-12, so its AC1 and AC2 cannot be
  obtained. It can close only through a recorded owner amendment, never as
  passed.
- [[BPR-1021]], the closeout. It is in Backlog because both of its
  dependencies, BPR-0653 and BPR-0746, are open.

[[BPR-0653]], the Windows title row, stays in `windows-strip-title-row`. It is
a prerequisite of this milestone, not its work. It is parked on three owner
asks: the Windows attended capture, Qt's private Windows native interface in a
shipped target, and the AC-01 decision below.

## The AC-01 conflict, re-checked at `38cc26d7`

On main, between the strip and the epoch line, sits the review surface's
composition entry row: Compose review and Draft comment or reply, from
RFC-BPR-0037's `cc038748` ([[BPR-0929]]). While a pull request loads,
[[BPR-0795]]'s readiness line sits there too.

- `build_central_surface` lays out the readiness line, then the entry row
  (inserted at index 1), then the epoch line, then the splitter.
- On the real Windows platform, `application_windows_title_row` passes and
  reports `composeReviewActionButton@30+20` and
  `composeInlineDraftActionButton@30+20` at 1280x852 and 1920x1080.

[[BPR-0940]] (`f839e26a`) has since made the window course assert that
arrangement, so no test is red over it. AC-01 and the chrome paragraph still
say "the strip and the epoch line only", so the owner must choose between
amending them and moving the entries. The milestone's second exit criterion
holds that decision open.

## Plan amendment

As with the RFC-0025 and RFC-0026 records, `check-amendment` refuses new work
IDs on their own. The first mechanical check, from the live plan to the
complete proposal, returned: "amendment must preserve every obligation;
reconcile changed requirements separately".

The change was reconciled explicitly instead. BPR-0746 and BPR-1021 were first
registered as unplanned work on a partial plan. The regrouping of both into
`close-rfc-0028` was then checked.

| | Fingerprint |
|---|---|
| Live plan before this amendment | `sha256:6637b906e9ff68e64dce1b34c1d25fbefd6e7e16a946ea5479af3d253d453289` |
| Partial plan (BPR-0746 and BPR-1021 unplanned) | `sha256:3639e8ad62011fad2b9342b4d494af04b2d3ca17bfc83691fe025e20546ec4e8` |
| Published complete plan | `sha256:5fde57a02f972710c46b50c3b10fd8d66f56e7c5b68d0d6b88bebfc00c267056` |
| `close-rfc-0028` definition | `sha256:5fb2e36f5814ecaf9b7ecbec3c908173305f617391a6b00f4b576fa8e411ac66` |
| `windows-strip-title-row` definition (unchanged, as bound for BPR-0653) | `sha256:c2ae5fcd320458fef0d11817c304e72439ffaf355617a0b90b4370bae8f8a4d3` |

The only affected milestone is `close-rfc-0028`. No existing milestone, exit
criterion or work item changed. Adding the milestone removes, weakens or
accepts nothing. Semantic acceptance remains an engineering and closeout
obligation, not a helper verdict.

[[BPR-1022]], opening the Windows application menu from the keyboard with Alt
and F10, is standalone. It is not a dependency of this closeout: the menu
control is already a keyboard focus stop.

No product code changed in this planning step. No live GitHub/GHES mutation,
owner question or attended session was used.
