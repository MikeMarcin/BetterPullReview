#include "bpr/core/suggestion_text_editing.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <utility>

namespace bpr::core {
	namespace {
		auto ending_bytes(const text_line_ending aEnding) -> std::string_view
		{
			switch (aEnding) {
			case text_line_ending::lf:   return "\n";
			case text_line_ending::crlf: return "\r\n";
			case text_line_ending::cr:   return "\r";
			case text_line_ending::none: return "";
			}
			return "";
		}

		/// Bytes in the UTF-8 scalar starting at `aIndex`; a malformed byte is one.
		auto scalar_length(const std::string_view aText, const std::size_t aIndex) -> std::size_t
		{
			const auto lead   = static_cast<unsigned char>(aText[aIndex]);
			const auto length = lead < 0x80U ? 1U : lead >= 0xF0U ? 4U
				: lead >= 0xE0U                                   ? 3U
				: lead >= 0xC0U                                   ? 2U
																  : 1U;
			if (aIndex + length > aText.size()) {
				return 1U;
			}
			for (std::size_t offset = 1; offset < length; ++offset) {
				if ((static_cast<unsigned char>(aText[aIndex + offset]) & 0xC0U) != 0x80U) {
					return 1U;
				}
			}
			return length;
		}

		/// Columns the scalar at `aIndex` occupies when it starts at `aColumn`.
		auto advance(const std::string_view aText, const std::size_t aIndex, const std::uint64_t aColumn, const std::uint32_t aTab) -> std::uint64_t
		{
			return aText[aIndex] == '\t' ? aTab - (aColumn % aTab) : 1U;
		}

		auto width_of(const std::string_view aText, const std::uint32_t aTab) -> std::uint64_t
		{
			auto column = std::uint64_t{ 0 };
			for (std::size_t index = 0; index < aText.size(); index += scalar_length(aText, index)) {
				column += advance(aText, index, column, aTab);
			}
			return column;
		}

		/// The byte offset of the scalar boundary at or before a visual column,
		/// and the virtual columns beyond the end of the line.
		auto offset_of(const std::string_view aText, const std::uint64_t aColumn, const std::uint32_t aTab) -> std::pair<std::size_t, std::uint64_t>
		{
			auto column = std::uint64_t{ 0 };
			auto index  = std::size_t{ 0 };
			while (index < aText.size()) {
				const auto next = column + advance(aText, index, column, aTab);
				if (next > aColumn) {
					return { index, 0U };
				}
				column = next;
				index += scalar_length(aText, index);
			}
			return { index, aColumn - std::min(aColumn, column) };
		}

		auto column_of(const std::string_view aText, const std::size_t aOffset, const std::uint32_t aTab) -> std::uint64_t
		{
			auto column = std::uint64_t{ 0 };
			for (std::size_t index = 0; index < aOffset && index < aText.size(); index += scalar_length(aText, index)) {
				column += advance(aText, index, column, aTab);
			}
			return column;
		}

		enum class character_class : std::uint8_t {
			space,
			word,
			punctuation,
		};
		auto class_of(const char aByte) -> character_class
		{
			const auto value = static_cast<unsigned char>(aByte);
			if (aByte == ' ' || aByte == '\t') {
				return character_class::space;
			}
			if (value >= 0x80U || (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z') || (aByte >= '0' && aByte <= '9') || aByte == '_') {
				return character_class::word;
			}
			return character_class::punctuation;
		}

		/// One replacement in the joined text, and where its caret lands.
		struct flat_edit
		{
			std::size_t   Start = 0;
			std::size_t   End   = 0;
			std::string   Insert;
			std::size_t   Caret        = 0;
			std::uint64_t CaretVirtual = 0;
		};

		constexpr auto blanks = std::string_view{ " \t" };

		/// The leading blanks of a line.
		auto leading_blanks(const std::string_view aText) -> std::string_view
		{
			return aText.substr(0, std::min(aText.find_first_not_of(blanks), aText.size()));
		}

		/// The closer of an opening bracket, or nothing.
		auto closer_of(const char aOpener) -> char
		{
			switch (aOpener) {
			case '{': return '}';
			case '[': return ']';
			case '(': return ')';
			default:  return '\0';
			}
		}

		auto lowercase(std::string_view aText) -> std::string
		{
			auto lowered = std::string{ aText };
			std::ranges::transform(lowered, lowered.begin(), [](const char aByte) { return aByte >= 'A' && aByte <= 'Z' ? static_cast<char>(aByte - 'A' + 'a') : aByte; });
			return lowered;
		}

		/// The most common step by which a line's leading spaces exceed the
		/// previous non-blank line's, or zero when no line steps in.
		auto common_space_step(const std::vector<text_line>& aLines) -> std::size_t
		{
			auto steps    = std::map<std::size_t, std::size_t>{ };
			auto previous = std::size_t{ 0 };
			for (const auto& line : aLines) {
				const auto lead = leading_blanks(line.Content);
				if (lead.size() == line.Content.size() || lead.find('\t') != std::string_view::npos) {
					continue;
				}
				if (lead.size() > previous) {
					++steps[lead.size() - previous];
				}
				previous = lead.size();
			}
			const auto most = std::ranges::max_element(steps, [](const auto& aLeft, const auto& aRight) { return aLeft.second < aRight.second; });
			return most == steps.end() ? 0U : most->first;
		}
	}

