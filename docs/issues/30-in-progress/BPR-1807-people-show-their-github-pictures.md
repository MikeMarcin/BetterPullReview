---
schema: "implement-rfc.card.v1"
id: "BPR-1807"
title: "People show their GitHub pictures, with a setting for the coloured initials"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1807--BPR-1807--754805aaa710.json","kind":"change","found_by":"owner, 2026-10-28","related_cards":["BPR-1583","BPR-1803","BPR-1804","BPR-1806"]}
acceptance: []
---

# BPR-1807: People show their GitHub pictures, with a setting for the coloured initials

Owner, 2026-10-28: "Is it possible to show users real avatars from GitHub/GHES
instead of our made up color ones? Maybe offer a setting to go back to the
current ones?"

Every surface draws a person with [[BPR-1583]]'s one painter, as coloured
initials. No surface fetched a picture.

## Acceptance

- AC1. With Appearance > People > Avatars on **GitHub pictures** (the default),
  every face the shared painter draws for a person in a pull request's window
  (Threads and Comments notes and reply faces, the open thread's posts,
  the reply composer, Discuss and the finder, Overview's people, strip and
  role line; reactions name people in words and draw no face) shows that person's own picture from the pull
  request's host, masked to the same disc. Mine keeps its ring. Teams keep
  their square and never ask.
- AC2. The coloured initials show while a picture loads, when it fails, is
  refused or too large, offline or signed out, and for a name that is not a
  GitHub login. A refused picture is not asked again until the account or host
  changes.
- AC3. **Coloured initials** draws the initials everywhere and fetches nothing;
  switching back draws held pictures without asking again. The choice is
  stored in the appearance store (schema 11) and a version-10 store migrates to
  GitHub pictures.
- AC4. The core admits the login and builds the address; every hop is held to
  the host's avatar origins (`avatars.githubusercontent.com` on GitHub.com;
  the GHES host or `avatars.<host>`), with the credential only to the GHES host
  itself, never on GitHub.com. Fetches go through the profile's existing image
  lane (concurrency, decoder, memory and disk caches), bounded at 1 MiB, ten
  seconds and 64 waiting pictures per window, at a size that covers the face's
  device pixels. No widget performs an effect: the Qt side holds decoded
  pixels and asks with a login and a size.
- AC5. Home's account chips and group headers and the Settings account card
  keep their initials (their account may be on another host than the pull
  request's).
- AC6. Offscreen renders of Threads and Overview with fixture pictures, both
  settings, Light and Dark, and the Settings row, outside the repository.

## Design

- Core `identity_avatar_picture`: `identity_avatar_style` (github,
  generated), the picture sizes (32, 48, 64, 96, 128, 192), login admission,
  `admit_identity_avatar` (address and policy) and its own cache key.
  `markdown_image_fetch_policy::AvatarOrigins` holds every hop, redirects
  included, to the avatar origins.
- Application `identity_avatar_window`: per window, under the window's
  body-image facts and scope, through `markdown_image_service`; the window
  read host owns it and publishes `github_presentation_update::Avatars`.
- Qt `identity_avatar_pictures`: a direct child of the window, found by
  `paint_identity_avatar` from the widget it paints (walking out through
  owning windows), so every surface that draws a person, including later
  ones, gets pictures without being told. It asks once per login and size
  at the end of the turn, repaints the faces that asked, masks a picture to
  the disc with a hairline edge, and is reset when the window's profile,
  host or account changes. `identity_avatar_face::Generated` and
  `identity_avatar_view::set_generated` keep a face on its initials.
- Setting: Appearance > People > Avatars, `appearance_settings::Avatars`,
  `apply_avatar_style_intent`, the `Avatars` invalidation and
  `workbench_window::adopt_avatar_style`.

The address is built from the login on the host's avatar origin rather than
read from the API's `avatarUrl`: the strict read contracts carry no avatar
field, and the same origin serves both forms (live, 2026-10-28:
`https://avatars.githubusercontent.com/MikeMarcin?s=64` answers 200
`image/png`; `https://github.com/MikeMarcin.png?size=64` redirects to
`avatars.githubusercontent.com/u/7605339?s=64&v=4`).
