JetBrains Mono NL, JetBrains Mono v2.304

The bundled default code font ([[BPR-1270]]): the family "JetBrains Mono NL",
JetBrains Mono without coding ligatures, the owner's choice for every platform.

Source: https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip

SHA256: 6f6376c6ed2960ea8a963cd7387ec9d76e3f629125bc33d1fdcd7eb7012f7bbf

Archive size: 5622857 bytes. Official JetBrains Mono release asset, published
2023-01-14 (the latest release when pinned, 2026-10-03). Pinned provider:
cmake/BprCodeFontProvider.cmake. Offline builds need the archive in the CPM
source cache as JetBrainsMono-2.304.zip. Nothing upstream is executed; the
faces are embedded unmodified as Qt resources of the Qt adapter.

Faces taken from the archive's `fonts/ttf/`, each pinned by the provider:

| Face | Bytes | SHA256 |
|---|---|---|
| JetBrainsMonoNL-Regular.ttf | 208576 | fb3b2575d7b0657359707993288f12a7360344d39387bb26050e276d61f6bd2a |
| JetBrainsMonoNL-Bold.ttf | 210988 | 0198e841824025f8876e5c297f0b9b497ee8d6eb9969710a3328e1303f996ec3 |
| JetBrainsMonoNL-Italic.ttf | 211624 | c7392a134293e1af1d36fbf04940dd844f632afbf97f8325d1591d3af5096cb8 |
| JetBrainsMonoNL-BoldItalic.ttf | 214132 | 6c2716f2e85101f4109a8c16916061eb38176b3b10342834858d6221ec2fda1b |

Each face's name table gives the family "JetBrains Mono NL" and its `post`
table `isFixedPitch = 1`; the OS/2 PANOSE proportion is 9 (monospaced).

Licence texts retained here and carried whole into the installed
THIRD-PARTY-NOTICES.txt (packaging/package_runtime.py NOTICE_FILES); the
provider checks that each equals the archive's own copy:

| File | From | SHA256 |
|---|---|---|
| OFL.txt | JetBrainsMono-2.304.zip `OFL.txt`: "Copyright 2020 The JetBrains Mono Project Authors", SIL Open Font License 1.1 | 30f0c136e3c88e422d0791acd97238870f9054a9729bc34cf2ff0d4ed8cac4ad |
| AUTHORS.txt | JetBrainsMono-2.304.zip `AUTHORS.txt`: the project authors the copyright line names | 33f99c94ef0a0909d7584a5d4bde1fb9b1630daa620f5a788c28c43327e79412 |

SPDX: the release manifest (packaging/dependencies.json) declares the component
`OFL-1.1`. Owner/legal release approval remains the separate release gate.
