#include "bpr/core/feedback_convention.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <utility>

namespace bpr::core {
	namespace {
		using enum feedback_label;
		using enum feedback_label_guidance;

		constexpr std::array<feedback_label_definition, feedback_label_count> catalog{ {
			{ praise, "praise", "Recognize something genuinely valuable.", positive, "feedback.label.praise" },
			{ nitpick, "nitpick", "Request a trivial preference change.", non_blocking_by_nature, "feedback.label.nitpick" },
			{ suggestion, "suggestion", "Propose an improvement and explain its benefit.", unspecified, "feedback.label.suggestion" },
			{ issue, "issue", "Identify a concrete problem.", unspecified, "feedback.label.issue" },
			{ todo, "todo", "Request a small necessary change.", necessary_change, "feedback.label.todo" },
			{ question, "question", "Seek clarification or investigation.", unspecified, "feedback.label.question" },
			{ thought, "thought", "Offer an exploratory idea.", non_blocking_by_nature, "feedback.label.thought" },
			{ chore, "chore", "Request required process work before acceptance.", required_before_acceptance, "feedback.label.chore" },
			{ note, "note", "Highlight information without requiring action.", always_non_blocking, "feedback.label.note" },
			{ typo, "typo", "Identify a spelling correction.", necessary_change, "feedback.label.typo" },
			{ polish, "polish", "Suggest an immediate quality improvement.", unspecified, "feedback.label.polish" },
			{ quibble, "quibble", "Request a minor preference change.", non_blocking_by_nature, "feedback.label.quibble" },
		} };

		auto ascii_lower(unsigned char aByte) noexcept -> unsigned char
		{
			return aByte >= 'A' && aByte <= 'Z' ? static_cast<unsigned char>(aByte - 'A' + 'a') : aByte;
		}
		auto ascii_equal(std::string_view aLeft, std::string_view aRight) noexcept -> bool
		{
			return std::ranges::equal(aLeft, aRight, [](char aA, char aB) { return ascii_lower(static_cast<unsigned char>(aA)) == ascii_lower(static_cast<unsigned char>(aB)); });
		}
		auto letter(unsigned char aByte) noexcept -> bool
		{
			return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z');
		}
		auto token_byte(unsigned char aByte) noexcept -> bool
		{
			return letter(aByte) || (aByte >= '0' && aByte <= '9') || aByte == '-' || aByte == '_';
		}
		auto horizontal_space(unsigned char aByte) noexcept -> bool
		{
			return aByte == ' ' || aByte == '\t';
		}
		auto line_end(unsigned char aByte) noexcept -> bool
		{
			return aByte == '\r' || aByte == '\n';
		}
		auto decoration_kind(std::string_view aRaw) noexcept -> feedback_decoration_kind
		{
			if (ascii_equal(aRaw, "blocking")) {
				return feedback_decoration_kind::blocking;
			}
			if (ascii_equal(aRaw, "non-blocking")) {
				return feedback_decoration_kind::non_blocking;
			}
			if (ascii_equal(aRaw, "if-minor")) {
				return feedback_decoration_kind::if_minor;
			}
			return feedback_decoration_kind::custom;
		}

		/// The bounded window the classifier may read. Running into its end
		/// while the user body continues is what makes a body `limited`.
		class prefix_cursor
		{
		public:
			prefix_cursor(std::string_view aBody, feedback_span aUserBody) noexcept
				: Body{ aBody }
				, Begin{ aUserBody.Begin }
				, End{ aUserBody.Begin + std::min(aUserBody.End - aUserBody.Begin, maximum_feedback_prefix_bytes) }
				, Truncated{ aUserBody.End - aUserBody.Begin > maximum_feedback_prefix_bytes }
				, Position{ aUserBody.Begin }
			{
			}
			[[nodiscard]] auto peek(std::size_t aAhead = 0U) noexcept -> std::optional<unsigned char>
			{
				const auto at = Position + aAhead;
				if (at >= End) {
					Furthest = End;
					return std::nullopt;
				}
				Furthest = std::max(Furthest, at + 1U);
				return static_cast<unsigned char>(Body[at]);
			}
			[[nodiscard]] auto peek_is(unsigned char aByte, std::size_t aAhead = 0U) noexcept -> bool
			{
				const auto byte = peek(aAhead);
				return byte && *byte == aByte;
			}
			void advance(std::size_t aCount = 1U) noexcept { Position += aCount; }
			/// \brief Skip spaces and tabs; returns how many.
			auto skip_space() noexcept -> std::size_t
			{
				const auto from = Position;
				for (auto byte = peek(); byte && horizontal_space(*byte); byte = peek()) {
					advance();
				}
				return Position - from;
			}
			/// \brief Whether a `)` remains on the current line; nothing when
			/// the window ends first.
			[[nodiscard]] auto close_on_line() noexcept -> std::optional<bool>
			{
				for (std::size_t ahead = 0U;; ++ahead) {
					const auto byte = peek(ahead);
					if (!byte) {
						return std::nullopt;
					}
					if (line_end(*byte) || *byte == ')') {
						return *byte == ')';
					}
				}
			}

