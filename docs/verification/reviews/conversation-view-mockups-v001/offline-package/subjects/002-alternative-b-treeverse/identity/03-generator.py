#!/usr/bin/env python3
"""Render the BPR-0211 and BPR-0212 conversation-view mockups as SVG.

These are mockups for a product decision, not evidence of implemented
behaviour. Nothing here is compiled, composed, or reachable from the running
application. What makes them worth looking at is that every string, colour, and
row shape is copied from live source rather than invented:

  palette          src/adapters/qt/src/appearance_profile.cpp:76-103
  row shape        src/adapters/qt/src/github_conversation_delegate.cpp:68-144
  projection       src/application/include/bpr/application/github_projection.hpp:124-139
  parent edge      src/application/src/github_projection_mapping.cpp:148,162,175,192
  reply edge       src/application/include/bpr/application/github_read_model.hpp:233
  epoch labels     src/adapters/qt/src/epoch_history_model.cpp:26-137

Standard library only, matching `scripts/dif_assets.py`: the repository does not
carry Pillow or a vector library, and a mockup generator is not the place to
introduce one. Output is a square canvas so that a thumbnailing rasteriser has
no aspect to letterbox.

    python3 render_mockups.py --out mockups
"""

from __future__ import annotations

import argparse
import pathlib
import sys

CANVAS = 1600
NODE_WIDTH = 128

# BetterPullReview Light, read from appearance_profile.cpp:76-103. Only roles
# that appear in a mockup are listed; inventing a value for an unread role is
# how a mockup stops being faithful.
SURFACE = "#F6F7F9"
TEXT = "#1F2328"
CODE_BACKGROUND = "#FFFFFF"
CODE_GUTTER = "#F0F2F5"
LINE_NUMBER_TEXT = "#5B6570"
DIFF_ADDED = "#E6FFEC"
DIFF_REMOVED = "#FFEBE9"
GROUP_BOUNDARY_RULE = "#8C8C8C"
STATE_UNREAD = "#0969DA"
STATE_ACCEPTED = "#1A7F37"
STATE_DEFECT = "#CF222E"
STATE_STALE = "#5B6570"

# Neutral mockup chrome. These are NOT product palette roles and no appearance
# profile defines them; they exist because a mockup needs page furniture -
# panel borders, a synthesized-content stamp - that the product has no role for.
# Everything that represents a product surface uses the roles above.
CHROME_BORDER = "#D4D9E0"
CHROME_EDGE = "#C3C9D2"
CHROME_TINT = "#EAF2FD"
CHROME_CONTROL = "#8C93A0"
CHROME_MUTED = "#B6BCC6"
STAMP_BORDER = "#C79A2E"
STAMP_TEXT = "#7A5A0B"

UI_FONT = "-apple-system, 'Helvetica Neue', Helvetica, Arial, sans-serif"
CODE_FONT = "ui-monospace, 'SF Mono', Menlo, Consolas, monospace"

# The delegate joins its facts with exactly this separator.
FACT_SEPARATOR = "  ·  "


def escape(value: str) -> str:
	return (
		value.replace("&", "&amp;")
		.replace("<", "&lt;")
		.replace(">", "&gt;")
	)


