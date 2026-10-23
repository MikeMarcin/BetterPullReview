# [[BPR-PENDING-AQ8]] reseal (2026-10-22) over the BPR-PENDING-AQ6 reseal
# below, linear, on this card's commit. Pinned-scope change, tests-on only:
# github_application.cpp's locate_agent_program finds this computer's own
# agents for an attended look (AttendedAgentSearch, which main.cpp sets with
# AttendedWindowBrowser for --attended-launch outside the automated test
# boundary), so the person can set up and ask their own Claude Code or Codex.
# Every automated course keeps its own search path. No grant, target,
# provider, credential, mutation or publication path changes; no new src/app
# source. Full and manifest pins bind these bytes; mutation 16d135419 stays
# exact.
# [[BPR-PENDING-AQ6]] reseal (2026-10-22) over the rebase reseal below,
# linear, on this card's commit. Pinned-scope change, RFC-BPR-0092 M6 (owner
# answer Q1 A): github_application.cpp, agent builds only, adopts the
# window's qt::agent_setup_view into the Agent tab and wires its presses, and
# the new Settings rows' (Codex program and protocol, a plain-text harness,
# clone folders and linked clones, the review skill's editor), to the
# existing agent_setup_host, clone_link_host and review_skill_host, whose
# projections are all they show. The first time the tab is shown the setup
# host looks for agents (file metadata only, owner answer Q2). Install Claude
# Code and Install Codex open their vendors' pages through the window's
# existing browser port. The ready line once both checks pass reads the
# accepted snapshot's title, files and threads. No grant, target, provider,
# credential, mutation or publication path changes; no new src/app source.
# Full and manifest pins bind these bytes; mutation 16d135419 stays exact.
# Rebase reseal (2026-10-22): BPR-PENDING-AQ1 to AQ5 rebased onto main
# 4e318b625 (BPR-1741's reactions and Resolve conversation, whose notes
# follow), and their RFC renumbered from RFC-BPR-0091, which main had taken,
# to RFC-BPR-0092. github_application.cpp holds both lanes' changes with no
# overlap; the only edits of this commit are the RFC number and commit
# identities in comments. The per-card notes below keep their wording with
# the rebased identities. Full and manifest pins bind these bytes; mutation
# 16d135419 stays exact.
# [[BPR-PENDING-AQ5]] reseal (2026-10-22) over the BPR-PENDING-AQ4 reseal
# below (c54c8094a), linear, on this card's commit. Pinned-scope change,
# RFC-BPR-0092 M5 (owner decision D1, answer Q9 A): github_application.cpp,
# agent builds only, gives every review request (each entry point but a
# comment-workshop press, never a writing request) the review brief:
# application::read_review_brief reads the accepted snapshot's title,
# description, base branch, changed files and review threads, and
# core::bound_review_brief bounds it (description 16 KiB cut at a line,
# threads 64 KiB unresolved first, every cut counted). It carries the review
# skill application::review_skill_host gives it: the built-in bpr-pr-review
# until the profile edits it, then the copy adapters::sqlite::
# make_review_skill_store keeps in the same agent-setup file. A file-local
# save_review_skill_copy writes an edited copy to a new file in Downloads
# (QIODevice::NewOnly) before Restore default removes it. The host is shut
# down with the window and counted in quiescence. No grant, target,
# provider, credential, mutation or publication path changes; no new src/app
# source. Full and manifest pins bind these bytes; mutation 16d135419 stays
# exact.
# [[BPR-PENDING-AQ4]] reseal (2026-10-22) over the BPR-PENDING-AQ3 reseal
# below (c6b06b83a), linear, on this card's commit. Pinned-scope change,
# RFC-BPR-0092 M4 (owner answer Q7 of 2026-10-22): github_application.cpp,
# agent builds only, gives each agent workspace request the linked clone the
# repository's link names (an absolute path, or none for the private copy),
# so the Git adapter's workspace borrows that clone's history through one
# alternates line and copies only what the clone lacks; nothing is written
# to the clone and the workspace has no remote. Check it works asks the agent
# host's new probe_workspace, which makes and removes the workspace an Ask
# would make on the accepted binding through the same workspace port, admits
# no session, runs no process and answers on the decision lane. No grant,
# target, provider, credential, mutation or publication path changes; no new
# src/app source. Full and manifest pins bind these bytes; mutation 16d135419
# stays exact.
# [[BPR-PENDING-AQ3]] reseal (2026-10-22) over the BPR-PENDING-AQ2 reseal
# below (a856809fa), linear, on this card's commit. Pinned-scope change,
# RFC-BPR-0092 M3 (owner decision D2 of 2026-10-22): github_application.cpp,
# agent builds only, composes the profile's clone folders and repository
# links: adapters::sqlite::make_clone_link_store over the same agent-setup
# file (folder paths in order; per repository a clone path, a remote name or
# the private copy; no URL or credential column) and
# application::clone_link_host, whose reads are the workspace adapter's
# scan_clone_folders, read_clone and existing_folders (metadata only, no
# link followed, no Git run, nothing written). The accepted review's host
# name, repository identity and owner/name are observed when its facts are
# set and cleared with them; a link that serves, or the private copy, marks
# setup's Repository step done. Shut down with the window and counted in
# quiescence. No grant, target, provider, credential, mutation or publication
# path changes; no new src/app source. Full and manifest pins bind these
# bytes; mutation 16d135419 stays exact.
# [[BPR-PENDING-AQ2]] reseal (2026-10-22) over the BPR-PENDING-AQ1 reseal
# below (b9492bbe2), linear, on this card's commit. Pinned-scope change,
# RFC-BPR-0092 M2 (owner answers Q2 and Q3 A of 2026-10-22):
# github_application.cpp, agent builds only, composes the profile's agent
# setup: adapters::sqlite::make_agent_setup_store over the application data
# root (agent-setup.sqlite3: the chosen agent, the Codex and plain-text harness
# programs and Codex's protocol; no credential column), a launcher and worker
# lane of its own, and application::agent_setup_host, which reads file
# metadata to detect agents, runs only the chosen agent's own status command
# and --version, bounded, with the profile's environment allowlist and no
# review context, and publishes the saved programs to the window's
# agent_program_source. The host is given this review's host policy beside the
# agent host's observe, shut down with the window and counted in quiescence.
# present_agent_setup prefers the saved profile in the Agent panel and in Send
# to Agent (review_cycle_desktop.{hpp,cpp}: prefer_send_profile keeps the given
# profiles and their order, the preferred one first). No grant, target,
# provider, credential, child environment, mutation or publication path
# changes; host policy unchanged; no new src/app source. Full and manifest pins
# bind these bytes; mutation 16d135419 stays exact.
# [[BPR-PENDING-AQ1]] reseal (2026-10-22) over last matching main 92162efed,
# linear, on this card's commit. Pinned-scope change, RFC-BPR-0092 Q5 (owner,
# 2026-10-22): github_application.cpp, agent builds only, puts
# application::located_agent_process in front of the Claude wrapper port, so
# a Codex or plain-text harness request that names its shipped bare program
# (codex, bpr-agent-terminal) runs the profile's saved program or, for Codex
# alone, the codex the agent adapter finds now on this application's PATH or
# in Codex's usual folders (locate_agent_program); none found ends the request
# failed_to_start and starts nothing. A request already naming its program by
# path, and every Claude request, passes through unchanged. Tests-on only: the
# fixture's CodexSearch, and a fixture course never finds the developer's own
# codex. review_cycle_desktop.{hpp,cpp} take the same source and locator for
# Send to Agent. No shell, grant, target, provider, credential, child
# environment, mutation or publication path changes; host policy unchanged;
# no new src/app source. Full and manifest pins bind these bytes; mutation
# 16d135419 stays exact.
# Upkeep reseal (2026-10-23): the only src/app change since the BPR-1746 note below
# is renaming its [[BPR-PENDING-AX]] comment tags to [[BPR-1746]]; no code change.
# [[BPR-1746]] reseal (2026-10-23) over last matching main b8d2ec6cf,
# linear. Pinned-scope commit cb4cbfd9c (the owner's decision of 2026-10-23,
# "Oh yeah sure let gh allow that."): github_application.cpp feeds GitHub's
# viewerCanReply and the pull request's lock, now read with the conversation,
# into core::decide_thread_reply (thread_reply_facts_of), and
# begin_review_reply makes no draft for a thread the core refuses.
# github_public_path_fixture.cpp and large_review_fixture.hpp, test-only, serve
# viewerCanReply and locked as the recorded fixtures now do, and the large
# review's read shape asks for them. The reply write the CLI connection now
# admits is the adapter's (gh_cli_transport.cpp), outside src/app. No
# credential, provider selection, ledger write or source-inventory change; the
# production make_live_review_mutation_provider call is untouched. Full and
# manifest pins bind these bytes; mutation 16d135419 stays exact.
# Upkeep reseal (2026-10-23): the only src/app change since the BPR-1745 note below
# is renaming its [[BPR-PENDING-AT]] comment tags to [[BPR-1745]]; no code change.
# [[BPR-1745]] reseal (2026-10-22) over last matching main 27d17fea5,
# linear. Pinned-scope change, the owner's decision of 2026-10-22 ("Allow
# replying to an outdated thread from the drawer, like GitHub does";
# RFC-BPR-0028 amendment): github_application.cpp takes an outdated thread's
# Reply (the window's new thread-reply dispatch) to begin_thread_bound_reply,
# which decides the route with core::decide_thread_reply from the accepted
# snapshot's thread, opens or makes a reply bound to the thread through the
# private review's new select_thread_reply (waiting, as PendingThreadReply,
# while drafts are restored or a line's draft is opened), and publishes it
# with the thread's binding (review_mutation_publication_request::ThreadReply:
# its path and its first comment's original commit, the head acknowledged)
# under the one-shot gesture grant, which binds to the accepted handoff when no
# selected diff exists. A Refresh holds the reply again without a line.
# private_review_workflow.cpp carries thread-bound replies to a new head in
# restore and select_thread_reply. main.cpp adds three reply-draft courses and
# exempts outdated-restart's smart calls; github_public_path_fixture.cpp and
# github_application_fixture_access.hpp, test-only, stage an outdated thread
# and record the reply's binding. No credential, provider selection, ledger
# write or source-inventory change; the production
# make_live_review_mutation_provider call is untouched. Full and manifest pins
# bind these bytes; mutation 16d135419 stays exact.
# Upkeep reseal (2026-10-22): the only src/app change since the BPR-1741 note below
# is renaming its [[BPR-PENDING-AR1]] comment tags to [[BPR-1741]]; no code change.
# [[BPR-1741]] reseal (2026-10-22) over last matching main 3ddfbc5f8,
# linear. Pinned-scope commits 1e097ce42, 4ff78af59, 2fcdde895 and d8bc00b29 (the owner's
# request of 2026-10-22 for reactions and Resolve conversation, RFC-DRAFT-0027).
# github_application.cpp names the core's new not_permitted resolution refusal,
# takes GitHub's per-thread viewerCanResolve / viewerCanUnresolve into the
# resolution decision, no longer withholds resolution from the GitHub CLI
# connection, keeps the core's conversation overlay (a reaction or resolution
# shows at once, GitHub's answer and the next read reconcile it), presents
# reactions and resolutions to the window after each accepted read, decides a
# post's reaction through core::decide_reaction (public preview refuses the
# new react_to_comment intent) and sends it to the mutation host's new react
# under a one-shot gesture grant from issue_reaction, the grant a resolution
# gets. Its BUILD_TESTING threaded-reply course goes on, with reactions served,
# to Unresolve conversation and the reaction steps; main.cpp's --reaction-probe
# alone serves reactions and resolves threads on the fixture for an attended
# look (d8bc00b29). No credential, provider
# selection, publication path, ledger write or source-inventory change; the
# production make_live_review_mutation_provider call is untouched. Full and
# manifest pins bind these bytes; mutation 16d135419 stays exact.
# [[BPR-1736]] reseal (2026-10-22) over last matching main 293d6181a,
# linear, on this card's composition commit. Pinned-scope change, owner
# decisions D1 to D4 of 2026-10-22 (RFC-BPR-0022 amendment r6):
# github_application.cpp opens the profile's learned off-hunk facts on their
# own connection to the recovery journal (make_off_hunk_fact_store), passes
# the presented fact to the mutation host as the source observation's
# OffHunkFact, says what was learned in the Whole PR notice (Learned) and drops
# the mark where GitHub accepted such a line, holds a pending comment outside
# the diff unless the review fact is supported, re-presents the notice and the
# holds after a publication's terminal, and, when a review carrying such a line
# under a supported fact is refused for an unplaceable line
# (review_submission_workflow's new unplaceable refusal observer), demotes the
# fact for both operations and holds the comment; nothing is resent.
# review_submission_workflow.cpp and .hpp are in the manifest;
# review_submission_fixture.hpp and github_public_path_fixture.cpp, test-only,
# are not (the fixture answers a core-placed off-hunk native comment in
# GitHub.com's measured refusal by default). No credential, grant, provider
# selection, ledger write or source-inventory change; the production
# make_live_review_mutation_provider call is untouched. Full and manifest pins
# bind these bytes; mutation 16d135419 stays exact.
# [[BPR-1735]] reseal (2026-10-22) over last matching main 4ed30b995,
# linear, on this card's commits. Pinned-scope change, owner decision of
# 2026-10-22 (RFC-BPR-0022 amendment r5): github_application.cpp reads
# GitHub's own diff of a file (the merge base to the head) through the epoch
# host's new resolve_github_diff_lines, places the selected Whole PR line and
# every comment pending in the review with the core's hunk rule, passes the
# facts to the mutation host as the source observation's WholeDiff, holds a
# pending comment the core places outside every hunk (the window's hold and
# review_submission_workflow::hold_comments, which refuses Submit while one is
# carried), and treats the journal's rejection (unplaceable_line,
# refused_not_published, settled_not_published) as a definite refusal that
# unlocks the draft. Its BUILD_TESTING courses: the attended draft seed
# chooses a line GitHub's diff holds, and the start-review, label-authoring,
# review round-trip and private-review courses place as a build before the
# rule (set_whole_diff_placement_enabled), their subjects being the review,
# the labels and RFC-BPR-0022's negotiation. review_submission_workflow.cpp
# and .hpp are in the manifest; private_review_fixture_course.cpp and
# github_application_fixture_access.hpp, test-only, are not. No credential, grant,
# provider selection, ledger write or source-inventory change; publication now
# admits a Whole PR off-hunk line to the file comment at once. Full and
# manifest pins bind these bytes; mutation 16d135419 stays exact.
# BPR-1694 combined integration over reviewed main b9a74b63a and release 0f52c515e.
# All main feature reviews below are retained. Additional pinned-source difference
# is exactly the retained complete_on_lane/observe_on_lane calls and the guarded
# pending-tab reading gesture, reviewed on BPR-1694; no mutation branch change.
# BPR-1694 scoped reseal (2026-10-21), over matching a023d1d81 / 3a1ee98c0.
# Retained initial refusals precede this renewal. The composition's tab
# completion and line observation use explicit on-lane host entry points, so
# Quit cannot overtake their queued persistence. An accepted interactive line
# on a pending tab's exact displayed material completes that load before its
# driver can restore an older selection; request, generation, accepted pair,
# file and displayed publication remain checked. The controlled public course
# fails before this repair and keeps its immediate Quit / exact durable checks.
# Arbitrary-thread entry points still post; the core validates request identity.
# No provider, credential, grant, mutation, publication or source-inventory
# change. Full/manifest pins bind these bytes; mutation 16d135419 stays exact.
# [[BPR-1715]] follow-up reseal (2026-10-21) over last matching main
# 683147056, linear. Pinned-scope change: github_application.cpp's
# BUILD_TESTING --start-review-course probe expects the refusal sentence
# BPR-1715 gave a review GitHub refused ("Not published: GitHub refused this
# review.") instead of the older "Review was not published."; one test
# expectation and its comment. No product path, credential, grant, provider
# selection, publication, mutation, ledger write or source-inventory change.
# Full and manifest pins bind these bytes; mutation 16d135419 stays exact.
# [[BPR-1733]] reseal (2026-10-22) over last matching main 0986da0ff,
# linear. Pinned-scope change: a kept suggestion from an earlier head is
# carried to the new head or its change copied, on this card's own commit.
# Inside the manifest, github_application.cpp remembers the head and path
# each comparison it showed was read at, reads the private store's kept
# suggestions on other heads (private_review_workflow.cpp's
# earlier_suggestions, a read), asks the epoch host for the file's lines at
# the earlier head and the head (resolve_head_move_lines), presents what the
# core decides, and once the carried draft is saved deletes the earlier one
# in the store at the revision shown (discard_earlier_suggestion). Its
# BUILD_TESTING label-authoring probe follows RFC-BPR-0090: Shift+Return on a
# range opens the lines in the head pane, the probe types its change there
# and publishes the core's composed body. No review mutation, publication
# path, credential, grant, provider selection, ledger write or
# source-inventory change. Full and manifest pins bind these bytes; mutation
# 16d135419 stays exact.
# Upkeep reseal (2026-10-21): the only src/app change since the BPR-1721 note below
# is renaming its [[BPR-PENDING-AB]] comment tags to [[BPR-1721]]; no code change.
# [[BPR-1721]] reseal (2026-10-21) over last matching main cb5679683,
# linear. Pinned-scope change: a comment on a selection shows its lines, on
# this card's own commit. Inside the manifest only github_application.cpp
# changed, in the label-authoring probe's --line-draft-course: three phases
# drag three right lines through QWindowSystemInterface, right-click inside
# them, click the popup's "Add comment on lines a-b", publish the draft and
# read the thread the conversations show back. No product path, credential,
# grant, provider selection, publication, mutation, ledger write or
# source-inventory change. Full and manifest pins bind these bytes; mutation
# 16d135419 stays exact.
# Upkeep reseal (2026-10-21): the only src/app change since the BPR-1720 note below
# is renaming its [[BPR-PENDING-AA]] comment tags to [[BPR-1720]]; no code change.
# [[BPR-1720]] reseal (2026-10-21) over last matching main ae867c2db,
# linear. Pinned-scope change: Help > About BetterPullReview, on this card's
# own commit. The source inventory gains src/about_desktop.cpp and .hpp, in
# both lists: make_about_menu builds the page's facts (compiled version and
# configuration, the commit cmake/BprBuildIdentity.cmake records, the running
# Qt and system) and opens its two links through the core's named reference
# pages and the existing system browser opener, and its two installed legal
# documents with the platform viewer only while each is a regular file.
# profile_desktop.cpp makes the menu and attaches it to each window's bar and
# the empty desktop's, after the updater. No review mutation, publication,
# credential, grant, provider selection or ledger change. Only the manifest
# pin changes; the full-application and mutation 16d135419 pins stay exact.
# [[BPR-1718]] note (2026-10-21) over last matching main ad6908848: no
# reseal. The only src/app change is the BUILD_TESTING fixture server
# (github_public_path_fixture.cpp, outside the manifest): its batched Home
# review nodes answer submittedAt and its REST review submitted_at, for Home's
# verdict chip. No pinned-scope file changed; full, manifest and mutation
# 16d135419 pins stay exact.
# Upkeep reseal (2026-10-21): the only src/app change since the BPR-1715 note below
# is renaming its [[BPR-PENDING-Y]] comment tags to [[BPR-1715]]; no code change.
# [[BPR-1715]] reseal (2026-10-21) over the BPR-1714 upkeep note below,
# linear. Pinned-scope change: a whole review GitHub refused, or one an earlier
# version left unknown, settles as not published and gives its held drafts
# back, on this card's own commit. Inside the manifest: github_application.cpp's
# Discard review deletes only the drafts the review carries, at the revisions
# and head it carries them; review_submission_workflow.cpp gives a review frozen
# on an earlier revision no comment of the current one, starts a new review in
# place of a frozen one that keeps nothing, carries the comments pending now
# after a refusal, says why GitHub refused a review when its answer reads that
# a line cannot be placed, and says Discard deletes the summary only; the
# fixture adds a GitHub CLI-shaped 422 with GitHub's review words and a dropped
# review. The settling is the GitHub
# adapter's session and the core's definitely_unpublished, outside src/app. No
# credential, grant, provider selection or source-inventory change. Full and
# manifest pins bind these bytes; mutation 16d135419 stays exact.
# Upkeep reseal (2026-10-21): the only src/app change since the BPR-1714 note below
# is renaming its [[BPR-PENDING-W]] comment tags to [[BPR-1714]]; no code change.
# [[BPR-1714]] reseal (2026-10-21) over the [[BPR-1707]] commit below,
# linear. Pinned-scope change: the known C++ namespaces setting, on this
# card's own commit. Inside the manifest only github_application.cpp changed:
# the appearance dispatch routes apply_known_namespaces_intent to the host and
# reports invalid_known_namespaces to the Settings panel; a Namespaces
# presentation invalidation asks again for the C++ spans of the comparison on
# screen (reclassify_shown_comparison, which binds its blob reads again); and
# adopt_classification drops a span publication whose key is not the host's
# classification_key for it, without releasing the binding the newer request
# reads through. No credential, grant, provider selection, publication,
# mutation, ledger write or source-inventory change. Full and manifest pins
# bind these bytes; mutation 16d135419 stays exact.
# [[BPR-1707]] reseal (2026-10-21) over the [[BPR-1706]] commit below, linear.
# Pinned-scope change: a reread after publishing keeps the chosen file and a
# filter gesture during a refresh applies, on this card's own commit. Inside
# the manifest only github_application.cpp changed: the commit filter's
# comparison comes from core::filter_comparison_in_force, which keeps the
# accepted comparison while a refresh resolves the same request again, and a
# kept diff's refreshed publication asks again for its filtered line facts
# under the new request. No credential, grant, provider selection,
# publication, mutation or source-inventory change. Full and manifest pins
# bind these bytes; mutation 16d135419 stays exact.
# [[BPR-1706]] reseal (2026-10-21) over last matching main 7435c6901, linear.
# Pinned-scope change: a commit-filtered diff's line GitHub resolves nowhere
# as a line comment is published as RFC-BPR-0022's file comment, on this
# card's own commit. Inside the manifest only github_application.cpp changed:
# the editor's filtered placement names a file comment's own commit (the
# diff's left commit for a left line), and Start a review refuses such a line
# before asking the mutation host, which refuses it too. The publication path
# is the existing one: the mutation host re-decides the placement before any
# effect, and the provider admits the file comment through the core's
# admit_unresolvable_line with no native attempt. No credential, grant,
# provider selection or source-inventory change. Full and manifest pins bind
# these bytes; mutation 16d135419 stays exact.
# Upkeep reseal (2026-10-21): the only src/app change since the BPR-1712 note below
# is renaming its [[BPR-PENDING-T]] comment tags to [[BPR-1712]]; no code change.
# [[BPR-1712]] reseal (2026-10-21) over last matching main 6e8d0834f,
# linear. Pinned-scope change: Find in files' result menu opens a result in a
# new tab, on this card's own commit. Inside the manifest github_application.cpp
# and pr_search_controller.cpp/.hpp changed: the controller's file opener takes
# a result_tab (the active tab or a new one); the composition's opener applies
# the core's open_in_new_tab_intent and then activate_tab_intent on the tab it
# added (open_search_result_in_new_tab), and a tab a result opened keeps the
# search bound to the tab it was bound to (SearchOpening, SearchOpeningBinding
# and SearchCreatedBinding generalize SearchOpeningNoTab), so the results and
# the existing reveal stay. No credential, grant, provider selection,
# publication, mutation, ledger write or source-inventory change. Full and
# manifest pins bind these bytes; mutation 16d135419 stays exact.
# RFC-BPR-0089 M4 and [[BPR-1688]] reseal (2026-10-21) over last matching main
# ab1698599, linear. Pinned-scope change: comments from commit-filtered diffs,
# filtered Find in files and the window's kept filters, on this milestone's
# own commit. Inside the manifest only github_application.cpp changed: the
# window asks the epoch host for a filtered diff's line facts, hands the
# core's placement (head line, or the diff's earlier commit after the
# editor's warning, or a refusal before Publish) to the editor and to the
# mutation host's source observation, places review threads on a filtered
# diff with core::filtered_thread_line, refuses a placed line in a pending
# review, composes the mutation host for the new workbench generation after a
# filter gesture, binds Find in files to the filtered set, and keeps and
# restores the window's commit and file filters through the restoration host.
# The publication path is the existing one: the mutation host re-decides the
# placement from host facts before any effect, and a commit outside the
# accepted chain is refused before any write. No credential, grant or
# provider selection change. The BUILD_TESTING commit-filter-comment-v1
# fixture adds one host-label case, its enum and evidence fields
# (github_application_fixture_access.hpp) and its descriptor and wire
# (github_public_path_fixture.cpp), outside the manifest. Full and manifest
# pins bind these bytes; mutation 16d135419 stays exact.
# Upkeep reseal (2026-10-21): the only src/app change since the BPR-1702 note below
# is renaming its [[BPR-PENDING-Q]] comment tag to [[BPR-1702]]; no code change.
# [[BPR-1702]] reseal (2026-10-21) over last matching main 1694a7f37,
# linear. Pinned-scope change: a read file the reviewer leaves returns to the
# Files drawer, on this card's own commit. Inside the manifest only
# github_application.cpp changed, in clear_diff_selection: when the retained
# receipt it drops had withheld its file from the actionable order, it asks the
# workbench host's existing retain_actionability to restore that file, as an
# observation without the receipt would answer. No credential, grant, provider
# selection, publication, mutation, ledger write or source-inventory change.
# Full and manifest pins bind these bytes; mutation 16d135419 stays exact.
# Upkeep reseal (2026-10-21): the only src/app change since the BPR-1701 note below
# is renaming its [[BPR-PENDING-P]] comment tag to [[BPR-1701]]; no code change.
# [[BPR-1701]] reseal (2026-10-21) over last matching main 0b10fa71a,
# linear. Pinned-scope change: Seen and Read uncheck from the review bar, on
# this card's own commit. Inside the manifest only github_application.cpp
# changed, in one condition of the --review-ledger-composition-probe
# automation course: an automatically seen file's Seen action is now checked and offers
# Mark unseen, so the probe requires it checked rather than disabled. No
# product composition, credential, grant, provider selection, publication,
# mutation or source-inventory change. Full and manifest pins bind these
# bytes; mutation 16d135419 stays exact.
# Upkeep reseal (2026-10-21): the only src/app change since the BPR-1699 note below
# is renaming its [[BPR-PENDING-M]] comment tags to [[BPR-1699]]; no code change.
# [[BPR-1699]] reseal (2026-10-21) over last matching main d8a3f6b97,
# linear. Pinned-scope change: Mark all comments as read, and all from one
# author, on this card's own commit. Inside the manifest only
# github_application.cpp changed: a post's read observation carries its
# author's identity (core::review_author_of over the login), and the window's
# Mark All Comments as Read and Mark all from <login> as read reach one new
# composition method that observes every conversation of the accepted
# snapshot, hands them to the review ledger host's mark_conversations_read,
# and publishes the read state once the batch is durable. No credential,
# grant, provider selection, publication, mutation or source-inventory change;
# read state is the private ledger's and nothing reaches GitHub. Full and
# manifest pins bind these bytes; mutation 16d135419 stays exact.
# RFC-BPR-0089 M3 reseal (2026-10-21) over last matching 8e73e5cb3 ([[BPR-1693]],
# on main ef5939561), linear. Pinned-scope change: M3's split per-commit diffs
# and force-push rematch, on this milestone's own commit. Inside the manifest
# only github_application.cpp changed: the workbench observes a split file's
# per-commit diffs from the commit filter, the window's strip chooses one by
# position through the workbench host, the Overview row combines the diffs'
# own receipts, a split file stays conservatively actionable, and a rematch
# gesture reaches the local commit filter. The BUILD_TESTING
# commit-filter-split-v1 fixture adds one host-label case there, its enum
# (github_application_fixture_access.hpp) and its descriptor
# (github_public_path_fixture.cpp), outside the manifest. No credential, grant,
# provider selection, publication, mutation or source-inventory change; every
# diff is read through the epoch host's existing selected-diff path, at
# commits the chain's touch job authorized. Full and manifest pins bind these
# bytes; mutation 16d135419 stays exact.
# [[BPR-1693]] reseal (2026-10-21) over last matching main 111235987, linear.
# Pinned-scope change: RFC-BPR-0089's open-on-request for a generated file set
# aside, on this card's own commit. Inside the manifest only
# github_application.cpp changed: the window's generated rows ask for one
# set-aside file by path; the composition builds its workbench entry from the
# commit filter's own record, retains it outside the walk through the
# workbench host's existing source-file path (as a Find in files result is)
# and opens it in the active tab. No credential, grant, provider selection,
# publication, mutation, ledger write or source-inventory change; the diff is
# read through the epoch host's existing selected-diff path. Full and manifest
# pins bind these bytes; mutation 16d135419 stays exact.
# [[BPR-1692]] reseal (2026-10-21) over last matching main 5e4c77757, linear.
# Pinned-scope change: RFC-BPR-0089 M2's linguist-generated read, on this
# card's own commit. Inside the manifest only github_application.cpp changed:
# the window asks the epoch host for the accepted head's .gitattributes once
# per head and directory set, beside the chain's tree diffs, adopts the core's
# rules into the commit filter, and publishes why they were not read to the
# Overview. The BUILD_TESTING commit-filter-attributes-v1 fixture adds one
# host-label case there, its enum (github_application_fixture_access.hpp) and
# its descriptor (github_public_path_fixture.cpp), outside the manifest. No
# credential, grant, provider selection, publication, mutation or
# source-inventory change; the read is a local cache lookup, or a sparse
# review's exact blob read through its own lease. Full and manifest pins bind
# these bytes; mutation 16d135419 stays exact.
# Upkeep reseal (2026-10-21) over last matching 1204a7194..7babe3593, linear.
# Pinned-scope commits: RFC-BPR-0089 M2 8e296033d ([[BPR-1689]]..[[BPR-1691]]).
# Only github_application.cpp changed inside the manifest: it wires the window's
# file-filter and generated-file gestures to the core commit filter, persists a
# generated mark through the review ledger host before adopting it, and
# publishes filtered and set-aside counts to the Overview. No credential, grant,
# provider selection, publication, mutation or source-inventory change. Full and
# manifest pins bind these bytes; mutation 16d135419 stays exact.
# Upkeep reseal (2026-10-21) over last matching 1ce075976..57cae1ba7, linear.
# Pinned-scope commits: RFC-BPR-0089 M1 95aa8a5b5, 50eb0cdfc and 1b0f28183
# ([[BPR-1685]]..[[BPR-1687]]), and [[BPR-1680]] 87d04edf2 and c229ebd9d.
# Only github_application.cpp and main.cpp changed inside the manifest. M1
# composes a window's local commit filter: the Commits tab and chips, the
# filtered file set the workbench and Overview observe, its own ledger receipt
# key per filtered diff, chain tree reads through the epoch host, suspended
# thread marks and a commit page opened through the window's browser. 1b0f28183
# adds only the BUILD_TESTING commit-filter-v1 fixture host label and main.cpp
# selection. BPR-1680 adds a monotonic clock for logged refresh durations, the
# discovery host's diagnostic log, and Home discovery lanes the provider runs a
# CLI account's discovery reads on, drained at close, beside the window reads
# home_loading_yields no longer waits for. No credential, grant, provider
# selection, publication, mutation or source-inventory change; the fixture and
# CLI-runtime edits in the range are outside the manifest. Full and manifest
# pins bind these bytes; mutation 16d135419 stays exact. Reviews are on the
# owning cards; the original drift refusal precedes this renewal.
# [[BPR-1675]] final integration over main b0e643531 and measured67cfff2dc:
# source review retains main's window-owned Home read-lane yield and local Ignore
# intent, plus its unset-by-default recorded repository-refusal fixture field.
# M3 adds only the previously reviewed enterprise preview observation guard in
# main.cpp; provider/vault/grant/publication/mutation ownership and source
# inventory are unchanged. The main full-source pin was already stale against
# its committed126f2ede8 source; the retained initial refusal precedes renewal.
# Full and combined manifest pins bind the explicitly reviewed merged bytes;
# mutation16d135419 stays exact. This source-only reconciliation grants no native
# or producer credit for main's concurrent product changes; measured products
# stay bound to67cfff2dc. See BPR-1675 integration receipt and measured report.
# [[BPR-1675]] f25a65274 observation repair over the previously reviewed c946:
# the sole src/app delta prevents an enterprise preview probe from latching
# transient signed-out mode before its typed refusal. Terminal refusal and zero
# effects remain required; ordinary preview observation and all provider/vault/
# grant/publication/mutation ownership are unchanged. The exact affected public
# courses pass on Mac/Windows and current Ubuntu. Only this combined manifest
# pin changes; full github_application and mutation seals stay exact. The Ubuntu
# drift failure is retained; source/compiler hostile controls run fresh.
# [[BPR-1675]] merged BPR-1676 promo9b9a4acc6 over native f230a8e5f:
# scoped source review finds only a BUILD_TESTING fixture enum/host label,
# fixture metadata and main fixture/attended-read-state selection. No new
# source inventory, credential, provider, grant or publication admission.
# Full/production-manifest seals bind this exact merged source; mutation
# production digest16d135419d39078bc3993b4e1cb80226e190edce50cd24d9456843066043a0e1
# remains unchanged. Initial drift failure and review are retained on BPR-1675.
# [[BPR-1652]] scoped reseal2026-10-19 over last matching76974f7d0..1cdaf43b3.
# Sole pinned-scope commit1cdaf43b3 corrects the Windows BUILD_TESTING
# UIforETW capture's stale caption index from lead+18 to its already asserted
# lead+16. The18-action exact row remains whole; no product/provider/credential,
# grant, publication or mutation admission changes. Source guard review and
# own-image RVA/linker-map diagnosis are recorded on BPR-1652 and the release
# repair ledger. The raw source-input mismatch is retained for explicit Mac
# reconciliation. Full/manifest pins bind these exact bytes; production mutation
# branch16d135419d39078bc3993b4e1cb80226e190edce50cd24d9456843066043a0e1
# remains unchanged. The original stale seal refusal is preserved.
# RFC-BPR-0086 Energy engineering: profile-owned30/60s metrics sampling and
# final-interval drain reuse the existing Insights effect lane. Git configuration
# carries only a passive observer; native support reads self counters and retains
# an exact Windows child handle. Unix child usage is explicitly unavailable while
# Qt's existing reap discards rusage. No launch, credential, grant, target, actor,
# provider or publication admission changes. Fixture metrics are a typed local
# observation port. Self-review and direct evidence are recorded in RFC0086.
# Full/manifest seals are renewed; the production mutation branch is unchanged.
# RFC-BPR-0086 review-stats integration over completed main a7e99406c.
# Reviewed pinned-scope commits00e0eb7dc and c94d3b59e: composition captures
# immutable PR/head/file facts for passive statistics, passes them to existing
# review hosts, and preserves the same grants, provider and publication admission.
# Workflow selections retain only that optional capture. Core factory wiring from
# BPR-1619 and bounded pinned tour reads from BPR-1377 remain as reviewed below.
# No app source inventory addition. The production mutation branch adds only
# the passive capture argument to its existing host constructor; grant, target,
# actor, source, provider and publication admission are unchanged.
# Combined full/manifest pins bind these integrated bytes; evidence is on
# RFC-BPR-0086 and its existing continuation.
# BPR-1609 correction integration over 318c3f5aa: adopted completed BPR-1618
# removes the regex adapter include and uses the core singleton. Recorded main
# audit/Find evidence is preserved; Insights UI-only correction adds no app source.
# Combined full/manifest digests renewed; mutation branch remains unchanged.
# RFC-BPR-0086 github-budget over 55ea64194, scoped engineering self-review.
# Profile composition attaches a passive numeric Insights recorder to the existing
# provider factory and diagnostic stream, drains it on the owned Insights effect
# lane, and hands semantic profile colors to the existing singleton window.
# The gh discovery runner and anonymous wrapper retain that observer. The new
# insights-budget-v1 fixture uses the real private store; no grant, credential,
# publication admission or executor authority is added. Reviewed provider/native/
# gh changes collect at their existing send/start/terminal boundaries only.
# Full application/production manifest resealed; mutation branch is unchanged.
# Evidence: docs/verification/RFC-BPR-0086.md and the milestone continuation.
# BPR-1377 reseal 2026-10-18: tour_remote pins the unique active signed-in
# keyring-held login through existing bounded inventory on SourceEffects before
# shared strict transport admission. It adds no token capture, global account
# switch, provider operation or publication grant. The source worker uses the
# epoch host's per-call sealed pinned-source read; ordinary blob grants stay
# unchanged. Sparse source positive/old-call negative and live CLI add/show pass.
# Current shared app scope also contains BPR-1619's xdiff factory/header move
# from adapter to core in github_application, cycle_session and review_cycle_desktop,
# plus removal of the old app link. Those wiring deltas were inspected for this
# deterministic reseal; engine acceptance belongs to BPR-1619. The production
# mutation-branch digest stays unchanged. No sibling work is reverted.
# RFC-BPR-0086 accepted-milestone integration over main 614c6bbb7.
# Self-reviewed the combined app scope: Insights keeps its profile-owned local
# lane/window, initialization/shutdown and existing export/cache gestures; main's
# accepted BPR-1605 reads carried draft comments into preflight with draft-ID
# deduplication and a carried-comment-aware cache. The changes are disjoint.
# No provider, credential, grant, executor or publication path is added here.
# Full github_application and production mutation-branch pins stay unchanged;
# only the combined production-source manifest is resealed.
# Evidence: docs/verification/RFC-BPR-0086.md, acceptance round 3.
# RFC-BPR-0086 first milestone reseal over 0378ab977..the window-and-storage
# candidate. Scoped engineering self-review: one profile-owned local Insights
# lane/window, initialization and shutdown drain, existing Settings/export
# gestures, and accepted cache-label persistence. No provider or mutation
# admission changes; the production mutation-branch digest is unchanged.
# Evidence and exact owner shell acceptance: docs/verification/RFC-BPR-0086.md.
# BPR-1603 reseal 2026-10-18 over d163c9109 (origin/main 937f96880).
# Self-reviewed the full src/app delta: the reply workflow persists bounded
# quoted drafts through the existing serialized submission host and journal,
# discovers before enabling edits, coalesces edits by generation, and requires
# the existing admission and one-use child grant for an explicit Send. It
# preserves possibly-transmitted records for reconciliation and drains saves
# on shutdown. github_application composes the workflow with the existing
# factory/authority and invalidates it with the selected PR/session. No new
# transport or credential path. The adapter uses the existing exact-PR reread
# and witnessed create_issue_comment; changed-head and lost-response controls
# pass. Tests-on main/fixtures seed synthetic PR comments, associate file
# posts to a review, and open a local draft without sending. The production
# mutation-branch pin remains exact; full and manifest pins bind this tree.
# BPR-1594 landing reseal 2026-10-18 over origin/main 3b68c4394.
# Reviewed merged main.cpp delta: tests-on attended Agent fixture course,
# profile selection and drawer gesture only. No provider, credential, grant,
# publication or mutation authority changes. All three printed pins match
# the accepted candidate on this merged tree; mutation pin unchanged.
# BPR-1593 landing reseal 2026-10-18 over main a7a45e1c9. Reviewed the
# merged src/app delta: main.cpp adds only the tests-on attended tour fixture
# gesture course and command-line selection. No grant, credential, provider,
# publication or mutation authority changes. All three printed pins match
# the accepted candidate on this merged tree and remain exact.
# BPR-1602 reseal 2026-10-18 over main cd80d1ab3 (pins matched at
# 4d6b63e86): 08d50fe93 adds Settings Troubleshooting resets. Reviewed in
# github_application.cpp: the config's one-shot DataReset, the startup root
# step that runs run_profile_data_reset under the held lease after
# recover_release_profile and before any store opens (refusing the start
# when a reset stopped part-way), the report handed to the runtime, and the
# fixture desktop's factory threading the reset. profile_desktop.cpp closes
# the profile's windows through the management host and reopens it with the
# request; main.cpp offers the recovery dialog after a failed start and
# restarts the desktop in process. New src/profile_data_reset.cpp calls only
# core admission, release_maintenance_files::reset and the queue store. No
# provider, grant, credential, publication or mutation-branch change; the
# mutation digest is unchanged. The source inventory lists the new runner.
# BPR-1601 landing 2026-10-17 over main 887feb0ed: PR comments and review
# summaries use the existing conversation receipt and core read/unread policy.
# Reviewed accepted-snapshot identity construction, singleton/native-thread
# cache bounds, seen-before queue counts, durable-before-publish callbacks,
# and the incremental Status count. Test-only server overlays supply exact
# summaries/comments for the composed restart/edit course. No provider write,
# grant, credential or production mutation branch changes; mutation pin stays
# exact. The full application and production manifest pins bind this tree.
# BPR-1598 landing 2026-10-17 over main 57f335411: strict-tidy cleanup in
# src/app preserves the fixture name used after startup, returns an optional
# without dereferencing/re-wrapping it (including the tour-stop staging
# argument), names fixture lambdas consistently,
# reserves fixture baseline storage and rejects an unknown fixture stage.
# Reviewed directly for the ordered handoff. The full application and mutation
# branch pins remain exact; only the production app manifest pin changes.
# BPR-1572 landing 2026-10-17: resealed over main ca55c0a71 on the merged
# tree. Main's recorded composition reviews cover refresh feedback and the
# accepted progress track. This branch adds only the reviewed stacked fixture
# dispatch, fixture answers and course access in src/app. Production mutation
# authority is unchanged; its digest remains exact. Fable/Astra round-four
# decisions and completed conditions are recorded on BPR-1572 and the handoff.
# BPR-1597 landing 2026-10-17 over main c38432adf: Settings pages move
# under existing typed owners, with no src/app, persistence or grant change.
# Main's three composition pins remain exact on the merged tree.
# BPR-1578 landing 2026-10-17: main 376a4190b merged into the accepted
# progress-track candidate. The app delta remains test-build bar order and
# private-progress coordinates only; no publication or credential authority
# changes. All three pins from 5f3a96baa remain exact on this merged tree.
# BPR-1582 reseal over main 1e72d4d60 (whose pins main matched): the only
# production-app change is 2a789264e in github_application.cpp, the Threads
# row menu's Mark as Unread: the window's new unread dispatch and
# mark_conversation_unread, which builds the accepted thread's post
# observation as Mark read does and hands it to the review ledger host, whose
# core decision moves the private seen cursor. No provider, grant, credential,
# publication or mutation-branch change; the mutation digest is unchanged.
# Reviewed by its implementer; the composed course is
# application_profile_multi_window_public_path's Mark as Unread section.
# BPR-1527 scoped Windows0.2.2/WSL follow-up over7aaf (Mac6747/c233 unchanged):
# a confirmed kept-reply action can arrive while a reread reopens private storage.
# Retain its core-admitted command/CAS with the exact shared owner and consumer;
# restore completion drains it, and scope replacement retires it. A later explicit
# action on the same draft replaces its pending intent without recomputing a CAS.
# The existing deleted-thread public course holds the private effect lane across
# confirmation, then requires the original durable discard/zero-send evidence.
# Native format22 and print-only digests were observed; print mode verifies nothing.
# Focused native/control/static acceptance remains pending in the BPR-1527 checkpoint.
# Production mutation-branch pin, provider, grant and credential admission unchanged.
# BPR-1527 Windows0.2.2 scoped review after Mac boundary c233c7683:
# github_application.cpp changes only test-build failure observations for
# the already-scoped UIforETW probe and label authoring. No new source body,
# credential, grant, provider or publication branch is executed or admitted.
# Exact native print-only digests were observed after format22; that mode
# verifies nothing. Self-review/checkpoint retain the pending native checks.
# The production mutation-branch digest below is unchanged.
# BPR-1526 scoped self-review over 8f0d25418: c3009b2b0 is the only
# committed production-app manifest change, plus nine behavior-preserving
# local/parameter naming repairs in this upkeep slice. The first-save workshop
# press retains its exact placeholder revision/scope until its own accepted
# materialization supplies a durable ID. The immutable press queues before
# acknowledgement can emit later edits; failed/retired saves never launch.
# No provider, grant, credential, publication or mutation-branch change.
# Direct public-path positive/cut/injection evidence and exact source review
# are on BPR-1526 and docs/verification/BPR-1526-macos-workshop-action.md.
# Combined BPR-1506/1515 fixture correction: identify the inverse Accept
# gesture from the persisted Accepted receipt, not the old ordinal that
# included a manual Seen press. Production paths and mutation pin unchanged.
# Final testing-note integration through bcb2eed79. BPR-1506 submits the
# existing Seen command only after exact visible Review material admission,
# with generation, tab and authority rechecks. Hidden/stale loading cannot mark
# a receipt; explicit unread remains until reopening. No new publication grant,
# provider route or production mutation branch change. Earlier integration
# review notes below remain bound to their respective changes.
# BPR-1517/1518 integration reseal: ordinary file navigation retains the
# exact accepted order through source failure, while entry navigation still
# restores reading. Cursor-relative change navigation uses the accepted semantic
# index and current source line. No new provider, grant or mutation authority.
# BPR-1515 integration reseal over95041e84d: only test-build fixture key
# delivery replaces the existing Accept gesture; repeated Accept proves no
# revoke and the explicit button still exercises the inverse. No production
# composition, grant, provider or mutation-branch change.
# BPR-1521 scoped self-review over 3c24b9ec8: the only new production
# manifest change is the shared draft subscription in github_application.cpp.
# An unused runtime placeholder follows a committed active draft only at the
# exact scope/anchor/reply, with no candidate or save pending and matching
# displayed selection. Authored concurrent first saves retain their identities.
# Existing durable-before-projection and first-save acknowledgements are kept;
# no publication, provider, credential, grant or mutation branch change.
# Testing-note integration reseal, baseline583eb4689 through e8884475d.
# BPR-1507 changes only the fixture acceptance presenter; BPR-1514 projects
# pending startup and gates competing setup intents through core policy.
# Existing publication grants/target checks and mutation branch are unchanged.
# Main P0 public recovery/restart contracts passed before these integrations;
# combined composition and UI checks are recorded in the coordinator checkpoint.
# BPR-1520 r2 self-review: baseline9f2ebc10a through this product slice.
# No intervening pinned-scope commits. Changed manifest paths are
# github_application.cpp, private_review_workflow.cpp and .hpp: runtime-only
# line selection, atomic first-save acknowledgement, scoped saved-history actions,
# and exact original held-comment settlement after head advance. Store restoration
# discovers retained held keys for the existing read-only lookup. No new remote
# grant or provider route; actor/PR/head/content checks remain. The production
# mutation-branch pin is unchanged. Review and composed evidence are on BPR-1520.
# BPR-0573 r3 storage compatibility merge: reviewed 829291bec plus main89f23eeec.
# Main's schema5 recovery, durable draft/lineage and tour semantics are preserved.
# Against main, github_application.cpp changes only the exact approved PR2 target
# constants and passes the existing diagnostic logger after the private lineage
# port. The provider retains both independent dependencies; diagnostics confer
# no authority. Off-hunk classifier2 and its exact scope remain unchanged.
# These three pins bind that reviewed composition. RFC0022 r4 records owner
# acceptance without an independent report.
# Main 90b79f0 adds the GitHub CLI runtime and tour sources to this inventory;
# the full application and source-tree digests below bind the merged files.
# BPR-0573: reviewed dc1a836b..this bounded target reassignment. Only the
# compiled attended target changes from PR1 to owner-designated PR2; application,
# bootstrap and independent observer agree on exact IDs, refs and OIDs. No grant,
# transport, classifier or production mutation-branch change. The touched TU also
# receives mechanical format of inherited tour fields/branches. Card records review.

