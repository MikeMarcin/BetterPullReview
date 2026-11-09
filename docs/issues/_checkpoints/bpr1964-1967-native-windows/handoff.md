# BPR-1964 / BPR-1967 native Windows handback

Assignment `/root/windows_native_diagnosis`, mandate
`owner-release-blockers-20261109-r1`. Plans: `bpr-1964-native-session-boundary`
(`measure-and-register`) and `bpr-1967-native-replay-marker`
(`portable-workspace-fault`). The coordinator allocated both IDs and approved
the exact source scopes and native engineering operations.

Source and this evidence packet are integrated and pushed through authoritative
main `107d346834f7ad2c316580c634fccbce314d6ad0`, with origin/main verified at
the same revision. The normal merge preserves main 7321, the BPR-1968
reservation and `NEXT_ID` BPR-1969. Card closeout is metadata only.

## Changes and exact candidate

- BPR-1967 source `3dc816125b7c7b82b1c2113f9067f57ee795d0c9` changes only
  `tests/end_to_end/agent_invocation_composition_public_path.cpp`. It withholds
  the required `.git/bpr-owned-workspace` marker, restores it on both explicit
  exit paths and reports file-operation failures. It does not rename a live
  backend's working directory, stop that backend or bypass a guard. This is
  missing required ownership evidence, not disappearance of the directory.
- BPR-1964 source `fd39ea35401ad57ef846ce1da6b43939f4576bbf` changes only this
  case's registry value/comment and CMake comment. Its measured Windows pin
  is `ancestor-control-denied`; `WINDOWS_UNCONTAINED` remains explicit. Every
  other pin, command, environment entry, assertion and limit remains.

The native candidate is clean source
`8396d01c3fedc31c5fbcaa9ef7d33eb7a8791406`: accepted main `8f7e8caf5` plus
the committed BPR-1967 test and task bookkeeping. It lives in the independent
checkout `D:/bpr-native-session-main-r1/src`, with its own object store,
no alternates and no hardlinked packs. The approved normal VS18/x64 Release
configuration uses Qt 6.12.0, agents/automation/Qt on, Tour CLI off, the pinned
cache, qualified temporary root and fixed C:/D: capacity guards. Native
execution is offscreen in a verified Limited Interactive session 3.

The sole test-file SHA is
`b6061e0bf6d3074922b69e3d3948f477c75b2705527c99c7c4fd0840b74d8a27`.
The unchanged wrapper SHA is
`7a3839a5a5a2bd567d03b361067fa88f2807309eb3f4a422acd3e9fe5e006392`.
The six original courses, 400-second CTest watchdog and 180-second child
limits remain. Passing baseline image:
`575b2d89659aae8af092facb7057dc5a0aeabd252093038b1e626025dbd10d3a`;
canary `fef193f90d6054aba9d5fb4c65f01c2154ad75ef797d709b8d02989cf71ed41d`;
sandbox `c53238e381d0e7aebb6ae018ca8fccb19a57b2247cde43919de8edc148e3ce3c`.
All have valid AMD64 PE headers.

## Direct observations

1. The fresh c4 source originally passed four sessions, then aborted with
   native code 3 during replay. An owned-process Win32 debug observer captured
   an unhandled C++ file-system exception: renaming the live working directory
   was refused because a process used it. Non-bootstrap exceptions remained
   unhandled by the observer; its final exception status is separate evidence.
2. The marker correction removes that abort. An earlier c4-plus-marker run
   reached a later unsupported-native admission failure. That failure remains
   retained. After reconciling accepted main 8f, the unchanged normal wrapper
   passes all six sessions and its final marker: native 0 / 45.94 seconds
   (CTest 45.60 seconds). No exact causal claim about the later c4 failure is
   inferred from that passing repetition.