	auto indentation_for(const std::string_view aPath, const std::string_view aText) -> text_indentation
	{
		const auto     dot       = aPath.find_last_of('.');
		const auto     slash     = aPath.find_last_of("/\\");
		const auto     extension = dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash) ? std::string{ } : lowercase(aPath.substr(dot + 1U));
		constexpr auto prose     = std::array<std::string_view, 6>{ "md", "markdown", "rst", "adoc", "txt", "text" };
		constexpr auto colon     = std::array<std::string_view, 5>{ "py", "pyi", "pyw", "yaml", "yml" };
		auto           rule      = text_indentation{ };
		rule.Brackets            = std::ranges::find(prose, extension) == prose.end();
		rule.Colon               = std::ranges::find(colon, extension) != colon.end();

		const auto lines  = split_text_lines(aText);
		const auto tabbed = std::ranges::count_if(lines, [](const text_line& aLine) { return aLine.Content.starts_with('\t'); });
		const auto spaced = std::ranges::count_if(lines, [](const text_line& aLine) { return aLine.Content.starts_with(' ') && leading_blanks(aLine.Content).size() < aLine.Content.size(); });
		if (tabbed > 0 && tabbed >= spaced) {
			return rule;
		}
		if (const auto step = common_space_step(lines); step > 0U) {
			rule.Unit = std::string(std::min<std::size_t>(step, 8U), ' ');
		} else if (rule.Colon) {
			rule.Unit = std::string(extension.starts_with('y') ? 2U : 4U, ' ');
		}
		return rule;
	}

	auto changes_text(const suggestion_edit& aEdit) noexcept -> bool
	{
		return std::holds_alternative<insert_text>(aEdit) || std::holds_alternative<insert_line_break>(aEdit) || std::holds_alternative<paste_text>(aEdit)
			|| std::holds_alternative<delete_backward>(aEdit) || std::holds_alternative<delete_forward>(aEdit) || std::holds_alternative<undo_edit>(aEdit)
			|| std::holds_alternative<redo_edit>(aEdit);
	}

	auto split_text_lines(const std::string_view aText) -> std::vector<text_line>
	{
		auto lines   = std::vector<text_line>{ };
		auto current = text_line{ };
		for (std::size_t index = 0; index < aText.size(); ++index) {
			const auto byte = aText[index];
			if (byte == '\n' || byte == '\r') {
				current.Ending = byte == '\n' ? text_line_ending::lf : text_line_ending::cr;
				if (byte == '\r' && index + 1U < aText.size() && aText[index + 1U] == '\n') {
					current.Ending = text_line_ending::crlf;
					++index;
				}
				lines.push_back(std::exchange(current, text_line{ }));
			} else {
				current.Content.push_back(byte);
			}
		}
		lines.push_back(std::move(current));
		return lines;
	}

	auto join_text_lines(const std::vector<text_line>& aLines) -> std::string
	{
		auto text = std::string{ };
		for (const auto& line : aLines) {
			text += line.Content;
			text += ending_bytes(line.Ending);
		}
		return text;
	}

	suggestion_text_editor::suggestion_text_editor(const std::string_view aText, const std::uint32_t aTabWidth)
	{
		State.Lines    = split_text_lines(aText);
		State.TabWidth = aTabWidth == 0U ? default_suggestion_tab_width : aTabWidth;
	}

	void suggestion_text_editor::set_indentation(text_indentation aIndentation)
	{
		Indentation = std::move(aIndentation);
	}

	auto suggestion_text_editor::indentation() const noexcept -> const text_indentation&
	{
		return Indentation;
	}

	auto suggestion_text_editor::state() const noexcept -> const suggestion_text_state&
	{
		return State;
	}

	auto suggestion_text_editor::text() const -> std::string
	{
		return join_text_lines(State.Lines);
	}

	auto suggestion_text_editor::can_undo() const noexcept -> bool
	{
		return !Undo.empty();
	}

	auto suggestion_text_editor::can_redo() const noexcept -> bool
	{
		return !Redo.empty();
	}

	auto suggestion_text_editor::line_width(const std::uint64_t aLine) const -> std::uint64_t
	{
		return aLine < State.Lines.size() ? width_of(State.Lines[aLine].Content, State.TabWidth) : 0U;
	}

	auto suggestion_text_editor::locate(const text_point& aPoint) const -> std::pair<std::size_t, std::uint64_t>
	{
		const auto line = std::min<std::uint64_t>(aPoint.Line, State.Lines.size() - 1U);
		return offset_of(State.Lines[line].Content, aPoint.Column, State.TabWidth);
	}

