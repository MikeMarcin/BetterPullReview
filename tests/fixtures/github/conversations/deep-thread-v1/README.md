# Deep Conversation Projection v1

This fixture is constructed, deterministic test data for BPR-0417. It was not
recorded from GitHub, does not contain public-pull-request observations, and is
not evidence that a deep conversation occurred on GitHub.com or GitHub
Enterprise Server. Its intended first consumer is BPR-0418's rendered Qt thread
map test.

`projection.json` models the fields and identity prefixes of
`github_conversation_projection` directly. The valid case contains one review
thread, a first comment whose provider `replyTo` would be null and therefore
projects under the thread root, a chain five reply edges deep, and sibling
branches at two reply depths. Parents precede children so consumers receive one
stable deterministic order.

The absent-parent and cycle cases are deliberately hostile projection inputs.
Each is machine-labelled `provider_reachable: false` and scoped only to Qt's
defensive row-preservation behavior. The validated GitHub read model rejects
those relationships before projection, so these cases must never be cited as
GitHub/provider acceptance coverage or mixed into the valid provider-shaped
tree.

There is no capture or regeneration command because the fixture has no live
source. Edit it directly only when the `v1` projection contract is deliberately
changing. Preserve the declared field order, stable identities, parent-before-
child order in the valid case, constructed provenance, and separation of valid
and hostile data. A schema-breaking change belongs in a new versioned directory.
After an edit, run:

```sh
python3 tests/scripts/bpr0417_deep_conversation_fixture_contract_tests.py
ctest --preset dev --output-on-failure -R '^bpr0417_github_conversation_fixture_contract$'
```
