---
id: BPR-0348
title: Keep the git subprocess error text that failures currently discard
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0409]
---

# Keep the git subprocess error text that failures currently discard

Diagnosing BPR-0341 needed an instrumented build, and it should not have.

The installed-Git process adapter drains a subprocess's standard error into a
byte count and passes `nullptr` as the sink, so the text is read and thrown
away. On a nonzero exit the adapter returns `git_process_failure::nonzero_exit`
and nothing else. Git had said exactly what was wrong:

```
error: unable to write file /tmp/.../pack-f041bc5e....pack:
       Invalid cross-device link
```

None of that reached any log, any evidence record, or any test output.

The loss compounds on the way up. `nonzero_exit` becomes
`git_cache_failure`, which becomes `agent_workspace_failure::unavailable`,
which `workspace_outcome` turns into `agent_run_outcome::failed_to_start`. The
operator is told the agent failed to start. The agent was never reached: the
failure was three layers earlier, in a pack write.

## Required final state

- A failing git subprocess retains its standard error, bounded by the limit
  already applied to the stream, and carries it in the typed failure.
- The text reaches evidence and test output on failure. It must not be
  retained on success, where it is currently a `malformed_output` signal, and
  that check keeps its current meaning.
- Nothing secret can enter it. Git is already invoked with credential helpers
  disabled and every protocol denied, and remote URLs are the thing to watch;
  the retained text needs the same redaction rule as any other evidence.
- A workspace that could not be materialized is distinguishable from an
  executable that could not be launched. `failed_to_start` should not be the
  answer to both.
- A test proves a failing git step surfaces its message rather than only a
  code.

## The redaction rule this card assumes does not exist

"The retained text needs the same redaction rule as any other evidence" reads
as a reference to an existing rule. There is none. `grep -rn "redact" src/`
finds no executable redaction anywhere in the tree, and the closest sibling
boundary — the GitHub CLI process adapter — avoids the question entirely by
carrying no free text at all: every one of its failures is a fixed enumerator,
and `tests/github_contract/github_cli_process_runner_tests.cpp` asserts exactly
that.

Worse, the repository has twice written down the opposite principle, in the two
places that faced this choice:

> The alternative — carrying the text and redacting it on the way out — is
> worse in the way that matters. A redaction step is a place a mistake can
> live, and the mistake is silent.
>
> — `review_queue_notification.hpp:24-28`, echoed at
> `docs/architecture/ai-agent-boundary.md:253`

So this card cannot cite a rule; it has to make one, and it has to say why this
case differs from the two where the answer was to carry nothing.

It does differ, and the difference is the destination. Those two are about
repository content leaving the product permanently — into an operating-system
notification store that outlives the application and syncs to other devices,
and into a third-party AI tool. This is a local diagnostic reaching a log and a
test failure, and the thing being carried is git's own report about the
filesystem, not the reviewer's content. What has to be excluded is narrower and
nameable: a remote URL, which is the one thing in git's diagnostics that can
carry a host, an account, or a token.

That is the rule this card will write and test, rather than the rule it assumed
it could reuse. If a reviewer disagrees that the difference is enough, the
alternative is to keep the exit code and add a typed enumerator per known cause,
which is what the CLI adapter did — and BPR-0341 is the evidence for how that
goes when the cause is not one of the ones already enumerated.

## Result — 2026-08-18: one bullet delivered, the rest needs a decision

### Delivered

`agent_run_outcome::workspace_unavailable` exists and is its own terminal
(`af8c3f7`). A workspace that could not be materialized is no longer reported
as an executable that would not launch, which is this card's fourth bullet.
Only `agent_workspace_failure::unavailable` maps to it; `invalid_request` and
`cleanup_failed` keep their previous answer. Both switches the value crosses are
exhaustive, so the compiler found every site.

That change is demonstrably a distinction and not a rename: the host test that
covers this terminal already proved no process was started and now requires the
outcome to say so and to not be `failed_to_start`, while the Qt panel test still
asserts the literal `Finished: failed_to_start` and still passes, because a
genuine launch failure still reports it.

### Not delivered, and it cannot be without a decision

Retaining git's standard error was implemented, verified, and then **reverted**.
It violates a mechanized architecture boundary.

`cmake/VerifyArchitectureBoundaries.cmake:1974-1979` forbids, in
`src/application/include/bpr/application/git_process_port.hpp` specifically:

```
"(Arguments|Environment|WorkingDirectory|Timeout|OutputLimit)"
"(URL|Url|URI|Credential|Authorization|Proxy|Certificate)"
"(std::string|std::vector)"
```

This card's mechanism — "carries it in the typed failure" — is putting a
`std::string` in that port. The gate is not a naming rule that a reworded
comment satisfies; it says that port carries no text, and it is enforced on
every run.