# BPR-1016 final integration reseal: reviewed main 93235bee plus tour 22763dc10.
# The only additional production-manifest changes against main are 5ca53796a
# (review_submission_workflow.cpp) and 54ba51bf8 (github_application.cpp).
# Initial descriptions use the same core mapped-byte preservation as maintenance;
# the separately previewed initial backlink retains its existing journal/grants.
# Insert-after resolves the selected stable stop identity and supplies it to core
# position validation. Neither change adds publication authority or private input.
# Retain main's BPR-0587 private workflow inventory and recorded M8 source review;
# WritingOnly still rejects private-run intent. Main's later M10 fixes 8cd73d96c
# and d4404fc44 are also covered: their source and independent review are recorded
# in RFC-BPR-0023.md. They restore interrupted runs and reconcile pending private
# publications through the existing host with GET-only recovery and no new POST.
# The mutation branch matches main exactly; reseal its stale M8 pin for the
# already-reviewed repair_pending_publications() call added by 8cd73d96c.
# Prior exact runtime/native evidence and the owner scope amendment are recorded
# on BPR-1013/BPR-1016 and in the RFC-BPR-0039-0043 qualification ledger.
# BPR-0587 reseal: scoped engineering self-review of 5b9080fb6..this M8 slice.
# Intervening pinned-source commits 9b71342a0, 1d84e59c2 and 1ef7a98c3 retain
# their recorded BPR-1007/BPR-1010/BPR-1011 writing acceptance; WritingOnly
# still bypasses private-run admission. M8 composes the existing private store,
# run and publication hosts, serializes durable draft updates, preserves frozen
# retry identities and waits for the durable receipt before shared refresh.
# The production provider gains only its storage lineage port. Existing user
# grants, target/actor/source checks and sole mutation-host publication remain.
# Add the private workflow to the production manifest and its testing-only
# executable course to inventory. This is self-review, not the independent
# RFC review reserved for M10. Evidence: RFC-BPR-0023.md M8 section.
# BPR-1002 staged-publication merge reseal: reviewed f1e0c652..ef05b58c.
# Pinned-scope commits: 36d535eb, 505c942d, 189d24f9, e0a870b3,
# 0fb249a2 and ef05b58c. Tour preview freezes the exact plan and grants
# bounded child operations through the existing host/journal; pending live
# operations remain unavailable until qualified. Diagnostic Help exports use
# the existing current-window/profile maintenance owner. Production lambda
# capture repairs preserve the fixture branch. Five changed manifest paths
# are reviewed on BPR-1002 and in the RFC-BPR-0039-0043 merged checkpoint.
# No new source inventory entry; ordinary mutation-branch pin is unchanged.
# BPR-0998 github-round-trip integration: reviewed dc1a836b..18c23cdd,
# pinned-scope product commits f44c95dd and 757e8b9b, plus this merge resolution.
# Raw tour imports and exact commit/path/side/range navigation retain existing
# source admission. Conversation refresh runs before tour range restoration;
# imports neither publish nor authorize an operation. Raw description selection
# retains exact UTF-8 spans. A duplicate codec include is removed. Other main
# composition/diagnostic behavior is retained. Mutation-branch pin is unchanged.
# Review and direct combined evidence: BPR-0998 and RFC-BPR-0039-0043 ledger.
# BPR-0571 merged integration: reviewed b8543067..a3533195, product commits
# f442ca2b, 791decea, 75700f4d and 3fd1224e plus their merge resolutions.
# Tour authoring/navigation and diagnostic controls add no publication grant;
# exact source admission, semantic normalization and receipt navigation remain.
# Add both diagnostic control files to the explicit inventory and manifest.
# Production mutation-branch digest is unchanged. Review is on BPR-0571 and
# the M3 integration checkpoint; this reseal does not change product source.
# BPR-0571: reviewed e12e0fda..this composition slice. Successful receipts
# request the ordinary server read and select only the matching accepted
# identity under the exact target, actor and comparison. No new write
# authority; production mutation branch is unchanged. The fixture replaces
# its root shortcut with the real protocol session and SQLite.
# BPR-0569 reseal: reviewed 2deb889b..this normalization slice. Accepted read
# and retained material now project file subjects through the strict codec;
# exact selected context is rechecked and clears on source replacement. The
# M1 handoff/material fields and clone helper are available in both graphs,
# repairing a tests-disabled compile failure. The mutation branch is unchanged.
# Source review and direct regression evidence are recorded on BPR-0569.
# BPR-0563 reseal: 9ee0350f..this product slice; reviewed host source
# port, accepted catalogue/material capture and both ctor connections. No other
# pinned-scope edits; grant and mutation authority paths are unchanged.
# BPR-1202 hand-back49: reviewed eb14f40e delta; only test-build bar order changes.
cmake_minimum_required(VERSION 4.4.2)