			std::string_view Body;
			std::size_t      Begin;
			std::size_t      End;
			bool             Truncated;
			std::size_t      Position;
			std::size_t      Furthest{ Begin };
		};

		enum class scan_outcome : std::uint8_t {
			plain,
			header,
			malformed,
			limited,
		};
		struct header_scan
		{
			scan_outcome                       Outcome = scan_outcome::plain;
			std::optional<feedback_label>      Label;
			std::optional<feedback_span>       RawLabel;
			feedback_header_form               Form         = feedback_header_form::plain;
			std::size_t                        HeaderBegin  = 0U;
			std::size_t                        SubjectBegin = 0U;
			std::vector<feedback_decoration>   Decorations;
			std::optional<feedback_diagnostic> Problem;
		};

		/// The single-pass scanner. Each method moves forward only, so the
		/// whole scan is linear in the window it reads.
		class header_scanner
		{
		public:
			header_scanner(std::string_view aBody, feedback_span aUserBody) noexcept
				: Cursor{ aBody, aUserBody }
			{
			}
			auto scan() -> header_scan
			{
				if (!skip_leading()) {
					return Result;
				}
				Result.HeaderBegin = Cursor.Position;
				const auto first   = Cursor.peek();
				if (first && *first == '*' && Cursor.peek_is('*', 1U)) {
					Result.Form = feedback_header_form::strong_header;
					Cursor.advance(2U);
				}
				if (!read_label() || !read_marker_and_decorations() || !read_colon_and_separator()) {
					return Result;
				}
				Result.Outcome      = scan_outcome::header;
				Result.SubjectBegin = Cursor.Position;
				return Result;
			}
			[[nodiscard]] auto inspected() const noexcept -> std::size_t { return Cursor.Furthest > Cursor.Begin ? Cursor.Furthest - Cursor.Begin : 0U; }

		private:
			/// Ends the scan. A broken header after a catalog label is
			/// `malformed`; after any other word it is just prose.
			auto refuse(feedback_diagnostic aProblem) -> bool
			{
				if (Result.Label) {
					Result.Outcome = scan_outcome::malformed;
					Result.Problem = aProblem;
				} else {
					Result.Outcome = scan_outcome::plain;
				}
				return false;
			}
			auto stop_plain() -> bool
			{
				Result.Outcome = scan_outcome::plain;
				return false;
			}
			auto stop_limited() -> bool
			{
				Result.Outcome = scan_outcome::limited;
				Result.Problem = feedback_diagnostic::classifier_budget_exceeded;
				return false;
			}
			/// The window ended. If the user body goes on, the budget did.
			auto at_end(feedback_diagnostic aProblem) -> bool
			{
				return Cursor.Truncated ? stop_limited() : refuse(aProblem);
			}
			auto at_end_plain() -> bool
			{
				return Cursor.Truncated ? stop_limited() : stop_plain();
			}