class Canvas:
	"""A minimal SVG accumulator. Deterministic: no time, no randomness."""

	def __init__(self) -> None:
		self.parts: list[str] = []

	def rect(
		self,
		x: float,
		y: float,
		width: float,
		height: float,
		fill: str,
		*,
		stroke: str | None = None,
		radius: float = 0,
		stroke_width: float = 1,
		opacity: float | None = None,
	) -> None:
		attributes = [
			f'x="{x:g}" y="{y:g}" width="{width:g}" height="{height:g}"',
			f'fill="{fill}"',
		]
		if radius:
			attributes.append(f'rx="{radius:g}"')
		if stroke:
			attributes.append(f'stroke="{stroke}" stroke-width="{stroke_width:g}"')
		if opacity is not None:
			attributes.append(f'opacity="{opacity:g}"')
		self.parts.append(f"<rect {' '.join(attributes)}/>")

	def line(
		self,
		x1: float,
		y1: float,
		x2: float,
		y2: float,
		stroke: str,
		*,
		width: float = 1,
		dash: str | None = None,
	) -> None:
		attributes = [
			f'x1="{x1:g}" y1="{y1:g}" x2="{x2:g}" y2="{y2:g}"',
			f'stroke="{stroke}" stroke-width="{width:g}"',
		]
		if dash:
			attributes.append(f'stroke-dasharray="{dash}"')
		self.parts.append(f"<line {' '.join(attributes)}/>")

	def path(self, definition: str, stroke: str, *, width: float = 1) -> None:
		self.parts.append(
			f'<path d="{definition}" fill="none" stroke="{stroke}" '
			f'stroke-width="{width:g}"/>'
		)

	def text(
		self,
		x: float,
		y: float,
		value: str,
		*,
		size: float = 18,
		fill: str = TEXT,
		font: str = UI_FONT,
		weight: str = "400",
		anchor: str = "start",
		opacity: float | None = None,
		preserve: bool = False,
	) -> None:
		attributes = [
			f'x="{x:g}" y="{y:g}"',
			f'font-family="{font}"',
			f'font-size="{size:g}"',
			f'font-weight="{weight}"',
			f'fill="{fill}"',
		]
		if anchor != "start":
			attributes.append(f'text-anchor="{anchor}"')
		if opacity is not None:
			attributes.append(f'opacity="{opacity:g}"')
		if preserve:
			# Source indentation is meaningful; SVG collapses it otherwise.
			attributes.append('xml:space="preserve"')
		self.parts.append(
			f"<text {' '.join(attributes)}>{escape(value)}</text>"
		)

	def render(self) -> str:
		body = "\n\t".join(self.parts)
		return (
			f'<svg xmlns="http://www.w3.org/2000/svg" width="{CANVAS}" '
			f'height="{CANVAS}" viewBox="0 0 {CANVAS} {CANVAS}">\n\t'
			f'<rect width="{CANVAS}" height="{CANVAS}" fill="{SURFACE}"/>\n\t'
			f"{body}\n</svg>\n"
		)


def chrome(canvas: Canvas, title: str, subtitle: str, footer: str) -> None:
	"""The shared banner. Every mockup says what it is on its face."""
	canvas.rect(0, 0, CANVAS, 132, "#FFFFFF")
	canvas.line(0, 132, CANVAS, 132, CHROME_BORDER, width=2)
	canvas.text(56, 58, title, size=30, weight="600")
	canvas.text(56, 96, subtitle, size=19, fill=LINE_NUMBER_TEXT)

	stamp = "MOCKUP - NOT IMPLEMENTED"
	canvas.rect(CANVAS - 56 - 330, 34, 330, 42, "#FFF1D6", radius=6,
				stroke=STAMP_BORDER)
	canvas.text(CANVAS - 56 - 165, 62, stamp, size=17, weight="600",
				fill=STAMP_TEXT, anchor="middle")

	canvas.line(56, CANVAS - 128, CANVAS - 56, CANVAS - 128,
				GROUP_BOUNDARY_RULE, width=1)
	y = CANVAS - 96
	for line in footer.split("\n"):
		canvas.text(56, y, line, size=18, fill=LINE_NUMBER_TEXT)
		y += 28


def conversation_row(
	canvas: Canvas,
	x: float,
	y: float,
	width: float,
	depth: int,
	heading: str,
	body: str,
	facts: str,
	*,
	selected: bool = False,
) -> float:
	"""One drawer row, painted the way github_conversation_delegate paints it.

	Heading, then body, then a facts line joined with "  ·  ". No icons, no
	colour coding: the shipped delegate renders those facts as plain text, and
	drawing badges here would misrepresent what exists.
	"""
	indent = depth * 38
	height = 92 if body else 62
	if selected:
		canvas.rect(x, y - 4, width, height, "#F2F6FC", radius=4)
	canvas.text(x + 20 + indent, y + 22, heading, size=19, weight="600")
	offset = 0
	if body:
		canvas.text(x + 20 + indent, y + 50, body, size=18, fill="#24292F")
		offset = 28
	canvas.text(x + 20 + indent, y + 50 + offset, facts, size=16,
				fill=LINE_NUMBER_TEXT)
	if depth:
		canvas.line(x + 20 + indent - 18, y - 4, x + 20 + indent - 18,
					y + height - 8, CHROME_BORDER, width=2)
	return y + height + 10