function(bpr_require_literal_count source_variable token expected description)
    set(bpr_count_source "${${source_variable}}")
    string(LENGTH "${bpr_count_source}" bpr_count_before)
    string(LENGTH "${token}" bpr_count_token_length)
    if(bpr_count_token_length EQUAL 0)
        message(FATAL_ERROR "${description} uses an empty inventory token")
    endif()
    string(REPLACE "${token}" "" bpr_count_without "${bpr_count_source}")
    string(LENGTH "${bpr_count_without}" bpr_count_after)
    math(EXPR bpr_count
        "(${bpr_count_before} - ${bpr_count_after}) / ${bpr_count_token_length}"
    )
    if(NOT bpr_count EQUAL expected)
        message(FATAL_ERROR
            "${description} literal inventory expected ${expected}, found "
            "${bpr_count}: ${token}"
        )
    endif()
endfunction()

# BPR-1015 qualification correction: reviewed from 2e2aca96 through this
# recorded correction commit. Only review_submission_workflow.cpp changed inside
# the production app manifest: initial and maintenance descriptions now share
# core mapped-byte preservation. Initial backlink remains separately named and
# journaled; no operation support/grant/actor/head/recovery boundary changed.
# Source-line mutation branch hash is unchanged. The final integration also
# found the full-application pin still at the manual-maintenance revision.
# Reviewed 5b9080fb..2e2aca96: 9b71342a, 1d84e59c, 1ef7a98c are the only
# intervening manifest-source commits, all in github_application.cpp. Profile
# hook restore, explicit bounded writing/cancel/delivery and visible per-stop
# completion retain captured identity; no implicit inference or publish grant.
# Prior scoped-AI/outline reviews and the final real-process course are recorded
# on BPR-1015 and the pack ledger. Reseal their unchanged composed source here.
# Resealing the pinned digests ([[BPR-0383]], [[BPR-0819]])
#
# The three digests below are review tripwires, not build products. A pin that
# goes red asks for a review of what changed inside its scope; it does not ask
# for a new number. Reseal only in the commit that records that review, on the
# card that owns the change, listing every commit the new values cover:
#
#   1. List the changes since the pins last matched:
#        git log <last-matching-commit>..HEAD -- src/app/src/github_application.cpp \
#            <each file in bpr_production_app_sources>
#   2. Confirm each change inside a pinned scope has a recorded review, and
#      record one for any that does not.
#   3. Print the current values, which verifies nothing:
#        cmake -DBPR_PRINT_PINNED_DIGESTS=ON -DBPR_SOURCE_ROOT=<checkout> \
#            -P tests/end_to_end/application_review_round_trip_composition_contract.cmake
#   4. Paste the printed values into the pins, and name the reviewed range in the
#      commit.
#
# BPR-1670 main integration reseal over matching d989efb5c..15bb7f8a4.
# Pinned-scope commits: 6ad4c4f28 (BPR-1671) and 3ae1183f7 (BPR-1672),
# release_update_desktop.cpp/.hpp only. Reviewed offered-access restoration,
# reentrant RequestedBinding ordering, unchanged readiness/capability/closing/
# generation guards, and fixed-label SHA-256 public binding in the existing
# SQLite format. Native public access stays first with the same anonymous
# release-only transport. No review mutation/publication or inventory change.
# Review is recorded on both owning cards and the BPR-1670 integration ledger;
# their existing acceptance remains scoped to their own candidates. Only the
# manifest pin changes; full-application and mutation-branch pins stay exact.
# Original early mismatch is retained; print-only mode is not verification.
#
# BPR-1669 reseal, found by BPR-1670: c5a906abe..aac127fa4 contains only
# c0d404c2c inside the pinned app scope (release_update_desktop.cpp). Reviewed
# the public release-only account, missing-account fallback and preservation
# of the existing generation, closing and capability guards. No review
# mutation or publication path changed. The full-application and production
# mutation-branch pins are unchanged. Review/evidence are recorded on BPR-1669
# and docs/verification/BPR-1670-windows-suite-runtime.md; retain the original
# failed cold run as evidence, rather than treating a printed digest as proof.
#
# BPR-1006 reseal: 6cff4914..22fd2150 (b56cad7e, 9a7ba459, 22fd2150).
# Reviewed Tour update preview/journal handoff, same-owner observed conflicts,
# immutable original versus proposed source and deliberate retarget/reaffirmation.
# Existing source-line mutation branch is unchanged. Production-mode syntax checks
# and the complete manual production-port course are recorded in the pack ledger.
# BPR-1210 reseal: 44bd1266..the BPR-1210 product commit. No intervening
# pinned-scope changes. Private ledger presentation/CRUD reuses its existing
# lifetime and capabilities; one owned item enters the core's revisioned edit
# through Comment's exact target. No publication or source deletion branch is
# added. Full changed-source review and composed negative controls are on BPR-1210.
# BPR-1132 reseal: fb4b8364..the BPR-1132 product commit. Per-window
# document-tab composition, exact restoration and gesture precedence, shutdown,
# and test-build UIforETW tab-strip geometry. No intervening pinned-scope commit;
# mutation branch unchanged. Full source review recorded on BPR-1132.
# BPR-0898 reseal: 0994aa0e..12830eff changes only the application failure
# predicate for a kept review draft. target_unavailable and malformed_response
# now retain the draft under the same exact selected comparison as unavailable;
# the operation still fails and no retry or mutation is started. The production
# mutation branch is unchanged. Review and focused refusal evidence are on
# BPR-0898; no other commit in this range changed the pinned application scope.
# BPR-0972 reseal: a2fded0c..90e21d0c, pinned-scope commits bcb982da,
# c55456e4, 0a8f9d79, 9ed1ce7d, 10b96b31 and 2cb4c2be. Retained receipt,
# conversation and queue indexes/deltas, exact sibling invalidation, indexed
# selection membership, same-head Overview history and read-only counters.
# The full review is recorded on BPR-0972; authority/lifetime and durable-before-
# publication guards remain. Mutation-branch digest is unchanged. No other
# production app manifest source changed in the reviewed range.
# BPR-1208 reseal: no intervening application change after30748ed6. The
# current slice joins durable defects to exact accepted thread identities with
# a per-snapshot cache and adds a tests-only composed menu assertion. No
# publication, authority, provider or storage path changes; mutation pin
# unchanged. Source review is recorded on BPR-1208.
# Combined main reseal: fe8a6937..8f36d1cb; pinned-scope product commits
# 9a22bbfe, 3fa6d614, 48543054 and 6fe51128, plus the formatting-only
# alignment in this reseal. The baseline matches BPR-0972 source 149704df.
# Reviewed Notes/defect snapshot cache invalidation, exact draft promotion,
# unchanged whole-review admission and retained FilesDelta/queue/epoch paths.
# Full review and 33-commit mapping: docs/verification/combined-main-integration-
# 2026-09-26.md and .json; owning card BPR-1202. Mutation pin unchanged.
# Follow-up strict tidy: the only additional pinned-source edit renames the
# test-only DEFECT_COMMENTS_STALLED lambda parameter stream to aStream and its
# one use. Runtime behavior and mutation digest remain unchanged. The combined
# review record includes this exact delta and the focused replay.
# BPR-1207 hand-back32 reseal: 9bd38bf1..this product slice; no intervening
# pinned-source change. Promotion preflight admits an editable left-side thread
# before Comment converts it to a reply; the following exact scope/anchor and
# Publish guards remain. Host target admission is shared with core and Qt,
# preserving left replies and refusing left roots. Full review is on BPR-1207;
# production mutation composition is byte-identical.
# Every value is the SHA-256 of LF-normalized bytes. The functions below compute
# them, and both the checks and step 3 use those same functions.
# BPR-0242 reseal: d6dfca2b..2a64e901 is reviewed in
# docs/rfcs/plans/rfc-bpr-0008-owner-closeout/composition-reseal-review.md.
# BPR-0933 reseal: 12d032cf changes only the read opt-in's presentation in
# github_application.cpp; the review is recorded on BPR-0933.
# BPR-0982 reseal: the main.cpp self-test CLI course follows the discovered
# CLI entry (BPR-0909); no product path changes. Review recorded on BPR-0982.
# Merge reseal (codex/rfc-0018-0026-closeout into main): the branch's
# 8d072d14 and 9210330a are reviewed on BPR-0976 and BPR-0977; 6f8e44e4
# changes only the BPR_BUILD_TESTING fixture desktop factory, reviewed on
# BPR-0978; main's 12d032cf and 841999ad are covered above. No other change.
# BPR-0984 reseal: the BPR_BUILD_TESTING workbench composition probe ignores
# the window's observed geometry in its session-fallback comparison; no
# product path changes. Review recorded on BPR-0984.
# BPR-0987 reseal: the setup open reads a pasted link's typed location, keeps
# the pull request the window already shows instead of reading it again, and
# places the location after acceptance; a Recently viewed row of the shown pull
# request lands on Overview without a read. No credential, provider, vault or
# mutation path changes. Review recorded on BPR-0987.
# BPR-0987 correction: the pins above were taken before BPR-0987's last edit
# (a null accepted-host guard in the setup open, a review location that is not
# read explained as not read, and a file location placed only once the epoch
# holds the shown pull request's authority), so they never matched 17e40dc8.
# Resealed to the committed file; no other change. Review recorded on BPR-0987.
# BPR-0990 reseal: the profile runtime composes one personal discovery host
# over the authenticated provider's personal reads and its queue host, adopts
# the accepted account session on each presentation, reconciles saved
# repositories after each queue projection, and wires each window's Follow and
# discovery intents; a fixture-only accessor reports its evidence. No
# credential, vault, transport or mutation path changes; the mutation-branch
# pin is unchanged. Review recorded on BPR-0990.
# BPR-0975 reseal: f863ce1b restructured cli_read_offered_here so GCC 15
# would not reject cli_read_offered as unused in a non-Apple tests-enabled
# graph; that broke this pin and the composition contract's CLI gate shape.
# The correction restores cli_read_offered_here byte for byte and marks
# cli_read_offered [[nodiscard, maybe_unused]] with a comment instead. Every
# graph's answer is unchanged. No credential, provider, vault or mutation path
# changes; the mutation-branch pin is unchanged. Review recorded on BPR-0975.
# BPR-0983 reseal: the automated-test boundary marker gets one reader,
# inside_automated_test_boundary() in github_application.cpp, which reads it
# only in a tests-enabled graph and answers false otherwise. A profile's
# composition, release_update_desktop.cpp and profile_desktop.cpp ask it
# instead of reading the environment themselves; github_application.hpp
# declares it. In a tests-enabled graph every answer is unchanged; in a
# tests-disabled graph the variable no longer stops install, the View Release
# browser or the update lifetime. No credential, provider, vault, transport or
# mutation path changes; the mutation-branch pin is unchanged. Covers
# 9903869f..the BPR-0983 commit; nothing else touched the pinned scope. Review
# recorded on BPR-0983.
# BPR-0817 reseal: a refused startup agent workspace recovery no longer sets
# the cleanup flag that a real quit turns into exit 1. It is recorded as a
# refusal, reported in the status record, and stated in every window's status
# bar. Only the quit's own workspace cleanup still folds into the profile's
# flag. No credential, provider, vault, transport or mutation path changes; the
# mutation-branch pin is unchanged. Review recorded on BPR-0817.
# BPR-1017 reseal: after a window's read host is admitted, the composition
# publishes to that window alone the account session its read host adopted
# (github_window_read_host::adopted_presentation), so a window opened after
# sign-in presents the session before its first read. It reads nothing and
# touches no credential, provider, vault, transport or mutation path; a
# public-preview session and a window with no session publish nothing new.
# The mutation-branch pin is unchanged. Covers 14a18b45..the BPR-1017 commit;
# nothing else touched the pinned scope. Review recorded on BPR-1017.
# BPR-0863 reseal: the BPR_BUILD_TESTING fixture access gains one shared step
# that enters a course's stated modality through the strip's public action and
# names a stall or a withheld surface, plus the facts line a course's failure
# ends with. The diff probe in main.cpp, the workbench composition probe, the
# UIforETW probe and the large-review benchmark enter their modality through
# it. No product path changes; no credential, provider, vault, transport or
# mutation path changes; the mutation-branch pin is unchanged. Covers
# bfd927e7..the BPR-0863 commit; nothing else touched the pinned scope. Review
# recorded on BPR-0863.
# BPR-0864 reseal: every phase of the BPR_BUILD_TESTING workbench composition
# probe that waits is bounded at five seconds through one wait and names its
# stall with the facts that phase depends on; the drawer-width phases move from
# a tick cap to the same bound. No product path changes; no credential,
# provider, vault, transport or mutation path changes; the mutation-branch pin
# is unchanged. Covers 7c6ba1a5..the BPR-0864 commit; nothing else touched the
# pinned scope. Review recorded on BPR-0864.
# BPR-0882 reseal: each startup arm that refuses as `unavailable`, and the
# desktop's own `registry` arm, records a fixed name before it refuses, first
# refusal wins, and a tests-enabled graph keeps the caught exception's text
# beside it (startup_failure_source, detail::note_startup_failure). The
# startup's catch in github_application.cpp names three steps; the desktop's
# recovery-drafts worker keeps the exception it used to drop; main.cpp prints
# the name on stderr in a tests-enabled graph only. Every caller reads the same
# failure, exit code and ordering as before. No credential, provider, vault,
# transport or mutation path changes; the mutation-branch pin is unchanged.
# Covers 3dca2d7a..the BPR-0882 commit; nothing else touched the pinned scope.
# Review recorded on BPR-0882.
# BPR-1037 reseal: the appearance theme, review ledger, round trip and defect
# probes, and the workbench probe, wait through one shared bounded wait in the
# BPR_BUILD_TESTING fixture-access region (keep_waiting, enter_phase,
# detail::fixture_probe_wait). Each waiting phase names its own stall, and a
# failed round trip releases the decision gate it held. The probes read
# window, host and fixture state and write only to standard output; every
# product action they take is unchanged. The range also holds 3727a762
# (BPR-0902), which moved the manifest pin without resealing it: main.cpp
# reads one more fixture counter inside BPR_BUILD_TESTING and prints
# DEVICE_FLOW_REFRESH_RETRIEVES after the refresh course's verdict. No
# credential, provider, vault, transport or mutation path changes in either;
# the mutation-branch pin is unchanged. Covers 8e080bbc..the BPR-1037 commit.
# Review recorded on BPR-1037.
# BPR-1046 reseal: begin_review_reply names a thread only on a drafting draft
# anchored at that thread's own line (draft_on_thread_line), describes the
# reply label only after the draft carries the thread, and PendingReplyThread
# is gone, so no later draft can take a thread named for another. The draft
# command it issues is the same edit with ChangeReplyTarget. No credential,
# provider, vault, transport or mutation path changes; the mutation-branch pin
# is unchanged. Covers a782886d..the BPR-1046 commit. Review recorded on
# BPR-1046.
# BPR-1036 reseal: the BPR_BUILD_TESTING UIforETW selected-diff probe waits
# for its observation and each selection's publication and blobs through the
# shared bounded wait (keep_waiting, enter_phase), naming each stall; its change
# walk names its two failures; its capture asserts the strip row with Home's
# discovery refresh in it and, under the layout floor, names every row that
# shares the window. Two Qt includes move into the testing-only block. The
# probe reads window, host and fixture state and writes only to standard
# streams; every product action it takes is unchanged. No credential,
# provider, vault, transport or mutation path changes; the mutation-branch pin
# is unchanged. Covers 669a5a58..the BPR-1036 commit. Review recorded on
# BPR-1036.
# BPR-1033 reseal: the BPR_BUILD_TESTING UIforETW capture waits, bounded at
# 5 s, for the workbench's pane measurement before each observation instead of
# a fixed four event passes, prints one facts line after
# UIFORETW_CAPTURE_FIRST_TARGET_FAILED naming each clause's values, and adds
# one capture fact, window_controls_rect_in_row (the Windows caption buttons'
# rectangle, empty elsewhere). The probe reads window and workbench state and
# writes only to standard streams and its evidence directory; every product
# action it takes is unchanged. No credential, provider, vault, transport or
# mutation path changes; the mutation-branch pin is unchanged. Covers
# 3d941d47..the BPR-1033 commit. Review recorded on BPR-1033.
# BPR-1045 reseal: profile_desktop's quit keeps a refused Quit with exit code 0
# as it was, leaving the windows usable. A refused failure quit (nonzero) is
# retried on the desktop poll and forced after the empty desktop's 200-poll
# bound, through one helper the empty desktop now shares (quit_within_bound);
# the empty desktop's retry is unchanged. profile_desktop.hpp documents it.
# main.cpp is unchanged. No credential, provider, vault, transport or mutation
# path changes; the mutation-branch and full-application pins are unchanged.
# Covers 9d3fb998..the BPR-1045 commit; nothing else touched the pinned scope.
# Review recorded on BPR-1045.
# BPR-1024 reseal: on Linux, main.cpp creates one xcb_activation_host right
# after QApplication, and the composition gains that host
# (xcb_activation_host.cpp and .hpp, Linux only). On the xcb platform it keeps
# an EWMH _NET_ACTIVE_WINDOW client message from Qt, unless it names the modal
# window, so Qt changes its focus window only on the X server's FocusIn. On
# every other platform it does nothing. It reads one atom and the modal window
# and writes nothing. No credential, provider, vault, transport or mutation path
# changes; the mutation-branch and full-application pins are unchanged. Covers
# b60e04b0..the BPR-1024 commit; nothing else touched the pinned scope. Review
# recorded on BPR-1024.
# BPR-1102 reseal: the BPR_BUILD_TESTING diff interaction capture in main.cpp
# waits, bounded at 5 s, for the workbench's pane measurement before each
# capture's four event passes, and prints one facts line after
# DIFF_INTERACTION_FAILURE:capture-state naming each clause and what the link
# map's frame waits on. The course reads workbench state and writes only to
# standard output and its evidence directory; every product action it takes is
# unchanged. github_application.cpp is unchanged. No credential, provider,
# vault, transport or mutation path changes; the mutation-branch and
# full-application pins are unchanged. Covers 59915464..the BPR-1102 commit;
# nothing else touched the pinned scope. Review recorded on BPR-1102.
# BPR-0897 reseal: a comment above select_observed_review_mutation in
# github_application.cpp records the owner's 2026-09-24 intent that a
# non-attended release build composes the live mutation provider for the open
# pull request, and contrasts it with the designated lane's two gates. No code
# changes; the mutation-branch pin is unchanged. Covers 4248720b..the BPR-0897
# commit; nothing else touched the pinned scope. Review recorded on BPR-0897.
# BPR-1036 floor reseal: the BPR_BUILD_TESTING UIforETW capture's height floor
# is the measured stack's under RFC-BPR-0028 AC-01 as the owner amended it on
# 2026-09-24: 842 on Linux, 835 on macOS, 850 kept on Windows. One constant
# becomes a three-way platform branch with its comment; the failure line that
# names the stack is unchanged. The capture reads window state and writes only
# to standard streams and its evidence directory; every product action it
# takes is unchanged. No credential, provider, vault, transport or mutation
# path changes; the mutation-branch pin is unchanged. Covers e6134dbc..the
# BPR-1036 floor commit; nothing else touched the pinned scope. Review
# recorded on BPR-1036.
# BPR-0644 reseal: device_authorization_enrollment_port::invalidate_grant now
# carries the renewal's failure (device_authorization_refresh_failure), and the
# two enrollment wrappers in github_application.cpp -- the tests-enabled
# counting one and setup_device_enrollment -- forward it unchanged to the
# enrollment they wrap. The GitHub host decides from it what a launch renewal
# that could not finish says; composition decides nothing. No other product
# path in the pinned scope changes; the vault, transport and mutation wiring
# are untouched and the mutation-branch pin is unchanged. Covers d3b583d0..the
# BPR-0644 commit; nothing else touched the pinned scope. Review recorded on
# BPR-0644.
# BPR-0974 reseal: the owner's decision 5c30287a moves the tests-disabled
# Windows image's Qt resources into a bundle beside it. The new
# src/installed_resources.cpp and .hpp read the bundle once, compare its SHA-256
# with the one the build compiled in, register it with QResource, and
# unregister it when the owner is destroyed; they take no credential, provider,
# vault, transport or mutation path. main.cpp gains one owner, declared after
# restrict_installed_runtime and before QApplication, and one refusal line,
# INSTALLED_RESOURCES_REFUSED:<file>:<reason>, that ends startup; every graph
# that embeds its resources gets an empty owner and behaves as before. Both
# files join the source inventory and the production manifest. The
# full-application and mutation-branch pins are unchanged. Covers 1347f776..the
# BPR-0974 commit (85e522d8); 9577a516 and 518fcc37, rebased beneath it,
# touched only fixture sources outside the manifest. Review recorded on
# BPR-0974.
# BPR-1129 reseal: RFC-BPR-0052's title row and review bar. Inside the
# BPR_BUILD_TESTING UIforETW capture only: it finds the navigation presenters
# on the window's review bar and the drawer toggles on the title row, asserts
# both rows' exact order (sixteen title-row actions, fourteen review-bar
# actions, Review details in the comparison's menu), writes capture schema v5
# with both rows' rectangles, names the review bar in its layout failure line,
# and takes the height floor from the measured stack: 829 on macOS, 834 on
# Linux (derived from BPR-1036's Linux stack), 850 kept on Windows. The capture
# reads window state and writes only to standard streams and its evidence
# directory; every product action it takes is unchanged. No credential,
# provider, vault, transport or mutation path changes; the mutation-branch pin
# is unchanged. Covers 0114d6a8..the BPR-1129 commit; nothing else touched the
# pinned scope. Review recorded on BPR-1129.
# BPR-1130 reseal: RFC-BPR-0052's Updates line folds, and a new window starts
# folded. Inside the BPR_BUILD_TESTING UIforETW capture only: before its floor
# check it records whether the window started folded, opens the line through
# the window's own Updates Line command (the core decides the fold and the
# workbench host publishes it), measures the open stack, and folds the line
# again, each through the existing bounded drain. The one height floor becomes
# two, folded and open: 879 and 845 on macOS as measured, 884 and 850 on Linux
# derived from BPR-1036's stack, and on Windows 884 and the owner's 850. The
# failure line names both stacks, and capture schema v6 records the initial
# fold, the open line's rectangle and the open comparison's height. The capture
# reads window state, triggers only the fold command a reviewer can, and writes
# only to standard streams and its evidence directory. No credential,
# provider, vault, transport or mutation path changes; the mutation-branch pin
# is unchanged. Covers 96a86433..the BPR-1130 commit; nothing else touched the
# pinned scope. Review recorded on BPR-1130.
# BPR-0899 reseal: 9a0b44d5..this implementation extracts the private
# layout service/port, records only completed store refusals with a separate
# in-flight count, and retires the unreachable in-flight-at-drain alternative
# after host quiescence. The new header is in both inventories. No intervening
# pinned-scope change; mutation branch unchanged. Review recorded on BPR-0899.
# BPR-0905 reseal: 97a7564b..this candidate changes only main.cpp's
# automated terminal report: one elapsed sample shared by success/failure,
# budget output on success, expiry before the failed-run marker. No scheduling,
# course verdict, credential, provider or mutation admission changed. The
# github_application.cpp and mutation-branch pins remain unchanged.
# BPR-0971 terminal reseal: f6de40b6..e1dbf44b replaces the tests-enabled
# startup exception capture with a private helper that copies at most 2 KiB,
# flattens ASCII controls and marks truncation. main.cpp uses the same helper;
# the public header documents the bound and the new private header joins both
# inventories. Source review is recorded on BPR-0971. First-failure ordering,
# mapped failures, tests-disabled behavior and mutation admission are unchanged;
# the production mutation-branch pin is unchanged.
# [[BPR-1196]]: reviewed the empty-tab comparison observer and synchronous
# first-file observation at f6de40b6 plus this patch. The test-only passive
# sampler reports accepted/window/active pairs; production ownership stays in
# the existing tab and epoch hosts. No mutation/provider/credential path changed.
# BPR-1198 reseal: f6de40b6..this candidate adds only a BPR_BUILD_TESTING
# read-only refresh identity observation to github_application.cpp. main.cpp
# reconciles the automated course's expected roots with the exact ready tab,
# selected file, accepted/published pair and rendered publication, and reports
# the last pre-shutdown identity. Production control flow and the mutation
# branch are unchanged. The 8ebf9e6c follow-up also observes whether the
# host/window modality presents tabs; Overview retains a tab without showing
# its file. Complete scope review is recorded on BPR-1198.
# BPR-0971 full-card: classified terminal producers, explicit process terminal
# owner and bounded queue; private paths/exception bytes omitted. Typed epoch
# Quill observations replace developer stderr bypasses. Self-review retains
# all session, mutation and durable terminal authority; mutation digest unchanged.
# BPR-1202 reseal: reviewed 5c094f80..this candidate. The only application
# source delta exchanges file/change positions in the test-only UIforETW
# capture's exact 14-action assertion and its comment. No runtime, authority,
# persistence or mutation code changes. Mutation branch unchanged. Record and
# direct capture/focus evidence on BPR-1202.
# RFC0022 D diagnostic reseal: reviewed e45f5d17..this candidate. The only
# github_application.cpp change passes the profile-owned diagnostic log to the
# existing production provider. Grant, target, source admission and journal
# decisions are unchanged. The app manifest also includes the reviewed6526f525
# exact profile selector. Three owning digests are recomputed from those bytes.
# BPR-1228 reseal: reviewed aa8826e5..e9e65eab. Pinned-scope commits, all in
# github_application.cpp, github_application.hpp or profile_desktop.cpp:
# 27dbeef51 adds the tour owner endpoint beside activation, bound to the
# runtime lease and polled one step at a time on the desktop effect lane;
# 637cd41df routes a forwarded tour command to the window's live draft on the
# decision lane (store otherwise, 5 s durable wait, editor_busy on preedit) and
# records the draft's target in the local tour catalog; eca094604 serves assist
# tickets with the owner's own clock and hook revision; bf7a96367 and 52fa4a06c
# change only BPR_BUILD_TESTING probes; 9ad03a828 (reviewed on BPR-1211) moves
# every ledger read and write to the ReviewLedgerEffects lane behind generation-
# guarded answers; acebc2089 composes the local review-cycle desktop and joins
# its shutdown and quiescence. The new composition sources tour_owner_service,
# tour_assist, tour_cli_types and review_cycle_desktop enter the tree inventory
# only: local store, workspace and Qt, no provider, credential or transport.
# No grant, target, provider or publication path changes; the mutation-branch
# pin is unchanged. The ReviewMutationEffects member check now tolerates
# alignment. Full review is recorded on BPR-1228.
# BPR-1238 reseal: 46e0d07b..this candidate; no intervening pinned-scope commit.
# A window's own private-record write now reloads its defects correlated with
# its own comparison (as opening does) instead of adopting the durable records
# as they are; siblings still reload without correlating. Correlation persists
# only a changed history through the existing ledger lane. The defect probe
# (BPR_BUILD_TESTING) selects a right-side line before its shared follow-up.
# No grant, provider or publication path changes; mutation pin unchanged.
# BPR-1243 reseal: 6e1bf4dd..this candidate; no intervening pinned-scope commit.
# The only pinned-scope change is clang-format's alignment of one declaration
# in private_review_workflow.cpp (`lock` beside `rows`). No behavior change;
# github_application.cpp and the mutation pin are unchanged.
# BPR-1246 reseal: 94250ab9..this candidate; no intervening pinned-scope commit.
# dispatch_review_draft takes an optional settlement callback, called once with
# whether the command was accepted, including on the private review's later
# answer. promote_private_item (note and chat-reply promotion) now shows
# Comments on that answer and refuses only when the draft was not accepted;
# before, it read the draft on return and refused every private-review
# promotion it had in fact made. Draft commands still go through the same
# private review and shared-draft owners. No grant, provider or publication
# path changes; mutation pin unchanged.
# BPR-1243 Windows reseal: 68599e10..this candidate; no intervening pinned-
# scope commit. clang-format 22 (Windows, Linux) and 23 (macOS) align the short
# case statements in dispatch_tour_action differently, so coding_format_check
# was red on Windows. Four of them now take one statement per line, which
# neither version aligns. Formatting only: the same actions in the same order;
# no grant, target, provider or publication path changes; mutation pin
# unchanged.
# BPR-1254 reseal: fd05277e..this candidate; no intervening pinned-scope commit
# (4169e097 and c3df92bf touch only unpinned tour sources). The profile gains
# one review-ledger file mutex; every window's ledger host gets the
# review-ledger lane through exclusive_file_effect_executor, which runs each
# task under it, and private_review_workflow takes it as its gate instead of a
# mutex of its own, so a reopen's preflight never sees a private read or write
# of the same file. Two BPR_BUILD_TESTING probes report the ledger's opening
# and the exclusion. No grant, target, provider or publication path changes;
# mutation pin unchanged.
# BPR-1256 reseal: 330104f2..this candidate; no intervening pinned-scope commit.
# main.cpp's BPR_BUILD_TESTING comparison_is_wholly_visible counts what the
# comparison's own visible descendants show as the comparison's, because
# visibleRegion() leaves out its opaque children and the shared scroll bar read
# as clipping on Linux. A sibling that covers the comparison still fails it. The
# probe reads widget state only; no product action, grant, target, provider or
# publication path changes; mutation pin unchanged.
# BPR-1262 reseal: 2191da24..this candidate; no intervening pinned-scope commit.
# GCC 15's tests-on Release graph (-O3, _FORTIFY_SOURCE) did not build. In
# github_public_path_fixture.cpp (BPR_BUILD_TESTING) the request's nested
# correlation names its type, avoiding the nested-aggregate double-destroy
# shape BPR-0816 found. profile_window_layout_port.hpp's store returns each
# terminal from a perform_store helper instead of reassigning an expected<void,
# E>, with the same results: a throw still answers unavailable. No grant,
# target, provider or publication path changes; mutation pin unchanged.
# BPR-1156 reseal: e257b236..this candidate; no intervening pinned-scope commit.
# Suggest change is coloured by the review panes' packaged classifier: the
# composition hands review_cycle_desktop the existing definition catalog and
# the appearance effect lane (the lane the panes' classifier already runs on),
# and apply_appearance gives it the profile's role colours. The desktop holds
# the composer's in-memory texts by blob identity for a small, separate
# syntax_classification_application_host and adopts only spans of the text
# still shown. It reads no file and runs no process; no grant, target, provider
# or publication path changes; mutation pin unchanged.
# BPR-1213 reseal (by bpr-main-linux): f3327b74..this candidate. Of the
# pinned-scope commits since the BPR-1156 seal, only 5b7eddb83 moves a pin;
# 028882c3a, 8cbdc1477 and 2a79d08eb leave all three digests unchanged
# (checked with BPR_PRINT_PINNED_DIGESTS at each). 5b7eddb83 adds the
# constructed nested-folders-v1 public-path fixture: one enum value, its
# fixture-host case in github_application.cpp (github.com, the same profile,
# host and server contract as uiforetw_74), its contract case in
# github_public_path_fixture.cpp and its --github-fixture expectations in
# main.cpp. Fixture selection only; no grant, target, provider, mutation or
# publication path changes; mutation pin unchanged. main was red here from
# 5b7eddb83 until this reseal.
# BPR-1167 inventory (registered by BPR-1168): 2a79d08eb added eight
# src/app/src files to the tour runtime library without registering them, so
# the tree inventory drifted from then on. Reviewed: cycle_cli (the `bpr cycle`
# terminal front over the local cycle store, read-only workspace source and
# local export destination), cycle_session (one command's composition of the
# Review Cycles runtime), profile_admission (registry lookup and runtime lease,
# shared with `bpr tour`) and terminal_text (terminal text neutralization).
# CLI and admission only: none composes a grant, target, provider, mutation or
# publication path, none is in github_application.cpp or the production
# manifest, and no digest changes.
# BPR-1168 reseal: 551551c9..this candidate; no intervening pinned-scope
# commit since the BPR-1213 seal (BPR-1266 and BPR-1267 leave all three pins as
# they were). github_application.cpp routes the owner channel's new `cycle`
# operation to desktop_cycle_owner_route, which runs one forwarded `bpr cycle`
# command on its own CycleOwnerEffects lane through cycle_owner_service (a
# session of the profile's cycle store under the owner's lease, the command's
# registry options only). Before review or freeze it asks the windows, on the
# decision lane, what they hold of that cycle's draft (core::admit_owner_command
# decides: save the typed goal first, or draft_busy while composing); after a
# write, cycle_written makes windows showing the cycle read it again. Shutdown
# stops the lane's work and waits for it to go idle. The tour-endpoint refusal
# line now prints on Windows too, where the endpoint is now a named pipe.
# github_application.hpp declares the three members. cycle_commands and
# cycle_owner_service enter the tree inventory: the cycle CLI's commands, run by
# the terminal and the owner alike, over the local cycle store, workspace source
# and export destination. No grant, target, provider, mutation or publication
# path changes; mutation pin unchanged.
# BPR-1250 reseal: 7e5a093cc..this candidate; no intervening pinned-scope
# commit since the BPR-1168 seal. On macOS the desktop owns one menu bar status
# item for the life of the process (profile_desktop: native_menu_bar_status_item,
# status_item(), start_with_factory's last argument) and passes it to each
# runtime as github_application_config::StatusItem; github_application.cpp
# composes the profile's system_tray_notification_sink over that item instead of
# a tray of its own, so the item no longer leaves with the last window, and the
# fixture desktop takes the same item. Notification presentation only: no grant,
# target, provider, credential, mutation or publication path changes; mutation
# pin unchanged; no new src/app source.
# BPR-1245 reseal: a4723f817..this candidate. The one intervening src/app
# commit, d46d37cf7 (BPR-1161), leaves all three pins as they were (checked
# with BPR_PRINT_PINNED_DIGESTS at 0890cf71e). Owner decision
# BPR-1245-workshop-v001: a comment-workshop press no longer sends the mutable
# draft. github_application.cpp asks private_review_workflow (a manifest
# source) to record the stored draft's text as one immutable reviewer revision
# in Private history on the private store's effect lane, then dispatches the
# press through the same launch path with that revision as the explicit private
# run's only source; the press holds no session while it records. The host's
# admission refuses a workshop run with no source, a second source or anything
# but a complete current reviewer revision. Agent context only: no grant,
# target, provider, credential, mutation or publication path changes; mutation
# pin unchanged; no new src/app source.
# BPR-1270 reseal: 9ff5bbbbc..this candidate; no intervening pinned-scope
# commit (the manifest pin matched at 9ff5bbbbc). main.cpp registers the
# bundled code font's faces (bpr::qt::register_bundled_code_font,
# Qt adapter resources) once, after QApplication and the window icon and before
# anything composes an appearance; a failure is not fatal. Its
# BPR_BUILD_TESTING --code-font-probe prints the fresh profile's code font and
# exits before composition, like --application-icon-probe. Presentation only:
# no grant, target, provider, credential, mutation or publication path changes;
# github_application.cpp and the mutation pin unchanged; no new src/app source.
# BPR-1270 amendment reseal: a54728d13..this candidate. The owner's amendment
# swaps the bundled faces from the Nerd Fonts build to plain JetBrains Mono NL
# (JetBrains Mono v2.304). The family and resource names live in the Qt
# adapter, so main.cpp and every other pinned-scope source are unchanged and
# all three pins stay as they were.
# BPR-1177 reseal: 327130672..this candidate; no intervening pinned-scope
# commit since the BPR-1270 seal. Publish as Pull Request from a local cycle:
# github_application.cpp remembers the installed Git it already resolved
# (github_profile_runtime::InstalledGit) and hands review_cycle_desktop that
# Git and, asked each time, github_application_host::accepted_publication_account:
# the BPR-1175/1176 publication catalog minted by the accepted account's own
# native HTTPS transport (github_publication_session_factory_port), bound to
# exactly that host, account and server contract, with no pull request snapshot
# needed and nothing for a gh CLI connection. The desktop's catalog runs in the
# profile's own owner-private isolation root. The fixture access gains
# set_cycle_publication_account, which only a BPR_BUILD_TESTING course reaches.
# This is a publication path, reviewed on BPR-1177: the finite catalog of blobs,
# tree, one commit, a new branch and one pull request, authorized only by the
# reviewer's confirmation of a recorded plan's digest; no review-mutation grant,
# target or provider changes; mutation pin unchanged; no new src/app source.
# BPR-1273 reseal: 1dbd72e48..this candidate; no intervening pinned-scope
# commit since the BPR-1177 seal. Add Notes to Review Tour from a linked
# cycle: open_window hands each window's review_cycle_desktop a route to this
# profile's existing tour owner (desktop_tour_owner_route), the one `bpr tour`
# commands reach, so a promoted note is a local tour edit run on the window
# holding that draft or on the stored draft, with a request identity and the
# tour's own receipts. review_cycle_desktop reads the tour list and sends only
# load, request_status and insert edits; tour_publication_plan takes its
# observation from the shared core helper with unchanged behaviour. No GitHub
# write, grant, target, provider or publication path changes: the tour is
# published only through its existing preview; mutation pin unchanged; no new
# src/app source. Reviewed on BPR-1273.
# BPR-1277 reseal: d5f9dfb76..this candidate, over 44e7b8a6c's BPR-1273
# reseal (the only intervening pinned-scope commit, reviewed there). The same selected diff asked for again
# (an epoch refresh re-requests the selected file) used to clear the selection
# and the review draft and present the diff afresh, which landed the selection on
# its first change; a reviewer writing a comment lost it from the editor, and the
# round-trip course stalled at its fallback draft about one run in ten on
# Windows. github_application.cpp's begin() now keeps the selection, the draft
# and any publish in flight when application::same_selected_diff (a new pure
# predicate in workbench_runtime_ports.hpp: the same authority, comparison
# context and file entry) holds against the shown request and publication and
# the surface presents that publication's key; publish() adopts the same key
# under the new request without presenting it again, and different bytes under a
# kept request still clear. Another file, revision or comparison clears as
# before. The round-trip probe now asks for the same diff while the fallback
# draft is written and requires the draft and its line to stay. Presentation and
# draft retention only: no grant, target, provider, credential, mutation or
# publication path changes; mutation pin unchanged; no new src/app source.
# BPR-1179 reseal: 91c01540e..this candidate, over c4579bd08's BPR-1277
# reseal; no intervening pinned-scope commit. Agent integrations and local
# automation become removable component families. Every agent-only block of
# github_application.cpp (the launcher, invocation, chat, finding, workshop and
# re-alignment hosts, the MCP helper and its snapshot, their projections, effect
# lanes, dispatch, writing assistance's launch and the matching fixture access)
# is compiled only under BPR_AGENT_INTEGRATIONS; without it the private-run
# request is refused, no agent profile is offered and the window withdraws its
# agent controls. The owner command endpoint and the cycle owner service are
# compiled only under BPR_LOCAL_AUTOMATION; without it the tour service is still
# made for the desktop's in-process tour route. Both macros are defined in every
# graph that builds the family, so a full graph composes exactly as before. One
# fixture lambda keeps its canonical_hex value alive for the view it compares.
# No grant, target, provider, credential, mutation or publication path changes;
# mutation pin unchanged; no new src/app source. Reviewed on BPR-1179.
# BPR-1083 reseal: over 192ffb56b's BPR-1179 reseal; no intervening
# pinned-scope commit. github_application.cpp gives the window the label tints
# of the theme on screen (set_label_tints, beside the conversation panel's
# appearance) and, in a tests-on fixture launch only, turns the comment
# editor's label helper on for --label-helper. main.cpp adds the constructed
# conventional-comments-v1 fixture (the nested-folders pull request and pack
# with labelled review threads) and the --label-helper switch;
# github_public_path_fixture.cpp and github_application_fixture_access.hpp
# name it. Presentation and fixture selection only: no grant, target,
# provider, credential, mutation or publication path changes; mutation pin
# unchanged; no new src/app source. Reviewed on BPR-1083.
# BPR-1083 v002 reseal: over the BPR-1083 reseal above; no intervening
# pinned-scope commit. github_application.cpp adds two tests-on fixture
# courses beside the threaded-reply course: reveal_fixture_file selects one
# fixture file in Read through the files drawer, and exercise_label_authoring
# writes a comment there (`:qu:`, the chip, ⌘L), publishes it through the
# existing review mutation port to the fixture and checks the refreshed
# thread's mark. main.cpp adds the tests-on --label-authoring-probe run and
# the --attended-open-file attended setup (the fixture account's own Connect,
# the Home bar's open, then the reveal). Fixture-only: no grant, target,
# provider, credential, mutation or publication path changes; mutation pin
# unchanged; no new src/app source. Reviewed on BPR-1083.
# BPR-1287 reseal: over the BPR-1083 v002 reseal above. The tests-on
# label-authoring course first opens Read with no file chosen and checks the
# comparison area's new idle direction (diff_workbench, Qt) before revealing
# its file; main.cpp prints that evidence bit and adds the attended
# --attended-open-pull-request, which stops after the Home bar's open. The
# UIforETW selected-diff course accepts that idle direction in the panes'
# place before it selects its file. Fixture-only: no grant,
# target, provider, credential, mutation or publication path changes;
# mutation pin unchanged; no new src/app source. Reviewed on BPR-1287.
# BPR-1083 v003 reseal: over the BPR-1287 reseal above. The tests-on
# label-authoring course reads each card's decoration as the Qt adapter's
# feedback_mark_emphasis (was a blocking flag), expects the second root's
# non-blocking emphasis and the drawer entries' root marks
# (compact_mark_rect), and the published thread's undecorated mark. Fixture-
# only: no grant, target, provider, credential, mutation or publication path
# changes; mutation pin unchanged; no new src/app source. Reviewed on BPR-1083.
# BPR-1083 v004 reseal: over the BPR-1083 v003 reseal above; no intervening
# pinned-scope commit. The tests-on label-authoring course reads a card's
# decorations as the Qt adapter's feedback_mark_emphasis struct (a weight and
# the if-minor corner) and expects the fixture's renderer.cpp threads to carry
# several and custom decorations, shown as written in each post's text.
# Fixture-only: no grant, target, provider, credential, mutation or
# publication path changes; mutation pin unchanged; no new src/app source.
# Reviewed on BPR-1083.
# BPR-1292 reseal: over the BPR-1083 v004 reseal above; no intervening
# pinned-scope commit. The tests-on label-authoring course checks that the
# core took its refresh and then waits for that refresh to answer (pull
# request ready, action released) before it judges the published thread,
# instead of passing on the card the application's own post-publish refresh
# had already drawn. Fixture-only: no grant, target, provider, credential,
# mutation or publication path changes; mutation pin unchanged; no new
# src/app source. The manifest digest also takes in BPR-1291's tests-on
# comparison-start and comparison-end line checks in main.cpp, which landed
# at 68c1fe9b7 without a reseal and left this row failing on origin/main;
# they add no grant, target, provider, credential, mutation or publication
# path either. Reviewed on BPR-1292.
# BPR-1236 reseal: over the BPR-1292 reseal above; no intervening
# pinned-scope commit. The comparison surface's selection and reading-gesture
# sinks now observe the active document tab's reading in the same turn as the
# window's reading position (observe_tab_reading, which returns before the tab
# host exists or once the window closes), so a Home or Quit that follows a
# selection at once no longer records a tab without it. No grant, target,
# provider, credential, mutation or publication path changes; mutation pin
# unchanged; no new src/app source. Reviewed on BPR-1236.
# BPR-1290 reseal: over the BPR-1236 reseal above; no intervening
# pinned-scope commit. The conversations drawer is inline only (owner verdict
# on the inline-drawer prototype, 2026-10-06: "I like it"): the composition no
# longer hands the thread map an unread colour, and the tests-on reply and
# resolve probe opens the thread in place and clicks its button instead of
# pressing a card action rectangle; the label probes read the note's mark.
# main.cpp and the fixture table add the constructed inline-drawer-v1 pull
# request. Presentation and fixture only: no grant, target, provider,
# credential, mutation or publication path changes; mutation pin unchanged;
# no new src/app source. Reviewed on BPR-1290.
# BPR-1289 reseal: over the BPR-1290 reseal above; no intervening
# pinned-scope commit. main.cpp adds the tests-on --attended-open-settings,
# which with --attended-launch triggers the workbench's own Settings action
# once the window shows and prints ATTENDED_SETTINGS_SHOWN or
# ATTENDED_SETTINGS_FAILED, for an attended look at the Settings search.
# Attended setup only: no grant, target, provider, credential, mutation or
# publication path changes; mutation pin unchanged; no new src/app source.
# Reviewed on BPR-1289.
# BPR-1210 reseal: over the BPR-1289 reseal above; no intervening
# pinned-scope commit. A new private file note is written with the selected
# comparison's presentation path (within maximum_note_path_bytes), and a file
# note's label falls back to that kept path when the file is in neither the
# current nor the previous accepted snapshot, before the honest "earlier
# update" text. Private ledger only (the owner's amendment of RFC-BPR-0007,
# 2026-10-06): no grant, target, provider, credential, mutation or publication
# path changes; mutation pin unchanged; no new src/app source. Reviewed on
# BPR-1210.
# BPR-1084 reseal (2026-10-06): over the BPR-1210 reseal above; no
# intervening pinned-scope commit. github_application.cpp includes the Qt
# suggestion_mark header, and the tests-on label-authoring course continues
# after its labelled comment: an issue with a suggested change, a suggestion
# with no patch, a suggested change on a line the fixture refuses an inline
# comment for, and a reply written in place, each checked through the
# editors' suggested-change marks and the fixture's received bytes. main.cpp
# prints six more evidence bits. github_public_path_fixture.cpp records each
# created comment's decoded body, path, side and line in the fixture's
# evidence (github_application_fixture_access.hpp), and its synthetic
# off-hunk 422 also answers a body containing "BPR-1084 off-hunk". The
# production composition is unchanged: the pre-publish mark is Qt
# presentation fed by the accepted conversation projection. No grant,
# target, provider, credential, mutation or publication path changes;
# mutation pin unchanged; no new src/app source. Reviewed on BPR-1084.
# BPR-1085 reseal (2026-10-06): over the BPR-1084 reseal above; no
# intervening pinned-scope commit. github_application.cpp reads each
# accepted snapshot once through application::observe_review_preflight
# into the review submission workflow (observe_review_preflight), hands it
# the window's unsent comment draft as the selected draft
# (publish_preflight_selection), and adds the tests-on stage_server_root
# fixture accessor. review_submission_workflow.cpp presents the preflight
# and the prepared decision, and refuses a Submit whose shown prepared
# decision no longer matches the draft, account, head, summary, decision or
# selected drafts, before and after the existing write confirmation. The
# request, idempotency key, grant, journal, host and provider are
# unchanged; the begin_submit gate only adds refusals. No grant, target,
# provider, credential, mutation or publication path changes; mutation pin
# unchanged; no new src/app source. Reviewed on BPR-1085.
# BPR-1085 attention-amber reseal (2026-10-06, macOS look pass): over the
# BPR-1085 reseal above; no intervening pinned-scope commit.
# github_application.cpp adds one presentation call beside set_label_tints:
# the window receives the theme's state_attention colour for Approve's dot
# (Window->set_attention_colour). For the attended look only, the tests-on
# fixture accessor seed_attended_comment_draft writes a comment through the
# window's own Comment action, and main.cpp's tests-on
# --attended-preflight-look stages two fixture thread roots as the fixture
# account's and opens Compose review. No grant, target, provider,
# credential, mutation or publication path changes; mutation pin unchanged;
# no new src/app source. Reviewed on BPR-1085.
# BPR-1086 reseal (2026-10-06): over the BPR-1085 attention-amber reseal
# above; no intervening pinned-scope commit. github_application.cpp: the
# tests-on label-authoring course continues after its reply. It triggers
# Classify as defect from the issue root's thread menu, then has the
# fixture's website remove that root's label, resolve the thread and show a
# labelled tour stop on another thread (the existing stage_server_root),
# refreshing through the public Refresh action each time, and reads the
# private defect and the cards. main.cpp prints five more evidence bits
# (github_application_fixture_access.hpp). The production composition is
# unchanged; the tour stop's mark is read in the Qt adapter, outside these
# pins. No grant, target, provider, credential, mutation or publication
# path changes; mutation pin unchanged; no new src/app source. Reviewed on
# BPR-1086.
# BPR-1088 reseal (2026-10-06): over the BPR-1086 reseal above; no
# intervening pinned-scope commit. github_application.cpp, inside its
# tests-on blocks only: --label-helper (and the label-authoring probe) no
# longer reaches into the comment editor. Once per profile it subscribes to
# the profile's workbench preferences and, while the accepted record has
# the label helper off, asks the same host for that record with it on: the
# write Reviews & queue > Comment labels > Label helper makes. The profile
# runtime gains a tests-on LabelHelperRequested flag. The production
# composition is unchanged; the editor now follows the window's published
# behaviour in the Qt adapter, outside these pins. No grant, target,
# provider, credential, mutation or publication path changes; mutation pin
# unchanged; no new src/app source. Reviewed on BPR-1088.
# BPR-1227 reseal (2026-10-06): over the BPR-1088 reseal above; no
# intervening pinned-scope commit. The tour owner channel (protocol v6)
# gains a `source` read. github_application.cpp: the profile route sends it
# to the window whose current tour is that draft, which reads one pinned
# range through its own epoch host and Git cache on its diff lane and
# answers there; a closing window stops its reads and waits for none. Other
# drafts go to tour_owner_service, which refuses them provider-free.
# tour_runtime.cpp, tour_cli.cpp: the CLI forwards source reads instead of
# refusing them, keeps assist source evidence standalone-only, and names
# protocol v6. The read uses the session's already-accepted authority and
# grants nothing the same user's standalone CLI could not read itself; it
# records nothing and writes nothing. No target, credential, mutation or
# publication path changes; mutation pin unchanged; no new src/app source.
# Reviewed on BPR-1227.
# BPR-1089 reseal (2026-10-06): over the BPR-1227 reseal above; no
# intervening pinned-scope commit (1d7625992 changed cycle_cli.cpp, whose
# digests the pins did not move). github_application.cpp composes BPR-1087's
# feedback_counter_model into the window: the accepted snapshot is observed
# into the label buckets before the overview is published; the Overview's
# request (Settings' chips, the view's mode and sort) is answered with
# project_overview_feedback from the kept tallies; a count's activation lists
# exactly its conversations in Discuss through list_overview_drill_down, and a
# refresh lists the same count again from the new population. Two window
# dispatches and four members are added. It reads only the accepted snapshot
# the window already holds; no provider, grant, target, credential, mutation
# or publication path changes; mutation pin unchanged; no new src/app source.
# Reviewed on BPR-1089.
# BPR-1295 reseal (2026-10-06): over the BPR-1089 reseal above; no other
# intervening pinned-scope commit (the BPR-1227 pins matched at bb9254ff3,
# and BPR-1089's 68f00eb15 is reviewed and resealed above). Read merged into Review. github_application.cpp:
# Resume, a link to a file and a tour stop enter Review instead of Read and
# keep their tab as Review's; a tab is shown in Review alone; the course
# helpers and fixture reveals choose Review, and the label probe's first bit
# is renamed ReviewEmptyDirected (github_application_fixture_access.hpp).
# main.cpp: the overview and epoch courses expect Review, and the review
# behaviour course no longer clicks "Continue review enters", which is gone;
# Continue review still has to enter Review and publish nothing. Presentation
# and course wording only: no grant, target, provider, credential, storage,
# mutation or publication path changes; mutation pin unchanged; no new src/app
# source. Reviewed on BPR-1295.
# BPR-1089 macOS look reseal (2026-10-06): over the BPR-1295 reseal above; no
# intervening pinned-scope commit. main.cpp and the fixture table add the
# constructed overview-labels-v1 pull request (the nested-folders pull request
# and pack with 34 labelled threads, pull request comments and review
# summaries, scripts/build_overview_labels_fixture.py) for the owner's
# Overview look; github_application.cpp names its fixture label. Fixture
# selection only: no grant, target, provider, credential, mutation or
# publication path changes; mutation pin unchanged; no new src/app source.
# Reviewed on BPR-1089.
# BPR-1068 reseal (2026-10-06): over the BPR-1089 macOS look reseal above; no
# other intervening pinned-scope commit. Production manifest only;
# github_application.cpp is unchanged. platform_credential_vault.{hpp,cpp}
# add a Linux read-only Secret Service readiness check, which touches no item;
# tour_remote.cpp asks it before a native device flow starts and refuses with
# vault_unavailable; tour_cli.cpp's capabilities names its platform and claims
# no qualification. No grant, target, provider, token, mutation or
# publication path changes; mutation pin unchanged; no new src/app source.
# Reviewed on BPR-1068.
# BPR-1301 reseal (2026-10-06): over the BPR-1068 reseal above; no
# intervening pinned-scope commit. github_application.cpp, agent builds only:
# each window holds its profile's Claude process wrapper as last published
# (set from the same preferences subscription as the writing hooks), and the
# agent host's process port is application::wrapped_claude_process in front of
# the existing launcher (in front of the tests-on probe), which replaces the
# executable of a Claude stream-JSON request alone and re-inspects the path
# before launch. Settings > Agents & context's Process wrapper row dispatches
# to application::claude_wrapper_editor, which has the agent adapter inspect
# the file on the effect lane and then writes the profile preference; the
# editor joins window shutdown and quiescence. review_cycle_desktop.{hpp,cpp}
# take the same source and wrap Send to Agent's launcher the same way. No grant,
# target, provider, credential, environment, mutation or publication path
# changes; host policy unchanged; mutation pin unchanged; no new src/app
# source. Reviewed on BPR-1301.
# BPR-1293 reseal (2026-10-06): over the BPR-1301 reseal above, rebased; no
# other intervening pinned-scope commit (the BPR-1301 pins matched at f5a64e041).
# Conversations, a fourth modality. github_application.cpp: the course's
# modality names gain Conversations ("conversations" for its centre); a
# `#discussion_r` link the read identified and the reveal of a just-published
# inline comment open the thread in place in Review through the window's
# reveal_conversation_in_review, a review link still revealing in Discuss; each
# accepted snapshot's label lists (project_conversation_labels over the kept
# feedback_counter_model) go to the window; the appearance gives the finder its
# colours and code font. Tests-on only: the conversations-v1 fixture's host
# profile, and an attended look's read state recorded through the same review
# ledger call Mark read makes (mark_conversation_read_through,
# apply_fixture_read_state). main.cpp: `--github-fixture conversations-v1` and
# `--attended-read-state`. It reads only the accepted snapshot and the kept
# counts the window already holds; no provider, grant, target, credential,
# storage, mutation or publication path changes; mutation pin unchanged; no new
# src/app source. Reviewed on BPR-1293.
# BPR-1086 owner-look reseal (2026-10-07): over the BPR-1293 reseal above; no
# intervening pinned-scope commit (the BPR-1293 pins matched at c01e04974).
# main.cpp only, tests-on: `--attended-tour-stop` (with `--attended-open-file`
# on conventional-comments-v1) stages renderer.cpp's line 12 thread, in the
# fixture's responses before the first read, as a published tour stop the
# fixture account wrote with core::render_tour_stop: pinned source, heading,
# a labelled narrative and its navigation. github_application.cpp, tests-on:
# the label-authoring course's tour-stop phase now expects the stop's note to
# read from its narrative (card_post::Lead) with the label as its header. No
# provider, grant, target, credential, storage, mutation or publication path
# changes; mutation pin unchanged; no new src/app source. Reviewed on BPR-1086.
# BPR-1305 gate repair reseal: reviewed 52d5a4866..7289987eb.
# The application and fixture header preserve the exact current request during
# same-head kept-tab revalidation and report pending tab work to tests. Fresh
# actor publication still must pass generation/file/epoch admission and the
# full material key comparison; different material keeps clear/reload. No
# provider, credential, target, grant, storage or mutation path changes. Native
# menu and hidden-card geometry corrections affect Qt/test presentation only.
# Direct failures and corrected focused courses are recorded on BPR-1305.
# The production mutation branch is unchanged. This is engineering self-review.
# BPR-1305 prerelease integration reseal (2026-10-07): efa26ca02..0a3cf40ee.
# Pinned changes: 29a55ef70 (tests-on Hide Drawers capture), da862cf32
# (tests-on label readiness), bba8a8d26 (BPR-1303 active-tab re-presentation),
# and 0a3cf40ee (their integration; remove the test's manual file reopening).
# BPR-1303 records first-bad and forced-off controls plus 20 passing label runs.
# The accepted handoff that preserves the diff records RefreshKeptTabView;
# once files return, the active tab refreshes its exact view through the existing
# document-tab host. Head-moving refresh retains its comparison reload. This
# uses the existing tab restoration path, introduces no provider/target/grant,
# and has no mutation/publication/storage/credential change. The label course
# now waits for that current comparison and for private draft/refresh transitions
# before the next exact body/anchor publication; all wire, refusal and mark
# assertions remain. BPR-1304's menu repair is outside the app inventory and its
# owner native evidence remains parked. No new app source; mutation pin unchanged.
# Review/evidence: BPR-1303 and BPR-1305.
# BPR-1301 v002 reseal (2026-10-08): over the BPR-1305 reseals above, rebased
# onto 79d7c85e0; pins recomputed on the merged tree. Owner, 2026-10-07: "Yes,
# search PATH (Recommended)". github_application.cpp, agent builds only: the
# window's locate_claude() gives the agent host's and Send to Agent's
# application::wrapped_claude_process, and the Process wrapper row's editor, a
# locator that reads this application's own executable search path (its name
# from core::agent_search_path_variable) and QDir::homePath() when asked and has
# the agent adapter take the first executable regular claude on it or in the
# fixed usual install folders. Only a request naming the bare `claude` with no
# wrapper saved is pointed at it; none found ends the request failed_to_start
# and starts nothing. Settings shows the found path read-only. Tests-on only:
# the fixture's ClaudeSearch replaces the path and the folders. No shell, grant,
# target, provider, credential, child environment, mutation or publication path
# changes; host policy unchanged; mutation pin unchanged; no new src/app source.
# Reviewed on BPR-1301.
# Prerelease-exception reseal (2026-10-08): 3627b98af..24b470494. The pins
# last matched at 3627b98af; three later commits touch the pinned scope, and
# none was resealed (docs/releases/v0.1.5.md records the red rows).
# fd96a3876 (BPR-1308), github_application.cpp: a submit_repository_setup
# intent retires CLI discovery and runs the setup host's explicit repository
# preparation; open() answers a repository destination with
# repository_prepared only under the exact authenticated session, profile,
# host and hostname, otherwise unavailable, and reads, navigates and saves
# nothing. Public preview cannot prepare one. Home's repository-addition
# dispatch reaches the existing queue host's repository_addition_intent with
# Source stamped by the composition as this window's DraftWindow, and the
# queue publishes a gesture result only to that attachment.
# 441d55e45 (BPR-1313), github_application.cpp: the Overview page's summary
# resize dispatch reaches WorkbenchHost::resize_summary, which stores a bounded
# per-window fraction in the existing workbench settings record.
# d8f74f890 (BPR-1316), production manifest: release_maintenance_controls.cpp
# asks a new diagnostic_export_directory.{hpp,cpp} on its effect lane for the
# first of Downloads, Documents and home that the existing read-only output
# admission accepts, and only gives that to the chooser as its initial folder.
# The export is still written only to the path the reviewer chooses, through
# the unchanged local_output. Both new files join the inventory and the
# production manifest. None of the three adds a grant, target, provider,
# credential, GitHub write, mutation or publication path; mutation pin
# unchanged. Reviewed on BPR-1308, BPR-1313 and BPR-1316.
# BPR-1090 reseal (2026-10-08): over the BPR-1308/1313/1316 reseal at
# 1e277adcc; no other intervening pinned-scope commit. Owner, 2026-10-08:
# "Accept (Recommended)" for a fifth profile writing hook, Review feedback.
# github_application.cpp: the profile preferences subscription also hands the
# writing-hooks panel the published label helper, and test_writing_hook
# refuses the feedback hook (it writes no tour field) before any launch.
# tour_assist/tour_cli/tour_owner_service/tour_runtime: `hooks show` lists the
# fifth hook with its version and whether the label helper sends it, read from
# the profile's own settings; `assist context --hook feedback` and
# tour_assist_issue refuse it; an owner without the hook is refused rather
# than shown as empty. No grant, target, provider, credential, child
# environment, GitHub write, mutation or publication path changes; owner
# protocol unchanged; mutation pin unchanged; no new src/app source. Reviewed
# on BPR-1090.
# BPR-1326 reseal (2026-10-08), over the BPR-1090 reseal above.
# github_application.cpp: present_saved_drafts() hands the window every
# unpublished draft the attached shared_draft_application_host already holds
# (recovery_snapshot, read only) after each accepted draft, adoption, attach and
# ledger retirement, so the drawer draws each draft at its line whichever line
# is selected. Before, only the editor's selected-line draft was drawn, and a
# file shown again selected its first change. Tests-on only: the
# published-file-switch course and its fixture-access entry points; main.cpp
# parses its two arguments and reports its markers; the label-authoring
# course's refused-patch check skips draft notes, since the kept text is now
# drawn as my draft and still no thread holds it. No grant, target, provider,
# credential, storage write, mutation or publication path; mutation pin
# unchanged; no new src/app source. Reviewed on BPR-1326.
# BPR-1212 slice 2 reseal (2026-10-08), compose in the drawer: over the
# BPR-1326 reseal; no other intervening pinned-scope commit.
# github_application.cpp changes in three places. A comment-workshop proposal
# opens its draft in Discuss's full composer (open_draft_in_discuss), whose
# before/after needs the room; the threaded-reply and label-authoring courses
# (tests-on) start the new comment in the drawer, then Discard it there or
# Maximize to the full composer. Publish is still the draft editor's own
# action and nothing else publishes. No grant, target, provider, credential,
# storage write, GitHub write, mutation or publication path changes; mutation
# pin unchanged; no new src/app source. Self-review on BPR-1212, not
# independent review.
# BPR-1328 reseal: a90419791 (BPR-1212 slice 2, which resealed over BPR-1326)
# ..the BPR-1328 product commit; no other intervening pinned-scope change. github_application.cpp's
# dispatch_workbench_intent treats the new core::cross_workbench_change_intent
# (a change arrow continuing into the neighbouring file) as a file gesture,
# exactly as it treats navigate_workbench_file_intent: it cancels a pending
# restoration and abandons a tab load before the workbench host applies the
# intent. No grant,
# target, provider, credential, GitHub write, mutation or publication path;
# mutation pin unchanged. Reviewed on BPR-1328.
# BPR-1329 reseal: over the BPR-1328 reseal at 352b12937; github_application.cpp
# and the fixture-access header only.
# confirm_cli_publication still asks its one macOS question in the same gate
# and still re-checks the exact session after Yes; it now also names why it
# refused (declined, session or target changed, unavailable) through an
# optional out-parameter. dispatch_review_draft presents that reason, and the
# reason for every Publish its private-draft path stops before sending, on the
# draft editor; finish_review_draft_command says Publishing once the host took
# the request. Two fixture-only accessors hold or release the fixture's
# review-mutation decision gate. No grant, target, provider, credential,
# transport, GitHub write, mutation or publication path changes; mutation pin
# unchanged. Reviewed on BPR-1329.
# BPR-1212 slice 1 reseal (2026-10-07, integrated 2026-10-08): over the
# BPR-1212 slice 2, BPR-1328 and BPR-1329 reseals above, rebased onto
# 5efe30739, whose own pins match; no other intervening pinned-scope change. Slice 1 is integrated onto slice 2's
# drawer flow, so this seal covers the integrated result. github_application.cpp:
# Return and Shift+Return no longer reach the composition. The window decides
# the selection with core::decide_comment_range and has the draft editor send
# the range's first line as one ordinary revisioned edit (ChangeRange) through
# the same dispatch_review_draft and durable store every edit takes, then
# slice 2's begin_new_comment opens the draft in the drawer and writes any
# suggestion block from core::suggestion_fence_body, the one fence builder.
# The composition's begin_line_draft, complete_line_draft and their waiting
# request are removed, and core::suggestion_prefill with them.
# desktop_update_drafts names a range's lines. Tests-on only: the
# label-authoring course gains the BPR-1212 phases in the drawer (Esc to leave
# a draft, the drawer draft's Publish), and `--line-draft-course` runs them
# alone; main.cpp parses the flag and reports LINE_DRAFT_EVIDENCE. Publication
# is unchanged in this file: the proposal carries StartLine from the draft, and
# the host, the GitHub adapter and the idempotency key (outside these pins)
# refuse or encode it; a single line's bytes and key are unchanged. No grant,
# target, provider, credential or mutation path changes here; mutation pin
# unchanged; no new src/app source. Self-review on BPR-1212, not independent
# review.
# BPR-1341 reseal (2026-10-08): over the BPR-1212 slice 1 reseal above, rebased;
# no other intervening pinned-scope commit. The owner renamed the Conversations modality
# Threads. github_application.cpp: core::workbench_modality::conversations and
# qt::review_modality::conversations are renamed threads in the course's
# modality mapping, its name reads "Threads" and its centre "threads", and two
# comments say Threads. The fixture-access header's comment likewise. Words and
# identifiers only: no grant, target, provider, credential, GitHub write,
# mutation or publication path changes; mutation pin unchanged. Reviewed on
# BPR-1341.
# The same seal also covers two pinned-scope commits that reached main after
# the BPR-1212 slice 1 reseal without resealing: 7a35a4bed (BPR-1336) has View's
# zoom commands step the profile's code zoom through the existing appearance
# host and gives the comparison the zoomed code font; e8e1567c4 (BPR-1338)
# opens a pull request's page in the composed browser from an address the core
# admits against the accepted session's profile hostname (a Home queue row only
# for the authenticated viewer, a Recently viewed row only if this window
# published it), keeps the read host's markdown context across the comment
# placement's re-projection, and adds two fixture-only recording-browser
# accessors. Neither adds a grant, target, credential, GitHub write, mutation
# or publication path; mutation pin unchanged. Scoped self-review on BPR-1341
# for the pin only; BPR-1336's and BPR-1338's own reviews stay on their cards.
# BPR-1334 reseal: over the BPR-1341 reseal at d9cc2aebf, whose pins match; no
# other intervening pinned-scope commit. github_application.cpp, main.cpp, the
# fixture-access header and github_public_path_fixture.cpp. The composition
# forwards the window's explicit Line details height, or Reset, to
# WorkbenchHost::resize_line_details, which stores it in the local workbench
# settings only. The UIforETW focused-layout probe hides Line details through
# View > Line Details beside the drawers and Review details, so the comparison's
# height floors still measure the panes alone, and its stall line names that
# action's state. main.cpp's diff courses read the strip's rows instead of the
# old detail text. The constructed line-details-v1 fixture joins the test-build
# fixture enum, host-profile table, served-fixture table and --github-fixture
# names. No grant, target, provider, credential, transport, GitHub write,
# mutation or publication path changes; mutation pin unchanged. Reviewed on
# BPR-1334.
# BPR-1338 follow-up reseal (2026-10-08), over the BPR-1334 reseal above; no
# other intervening pinned-scope commit. github_application.cpp's UIforETW
# course now expects Open on GitHub (`workbenchOpenOnGithubAction`) right after
# the name, so the title row's exact order counts 18 actions and the Windows
# caption buttons move to index 18. Test evidence only: no grant, target,
# provider, credential, GitHub write, mutation or publication path changes;
# mutation pin unchanged; no new src/app source. Self-review on BPR-1338.
# BPR-1327 reseal (2026-10-08), recomputed on the merged tree over the
# BPR-1338 follow-up reseal above and BPR-1340's manifest digest at ed337d92a
# (that commit changed only the manifest value, without a note), whose pins
# match; no other intervening pinned-scope commit. github_application.cpp: a
# window prepares the files around the reviewer's cursor. After an available
# comparison, a file-list change or a cursor move it asks
# WorkbenchHost::preparation_window for at most 16 open requests (the selected
# file left out), runs EpochHost::prepare on a new profile-owned PrepareEffects
# worker under a per-window stop source, and hands the available results' keys
# to DiffHost::prepare on the decision lane. The worker is drained before the
# epoch lane at profile shutdown; window close stops it and shutdown waits for
# PreparationsInFlight. Invalidation cancels it. Tests-on only:
# diff_work_evidence reports the prepared share and counts, and
# set_preparation_enabled turns preparation off for one course's control.
# The epoch host's batched blob read goes through the existing observed
# GitHub CLI Git lease (read_exact_blobs: one GraphQL read of verified blobs
# between account observations), reads only blobs the host resolved from the
# accepted trees, and is a read. No grant, target, provider, credential,
# child environment, GitHub write, mutation or publication path; mutation pin
# unchanged; no new src/app source. Reviewed on BPR-1327.
# BPR-1342 reseal (2026-10-08): be0c3315f..this candidate, over the BPR-1327
# reseal above (its pins match at be0c3315f); no other intervening pinned-scope
# commit. Open Review in New Window from Home's rows.
# github_application.cpp admits the row a window names (a queue row only as the
# accepted account's on the accepted host, a Recently viewed row only as one the
# window published and an account's reading only under that account's exact
# authority, via core::admit_queue_review_window and
# core::admit_recent_review_window) and hands the typed destination to the
# profile desktop through github_profile_runtime::ReviewWindowRequest; the
# fresh window re-admits its parts, requires its own session to match an
# account's destination, and opens it through its Home bar's existing
# setup-destination path. profile_desktop.cpp sends the profile's ordinary
# new_bpr_window_intent carrying the destination, keeps it on that one fresh
# window's entry and opens it after Home; github_application.hpp declares
# set_review_window_request and open_window_review. Reads only, through the
# path a pasted link already takes: no grant, target, provider, credential,
# transport, GitHub write, mutation or publication path changes; mutation pin
# unchanged; no new src/app source. Self-review on BPR-1342, not independent
# review.
# BPR-1091 reseal (2026-10-08): 6752a7136..this candidate, over the BPR-1342
# reseal above (its pins match at 6752a7136); no other intervening pinned-scope
# commit. The comment workshop sends the Review feedback hook.
# github_application.cpp's dispatch_agent_invocation fills the observation's
# FeedbackGuidance from application::workshop_feedback_supplement for a
# workshop_comment gesture with an operation, reading the profile's accepted
# writing hooks and label helper; with the helper off it sets nothing and the
# press's wire is unchanged. The guidance is text inside the existing agent
# supplement on the existing launch: no grant, target, provider, credential,
# argument, child environment, tool, GitHub write, mutation or publication
# path; mutation pin unchanged; no new src/app source. Self-review on
# BPR-1091, not independent review.
# BPR-1091 macOS reseal (2026-10-08): 396941494..this candidate, over the
# BPR-1091 reseal above; no other intervening pinned-scope commit. Only
# main.cpp changes, inside BPR_BUILD_TESTING: `--attended-workshop-agent`
# (with `--attended-open-file`) names the fixture Codex executable and turns
# the label helper on, then, after the file is shown, writes a labelled
# question through the Comment action, saves a Review feedback override on the
# shipped Settings row, presses Clarify and converts the delivered finding on
# the queue, and prints one ATTENDED_WORKSHOP_ line. Optional
# BPR_ATTENDED_WORKSHOP_THEME/RENDER give an offscreen look. Fixture-only;
# no grant, target, provider, credential, child environment, tool, GitHub
# write, mutation or publication path; full-application and mutation pins
# unchanged; no new src/app source. Self-review on BPR-1091, not independent
# review.
# BPR-1092 inventory (2026-10-08): tour_feedback.cpp and tour_feedback.hpp
# enter the tree inventory, in the tour runtime library only: `bpr tour labels`
# names what the core catalog and recognizer, core::plan_feedback_tour_stop_edit
# and the application counter model decide. No pinned source changed (no
# github_application.cpp or production-manifest edit), so no digest moves; no
# grant, target, provider, mutation or publication path; mutation pin unchanged.
# BPR-1338 attended-browser reseal (2026-10-08): c8ae21f08..this candidate,
# over the BPR-1092 inventory above (pins match at c8ae21f08); no other
# intervening pinned-scope commit. The owner pressed Open on GitHub in a
# frozen attended build and no browser opened: every fixture run bound the
# recording browser for the window's opens as well as device sign-in.
# github_application.cpp: the window's own opens (pull-request page, body
# links) use github_profile_runtime::WindowBrowser, which is ComposedBrowser
# except in a tests-enabled graph whose fixture options set
# AttendedWindowBrowser, where it is the existing system_browser_opener.
# Device sign-in is unchanged. main.cpp, inside BPR_BUILD_TESTING: the option
# is set only for --attended-launch outside the automated test boundary, and
# --attended-open-on-github (with --attended-open-pull-request) presses the
# window's action once and prints one ATTENDED_OPEN_ON_GITHUB_ line. Product
# binding without a fixture is unchanged; no grant, target, provider,
# credential, child environment, GitHub write, mutation or publication path;
# mutation pin unchanged; no new src/app source. Self-review on BPR-1338.
# BPR-1328 v003 reseal (2026-10-08): over the BPR-1338 attended-browser reseal
# above; no other intervening pinned-scope change. The owner rejected change arrows that cross
# into the next file ("Just let it be a bounded prev and next that you can
# always use"), so core::cross_workbench_change_intent is removed, and with it
# the one clause of dispatch_workbench_intent that treated it as a file
# gesture; the line is back to select_ and navigate_workbench_file_intent
# alone. No grant, target, provider, credential, GitHub write, mutation or
# publication path; mutation pin unchanged; no new src/app source. Reviewed on
# BPR-1328.
# BPR-1328 v003 reseal, rebased: over 7dfde8915 (BPR-1331), which changed
# github_application.cpp and main.cpp without moving these pins. Read here, not
# BPR-1331's own review: the private-progress receipt attaches to
# core::file_material instead of core::comparison_digest (the same dimensions
# without the epoch anchors), and main.cpp gains the fixture-only
# --review-progress-push-probe course and an ATTENDED_HEAD line inside
# BPR_BUILD_TESTING. No grant, target, provider, credential, GitHub write,
# mutation or publication path; mutation pin unchanged; no new src/app source.
# BPR-1094 reseal (2026-10-08): over the BPR-1328 v003 rebased reseal above,
# whose pins match at a5cba14fb (checked in a detached worktree); no other
# intervening pinned-scope commit. github_application.cpp: the window counts
# each observation of its label buckets and the bodies it parsed
# (FeedbackObservations, FeedbackBodiesParsed), and scaling_evidence reports
# them with the model's feedback_counter_work and the Overview count
# publications; github_application_fixture_access.hpp names the four fields
# inside BPR_BUILD_TESTING. Evidence counters only: no decision, grant, target,
# provider, credential, GitHub write, mutation or publication path; mutation
# pin unchanged; no new src/app source. Reviewed on BPR-1094.
# BPR-1331 v003 reseal (2026-10-08): over the BPR-1094 reseal above; no other
# intervening pinned-scope change. The owner rejected two pictures of one
# private-progress state, so apply_appearance hands the window the theme's
# progress mark colours (qt::progress_mark_colours_for, a presentation-only
# struct of five QColors read from the appearance profile) beside the attention
# amber it already hands over, and the file includes bpr/qt/progress_mark.hpp.
# No grant, target, provider, credential, GitHub write, mutation or publication
# path; mutation pin unchanged; no new src/app source. Self-review on BPR-1331.
# BPR-1328 follow-up reseal (2026-10-09): over the pins as they stood at
# 052702a54 (the full digest matched there). dda61041d left Next file enabled
# and bounded on the last file, but exercise_workbench_composition still
# required it disabled, so application_workbench_composition_contract failed.
# The probe's ProjectionDelivery clause now requires the arrow enabled with the
# status tip "Last file in this pull request.". Fixture-probe evidence only: no
# decision, grant, target, provider, credential, GitHub write, mutation or
# publication path; mutation pin unchanged; no new src/app source. Recorded on
# BPR-1328.
# BPR-1357 reseal (2026-10-09): over the BPR-1328 follow-up reseal above; no other
# intervening pinned-scope change. Home says at once that a review is opening:
# the window's read-lane publication feeds application::home_open_host in the
# same turn; its wait is one QTimer::singleShot on the window; Cancel calls the
# read host's existing cancel() only when the core admits the request, and
# Retry is an ordinary open through dispatch_intent. apply_appearance hands the
# window the theme's failure ink. Read-only presentation and an existing
# cancellation; no grant, target, provider, credential, GitHub write, mutation
# or publication path; mutation pin unchanged; no new src/app source. The
# tests-on fixture gains a cancellable read delay. Self-review on BPR-1357.
# BPR-1212 v002 reseal, rebased onto main (bpr-main-win, 2026-10-09): the
# branch's reviewed v002 change, carried onto 1428dad74 with every intervening
# pinned-scope reseal kept. The change itself, as reviewed on the branch:
# github_application.cpp asks the epoch host once per selected comparison
# whether its left document is the merge base's version of the file
# (resolve_github_base_numbering, a read of the retained Git objects; no
# fetch, no GitHub read), keeps the answer (BaseNumbering), and only with
# "same" sets the window's left endpoint, conversation_endpoint_is_current for
# the left side, the source material's BaseNumberedAsGitHub, and a draft for a
# base line in present_review_draft. A new root comment on a base line is then
# publishable as GitHub's side LEFT; the mutation host admits it only with the
# same fact. Tests-on: the line-draft course gains three phases and main.cpp
# prints BASE_LINE_DRAFT_EVIDENCE; the fixture-access header names the three
# facts. No grant, target, credential or transport change; mutation pin
# unchanged; no new src/app source. The rebase adds no change of its own.
# BPR-1212 v002 base-numbering reseal (bpr-main-win, 2026-10-09): over the
# rebased v002 reseal above; no other intervening pinned-scope change.
# github_application.cpp asks the base-numbering question again when an
# accepted review keeps the comparison and the answer is neither known nor
# pending, and re-arms it (25 ms, at most 80 in a row) after a cancelled
# answer or a refused admission. Testing only: a fixture-access switch turns
# the next answers into cancelled, and main.cpp's line-draft course takes
# --cancel-first-base-numbering to set it. Reads only; no grant, target,
# provider, credential, GitHub write, mutation or publication path; mutation
# pin unchanged; no new src/app source.
# BPR-1365 reseal (2026-10-09): over the BPR-1212 v002 base-numbering reseal
# above; no other intervening pinned-scope change. github_application.cpp
# marks the Recently viewed entry whose destination is this window's
# ReadingDestination as Open, and publishes Recently viewed again when that
# destination is set by an accepted read or handoff or cleared by an
# invalidation, so Home can draw the open review apart from a selected row.
# Presentation of an existing projection only; no grant, target, provider,
# credential, GitHub write, mutation or publication path; mutation pin
# unchanged; no new src/app source.
# BPR-1345 start-a-review reseal (bpr-main-mac, 2026-10-09): over the
# BPR-1365 reseal above. github_application.cpp keeps a
# pending mark on the shown draft through the editor's edit path (its path and
# head decided by the mutation host's comment_destination, as Publish decides
# them), passes the pending drafts to the review submission workflow as its
# line comments, holds them in one store transaction before the submission is
# sent, completes or releases them on its answer, and clears the marks on
# Discard review; Add single comment takes a pending draft out of the review
# in the same revision, and a draft a submission holds is never published on
# its own. The review submission's one POST now carries the comments; its
# grant, target and actor binding are unchanged. Testing only: the
# start-a-review course and its fixture-access evidence. Mutation pin
# unchanged; no new src/app source.
# BPR-1369 reseal (bpr-main-linux, 2026-10-09): over the BPR-1345
# start-a-review reseal above, whose pins match at origin/main f3c65bc6a
# (checked in a detached worktree); BPR-1360's commits change no src/app
# source. Testing only: inside BPR_BUILD_TESTING, github_application.cpp hands
# the diff host a counting_diff_blob_port that forwards materialize and
# materialize_both to the epoch host unchanged and counts each blob it
# answers, and fixture access reports the count for one blob
# (diff_blob_reads); github_application_fixture_access.hpp declares it. A
# tests-off build hands the epoch host over directly, as before. Evidence
# counter only: no decision, grant, target, provider, credential, GitHub
# write, mutation or publication path; mutation pin unchanged; no new src/app
# source.
# BPR-1345 v002 reseal (bpr-main-mac, 2026-10-09): over the BPR-1369 reseal
# above; the BPR-1367 and BPR-1368 commits before it touched only
# review_submission_fixture.cpp and left both pins matching.
# github_application.cpp's Discard review deletes every pending draft (discard
# commands in one store transaction) instead of clearing the marks, and a new
# delete_pending_comment discards one pending draft at the revision Finish your
# review listed; review_submission_workflow.cpp starts a new review draft after
# any discard and when pending comments arrive while the shown draft is
# submitted or discarded, so a finished review never takes the next one's
# comments (the stale state behind the owner's "Review draft discarded.").
# Testing only: main.cpp's `again` start-review course and its fixture-access
# evidence, and an optional render directory for the look; the threaded-reply
# course confirms the drawer's Delete question; the UIforETW review-bar order
# check counts the review chip v001 added before Compose review. No grant, target,
# provider, credential, GitHub write or publication path changes; mutation pin
# unchanged; no new src/app source.
# BPR-1345 v002 follow-up reseal (bpr-main-mac, 2026-10-09): over the v002
# reseal above (1cb430214). Testing only: the `again` course also presses a
# Finish your review row and checks it went to its comment's line in Review
# (RowRevealed in the fixture-access evidence, main.cpp's evidence line).
# Mutation pin unchanged; no new src/app source.
# BPR-1345 v003 reseal (bpr-main-mac, 2026-10-09): over the v002 follow-up
# reseal above, rebased onto 3084597af (BPR-1367's fixture change and
# BPR-1370's GitHub adapter change left both pins matching). github_application.cpp: testing only. The start-review
# course checks that a confirmed review closes Finish your review and returns
# to Review with "Review submitted with N comments" in the status bar, that a
# refused or unknown answer keeps the page, reads the shared destructive
# question (`question()`, not QMessageBox's `text()`), and renders the
# questions over the window in the four themes. review_submission_workflow.cpp
# starts the next review at once after a confirmed submission (as after a
# discard), restores the next unfinished one instead of waiting for New review,
# and tells the editor which review was confirmed. No grant, target, provider,
# credential, GitHub write or publication path changes; mutation pin
# unchanged; no new src/app source.
# BPR-1344 reseal (bpr-main-win, 2026-10-09): over main's latest reseal; no
# other intervening pinned-scope change. github_application.cpp: a preparation
# round starts only while the reviewer has no diff, workbench or active epoch
# work and no classification still to arrive. A round deferred meanwhile is
# resumed from the decision lane: the window's epoch, diff and workbench hosts
# post through a wrapper that re-checks after each of their tasks (its
# callback a weak handle, since the lane is the profile's), and an adopted
# classification re-checks too. A prune, a cache delete or any cancellation
# cancels preparation and empties the diff host's prepared queue. The
# fixture-access header reports the epoch host's reviewer and preparation job
# counts and a pending round. Scheduling and testing evidence only; no grant,
# target, provider, credential, GitHub write, mutation or publication path;
# mutation pin unchanged; no new src/app source.
# BPR-1345 reseal (bpr-main-win, 2026-10-09): over BPR-1344's reseal; no other
# intervening pinned-scope change. github_application.cpp: the fixture
# helper that reveals a file in Review finds the file's drawer row again
# after choosing Review, which can re-project the drawer, instead of
# selecting the stale row it found before. Fixture access only; no grant,
# target, provider, credential, GitHub write, mutation or publication path;
# mutation pin unchanged; no new src/app source.
# BPR-1374 reseal (2026-10-09), over the reseals above.
# github_application.cpp: present_saved_drafts() also hands the window where
# each unsent reply stands (core::decide_reply_drafts over
# application::observe_reply_threads of the accepted snapshot), and does so
# again when the conversations are published. compose_review_mutation()
# remembers the reply the editor held; resume_reply()/resume_reply_at() hold it
# again once its line is chosen after the Refresh, as
# core::resume_reply_after_refresh decides. begin_review_reply() opens a saved
# unsent reply on its thread (core::reopen_draft_at) before converting the
# shown draft. A kept reply's Keep as comment and Discard go through the
# existing private change_draft (core::keep_reply_as_comment,
# core::discard_unsent_reply). Publish refuses a reply whose thread the
# accepted snapshot no longer holds. private_review_workflow.cpp's line and
# reply selection go through review_composition_application_host::select_draft.
# Tests-on only: the reply-draft course, its fixture-access entry points and
# the fixture's staged thread deletion; main.cpp parses --reply-draft-course and
# reports its markers. No grant, target, provider, credential, mutation or
# publication path is added; mutation pin unchanged; no new src/app source.
# Reviewed on BPR-1374.
# BPR-1222 reseal (2026-10-09, bpr-main-linux, at the coordinator's request):
# over the BPR-1374 reseal above. Two pinned-scope commits landed without one:
# BPR-1373's 170c706c0 (each gh call pinned to a chosen account; composition
# records which profile uses which pinned gh account for Accounts & hosts'
# "Used by", plus tour_cli.cpp, tour_remote.cpp, github_cli_runtime.cpp and
# review_submission_workflow.{hpp,cpp}) and BPR-1222's 9cab2ad75 (live tour
# publication on; stop links open the Files changed tab). Both pinned values
# are resealed here; the production mutation-branch pin did not move, so
# neither changed a mutation path. Resealed as the drift fix for the
# pre-release build; BPR-1373's account pinning was reviewed on its own card.
# BPR-1382 reseal (2026-10-10), over 611abc31c: the only intervening
# manifest member change is 636c885c2's tests-on main.cpp mouse-event helper
# rename (deliver -> send_mouse_event), with identical arguments and calls.
# This slice changes syntax adoption in github_application.cpp to the surface's
# exact loading/display context. The Next/Previous regression checks cached
# large documents and both panes' rendered formats and fails with the old
# context restored. Self-review recorded on BPR-1382; no provider, credential,
# mutation or source-inventory change. Production mutation pin unchanged.
# BPR-1387 reseal (2026-10-11), over c19d159e4: github_application.cpp is
# the only changed production-manifest member. Engineering self-review covers
# reusing the already mapped conversation projection when no source is selected,
# clearing the placement cache in that case, and avoiding a second publication
# when initial setup clears no displayed comparison. A real comparison still
# clears and reprojects; selected-source placement keeps its Markdown identity.
# No request, credential, mutation, grant or source-inventory change. The
# production mutation-branch pin is unchanged. Focused public-path and unchanged
# performance-budget evidence is recorded on BPR-1387 and its verification note.
# BPR-1403 reseal (2026-10-12), over 493d08433: the only intervening
# production-manifest changes are f170654e6 and 393dfe602. The first carries
# accepted optional Draft facts through exact account-bound queue/Recent
# destinations, and changes tests-on Home fixtures in main.cpp. The second
# carries accepted read-only waiting/authorship facts and dispatch revisions
# through the existing queue host; no new enrollment or notification authority.
# Scoped source review and prior accepted RFC0067/0070 evidence are recorded
# on BPR-1403. No provider selection, credential, grant, mutation, publication
# or production source-inventory change; mutation-branch pin stays unchanged.
# BPR-1404 reseal (2026-10-12), over last-matching 3cc1b2fc3: the only
# intervening src/app commits are 8cf67d793 and f5e79380c. The latter removes
# every temporary github_application diagnostic, restoring its full source pin.
# Its private workflow follows a retired read-only draft restore into the same
# current PR context and comparison, keeping the serialized store/decision lanes,
# shutdown/context guards and all mutation revision/authority checks. Stable
# store errors are returned normally. The other manifest edits reserve an
# existing vector bound and make the existing virtual destructor explicit.
# No source inventory or mutation-branch change. Scoped review and deterministic
# old-code/refusal controls are recorded on BPR-1404 and its release gate ledger.
# BPR-1404/BPR-1407 reseal (2026-10-12), over a0ba57706: only
# 8cc80c6ed changes a production manifest member before this repair. Its
# github_application.cpp supplies the existing discovery clock to Home and
# retains its profile-owned reference; the tests-only public fixture adds five
# already-read pull-request facts. No new effect, request or source member.
# This repair factors the existing bounded tests-only ledger-lane hold and
# exposes the same hold for private draft persistence. It pins the labelled
# publication course's genuinely pending ordering while its public menu turns
# events; no production mutation, authority or persistence rule changes.
# The production mutation-branch pin remains e067befefeff198fdbd992259b2ee8ea105520d50ddf7125e3b091519cb8a5df.
# BPR-1407's AC4 owner look remains pending in its separate attended session.
# BPR-1429 post-critical-review M3 repair (2026-10-13), over frozen611c:
# exact publishing knowledge is captured before a retry and merged in core with
# the current outcome. Early capability/coordinator refusals retain the original
# locked draft and journal identity; only an exact receipt resolves uncertainty.
# Qt presents unknown plus the current refusal, with the bound account/server.
# The fixture's existing acquisition seam now covers coordinator observations.
# No grant, actor/target/head/content admission, provider selection, publication
# journal transition, source inventory or production mutation-branch change.
# The composed old-library regression fails; repeated cuts then restore must
# resolve exactly one comment with no second POST. M3 independent recheck pending.
function(bpr_lf_normalized_sha256 content out_variable)
    string(REPLACE "\r\n" "\n" bpr_normalized "${content}")
    string(REPLACE "\r" "\n" bpr_normalized "${bpr_normalized}")
    string(SHA256 bpr_normalized_digest "${bpr_normalized}")
    set(${out_variable} "${bpr_normalized_digest}" PARENT_SCOPE)
