# Branching Conversation Projection v1

Constructed, deterministic test data for BPR-0648 and RFC-BPR-0028 AC-08: a
recorded-shaped conversation with more than twenty posts and three branches.
It was not recorded from GitHub, contains no public pull-request observations,
and is not evidence that such a conversation occurred on GitHub.com or GitHub
Enterprise Server. Its consumers are the Read drawer's aligned comment cards
(BPR-0648) and the Discuss surfaces (BPR-0649).

`projection.json` models `github_conversation_projection` directly, with the
same schema and field order as `deep-thread-v1`. The single valid case holds
one review thread on `src/core/thread_map.cpp` line 42, a first reply, and
three branches hanging off that reply: A (eight replies deep), B (seven) and
C (seven, with an aside off its third reply). Parents precede children, so
consumers receive one stable order; twenty-four posts in all.

There is no capture or regeneration command because the fixture has no live
source. Edit it directly only when the projection contract deliberately
changes; a schema-breaking change belongs in a new versioned directory.
