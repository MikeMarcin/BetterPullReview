gemoji 4.1.0

Source: https://codeload.github.com/github/gemoji/tar.gz/refs/tags/v4.1.0

SHA256: 68c26ccc2901eca303f29d38efe65b66434f92461fb218a00fd7919fb7839c21

Archive size: 104847 bytes. Only `db/emoji.json` (SHA256
b174ae2aeb321b52f64adb9ff412f966a7f338839d780784dd15dcad702c2dd6) is read,
by `scripts/generate_markdown_emoji_table.py`, which writes the committed
`src/adapters/qt/src/markdown_emoji_table.inc`: each of its 1,913 aliases with
its emoji characters. No gemoji code, image or build rule is used, and nothing
is fetched at build or run time. The table serves the Markdown reader's emoji
shortcodes ([[BPR-1544]]), as GitHub's own renderer uses gemoji.
MIT license retained alongside this record.

License SHA256: 7ea1d1fd0602e6623b9a65e791499a0ea5a2f5f60f91bfad78cbaeee19da26e6.