endfunction()

function(bpr_compute_full_application_digest source_variable out_variable)
    bpr_lf_normalized_sha256("${${source_variable}}" bpr_full_digest)
    set(${out_variable} "${bpr_full_digest}" PARENT_SCOPE)
endfunction()

# BPR-1664 diagnostic repair over 64b15f6fc: test-only bounded live
# counting/self-check ceiling follows the complete fifteen-send single-page
# content/timeline/merge-base audit, with independent sequence/page controls.
# No product provider/mutation/authority change; native failure and recheck
# are retained on BPR-1664.
#
# BPR-1664: over 12ef1db53, only the supported-platform public-preview
# selection changes. Anonymous authority/provider/refusals are reused exactly;
# no mutation-branch or source inventory changes. Native courses own acceptance.
#
# BPR-1614: reviewed passive Work observer wiring, bounded display labels,
# dispatcher latency observation and shutdown drain. No grant, credential,
# provider or mutation admission changes; mutation-branch digest unchanged.

# BPR-1694 atomic-reading reseal over matching e031a961a. Only
# github_application.cpp changes in the pinned manifest: the already guarded
# pending reading gesture completes ready and accepts the current selection
# through one core transition before any projection/persistence callback.
# Existing request/material/generation guards and ordinary completions remain.
# No provider, credential, grant, mutation or publication branch changes.
# Core refusal is atomic for stale/invalid input; host callback counterexamples
# and review are recorded on BPR-1694. Mutation pin stays unchanged.
function(bpr_verify_full_application_digest source_variable)
    bpr_compute_full_application_digest(
        ${source_variable}
        bpr_full_application_digest
    )
	# BPR-1679 reseal (2026-10-20), over 7a96faade: Home's discovery
	# gives way to a window's pending pull request read. Each window observer
	# exposes its read lane's core state; the profile asks
	# core::home_loading_yields after each window read update, profile
	# presentation and window close, holds discovery at once and posts its
	# resumption. No provider selection, credential, grant, mutation,
	# publication or source-inventory change; mutation-branch pin unchanged.
	# BPR-1683 reseal (2026-10-20), over 2085a4d6e: the window's Home
	# Ignore and Unignore reach the profile queue host as a local
	# core::home_ignore_intent, beside Follow. Nothing reaches a provider;
	# no credential, grant, mutation, publication or source-inventory
	# change; mutation-branch pin unchanged.
	set(bpr_expected_full_application_digest
		"d7325078364fd29979da433cf5cb42c1c5592c69bae2776e853e78ad1b408b03"
	)
    if(NOT bpr_full_application_digest STREQUAL
        bpr_expected_full_application_digest
    )
        message(FATAL_ERROR
            "full LF-normalized github_application.cpp digest drifted: "
            "expected ${bpr_expected_full_application_digest}, found "
            "${bpr_full_application_digest}"
        )
    endif()
