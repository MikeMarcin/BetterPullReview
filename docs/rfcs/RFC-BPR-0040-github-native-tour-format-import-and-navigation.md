---
id: "RFC-BPR-0040"
title: "GitHub-Native Tour Format, Import, and Navigation"
status: "Auto-Verified"
area: "BPR"
milestone: "Review tours"
depends_on: ["RFC-BPR-0039"]
pack_version: "review-tour-v1"
metadata: {"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"},"owner_closeout_amendment":{"revision":"r2-20260927-close-implemented-scope","deferred_formal_dependencies":["RFC-BPR-0021","RFC-BPR-0022"],"follow_up":"BPR-1226"}}
---

# RFC-BPR-0040: GitHub-Native Tour Format, Import, and Navigation

## Decision

The shared tour **is normal GitHub comments**. Its primary root is a normal author review-summary comment containing a Start here index. Each ordinary inline stop is a root review comment with its own discussion. A normal PR description contains the concise Problem and Solution and, after publication, an optional prominent link to the index. BPR is an authoring and navigation client, not the owner of a proprietary shared-tour object.

All essential shared explanation, ordering, destinations, and relationships must be recoverable from GitHub without local BPR records. No hidden HTML metadata, custom URL scheme, opaque marker, required bot/app, label, status check, hosted tour service, or BPR install instruction is part of publication. A normal visible heading and normal Markdown links are a presentation convention, not evidence that a remote author used BPR.

## Published representation

See the entirely synthetic [examples](assets/bpr-review-tour-v1/examples/README.md). Suggested output:

- PR description: `## Problem`, `## Solution`, other preserved author/team content, and an optional `Review tour` link.
- Review summary: `## Review tour — Start here`, a concise orientation line, and an ordered list linking each meaningful stop title to its actual comment URL.
- The brief (Problem and Solution) is published in exactly one place ([[BPR-1787]]). By default it is the description: **Update PR description after publishing** starts on, the `[Review tour](…)` link leads the description ([[BPR-1786]]), and the review summary (or compatibility index comment) opens with the one line `The problem and solution are in the pull request description.` ahead of its route. With the description update off, the summary carries `## Problem` and `## Solution` ahead of the route and the description is not edited. The publication preview says which. A published tour keeps its brief where it was first published: an update patches the description, the summary, or both, whatever the checkbox says, and never moves the brief. Tours published before BPR-1787 carry it in the summary (and, if the description was updated, there too) and keep working unchanged.
- Stop: `### Author's tour · 2 — Why this boundary matters`, locally meaningful explanation, optional related code links, and a short footer containing Previous, Tour overview, and Next links where applicable. When the footer has Previous or Next, the line directly under it reads `_If Next or Previous doesn't jump (GitHub hides large diffs), use the Tour overview._`: GitHub does not jump to an anchor inside a diff it hides as too large. Stops published before that note still map, and gain it on their next update ([[BPR-1785]]).

The root index's **textual list order** defines the route. Markdown numbering is decoration; repeated `1.` list syntax is legal. Neither server comment order, title text nor displayed ordinal identifies a stop. Returned GitHub comment identities do. BPR maintains ordinals/navigation as derived text but preserves author-edited narrative. The index may link comments in several reviews or, in compatibility mode, ordinary PR conversation comments.

Use canonical `html_url` or an equivalent returned provider URL for remote comment destinations after identity validation. Never invent a comment ID from a draft ID, decode a node ID to guess a database key, or synthesize an unverified hash fragment. Preserve ordinary commit-pinned file/line links for source references. Link labels escape Markdown delimiters and untrusted titles; source paths use the existing canonical permalink construction/escaping policy.

Each stop must remain intelligible when encountered directly in Files changed. Do not require reading the previous stop to discover what the local comment is talking about. Discussion remains in the normal thread beneath the author's explanation. Tour membership is not thread resolution and does not hide replies or unresolved objections. Resolved/outdated comments remain in the index unless the author changes the route.

## Source destination and compatibility

Separate the **semantic source destination** from the **remote comment subject**. The former includes repository identity, full commit/blob identity where known, original path, side/current-or-historical meaning, and a one-based inclusive line interval. The latter is what GitHub actually accepted: inline line/range, file-level review comment, or PR conversation comment. Do not claim a file subject is a native inline subject.

