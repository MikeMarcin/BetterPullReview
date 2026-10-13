# M4 native pane Find design

Mandate `find-and-find-in-files-v1-r1`; assigned `pane-find`, BPR-1422.
The implementer loaded frontend-design before UX/rendering, from
`/Users/jaeger/.claude/plugins/cache/claude-plugins-official/frontend-design/f359d43b4060/skills/frontend-design/SKILL.md`.

The brief is a read-only source reader with exact immutable comparisons. The
native palette roles Window, WindowText, Base, Text, Highlight and PlaceholderText
supply the surfaces, text, active outline and quiet wash. No fixed hex palette,
font family, style or QSS is introduced. Query/control text uses QApplication's
interface font; the existing source readers keep configured code typography.

The bar sits above the source comparison rather than covering an occurrence:

```text
[Earlier / Later / Unified] [multiline native query             ] [↑] [↓] [×]
[Aa] [Word] [.*] [Selection]  [position/count or qualified status           ]
```

The second row keeps every option visible at narrow widths. The scope label
makes the retained source target explicit. Ordinary/native focus rings remain.
A quiet palette-derived wash shows visible occurrences; the current one has an
outline. Thin search marks share the existing scroll track, leaving the link map
and source row geometry intact. No per-match widget or animation is introduced.

The initial plan was checked against the RFC: native controls/roles override the
skill's generic typeface/palette exploration. No owner choice is needed for this
engineering composition. Full-window objective offscreen renders and shipped
shortcut/interaction courses will judge the result. Offscreen evidence does not
certify native desktop appearance, Windows/Linux or an installed package.
