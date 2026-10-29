---
schema: "implement-rfc.card.v1"
id: "BPR-1807"
title: "People show their GitHub pictures, with a setting for the coloured initials"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1807--BPR-1807--754805aaa710.json","kind":"change","found_by":"owner, 2026-10-28","related_cards":["BPR-1583","BPR-1803","BPR-1804","BPR-1806"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "7b2ef2a795b7766d18b564b1737f5ec11674b925", "evidence": {"kind": "command", "detail": "qt_identity_avatar test_a_picture_replaces_the_initials: a face asks once for its device pixels, the published picture replaces the initials masked to the disc, the ring stays; test_every_person_surface_asks: Threads notes ask for tbutler, jiayin and rcaloca and draw tbutler's picture; Overview's people ask; a team never asks. The reviewer picker (BPR-1804) and tour-stop cards draw through paint_identity_avatar and pick it up."}}, {"criterion": "AC2", "outcome": "pass", "revision": "7b2ef2a795b7766d18b564b1737f5ec11674b925", "evidence": {"kind": "command", "detail": "qt_identity_avatar: initials while loading, after a failed answer (not asked again), in a window without pictures, and after reset the face asks again. application_markdown_image_host avatar_refusals_and_scope: unavailable window is failed at once with no fetch; failed and undecodable fetches publish failed/unsupported; a changed scope cancels and drops the earlier answer; past 64 waiting is over_limit; a non-login is blocked."}}, {"criterion": "AC3", "outcome": "pass", "revision": "7b2ef2a795b7766d18b564b1737f5ec11674b925", "evidence": {"kind": "command", "detail": "qt_identity_avatar: Coloured initials draws initials, switching back draws the held picture without asking. qt_appearance_settings_panel test_avatar_style_row_offers_and_shows: the row shows the adopted style and offers apply_avatar_style_intent. sqlite_appearance_settings_storage test_avatar_style_round_trips_migrates_and_refuses: both styles round-trip, version 10 and 9 files migrate to GitHub pictures, the CHECK refuses 2."}}, {"criterion": "AC4", "outcome": "pass", "revision": "7b2ef2a795b7766d18b564b1737f5ec11674b925", "evidence": {"kind": "command", "detail": "core_identity_avatar: login admission, sizes, github.com address without credential, GHES address with credential to the host only, redirects to media./elsewhere/camo blocked, distinct cache key. application_markdown_image_host avatar_flow/avatar_restart_from_disk: fetch through markdown_image_service with AvatarOrigins, 1 MiB and 10 s bounds, one fetch per login and size, memory and disk cache answers. Live, 2026-10-28: https://avatars.githubusercontent.com/mikemarcin?s=48 and /mikemarcintest?s=96 answer 200 image/png without credentials (no API rate limit spent)."}}, {"criterion": "AC5", "outcome": "pass", "revision": "7b2ef2a795b7766d18b564b1737f5ec11674b925", "evidence": {"kind": "command", "detail": "qt_identity_avatar test_every_person_surface_asks: a Home group header and a Settings server card in a window with pictures ask nothing; Home chips paint with Generated."}}, {"criterion": "AC6", "outcome": "pass", "revision": "7b2ef2a795b7766d18b564b1737f5ec11674b925", "evidence": {"kind": "artifact", "detail": "BPR_AVATAR_PICTURE_RENDER_DIR renders outside the repository (session scratchpad avatar-renders): threads-{github,generated}-{light,dark}.png, overview-{github,generated}-{light,dark}.png and settings-people-{light,dark}.png, with public avatars as fixture pictures."}}]
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

## Evidence

- Focused, on the merged tree: core_identity_avatar, core_markdown_image,
  application_markdown_image_host, application_appearance_*,
  application_github_window_read_host, sqlite_appearance_settings_storage,
  qt_identity_avatar, qt_appearance_settings_panel, qt_conversation_cards_view,
  qt_conversation_finder_scale_100, qt_reviewer_picker*, qt_pull_request_overview*,
  qt_server_card*, qt_home*, qt_settings_window, qt_workbench_window_scale_100,
  application_composition_contract and
  application_review_round_trip_composition_contract (resealed) pass.
  command_correctness_registry_contract fails on main as well: its pins for
  github_application.cpp and github_application_fixture_access.hpp were
  already stale there.
- clang-format 22 and clang-tidy on the changed files, and the documentation
  gate and its tests, pass. Design loaded `frontend-design`.
- Not done: Home and Settings pictures for accounts (they would need the
  account's own host), and pictures in a signed-out public preview.
