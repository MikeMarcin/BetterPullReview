---
schema: "implement-rfc.card.v1"
id: "BPR-0325"
title: "Name the application in the macOS Dock"
status: "Done"
rfcs: ["RFC-BPR-0014"]
depends_on: ["BPR-0324"]
---

# Name the application in the macOS Dock

The macOS bundle declared `CFBundleName`, but not the user-facing
`CFBundleDisplayName`. That allowed Dock presentation to fall back to the
snake-case bundle or executable name even though Qt used the product name.

## Required final state

- Publish `BetterPullReview` as the explicit macOS bundle display name.
- Publish the same display name through the real Qt application instance.
- Preserve the bundle identifier, executable name, icon binding, and platform
  package boundaries.
- Prove the built production bundle contains the exact display name.

## Completion record

- The macOS bundle uses a repository-owned Info.plist template that sets both
  `CFBundleDisplayName` and `CFBundleName` from the canonical
  `BetterPullReview` bundle name.
- Application startup sets Qt's application name and display name to
  `BetterPullReview` before composition creates any user-facing surface.
- The focused metadata contract, application build, production bundle
  inspection, production self-test, and documentation gate pass.

## Non-goals

- Do not rename the `better_pull_review` executable or bundle directory.
- Do not alter the bundle identifier, icon, signing, installation, or release
  readiness.
- Do not claim a signed or notarized package.
