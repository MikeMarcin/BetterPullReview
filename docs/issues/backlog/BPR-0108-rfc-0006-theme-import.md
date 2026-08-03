---
id: BPR-0108
title: Implement safe theme import and export
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0107]
---

# Implement safe theme import and export

Implement theme import and export over the semantic role vocabulary.

Import must reject unknown fields, URLs, includes, oversized files, and executable or style payloads. Each rejection needs its own case; a single catch-all that refuses everything would pass a weak test while accepting nothing useful.