def panel(
	canvas: Canvas,
	x: float,
	y: float,
	width: float,
	height: float,
	title: str,
	note: str = "",
) -> float:
	canvas.rect(x, y, width, height, "#FFFFFF", radius=10, stroke=CHROME_BORDER)
	canvas.rect(x, y, width, 56, "#F0F2F5", radius=10)
	canvas.rect(x, y + 40, width, 16, "#F0F2F5")
	canvas.line(x, y + 56, x + width, y + 56, CHROME_BORDER)
	canvas.text(x + 20, y + 36, title, size=19, weight="600")
	if note:
		canvas.text(x + width - 20, y + 36, note, size=16,
					fill=LINE_NUMBER_TEXT, anchor="end")
	return y + 56


def code_pane(
	canvas: Canvas,
	x: float,
	y: float,
	width: float,
	rows: list[tuple[int, str, str]],
) -> None:
	"""A small diff pane. `rows` is (line number, kind, text)."""
	line_height = 30
	canvas.rect(x, y, width, len(rows) * line_height, CODE_BACKGROUND)
	canvas.rect(x, y, 68, len(rows) * line_height, CODE_GUTTER)
	for index, (number, kind, text) in enumerate(rows):
		top = y + index * line_height
		if kind == "added":
			canvas.rect(x + 68, top, width - 68, line_height, DIFF_ADDED)
		elif kind == "removed":
			canvas.rect(x + 68, top, width - 68, line_height, DIFF_REMOVED)
		canvas.text(x + 56, top + 21, str(number), size=15,
					fill=LINE_NUMBER_TEXT, font=CODE_FONT, anchor="end")
		marker = "+" if kind == "added" else "-" if kind == "removed" else " "
		canvas.text(x + 82, top + 21, f"{marker} {text}", size=15,
					font=CODE_FONT, preserve=True)


# --------------------------------------------------------------------------
# The synthesized conversation.
#
# No recorded pull request in this repository carries a conversation at all,
# and the only conversation content that exists - the GraphQL contract fixture
# tests/fixtures/github/contracts/v1/github-com/responses/review_threads.json -
# is three threads of one, two and one comments with a single replyTo edge.
# Nothing here is claimed to be recorded. It is written to be representative of
# the shape the projection can carry.
# --------------------------------------------------------------------------

ANCHOR_FILE = "src/core/src/diff_alignment.cpp"


