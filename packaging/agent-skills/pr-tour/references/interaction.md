# Interaction rules

## Resume before interviewing

On restart or an interface change, read the current shared draft. Say what is saved locally and what came from GitHub. Ask about only the next missing point. Existing Problem or Solution text is not rewritten because it sounds generic, and a linked issue or ticket is not permission for a company-wide search.

## Explicit scope

"Improve the problem statement" permits that field's rewrite, not the solution or the stops. "Tighten only the last sentence" permits that exact sentence: resolve it with `--quote`, and when it matches more than once ask one clarifying question. Show the updated wording; there is no Apply or Dismiss step. An identical result is reported as no change, not as a new undo entry.

An answer that arrives after the text, the hook or the draft changed is stale. Respect the refusal. Do not reissue the completion with a new target, an older revision or a different ticket to get it accepted: read the draft again and follow the author's next instruction.

## Outline is not a full tour

After outline assistance show the titles and destinations and let the author choose with `outline use`. Do not generate hidden bodies. At a chosen stop, present its code and its explanation together; a request to draft a comment applies only there. Show the resulting text before you record a response such as "looks good, next". Opening an empty placeholder is not acceptance of text that arrives later.

The conversation can convey an author's response; it cannot prove what they understood. Never call an agent-relayed acknowledgement direct terminal observation, and never infer correctness, coverage of the reviewed code, GitHub approval or permission to publish from readiness.

## Examples

Author: "Use those three stops, but start with the failure path." Action: `outline use` the chosen suggestions, `move` by stable IDs, present the first stop's code.

Author: "Explain the interleaving here." Action: `assist context --hook comment --field stop:<stop>` for this stop only, write the bounded result, return it with `assist complete`, and show the new wording.

Author: "Good, next." Action: `ready <stop>` with the revision you just read for this stop's current text, then present the next stop. Do not draft the next comment until asked.

Author: "Publish the tour." Action: `preview`, then use a real approval path if there is one; otherwise give the normal terminal command. Do not fabricate authorization.
