---
schema: "implement-rfc.card.v1"
id: "BPR-2169"
title: "The live second-account course waits for sign-in's outcome, not the account step"
status: "Backlog"
rfcs: ["RFC-BPR-0032"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2169--BPR-2169--be5f904c510f.json","origin":"found 2026-11-13 on lane claude/v0-5-0-prerelease-targets-52265d diagnosing the RFC-BPR-0111 live draft course; the defect is on main since f0dfd707c (BPR-2132)"}
---

# BPR-2169: The live second-account course waits for sign-in's outcome, not the account step

Belongs to RFC-BPR-0032's account setup (amendment r1, [BPR-2132](../60-done/BPR-2132-account-setup-in-one-surface.md)), whose commit `f0dfd707c` moved `tests/end_to_end/live_second_account_course.cpp` onto the account setup driver.

## Defect

Every live course run through `live_second_account_course` (`--course reader`, `live-draft`, `live-session`, ...) fails at sign-in with `GitHub CLI discovery stopped at idle` (on `main` the same race reads `timed out waiting for GitHub CLI discovery`). The course begins the setup through `e2e::account_setup_driver` and then polls for the phase `choosing_account`. The driver's 5 ms timer presses Connect on the tick that first shows gh's accounts and Start reviewing once connected, so the account step passes between two of the course's 20 ms polls and the setup is back at `idle`, signed in. Reproduced 2026-11-13 with the read-only `--course reader --pull 1` on BPR-Test; instrumenting the course showed `choosing_server` → `inspecting_cli` → `connecting` within three seconds, and the setup ending connected.

The application is not affected: the window's account setup signs in through gh as designed (the same run reached "GitHub account connected." with the card `MikeMarcin, GitHub.com, GitHub CLI`). Only the opt-in live course, which is never a registered row, is wrong.

## Fix

Lane commit `0a85bc2b5` ("Live course sign-in waits for the outcome, not the account step"): the course waits for the window to be connected with the setup closed, or for the account step to refuse (a problem other than `connection_failed`), and reads the accounts it was offered from the driver (`account_setup_driver::offered()`, recorded when Connect is pressed). Its failure names the phase, problem, status and Connect count. `main` takes it with the lane's next integration, or by applying that commit's two test files; no product code changes.

## Acceptance

- On `main`, `live_second_account_course --course reader --root DIR --pull 1` against MikeMarcin/BPR-Test signs in through the active gh account and prints `cli-account:` lines, `setup-connects:1` and `login:MikeMarcin`, where before the fix it failed at sign-in.

## Notes

Verified on the lane after the fix (2026-11-13): the reader course signs in (`cli-account:MikeMarcin (active in gh)`, `cli-account:MikeMarcinTest`, `setup-connects:1`, `signed-in-card:MikeMarcin kind=GitHub.com, GitHub CLI`); the live draft course then pushed and created draft pull requests on BPR-Test (#181, #182, closed after).
