---
schema: "implement-rfc.card.v1"
id: "BPR-0917"
title: "Verify and close RFC-BPR-0036"
status: "Backlog"
rfcs: ["RFC-BPR-0036"]
depends_on: ["BPR-0916"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0036"]
---

# Verify and close RFC-BPR-0036

RFC base: `7ad0e43eb92c217ac6cd01fc478bde2968d217a0`.

Planned cards: BPR-0916, BPR-0917;
append later implementation slices. Related layout investigation: [[BPR-0918]].

[RFC-BPR-0036](../../rfcs/RFC-BPR-0036-native-markdown-rendering.md) is registered
only. No renderer implementation or runtime evidence is claimed.

## Closeout checklist

| Criterion | Direct check | Result |
|---|---|---|
| AC-01: Overview and public conversations render the declared Markdown subset through the shipped composition; fixtures cover headings, nested lists, tasks, quotes, code, tables and links. | Composed overview and public conversation Markdown corpus | Not run |
| AC-02: Source, copy and preview preserve original body bytes and publication payloads; empty, malformed and unsupported input stays readable with explicit fallback. | Exact source/copy/publication preservation and readable fallback | Not run |
| AC-03: Hostile HTML, local-file URLs, executable schemes and image/resource references execute nothing and cause no automatic filesystem or network access; admitted link gestures traverse the host-owned path. | Hostile content/resource fixtures and admitted link dispatch | Not run |
| AC-04: Keyboard reading, selection, links and source switching work with appropriate accessibility, light/dark/high-contrast appearance and representative display scales; wide blocks remain usable. | Native keyboard/accessibility and theme/scale inspection | Not run |
| AC-05: Declared bounds and measured long/pathological-body behavior keep the UI responsive; unchanged refresh preserves position and stale completions cannot replace another body's content. | Measured long-body bounds, refresh preservation and stale results | Not run |
| AC-06: Composed long-description reading reaches the final content without a fixed six-line viewport constraint; documentation describes the supported subset, fallback, limits and image policy. | Full long-description reading and supported-feature documentation | Not run |

Record producing commits, commands and observed results for every criterion.
Keep unavailable platform evidence explicit. Require implementation cards done
and RFC dependency eligibility before changing status in canonical order.
Run both documentation gates and focused public-path checks after integration.