def mockup_today(out: pathlib.Path) -> None:
	canvas = Canvas()
	chrome(
		canvas,
		"Today: the conversations drawer as it ships",
		"Filtered to the current file. This is the baseline the two alternatives "
		"are judged against, not a proposal.",
		"Every row above is painted by github_conversation_delegate.cpp:68-144: heading, body, then a facts line joined with \"  ·  \".\n"
		"There are no lane rails and no git-graph rendering in the shipped code. You asked for that style in BPR-0191; the reading that \"filtering by lane means\n"
		"filtering by thread\" is that card's own argument, not something you said - see mockup 05, which draws what you asked for so it can be decided rather than assumed.\n"
		"A thread's first comment reads \"Reply by\" too, because the delegate asks whether the row has a parent and every comment's parent is the thread.",
	)

	left = 56
	top = 180
	drawer_width = 700
	code_width = CANVAS - left * 2 - drawer_width - 28

	code_top = panel(canvas, left, top, code_width, 1180,
					 ANCHOR_FILE.rsplit("/", 1)[-1], "Whole PR")
	listing = [
		(206, "same", "/// Pair rows of two documents by exact content first,"),
		(207, "same", "/// then by similarity within the remaining runs."),
		(208, "same", "[[nodiscard]] auto align_rows("),
		(209, "same", "    const document& left,"),
		(210, "same", "    const document& right) -> alignment"),
		(211, "same", "{"),
		(212, "same", "    auto result = alignment{ };"),
		(213, "removed", "    auto cursor = left.begin();"),
		(214, "added", "    auto cursor = left.cbegin();"),
		(215, "same", "    while (cursor != left.cend()) {"),
		(216, "same", "        if (const auto exact = pair_exact(cursor, right);"),
		(217, "same", "            exact.has_value()) {"),
		(218, "same", "            result.push_back(*exact);"),
		(219, "same", "            ++cursor;"),
		(220, "same", "            continue;"),
		(221, "same", "        }"),
		(222, "same", "        break;"),
		(223, "same", "    }"),
		(224, "same", ""),
		(225, "same", "    pair_similar(cursor, left.cend(), right, result);"),
		(226, "same", "    return result;"),
		(227, "same", "}"),
		(228, "same", ""),
		(229, "same", "[[nodiscard]] auto pair_similar("),
		(230, "same", "    document::const_iterator first,"),
		(231, "same", "    document::const_iterator last,"),
		(232, "same", "    const document& right,"),
		(233, "same", "    alignment& out) -> void"),
		(234, "same", "{"),
		(235, "same", "    for (; first != last; ++first) {"),
		(236, "same", "        out.push_back(best_match(*first, right));"),
		(237, "same", "    }"),
	]
	code_pane(canvas, left, code_top + 16, code_width, listing)
	rule_y = code_top + 16 + len(listing) * 30 + 24
	canvas.line(left + 20, rule_y, left + code_width - 20, rule_y,
				GROUP_BOUNDARY_RULE)
	canvas.text(left + 20, rule_y + 38,
				"Selected line 214 carries two threads; line 288 carries a",
				size=17, fill=LINE_NUMBER_TEXT)
	canvas.text(left + 20, rule_y + 66,
				"third that GitHub has already marked outdated.", size=17,
				fill=LINE_NUMBER_TEXT)

	drawer_left = left + code_width + 28
	drawer_top = panel(canvas, drawer_left, top, drawer_width, 1180,
					   "Conversations", "3 threads in this file")
	canvas.text(drawer_left + 20, drawer_top + 34,
				f"Anchored to {ANCHOR_FILE}", size=16, fill=STATE_UNREAD)

	y = drawer_top + 60
	y = conversation_row(
		canvas, drawer_left, y, drawer_width, 0, "Review thread", "",
		f"{ANCHOR_FILE}:214{FACT_SEPARATOR}Open", selected=True)
	y = conversation_row(
		canvas, drawer_left, y, drawer_width, 1, "Reply by @dcarter",
		"begin() on a const document returns a const_iterator already;",
		f"{ANCHOR_FILE}:214")
	y = conversation_row(
		canvas, drawer_left, y, drawer_width, 1, "Reply by @mikemarcin",
		"It does, but the overload set makes that accidental. Keep cbegin().",
		f"{ANCHOR_FILE}:214")
	y = conversation_row(
		canvas, drawer_left, y, drawer_width, 1, "Reply by @apatel",
		"Agreed - it also matches the cend() below it.",
		f"{ANCHOR_FILE}:214")

	y += 14
	y = conversation_row(
		canvas, drawer_left, y, drawer_width, 0, "Review thread", "",
		f"{ANCHOR_FILE}:214{FACT_SEPARATOR}Resolved")
	y = conversation_row(
		canvas, drawer_left, y, drawer_width, 1, "Reply by @rlindgren",
		"Does this still hold when the document is empty?",
		f"{ANCHOR_FILE}:214{FACT_SEPARATOR}Resolved")

	y += 14
	y = conversation_row(
		canvas, drawer_left, y, drawer_width, 0, "Review thread", "",
		f"{ANCHOR_FILE}:288{FACT_SEPARATOR}Open{FACT_SEPARATOR}Outdated")
	y = conversation_row(
		canvas, drawer_left, y, drawer_width, 1, "Reply by @dcarter",
		"This branch is unreachable after the rewrite above.",
		f"{ANCHOR_FILE}:288{FACT_SEPARATOR}Outdated")

	canvas.text(drawer_left + 20, y + 36,
				"Threads on other files are filtered out entirely.",
				size=17, fill=STATE_STALE)

	(out / "01-today.svg").write_text(canvas.render(), encoding="utf-8")


