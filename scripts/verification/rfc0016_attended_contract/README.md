# RFC-0016/0017 recorded attendance contracts

These are BetterPullReview product verification artifacts, outside agent skill
roots. They preserve the existing RFC-0016 capture and operator authority
protocols and RFC-0017 decision fixtures. New general attended sessions use the
shared `attended-review` skill; this directory is not a workflow or discovery
entrypoint.

The source builder, operator validator and CSS/JavaScript are retained byte for
byte from the retired `bpr-attended-review` package. The capture contract
rebuilds reviewed HTML and binds its source bytes, so its renderer/templates
cannot be removed without changing the product acceptance check. The builder's
CLI remains for specialized protocol fixtures, not new generic review sessions.

`revalidate_operator_record.py --validate-existing --record-commit SHA` replays
existing operator records. It preserves original recorded command/source paths,
committed source digests, ancestor relationships, exact response bytes, claims,
and all reviewed artifact checks. Its sole physical mapping recognizes the
historical validator path and this retained source path. It never rewrites
historical records or generates approvals. Older records bound to a different
source revision still refuse; relocation does not silently change that rule.

See `source-provenance.json` for original paths and SHA-256 values. Do not edit
the retained sources to relax a test; any product protocol change needs its own
bounded acceptance and migration decision.
