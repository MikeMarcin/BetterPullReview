# Pack glossary

This uses the existing [domain ownership](../../../architecture/domain-and-state-model.md)
and [agent boundary](../../../architecture/ai-agent-boundary.md). The
[RFC](RFC-DRAFT-0030-native-agent-sessions-and-tour-context.md) owns behavior.

| Term | Meaning here | Avoid confusing it with |
|---|---|---|
| Native agent session | The selected runtime's continuing conversation, including its tool history and resume handle | A list of BPR-saved questions and replies |
| BPR conversation scope | The authority/review/agent identity, plus draft and tour thread for tour chats | A provider token or process ID |
| Session generation | One durable conversation identity in a scope; New session or explicit replay creates another, reopening does not | A code head, window attachment or turn number |
| Activation | A transient attachment of a window/host to a conversation, used to fence late events | The durable conversation or its backend handle |
| Turn | One submitted message and the agent work it starts | A new native session or process |
| Transcript replay | A separately identified generation carrying permitted bounded prose from an identified history | Native resumption or restoration of old workspace changes |
| Tour revision | BPR's current accepted tour contents and source/preparation state | The agent's remembered plan or the PR's head OID |
| Snapshot freshness | Identity of all current BPR facts supplied with a turn, including observations and proposal decisions | Only the accepted tour's revision number |
| Confirmed source | A source binding established by the core's confirmation contract | An agent having read a file or naming valid-looking lines |
| Explanation reaffirmation | The author's existing acknowledgment that a stop's explanation applies at the observed head | Moving its source or accepting AI prose |
| Ready | The existing author-controlled tour preparation action and its eligibility | Accepted AI prose, line navigation or permission to publish |
| New session | Fresh conversational generation with the previous one kept in history | Delete history, Reset tour or profile purge |

“Session” without qualification is insufficient when a statement could mean
backend memory, a process connection, application identity or visible history.