			/// Optional BOM, blank lines, then at most three spaces.
			auto skip_leading() -> bool
			{
				if (Cursor.peek_is(0xEFU) && Cursor.peek_is(0xBBU, 1U) && Cursor.peek_is(0xBFU, 2U)) {
					Cursor.advance(3U);
				}
				while (true) {
					const auto lineBegin = Cursor.Position;
					auto       byte      = Cursor.peek();
					while (byte && (horizontal_space(*byte) || *byte == '\r')) {
						Cursor.advance();
						byte = Cursor.peek();
					}
					if (!byte) {
						return at_end_plain();
					}
					if (*byte == '\n') {
						Cursor.advance();
						continue;
					}
					Cursor.Position = lineBegin;
					break;
				}
				for (std::size_t indent = 0U;; ++indent) {
					const auto byte = Cursor.peek();
					if (byte && *byte == ' ' && indent < 3U) {
						Cursor.advance();
						continue;
					}
					// A fourth space or a tab opens an indented code block.
					return byte && *byte != ' ' && *byte != '\t' ? true : stop_plain();
				}
			}
			auto read_label() -> bool
			{
				const auto first = Cursor.peek();
				if (!first) {
					return at_end_plain();
				}
				if (!letter(*first)) {
					return stop_plain();
				}
				const auto begin = Cursor.Position;
				for (auto byte = Cursor.peek(); byte && token_byte(*byte); byte = Cursor.peek()) {
					if (Cursor.Position - begin == maximum_feedback_label_bytes) {
						return stop_plain();
					}
					Cursor.advance();
				}
				Result.RawLabel = feedback_span{ begin, Cursor.Position };
				Result.Label    = find_feedback_label(Cursor.Body.substr(begin, Cursor.Position - begin));
				return true;
			}
			/// The strong label closer, then either the colon or a
			/// decoration list after optional space.
			auto read_marker_and_decorations() -> bool
			{
				if (Result.Form == feedback_header_form::strong_header && Cursor.peek_is('*') && Cursor.peek_is('*', 1U)) {
					Result.Form = feedback_header_form::strong_label;
					Cursor.advance(2U);
				}
				const auto spaces = Cursor.skip_space();
				const auto byte   = Cursor.peek();
				if (!byte) {
					return at_end_plain();
				}
				if (*byte == ':') {
					return spaces == 0U ? true : refuse(feedback_diagnostic::malformed_header);
				}
				if (*byte != '(') {
					return stop_plain();
				}
				Cursor.advance();
				if (!read_decorations()) {
					return false;
				}
				const auto colon = Cursor.peek();
				if (!colon) {
					return at_end(feedback_diagnostic::missing_colon);
				}
				return *colon == ':' ? true : refuse(feedback_diagnostic::missing_colon);
			}
			/// A token that cannot continue the list: if no `)` follows on
			/// this line the list was never closed.
			auto refuse_token() -> bool
			{
				const auto closed = Cursor.close_on_line();
				if (closed && *closed) {
					return refuse(feedback_diagnostic::invalid_decoration);
				}
				return closed ? refuse(feedback_diagnostic::unclosed_decoration) : at_end(feedback_diagnostic::unclosed_decoration);
			}
			auto refuse_delimiter(unsigned char aByte, feedback_diagnostic aEmpty) -> bool
			{
				if (aByte == ')' || aByte == ',') {
					return refuse(aEmpty);
				}
				if (aByte == '(') {
					return refuse(feedback_diagnostic::nested_decoration);
				}
				if (line_end(aByte)) {
					return refuse(feedback_diagnostic::unclosed_decoration);
				}
				return refuse_token();
			}
			/// `( token [, token]* )` with optional space around separators.
			auto read_decorations() -> bool
			{
				while (true) {
					Cursor.skip_space();
					auto byte = Cursor.peek();
					if (!byte) {
						return at_end(feedback_diagnostic::unclosed_decoration);
					}
					if (!token_byte(*byte)) {
						return refuse_delimiter(*byte, Result.Decorations.empty() && *byte == ')' ? feedback_diagnostic::empty_decoration_list : feedback_diagnostic::empty_decoration);
					}
					if (Result.Decorations.size() == maximum_feedback_decorations) {
						return stop_limited();
					}
					const auto begin = Cursor.Position;
					while (byte && token_byte(*byte)) {
						Cursor.advance();
						byte = Cursor.peek();
					}
					const feedback_span raw{ begin, Cursor.Position };
					Result.Decorations.push_back({ raw, decoration_kind(feedback_text(Cursor.Body, raw)) });
					Cursor.skip_space();
					byte = Cursor.peek();
					if (!byte) {
						return at_end(feedback_diagnostic::unclosed_decoration);
					}
					if (*byte != ')' && *byte != ',') {
						return refuse_delimiter(*byte, feedback_diagnostic::empty_decoration);
					}
					Cursor.advance();
					if (*byte == ')') {
						return true;
					}
				}
			}
			/// The colon (and the strong closer in `**label:**`), then space
			/// and a nonblank subject on the same line.
			auto read_colon_and_separator() -> bool
			{
				Cursor.advance();
				if (Result.Form == feedback_header_form::strong_header) {
					if (!Cursor.peek_is('*') || !Cursor.peek_is('*', 1U)) {
						const bool ended = !Cursor.peek() || (Cursor.peek_is('*') && !Cursor.peek(1U));
						return ended ? at_end(feedback_diagnostic::malformed_header) : refuse(feedback_diagnostic::malformed_header);
					}
					Cursor.advance(2U);
				}
				const auto spaces = Cursor.skip_space();
				const auto byte   = Cursor.peek();
				if (!byte) {
					return at_end(feedback_diagnostic::empty_subject);
				}
				if (line_end(*byte)) {
					return refuse(feedback_diagnostic::empty_subject);
				}
				return spaces > 0U ? true : refuse(feedback_diagnostic::malformed_header);
			}

