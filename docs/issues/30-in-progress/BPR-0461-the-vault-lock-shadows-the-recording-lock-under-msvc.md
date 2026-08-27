---
id: BPR-0461
title: The fixture's vault lock shadows its recording lock under MSVC
status: In Progress
rfc: RFC-BPR-0016
depends_on: []
---

# The fixture's vault lock shadows its recording lock under MSVC

The first target-native MSVC compile of the merged device-authorization
program stops the `windows-dev` build at
`src/app/src/github_public_path_fixture.cpp(1705)`: the vault-counter
block inside `evidence()` names its `std::scoped_lock` `lock` inside the
outer block whose recording-mutex guard already holds that name, and the
strict warning policy makes C4456 an error. The composition target and
every `application_*` selector behind it cannot build until it clears.
The slice landed from a macOS session whose compiler accepts the shadow
silently — the same shape as [[BPR-0460]]'s pinning drift, one card
earlier the same day.

## Required final state

- The inner guard has its own name and the file compiles under the strict
  `windows-dev` warning policy.
- The composition target builds target-native, proven by the rebuilt-tree
  matrix this session is running for [[BPR-0036]].
