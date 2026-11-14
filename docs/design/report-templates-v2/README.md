# Report templates, round 2: the Dressed report as an HTML template (mockups)

[Round 1](../report-templates-v1/README.md) set a Markdown template for each
report and asked how the app should show it. The owner's answer, 2026-11-14:

> Dressed, possibly with an actual html template to render like how we do our
> attended review.

This round draws that: the agent writes the same Markdown, and the app turns it
into a page in the hand of the attended-review pages. Three reports are drawn
(What changed since my review, Was this addressed?, Review file), light and
dark, with the chosen floating menu. Nothing here is built and no product code
or dependency changed.

[index.html](index.html) is standalone HTML with a Light and Dark link at its
top right.

## The finding on the dependency

**A real HTML and CSS template needs a browser engine, and the app is decided
against one.**

- `docs/architecture/system-architecture.md`, in the dependency inventory: "No
  Qt WebEngine, QML runtime, Electron, Tauri, Node runtime, local HTTP service,
  or browser-extension requirement".
- RFC-BPR-0001: "The application uses Qt Widgets, not Qt WebEngine
  or QML, for the primary product", with a dependency scan that proves it.
- RFC-BPR-0036 (native Markdown rendering): "No WebEngine dependency is
  introduced." RFC-BPR-0038's release qualification scans for one.

Adding Qt WebEngine would reverse those. That is the owner's decision. Nothing
here adds it.

**The look does not need it.** The report tab lays its body out with Qt's rich
text (`QTextDocument`). Each report below was also written in the HTML subset
Qt supports and laid out by Qt 6.11 itself, offscreen. The two come out close:

| | The HTML template | Within today's tab (Qt rich text) |
|---|---|---|
| Serif reading text, sans labels, mono places | Yes | Yes |
| The slate and paper colours, light and dark | Yes | Yes |
| A verdict as a rule at the left of its item | Yes | Yes, a three point table cell |
| The tally: a meter and its counts | Yes, rounded | Yes, square |
| Code with line numbers in a box | Yes, rounded | Yes, square |
| A suggestion as a diff with washes | Yes | Yes |
| Add as draft comment | A button | A link in the text. A real button would be the app's own widget over the page |
| Places at the right of an item, rows in columns | Yes | Yes, with tables |
| Rounded corners, hover, balanced title lines | Yes | No |
| The floating menu and its popover | The app's widgets | The app's widgets |

So the choice for the owner is:

1. **Stay within Qt's rich text.** No new dependency. The app builds the page
   from the Markdown as it builds any body today, with more structure. What is
   lost is in the last three rows.
2. **Add Qt WebEngine** for the report tab. The template is then real HTML and
   CSS, and a Chromium runtime ships with the app, against the three documents
   above.

The frames suggest the first is enough for this hand, which is rules, washes
and type.

## The frames

| Report | The HTML template | Within today's tab |
|---|---|---|
| What changed since my review | [light](png/wc-html-light.png), [dark](png/wc-html-dark.png) | [light](png/wc-qt-light.png), [dark](png/wc-qt-dark.png) |
| Was this addressed? | [light](png/ad-html-light.png), [dark](png/ad-html-dark.png) | [light](png/ad-qt-light.png), [dark](png/ad-qt-dark.png) |
| Review file | [light](png/rf-html-light.png), [dark](png/rf-html-dark.png) | [light](png/rf-qt-light.png), [dark](png/rf-qt-dark.png) |
| The floating menu, open | [light](png/menu-light.png), [dark](png/menu-dark.png) | |

## The hand

Taken from the attended-review page (`review.html` in the skill's assets), whose
own note reads "Chalk on slate by night, slate on paper by day. Colour is spent
only on verdicts".

- **Type.** Charter for the title (28 points) and the reading text (17 on
  1.55). The system sans for labels and counts. The system mono for places and
  code.
- **Colour.** The page's slate and paper, its ink, and three verdict colours:
  red for what still needs work (Not addressed, a blocking issue), amber for
  what is half done or asked (Partly, a question, Lines changed), green for
  what is done or good (Addressed, praise). Everything else is neutral.
- **One device.** A rule at the left marks a verdict: on the summary, on every
  item, and on the item's row in the menu.
- **The tally** is the session bar's meter: one segment a verdict, sized by its
  count, with the counts in words under it.

## What is in the folder

| | |
|---|---|
| `template/report.css` | The template's stylesheet |
| `template/wc.html`, `ad.html`, `rf.html` | The three reports as standalone pages on that stylesheet |
| `qt/*.html` | The same reports in Qt's rich-text subset, one a theme |
| `qt/*.png` | Those files as Qt 6.11 lays them out, offscreen |
| `qt/qtdoc.cpp` | The throwaway program that made them: it loads one file into a `QTextDocument` and saves the drawing. Not product code |
| `examples/review-file.md` | The Review file report's Markdown. The other two are round 1's examples |
| `png/` | Headless renders of the page's frames |

The Qt frames were made with:

```sh
clang++ -std=c++17 -F"$QT/lib" -I"$QT/lib/QtGui.framework/Headers" \
  -I"$QT/lib/QtCore.framework/Headers" -framework QtGui -framework QtCore \
  -Wl,-rpath,"$QT/lib" qt/qtdoc.cpp -o qtdoc
QT_QPA_PLATFORM=offscreen ./qtdoc qt/ad-dark.html ad-dark.png 572 "#20272b" "#ebe6da"
```

with `QT` the machine's Qt 6.11 (`qtbase`). The template frames were drawn by
Chrome's headless mode on a throwaway profile. Neither is a screen capture.

## Not settled

- **The palette.** The attended-review slate is not the app's own surface
  colours. The frames use the slate, as the owner's words ask. Inside the
  window the report tab would then be a different dark from the code beside it.
- **Syntax colours** in code are not drawn in either version.
- **The Qt version is laid out by `QTextDocument` alone**, not by the app's
  `markdown_body_view`, which adds its own code-block tools and link handling.
- **No owner look was filed for this round.**
