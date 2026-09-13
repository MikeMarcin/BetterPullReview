# Synthetic ordinary update

The initial pack retains the existing github-com baseline and first head.
The replacement adds exactly three descendant commits in one observed update,
using the existing replacement tree. No timeline force-push event is served.
All identities and content are synthetic; no remote repository is contacted.

Initial head: `f9108913c053354eab2e4771eef27e957a400966`.

New commits, oldest first:

- `e9e0fbcba1f5f7ccb31b5c3734cf2f8d20d44569`
- `c9d8dc7949a538978fb33945f2f6e482d66e14c0`
- `b7b8dc0e5c4fbac8079b2c44bacfff5d36519c13`

Each commit uses author and committer `Fixture <fixture@example.invalid>`,
UTC seconds 1789000000 through 1789000002, and message
`Ordinary update commit N` with a final newline. All three use tree
`c4e461a25e94ac3d4f88116478bf1d0925d2b912`. Packs contain the complete reachable object graph.