endfunction()

function(bpr_compute_production_app_manifest_digest app_source_root out_variable)
    set(bpr_production_app_sources
        "include/bpr/app/command_line.hpp"
        "include/bpr/app/diagnostic_controls.hpp"
        "include/bpr/app/diagnostic_export_directory.hpp"
        "include/bpr/app/foundation_application.hpp"
        "include/bpr/app/github_application.hpp"
        "include/bpr/app/native_filesystem_path.hpp"
        "include/bpr/app/profile_data_reset.hpp"
        "include/bpr/app/profile_desktop.hpp"
        "include/bpr/app/release_maintenance_controls.hpp"
        "include/bpr/app/release_update_desktop.hpp"
        "include/bpr/app/worker_executor.hpp"
		"main.cpp"
		"src/about_desktop.cpp"
		"src/about_desktop.hpp"
		"src/command_line.cpp"
		"src/designated_live_macos_supervisor.cpp"
		"src/designated_live_macos_supervisor_identity.cpp"
		"src/designated_live_macos_supervisor_identity.hpp"
		"src/designated_live_macos_supervisor_launch.cpp"
		"src/designated_live_macos_supervisor_launch.hpp"
        "src/diagnostic_controls.cpp"
        "src/diagnostic_export_directory.cpp"
        "src/foundation_application.cpp"
        "src/github_application.cpp"
        "src/installed_resources.cpp"
        "src/installed_resources.hpp"
        "src/installed_runtime.cpp"
        "src/installed_runtime.hpp"
        "src/pane_find_controller.cpp"
        "src/pane_find_controller.hpp"
        "src/platform_credential_vault.cpp"
        "src/platform_credential_vault.hpp"
        "src/pr_search_controller.cpp"
        "src/pr_search_controller.hpp"
        "src/private_review_workflow.cpp"
        "src/private_review_workflow.hpp"
        "src/profile_data_reset.cpp"
        "src/profile_desktop.cpp"
        "src/profile_window_layout_port.hpp"
        "src/release_maintenance_controls.cpp"
        "src/release_update_desktop.cpp"
        "src/review_submission_workflow.cpp"
        "src/review_submission_workflow.hpp"
        "src/startup_diagnostic.hpp"
        "src/terminal_diagnostics.cpp"
        "src/terminal_diagnostics.hpp"
		"src/windows_title_row_host.cpp"
		"src/windows_title_row_host.hpp"
		"src/worker_executor.cpp"
		"src/xcb_activation_host.cpp"
		"src/xcb_activation_host.hpp"
        "update_helper_main.cpp"
    )
    set(bpr_production_app_manifest "")
    foreach(relative_source IN LISTS bpr_production_app_sources)
        file(READ
            "${app_source_root}/${relative_source}"
            bpr_app_source_contents
        )
        bpr_lf_normalized_sha256("${bpr_app_source_contents}" bpr_app_source_digest)
        string(APPEND
            bpr_production_app_manifest
            "${relative_source}\n${bpr_app_source_digest}\n"
        )
    endforeach()
    string(SHA256
        bpr_manifest_digest
        "${bpr_production_app_manifest}"
    )
    set(${out_variable} "${bpr_manifest_digest}" PARENT_SCOPE)