def mockup_whole_review(out: pathlib.Path) -> None:
	canvas = Canvas()
	chrome(
		canvas,
		"Alternative A: the whole review in one place",
		"Every thread across every file, plus the pull-request-level comments "
		"the file drawer never shows.",
		"Reachable from the projection exactly as it is today: this drops the set_anchor_file filter and groups by Path. No model change, no fetch change.\n"
		"The row vocabulary is unchanged, so \"Review by\", \"Pull request comment by\" and the Open/Resolved/Outdated facts are the strings the delegate already paints.\n"
		"What it costs is a second surface to keep in sync with the drawer, and a second place a reviewer can lose their position.",
	)

	left = 56
	top = 180
	width = CANVAS - left * 2

	body_top = panel(canvas, left, top, width, 1010,
					 "Review conversation", "12 threads · 4 unresolved")

	y = body_top + 28
	canvas.rect(left + 16, y, width - 32, 44, "#F0F2F5", radius=6)
	canvas.text(left + 32, y + 30, "Pull request", size=18, weight="600")
	canvas.text(left + width - 32, y + 30, "2 comments", size=16,
				fill=LINE_NUMBER_TEXT, anchor="end")
	y += 60
	y = conversation_row(
		canvas, left, y, width, 0, "Review by @mikemarcin",
		"Two things below, then this is good to go.",
		"Changes requested")
	y = conversation_row(
		canvas, left, y, width, 0, "Pull request comment by @rlindgren",
		"Rebased onto main to pick up the alignment fix.", "")

	y += 16
	canvas.rect(left + 16, y, width - 32, 44, "#F0F2F5", radius=6)
	canvas.text(left + 32, y + 30, ANCHOR_FILE, size=18, weight="600",
				font=CODE_FONT)
	canvas.text(left + width - 32, y + 30, "3 threads · 1 unresolved",
				size=16, fill=LINE_NUMBER_TEXT, anchor="end")
	y += 60
	y = conversation_row(
		canvas, left, y, width, 0, "Review thread", "",
		f"{ANCHOR_FILE}:214{FACT_SEPARATOR}Open")
	y = conversation_row(
		canvas, left, y, width, 1, "Reply by @dcarter",
		"begin() on a const document returns a const_iterator already;",
		f"{ANCHOR_FILE}:214")
	y = conversation_row(
		canvas, left, y, width, 0, "Review thread", "",
		f"{ANCHOR_FILE}:288{FACT_SEPARATOR}Open{FACT_SEPARATOR}Outdated")

	y += 16
	other = "src/adapters/qt/src/diff_pane.cpp"
	canvas.rect(left + 16, y, width - 32, 44, "#F0F2F5", radius=6)
	canvas.text(left + 32, y + 30, other, size=18, weight="600", font=CODE_FONT)
	canvas.text(left + width - 32, y + 30, "5 threads · 2 unresolved",
				size=16, fill=LINE_NUMBER_TEXT, anchor="end")
	y += 60
	y = conversation_row(
		canvas, left, y, width, 0, "Review thread", "",
		f"{other}:96{FACT_SEPARATOR}Open")
	y = conversation_row(
		canvas, left, y, width, 1, "Reply by @apatel",
		"Can this repaint be coalesced with the one below?",
		f"{other}:96")

	canvas.text(left + 32, y + 40,
				"… 4 more files", size=17, fill=STATE_STALE)

	(out / "02-whole-review.svg").write_text(canvas.render(), encoding="utf-8")


