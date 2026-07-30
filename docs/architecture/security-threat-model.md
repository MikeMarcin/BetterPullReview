# Security and Threat Model

## Protected assets

- GitHub credentials and authenticated identity
- AWS/Bedrock, Anthropic, and OpenAI agent credentials
- Corporate source code and review comments
- Private review notes and observer rules
- Local Git caches and SQLite ledger
- Integrity of published review mutations

## Trust boundaries

1. GitHub.com or configured GHES host
2. BetterPullReview native process
3. Installed Git and GitHub CLI processes
4. Configured AI agent processes
5. Operating-system credential vault and trust store
6. Untrusted repository content, comments, commit messages, diffs, and theme files

## In-process architecture boundary

The functional-core/host/Qt split is not presented as an operating-system security sandbox. It is a capability and authority boundary that reduces accidental credential flow and hidden effects:

- the core receives no credentials, sockets, process handles, filesystem handles, or provider clients;
- the Qt adapter receives projections and command contracts, not credentials or unrestricted service objects;
- credential-bearing transports and child-process execution remain host-owned capabilities;
- all effects are traceable through typed host requests and result events;
- no widget can bypass the mutation coordinator or select a remote target directly.

The portable native-vault contract admits at most 2,560 bytes of nonempty
credential material so every supported native vault can represent the valid
domain. The material has a noncopyable owner; movement empties the source,
replacement and destruction overwrite owned bytes, and the type offers no
text, formatting, comparison, hashing, logging, serialization, settings,
environment, or Qt conversion. Vault authority includes exact profile
revision, host, account, and server contract; async results also retain
request, session, and identity generation. Cancellation that wins suppresses
retrieval success and destroys the returned material before publication.

The current macOS adapter targets one explicitly resolved default file-based
Keychain. Every operation matches that exact keychain, a fixed adapter-owned
service, and a versioned encoding of the complete portable authority, and
disallows authentication UI. It does not search arbitrary keychains or enable
data protection, synchronization, access groups, or accessibility attributes.
There is no fallback to a second Keychain implementation or to plaintext
storage. Raw Security framework status numbers and messages do not cross the
adapter boundary.

Synchronous Security framework work runs through a host effect executor,
separate from the serialized decision and GUI lanes. The native integration
fixture uses a distinct adapter-owned service and exact synthetic authority,
passes runtime canaries only over standard input, audits bounded child
arguments, environment, output, executable, source, and scratch surfaces, and
verifies exact-item absence after same-binary parent and CTest cleanup.

The current unbundled integration executable is ad hoc linker-signed. Its
legacy default-Keychain item ACL is therefore tied to an exact code-directory
hash. Same-binary cleanup is proven, but a host crash followed by rebuilding
the test executable can leave an orphan that the new binary cannot remove
noninteractively. This is not cross-rebuild crash-recovery evidence; stable
signed package identity and that lifecycle gate remain RFC-BPR-0014 work.

The Windows adapter selects one generic Credential Manager item under a fixed
versioned target prefix and the complete portable authority. Its integration
fixture uses a distinct prefix and one exact synthetic authority. Store is a
single create-or-replace call; read and delete use the exact target; valid
returned blobs are overwritten before native release. Credential UI,
enumeration, DPAPI, Credential Locker, registry, settings, SQLite,
environment, file, logging, and alternate-vault paths are absent.

Windows generic credentials are protected by the current user's Credential
Manager boundary, not by an executable-specific ACL. A same-user process that
knows the target can request the item. Local-machine persistence means later
logons of that user on the same computer, not all users, roaming, or hardware
backing. The target prefix limits accidental name collision but is not an
authorization boundary.

The Windows fixture passes runtime-only canaries over anonymous standard
input, starts the canonical test executable with an exact inherited-handle
list and explicit minimal environment, captures fixed output, bounds process
time, scans its isolated scratch tree, and requires exact parent and fixture
cleanup. These are prepared checks, not completed native evidence: no Windows
x64 SDK/runtime is available on the current macOS host. Cross-compilation and
static boundaries cannot verify operating-system storage, denial,
unavailable-session behavior, or cleanup. BPR-0036 therefore retains those
Windows target-native checks and the affected claim remains open.