			prefix_cursor Cursor;
			header_scan   Result;
		};

		auto digest_of(std::string_view aBody) noexcept -> std::array<std::uint8_t, 32>
		{
			return sha256({ reinterpret_cast<const std::uint8_t*>(aBody.data()), aBody.size() });
		}

		/// Advisory: an explicit decoration at odds with the label's guidance.
		auto contradicts(feedback_label_guidance aGuidance, feedback_expectation aExpectation) noexcept -> bool
		{
			switch (aGuidance) {
			case feedback_label_guidance::positive:
			case feedback_label_guidance::non_blocking_by_nature:
				return aExpectation == feedback_expectation::blocking;
			case feedback_label_guidance::always_non_blocking:
				return aExpectation == feedback_expectation::blocking || aExpectation == feedback_expectation::if_minor;
			case feedback_label_guidance::necessary_change:
			case feedback_label_guidance::required_before_acceptance:
				return aExpectation == feedback_expectation::non_blocking;
			case feedback_label_guidance::unspecified:
				return false;
			}
			return false;
		}
		auto expectation_of(feedback_decoration_kind aKind) noexcept -> feedback_expectation
		{
			switch (aKind) {
			case feedback_decoration_kind::blocking:
				return feedback_expectation::blocking;
			case feedback_decoration_kind::non_blocking:
				return feedback_expectation::non_blocking;
			case feedback_decoration_kind::if_minor:
				return feedback_expectation::if_minor;
			case feedback_decoration_kind::custom:
				break;
			}
			return feedback_expectation::unspecified;
		}
		/// Declared expectation and the decoration diagnostics. Duplicates
		/// are kept as written; only distinct standard tokens conflict.
		void classify_decorations(std::string_view aBody, feedback_annotation& aResult)
		{
			const auto& decorations = aResult.Decorations;
			bool        duplicate   = false;
			for (std::size_t at = 0U; at < decorations.size() && !duplicate; ++at) {
				duplicate = std::ranges::any_of(decorations.begin() + static_cast<std::ptrdiff_t>(at) + 1, decorations.end(), [&](const feedback_decoration& aOther) { return ascii_equal(feedback_text(aBody, decorations[at].Raw), feedback_text(aBody, aOther.Raw)); });
			}
			if (duplicate) {
				aResult.Diagnostics.push_back(feedback_diagnostic::duplicate_decoration);
			}
			if (aResult.Status != feedback_prefix_status::recognized) {
				return;
			}
			for (const auto& decoration : decorations) {
				const auto declared = expectation_of(decoration.Kind);
				if (declared == feedback_expectation::unspecified || declared == aResult.Expectation) {
					continue;
				}
				aResult.Expectation = aResult.Expectation == feedback_expectation::unspecified ? declared : feedback_expectation::conflicting;
			}
			if (aResult.Expectation == feedback_expectation::conflicting) {
				aResult.Diagnostics.push_back(feedback_diagnostic::conflicting_decorations);
			} else if (contradicts(*aResult.Guidance, aResult.Expectation)) {
				aResult.Diagnostics.push_back(feedback_diagnostic::label_expectation_conflict);
			}
		}
		/// Subject runs to the end of its line, less trailing space and CR.
		void place_subject(std::string_view aBody, feedback_span aUserBody, std::size_t aSubjectBegin, feedback_annotation& aResult)
		{
			const auto line    = aBody.substr(aSubjectBegin, aUserBody.End - aSubjectBegin);
			const auto newline = line.find('\n');
			const auto text    = line.substr(0U, newline);
			const auto last    = text.find_last_not_of(" \t\r");
			aResult.Subject    = feedback_span{ aSubjectBegin, aSubjectBegin + last + 1U };
			if (newline != std::string_view::npos) {
				aResult.Discussion = feedback_span{ aSubjectBegin + newline + 1U, aUserBody.End };
			}
		}
		auto recognize(std::string_view aBody, feedback_span aUserBody, const std::array<std::uint8_t, 32>& aBodyDigest) -> feedback_annotation
		{
			feedback_annotation result;
			result.BodyDigest = aBodyDigest;
			result.UserBody   = aUserBody;
			header_scanner scanner{ aBody, aUserBody };
			auto           scan   = scanner.scan();
			result.InspectedBytes = scanner.inspected();
			result.RawLabel       = scan.RawLabel;
			switch (scan.Outcome) {
			case scan_outcome::plain:
				result.RawLabel = std::nullopt;
				return result;
			case scan_outcome::malformed:
				result.Status = feedback_prefix_status::malformed;
				result.Diagnostics.push_back(*scan.Problem);
				return result;
			case scan_outcome::limited:
				result.Status = feedback_prefix_status::limited;
				result.Diagnostics.push_back(feedback_diagnostic::classifier_budget_exceeded);
				return result;
			case scan_outcome::header:
				break;
			}
			result.Status = scan.Label ? feedback_prefix_status::recognized : feedback_prefix_status::unknown_label;
			if (scan.Label) {
				result.Label    = scan.Label;
				result.Guidance = definition_of(*scan.Label).Guidance;
			}
			result.Form        = scan.Form;
			result.Header      = feedback_span{ scan.HeaderBegin, scan.SubjectBegin };
			result.Decorations = std::move(scan.Decorations);
			place_subject(aBody, aUserBody, scan.SubjectBegin, result);
			classify_decorations(aBody, result);
			return result;
		}

