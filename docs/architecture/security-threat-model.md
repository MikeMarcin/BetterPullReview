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
