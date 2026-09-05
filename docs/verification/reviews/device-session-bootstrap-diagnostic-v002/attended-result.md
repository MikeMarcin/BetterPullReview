# v002 attended diagnostic result — 2026-09-04

The exact approved executable was launched once, without arguments, after
packet/binary digest checks, with all 24 specified environment variables
removed from its child environment and child stdout/stderr discarded. The
operator completed the browser interaction and reported “done”.

Native accessibility showed the safe diagnostic:

> Connection unavailable
> Capability discovery: GitHub returned data that BetterPullReview could not safely use. The capability response contained a GraphQL error envelope.

The agent pressed Copy GitHub message, then explicitly Sign out. Terminal UI
read “No GitHub host is connected.” and “Connect to GitHub”; Open pull request
was disabled. The window was closed normally and the process exited with code
0. No PR was opened or publication attempted. No device code, token, raw
provider response, browser credentials or secret-bearing logs were retained.
Cleanup is supported by visible terminal UI; no vault contents were inspected.

At source `9414ffc93b70aae51104b20c81a974b890eecf5a`, this category identifies
the normalizer branch detecting a top-level GraphQL `errors` member. It is
beyond native transport/header admission and bounded JSON parsing. The unchanged
malformed-response reason rules out the recognized rate-limit mapping for this
observation. The error member's contents were not retained, so syntax,
permission, service or other GraphQL refusal details remain unknown. No root
repair or successful application-session claim is made.

The v002 one-launch authority is consumed. No retry is authorized by this record.