3. The same passing image, arguments, properties, GUI serialization and limits
   fail inside the maintained AppContainer prefix: CTest 8 / 0.31 seconds,
   native child 1, before any profile database. A separate read-only probe
   records both parent and exact created child as AppContainers. The owned
   data handle resolves in both DOS and NT form. The first retained ancestor,
   `\\?\C:\`, refuses the production attributes/control open with WinError 5.
   Source validation reaches this required ancestor check before state/v1.
   This is an ancestor-control prerequisite, without a DOS-name claim or
   contained functional coverage for the unreached sessions.
4. The negative variant withholds only the unrelated canary state marker;
   the real ownership marker remains. Image
   `7362532a3f4085c1fb80fd0ef6256741cd4451c8caa428e6c316b6a81579b4c1`
   fails the unchanged `missing-native-no-retry` oracle with `Finished: success`
   (CTest 8 / 84.49 seconds). No standalone wrapper-pass marker appears.
   The original postcheck incorrectly matched CTest's printed pass-regex
   header; its raw assertion and the exact-line reconciliation both remain.
5. The exact corrected test file is restored and normally rebuilt (0 / 65.73
   seconds), with clean native Git status before the pin overlay. The restored
   uninstrumented image is
   `270d8f5d7293f37bb4b5f351d8ad4175747e11b3d87394b367dacb368d352b85`.
   This restoration receives no reused image-pass claim. Normal build identity
   tracks dirty source through About's generated header; `about_desktop.cpp`
   and the composition archive rebuilt during the variants. One library hash
   changes, explicitly retained; the other guarded libraries remain identical.
6. The normal registered Windows manifest gate refuses the unmeasured entry
   (CTest 8 / 1.71 seconds), then passes after the exact measured pin (0 / 1.71
   seconds). Executable, wrapper, corrected test, preset and cache hashes stay
   unchanged across these gates. Only the two approved pin/comment files are
   dirty in the private native checkout afterward.

## Verification and retained evidence

clang-format 22, 74 network-isolation contract tests and documentation structure
pass. Scoped board/plan bindings and documentation contract checks accompany
closeout. The generic milestone checkpoint helper still refuses an unrelated
historical plan's unsupported `amendments` field; its raw refusal is preserved.
Explicit bindings to these two authored plans resolve their criteria; no
historical plan was changed to silence the helper.

External evidence root:
`/Users/jaeger/.codex/release-runs/v0.4.2-20261104/candidate43/windows-native-diagnosis-r1/BPR-1964`.
It retains original/updated source identities, every command and both streams,
JUnit, project/cache/source seals, PE headers, task principal/action hashes,
token/handle observations, normal and negative images/PDBs, and the rejected
operator attempts. One probe launcher named the wrong file and exited before
Qt; its fresh corrected retry supplies the actual observation.

Before updating the private checkout, 11,892 archived members were individually
verified: tracked c4 source plus the marker overlay, linked libraries, runtime
outputs/PDBs, target trackers/projects, generated graphs/cache and completed
raw diagnostic packets. Archive:
`D:/bpr-native-session-main-r1/archives/c4-plus1967-before8f-r1/inputs.tar`,
1,701,468,160 bytes, SHA
`c8c3cc065c3f3b4017927072899222ce3269ca24927e27402fa32f9f440cbb60`.
The separate earlier five-image build43 repair archive remains preserved.

The manifest operator also demonstrates exact own-PID CPU/IO measurements:
PID 157096, creation FILETIME 134387152938797766. Its two native-step windows
use 0.21875/0.171875 CPU seconds over 1.707/1.713 wall seconds and write
12,670/30,046 bytes. These small-graph observations do not measure the original
full guardian's cost or explain any startup, Debug cutoff or Release budget.

## Ownership, limits and continuation

All completed native command forests are empty. Task-owned scheduled tasks
are retained Ready only until the separately authorized BPR-1968 mechanical
baseline/boolean-probe/restoration completes, then removed by exact identity.
No review server or human criterion exists for these test/registry changes.

Frozen Windows43 source/cache and frozen Mac44/45 source were not changed.
No full qualification, signing, trust, publication or Debug timing credit is
supplied here. Remaining build43 first-party groups stay explicit in the
coordinator's residual triage packet. The next common source needs normal
registered native qualification; it must not reuse these engineering image
passes. BPR-1968 semantic diagnosis and source correction belong to the other
implementer; this leaf may execute only its sealed mechanical course.
