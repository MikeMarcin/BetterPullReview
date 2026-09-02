---
id: BPR-0516
title: Compose a system-browser opener for the device-authorization handoff
status: In Progress
rfc: RFC-BPR-0016
depends_on: [BPR-0280, BPR-0318]
---

# Compose a system-browser opener for the device-authorization handoff

## Why

RFC-BPR-0016 requires the verification address to open in the system
browser and never in an embedded one. [[BPR-0280]] delivered the Qt
presentation and the handoff's authority cut and failing-opener contracts,
and said in so many words that the Qt adapter ships no opener because the
opener belongs to composition. Composition never received one:
`src/app/src/github_application.cpp` composes
`device_authorization_browser_handoff{ .Browser = nullptr }` on every
platform under a comment written when no shipped platform could start a
flow. Since [[BPR-0318]], [[BPR-0310]], and [[BPR-0311]] every shipped
platform can, so a reviewer reaches `awaiting_authorization` with a code
and a URL and an "Open browser" action that always answers `unavailable`.
Found by the [[BPR-0283]] scout on 2026-09-02; nothing in `src/` calls
`QDesktopServices::openUrl` or any platform equivalent.

## Required final state

- One production implementation of `device_authorization_browser_port` that
  opens the validated verification address in the operating system's default
  browser through the platform's own launch service, with no embedded engine
  and no network of its own. It is composed under the same
  fixture/production split as the device host, and the fixture profile keeps
  the recording opener the contracts drive.
- The host's authority check still runs before the opener is called; an
  off-authority address reaches no opener (the existing cut stays green).
- A construction cut: with the opener absent the flow still starts and the
  action answers `unavailable`, exactly as today, and the positive marker is
  false. `composed_application_hosts` names the opener's constructor.
- The platform course's `check_platform_surfaces` still proves the absence
  of every embedded browser engine on every platform.
- Target-native observation on each enabled platform that the action opens
  the system browser at the verification address — recorded by each
  station as a row on this card, since no isolated selector can prove a
  real browser launched.

## Non-goals

- Changing the handoff's presentation or the panel — [[BPR-0280]].
- Any change to the authority rule — it is the core's and the host's.

## Verification

```text
ctest --preset dev -R '^(application_device_authorization_host|qt_device_authorization_scale_(100|125|200)|application_device_authorization_(public_path|unavailable|release_contract|registered_course)|composed_application_hosts)$' --output-on-failure
python3 scripts/verify_rfc0016_platform_course.py --platform macos --dev-preset dev --release-preset app-release --build-root build/rfc0016-macos-browser --expected-device-selection enabled --fresh
```

The Linux and Windows courses and the browser observation run on the
stations.

## What this slice delivered — 2026-09-02

**The opener lives in composition, not in the Qt adapter.** That was decided
by the rule rather than by preference: `VerifyArchitectureBoundaries.cmake`
refuses `QDesktopServices`, `QUrl`, and a literal scheme anywhere under
`src/adapters/qt/`, and `src/adapters/qt/AGENTS.md` states the consequence
outright — "the system-browser capability is host-owned and lives outside this
adapter". [[BPR-0280]] had already written the same sentence from the other
side when it declared the port. So `bpr::app::system_browser_opener` sits in
`src/app/src/system_browser_opener.{hpp,cpp}`, in the composition library that
already links Qt GUI, and `QDesktopServices::openUrl` is its entire effect —
the call that defers to `LSOpenCFURLRef`, `xdg-open`, or `ShellExecute`. No
embedded engine, no network, no listener, no supervised child process. It
re-parses the address in `QUrl::StrictMode` and refuses anything that is not an
absolute `https` address with a host and no userinfo, which costs one parse and
removes the possibility of an opener that trusts its caller completely.

**The split is the device host's seam, made total.** The shipped graph binds
`system_browser_opener`; a tests-enabled graph binds a recording opener
instead. The seam is `BPR_BUILD_TESTING` rather than `Fixture != nullptr`,
which is stricter than the transport's split beside it, and deliberately: a
fixture transport reached by mistake writes a line to a log, and a browser
opened by mistake opens a browser on whoever's machine is running the suite.
No test in this repository can launch one. `SystemBrowser` is constructed
either way, so the shipped implementation is present by linkage and by
construction in both profiles, which is what the composed-host anchor audits.