A source range must be contiguous in one actual blob. Do not infer a range from visual alignment rows, join disjoint hunks, cross sides, or treat virtual-space columns as source characters. Reuse `source_line_ref` and exact retained comparison binding; add a narrow range type without globally rewriting all comment APIs. Native batch publication is initially qualified for current-PR diff locations. Where a range is unsupported, preserve the entire range as a pinned source link rather than silently anchoring only its last line.

Placement policy:

| Destination | Shared representation |
|---|---|
| Supported current diff line/range | Native inline review comment. |
| Supported current off-hunk location | Native only on evidence for the exact batch/write operation; website support alone is not evidence. |
| Off-hunk location in a changed file without native support | Reuse RFC-BPR-0022's ordinary file-level comment plus visible exact permalink. |
| Historical/base or entirely unchanged file that has no supported review subject | A normal PR conversation comment with visible commit-pinned source link; this is an explicit tour-specific extension, not a fabricated inline anchor. |
| Unavailable/ambiguous source identity | Keep the draft and explain the unresolved destination; no guessed location is published. |

Prefer putting compatible file-level threads into the same pending review when the exact GraphQL operation supports that on the connected host. The documented REST batch `comments` shape does not itself establish file-subject support. Qualify the necessary GraphQL path under RFC-BPR-0041; do not copy standalone REST fields into a batch payload. When a destination cannot participate in the pending review, the publication plan must expose the different visibility boundary. No blanket atomicity claim survives a standalone comment fallback. [GH-REVIEWS], [GH-COMMENTS], [GH-GRAPHQL], [GH-ISSUES] in [EXTERNAL-AUTHORITY](assets/bpr-review-tour-v1/references/EXTERNAL-AUTHORITY.md) are the external API basis; runtime support must still be demonstrated.

For RFC-BPR-0022-compatible single-line file subjects, keep the **canonical bare permalink as the first line**, then a blank line, then the heading/narrative/footer. Do not prepend the tour heading and break that existing rehydration grammar. Range and historical permalink forms are explicit new tour codec variants; do not claim the old single-line parser already accepts them. For Markdown source, preserve the existing `?plain=1` convention before the line fragment where applicable.

Fallback preserves guiding explanation and discussion, not GitHub's Apply suggestion semantics. The tour generator must not emit apply-able suggestion blocks or claim they remain executable in a permalink fallback. Author-provided suggestion blocks require the existing explicit compatibility check; do not silently convert them.

## Discovery and cold import

Loading existing content is deterministic and AI-free. Fetch raw Markdown, entity IDs, actors, PR relationship, review membership, web URLs and available edit observations through existing bounded read ports. Page collections completely before making a completeness claim. Partial data remains Partial; absence from a page is not deletion.

Discovery produces candidates, not automatic write ownership. Prefer an explicit linked index in the description or a previously bound root. Otherwise enumerate plausible author review-summary/PR-comment indexes whose ordered links resolve to comments on this exact PR. A title alone is insufficient. Multiple plausible roots require a choice; do not guess “the newest one” and start modifying it. Any usable normal comment/list can be manually selected as the index, and the author may manually select the appropriate list when its body contains several.

Import reads the brief from the description first, then from the selected root's own comment, so a tour published with its brief in the description and one published before BPR-1787 with it in the summary both reconstruct on a clean profile ([[BPR-1787]]). Each field's baseline names the entity it came from, and reconciliation compares a field only with that same place.

Resolve each index entry by validated remote identity. Deduplicate fetches without collapsing separate list entries silently. Duplicate IDs, a link back to the root, malformed or cross-PR links, unavailable comments, or mixed authors remain visible and require author resolution before editable publication. Do not recursively traverse Previous/Next links as an unbounded graph. The root's ordered entries are enough. Supporting links outside the PR may remain ordinary links, but do not become writable tour stops or automatic credential-bearing fetch destinations.

The actor owning an indexed comment is not necessarily the PR author. Imported third-party comments can be read and linked with attribution, but BPR may edit only objects for which the acting account has the exact supported ownership/permission authority. Default author editing is restricted to the acting account's own tour comments. Do not rewrite someone else's review because they share a repository role.

## Markdown round trip

