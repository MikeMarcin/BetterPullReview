---
schema: "implement-rfc.card.v1"
id: "BPR-2150"
title: "Verify and close RFC-BPR-0125"
status: "Backlog"
rfcs: ["RFC-BPR-0125"]
depends_on: ["BPR-2144","BPR-2145","BPR-2146","BPR-2147","BPR-2148","BPR-2149"]
closeout_for: ["RFC-BPR-0125"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2144--BPR-2150--17b3cd6e7b65.json"}
---

# BPR-2150: Verify and close RFC-BPR-0125

RFC base: `4bdf03b330388cb36b07bdabad6d8d66168eddca`.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| **AC01:** With the shipped set and nothing stored, for every row of the inventory above the core offers the same names in the same order under the same facts, and each expansion, subject and entry point equals what is sent today. | Not run | No producing revision |
| **AC02:** The scan reads `{name}` and `{{`, leaves every other brace alone, and refuses an unknown name and a name of another place with its position. A value that contains `{file}` is inserted as written. A value with a line break arrives as one line. An expansion over 16 KiB makes the macro not offered, with that reason. | Not run | No producing revision |
| **AC03:** For each fact, "is" and "is not" each hold exactly for their value and neither holds when the fact is unknown. A subject or variable's need withholds the macro without a condition row. A fifth condition, a ninth pattern and an unterminated bracket are refused. | Not run | No producing revision |
| **AC04:** With seven macros matching, the first five in order are offered as chips and two as More; equal inputs give equal offers and reasons. The tests run without `QApplication` in the `core-only` preset, and both architecture-boundary tests pass. | Not run | No producing revision |
| **AC05:** On the fixture pull request, the Agent tab, a conversation and each kind of tour chat show exactly today's chips, and a click on each built-in sends a request equal, byte for byte, to today's. Neither widget nor `tour_chat_prompts` builds a prompt's text any more. | Not run | No producing revision |
| **AC06:** For a macro whose name differs from its template, a click sends the expansion and not the name; Shift-click puts the expansion in the field with the caret at its end, sending nothing; typed words survive as before; a macro with `{cursor}` is put in the field by a plain click, the caret at the mark. | Not run | No producing revision |
| **AC07:** A chip's tooltip and accessible description hold the expansion and how to send or edit it. The turn shows the sent text under the macro's name, folded past six lines, and shows the same after a restart. | Not run | No producing revision |
| **AC08:** With more than five matching, five chips and More prompts (n)… show, and a menu item sends or drafts as a chip does. A refresh that changes no name leaves keyboard focus on its chip. | Not run | No producing revision |
| **AC09:** On a direct send, a title containing `/some-skill` brings in no skill, and an expansion beginning `/model` changes no model and is sent as text. | Not run | No producing revision |
| **AC10:** A macro drafted with Shift, edited and sent keeps its subject and kind and shows its chip in the field; with the chip removed the same text is sent as a typed question. | Not run | No producing revision |
| **AC11:** Settings › Agents & context lists every macro under its place with its name, its sentence and its state in words. Turning one off removes its chip from an open window without a restart; a new order, made by drag and by keyboard, reaches the chips; both survive a restart. | Not run | No producing revision |
| **AC12:** A profile whose store lacks the tables shows the shipped set. An unreadable store leaves the shipped chips working and Settings says edits are unavailable. A write from an older revision is refused and changes nothing. A row from a newer version is kept, named as such and never offered. | Not run | No producing revision |
| **AC13:** The suggestions toggle still hides every chip. Remove every agent and set up again changes no macro. Restore built-ins… returns the built-ins to shipped text, order and On after confirmation and keeps the reviewer's own. | Not run | No producing revision |
| **AC14:** Add prompt… makes a macro in a chosen place. Name, template, About, Ask as and conditions can be edited and saved. Duplicate copies one. Delete removes one of the reviewer's own and Undo returns it. A built-in cannot be deleted or moved and offers Reset to built-in once changed. Cancel with changes asks first. | Not run | No producing revision |
| **AC15:** Insert and a typed `{` offer exactly the place's variables. Each case of the message table produces its sentence, and Save is unavailable exactly where the table says. The byte count is shown. | Not run | No producing revision |
| **AC16:** The preview shows the chip's text and the expansion against the pull request Settings was opened from, and against sample values when there is none, changing as the reviewer types. Shows now says yes, or names the first unmet condition. | Not run | No producing revision |
| **AC17:** A macro with a path condition of `*.cpp` is a chip while a `.cpp` file is open and is absent for a `.md` file, through the shipped composition; its roster sentence states the condition. | Not run | No producing revision |
| **AC18:** The roster and sheet are completed by keyboard alone; a screen reader hears each row's name, place, state and On, each condition picker's label and one announcement a change; the section fits a 760 by 480 Settings window without sideways scrolling and uses no QSS. | Not run | No producing revision |
| **AC19:** On a frozen build the owner accepts the roster. | Not run | No producing revision |
| **AC20:** On the same build the owner accepts the sheet. | Not run | No producing revision |
| **AC21:** On the same build the owner accepts the chips, the tooltip and the sent turn. | Not run | No producing revision |
| **AC22:** A chip's menu opens by mouse and by the Menu key. Edit this prompt… opens Settings on that macro's sheet. Turn off removes the chip and Undo returns it. | Not run | No producing revision |
| **AC23:** Typing `/` and letters lists skills only, as before. Typing `/p ` and letters lists this place's matching macros. Choosing one replaces the typed `/p …` with the expansion and sends nothing. `/model`, `/effort`, a typed `/skill-name` and an unchosen `/p …` behave as before. | Not run | No producing revision |
| **AC24:** Export offers each of the reviewer's own prompts with its exact text under a new or existing pack, writes version 2 with the pack's ID, label and version and each prompt's UUID, and writes version 1 when none is chosen. Import matches by ID, never by name: an unmatched prompt is Added; a matched one not edited since the last import or export of that pack is updated in place with its On and placements kept; an edited one is a conflict, Replace or Keep mine; one the pack dropped is offered as Remove or Keep; an older pack version is flagged; prompts not from the pack are never changed. Duplicate mints a new UUID and drops the pack link. Import applies all chosen or none, refuses after a concurrent edit and launches nothing. A version 1 file imports as before. An item naming an unknown place, About, fact or variable, or with an ID that is not a UUID, is listed as unsupported and not applied. | Not run | No producing revision |
| **AC26:** A pack exported from one profile and imported into a clean profile adds every prompt; re-importing a later version of that pack updates them in place and leaves the profile's other prompts unchanged. | Not run | No producing revision |
| **AC25:** `docs/ux/ai-experience.md` and the feature allocation in `docs/architecture/system-architecture.md` describe suggested prompts as macros, the amendments above are applied to their RFCs, and the documentation gate passes. | Not run | No producing revision |

## Outcome and work

AC25 and closeout of AC01-AC24: update `ai-experience.md` and the system
architecture, apply the amendments to RFC-BPR-0102, RFC-BPR-0107 and
BPR-1934, run the documentation gate.

## Hygiene audit 2026-11-15

Verdict: **STILL-VALID**. Closeout. Needs refreshing: AC14-AC21 should be remapped onto the Prompts section the owner accepted (BPR-2179, BPR-2185) rather than the RFC's original sheet.
This is a read-only tracker audit: it changes no status, folder or metadata.