**There is no way around it, which is the part that makes this a decision
rather than an engineering problem.** `installed_git_cache` holds
`std::unique_ptr<application::git_process_port>`, so every result crosses that
interface. Git's standard error cannot reach an operator without either
crossing the forbidden surface or changing the gate.

The section above, written before any of this was implemented, guessed at the
tension from two architecture documents and concluded the difference in
destination was probably enough to justify carrying text. That reasoning was
incomplete in a way worth recording: it treated the principle as prose. It is
also a gate, aimed at this exact file, and a gate is a decision already taken.

### What was actually built, so the decision is not made blind

Both reverted slices are real, complete, and passed their own verification
before the gate was consulted, so the cost of the "carry it" answer is known
rather than estimated:

- The port's error type became `git_process_error { Code; Detail }` with an
  implicit constructor from the bare code and a free `operator==` against it.
  That shim worked: all 89 existing `.error() == git_process_failure::X`
  comparisons compiled untouched, and only signatures taking the enum by value
  moved.
- The same pattern carried through `git_cache_error` and
  `agent_workspace_error` to `agent_run_result::StandardError`, which the
  process launcher already uses for exactly this purpose. Of 225 `git_cache_failure`
  mentions in the adapter the compiler forced 2; of 65 in the epoch host, 11;
  in two of the nine in-scope files, zero.
- Redaction was implemented and tested as the section above describes: remote
  URLs replaced by a fixed placeholder, everything else preserved, bounded by
  the standard-error limit already applied to the stream.
- Tests proved a real failing git step surfaces its own `cannot lock ref`
  diagnosis naming the exact ref, that a URL is redacted in place, that success
  carries no text, and that the message arrives at the run terminal unaltered.
  Each was mutation-checked.

The patches are not in the tree. They are recoverable from `5e79a5c` and its
revert.

### The decision

Someone owning the architecture has to choose one:

1. **The gate stands, and this card is refused.** Git's failures stay typed
   codes, and diagnosing the next BPR-0341 needs an instrumented build again.
   The honest follow-up is then to enumerate causes as typed codes, the way the
   GitHub CLI adapter did, accepting that an unenumerated cause is invisible.
2. **The gate is revised, with the revision written down.** If the port may
   carry bounded, redacted diagnostic text, the rule should say so and say why
   that is not the same as carrying repository content — because the two
   architecture documents that argue for carrying nothing are about content
   leaving the product, and this is not that.

This card does not choose. Choosing would mean weakening a security boundary to
make a card pass, and a card is not evidence about a boundary.

### Also unaddressed

- `cleanup_agent_workspace` drops any explanation, independent of the above.
- `make_installed_git_cache` and `make_installed_git_process` keep bare-code
  error types, so construction failures carry nothing.
- The propagation hops were never covered by an executable test even in the
  reverted work: removing the text from the cache boundary broke no test,
  because only the far end was asserted.

## Operator ruling — revise the gate — 2026-08-19

**Selected: option 2, the gate is revised, with the revision written down.**

Given as direct operator direction in session. Recorded verbatim:

> Revise the gate

This card correctly refused to choose, on the grounds that choosing would mean
weakening a security boundary to make a card pass. The person who owns the
architecture has now chosen, which is the only thing that was missing.

### What the ruling obliges

The ruling is not "delete the forbidden-pattern rule". The option as written
carries its own conditions, and they are the deliverable:

- the rule must **say** that the port may carry bounded, redacted diagnostic
  text, rather than merely stopping short of forbidding it;
- it must say **why that is not the same as carrying repository content**,
  because the two architecture documents arguing for carrying nothing are about
  content leaving the product, and this is not that;
- it must therefore state what bounded diagnostic text still may **not** carry.
  A revision that only widens is not a revision, it is a removal.

A gate that permits something without saying what it still forbids would leave
this card in exactly the position it refused to reach: a boundary weakened so a
card could pass.

### Sequencing, stated rather than left implicit

This card has two halves and they are not equally reachable today.

The **rule and its written rationale** live in `cmake/` and the architecture
documents. Nothing there relinks the application.

The **restoration itself** — letting `git_process_port` actually carry the text —
is a change under `src/`, and the application links it. Three operator packets
currently bind the exact bytes of the built `dev` application
(`sha256:7b9d6be9…a78fcd93`), and a relink invalidates all three; see the
dev-binary hold in the board README. Those three are attended runs only the owner
can perform.

So the `src/` half is deliberately queued behind them rather than taken now. That
is a sequencing choice about somebody else's pending work, not a technical
obstacle, and it reverses the moment those three verdicts come back — or on
owner direction, if the diagnostic text is wanted sooner than the three runs.

The card returns to `20-ready/`: the ruling has arrived, and what remains is
implementation.

## The rule is revised — 2026-08-20

The `cmake/` and documentation half of the ruling is done. The `src/` half is
[[BPR-0409]], and this card now depends on it.