		/// A managed body's user segment, or `classification_unavailable` when
		/// its owner found none inside these bytes.
		auto recognize_segment(std::string_view aBody, std::optional<feedback_span> aUserBody, const std::array<std::uint8_t, 32>& aBodyDigest)
			-> feedback_annotation
		{
			if (!aUserBody || aUserBody->Begin > aUserBody->End || aUserBody->End > aBody.size()) {
				feedback_annotation result;
				result.BodyDigest = aBodyDigest;
				result.Diagnostics.push_back(feedback_diagnostic::classification_unavailable);
				return result;
			}
			return recognize(aBody, *aUserBody, aBodyDigest);
		}
	}

	auto feedback_label_catalog() noexcept -> std::span<const feedback_label_definition, feedback_label_count>
	{
		return catalog;
	}
	auto definition_of(feedback_label aLabel) noexcept -> const feedback_label_definition&
	{
		return catalog[static_cast<std::size_t>(aLabel)];
	}
	auto find_feedback_label(std::string_view aRaw) noexcept -> std::optional<feedback_label>
	{
		const auto found = std::ranges::find_if(catalog, [&](const feedback_label_definition& aEntry) { return ascii_equal(aEntry.Id, aRaw); });
		return found == catalog.end() ? std::nullopt : std::optional{ found->Label };
	}
	auto stable_name(feedback_label aLabel) noexcept -> std::string_view
	{
		return definition_of(aLabel).Id;
	}
	auto stable_name(feedback_label_guidance aGuidance) noexcept -> std::string_view
	{
		switch (aGuidance) {
		case feedback_label_guidance::positive:
			return "positive";
		case feedback_label_guidance::non_blocking_by_nature:
			return "non_blocking_by_nature";
		case feedback_label_guidance::unspecified:
			return "unspecified";
		case feedback_label_guidance::necessary_change:
			return "necessary_change";
		case feedback_label_guidance::required_before_acceptance:
			return "required_before_acceptance";
		case feedback_label_guidance::always_non_blocking:
			return "always_non_blocking";
		}
		return "unknown";
	}
	auto stable_name(feedback_prefix_status aStatus) noexcept -> std::string_view
	{
		switch (aStatus) {
		case feedback_prefix_status::recognized:
			return "recognized";
		case feedback_prefix_status::plain:
			return "plain";
		case feedback_prefix_status::unknown_label:
			return "unknown_label";
		case feedback_prefix_status::malformed:
			return "malformed";
		case feedback_prefix_status::limited:
			return "limited";
		}
		return "unknown";
	}
	auto stable_name(feedback_expectation aExpectation) noexcept -> std::string_view
	{
		switch (aExpectation) {
		case feedback_expectation::unspecified:
			return "unspecified";
		case feedback_expectation::blocking:
			return "blocking";
		case feedback_expectation::non_blocking:
			return "non_blocking";
		case feedback_expectation::if_minor:
			return "if_minor";
		case feedback_expectation::conflicting:
			return "conflicting";
		}
		return "unknown";
	}
	auto stable_name(feedback_diagnostic aDiagnostic) noexcept -> std::string_view
	{
		switch (aDiagnostic) {
		case feedback_diagnostic::conflicting_decorations:
			return "conflicting_decorations";
		case feedback_diagnostic::duplicate_decoration:
			return "duplicate_decoration";
		case feedback_diagnostic::label_expectation_conflict:
			return "label_expectation_conflict";
		case feedback_diagnostic::empty_subject:
			return "empty_subject";
		case feedback_diagnostic::empty_decoration_list:
			return "empty_decoration_list";
		case feedback_diagnostic::empty_decoration:
			return "empty_decoration";
		case feedback_diagnostic::nested_decoration:
			return "nested_decoration";
		case feedback_diagnostic::unclosed_decoration:
			return "unclosed_decoration";
		case feedback_diagnostic::invalid_decoration:
			return "invalid_decoration";
		case feedback_diagnostic::missing_colon:
			return "missing_colon";
		case feedback_diagnostic::malformed_header:
			return "malformed_header";
		case feedback_diagnostic::classifier_budget_exceeded:
			return "classifier_budget_exceeded";
		case feedback_diagnostic::classification_unavailable:
			return "classification_unavailable";
		}
		return "unknown";
	}
	auto feedback_text(std::string_view aBody, feedback_span aSpan) noexcept -> std::string_view
	{
		if (aSpan.Begin > aSpan.End || aSpan.End > aBody.size()) {
			return { };
		}
		return aBody.substr(aSpan.Begin, aSpan.End - aSpan.Begin);
	}
	auto recognize_feedback_prefix(std::string_view aBody) -> feedback_annotation
	{
		return recognize(aBody, { 0U, aBody.size() }, digest_of(aBody));
	}
	auto recognize_feedback_segment(std::string_view aBody, std::optional<feedback_span> aUserBody) -> feedback_annotation
	{
		return recognize_segment(aBody, aUserBody, digest_of(aBody));
	}
	auto recognize_feedback_body(std::string_view aBody, const feedback_body_segment& aSegment) -> feedback_annotation
	{
		return recognize_feedback_body(aBody, aSegment, digest_of(aBody));
	}
	auto recognize_feedback_body(std::string_view aBody, const feedback_body_segment& aSegment, const std::array<std::uint8_t, 32>& aBodyDigest)
		-> feedback_annotation
	{
		return aSegment.Managed ? recognize_segment(aBody, aSegment.UserBody, aBodyDigest) : recognize(aBody, { 0U, aBody.size() }, aBodyDigest);
	}
}