def mockup_treeverse(out: pathlib.Path) -> None:
	canvas = Canvas()
	chrome(
		canvas,
		"Alternative B: a Treeverse-style overview of one deep thread",
		"A map of the reply structure, with the read path highlighted and one "
		"node in focus.",
		"The conversation drawn here is SYNTHESIZED. No recorded pull request in this repository has any conversation, and the deepest real structure anywhere\n"
		"is one replyTo edge in a contract fixture. This mockup therefore shows a shape the product could carry, not one it has ever displayed.\n"
		"Cost: the reply edge exists at github_read_model.hpp:233 and is discarded at github_projection_mapping.cpp:175 - but changing that line alone would FLATTEN the\n"
		"drawer, because github_conversation_model.cpp:317-326 drops any parent that is not a review_thread. The read/unread state legended below is also proposed, not carried.",
	)

	left = 56
	top = 180
	map_width = 880
	detail_width = CANVAS - left * 2 - map_width - 28

	map_top = panel(canvas, left, top, map_width, 980,
					"Thread map", "18 comments · depth 5")

	# depth, index-in-depth, label, state
	nodes = [
		(0, 0, "dcarter", "root"),
		(1, 0, "mikemarcin", "read"),
		(1, 1, "apatel", "read"),
		(2, 0, "rlindgren", "read"),
		(2, 1, "dcarter", "unread"),
		(2, 2, "mikemarcin", "unread"),
		(3, 0, "apatel", "focus"),
		(3, 1, "dcarter", "unread"),
		(4, 0, "mikemarcin", "unread"),
		(4, 1, "rlindgren", "unread"),
		(5, 0, "apatel", "unread"),
	]
	edges = [
		(0, 1), (0, 2), (1, 3), (1, 4), (2, 5),
		(3, 6), (3, 7), (6, 8), (6, 9), (8, 10),
	]

	# Depth runs down the panel and siblings run across it. A horizontal tree
	# put six depths into 880 points and left no gap for the edges to show in.
	origin_x = left + 150
	origin_y = map_top + 96
	step_x = 214
	step_y = 100

	def position(index: int) -> tuple[float, float]:
		depth, slot, _, _ = nodes[index]
		return origin_x + slot * step_x + depth * 16, origin_y + depth * step_y

	# The path from the root to the focused node, drawn heavier so the reader
	# can see where they are. This is the whole point of the alternative.
	read_path = [(0, 1), (1, 3), (3, 6)]

	for start, end in edges:
		x1, y1 = position(start)
		x2, y2 = position(end)
		on_path = (start, end) in read_path
		start_y = y1 + 22
		end_y = y2 - 22
		waist = (start_y + end_y) / 2
		canvas.path(
			f"M {x1:g} {start_y:g} C {x1:g} {waist:g} {x2:g} {waist:g} "
			f"{x2:g} {end_y:g}",
			STATE_UNREAD if on_path else CHROME_EDGE, width=4 if on_path else 2)

	fills = {
		"root": ("#FFFFFF", STATE_UNREAD),
		"read": ("#FFFFFF", CHROME_BORDER),
		"unread": (CHROME_TINT, STATE_UNREAD),
		"focus": ("#FFF1D6", STAMP_BORDER),
	}
	for index, (_, _, author, state) in enumerate(nodes):
		x, y = position(index)
		fill, stroke = fills[state]
		canvas.rect(x - NODE_WIDTH / 2, y - 22, NODE_WIDTH, 44, fill,
					radius=8, stroke=stroke,
					stroke_width=2 if state in ("focus", "unread") else 1)
		canvas.text(x, y + 6, f"@{author}", size=15, anchor="middle")

	legend_y = map_top + 720
	canvas.line(left + 20, legend_y - 30, left + map_width - 20, legend_y - 30,
				GROUP_BOUNDARY_RULE)
	for offset, (label, state) in enumerate(
			[("read", "read"), ("unread", "unread"), ("in focus", "focus")]):
		fill, stroke = fills[state]
		x = left + 40 + offset * 220
		canvas.rect(x, legend_y, 34, 24, fill, radius=5, stroke=stroke,
					stroke_width=2)
		canvas.text(x + 48, legend_y + 18, label, size=17,
					fill=LINE_NUMBER_TEXT)

	canvas.text(left + 40, legend_y + 80,
				"Depth 5. The drawer renders this same thread as a flat list",
				size=18, fill=TEXT)
	canvas.text(left + 40, legend_y + 110,
				"of 18 replies, because every ParentId is the thread root.",
				size=18, fill=TEXT)

	detail_left = left + map_width + 28
	detail_top = panel(canvas, detail_left, top, detail_width, 980,
					   "In focus", "3 of 18")
	y = detail_top + 40
	y = conversation_row(
		canvas, detail_left, y, detail_width, 0, "Reply by @apatel",
		"Then cend() is the comparison that matters.",
		f"{ANCHOR_FILE}:214")

	y += 30
	canvas.text(detail_left + 20, y, "Path to this reply", size=17,
				weight="600")
	y += 34
	for label in ["@dcarter (root)", "@mikemarcin", "@rlindgren", "@apatel"]:
		canvas.text(detail_left + 32, y, label, size=17,
					fill=LINE_NUMBER_TEXT)
		y += 30

	y += 20
	canvas.line(detail_left + 20, y, detail_left + detail_width - 20, y,
				GROUP_BOUNDARY_RULE)
	y += 36
	canvas.text(detail_left + 20, y, "2 replies below this one", size=17,
				fill=STATE_UNREAD)

	(out / "03-treeverse.svg").write_text(canvas.render(), encoding="utf-8")