	auto suggestion_text_editor::point_of(std::size_t aOffset) const -> text_point
	{
		for (std::uint64_t line = 0; line < State.Lines.size(); ++line) {
			const auto& content = State.Lines[line].Content;
			if (aOffset <= content.size() || line + 1U == State.Lines.size()) {
				return { line, column_of(content, std::min(aOffset, content.size()), State.TabWidth) };
			}
			const auto total = content.size() + ending_bytes(State.Lines[line].Ending).size();
			if (aOffset < total) {
				return { line, width_of(content, State.TabWidth) };
			}
			aOffset -= total;
		}
		return { };
	}

	auto suggestion_text_editor::byte_offset_of(const text_point& aPoint) const -> std::size_t
	{
		const auto line   = std::min<std::uint64_t>(aPoint.Line, State.Lines.size() - 1U);
		auto       offset = std::size_t{ 0 };
		for (std::uint64_t index = 0; index < line; ++index) {
			offset += State.Lines[index].Content.size() + ending_bytes(State.Lines[index].Ending).size();
		}
		return offset + offset_of(State.Lines[line].Content, aPoint.Column, State.TabWidth).first;
	}

	auto suggestion_text_editor::copy() const -> copied_text
	{
		auto       result = copied_text{ };
		auto       pieces = std::vector<std::string>{ };
		auto       ending = std::string_view{ "\n" };
		const auto found  = std::ranges::find_if(State.Lines, [](const text_line& aLine) { return aLine.Ending != text_line_ending::none; });
		if (found != State.Lines.end()) {
			ending = ending_bytes(found->Ending);
		}
		const auto selected = std::ranges::count_if(State.Carets, [](const text_caret& aCaret) { return !aCaret.empty(); });
		result.Box          = selected > 1 && std::ranges::all_of(State.Carets, [](const text_caret& aCaret) { return aCaret.Anchor.Line == aCaret.Head.Line; });
		for (const auto& caret : State.Carets) {
			if (caret.empty()) {
				continue;
			}
			const auto start = std::min(caret.Anchor, caret.Head);
			const auto end   = std::max(caret.Anchor, caret.Head);
			auto       piece = std::string{ };
			for (auto line = start.Line; line <= end.Line; ++line) {
				const auto& content = State.Lines[line].Content;
				const auto  from    = line == start.Line ? offset_of(content, start.Column, State.TabWidth) : std::pair<std::size_t, std::uint64_t>{ 0U, 0U };
				const auto  to      = line == end.Line ? offset_of(content, end.Column, State.TabWidth) : std::pair{ content.size(), std::uint64_t{ 0 } };
				piece += content.substr(from.first, to.first - from.first);
				if (result.Box) {
					// A box keeps its width: virtual space copies as spaces.
					piece.append(to.second - std::min(to.second, from.second), ' ');
				}
				if (line != end.Line) {
					piece += ending_bytes(State.Lines[line].Ending);
				}
			}
			pieces.push_back(std::move(piece));
		}
		for (std::size_t index = 0; index < pieces.size(); ++index) {
			result.Text += pieces[index];
			if (index + 1U < pieces.size()) {
				result.Text += ending;
			}
		}
		return result;
	}

	void suggestion_text_editor::remember(const bool aCoalesce)
	{
		if (!(aCoalesce && Typing)) {
			Undo.push_back(State);
			// A whole file per step: the oldest steps go first, by count and by size.
			auto bytes = std::size_t{ 0 };
			for (const auto& line : State.Lines) {
				bytes += line.Content.size() + 2U;
			}
			while (Undo.size() > maximum_suggestion_undo_steps || (Undo.size() > 1U && Undo.size() * bytes > maximum_suggestion_undo_bytes)) {
				Undo.erase(Undo.begin());
			}
		}
		Redo.clear();
		Typing = aCoalesce;
	}