### What changed

`cmake/VerifyArchitectureBoundaries.cmake` no longer forbids the port any text.
The revision is a narrowing, and it is written down in three places rather than
one: the rule itself carries its rationale as comments, the mechanism is
enforced, and `docs/architecture/security-threat-model.md` gains "A failing
subprocess may explain itself" under Process handling — the section whose
standing instruction already reads "use bounded stdin/stdout/stderr, timeouts,
cancellation, and redaction".

The distinction the ruling demanded is stated where someone will hit it: both
documents arguing for carrying nothing — `review-queue.md` for the operating
system's notification store, `ai-agent-boundary.md` for the AI supplement — are
about repository content **leaving the product**, and both reject redaction on
the grounds that it is a place a silent mistake can live. A git diagnostic
travels from an adapter to the application that invoked it and reaches the
operator who caused it. It does not leave the product and it is not repository
content.

### What it still forbids, which is the half that makes it a revision

- **Collections stay forbidden.** `std::string` and `std::vector` shared one
  alternation, `"(std::string|std::vector)"`. Dropping the string half without
  separating them would have admitted argument and environment lists in the same
  edit — a removal wearing a revision's name. They are now separate rules.
- **Exactly one text member, named `Detail`**, enforced by count, so a second
  string cannot arrive unnoticed and be argued about afterwards.
- **The named-authority patterns still apply to it**, so a URL, credential,
  authorization, proxy, or certificate field is rejected by name whatever its
  type.

### The mechanism, and the bug the test caught

The check counts occurrences of `std::string` and of `std::string[ \t]+Detail`
and requires them equal. The first attempt iterated matched *lines* instead, and
the meta-test rejected the very shape the rule exists to permit:

```
Git application port carries text outside its one permitted diagnostic
member 'Detail': struct git_process_error { int Code
```

`string(REGEX MATCHALL)` yields a CMake list, a CMake list splits on `;`, and a
C++ declaration is full of semicolons — so the loop was handed the fragment
`struct git_process_error { int Code` and failed it. Counting sidesteps this,
because neither match can contain a semicolon. Recorded because a gate that
rejects correct code is worse than the gate it replaced, and only the positive
fixture caught it.

### Verification matrix

| claim | surface | command | proves it | result | evidence |
|---|---|---|---|---|---|
| the live tree still passes the gate | `architecture_boundaries` | `ctest -R '^architecture_boundaries$'` | it runs `cmake -P` over the real port header | pass, 4.71 s | this commit |
| the narrowed rule behaves in both directions | `architecture_boundary_contract` | `ctest -R '^architecture_boundary_contract$'` | it plants synthetic port contents and asserts the scan fires or does not | pass, 56.76 s | this commit |
| the permitted shape is accepted | new fixture | `std::string Detail` planted on the port | without this the revision would be unusable | exit 0 | this commit |
| a second string is rejected | new fixture | `std::string Chatter` planted | `Chatter` matches no named-authority pattern, so only the new rule can reject it | fails as designed | this commit |
| collections are still rejected | new fixture | `std::vector<int> Values` planted | `Values` matches no named-authority pattern, so only the collection rule can reject it | fails as designed | this commit |
| the meta-test suite grew rather than shifted | `tests/scripts/architecture_boundary_tests.py` | `python3 tests/scripts/architecture_boundary_tests.py` | 122 cases | **122, OK**, was 119 | this commit |
| the board stays consistent | `scripts/verify_documentation.py` | `python3 scripts/verify_documentation.py --root .` | lane, status, dependency, and frontier rules | `DOCUMENTATION_OK` | this commit |
| the dev application is untouched | the bound binary | `shasum -a 256 build/dev/.../better_pull_review` | a relink invalidates three pending operator packets | `7b9d6be9…a78fcd93`, unchanged | this commit |

### This card does not close

Its own required final state is entirely about behaviour — bounded stderr
retained, success textless, URLs redacted, an unmaterializable workspace
distinguished from a launch failure, proven by a test. None of that is true yet,
because none of it is in `cmake/`. What exists now is a permissive rule with no
user, named as such rather than left to be discovered.

[[BPR-0409]] carries the restoration and this card depends on it.

## Closure — 2026-08-21

[[BPR-0409]] completed the behavior this card deferred. Bounded, redacted
nonzero-exit stderr now crosses the installed-Git process and cache factories,
cache session, workspace materialization and cleanup, agent result, and visible
Qt terminal. Success and every non-diagnostic failure stay textless; exact
tests preserve refs and local paths while redacting HTTP(S) and SSH-style remote
tokens. A real selected-diff composition course proves the same redacted
diagnostic reaches `Finished: workspace_unavailable` before any agent process
starts. The architecture ruling, implementation, negative mutations,
documentation ledger, and independent closeout review now agree, so the
original card closes with its restoration card rather than returning to Ready.
