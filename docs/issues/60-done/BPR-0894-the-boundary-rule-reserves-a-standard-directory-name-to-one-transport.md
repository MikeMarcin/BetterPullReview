---
schema: "implement-rfc.card.v1"
id: "BPR-0894"
title: "The architecture boundary rule reserves the Linux runtime-directory variable name to one transport, so nothing else may discover it"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-006, hit while implementing BPR-0881","found_at":"68f8152e","run":"run-20260916-stations","dispatch":"dsp-006"}
acceptance: [{"criterion":"The rule expresses the boundary it intends","outcome":"pass","revision":"75d9793daca490aaad2ebd1d712702bdc5af17ee","evidence":{"kind":"observation","detail":"cmake/VerifyArchitectureBoundaries.cmake now separates constructing a child environment from reading a variable to find a directory. verify_environment_construction_owner strips the environment_directory(\"NAME\") accessor form before the ownership match, for XDG_RUNTIME_DIR, USERPROFILE, APPDATA and LOCALAPPDATA only. The other reserved names are each argued on the card rather than left out by omission."}},{"criterion":"architecture_boundaries still fails a real leak of the transport's environment construction","outcome":"pass","revision":"75d9793daca490aaad2ebd1d712702bdc5af17ee","evidence":{"kind":"command","detail":"ctest --test-dir build/core-only -R '^(architecture_boundaries|architecture_boundary_contract)$' passed 2/2 (13.76 s, 98.75 s), log build/r2-logs/dsp7-0894-boundaries-final.log. test_platform_directory_variables_stay_owned_for_construction asserts 16 hostile forms: bare literal, setenv, append_environment, and a discovery read followed by a construction naming the same variable. The pre-existing test_cli_invocation_and_environment_construction_has_one_owner hostile-literal cases are unchanged and still fail."}},{"criterion":"No component is blocked from discovering a platform directory by the standard means","outcome":"pass","revision":"75d9793daca490aaad2ebd1d712702bdc5af17ee","evidence":{"kind":"observation","detail":"test_platform_directory_variables_may_be_read_for_discovery passes for all four variables. Falsified against the pre-change rule by restoring HEAD's copy of the script: the same test then failed on all four, so the narrowing is load-bearing rather than a test that would pass either way. The product probe in profile_activation.cpp is deliberately unchanged while bpr-linux measures BPR-0881's Linux criterion against the current resolution order; its stale comment citing the old rule is corrected and the follow-up is recorded on the card."}}]
---

# BPR-0894: The architecture boundary rule reserves the Linux runtime-directory variable name to one transport, so nothing else may discover it

## Defect

`cmake/VerifyArchitectureBoundaries.cmake:5065` reserves the *name* of the
standard Linux per-user runtime directory environment variable to
`src/adapters/github/src/gh_cli_transport.cpp`. `architecture_boundaries` fails
any other file that spells it.

The rule's intent is sound and worth keeping: the GitHub CLI transport
constructs a child process environment, and which variables it passes is a
boundary decision that belongs to one file. But the rule is written against the
*token*, not against that behaviour, so it also forbids any component from
**reading** the variable to discover a directory the platform documents for
exactly this purpose.

[[BPR-0881]] hit this directly. The profile activation rendezvous needs a short
per-user runtime directory; on Linux the documented way to find one is that
variable. The adapter cannot read it, so it probes the standard location
(`/run/user/<uid>`) and verifies it owner-private like any other candidate —
which works, and is arguably more defensive since it verifies rather than
trusts, but it is a workaround for a rule that meant to say something narrower.
A system that puts the runtime directory elsewhere falls through to `$TMPDIR`
instead of being found.

## Scope

- Narrow the rule to what it means: the GitHub CLI transport owns *constructing*
  the child process environment, including deciding whether that variable is
  passed through. Reading it to locate a directory is not that.
- Check the other names the same rule reserves for the same over-reach before
  changing only this one; a single-token fix that leaves the same shape behind
  is not worth a card.
- Keep `architecture_boundaries` green, and keep a case that would fail if the
  transport's environment construction leaked into another file — that is the
  property worth defending.
