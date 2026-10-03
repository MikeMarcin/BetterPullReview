JetBrainsMonoNL Nerd Font, Nerd Fonts v3.5.1

The bundled default code font ([[BPR-1270]]): the family "JetBrainsMonoNL Nerd
Font", the owner's choice for every platform.

Source: https://github.com/ryanoasis/nerd-fonts/releases/download/v3.5.1/JetBrainsMono.tar.xz

SHA256: 04d5e8f903693f9dd13e16f867e994834e681eb3c72c0d337a770dcda09010cf

Archive size: 7284244 bytes. Official Nerd Fonts release asset, published
2026-08-21; the release lists the same SHA-256. Pinned provider:
cmake/BprCodeFontProvider.cmake. Offline builds need the archive in the CPM
source cache as nerd-fonts-v3.5.1-JetBrainsMono.tar.xz. Nothing upstream is
executed; the faces are embedded unmodified as Qt resources of the Qt adapter.

Faces taken from the archive, each pinned by the provider:

| Face | Bytes | SHA256 |
|---|---|---|
| JetBrainsMonoNLNerdFont-Regular.ttf | 2489128 | f726c0fb0db90a00920e8fa9ef281ccf9f5f848785ebcaca09bade9d15d38b58 |
| JetBrainsMonoNLNerdFont-Bold.ttf | 2491512 | 104897b8ff7f74674c8d6d81b6dbe9138e5c249b54d6c08c8b21600f75d3c5c1 |
| JetBrainsMonoNLNerdFont-Italic.ttf | 2492084 | dd76e7fe83fd2b1ae8e416efb3af8348d975b004648eb28e9a836e1fc4dd72a0 |
| JetBrainsMonoNLNerdFont-BoldItalic.ttf | 2494624 | 8d5892b455cc166f8aac459182aa7a0d7df6cf2d70d61047deb8783fbd2b5d67 |

Each face's name table: "Copyright 2020 The JetBrains Mono NL Project Authors
(https://github.com/JetBrains/JetBrainsMonoNL)"; licensed under the SIL Open
Font License 1.1; version "Version 2.304; ttfautohint (v1.8.4.7-5d5b);Nerd
Fonts 3.5.1".

Licence and attribution texts retained here and carried whole into the
installed THIRD-PARTY-NOTICES.txt (packaging/package_runtime.py NOTICE_FILES):

| File | From | SHA256 |
|---|---|---|
| OFL.txt | JetBrainsMono.tar.xz `OFL.txt` (the provider checks it equals the archive's) | 30f0c136e3c88e422d0791acd97238870f9054a9729bc34cf2ff0d4ed8cac4ad |
| NERD-FONTS-README.md | JetBrainsMono.tar.xz `README.md`: the patched glyph sets, their upstreams, versions and licences (the provider checks it equals the archive's) | 7ced4e915272073713607cbc1646f625f4265cdf7aef964fdeebc030cc7a4440 |
| glyph-licenses/font-patcher-readme.md | FontPatcher.zip `readme.md`: the patcher is MIT; the added icons' licences are in `src/` | 7f251bce74d21dca8c54a1e7237930aee5561b363ae73727ea12e17beac37705 |
| glyph-licenses/codicons-LICENSE.txt | FontPatcher.zip `src/glyphs/codicons/LICENSE.txt` (CC BY 4.0) | d6239afa918961b465b07bf7411cbe34ff6685854f58553db7966f4881a0211f |
| glyph-licenses/font-awesome-LICENSE.txt | FontPatcher.zip `src/glyphs/font-awesome/LICENSE.txt` (CC BY 4.0 icons) | de784a808496d49f5f80dd9d1d6e9a51cff712cfbc339e7c25155631ae3cdb5d |
| glyph-licenses/materialdesign-LICENSE | FontPatcher.zip `src/glyphs/materialdesign/LICENSE` (Apache 2.0) | 3bae6305bcaa6d10219b999f65c342034c2d40b1c0e8567fa08605e73ccba829 |
| glyph-licenses/octicons-LICENSE | FontPatcher.zip `src/glyphs/octicons/LICENSE` (MIT) | 4f886642f9e8d99eeadd484acdac011c3fde8a338faf52f5ca1408c13b9a9b63 |
| glyph-licenses/pomicons-LICENSE | FontPatcher.zip `src/glyphs/pomicons/LICENSE` (OFL 1.1, RFN Pomicons) | cb4797a9745683f287c28833db596e9d68ec43082d59359b3a49c504f0f610ce |
| glyph-licenses/powerline-extra-LICENSE | FontPatcher.zip `src/glyphs/powerline-extra/LICENSE` (MIT) | 544307a58e6826c24145b2af1c3578b53e2e21ae0e6c67ab4c6abd544304d26a |
| glyph-licenses/powerline-symbols-LICENSE.txt | FontPatcher.zip `src/glyphs/powerline-symbols/LICENSE.txt` (MIT) | 46d5355e4734468135d8e4a30d0916292cc4df2b3ec30aa68c866f82497933be |
| glyph-licenses/weather-icons-OFL.txt | FontPatcher.zip `src/glyphs/weather-icons/OFL.txt` (OFL 1.1) | 69cdedc8a4f57a30d68e4f045af8b9cce0ec3b1a6e7eec1ad41737c894f26790 |

FontPatcher.zip is the same release's official asset,
https://github.com/ryanoasis/nerd-fonts/releases/download/v3.5.1/FontPatcher.zip,
SHA256 42bcb32145499a35732274c7fc48deb434ad0d2e0e118f98527c1479c6fa251a. It is a
licence source only; it is not built, embedded or executed.
`glyph-licenses/` keeps the upstream bytes exactly, CRLF line ends and a BOM
included (`.gitattributes`).

Not retained as full texts, because no v3.5.1 release asset carries them: the
Nerd Fonts repository's own MIT LICENSE, and the licence texts of Devicons
(MIT), Hack's extra glyphs (MIT), Font Awesome Extension (MIT), Seti UI (MIT),
Power Symbols IEC (MIT) and Font Logos (marked "unlicensed" upstream). Their
names, upstreams, versions and licences are in NERD-FONTS-README.md. Fetching
those texts needs a source beyond the release assets, which this card was not
authorized to download; it is recorded as open on BPR-1270.

SPDX: the release manifest (packaging/dependencies.json) declares the component
`NOASSERTION`, the repository's convention for an aggregate of several licences,
with the licences named in its linkage text. Owner/legal release approval of the
aggregate remains the separate release gate.
