#include "bpr/core/suggestion_text_editing.hpp"

#include <algorithm>
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
		State.Lines    = split_text_lines(aText.substr(0, std::min(aText.size(), maximum_suggestion_editor_bytes)));
		State.TabWidth = aTabWidth == 0U ? default_suggestion_tab_width : aTabWidth;
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
		const auto before = State;
		const auto tab    = State.TabWidth;
		const auto lines  = [this] { return static_cast<std::uint64_t>(State.Lines.size()); };
		const auto width  = [this](const std::uint64_t aLine) { return line_width(aLine); };
		const auto snap   = [&](const text_point& aPoint) {
			const auto  line                    = std::min(aPoint.Line, lines() - 1U);
			const auto& content                 = State.Lines[line].Content;
			const auto [offset, virtualColumns] = offset_of(content, aPoint.Column, tab);
			return text_point{ line, column_of(content, offset, tab) + virtualColumns };
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
				if (virtualColumns > 0U) {
					return { aHead.Line, aHead.Column - 1U };
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
				// Virtual space: Right never wraps onto the next line.
				if (offset < content.size()) {
					return { aHead.Line, column_of(content, offset + scalar_length(content, offset), tab) };
				}
				return { aHead.Line, aHead.Column + 1U };
			case text_motion::up:             return aHead.Line > 0U ? text_point{ aHead.Line - 1U, aHead.Column } : text_point{ 0U, 0U };
			case text_motion::down:           return aHead.Line + 1U < lines() ? text_point{ aHead.Line + 1U, aHead.Column } : text_point{ aHead.Line, width(aHead.Line) };
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
		const auto insertEverywhere = [&](const std::vector<std::string>& aTexts) {
			auto edits = std::vector<std::optional<flat_edit>>{ };
			for (std::size_t index = 0; index < State.Carets.size(); ++index) {
				const auto& caret = State.Carets[index];
				const auto  start = std::min(caret.Anchor, caret.Head);
				const auto  end   = std::max(caret.Anchor, caret.Head);
				const auto  from  = flat(start);
				const auto  to    = flat(end);
				auto        text  = std::string(from.second, ' ') + (aTexts.size() == 1U ? aTexts.front() : aTexts[index]);
				edits.emplace_back(flat_edit{ .Start = from.first, .End = std::max(from.first, to.first), .Insert = text, .Caret = text.size() });
			}
			return replace(std::move(edits));
		};

		auto changed = false;
		std::visit(
			[&](const auto& aValue) {
				using value_type = std::decay_t<decltype(aValue)>;
				if constexpr (std::is_same_v<value_type, move_carets>) {
					Typing          = false;
					State.BoxAnchor = std::nullopt;
					auto carets     = State.Carets;
					for (auto& caret : carets) {
						if (!aValue.Extend && !caret.empty() && (aValue.Motion == text_motion::left || aValue.Motion == text_motion::right)) {
							const auto edge = aValue.Motion == text_motion::left ? std::min(caret.Anchor, caret.Head) : std::max(caret.Anchor, caret.Head);
							caret           = { edge, edge };
							continue;
						}
						const auto head = moved(caret.Head, aValue.Motion);
						caret           = { aValue.Extend ? caret.Anchor : head, head };
					}
					const auto primary = carets[State.Primary].Head;
					normalize(std::move(carets), primary);
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
					const auto at   = snap(aValue.At);
					State.Carets    = { { aValue.Extend ? State.Carets[State.Primary].Anchor : at, at } };
					State.Primary   = 0;
				} else if constexpr (std::is_same_v<value_type, add_caret>) {
					Typing          = false;
					State.BoxAnchor = std::nullopt;
					if (State.Carets.size() >= maximum_suggestion_editor_carets) {
						return;
					}
					auto       carets = State.Carets;
					const auto at     = snap(aValue.At);
					carets.push_back({ at, at });
					normalize(std::move(carets), at);
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
					if (!insertEverywhere(texts)) {
						if (pushed) {
							Undo.pop_back();
						}
						Typing = typing;
						State  = before;
					}
				} else if constexpr (std::is_same_v<value_type, insert_line_break>) {
					State.BoxAnchor = std::nullopt;
					remember(false);
					auto texts = std::vector<std::string>{ };
					for (const auto& caret : State.Carets) {
						texts.emplace_back(ending_bytes(endingAt(std::min(caret.Anchor, caret.Head).Line)));
					}
					if (!insertEverywhere(texts)) {
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
					if (!insertEverywhere(texts)) {
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
		changed = State != before;
		return changed;
	}
}