def mockup_revision_join(out: pathlib.Path) -> None:
	canvas = Canvas()
	chrome(
		canvas,
		"BPR-0212: conversation and code, on the same revision axis",
		"Which comments were live on each side, and what went outdated in "
		"between.",
		"The epoch axis is real: epoch_comparison_control already offers independent left and right selectors, and the labels above are the exact format at\n"
		"epoch_history_model.cpp:132-137. The conversation axis is real. The projection carries no commit OID, epoch, or ordinal - verified field by field at\n"
		"github_projection.hpp:124-139 - so no cell above can be computed from it. But a review's commit_id is already REQUIRED on read and then dropped, so this\n"
		"is nearer than \"unknown\": what is missing for a per-comment row is a field the review-thread GraphQL selection does not yet ask for.",
	)

	left = 56
	top = 180
	width = CANVAS - left * 2

	selector_top = panel(canvas, left, top, width, 150,
						 "Comparison", "Whole PR · Previous upload · Last reviewed")
	for offset, (side, label) in enumerate([
		("Left", "Epoch 2 · Head — Author upload — Available"),
		("Right", "Epoch 4 · Head — Force-push/rebase — Available"),
	]):
		x = left + 24 + offset * (width // 2)
		canvas.text(x, selector_top + 40, side, size=17,
					fill=LINE_NUMBER_TEXT)
		canvas.rect(x, selector_top + 54, width // 2 - 48, 44, "#FFFFFF",
					radius=6, stroke=CHROME_CONTROL)
		canvas.text(x + 14, selector_top + 82, label, size=17)
		canvas.text(x + width // 2 - 76, selector_top + 82, "▾", size=17,
					fill=LINE_NUMBER_TEXT)

	grid_top = panel(canvas, left, top + 178, width, 620,
					 "Comments against revisions",
					 "outlined cells are the join that does not exist")

	epochs = [
		"Epoch 1 · First observation",
		"Epoch 2 · Author upload",
		"Epoch 3 · Base update",
		"Epoch 4 · Force-push/rebase",
	]
	column_width = (width - 460) / len(epochs)
	header_y = grid_top + 44
	for index, label in enumerate(epochs):
		x = left + 440 + index * column_width
		selected = index in (1, 3)
		if selected:
			canvas.rect(x, grid_top + 12, column_width - 12, 578, "#F2F6FC",
						radius=6)
		canvas.text(x + 10, header_y, label.split(" · ")[0], size=17,
					weight="600" if selected else "400")
		canvas.text(x + 10, header_y + 24, label.split(" · ")[1], size=15,
					fill=LINE_NUMBER_TEXT)

	# Three states, not two. A comment written at epoch 2 was not "outdated" at
	# epoch 1, it did not exist - and collapsing those two is exactly the loss
	# of information this card is about.
	states = {
		"live": (DIFF_ADDED, STATE_ACCEPTED, "live"),
		"outdated": (DIFF_REMOVED, STATE_DEFECT, "outdated"),
		"absent": (CODE_GUTTER, CHROME_MUTED, "not yet written"),
	}
	rows = [
		("@dcarter on :214", ["live"] * 4, "live throughout"),
		("@mikemarcin on :214", ["absent", "live", "live", "live"],
		 "written at epoch 2"),
		("@rlindgren on :288", ["live", "live", "outdated", "outdated"],
		 "the base update moved its anchor"),
		("@apatel on :96", ["live", "live", "live", "outdated"],
		 "the force-push rewrote the line"),
	]
	y = grid_top + 120
	for label, cells, note in rows:
		canvas.text(left + 24, y + 26, label, size=18)
		canvas.text(left + 24, y + 52, note, size=15, fill=LINE_NUMBER_TEXT)
		for index, state in enumerate(cells):
			x = left + 440 + index * column_width
			fill, stroke, caption = states[state]
			canvas.rect(x + 10, y + 8, column_width - 44, 52, fill, radius=6,
						stroke=stroke)
			canvas.text(x + 10 + (column_width - 44) / 2, y + 40, caption,
						size=15, fill=stroke, anchor="middle")
		y += 124

	gap_top = panel(canvas, left, top + 822, width, 424,
					"What the join needs, and what exists")
	y = gap_top + 40
	entries = [
		("The revision axis", "exists",
		 "accepted_pull_request_revision carries Base and Head OIDs and an "
		 "Origin of live or explicit_historical_import."),
		("The conversation axis", "exists",
		 "github_conversation_projection carries Path, Line, Resolved and "
		 "Outdated for every node."),
		("A comment's revision", "not projected",
		 "Nothing on github_conversation_projection keys a node to a revision. "
		 "A review's commit_id is required on read and dropped here; a "
		 "comment's commit is not requested at all."),
		("Therefore", "",
		 "Not computable from the projection today, and nearer than it looks: "
		 "the write path already sends commit_id. It is still a model question "
		 "before a presentation one, which is why this asks for an RFC."),
	]
	for title, state, detail in entries:
		canvas.text(left + 24, y, title, size=19, weight="600")
		if state:
			colour = STATE_ACCEPTED if state == "exists" else STATE_DEFECT
			canvas.text(left + 300, y, state, size=18, fill=colour,
						weight="600")
		canvas.text(left + 24, y + 30, detail, size=17, fill="#24292F")
		y += 90

	(out / "04-revision-join.svg").write_text(canvas.render(), encoding="utf-8")


def mockup_lane_rails(out: pathlib.Path) -> None:
	"""What BPR-0191 recorded the owner asking for, which was never drawn.

	[[BPR-0191]]:92-93: "The product owner took route 2 and asked for a threaded
	conversations drawer drawn in a git-graph / DAGVIZ style, with filtering by
	lane." The narrowing at :108-110 - filtering by lane means filtering by
	thread - is that card's engineering argument. The owner's own quoted words
	at :120-122 are about anchoring to the current file and say nothing about
	rails. So this is an open request, not a settled one, and it is drawn here
	so it can be answered rather than assumed away.
	"""
	canvas = Canvas()
	chrome(
		canvas,
		"05: the git-graph drawer you asked for, which was never built",
		"Threads as lanes with rails, filtered by lane. BPR-0191 recorded this "
		"request and then reinterpreted it.",
		"This is not a secondary view - it is the anchored drawer itself, drawn the way BPR-0191 recorded you asking for it. It is on the page because that card\n"
		"settled the request with its own reasoning rather than yours, and the baseline mockup repeated the conclusion. Decide it rather than inherit it.\n"
		"Cost is presentation-only: lanes are threads, which the projection already carries. It changes no model, and it is compatible with either alternative above.",
	)

	left = 56
	top = 180
	width = CANVAS - left * 2

	body_top = panel(canvas, left, top, width, 940, "Conversations",
					 "3 lanes in this file · filtered by lane")

	lane_colours = [STATE_UNREAD, STATE_ACCEPTED, STATE_DEFECT]
	lane_labels = [
		("Lane 1", f"{ANCHOR_FILE}:214", "Open", 3),
		("Lane 2", f"{ANCHOR_FILE}:214", "Resolved", 1),
		("Lane 3", f"{ANCHOR_FILE}:288", "Outdated", 1),
	]

	rail_x = left + 60
	y = body_top + 50
	for index, (lane, anchor, state, replies) in enumerate(lane_labels):
		colour = lane_colours[index]
		height = 84 + replies * 74
		canvas.line(rail_x + index * 34, y + 14, rail_x + index * 34,
					y + height - 20, colour, width=4)
		canvas.rect(left + 20, y - 10, width - 40, height, "#FFFFFF", radius=8,
					stroke=CHROME_BORDER)
		canvas.line(rail_x + index * 34, y + 14, rail_x + index * 34,
					y + height - 20, colour, width=4)

		canvas.text(left + 150, y + 20, lane, size=19, weight="600",
					fill=colour)
		canvas.text(left + 150, y + 48, f"{anchor}{FACT_SEPARATOR}{state}",
					size=16, fill=LINE_NUMBER_TEXT)

		node_y = y + 88
		authors = ["@dcarter", "@mikemarcin", "@apatel"][:replies]
		for author in authors:
			canvas.rect(rail_x + index * 34 - 9, node_y - 9, 18, 18, colour,
						radius=9)
			canvas.text(left + 150, node_y + 6, author, size=18)
			canvas.text(left + 150 + 190, node_y + 6,
						"replied on this lane", size=16,
						fill=LINE_NUMBER_TEXT)
			node_y += 74
		y += height + 22

	canvas.text(left + 40, y + 30,
				"Filter: show only lanes with something unresolved  ·  "
				"collapse a lane to its root", size=17, fill=STATE_UNREAD)
	canvas.text(left + 40, y + 62,
				"A lane is a thread. That is BPR-0191's own reading, and it is "
				"the part of it that was right.", size=17,
				fill=LINE_NUMBER_TEXT)

	(out / "05-lane-rails.svg").write_text(canvas.render(), encoding="utf-8")


def main(argv: list[str]) -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--out", default="mockups",
						help="directory to write the SVG sources into")
	arguments = parser.parse_args(argv)

	out = pathlib.Path(arguments.out)
	out.mkdir(parents=True, exist_ok=True)

	mockup_today(out)
	mockup_whole_review(out)
	mockup_treeverse(out)
	mockup_revision_join(out)
	mockup_lane_rails(out)

	for path in sorted(out.glob("*.svg")):
		print(f"wrote {path}")
	return 0


if __name__ == "__main__":
	raise SystemExit(main(sys.argv[1:]))
