# Windows contained-course observations for v0.4.0

[[BPR-1797]], 2026-10-28. The native source is
`144de719a8d972162f92ec04f98ba11293ae0a8f`, preserved unchanged in
`C:/Users/mikem/bpr-v040`. Both Release and Debug were run in the approved
interactive limited producer context. The original full-suite failures remain
in their original reports; this evidence does not relabel them.

Each of these ten courses passes its actual uncontained functional baseline in
both configurations and exits 1 when the same manifest command, environment
and working directory run through the maintained network sandbox with zero
capabilities. The three existing uncontained registrations needed missing pins;
the seven CLI registrations were still contained. The change records only those
measured registrations and pins. No assertion, deadline, network capability,
fixture input or product permission changes.

| Course | First contained refusal |
|---|---|
| `application_agent_chat_conversation_contract` | `private-test-root` |
| `application_description_edit_contract` | `STARTUP_PROFILE_REFUSAL:registry_unavailable` |
| `application_comment_edit_contract` | `STARTUP_PROFILE_REFUSAL:registry_unavailable` |
| `application_device_flow_cli_choice` | `ineligible-inventory composition is available` is false |
| `application_inline_publish_cli_consent` | private desktop test root is unavailable |
| `application_home_waiting_cli_public_path` | private desktop test root is unavailable |
| `application_personal_discovery_cli_public_path` | private desktop test root is unavailable |
| `application_review_submission_cli_ordinary` | `private data root` |
| `application_review_tour_cli_ordinary_native` | private course root is unavailable; only cleanup is printed |
| `application_review_tour_cli_ordinary_stop` | private course root is unavailable; only cleanup is printed |

The tour executable's only initial refusal before startup is its explicit
`if (!dataRoot) return EXIT_FAILURE` after `e2e::private_data_root`; the retained
stream has the QTemporaryDir cleanup warning rather than a predicate message.
The device-flow course waits for startup and fails its composition predicate.
The other CLI courses explicitly check the same private root before composing
fixtures. Their strict fixture grants and local CLI course helpers remain in
place. The description/comment courses stop at registry startup, before their
public UI or mutation steps. These are measured admission refusals, with no
claim about a specific Win32 error, network escape or a newly passing sandboxed
functional course. Containment's separate child/refusal courses remain unchanged.

The external request evidence under
`/Users/jaeger/.codex/release-runs/v0.4.0-20261028/qualification-repairs-r1`
retains all 20 paired normal/contained observations, exact argv/environment/cwd,
raw streams and per-file digests:

- `windows-containment-native-r1/receipt.json` and
  `windows-original-three-refusal-classification-r1.json`, classification SHA-256
  `a2d1ad679cd2fb850427efd6b2ad9d17f005912430b2987d4f0cb66efd23ac0a`.
- `windows-additional-contained-native-r1/receipt.json` and
  `windows-additional-contained-classification-r1.json`, classification SHA-256
  `21b649f87a7a20ebbc4d1dbb8754a35fcedcb9b293c7e6106da20aad3f32b176`.

New-source native manifest and functional verification remain pending. The
five agent-completion preconditions and the threaded-reply resolution completion
are separate unresolved failures; these pins accept neither one.

The focused Python manifest contract passes 73 cases after adding the two
observed refusal categories to its closed vocabulary. The first Mac manifest
run correctly refused an absent `gh_cli_git_smart_transport` executable in the
partial engineering build; it supplied no metadata acceptance. After building that exact
helper, the complete Mac manifest passes (0.54 s). No test is removed to avoid
the prerequisite. Actual new-source Windows verification remains pending.

## BPR-1800 controlled refresh course

On 2026-10-29, the new `application_thread_resolution_refresh` course was
measured on source `a74c9a8755e0ec87365f00a758e963fc360d7bb7` in the isolated
`C:/Users/mikem/bpr-v040-repairs-r1` checkout, in the same approved limited
interactive context. The exact Release image passes directly in 7.21 seconds
and its contained command exits 1 at `FAILED: fixture application starts`.
The exact Debug image passes directly in 17.80 seconds and its contained
command exits 1 at the same predicate. These are functional observations,
with no performance or lower-level Win32 cause claim.

The paired commands preserve each manifest's environment, working directory,
arguments and executable digest. Release image SHA-256 is
`995009ebc4d47d1f04f43cf12c9b95459e937823fe55cc26866a6b3f7be27590`;
Debug is `f44f0ffb06420ff18949dabce7188e05edb121a7d7cc472adc74b82b427669d3`.
Raw streams, manifests and the terminal receipt remain externally under
`lifecycle-containment-r1`; the Mac copy is in the release run's
`qualification-repairs-r1` directory. The `fixture-startup-unavailable` pin
names only that observed predicate. It changes this new Windows registration;
the product, assertions, deadline, fixture and capability requests are unchanged.
Registered native functional verification at metadata commit `1c543f372`
then passes the new course in both configurations. The six-course affected
closure passes in Release (five unchanged passing rows plus the new registered
course) and Debug (all six run), and both actual native manifest checks pass.
Before/after configuration and final executable, DLL and PDB hashes are equal;
the compiled source remains `a74c9a875`. These results supply functional
evidence, with no successful contained course or native package/trust claim.