**The authority check still runs first, and reaches no opener when it fails.**
Untouched: `resolve_handoff()` validates against the compiled authority before
it consults the capability, so a build with no browser and a build with a bad
address give different answers. `test_an_off_authority_address_is_refused_with_no_browser_call`
asserts `world.Browser.Addresses.empty()` for
`https://github.com.evil.invalid/login/device` and still passes.

**The construction cut.** `--device-authorization-browser-absent` composes the
device host with no capability bound and changes nothing else. The flow still
starts, still presents, still reaches `authorized`, and still enrolls exactly
once through the real vault; the action answers `unavailable` and the positive
marker `DEVICE_FLOW_BROWSER_COMPOSED` is `false`.

**Two pins moved, in two passes, and a third gate fired between them.** The
second drift stays hidden until the first is repinned, and the contract's
message wraps across lines, so the found digest has to be matched with `\s+`
rather than read off one line. `bpr_expected_full_application_digest`
`408d9746…babe1` → `f30b9005…24437a`;
`bpr_expected_production_app_manifest_digest` `ff2c96c3…abeaff` →
`31b8fa26…9e447c`. Between them `bpr_expected_app_sources` refused the two new
files by name and was extended. All three moved twice, because `clang-format`
reflowed four lines of `github_application.cpp` after the first repin — the
values above are the final ones, and again after `--device-authorization-browser-probe`
was gated to tests-enabled builds so no station's unattended release course can
be handed a flag that opens a browser.

**No test opens a real browser, and none should.** The shipped opener's
presence is proven by linkage and construction; that it reaches a real browser
at a real verification address is a target-native observation, recorded below
by each station.