endfunction()

function(bpr_verify_app_source_tree app_source_root)
    if(NOT IS_DIRECTORY "${app_source_root}")
        message(FATAL_ERROR
            "application source-tree inventory root does not exist: "
            "${app_source_root}"
        )
    endif()
    set(bpr_expected_app_sources
        "include/bpr/app/command_line.hpp"
        "include/bpr/app/diagnostic_controls.hpp"
        "include/bpr/app/diagnostic_export_directory.hpp"
        "include/bpr/app/foundation_application.hpp"
        "include/bpr/app/github_application.hpp"
        "include/bpr/app/native_filesystem_path.hpp"
        "include/bpr/app/profile_data_reset.hpp"
        "include/bpr/app/profile_desktop.hpp"
        "include/bpr/app/release_maintenance_controls.hpp"
        "include/bpr/app/release_update_desktop.hpp"
        "include/bpr/app/worker_executor.hpp"
        "main.cpp"
        "src/about_desktop.cpp"
        "src/about_desktop.hpp"
        "src/command_line.cpp"
        "src/cycle_cli.cpp"
        "src/cycle_cli.hpp"
        "src/cycle_commands.cpp"
        "src/cycle_commands.hpp"
        "src/cycle_owner_service.cpp"
        "src/cycle_owner_service.hpp"
        "src/cycle_session.cpp"
        "src/cycle_session.hpp"
        "src/designated_live_macos_capsule_identity.cpp"
        "src/designated_live_macos_capsule_identity.hpp"
        "src/designated_live_macos_provenance.cpp"
        "src/designated_live_macos_provenance.hpp"
        "src/designated_live_macos_supervisor.cpp"
        "src/designated_live_macos_supervisor_identity.cpp"
        "src/designated_live_macos_supervisor_identity.hpp"
        "src/designated_live_macos_supervisor_launch.cpp"
        "src/designated_live_macos_supervisor_launch.hpp"
        "src/diagnostic_controls.cpp"
        "src/diagnostic_export_directory.cpp"
        "src/foundation_application.cpp"
        "src/github_application.cpp"
        "src/github_application_fixture_access.hpp"
        "src/github_cli_runtime.cpp"
        "src/github_cli_runtime.hpp"
        "src/github_public_path_fixture.cpp"
        "src/github_public_path_fixture.hpp"
        "src/installed_resources.cpp"
        "src/installed_resources.hpp"
        "src/installed_runtime.cpp"
        "src/installed_runtime.hpp"
        "src/large_review_fixture.hpp"
        "src/pane_find_controller.cpp"
        "src/pane_find_controller.hpp"
        "src/platform_credential_vault.cpp"
        "src/platform_credential_vault.hpp"
        "src/pr_search_controller.cpp"
        "src/pr_search_controller.hpp"
        "src/private_review_fixture_course.cpp"
        "src/private_review_fixture_course.hpp"
        "src/private_review_workflow.cpp"
        "src/private_review_workflow.hpp"
        "src/profile_admission.cpp"
        "src/profile_admission.hpp"
        "src/profile_data_reset.cpp"
        "src/profile_desktop.cpp"
        "src/profile_window_layout_port.hpp"
        "src/release_maintenance_controls.cpp"
        "src/release_update_desktop.cpp"
        "src/review_cycle_desktop.cpp"
        "src/review_cycle_desktop.hpp"
        "src/review_submission_fixture.cpp"
        "src/review_submission_fixture.hpp"
        "src/review_submission_workflow.cpp"
        "src/review_submission_workflow.hpp"
        "src/startup_diagnostic.hpp"
        "src/system_browser_opener.cpp"
        "src/system_browser_opener.hpp"
        "src/terminal_diagnostics.cpp"
        "src/terminal_diagnostics.hpp"
        "src/terminal_text.cpp"
        "src/terminal_text.hpp"
        "src/tour_assist.cpp"
        "src/tour_assist.hpp"
        "src/tour_cancellation.cpp"
        "src/tour_cancellation.hpp"
        "src/tour_cli.cpp"
        "src/tour_cli.hpp"
        "src/tour_cli_types.hpp"
        "src/tour_decisions.hpp"
        "src/tour_editor.cpp"
        "src/tour_editor.hpp"
        "src/tour_feedback.cpp"
        "src/tour_feedback.hpp"
        "src/tour_main.cpp"
        "src/tour_observation.hpp"
        "src/tour_owner_service.cpp"
        "src/tour_owner_service.hpp"
        "src/tour_publication_plan.cpp"
        "src/tour_publication_plan.hpp"
        "src/tour_remote.cpp"
        "src/tour_runtime.cpp"
        "src/tour_runtime.hpp"
        "src/tour_text_input.cpp"
        "src/tour_text_input.hpp"
        "src/windows_title_row_host.cpp"
        "src/windows_title_row_host.hpp"
        "src/worker_executor.cpp"
        "src/xcb_activation_host.cpp"
        "src/xcb_activation_host.hpp"
        "update_helper_main.cpp"
    )
    file(GLOB_RECURSE bpr_actual_app_sources
        LIST_DIRECTORIES false
        RELATIVE "${app_source_root}"
        "${app_source_root}/*.c"
        "${app_source_root}/*.cc"
        "${app_source_root}/*.cpp"
        "${app_source_root}/*.cxx"
        "${app_source_root}/*.h"
        "${app_source_root}/*.hpp"
    )
    list(SORT bpr_actual_app_sources)
    if(NOT bpr_actual_app_sources STREQUAL bpr_expected_app_sources)
        message(FATAL_ERROR
            "application source-tree inventory drifted: expected "
            "${bpr_expected_app_sources}, found ${bpr_actual_app_sources}"
        )
    endif()

    bpr_compute_production_app_manifest_digest(
        "${app_source_root}"
        bpr_production_app_manifest_digest
    )
	set(bpr_expected_production_app_manifest_digest
        "941942272c42ba1bb4939f899569b81b16779d4105591a79c38cf7eeba3da9d0"
	)
    if(NOT bpr_production_app_manifest_digest STREQUAL
        bpr_expected_production_app_manifest_digest
    )
        message(FATAL_ERROR
            "production application source content digest drifted: expected "
            "${bpr_expected_production_app_manifest_digest}, found "
            "${bpr_production_app_manifest_digest}"
        )
    endif()