Retain raw UTF-8 Markdown as the baseline. Recognize complete top-level field sections and the selected index list using a bounded syntax-aware scanner that respects fenced code, block quotes, nested lists and raw HTML regions. A heading-looking line inside a fence is not a Problem section. Do not use unbounded regexes or regenerate the complete PR description from a renderer AST.

For recognized Problem/Solution sections, bind the editable content to exact source spans. Reuse them verbatim until explicitly changed. Keep the title, linked Jira references, checkboxes, disclosures, whitespace/newline conventions, ordering of unrelated sections and other template material untouched. On first preparation of an unstructured body, provide a manual “Use as problem/solution” span selection and an insert-location preview. Ambiguous duplicate sections require explicit mapping; no LLM is invoked to import.

For stop comments, manage a confidently recognized heading/ordinal and exact navigation footer separately from the narrative. A footer-looking paragraph in quoted code or ordinary prose is not automatically owned. When a website edit changes the layout, retain the complete remote body and enter mapping-needed mode instead of deleting unfamiliar text. On cold import, show how a comment is split before the first change when structure is ambiguous. It is acceptable to maintain only the index automatically until the author maps a nonstandard stop footer.

Initial generated layout is not mandatory for website editing. Supported existing syntax is preserved. A fully custom body that cannot be structurally mapped remains readable and manually editable as one normal comment; BPR must not demand hidden markers to proceed.

## Navigation

BPR's Tour projection uses the same remote comments and source navigation policies as normal reviews. Distinguish Next stop from Next file/change. Save a local return point before excursions to callers or related code. Read source for the displayed original revision when available; an unmappable historical comment remains visible with a code link and unavailability explanation. Never substitute the latest source while displaying the old explanation as exact.

Navigation is not acknowledgement of code, viewed-file state, comment resolution, or approval. No receipt used by review correctness/status is minted from merely visiting a tour stop. Private tour reading progress can be lost with the local database; shared comments cannot.

## Owner-amended terminal scope — 2026-09-27

Owner amendment `r2-20260927-close-implemented-scope` accepts this RFC's
verified implementation using the named production-port fixtures, real local
storage/process/editor checks and seven exact accepted Mac candidates. The
[r2 mandate](../verification/RFC-BPR-0039-0043.owner-closeout-r2.md) records the
owner instruction, original criteria and transfer of every unmet qualification.
BPR-1222 retains GitHub.com live evidence; BPR-1223 retains GHES evidence;
BPR-1224 retains other native stations; BPR-1225 retains installed-provider/model
evidence; BPR-1226 retains deferred predecessor acceptance. These are standalone
Blocked follow-ups outside this RFC's amended terminal gate.

Auto-Verified records the amended implementation scope. It supplies no live
server, website, other-platform, installed-model or predecessor verdict. Exact
production Tour operations remain unavailable until separately qualified. The
original verification prescriptions below still govern those follow-ups. All
credential, source-identity, publication and no-implicit-effect boundaries remain
normative; no capability is enabled by this documentation amendment.

## Acceptance criteria

- **AC40-01:** Every shared stop and index is ordinary readable Markdown in normal GitHub comments, with real links and no required BPR-specific metadata.
- **AC40-02:** Index order, comment identity and visible stop numbering remain distinct; duplicate titles and reordered server collections do not scramble the route.
- **AC40-03:** A fresh local database reconstructs a published tour and its brief from paged GitHub data without AI or original private IDs.
- **AC40-04:** Markdown round trips preserve all untouched source bytes, including unrelated description/template content and CRLF/LF boundaries.
- **AC40-05:** Ambiguous mapping, duplicate/missing/foreign links, partial pages and third-party comments remain visible and do not grant mutation authority.
- **AC40-06:** Line/range/source-version identity and native/file/conversation fallback subjects remain explicit and exact.
- **AC40-07:** Production-port fixtures preserve Start here, comment, Next/Previous and overview links using returned identities without invented heading fragments; actual website-only navigation and replies remain explicitly unqualified.
- **AC40-08:** BPR excursions/return, resolved/outdated threads and source-unavailable states retain the tour without altering review state.

## Verification

Use a raw-Markdown golden corpus, parser/property tests, paged provider fixtures and mixed-client simulations. Perform a designated real GitHub website walk and, for each claimed GHES capability, a corresponding host-specific course. Documentation proves an endpoint's documented shape, not successful pending-comment edits or actual website navigation on a deployed server. Preserve such unknowns in the verification matrix.