- If the rule cannot be narrowed without weakening it, say so and leave the
  probe in place with this card as the record of why it is written that way.

## Acceptance

- The rule expresses the boundary it intends, or the card records why it cannot
  and BPR-0881's direct probe stands as deliberate.
- `architecture_boundaries` still fails a real leak of the transport's
  environment construction.
- No component is blocked from discovering a platform directory by the standard
  means, or the card names why that is intended.

## Outcome, dsp-007

The rule could be narrowed, so it was. It now distinguishes the two things the
old rule ran together.

**What the rule says now.** The GitHub CLI transport still owns *constructing*
the child process environment, including the decision about which variables are
passed through. Reading a variable as a directory path, through the
`environment_directory("NAME")` accessor that yields an absolute path or
nothing, is discovery rather than construction and is permitted in any file.
Every other spelling remains the transport's alone: a bare literal, an
assignment, a name appended to a child environment, and — the case worth
naming — a discovery read followed in the same file by a construction that
names the variable again. The accessor form is stripped before the ownership
match, so it cannot launder a later leak.

**The other reserved names were checked, and only four belong in the set.** The
set is `XDG_RUNTIME_DIR`, `USERPROFILE`, `APPDATA`, `LOCALAPPDATA` — the
variables by which a platform publishes a per-user directory. The rest stay
reserved to the token, each for a reason rather than by omission:

- `HOME` and `PATH` already have three named owners and are more than
  directories; they redirect a child tool's whole notion of state, so a fourth
  reader is its own decision rather than this card's.
- `DBUS_SESSION_BUS_ADDRESS` is a bus address, not a directory, and the Linux
  Secret Service adapter is separately and deliberately forbidden from naming
  it.
- `HTTPS_PROXY`, `NO_PROXY`, `SSL_CERT_FILE` and `SSL_CERT_DIR` are network and
  TLS-trust policy. `SSL_CERT_DIR` is the trap: it *is* a directory, but a
  component that read it would be making a trust decision, which is exactly the
  leak this rule exists to catch.

**The rule still fails a real leak, and that is measured rather than assumed.**
`test_platform_directory_variables_stay_owned_for_construction` asserts sixteen
hostile forms — four variables against four spellings, including the
discovery-then-construct case. `test_platform_directory_variables_may_be_read_for_discovery`
asserts the four permitted reads. Run against the *pre-change* rule, the
discovery test fails on all four variables, which is what shows the narrowing
is load-bearing and not a test that would pass either way.
`architecture_boundaries` and `architecture_boundary_contract` are both green.

**BPR-0881's probe is unchanged, on purpose, and this is the follow-up.** The
adapter still probes `/run/user/<uid>` directly. It may now read the variable
and does not, for two reasons: probing verifies ownership rather than trusting
a value, and it works on a system that sets no such variable. Adding the
variable as a further candidate would change the Linux rendezvous resolution
order while `bpr-linux` is measuring BPR-0881's Linux criterion against the
current order, so it was deliberately not done in the same dispatch as that
measurement. The stale comment in
`src/adapters/sqlite/src/profile_activation.cpp` that cited this rule as the
reason for the workaround has been corrected — a comment naming a rule as its
justification must not outlive the rule.

**Deferred, and small enough to fold into other work:** offer
`environment_directory("XDG_RUNTIME_DIR")` as a candidate ahead of the
`/run/user/<uid>` probe, verified owner-private like every other candidate, so
a system that puts the runtime directory elsewhere is found rather than falling
through to `$TMPDIR`. Do it after BPR-0881's Linux criterion is measured and
green. It touches Linux behaviour only.

**Noticed, not fixed, not carded.** In the same loop, the `"PATH"` arm of the
first branch and the whole `"SystemRoot"` arm are dead: neither string is in
`github_cli_invocation_and_environment_patterns`. Both variables are covered by
the separate explicit check below the loop, so nothing is unguarded — the arms
are unreachable rather than wrong. Left alone because removing them changes no
behaviour and this card is about the opposite problem.