endfunction()

function(bpr_verify_replay_operator_surface app_source_root)
    file(READ "${app_source_root}/main.cpp" bpr_main_source)
    file(READ
        "${app_source_root}/include/bpr/app/github_application.hpp"
        bpr_app_header_source
    )
    foreach(required IN ITEMS
        "--initialize-or-reset-designated-live-replay-state"
        "argc != 2"
        "initialize_or_reset_designated_live_replay_state("
        "DESIGNATED_LIVE_REPLAY_STATE_READY"
        "config.ApplicationDataRoot"
    )
        string(FIND "${bpr_main_source}" "${required}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "replay operator/startup surface omitted ${required}"
            )
        endif()
    endforeach()
    foreach(required IN ITEMS
        "initialize_or_reset_designated_live_replay_state("
        "This is an operator operation, not an application-startup"
    )
        string(FIND "${bpr_app_header_source}" "${required}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "replay operator API omitted ${required}"
            )
        endif()
    endforeach()
    bpr_require_literal_count(
        bpr_main_source
        "--initialize-or-reset-designated-live-replay-state"
        1
        "no-path replay initializer argument"
    )
    bpr_require_literal_count(
        bpr_main_source
        "initialize_or_reset_designated_live_replay_state("
        1
        "replay initializer invocation"
    )
    foreach(forbidden IN ITEMS
        "designated-live-review-replay-state-v1"
        "NamespaceId"
        "QTextStream{ stdout } << applicationDataRoot"
    )
        string(FIND "${bpr_main_source}" "${forbidden}" found)
        if(NOT found EQUAL -1)
            message(FATAL_ERROR
                "replay path or identity leaked into the CLI surface: ${forbidden}"
            )
        endif()
    endforeach()
endfunction()

function(bpr_verify_async_mutation_host_inventory source_variable)
    set(bpr_async_mutation_source "${${source_variable}}")
    foreach(required IN ITEMS
        "#include \"live_review_mutation_provider.hpp\""
        "using adapters::github::make_live_review_mutation_provider;"
        "auto provider = make_live_review_mutation_provider("
		"adapters::sqlite::make_review_mutation_recovery_store("
		"std::move(*recovery)"
		"application::detail::designated_review_authority_bootstrap::run("
		"application::detail::designated_review_replay_initializer::load("
		".Replay             = std::move(*replay)"
        "adapters::github::detail::make_designated_review_public_observer("
        "ReviewMutationHost = std::make_unique<"
        "application::review_mutation_application_host>("
        "std::move(LiveReviewMutationGrant)"
        "std::unique_ptr<application::review_mutation_application_host>"
    )
        string(FIND "${bpr_async_mutation_source}" "${required}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "asynchronous mutation-host inventory omitted ${required}"
            )
        endif()
    endforeach()
    # [[BPR-1228]]. The member is found by its type and name; the run of
    # spaces between them is clang-format's alignment, not the contract.
    string(REGEX MATCH
        "worker_executor[ \t]+ReviewMutationEffects;"
        bpr_mutation_effects_member
        "${bpr_async_mutation_source}"
    )
    if(bpr_mutation_effects_member STREQUAL "")
        message(FATAL_ERROR
            "asynchronous mutation-host inventory omitted worker_executor "
            "ReviewMutationEffects;"
        )
    endif()
    bpr_require_literal_count(
        bpr_async_mutation_source
        "application::detail::review_mutation_user_authority_issuer::issue("
        1
        "normal exact user-publish issuer"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "make_live_review_mutation_provider("
        1
        "live review mutation provider factory call"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "ReviewMutationHost->publish("
        1
        "review mutation host publication call"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "ReviewMutationHost = std::make_unique<"
        2
        "fixture and production mutation host construction"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "std::unique_ptr<application::review_mutation_port>"
        1
        "production provider erasure"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "review_mutation_port"
        1
        "whole-app mutation port reference"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "make_review_mutation_testing_grant("
        1
        "fixture-only mutation grant factory call"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
		"application::detail::designated_review_authority_bootstrap::run("
        1
        "attended production authority bootstrap call"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
		"application::detail::designated_review_replay_initializer::load("
        1
        "attended replay-state load"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
		"application::detail::designated_review_replay_initializer::initialize_or_reset("
        1
        "explicit replay-state operator operation"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
		".Replay             = std::move(*replay)"
        1
        "explicit replay configuration handoff"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "adapters::github::detail::make_designated_review_public_observer("
        1
        "attended public observation adapter construction"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "std::move(LiveReviewMutationGrant)"
        1
        "production one-use execution grant handoff"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "application::review_mutation_execution_grant{ }"
        2
        "per-gesture authority starts absent"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "review_mutation_coordinator"
        0
        "whole-app direct mutation coordinator reference"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        ".observe_target("
        0
        "whole-app inferred provider observation"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "->observe_target("
        0
        "whole-app provider observation"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        ".submit("
        0
        "whole-app inferred direct submit"
    )
    # Four sites: the ledger host's command submit and the three
    # SetupDestination submits (the Home paste field, since BPR-0733 a
    # Recently viewed entry opened by the same path, and since BPR-1342 the
    # pull request a fresh window's New Window carried, from its admitted
    # parts).
    bpr_require_literal_count(
        bpr_async_mutation_source
        "->submit("
        4
        "whole-app pointer submit allowlist"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "SetupDestination->submit(application::pull_request_page_address(*page))"
        1
        "a new window's carried pull request is local navigation, not review publication"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "ReviewLedgerHost->submit("
        1
        "review-ledger pointer submit allowlist"
    )
    bpr_require_literal_count(
        bpr_async_mutation_source
        "SetupDestination->submit(aValue.Text)"
        1
        "local setup text navigation is not review publication"
    )
    foreach(forbidden IN ITEMS
        "LiveReviewMutationProvider"
        "ReviewMutationCoordinator->publish("
        "provider->observe_target("
        "provider->submit("
        "(*provider)->observe_target("
        "(*provider)->submit("
        "erased->observe_target("
        "erased->submit("
    )
        string(FIND "${bpr_async_mutation_source}" "${forbidden}" found)
        if(NOT found EQUAL -1)
            message(FATAL_ERROR
                "concrete provider bypasses the asynchronous host with an "
                "observe, submit, or publish action: ${forbidden}"
            )
        endif()
    endforeach()
endfunction()

function(bpr_compute_production_mutation_host_digest source_variable out_variable)
    set(bpr_mutation_branch_source "${${source_variable}}")
    string(FIND
        "${bpr_mutation_branch_source}"
        "void compose_review_mutation("
        bpr_mutation_compose_begin
    )
    string(FIND
        "${bpr_mutation_branch_source}"
		"void refresh_review_projection("
        bpr_mutation_compose_end
    )
    if(bpr_mutation_compose_begin EQUAL -1
        OR bpr_mutation_compose_end EQUAL -1
        OR bpr_mutation_compose_end LESS_EQUAL bpr_mutation_compose_begin
    )
        message(FATAL_ERROR
            "review mutation composition boundary is missing for digest lock"
        )
    endif()
    math(EXPR bpr_mutation_compose_length
        "${bpr_mutation_compose_end} - ${bpr_mutation_compose_begin}"
    )
    string(SUBSTRING
        "${bpr_mutation_branch_source}"
        ${bpr_mutation_compose_begin}
        ${bpr_mutation_compose_length}
        bpr_mutation_compose_source
    )
    string(FIND
        "${bpr_mutation_compose_source}"
        "#else"
        bpr_mutation_production_marker
    )
    if(bpr_mutation_production_marker EQUAL -1)
        message(FATAL_ERROR
            "tests-disabled production review mutation branch is missing"
        )
    endif()
    string(LENGTH "#else" bpr_mutation_production_marker_length)
    math(EXPR bpr_mutation_production_begin
        "${bpr_mutation_production_marker} + ${bpr_mutation_production_marker_length}"
    )
    string(FIND
        "${bpr_mutation_compose_source}"
        "#endif"
        bpr_mutation_production_end
    )
    if(bpr_mutation_production_end LESS_EQUAL bpr_mutation_production_begin)
        message(FATAL_ERROR
            "tests-disabled production review mutation branch is malformed"
        )
    endif()
    math(EXPR bpr_mutation_production_length
        "${bpr_mutation_production_end} - ${bpr_mutation_production_begin}"
    )
    string(SUBSTRING
        "${bpr_mutation_compose_source}"
        ${bpr_mutation_production_begin}
        ${bpr_mutation_production_length}
        bpr_mutation_production_source
    )
    # SHA-256 of the exact LF-normalized bytes after the branch's #else and
    # before its #endif. Tokens and horizontal whitespace remain unmodified.
    bpr_lf_normalized_sha256(
        "${bpr_mutation_production_source}"
        bpr_mutation_digest
    )
    set(${out_variable} "${bpr_mutation_digest}" PARENT_SCOPE)
endfunction()

function(bpr_verify_production_mutation_host_digest source_variable)
    bpr_compute_production_mutation_host_digest(
        ${source_variable}
        bpr_mutation_production_digest
    )
    set(bpr_expected_mutation_production_digest
        "16d135419d39078bc3993b4e1cb80226e190edce50cd24d9456843066043a0e1"
    )
    if(NOT bpr_mutation_production_digest STREQUAL
        bpr_expected_mutation_production_digest
    )
        message(FATAL_ERROR
            "tests-disabled production compose_review_mutation branch "
            "digest drifted: expected "
            "${bpr_expected_mutation_production_digest}, found "
            "${bpr_mutation_production_digest}"
        )
    endif()
endfunction()

# Step 3 of the reseal procedure at the top of this file. It prints and
# verifies nothing, so no registered test passes this variable.
if(BPR_PRINT_PINNED_DIGESTS)
    if(NOT DEFINED BPR_SOURCE_ROOT OR BPR_SOURCE_ROOT STREQUAL "")
        message(FATAL_ERROR "BPR_PRINT_PINNED_DIGESTS requires BPR_SOURCE_ROOT")
    endif()
    file(READ
        "${BPR_SOURCE_ROOT}/src/app/src/github_application.cpp"
        bpr_print_application_source
    )
    bpr_compute_full_application_digest(
        bpr_print_application_source
        bpr_print_full_digest
    )
    bpr_compute_production_app_manifest_digest(
        "${BPR_SOURCE_ROOT}/src/app"
        bpr_print_manifest_digest
    )
    bpr_compute_production_mutation_host_digest(
        bpr_print_application_source
        bpr_print_mutation_digest
    )
    message("PINNED_DIGEST full_application ${bpr_print_full_digest}")
    message("PINNED_DIGEST production_app_manifest ${bpr_print_manifest_digest}")
    message("PINNED_DIGEST production_mutation_branch ${bpr_print_mutation_digest}")
    return()
endif()

if(DEFINED BPR_PROVIDER_INVENTORY_SOURCE_FILE)
    if(NOT EXISTS "${BPR_PROVIDER_INVENTORY_SOURCE_FILE}")
        message(FATAL_ERROR
            "provider inventory source does not exist: "
            "${BPR_PROVIDER_INVENTORY_SOURCE_FILE}"
        )
    endif()
    file(READ
        "${BPR_PROVIDER_INVENTORY_SOURCE_FILE}"
        bpr_provider_inventory_source
    )
    bpr_verify_full_application_digest(bpr_provider_inventory_source)
    bpr_verify_async_mutation_host_inventory(bpr_provider_inventory_source)
    bpr_verify_production_mutation_host_digest(
        bpr_provider_inventory_source
    )
    if(DEFINED BPR_APP_SOURCE_ROOT)
        bpr_verify_app_source_tree("${BPR_APP_SOURCE_ROOT}")
        bpr_verify_replay_operator_surface("${BPR_APP_SOURCE_ROOT}")
    endif()
    message("ASYNC_MUTATION_HOST_INVENTORY_OK")
    return()
endif()

foreach(required IN ITEMS
    BPR_APPLICATION
    BPR_APP_SOURCE
    BPR_ROOT
    BPR_RUN_BUDGET_MS
    BPR_SOURCE_ROOT
)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(READ "${BPR_APP_SOURCE}" bpr_application_source)
bpr_verify_full_application_digest(bpr_application_source)
bpr_verify_async_mutation_host_inventory(bpr_application_source)
bpr_verify_production_mutation_host_digest(bpr_application_source)
bpr_verify_app_source_tree("${BPR_SOURCE_ROOT}/src/app")
bpr_verify_replay_operator_surface("${BPR_SOURCE_ROOT}/src/app")
string(FIND
    "${bpr_application_source}"
    "exercise_review_round_trip_composition("
    probe_begin
)
string(FIND
    "${bpr_application_source}"
    "review_round_trip_composition_evidence("
    probe_end
)
if(probe_begin EQUAL -1 OR probe_end EQUAL -1 OR probe_end LESS_EQUAL probe_begin)
    message(FATAL_ERROR "review round-trip public probe boundary is missing")
endif()
math(EXPR probe_length "${probe_end} - ${probe_begin}")
string(SUBSTRING
    "${bpr_application_source}"
    ${probe_begin}
    ${probe_length}
    probe_source
)
foreach(forbidden IN ITEMS
    "ReviewMutationCoordinator->publish("
    "review_mutation_provider().submit("
    "ReviewProjection.push_back("
)
    string(FIND "${probe_source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "review round-trip fixture helper bypasses the public path: ${forbidden}"
        )
    endif()
endforeach()
foreach(required_wire IN ITEMS
    "body_editor()->setPlainText("
    "publish_action()->trigger()"
    "report_review_mutation_target("
    "review_mutation_evidence()"
)
    string(FIND "${probe_source}" "${required_wire}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "review round-trip public trace omitted ${required_wire}"
        )
    endif()
endforeach()
foreach(required_product IN ITEMS
    "dispatch_review_draft("
    "ReviewMutationHost->publish("
    "set_selection_sink("
	"refresh_review_projection(aTerminal)"
)
    string(FIND "${bpr_application_source}" "${required_product}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "review round-trip product composition omitted ${required_product}"
        )
    endif()
endforeach()

string(FIND
    "${bpr_application_source}"
    "void compose_review_mutation("
    mutation_composition_begin
)
string(FIND
    "${bpr_application_source}"
	"void refresh_review_projection("
    mutation_composition_end
)
if(mutation_composition_begin EQUAL -1
    OR mutation_composition_end EQUAL -1
    OR mutation_composition_end LESS_EQUAL mutation_composition_begin
)
    message(FATAL_ERROR "review mutation composition boundary is missing")
endif()
math(EXPR mutation_composition_length
    "${mutation_composition_end} - ${mutation_composition_begin}"
)
string(SUBSTRING
    "${bpr_application_source}"
    ${mutation_composition_begin}
    ${mutation_composition_length}
    mutation_composition_source
)
string(FIND "${mutation_composition_source}" "#else" production_begin)
string(FIND "${mutation_composition_source}" "#endif" production_end)
if(production_begin EQUAL -1
    OR production_end EQUAL -1
    OR production_end LESS_EQUAL production_begin
)
    message(FATAL_ERROR "production review mutation branch is missing")
endif()
math(EXPR production_length "${production_end} - ${production_begin}")
string(SUBSTRING
    "${mutation_composition_source}"
    ${production_begin}
    ${production_length}
    production_mutation_source
)
foreach(required_production_host IN ITEMS
    "select_observed_review_mutation("
    "AttendedReviewMutationMode"
    "select_designated_review_mutation("
    "WindowRead->mutation_transport()"
	"adapters::sqlite::make_review_mutation_recovery_store("
    "make_live_review_mutation_provider("
	"std::move(*recovery)"
    "std::move(LiveReviewMutationGrant)"
    "ReviewMutationHost = std::make_unique<"
    "application::review_mutation_application_host>("
)
    string(FIND "${production_mutation_source}" "${required_production_host}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "production review mutation host composition omitted "
            "${required_production_host}"
        )
    endif()