	auto suggestion_text_editor::apply(const suggestion_edit& aEdit) -> bool
	{
		// A gesture that only moves carets compares the carets, so reading a
		// large file costs nothing per keystroke; an edit keeps the whole
		// state to fall back to.
		// Up and Down remember the column they aim for; any other gesture forgets it.
		auto       goals   = std::exchange(State.Goals, { });
		const auto editing = changes_text(aEdit);
		const auto before  = editing ? State : suggestion_text_state{ .Lines = { }, .Carets = State.Carets, .Primary = State.Primary, .TabWidth = State.TabWidth, .BoxAnchor = State.BoxAnchor };
		const auto tab     = State.TabWidth;
		const auto lines   = [this] { return static_cast<std::uint64_t>(State.Lines.size()); };
		const auto width   = [this](const std::uint64_t aLine) { return line_width(aLine); };
		// A point on its line's text: past the end is the end, inside a tab
		// is the tab's start. Only the box gesture reaches virtual space.
		const auto clamped = [&](const text_point& aPoint) {
			const auto  line    = std::min(aPoint.Line, lines() - 1U);
			const auto& content = State.Lines[line].Content;
			return text_point{ line, column_of(content, offset_of(content, aPoint.Column, tab).first, tab) };
		};
		// The word, run of blanks or punctuation mark at a point; past the
		// end of a line, the last thing on it.
		const auto wordAt = [&](const text_point& aPoint) {
			const auto  at      = clamped(aPoint);
			const auto& content = State.Lines[at.Line].Content;
			auto        first   = std::min(offset_of(content, at.Column, tab).first, content.size());
			if (first == content.size() && first > 0U) {
				first = 0U;
				for (std::size_t index = 0; index < content.size(); index += scalar_length(content, index)) {
					first = index;
				}
			}
			auto last = first;
			if (first < content.size()) {
				const auto kind = class_of(content[first]);
				last            = first + scalar_length(content, first);
				if (kind != character_class::punctuation) {
					while (first > 0U && class_of(content[first - 1U]) == kind) {
						--first;
					}
					while (last < content.size() && class_of(content[last]) == kind) {
						++last;
					}
				}
			}
			return std::pair{ text_point{ at.Line, column_of(content, first, tab) }, text_point{ at.Line, column_of(content, last, tab) } };
		};
		// A line with its line break: from its start to the next line's.
		const auto lineAt = [&](const std::uint64_t aLine) {
			const auto line = std::min(aLine, lines() - 1U);
			return std::pair{ text_point{ line, 0U }, line + 1U < lines() ? text_point{ line + 1U, 0U } : text_point{ line, width(line) } };
		};
		// The ending a new line break takes at a line: its own, or the text's first.
		const auto endingAt = [&](const std::uint64_t aLine) {
			if (State.Lines[aLine].Ending != text_line_ending::none) {
				return State.Lines[aLine].Ending;
			}
			const auto found = std::ranges::find_if(State.Lines, [](const text_line& aLine2) { return aLine2.Ending != text_line_ending::none; });
			return found != State.Lines.end() ? found->Ending : text_line_ending::lf;
		};
		const auto normalizeBreaks = [&](const std::string_view aText, const std::uint64_t aLine) {
			const auto parts = split_text_lines(aText);
			auto       text  = std::string{ };
			for (std::size_t index = 0; index < parts.size(); ++index) {
				text += parts[index].Content;
				if (index + 1U < parts.size()) {
					text += ending_bytes(endingAt(aLine));
				}
			}
			return text;
		};
		const auto lineStart = [&](const std::uint64_t aLine) {
			auto offset = std::size_t{ 0 };
			for (std::uint64_t line = 0; line < aLine; ++line) {
				offset += State.Lines[line].Content.size() + ending_bytes(State.Lines[line].Ending).size();
			}
			return offset;
		};
		const auto flat = [&](const text_point& aPoint) {
			const auto [offset, virtualColumns] = locate(aPoint);
			return std::pair{ lineStart(std::min(aPoint.Line, lines() - 1U)) + offset, virtualColumns };
		};
		const auto pointOf = [&](const std::vector<text_line>& aLines, std::size_t aOffset, const std::uint64_t aVirtual) {
			for (std::uint64_t line = 0; line < aLines.size(); ++line) {
				const auto length = aLines[line].Content.size();
				const auto total  = length + ending_bytes(aLines[line].Ending).size();
				if (aOffset <= length || line + 1U == aLines.size()) {
					const auto offset = std::min(aOffset, length);
					return text_point{ line, column_of(aLines[line].Content, offset, tab) + (offset == length ? aVirtual : 0U) };
				}
				aOffset -= std::min(aOffset, total);
			}
			return text_point{ };
		};
		// Sorts and merges overlapping carets, keeping the primary one's head.
		const auto normalize = [&](std::vector<text_caret> aCarets, const text_point aPrimaryHead) {
			std::ranges::sort(aCarets, [](const text_caret& aLeft, const text_caret& aRight) {
				return std::min(aLeft.Anchor, aLeft.Head) < std::min(aRight.Anchor, aRight.Head);
			});
			auto merged = std::vector<text_caret>{ };
			for (const auto& caret : aCarets) {
				if (!merged.empty()) {
					auto&      last      = merged.back();
					const auto lastEnd   = std::max(last.Anchor, last.Head);
					const auto thisStart = std::min(caret.Anchor, caret.Head);
					if (thisStart < lastEnd || (thisStart == lastEnd && (caret.empty() || last.empty()))) {
						const auto start = std::min(std::min(last.Anchor, last.Head), thisStart);
						const auto end   = std::max(lastEnd, std::max(caret.Anchor, caret.Head));
						last             = last.Head < last.Anchor ? text_caret{ end, start } : text_caret{ start, end };
						continue;
					}
				}
				merged.push_back(caret);
			}
			State.Carets  = std::move(merged);
			State.Primary = 0;
			for (std::size_t index = 0; index < State.Carets.size(); ++index) {
				const auto& caret = State.Carets[index];
				if (std::min(caret.Anchor, caret.Head) <= aPrimaryHead && aPrimaryHead <= std::max(caret.Anchor, caret.Head)) {
					State.Primary = index;
				}
			}
		};
		// Applies replacements to the joined text; untouched carets keep their place.
		const auto replace = [&](std::vector<std::optional<flat_edit>> aEdits) {
			auto text  = join_text_lines(State.Lines);
			auto order = std::vector<std::size_t>{ };
			for (std::size_t index = 0; index < aEdits.size(); ++index) {
				if (aEdits[index]) {
					order.push_back(index);
				}
			}
			std::ranges::sort(order, [&](const std::size_t aLeft, const std::size_t aRight) { return aEdits[aLeft]->Start < aEdits[aRight]->Start; });
			auto result   = std::string{ };
			auto cursor   = std::size_t{ 0 };
			auto landed   = std::vector<std::pair<std::size_t, std::uint64_t>>(aEdits.size());
			auto accepted = std::vector<bool>(aEdits.size(), false);
			for (const auto index : order) {
				auto& edit = *aEdits[index];
				// Two carets reaching for the same bytes: the later one joins the earlier edit.
				edit.Start = std::max(edit.Start, cursor);
				edit.End   = std::max(edit.End, edit.Start);
				result.append(text, cursor, edit.Start - cursor);
				const auto insertedAt = result.size();
				result += edit.Insert;
				landed[index]   = { insertedAt + std::min(edit.Caret, edit.Insert.size()), edit.CaretVirtual };
				accepted[index] = true;
				cursor          = edit.End;
			}
			result.append(text, cursor, std::string::npos);
			if (result.size() > maximum_suggestion_editor_bytes) {
				return false;
			}
			// Carets without an edit move with the text before them.
			auto newLines = split_text_lines(result);
			auto carets   = std::vector<text_caret>{ };
			for (std::size_t index = 0; index < aEdits.size(); ++index) {
				auto position = std::pair<std::size_t, std::uint64_t>{ };
				if (accepted[index]) {
					position = landed[index];
				} else {
					const auto [offset, virtualColumns] = flat(State.Carets[index].Head);
					auto shift                          = std::ptrdiff_t{ 0 };
					for (const auto other : order) {
						if (aEdits[other]->End <= offset) {
							shift += static_cast<std::ptrdiff_t>(aEdits[other]->Insert.size()) - static_cast<std::ptrdiff_t>(aEdits[other]->End - aEdits[other]->Start);
						}
					}
					position = { static_cast<std::size_t>(static_cast<std::ptrdiff_t>(offset) + shift), virtualColumns };
				}
				const auto point = pointOf(newLines, position.first, position.second);
				carets.push_back({ point, point });
			}
			State.Lines             = std::move(newLines);
			const auto primaryPoint = carets[std::min(State.Primary, carets.size() - 1U)].Head;
			normalize(std::move(carets), primaryPoint);
			return true;
		};
		const auto moved = [&](const text_point& aHead, const text_motion aMotion) -> text_point {
			const auto& content                 = State.Lines[std::min(aHead.Line, lines() - 1U)].Content;
			const auto [offset, virtualColumns] = offset_of(content, aHead.Column, tab);
			switch (aMotion) {
			case text_motion::left:
				// A caret a box left in virtual space comes back to the text.
				if (virtualColumns > 0U) {
					return { aHead.Line, width(aHead.Line) };
				}
				if (offset > 0U) {
					auto previous = std::size_t{ 0 };
					for (std::size_t index = 0; index < offset; index += scalar_length(content, index)) {
						previous = index;
					}
					return { aHead.Line, column_of(content, previous, tab) };
				}
				return aHead.Line > 0U ? text_point{ aHead.Line - 1U, width(aHead.Line - 1U) } : aHead;
			case text_motion::right:
				// At the end, Right goes on to the next line, as in any editor.
				if (offset < content.size()) {
					return { aHead.Line, column_of(content, offset + scalar_length(content, offset), tab) };
				}
				if (virtualColumns > 0U || aHead.Line + 1U >= lines()) {
					return { aHead.Line, width(aHead.Line) };
				}
				return { aHead.Line + 1U, 0U };
			// The column aimed for, stopping at a shorter line's end.
			case text_motion::up:             return aHead.Line > 0U ? clamped({ aHead.Line - 1U, aHead.Column }) : text_point{ 0U, 0U };
			case text_motion::down:           return aHead.Line + 1U < lines() ? clamped({ aHead.Line + 1U, aHead.Column }) : text_point{ aHead.Line, width(aHead.Line) };
			case text_motion::line_end:       return { aHead.Line, width(aHead.Line) };
			case text_motion::document_start: return { 0U, 0U };
			case text_motion::document_end:   return { lines() - 1U, width(lines() - 1U) };
			case text_motion::line_start:     {
				const auto blank = std::ranges::find_if(content, [](const char aByte) { return aByte != ' ' && aByte != '\t'; });
				const auto first = column_of(content, static_cast<std::size_t>(blank - content.begin()), tab);
				return { aHead.Line, aHead.Column == first ? 0U : first };
			}
			case text_motion::word_right: {
				if (offset >= content.size()) {
					return aHead.Line + 1U < lines() ? text_point{ aHead.Line + 1U, 0U } : aHead;
				}
				auto       index = offset;
				const auto kind  = class_of(content[index]);
				while (index < content.size() && class_of(content[index]) == kind && kind != character_class::space) {
					index += scalar_length(content, index);
				}
				while (index < content.size() && class_of(content[index]) == character_class::space) {
					++index;
				}
				return { aHead.Line, column_of(content, index, tab) };
			}
			case text_motion::word_left: {
				if (offset == 0U && virtualColumns == 0U) {
					return aHead.Line > 0U ? text_point{ aHead.Line - 1U, width(aHead.Line - 1U) } : aHead;
				}
				auto index = offset;
				while (index > 0U && class_of(content[index - 1U]) == character_class::space) {
					--index;
				}
				if (index > 0U) {
					const auto kind = class_of(content[index - 1U]);
					while (index > 0U && class_of(content[index - 1U]) == kind) {
						--index;
					}
				}
				return { aHead.Line, column_of(content, index, tab) };
			}
			}
			return aHead;
		};
		// A closer typed on a line of blanks ending in one level takes that level away.
		const auto outdented = [&](const text_caret& aCaret, const std::string_view aText) -> std::size_t {
			const auto& unit = Indentation.Unit;
			if (!Indentation.Brackets || unit.empty() || aText.size() != 1U || std::string_view{ "}])" }.find(aText.front()) == std::string_view::npos || !aCaret.empty()) {
				return 0U;
			}
			const auto& content                 = State.Lines[aCaret.Head.Line].Content;
			const auto [offset, virtualColumns] = offset_of(content, aCaret.Head.Column, tab);
			const auto prefix                   = std::string_view{ content }.substr(0, offset);
			return virtualColumns == 0U && leading_blanks(prefix).size() == prefix.size() && prefix.ends_with(unit) ? unit.size() : 0U;
		};
		const auto insertEverywhere = [&](const std::vector<std::string>& aTexts, const bool aTyped) {
			auto edits = std::vector<std::optional<flat_edit>>{ };
			for (std::size_t index = 0; index < State.Carets.size(); ++index) {
				const auto& caret  = State.Carets[index];
				const auto  start  = std::min(caret.Anchor, caret.Head);
				const auto  end    = std::max(caret.Anchor, caret.Head);
				const auto  from   = flat(start);
				const auto  to     = flat(end);
				const auto& typed  = aTexts.size() == 1U ? aTexts.front() : aTexts[index];
				const auto  remove = aTyped ? outdented(caret, typed) : 0U;
				auto        text   = std::string(from.second, ' ') + typed;
				edits.emplace_back(flat_edit{ .Start = from.first - remove, .End = std::max(from.first, to.first), .Insert = text, .Caret = text.size() });
			}
			return replace(std::move(edits));
		};
		// Enter: the line's own blanks before the caret, one more level after
		// an opener, and a bracket's closer on a line of its own.
		const auto lineBreak = [&](const text_caret& aCaret) {
			const auto  start       = std::min(aCaret.Anchor, aCaret.Head);
			const auto  end         = std::max(aCaret.Anchor, aCaret.Head);
			const auto  ending      = std::string{ ending_bytes(endingAt(start.Line)) };
			const auto  prefix      = std::string_view{ State.Lines[start.Line].Content }.substr(0, offset_of(State.Lines[start.Line].Content, start.Column, tab).first);
			const auto  lead        = std::string{ leading_blanks(prefix) };
			const auto  last        = prefix.find_last_not_of(blanks);
			const auto  opener      = last == std::string_view::npos ? '\0' : prefix[last];
			const auto  opens       = (Indentation.Brackets && closer_of(opener) != '\0') || (Indentation.Colon && opener == ':');
			const auto  indent      = opens ? lead + Indentation.Unit : lead;
			const auto& after       = State.Lines[end.Line].Content;
			const auto  afterOffset = offset_of(after, end.Column, tab).first;
			const auto  closer      = std::min(after.find_first_not_of(blanks, afterOffset), after.size());
			const auto  from        = flat(start).first;
			const auto  to          = std::max(from, flat(end).first);
			if (opens && closer < after.size() && after[closer] == closer_of(opener)) {
				return flat_edit{ .Start = from, .End = to + (closer - afterOffset), .Insert = ending + indent + ending + lead, .Caret = ending.size() + indent.size() };
			}
			return flat_edit{ .Start = from, .End = to, .Insert = ending + indent, .Caret = ending.size() + indent.size() };
		};

		auto changed = false;
		std::visit(
			[&](const auto& aValue) {
				using value_type = std::decay_t<decltype(aValue)>;
				if constexpr (std::is_same_v<value_type, move_carets>) {
					Typing              = false;
					State.BoxAnchor     = std::nullopt;
					auto       carets   = State.Carets;
					const auto vertical = aValue.Motion == text_motion::up || aValue.Motion == text_motion::down;
					// A run of Up and Down keeps aiming for the columns it started from.
					if (vertical && goals.size() != carets.size()) {
						goals.clear();
						for (const auto& caret : carets) {
							goals.push_back(caret.Head.Column);
						}
					}
					for (std::size_t index = 0; index < carets.size(); ++index) {
						auto& caret = carets[index];
						if (!aValue.Extend && !caret.empty() && (aValue.Motion == text_motion::left || aValue.Motion == text_motion::right)) {
							const auto edge = aValue.Motion == text_motion::left ? std::min(caret.Anchor, caret.Head) : std::max(caret.Anchor, caret.Head);
							caret           = { edge, edge };
							continue;
						}
						const auto head = moved(vertical ? text_point{ caret.Head.Line, goals[index] } : caret.Head, aValue.Motion);
						caret           = { aValue.Extend ? caret.Anchor : head, head };
					}
					const auto primary = carets[State.Primary].Head;
					normalize(std::move(carets), primary);
					if (vertical && State.Carets.size() == goals.size()) {
						State.Goals = std::move(goals);
					}
				} else if constexpr (std::is_same_v<value_type, extend_box>) {
					Typing            = false;
					const auto& owner = State.Carets[State.Primary];
					if (!State.BoxAnchor) {
						State.BoxAnchor = owner.Anchor;
					}
					auto corner = owner.Head;
					switch (aValue.Motion) {
					case text_motion::up:    corner.Line = corner.Line > 0U ? corner.Line - 1U : 0U; break;
					case text_motion::down:  corner.Line = std::min(corner.Line + 1U, lines() - 1U); break;
					case text_motion::left:  corner.Column = corner.Column > 0U ? corner.Column - 1U : 0U; break;
					case text_motion::right: corner.Column += 1U; break;
					default:                 corner = moved(corner, aValue.Motion); break;
					}
					const auto anchor = *State.BoxAnchor;
					const auto first  = std::min(anchor.Line, corner.Line);
					const auto last   = std::max(anchor.Line, corner.Line);
					if (last - first + 1U > maximum_suggestion_editor_carets) {
						return;
					}
					auto carets = std::vector<text_caret>{ };
					for (auto line = first; line <= last; ++line) {
						carets.push_back({ { line, anchor.Column }, { line, corner.Column } });
					}
					State.Carets  = std::move(carets);
					State.Primary = static_cast<std::size_t>(corner.Line - first);
				} else if constexpr (std::is_same_v<value_type, place_caret>) {
					Typing          = false;
					State.BoxAnchor = std::nullopt;
					const auto at   = clamped(aValue.At);
					State.Carets    = { { aValue.Extend ? State.Carets[State.Primary].Anchor : at, at } };
					State.Primary   = 0;
				} else if constexpr (std::is_same_v<value_type, add_caret>) {
					Typing          = false;
					State.BoxAnchor = std::nullopt;
					if (State.Carets.size() >= maximum_suggestion_editor_carets) {
						return;
					}
					auto       carets = State.Carets;
					const auto at     = clamped(aValue.At);
					carets.push_back({ at, at });
					normalize(std::move(carets), at);
				} else if constexpr (std::is_same_v<value_type, select_word_at>) {
					Typing                = false;
					State.BoxAnchor       = std::nullopt;
					const auto [from, to] = wordAt(aValue.At);
					State.Carets          = { { from, to } };
					State.Primary         = 0;
				} else if constexpr (std::is_same_v<value_type, select_line_at>) {
					Typing                = false;
					State.BoxAnchor       = std::nullopt;
					const auto [from, to] = lineAt(aValue.At.Line);
					State.Carets          = { { from, to } };
					State.Primary         = 0;
				} else if constexpr (std::is_same_v<value_type, drag_selection>) {
					Typing           = false;
					State.BoxAnchor  = std::nullopt;
					const auto start = clamped(std::min(aValue.Origin.Anchor, aValue.Origin.Head));
					const auto end   = clamped(std::max(aValue.Origin.Anchor, aValue.Origin.Head));
					const auto to    = clamped(aValue.To);
					const auto unit  = aValue.Unit == selection_unit::word ? wordAt(to) : aValue.Unit == selection_unit::line ? lineAt(to.Line)
																															  : std::pair{ to, to };
					// From the far side of what the press selected, toward the pointer.
					if (aValue.Unit == selection_unit::character) {
						State.Carets = { { clamped(aValue.Origin.Anchor), to } };
					} else if (unit.first < start) {
						State.Carets = { { end, unit.first } };
					} else if (end < unit.second) {
						State.Carets = { { start, unit.second } };
					} else {
						State.Carets = { { start, end } };
					}
					State.Primary = 0;
				} else if constexpr (std::is_same_v<value_type, insert_text>) {
					const auto single = split_text_lines(aValue.Text).size() == 1U && !aValue.Text.empty() && scalar_length(aValue.Text, 0) == aValue.Text.size();
					State.BoxAnchor   = std::nullopt;
					const auto typing = Typing;
					const auto pushed = !(single && Typing);
					remember(single);
					auto texts = std::vector<std::string>{ };
					for (const auto& caret : State.Carets) {
						texts.push_back(normalizeBreaks(aValue.Text, std::min(caret.Anchor, caret.Head).Line));
					}
					if (!insertEverywhere(texts, true)) {
						if (pushed) {
							Undo.pop_back();
						}
						Typing = typing;
						State  = before;
					}
				} else if constexpr (std::is_same_v<value_type, insert_line_break>) {
					State.BoxAnchor = std::nullopt;
					remember(false);
					auto edits = std::vector<std::optional<flat_edit>>{ };
					for (const auto& caret : State.Carets) {
						edits.emplace_back(lineBreak(caret));
					}
					if (!replace(std::move(edits))) {
						Undo.pop_back();
						State = before;
					}
				} else if constexpr (std::is_same_v<value_type, paste_text>) {
					State.BoxAnchor = std::nullopt;
					auto parts      = split_text_lines(aValue.Text);
					// A trailing line break ends the last line; it is not another line.
					if (parts.size() > 1U && parts.back().Content.empty() && parts.back().Ending == text_line_ending::none) {
						parts.pop_back();
					}
					remember(false);
					if (aValue.Box && State.Carets.size() == 1U && parts.size() > 1U) {
						// A box copy onto one caret: one caret per line from here down.
						const auto origin = std::min(State.Carets.front().Anchor, State.Carets.front().Head);
						while (lines() < origin.Line + parts.size()) {
							State.Lines.back().Ending = endingAt(lines() - 1U);
							State.Lines.push_back({ });
						}
						State.Carets.clear();
						for (std::size_t index = 0; index < parts.size(); ++index) {
							const auto at = text_point{ origin.Line + index, origin.Column };
							State.Carets.push_back({ at, at });
						}
						State.Primary = 0;
					}
					auto texts = std::vector<std::string>{ };
					if (State.Carets.size() > 1U && parts.size() == State.Carets.size()) {
						for (const auto& part : parts) {
							texts.push_back(part.Content);
						}
					} else {
						for (const auto& caret : State.Carets) {
							texts.push_back(normalizeBreaks(aValue.Text, std::min(caret.Anchor, caret.Head).Line));
						}
					}
					if (!insertEverywhere(texts, false)) {
						Undo.pop_back();
						State = before;
					}
				} else if constexpr (std::is_same_v<value_type, delete_backward> || std::is_same_v<value_type, delete_forward>) {
					constexpr auto backward = std::is_same_v<value_type, delete_backward>;
					State.BoxAnchor         = std::nullopt;
					auto edits              = std::vector<std::optional<flat_edit>>{ };
					auto carets             = State.Carets;
					auto any                = false;
					for (auto& caret : carets) {
						if (!caret.empty()) {
							const auto from = flat(std::min(caret.Anchor, caret.Head));
							const auto to   = flat(std::max(caret.Anchor, caret.Head));
							edits.emplace_back(flat_edit{ .Start = from.first, .End = std::max(from.first, to.first) });
							any = true;
							continue;
						}
						const auto& content                 = State.Lines[caret.Head.Line].Content;
						const auto [offset, virtualColumns] = offset_of(content, caret.Head.Column, tab);
						const auto at                       = lineStart(caret.Head.Line) + offset;
						if (backward && virtualColumns > 0U) {
							// Backspace in virtual space moves left; nothing is deleted.
							caret.Head.Column -= 1U;
							caret.Anchor = caret.Head;
							edits.emplace_back(std::nullopt);
							continue;
						}
						if (backward && offset > 0U) {
							auto previous = std::size_t{ 0 };
							for (std::size_t index = 0; index < offset; index += scalar_length(content, index)) {
								previous = index;
							}
							edits.emplace_back(flat_edit{ .Start = at - (offset - previous), .End = at });
						} else if (backward && caret.Head.Line > 0U) {
							const auto ending = ending_bytes(State.Lines[caret.Head.Line - 1U].Ending).size();
							edits.emplace_back(flat_edit{ .Start = at - ending, .End = at });
						} else if (!backward && offset < content.size()) {
							edits.emplace_back(flat_edit{ .Start = at, .End = at + scalar_length(content, offset) });
						} else if (!backward && caret.Head.Line + 1U < lines()) {
							edits.emplace_back(flat_edit{ .Start = at, .End = at + ending_bytes(State.Lines[caret.Head.Line].Ending).size() });
						} else {
							edits.emplace_back(std::nullopt);
							continue;
						}
						any = true;
					}
					if (!any) {
						Typing             = false;
						const auto primary = carets[State.Primary].Head;
						normalize(std::move(carets), primary);
						return;
					}
					remember(false);
					State.Carets = std::move(carets);
					replace(std::move(edits));
				} else if constexpr (std::is_same_v<value_type, select_all>) {
					Typing          = false;
					State.BoxAnchor = std::nullopt;
					State.Carets    = { { { 0U, 0U }, { lines() - 1U, width(lines() - 1U) } } };
					State.Primary   = 0;
				} else if constexpr (std::is_same_v<value_type, collapse_carets>) {
					Typing           = false;
					State.BoxAnchor  = std::nullopt;
					const auto owner = State.Carets[State.Primary].Head;
					State.Carets     = { { owner, owner } };
					State.Primary    = 0;
				} else if constexpr (std::is_same_v<value_type, undo_edit>) {
					Typing = false;
					if (!Undo.empty()) {
						Redo.push_back(std::move(State));
						State = std::move(Undo.back());
						Undo.pop_back();
					}
				} else {
					Typing = false;
					if (!Redo.empty()) {
						Undo.push_back(std::move(State));
						State = std::move(Redo.back());
						Redo.pop_back();
					}
				}
			},
			aEdit);
		changed = editing ? State != before : State.Carets != before.Carets || State.Primary != before.Primary || State.BoxAnchor != before.BoxAnchor;
		return changed;
	}
}
