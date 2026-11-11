# Glossary

Terms the RFCs share. Each is used in one sense only. The first table serves
RFC-DRAFT-0001 to 0009; the second came with the absorbed local PR drafts
RFCs, RFC-DRAFT-0010 to 0015.

| Term | Meaning | Not |
|---|---|---|
| Next move | The one thing an authored pull request is waiting for, decided by the core from observed facts | An attention bucket; a stored review state |
| Returned | A next move: a reviewer requested changes, or a conversation awaits my reply | "Needs me", which is for other people's pull requests |
| Head (of the Tour tab) | The band at the top of the Tour tab that states the tour's publication state and carries its publication action | The pull request's head commit, which the RFCs call "the pull request's head" |
| Form (of a publication) | How a tour is carried to GitHub: one review, a mixed tour, or compatibility comments | "Route", which in the tour means the ordered stops |
| Step | One write of a publication, with a state: waiting, sending, sent, failed or not sent | A tour stop |
| Accepted (of a stop) | The author's private acknowledgement of a stop's text, destination and the code it shows and links to, as they were when accepted; the same word and meaning Review uses for a file | Accepting a proposal into the draft; Ready, which this replaces |
| Changed since accepted | A stop whose accepted material differs at the current head, or that was moved | A stop that merely predates the current head |
| Part | The Problem, the Solution, one stop, or the whole tour, as the subject of a side chat thread | A file |
| Update turn | The existing turn that asks the agent to rewrite a part from the conversation so far | An ordinary question |
| Reference | Text in a reply or a tour that names a file and a line or range | A hyperlink the agent wrote |
| Agent run | One launch of an installed agent that consumes its time and usually money | Accepting, moving or discarding what a run produced |
| Agent accent | The palette role worn by a control that starts or continues an agent run. It is configurable per theme on Appearance › Colours; indigo (`#4A3FD6` light, `#A3A0FF` dark) is its default | A marker for anything an agent produced; a fixed colour |
| Subject (of a question) | What a request is about: the pull request, a file, attached lines or a conversation | Where the reviewer happens to be looking |
| Attachment | Selected lines shown as a chip in the composer and sent as the question's subject | Private notes or drafts, which keep their own explicit actions |
| Helper | A sub-agent or background task that an agent's runtime reports it started | A second agent entry configured in BPR |
| Quiet (of a run) | No frame of any kind from the runtime for 120 seconds | Finished; failed; Home's Quiet reason word, which RFC-DRAFT-0001 keeps for a pull request waiting on nobody |
| Scratch folder | A per-session directory outside the repository and the workspace where an allowed agent may write and run files | The workspace; a sandbox |
| Workspace | The checkout an agent runs in: disposable, or one the reviewer chose | The scratch folder |
| Grant | What BPR asks an agent's own permission system to allow | A containment boundary, unless the runtime enforces one and the run details say so |

## Local PR drafts

| Term | Meaning and boundary |
|---|---|
| Local PR draft | Durable author preparation for a prospective pull request, starting from a local Git workspace and requiring no remote PR identity. |
| GitHub draft PR | An actual server-side pull request whose GitHub state is draft. Creating it is publication. On Home its next-move word is Draft. |
| Preparation | The author's iterative work on changes, claims/evidence, description and tour. It is not a one-shot agent run or a mandatory sequence of completed steps. |
| Candidate | Exact selected source/material and comparison context under consideration. Distinguish inspected, outgoing and actually published candidates. |
| Source binding | The exact source an agent conversation was started against. For a local draft it is one candidate; a successor candidate is a different binding with its own workspace and session. The term is RFC-BPR-0096's. |
| Candidate workspace | A private, read-only materialization of one candidate for agent runs. It is never the author's worktree, and it has no scratch folder. |
| Agent identity | The host-minted identity of an agent entry that owns its retained conversations. It is neither the numeric slot nor portable. |
| Worktree | The concrete chosen checkout/linked-worktree identity and directory, including an isolated local clone whose remote is the author's primary clone. Its branch can change; its pathname alone is not the local draft's identity. |
| Target tip / comparison base | Separately observed destination-branch tip and the exact base used for the proposed diff. A feature branch's tracking branch is not automatically either. |
| Evidence record | A method/result with producer, source basis, conditions and limitations. Planned, reported and observed describe different provenance. |
| Formal evidence | A checked property of a stated model or implementation under named assumptions/bounds. It is not a whole-PR approval. |
| Reviewer verification | The existing explicit reviewer judgment on a defect or cycle item. It remains separate from an author's test result. |
| Review Cycle | The existing local folder/file/checkout review subject with captured revisions, feedback rounds and reviewer judgments. It is preserved, not renamed to local PR draft. |
| Tour draft | An authored brief and ordered source stops. Local ownership is proposed in RFC-DRAFT-0013; current PR-owned tours retain their existing authority. |
| Review-comment draft | Existing unpublished comment/review content. It is not a local PR draft. |
| Configuration package | Selectively exported portable settings, with local bindings resolved on import. It is not an account/profile backup or an installed executable plugin. |
| Workflow configuration | Optional named guidance and action bindings for preparation; default BPR remains independent of it. |
| Prompt | Instructions/context sent to an agent within its selected run's actual capabilities. |
| Extension action | A deliberately invoked operation with identified inputs, effect scope and result. Its declaration does not itself enforce isolation. |
| Companion skill | A separately maintained consumer of BPR preparation and installed upstream organization tooling. It avoids copying that upstream policy into BPR. |
| Publication attempt | One identified, authorized attempt with exactly one publisher and recoverable observed effects. |

Avoid standalone **draft** when distinguishing local preparation, a comment,
a tour or GitHub state would change the meaning. Avoid **verified** without
naming the actor, property and method; prefer **needs recheck** for evidence
currency. **Accept** has two senses: accepting a proposal, a description or
a refresh puts something in the draft, and accepting a stop is the author's
sign-off under RFC-DRAFT-0009. Text that could mean either says which.