endforeach()
foreach(forbidden_live_action IN ITEMS
    "make_review_mutation_testing_grant("
    ".observe_target("
    "->observe_target("
    ".submit("
    "->submit("
    ".publish("
    "->publish("
)
    string(FIND
        "${production_mutation_source}"
        "${forbidden_live_action}"
        found
    )
    if(NOT found EQUAL -1)
        message(FATAL_ERROR
            "production composition bypasses the asynchronous host with "
            "${forbidden_live_action}"
        )
    endif()
endforeach()

# [[BPR-0875]]. The data root moves inside an owned parent, because the build
# tree this course roots in is writable by other principals and the agent
# workspace neighbourhood refuses such a parent.
include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")
bpr_private_data_root(BPR_ROOT)

file(REMOVE_RECURSE "${BPR_ROOT}")
file(MAKE_DIRECTORY "${BPR_ROOT}")
include("${CMAKE_CURRENT_LIST_DIR}/bpr_narrow_owner_private.cmake")
bpr_narrow_owner_private("${BPR_ROOT}")
math(EXPR bpr_timeout "${BPR_RUN_BUDGET_MS} / 1000 + 30")
execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        -E
        env
        "QT_QPA_PLATFORM=offscreen"
        "${BPR_APPLICATION}"
        --github-fixture
        github-com
        --epoch-fixture-root
        "${BPR_ROOT}"
        --epoch-fixture-phase
        single
        --review-round-trip-composition-probe
        --automated-run-budget-ms
        "${BPR_RUN_BUDGET_MS}"
    RESULT_VARIABLE probe_result
    OUTPUT_VARIABLE probe_output
    ERROR_VARIABLE probe_error
    TIMEOUT ${bpr_timeout}
)
if(NOT probe_result EQUAL 0)
    message(FATAL_ERROR
        "review round-trip composition process failed (${probe_result})\n"
        "stdout:\n${probe_output}\n"
        "stderr:\n${probe_error}"
    )
endif()
foreach(marker IN ITEMS
    PUBLIC_PATH_OK
    GITHUB_FIXTURE_OK:github.com
    REVIEW_MUTATION_PROVIDER:fixture
    REVIEW_ROUND_TRIP_COMPOSITION_OK
    SEMANTIC_COMMENT_COMPOSITION_OK:native-posts=2,file-posts=1,stored=2,journal-reopens=3,session-constructions=2
    REVIEW_ROUND_TRIP_COMPOSITION_EVIDENCE:11111111111111
    DORMANT_LIVE_PROVIDER_SELECTION:1:28/28:1:1
    WORKBENCH_HOST_COMPOSED_OK
)
    string(FIND "${probe_output}" "${marker}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR
            "review round-trip composition process omitted ${marker}\n"
            "stdout:\n${probe_output}"
        )
    endif()
endforeach()
string(FIND "${probe_output}" "REVIEW_MUTATION_PROVIDER:live" live_marker)
if(NOT live_marker EQUAL -1)
    message(FATAL_ERROR
        "development fixture selected the live review mutation provider\n"
        "stdout:\n${probe_output}"
    )
endif()

# The independent live_mutation_isolation CTest owns the exhaustive executable
# and object-symbol scan with its own 300-second bound. This course keeps its
# source-boundary checks and exercises the editor/provider path once.

file(REMOVE_RECURSE "${BPR_ROOT}")
message("REVIEW_ROUND_TRIP_COMPOSITION_CONTRACT_OK")

# BPR-1428 reseal: over accepted strict tracker 4ae67d6b. Sole new pinned
# changes are github_application.cpp and review_submission_workflow.cpp/.hpp:
# CLI popup becomes exact-current admission with retained strict-wire capability;
# whole-review and inline recheck immediately before one-use grant issuance,
# including after durable draft/comment holding. Grant issuers and mutation
# branch are unchanged. Self-review checks scope/default refusal/lease lifetime;
# six focused architecture/provider/ordinary composed CLI courses pass.
# Independent final critical acceptance remains a separately recorded M4 obligation.

# BPR-1429 reseal over BPR-1428 a8d6cfdc3. New pinned changes name the chosen
# login in first-use trust, retain the originating inline failure binding,
# present typed acquisition failures in existing whole/tour surfaces, and keep
# prior partial/unknown/confirmed tour outcome precedence on fresh refusal.
# The one-use grant issuers and production mutation-branch digest are unchanged.
# Self-review covers failed/current identity separation and receipt truth;
# final independent critical acceptance is owed at M4, not self-certified here.

# Final M3 tidy correction: over f4ba6409d, touched tour callback argument
# names and workflow quiescence callback names follow the coding standard;
# the synthetic trust responder merges equivalent branches. No grant,
# request, failure or publication behavior changes. Self-review and focused
# post-correction checks bind this successor; older unrelated naming findings
# are retained separately rather than described as a whole-file tidy pass.

# RFC0079 main integration reseal: combined 411f05111d6a425934f20e7cf20e474cfd54b2c8
# against independently accepted 34b6a43458659747648e4039a99ca52407d14624.
# The only additional app source is the already-landed cpp_identifiers_v1
# fixture enum/descriptor and shared fixture server-selection case. M4 bounded
# review confirms no publication, credential, grant or inventory changes.
# Refresh the full application and production-manifest digests only; the
# 42-file inventory and production mutation-branch pin remain unchanged.

# RFC-BPR-0077 M5 combined-source review (2026-10-13): source-search ownership
# and exact-reveal composition in a7fb9af0a and 0220258f4, adopted main
# 31ab8129f9b8529423d0ca4a5d637b0443eedabb through a4dc59cea, and the final
# M5 corrections are reviewed in m5-source-review.json and BPR-1423. Register
# both pane and PR controllers in production manifest and exact tree inventory.
# Search adds read-only epoch acquisition, its own profile effect lane, exact
# scope/query/endpoint and monotonic reading seals, then uses ordinary tab open.
# Selected navigation yields background reads and the original corpus resumes.
# Runtime authority reset clears input; every new host drains before ports die.
# The agent-disabled workshop fixture guard fixes inherited executable linkage.
# No provider, credentials, publication admission or mutation branch changes;
# production mutation digest remains e067befefeff198fdbd992259b2ee8ea105520d50ddf7125e3b091519cb8a5df.
# Existing language, provider bypass, full-source and exact-tree negative controls
# remain mandatory. No PRINT_PINNED_DIGESTS switch is passed by registered tests.

# [[BPR-1431]] and [[BPR-1437]] reseal. github_application.cpp gained only the
# RE2 source-regex engine member and its two finder constructor arguments.
# main.cpp now reads its options once through the shared QCommandLineParser
# reader (src/command_line.cpp and include/bpr/app/command_line.hpp, added to
# the inventory and the production manifest): unknown, repeated and
# value-less options are refused at startup. The attended approval, inventory
# and replay options keep their names, values and ordering, and the
# pre-QApplication provenance check still reads the raw arguments. No
# publication, credential, grant or mutation-branch change; the full
# application and production-manifest digests are refreshed.

# [[BPR-1439]] reseal. github_application.cpp's two hand-written lowercase hex
# loops (the review identity and a byte rendering) became Qt's byte-array hex
# conversion, with identical output. No publication, credential, grant,
# inventory or mutation-branch change; the full application and
# production-manifest digests are refreshed.

# [[BPR-1472]] reseal, over main 58ca8e97a. github_application.cpp: a
# Recently viewed row or pasted link for the pull request the window shows
# reuses that read (BPR-0987) only if this window has not published to it
# over that read. A review that is published or of unknown outcome, or a
# single comment or reply that succeeded, records the shown snapshot, and the
# next accepted read clears it. The start-review `finish` course gains a
# reopen-from-Recent phase and evidence bit (main.cpp prints it;
# github_application_fixture_access.hpp holds it). No publication,
# credential, grant, inventory or mutation-branch change; the full
# application and production-manifest digests are refreshed.

# [[BPR-1475]] reseal, over main a05bf2623. github_application.cpp: every
# reopen reads fresh (owner, 2026-10-13), so BPR-1472's reuse of the shown
# read and its publication record are removed. After a review, comment, reply
# or tour publishes (not on an unknown outcome) the window dispatches
# the core's refresh of the shown pull request itself, once, after any read
# in flight settles; a new comment being written is held again and reopened
# in the drawer (core::resume_comment_after_refresh). The review submission
# workflow gains a publication observer. The start-review `pending` course
# waits for the single comment's reread; the `finish` course gains two
# evidence bits (main.cpp prints them;
# github_application_fixture_access.hpp holds them). No publication,
# credential, grant, inventory or mutation-branch change; the full
# application and production-manifest digests are refreshed.

# [[BPR-1447]] reseal. github_application.cpp includes the GitHub adapter's
# platform body digest, keeps one per profile and passes it to each window
# read host, which digests a fetched snapshot's feedback bodies on the
# provider's thread. No publication, credential, grant, inventory or
# mutation-branch change; the full application and production-manifest
# digests are refreshed.

# [[BPR-1477]] reseal, over main e145ccd35, which includes BPR-1479's
# github_application.cpp change. github_application.cpp: a reread (Refresh)
# while a new comment's outcome is unknown holds its draft again, publishing,
# for the same retry (core::resume_comment_after_refresh now holds a comment
# awaiting its retry, not one a submitted review holds). The composition keeps
# the retry record and its reason across the replaced mutation host, says the
# reason again, and reopens the locked draft in the drawer
# (workbench_window::reopen_draft_in_drawer). An unknown outcome is still not
# reread by the window itself. The fixture access reports the window read's
# snapshot and whether a read is in flight or owed. No publication,
# credential, grant, inventory or mutation-branch change; the full
# application and production-manifest digests are refreshed.

# [[BPR-1478]] reseal, over main c43d85e97 (BPR-1477's reseal above).
# github_application.cpp: a file
# gesture in Review, or a pasted link's file, reports the chosen file to the
# document tabs (core::observe_file_choice), so a reread or Refresh that lands
# before the active tab takes the pick presents it instead of the view shown
# before the read. A new tests-enabled course (`--refresh-pick-course
# refresh|reread`) holds the read and the tab's reflection to land the read
# between them; main.cpp runs and prints it, and
# github_application_fixture_access.hpp holds it. The start-review `pending`
# course no longer waits for the single comment's reread. No publication,
# credential, grant, inventory or mutation-branch change; the full
# application and production-manifest digests are refreshed.
#
# [[BPR-1485]] reseal, over main at f7a2945ea's merge. github_application.cpp,
# cycle_session.cpp and review_cycle_desktop.cpp hand the diff hosts and the
# Review Cycle runtime and desktop the application's line matcher (linked
# xdiff, `bpr::adapters::xdiff::make_xdiff_line_matcher`), and src/app's
# CMakeLists.txt links the xdiff adapter. The first-party primary matcher and
# BPR-1366's pause and resume are gone, so a stopped preparation is abandoned
# and recomputed. No publication, credential, grant, inventory or
# mutation-branch change; the full application and production-manifest digests
# are refreshed.

# [[BPR-1488]] and [[BPR-1486]] reseal. github_application.cpp's retained
# Review navigation press now waits for the active tab's current view, and each
# tab publication re-evaluates it; the workbench course's wrap step waits for
# the dispatch with a named stall; the appearance theme probe prints its commit
# facts and the panel's refusal text; and the UIforETW capture measures a
# pane's masked viewport ([[BPR-1487]]). No publication, credential, grant,
# inventory or mutation-branch change; the full application and
# production-manifest digests are refreshed.

# [[BPR-1500]] reseal. github_application.cpp's test-only scaling evidence
# reports the queue host's existing quiescence, and the fixture access header
# carries that field, so the active-changed scaling course waits for the queue
# store before counting its item publication. No publication, credential,
# grant, inventory or mutation-branch change; the full application and
# production-manifest digests are refreshed.

# [[BPR-1501]] reseal. review_submission_workflow.cpp's tour publication
# workflow treats a captured plan as stale only when the draft it observes
# changes owner or revision, not on a window's first loaded draft or a repeat,
# in whichever order they arrive. No publication, credential, grant, inventory
# or mutation-branch change; the production-manifest digest is refreshed.

# [[BPR-1503]] and [[BPR-1504]] reseal. github_application.cpp hands the Find
# in Files panel the appearance profile beside the workbench, for its side
# tags and match wash; pr_search_controller applies the panel's Left/Right side
# filter to F4 traversal and moves a hidden current result to a shown one
# through the existing source-search host. No publication, credential, grant,
# inventory or mutation-branch change; the full application and
# production-manifest digests are refreshed.

# [[BPR-1494]] reseal, over main at 710aed65f (the Find redesign). The
# composition's native device and manual enrollments ask
# rebind_native_sign_in, which applies core's adopt_native_connection answer
# after the bootstrap /user names the account: a returning account's sign-in
# re-binds to its own earlier native identity (re-registering a retired one
# exactly as it was), and the device capability follows a never-saved
# profile. No publication, credential, grant, inventory or mutation-branch
# change; the full application and production-manifest digests are refreshed.

# [[BPR-1527]] reseal over d52f5bb1: pinned-scope source commit681b41b9c
# and this BPR-1527 repair slice. Self-review is recorded in
# docs/issues/_checkpoints/BPR-1527/SELF_REVIEW.md. An unused exact placeholder
# chooses the existing typed reply operation. Tour entry/navigation retains its
# exact source request through receipt completion; ordinary file/modality entry
# clears it. Test-only capture asserts the shipped16-action bar and Accept's
# binding. No new capability, grant, actor/target admission or publication path;
# source inventory and production mutation-branch digest remain unchanged.
# Normal public positives, old-route/guard/layout and missing/misplaced-control
# counterexamples are recorded; final native release gates remain pending.

# [[BPR-1527]] review correction over0f6d288b1, which follows681b41b9c.
# Coordinator read-only review found preparation retroactively reclassified an
# admitted ordinary request; actual held reverse control fails26.06s on0f6d.
# This correction removes the added Qt preparation event and preserves admitted
# origin across modality entry. Only begin(request), before effect submission,
# binds a newly admitted tour source; an ordinary Files gesture clears origin
# before its own request. Both held directions, original cold zero-receipt and
# ordinary ledger pass unchanged budgets. Review disposition is recorded under
# docs/issues/_checkpoints/BPR-1527/review-origin-correction.json. No capability,
# grant, publication, inventory or production mutation-branch change.

# [[BPR-1537]] reseal over 6747ca029 (the last matching pins), recomputed on
# the tree that merges main at fbe406db8. The one other pinned-scope commit in
# between is [[BPR-1539]]'s 6ea8f221f, which changed github_application.cpp
# without resealing: document tabs record the pull request they were opened
# in and start again when another one is accepted, and a press refused for an
# unavailable exact view says so on the status bar. Reviewed here: tab and
# navigation state only, no grant, target, provider, credential, transport,
# GitHub write, mutation or publication path. The constructed intraline-v1 fixture joins
# the test-build fixture enum (github_application_fixture_access.hpp), the
# host-profile table (github_application.cpp), the served-fixture table
# (github_public_path_fixture.cpp) and the --github-fixture names (main.cpp),
# exactly as line-details-v1 did for BPR-1334. Test builds only: no grant,
# target, provider, credential, transport, GitHub write, mutation or
# publication path changes; the production mutation-branch pin is unchanged.
# Self-review on BPR-1537.

# [[BPR-1558]] reseal over main 1e5c35b7f, whose own pins match its tree.
# github_application.cpp hands one syntax classification to every comparison
# side whose blob it names (a pure rename's two panes share one classification),
# and clear_diff_selection retires the appearance host's in-flight
# classifications beside the classifier blob bindings it already dropped. A
# tests-on hold in the composition blob source and two fixture accessors
# (github_application_fixture_access.hpp) let a course order classification
# after the diff. Presentation delivery only: no grant, target, provider,
# credential, transport, GitHub write, mutation or publication path changes;
# mutation pin unchanged; no new src/app source. Self-review on BPR-1558.

# [[BPR-1543]] reseal over main at ec097d8dc (RFC-BPR-0084's inline images
# and BPR-1558 merged; the last matching pins), recomputed on the tree that
# merges this card onto it. The constructed markdown-parity-v1 fixture
# joins the test-build fixture enum (github_application_fixture_access.hpp),
# the host-profile table (github_application.cpp), the served-fixture table
# (github_public_path_fixture.cpp, sharing overview-labels-v1's descriptor with
# its own contract directory) and the --github-fixture names (main.cpp),
# exactly as intraline-v1 did for BPR-1537. Test builds only: no grant,
# target, provider, credential, transport, GitHub write, mutation or
# publication path changes; the production mutation-branch pin is unchanged.
# Self-review on BPR-1543.

# Merge reseal (2026-10-16): local main e64c7158f merged with origin/main
# 23447f3a9 over merge base c233c7683. Each side's pinned-scope changes carry
# their own recorded review: BPR-1527's Windows 0.2.2 commits (retained
# first-file choice, abandoned tab load on an accepted refresh, kept-reply
# commands held while private storage reopens, test-build probe observations)
# and, on local main, BPR-1531/1532/1537/1539/1540/1541/1543/1558, and the
# RFC-BPR-0084 image cards. git merged github_application.cpp and
# github_application_fixture_access.hpp without conflict; the merged text is
# the union of the reviewed diffs, and the only adjacent pair (BPR-1539's
# adopt_tab_destination reset and BPR-1527's retained Chosen file) composes:
# another pull request clears the choice, an accepted refresh of the same one
# keeps it. Digests recomputed on the merged tree with
# BPR_PRINT_PINNED_DIGESTS. No grant, provider, credential, transport,
# publication or production mutation-branch change.

# [[BPR-1565]] reseal over main at 05e32be79, after the merge reseal above. The
# owner removed the comment editor's typed `:xx:` label codes on 2026-10-16.
# github_application.cpp, in its tests-on label-authoring course only: the
# `:qu:` phase now types `:qu: ` and the subject with the label helper on,
# checks the text stays as typed with no label and no control edit, takes
# the code out with one selection and Backspace, then goes on to the chip as
# before; one comment explains why the course still turns the helper on.
# main.cpp prints the renamed evidence bit (CodeStaysLiteral, in the same
# place) and github_application_fixture_access.hpp names it. Test builds
# only: no grant, target, provider, credential, transport, GitHub write,
# mutation or publication path changes; the production mutation-branch pin
# is unchanged; the full application and production-manifest digests are
# refreshed. Self-review on BPR-1565.

# [[BPR-1544]] reseal over main at 7132f6356 (BPR-1565's pins), recomputed on
# the tree that merges this card onto it. The constructed markdown-html-v1
# fixture joins the same four test-build tables as markdown-parity-v1 did for
# BPR-1543, sharing overview-labels-v1's descriptor with its own contract
# directory. Test builds only: no grant, target, provider, credential,
# transport, GitHub write, mutation or publication path changes; the
# production mutation-branch pin is unchanged. Self-review on BPR-1544.

# [[BPR-1562]] reseal over main at c9ff3bf50 (BPR-1544's pins), recomputed on
# the tree that rebases BPR-1562..1564 onto it. github_application.cpp composes
# the Markdown readers' fenced-code classification: a host of its own over a
# packaged classifier of its own, on the appearance effect lane with the shared
# definitions, relayed through the window. The constructed markdown-code-v1
# fixture joins the test-build tables, sharing conventional-comments-v1's
# descriptor with its own contract directory. No grant, target, provider,
# credential, transport, GitHub write, mutation or publication path changes;
# the production mutation-branch pin is unchanged. Self-review on BPR-1562.

# [[BPR-1560]] reseal over main at 0f30a77a1 (after BPR-1562). Whole PR compares from the merge
# base GitHub names, so github_application.cpp and tour_remote.cpp change in
# production: the mutation target binding keeps GitHub's recorded base as
# `BaseOid` (the staleness check against GitHub is unchanged) and carries the
# merge base as `ComparisonBaseOid` only when it differs, so a LEFT line is
# numbered in the merge base GitHub numbers LEFT lines in; the designated
# selection requires both to be its one constant base; the observed selection
# compares the snapshot with the recorded base; BPR-1212 numbering answers
# "same" without the cache for a left side that is the named merge base. No
# grant, credential, transport, provider or mutation-branch pin changes; the
# publication path's LEFT source check is the deliberate change, and before
# anything on a LEFT line is written the adapter's sessions ask GitHub's compare
# for the current merge base and refuse a moved one (target_moved). The full
# application and production-manifest digests are refreshed (main.cpp: the
# public-preview rewritten-base course accepts Whole PR's honest
# merge_base_unavailable, a base sharing no history with the head). Reviewed by the
# owner's cross-review (Fable, Astra) before landing.

# [[BPR-1566]] reseal over main at 2bfc9827c (after BPR-1560), recomputed on
# the tree that merges this card onto it. github_application.cpp binds each window's
# reference page dispatch (About Conventional Comments) to
# application::open_reference_page with that window's browser port, the same
# port Open on GitHub uses. No grant, target, provider, credential, transport,
# GitHub write, mutation or publication path changes; the production
# mutation-branch pin is unchanged. Self-review on BPR-1566.

# [[BPR-1570]] reseal over main at eee6187fd, recomputed on the tree that
# merges this card onto it. github_application.cpp keeps a definite BPR-1212
# base-numbering answer (same or differs) for any request about the same base,
# head, left commit and path, checks it when a kept refresh adopts the same
# diff under a new workbench authority (BPR-1277), and lets
# present_review_draft go on when its own question was answered at once
# (BPR-1560's named merge base). Testing only: the line-draft course forces a
# kept refresh before the base line, and main.cpp takes --stale-base-numbering
# to make the kept answer one about another path. No grant, target, provider,
# credential, transport, GitHub write, mutation or publication path changes;
# the production mutation-branch pin is unchanged. Self-review on BPR-1570.

# [[BPR-1580]] reseal over main at c29d90e19, recomputed on the tree that
# merges this card onto it. github_application.cpp's strip-order check names
# Home's one View action (homeViewAction) where Group, State headers and Filter
# stood, so the title row holds sixteen actions. No grant, target, provider,
# credential, transport, GitHub write, mutation or publication path changes;
# the production mutation-branch pin is unchanged. Self-review on BPR-1580.

# [[BPR-1573]] reseal over main at 83ed4d8fd, recomputed on the tree that
# merges this card onto it (branch 647fbc653..the BPR-1573 product commits
# 86d655c1c, 58ee21f56 and 6ae61c231). github_application.cpp adds Close
# review: the window's close dispatch cancels a reading restoration, sends
# close_pull_request_intent to this window's read lane, whose host invalidates
# the epoch authority (the composition's existing invalidate path, as opening
# another pull request takes), then starts the document tabs again as BPR-1539
# does. A test-build accessor reports what the window holds of its review. No
# grant, target, provider, credential, transport, GitHub write, mutation or
# publication path changes; the production mutation-branch pin is unchanged.
# Self-review on BPR-1573.

# [[BPR-1575]], [[BPR-1576]] reseal over main at 83a58c893, recomputed on the tree
# that merges these cards onto it. github_application.cpp marks the window's
# own rereads (after publishing, after resolving a thread) as automatic through
# workbench_window::run_automatic_refresh, which runs the same dispatch it ran
# before; tells the comparison surface when a syntax classification is awaited
# or failed; and passes whether the theme is high contrast. main.cpp and the
# public-path fixture add test-only delays and a refresh outcome script for
# courses and frozen looks. No grant, target, provider, credential, transport,
# GitHub write, mutation or publication path changes; the production
# mutation-branch pin is unchanged.

# [[BPR-1597]] lands over main at 95e900ba4 (after BPR-1581) with main's seal
# unchanged: it changes no src/app file. Self-review on BPR-1597.

# [[BPR-1572]] reseal over main at c77419079, recomputed on the tree that
# merges this card onto it. github_application.cpp serves the new
# stacked-merge-base-v1 fixture with the same github.com host profile as
# merge-base-v1. No grant, target, provider, credential, transport, GitHub
# write, mutation or publication path changes in the composition; the
# production mutation-branch pin is unchanged.

# [[BPR-1605]]. The review-submission workflow reads carried comments into
# preflight, deduplicates them against the selected composer draft, and keys
# its projection cache by those comments. Only review_submission_workflow.cpp
# and .hpp change in the production app tree; observation/context projection
# gains no publication, executor or capability path. The production-manifest
# digest is refreshed after reviewing those changes. Source inventory, full
# github_application digest and production mutation-host digest are unchanged.
