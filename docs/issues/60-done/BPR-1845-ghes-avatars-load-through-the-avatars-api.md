---
schema: "implement-rfc.card.v1"
id: "BPR-1845"
title: "GHES people's pictures load through the server's avatars API, which private mode does not block"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"bug","found_by":"owner, 2026-11-03: \"It seems like GHES isn't loading avatars in 0.4.1.\"","reservation":"docs/issues/_reservations/BPR-1845--BPR-1845--7fd7d163d2d1.json","related_cards":["BPR-1807","BPR-1818","BPR-1831"],"regressed_in":"0.4.1 (BPR-1807 introduced GHES pictures; never live-checked on GHES)","frontend_design":"not loaded: no layout, wording or colour change; only which address a picture is fetched from"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "f9c7cb90f", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_markdown_image_host$'", "detail": "avatar_ghes_private_mode: a fake that answers as a private-mode GHES does (web route and anything without the token: text/html sign-in page; /api/v3/enterprise/avatars/ with the credential: a PNG). On the 0.4.1 source the test fails with 'FAILED: a private-mode GHES picture loads through the avatars API with the credential'; with the fix the picture is loaded from the one API fetch and the web route is not asked."}}, {"criterion": "AC2", "outcome": "pass", "revision": "f9c7cb90f", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_markdown_image_host$'", "detail": "A failed API answer asks the web route https://<host>/<login>.png?size=<n> exactly once under the avatar origins and publishes its picture once; an undecodable answer on both routes publishes unsupported and is not asked again; a changed scope while the first answer is out asks no fallback."}}, {"criterion": "AC3", "outcome": "pass", "revision": "f9c7cb90f", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^(core_identity_avatar|core_markdown_image|application_github_window_read_host)$'", "detail": "test_ghes_asks_its_avatars_api_first: GHES with the credential admits https://<host>/api/v3/enterprise/avatars/<folded login>?s=<n> with the credential and the web route as Fallback; bots are percent-encoded; a GHES profile without the credential (gh CLI) asks only the web route without credential; GitHub.com still asks https://avatars.githubusercontent.com/<login>?s=<n> once, never with the credential, with no fallback. Existing redirect and origin checks are unchanged and pass."}}, {"criterion": "AC4", "outcome": "pass", "revision": "f9c7cb90f", "evidence": {"kind": "command", "command": "cmake --build --preset dev --target coding-format-check; _coding-standard/scripts/tidy.py --warnings-as-errors on the four changed TUs; python3 scripts/verify_documentation.py --root .", "detail": "Format check passed (clang-format 22.1.8); clang-tidy passed; DOCUMENTATION_OK."}}]
---

# BPR-1845: GHES people's pictures load through the server's avatars API

Owner, 2026-11-03: "It seems like GHES isn't loading avatars in 0.4.1."

## Cause

[[BPR-1807]] (new in 0.4.1) builds a GHES picture's address as the web route
`https://<host>/<login>.png?size=<n>` and sends the API token to it. It was
checked live only on GitHub.com. On GHES, the web route, its redirect target
on the host (`/avatars/u/<id>`) and the `avatars.<host>` subdomain are behind
private mode, which most GHES installations enable, and private mode does
not take an API token: it redirects to the sign-in page, which arrives as
`text/html`, so the image service ends the picture `unsupported` and the face
keeps its initials. Nothing in BetterPullReview's own code path differs for
GHES beyond that address; the transport, credential gate, redirect admission
and Qt holder behave as specified.

Evidence, from public GitHub Desktop sources, since no GHES is available here:

- desktop/desktop#821: GitHub's answer in 2017 was that "avatars won't
  authenticate with a token" on Enterprise; Desktop fell back to Gravatar.
- desktop/desktop#13719 (2022), "Support avatars on GitHub Enterprise
  Server": "we haven't been able to load avatars from behind GHES private
  mode. As of GHES 3.0 we have access to an internal API that I built
  specifically for Desktop." Its filter adds `Authorization: token <token>`
  only for paths under `/api/v3/enterprise/avatars/` on the account's origin.
- Desktop's current `avatar.tsx` still notes that GHES avatar URLs "are
  behind private mode" and asks `<host>/api/v3/enterprise/avatars/u/e?email=`
  (`supportsAvatarsAPI` is GHES >= 3.0; this product's contract is GHES 3.18).

## Fix

- Core `admit_identity_avatar`: on GHES with the profile's credential the
  first hop is `https://<host>/api/v3/enterprise/avatars/<login>?s=<n>` with
  the credential (the host itself is a credential origin), and the web route
  becomes `identity_avatar_fetch::Fallback`. A GHES profile without the
  credential (gh CLI) asks the web route only, as before. GitHub.com is
  unchanged and has no fallback. Every hop stays under `AvatarOrigins`.
- Application `identity_avatar_window`: when the first hop ends `failed`,
  `unsupported`, `blocked` or with no frames, and a fallback exists and the
  scope is unchanged, it loads the fallback once under the same cache key,
  policy and bounds; otherwise it publishes as before.
- `docs/ux/workbench-specification.md` describes the two GHES addresses.

## Open

- The avatars API is asked by login. Desktop's documented form is by email
  (`u/e?email=`); the same avatars service answers by login on GitHub.com
  (BPR-1807's live check), and the web-route fallback keeps a server without
  private mode working if the API does not resolve a login. A private-mode
  GHES whose API does not answer by login would still show initials; only a
  live GHES can confirm, and none is available. If it does not, the next
  step is the API's `u/<id>` form, which needs each author's database id in
  the read contracts.
- A gh CLI GHES profile carries no credential to image fetches, so a
  private-mode server still shows initials there, as body images already do.