| Claim | Command | Result | Commit |
|---|---|---|---|
| The device-authorization selectors, the Qt scales, the composed-host audit, and the pinned composition contract all hold | `ctest --preset dev -R '^(application_device_authorization_host\|qt_device_authorization_scale_(100\|125\|200)\|application_device_authorization_(public_path\|unavailable\|release_contract\|registered_course)\|composed_application_hosts\|application_review_round_trip_composition_contract)$' --output-on-failure` | 10/10 passed in 56.6 s; host 0.07 s, scales 4.90/4.89/4.89 s, public_path 0.89 s, unavailable 0.48 s, registered_course 5.44 s, release_contract 0.46 s, composition contract 32.9 s, composed hosts 1.62 s | this commit |
| The composed opener is reached and answers, and the construction cut leaves the flow intact with the marker false | `ctest --preset dev -R '^application_device_authorization_browser_(handoff\|absent)$' --output-on-failure` | Both passed, 5.44 s and 5.44 s. Handoff: `DEVICE_FLOW_BROWSER_COMPOSED:true`, `DEVICE_FLOW_BROWSER_OPENS:1`, `DEVICE_FLOW_HANDOFF:opened`, `DEVICE_FLOW_ENROLLMENTS:1`. Cut: `DEVICE_FLOW_PRESENTED:true`, `DEVICE_FLOW_STAGE:authorized`, `DEVICE_FLOW_REQUESTS:0`, `DEVICE_FLOW_BROWSER_COMPOSED:false`, `DEVICE_FLOW_BROWSER_OPENS:0`, `DEVICE_FLOW_HANDOFF:unavailable`, `DEVICE_FLOW_ENROLLMENTS:1` | this commit |
| The composed-host anchor tracks composition rather than mere linkage | Deleted the `SystemBrowser` member and bound production to `nullptr`, leaving `system_browser_opener.cpp` compiled and the verifier untouched; rebuilt; ran `composed_application_hosts` | Failed in 1.58 s with `COMPOSED_HOSTS_FAILED: required exact demangled anchor is absent: system browser opener constructor` — the object was still compiled, so the linker dropped it and the audit noticed | this commit |
| Restoring the composed object restores the audit | Restored `github_application.cpp` byte-exact (md5 `f63aca6a1a47c840709c901f45385b6c` before the cut and after the restore; the file's final md5 is `c9a5fde46be8ce8c530a8da204740d19` because `clang-format` reflowed four lines later in the slice), rebuilt, reran the selector set | 10/10 passed; `composed_application_hosts` 1.62 s. The Debug Mach-O is not bit-reproducible across relinks, so the byte-exact claim is the source file and the contract's own LF-normalized source digest, both of which match | this commit |
| No embedded browser engine is present in the artifact | `verify_composed_hosts.py`'s `inspect_symbols` (`llvm-nm --demangle`) plus a raw image scan over `build/dev`'s app binary, against `check_platform_surfaces`'s own forbidden sets | 117 727 demangled symbols, zero hits for `QWebEngine`, `QWebView`, `QtWebEngine`; the 58 137 872-byte image contains none of those as ASCII, nor `QtWebEngineCore`. The full platform course was not run here — it builds two fresh trees and is a station command | this commit |
| The whole dev graph builds clean under `-Werror` | `cmake --build --preset dev` | Exit 0, zero `error:` or `warning:` lines in the whole log | this commit |
| Every touched C++ file is formatted | `clang-format --dry-run --Werror` on the six touched files | Exit 0 after reflowing four lines of `github_application.cpp` and one of `main.cpp` | this commit |
| macOS: the action opens the system browser at the verification address | Target-native observation on the macOS station | Pending — the station records it | Pending |
| Linux: the action opens the system browser at the verification address | Target-native observation on the Linux station | Pending — the station records it | Pending |
| Windows x64: the action opens the system browser at the verification address | Target-native observation on the Windows station | Pending — the station records it | Pending |

## Integrator reproduction — 2026-09-02

Rebuilt `build/dev` (exit 0) and ran the device-authorization host, the three
Qt scales, the five end-to-end device contracts including the two new
`browser_handoff` and `browser_absent` ones, `composed_application_hosts`,
`application_review_round_trip_composition_contract`,
`application_composition_contract` (109.44 s),
`application_agent_finding_composition_contract`, and
`rfc0016_test_inventory`: 15/15. `clang-format` clean by exit code on the
six touched C++ files. The delegation had suggested the Qt adapter as the
opener's home; the slice was right to refuse it — the architecture gate
forbids `QDesktopServices` under `src/adapters/qt/`, and the adapter's own
guide says the capability is host-owned. The two new selectors are now
allocated in `scripts/verify_rfc0016_test_inventory.py` with their required
arguments, so the stations' release scan proves their absence from the
tests-disabled artifact. The code is landed; the card stays open for the
three target-native observation rows.

## macOS — 2026-09-02

Run on the integrator host at `5b5f0d65` (the tree of `d46a766a` plus one
card move), in a fresh build root:

| Row | Command | Result |
|---|---|---|
| Enabled platform course | `python3 scripts/verify_rfc0016_platform_course.py --platform macos --dev-preset dev --release-preset app-release --build-root build/rfc0016-macos-browser --expected-device-selection enabled --fresh` | exit 0 in 146 s: `RFC0016_COURSE_SELECTORS:16`, `RFC0016_DEVICE_STAGE:cancelled`, `RFC0016_DEVICE_REQUESTS:0`, `RFC0016_PLATFORM_COURSE_OK` |
| The opener's anchor in both binaries | `composed_application_hosts` inside the course | `composed_application_hosts passed inside the course with the opener anchor required` |
| Staged artifact | `build/rfc0016-macos-browser/release/src/app/BetterPullReview.app` | 10,406,976 bytes, sha256 `759e743c4ce6a1dbe972098e90cb0264b5b53e11629ff2d9e2f87f7fc1d12050` |
| The action opens the system browser at the verification address | Launch the staged app, start sign-in, click "Open browser" | **Pending** — the owner's attended observation; it starts a real device flow, so no agent performs it |
