# Application icon

`better_pull_review.svg` is the canonical, unchanged vector source for the
BetterPullReview application icon. The tracked PNG family supplies the Qt
runtime resource and Linux desktop sizes; the ICO is embedded in the Windows
executable; and the ICNS is copied into the macOS application bundle.

The Linux desktop file uses the stable application ID
`io.github.MikeMarcin.BetterPullReview`. A future Linux package must install it
under `share/applications`, install the SVG under
`share/icons/hicolor/scalable/apps` with that application ID, and install the
tracked PNGs into the matching fixed-size `hicolor` directories. Those install
rules remain part of RFC-BPR-0014 rather than being implied by the presence of
the assets.

Regenerate the platform files on macOS with Apple `sips` and `iconutil` plus
ImageMagick 7:

```sh
packaging/icons/generate_app_icons.sh
```

The generator deliberately uses the native SVG renderer before resizing. The
current ImageMagick built-in SVG reader does not preserve this source's filter
stack.