The Linux adapter selects only the default Secret Service collection under
the literal canonical `org.freedesktop.secrets` service. It opens with
`SECRET_SERVICE_NONE`, validates the opened proxy name, and only then
establishes a Secret Service session. Dependency-level service redirection
therefore becomes typed unavailability before session or item work even
though production never reads or mutates routing environment. A live hostile
owner observed property inspection only and no `OpenSession`, collection, or
credential-item method. A derived `SecretService` denies prompt vfuncs and
makes at most one bounded, no-auto-start `Dismiss`; it never displays a
prompt, unlocks or creates a collection, or selects another vault.

Production and integration-test schemas are disjoint. Their only explicit
attribute is the non-secret 66-character portable authority; libsecret adds
`xdg:schema`. Credential bytes are stored as
`application/octet-stream`. Retrieval also accepts exact `text/plain` because
the directly tested gnome-keyring 50/libsecret 0.21.7 stack normalized that
metadata while preserving arbitrary opaque bytes. Exact attribute checks,
unique-item enforcement, collection/item lock checks, the 2,560-byte portable
bound, and typed unavailable, access-denied, corrupt, allocation, and
operation failures keep malformed or redirected storage from becoming a
credential result. No raw provider error or credential material crosses the
adapter.

The Linux native fixture uses only the integration schema and a synthetic
authority. In an Ubuntu 26.04 x86_64 full-system QEMU VM with an emulated CPU,
a dedicated real user ran it under Xvfb and a desktop D-Bus Secret Service
session. Runtime-only canaries entered the same-executable child through
framed standard input; arguments, the explicit child environment, fixed
output, executable, source, and isolated scratch were checked. A separate
opaque-byte lifecycle included NUL, high-bit, and invalid UTF-8 bytes.
Checked-in cleanup proved the exact fixture authority absent; a controlled
D-Bus audit separately proved integration-schema absence. A truly locked
default collection, no-session-bus unavailability, and hostile service-name
rejection were also checked. Access denial and failed deletion are
fake-backed/status-classification evidence rather than desktop-policy
fixtures. This proves the vault surface in the selected full-system Linux
stack, not bare metal, arbitrary user-home or system-log absence, native
HTTPS, or the composed public provider path.

## Baseline posture

- No hosted BetterPullReview service.
- No listening socket or local web server.
- No embedded browser, HTML renderer, JavaScript runtime, CDN, remote font, analytics, or telemetry.
- No general plugin system or remote theme/highlighter download.
- No repository code execution, hooks, submodules, filters, textconv, external diff, build, or test invocation.
- No credential sharing between BetterPullReview and AI agents.
- No automatic AI invocation or automatic AI publication.

## Input handling

All API, Git, diff, syntax, Markdown, agent, and settings inputs are untrusted. Apply explicit size, nesting, time, allocation, and line-length limits. Render a safe Markdown subset with raw HTML disabled. Links require a user gesture and show the destination.

## Process handling

- Resolve executables explicitly.
- Invoke with argument arrays, never a shell.
- Use bounded stdin/stdout/stderr, timeouts, cancellation, and redaction.
- Pass prompts and request bodies through stdin or protocol messages, not command lines.
- Allowlist inherited environment variables for sensitive child processes.

## Network handling

- Connect only to explicitly configured hosts.
- Require HTTPS and operating-system certificate validation.
- Restrict redirects.
- Respect approved system proxy configuration.
- Do not provide certificate-error bypass.
- Maintain a metadata-only network activity log.

GitHub profile authority is established before transport execution. The
selected v1 validator accepts only trusted explicit/builtin/managed
provenance, canonical lowercase ASCII FQDNs, and the exact GitHub.com or GHES
3.18 HTTPS roots. Provider content and redirect targets cannot create or
rewrite a profile. GraphQL redirects are rejected; REST redirects require the
same validated authority and API namespace.

Capability introspection is untrusted input. One allowlisted read-only query
is subject to response-byte, depth, member, collection, string, and aggregate
value bounds. Duplicate names, partial data, GraphQL errors, malformed type
wrappers, permission failures, and transport failures yield explicit
all-Unknown unavailability rather than support or a misleading unsupported
claim. Only complete schema compatibility evidence enters the
profile/host/account/server/probe-contract cache.

## Security evidence

Required release evidence includes threat-model review, SBOM, dependency inventory, secret scan, static analysis, sanitizers, parser fuzzing, hardened build flags, signed package verification, mutation-target tests, and diagnostics redaction tests.
