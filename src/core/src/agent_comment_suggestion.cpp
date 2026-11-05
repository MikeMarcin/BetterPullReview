#include "bpr/core/agent_comment_suggestion.hpp"

#include "bpr/core/feedback_draft_edit.hpp"
#include "bpr/core/feedback_header_edit.hpp"

#include <algorithm>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto horizontal_space(const char aByte) noexcept -> bool { return aByte == ' ' || aByte == '\t'; }
		[[nodiscard]] auto white_space(const char aByte) noexcept -> bool { return horizontal_space(aByte) || aByte == '\r' || aByte == '\n'; }
		[[nodiscard]] auto ascii_letter(const char aByte) noexcept -> bool { return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z'); }
		[[nodiscard]] auto ascii_lower(const char aByte) noexcept -> char { return aByte >= 'A' && aByte <= 'Z' ? static_cast<char>(aByte - 'A' + 'a') : aByte; }

		/// A standard decoration, from any case and with `-`, `_` or spaces
		/// between its words; otherwise none.
		[[nodiscard]] auto standard_decoration(const std::string_view aToken) -> std::optional<feedback_decoration_kind>
		{
			auto folded           = std::string{ };
			auto pendingSeparator = false;
			for (const char byte : aToken) {
				if (byte == '-' || byte == '_' || horizontal_space(byte)) {
					pendingSeparator = true;
					continue;
				}
				if (pendingSeparator && !folded.empty()) {
					folded.push_back('-');
				}
				pendingSeparator = false;
				folded.push_back(ascii_lower(byte));
			}
			if (folded == "blocking") {
				return feedback_decoration_kind::blocking;
			}
			if (folded == "non-blocking") {
				return feedback_decoration_kind::non_blocking;
			}
			if (folded == "if-minor") {
				return feedback_decoration_kind::if_minor;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto canonical_token(const feedback_decoration_kind aKind) noexcept -> std::string_view
		{
			switch (aKind) {
			case feedback_decoration_kind::blocking:
				return "blocking";
			case feedback_decoration_kind::non_blocking:
				return "non-blocking";
			case feedback_decoration_kind::if_minor:
				return "if-minor";
			case feedback_decoration_kind::custom:
				break;
			}
			return { };
		}

		[[nodiscard]] auto trimmed(std::string_view aText) noexcept -> std::string_view
		{
			while (!aText.empty() && horizontal_space(aText.front())) {
				aText.remove_prefix(1U);
			}
			while (!aText.empty() && horizontal_space(aText.back())) {
				aText.remove_suffix(1U);
			}
			return aText;
		}

		/// What the tolerant header scan found on the first line.
		struct scanned_header
		{
			/// The line has a header's shape: a word, optional decorations
			/// and a colon, with any emphasis closed.
			bool Shaped = false;
			/// The decoration list was broken (empty, nested, unclosed or
			/// holding a token the editor cannot write).
			bool                          Broken = false;
			std::string_view              Label;
			std::vector<std::string_view> Decorations;
			/// Offset into the scanned text where the subject begins.
			std::size_t SubjectBegin = 0U;
		};

		[[nodiscard]] auto scan_header(const std::string_view aText) -> scanned_header
		{
			auto result = scanned_header{ };
			// Only the first line, within the prefix bound, can be a header.
			const auto lineEnd = std::min(aText.find('\n'), std::min(aText.size(), maximum_feedback_prefix_bytes));
			const auto line    = aText.substr(0U, lineEnd);
			auto       at      = std::size_t{ 0U };
			auto       marker  = std::string_view{ };
			for (const std::string_view candidate : { std::string_view{ "**" }, std::string_view{ "__" }, std::string_view{ "*" }, std::string_view{ "_" } }) {
				if (line.starts_with(candidate)) {
					marker = candidate;
					at     = candidate.size();
					break;
				}
			}
			auto       closed = marker.empty();
			const auto close  = [&] {
				if (!closed && line.substr(at).starts_with(marker)) {
					closed = true;
					at += marker.size();
				}
			};
			const auto skipSpace = [&] {
				while (at < line.size() && horizontal_space(line[at])) {
					++at;
				}
			};
			const auto labelBegin = at;
			while (at < line.size() && ascii_letter(line[at]) && at - labelBegin <= maximum_feedback_label_bytes) {
				++at;
			}
			if (at == labelBegin || at - labelBegin > maximum_feedback_label_bytes) {
				return result;
			}
			result.Label = line.substr(labelBegin, at - labelBegin);
			close();
			skipSpace();
			if (at < line.size() && line[at] == '(') {
				const auto listEnd = line.find(')', at);
				const auto nested  = line.find('(', at + 1U);
				if (listEnd == std::string_view::npos || nested < listEnd) {
					result.Broken = true;
					return result;
				}
				auto list = line.substr(at + 1U, listEnd - at - 1U);
				while (true) {
					const auto comma = list.find(',');
					const auto token = trimmed(list.substr(0U, comma));
					if (token.empty() || result.Decorations.size() == maximum_feedback_decorations) {
						result.Broken = true;
					} else {
						result.Decorations.push_back(token);
					}
					if (comma == std::string_view::npos) {
						break;
					}
					list.remove_prefix(comma + 1U);
				}
				at = listEnd + 1U;
				close();
				skipSpace();
			}
			if (at >= line.size() || line[at] != ':') {
				return result;
			}
			++at;
			close();
			if (!closed) {
				return result;
			}
			skipSpace();
			result.Shaped       = true;
			result.SubjectBegin = at;
			return result;
		}

		[[nodiscard]] auto as_written(const agent_comment_reading aReading, const std::string_view aText) -> agent_comment_suggestion
		{
			return { .Reading = aReading, .Body = std::string{ aText } };
		}

		/// One edit through the label control's own serializer, against the
		/// exact current body.
		[[nodiscard]] auto edit(std::string& aBody, const feedback_header_edit& aEdit) -> bool
		{
			const auto current  = recognize_feedback_prefix(aBody);
			const auto decision = decide_feedback_composition_edit({ .Body = aBody, .Segment = { }, .ExpectedDigest = current.BodyDigest, .Edit = aEdit });
			if (decision.Refusal != feedback_edit_refusal::none) {
				return false;
			}
			if (decision.Changed) {
				aBody = decision.Body;
			}
			return true;
		}
	}

	auto stable_name(const agent_comment_reading aReading) noexcept -> std::string_view
	{
		switch (aReading) {
		case agent_comment_reading::unlabelled:
			return "unlabelled";
		case agent_comment_reading::unknown_label:
			return "unknown_label";
		case agent_comment_reading::labelled:
			return "labelled";
		case agent_comment_reading::kept_as_written:
			return "kept_as_written";
		}
		return "unknown";
	}

	auto read_agent_comment_suggestion(const std::string_view aText) -> agent_comment_suggestion
	{
		auto start = std::size_t{ 0U };
		while (start < aText.size() && white_space(aText[start])) {
			++start;
		}
		const auto text    = aText.substr(start);
		const auto scanned = scan_header(text);
		if (scanned.Label.empty() || (!scanned.Shaped && !scanned.Broken)) {
			return as_written(agent_comment_reading::unlabelled, aText);
		}
		const auto label = find_feedback_label(scanned.Label);
		if (!label) {
			return as_written(scanned.Shaped ? agent_comment_reading::unknown_label : agent_comment_reading::unlabelled, aText);
		}
		if (scanned.Broken) {
			return as_written(agent_comment_reading::kept_as_written, aText);
		}
		auto tokens = std::vector<std::string>{ };
		for (const auto decoration : scanned.Decorations) {
			if (const auto standard = standard_decoration(decoration)) {
				tokens.emplace_back(canonical_token(*standard));
			} else if (valid_feedback_decoration(decoration)) {
				tokens.emplace_back(decoration);
			} else {
				return as_written(agent_comment_reading::kept_as_written, aText);
			}
		}
		// The subject is the rest of the first line, or, when the header
		// stands alone on it, the next line that is not blank.
		auto subject = scanned.SubjectBegin;
		if (subject >= text.size() || text[subject] == '\r' || text[subject] == '\n') {
			while (subject < text.size() && white_space(text[subject])) {
				++subject;
			}
		}
		auto body = std::string{ text.substr(subject) };
		if (!edit(body, { .Change = feedback_label_change::set, .Label = *label })) {
			return as_written(agent_comment_reading::kept_as_written, aText);
		}
		for (auto& token : tokens) {
			if (!edit(body, { .Change = feedback_label_change::keep, .Decoration = feedback_decoration_change{ .Token = std::move(token), .Written = true } })) {
				return as_written(agent_comment_reading::kept_as_written, aText);
			}
		}
		// Read back as the editor reads it. A header with no subject yet is
		// `malformed` (`empty_subject`) and still names its label.
		const auto written = recognize_feedback_prefix(body);
		const auto named   = written.RawLabel ? find_feedback_label(feedback_text(body, *written.RawLabel)) : std::nullopt;
		if (named != label || (written.Status != feedback_prefix_status::recognized && subject < text.size())) {
			return as_written(agent_comment_reading::kept_as_written, aText);
		}
		auto result = agent_comment_suggestion{ .Reading = agent_comment_reading::labelled, .Label = label };
		for (const auto& decoration : written.Decorations) {
			switch (decoration.Kind) {
			case feedback_decoration_kind::blocking:
				result.Blocking = true;
				break;
			case feedback_decoration_kind::non_blocking:
				result.NonBlocking = true;
				break;
			case feedback_decoration_kind::if_minor:
				result.IfMinor = true;
				break;
			case feedback_decoration_kind::custom:
				result.Custom.emplace_back(feedback_text(body, decoration.Raw));
				break;
			}
		}
		result.Body = std::move(body);
		return result;
	}
}
