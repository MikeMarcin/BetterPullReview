---
id: BPR-0096
title: Virtualize code rows for large files and high DPI
status: Backlog
rfc: RFC-BPR-0005
depends_on: [BPR-0095]
---

# Virtualize code rows for large files and high DPI

Virtualize code row rendering so widget count is bounded by visible UI structure rather than source-line count, at 1.0x, 1.25x, and 2.0x.

Prove it with an instrumented large-file harness that records widget count, and require the count to stay bounded as file length grows by orders of magnitude. A test that merely completes rendering is not sufficient.
