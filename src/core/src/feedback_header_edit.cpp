#include "bpr/core/feedback_header_edit.hpp"

#include "bpr/core/revisioned_review_draft.hpp"

#include <algorithm>
#include <iterator>
#include <ranges>
#include <vector>

namespace bpr::core {
	namespace {
		auto has(const feedback_annotation& aAnnotation, feedback_diagnostic aDiagnostic) -> bool
		{
			return std::ranges::find(aAnnotation.Diagnostics, aDiagnostic) != aAnnotation.Diagnostics.end();
		}
		auto standard_text(feedback_expectation aExpectation) noexcept -> std::string_view
		{
			switch (aExpectation) {
			case feedback_expectation::blocking:
				return "blocking";
			case feedback_expectation::non_blocking:
				return "non-blocking";
			case feedback_expectation::if_minor:
				return "if-minor";
			case feedback_expectation::unspecified:
			case feedback_expectation::conflicting:
				break;
			}
			return { };
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

		auto ascii_lower(char aByte) noexcept -> char
		{
			return aByte >= 'A' && aByte <= 'Z' ? static_cast<char>(aByte - 'A' + 'a') : aByte;
		}
		auto ascii_equal(std::string_view aLeft, std::string_view aRight) noexcept -> bool
		{
			return aLeft.size() == aRight.size() && std::ranges::equal(aLeft, aRight, [](char aFirst, char aSecond) { return ascii_lower(aFirst) == ascii_lower(aSecond); });
		}
		/// The recognizer's own reading of one token.
		auto kind_of(std::string_view aToken) noexcept -> feedback_decoration_kind
		{
			for (const auto kind : { feedback_decoration_kind::blocking, feedback_decoration_kind::non_blocking, feedback_decoration_kind::if_minor }) {
				if (ascii_equal(aToken, standard_text(expectation_of(kind)))) {
					return kind;
				}
			}
			return feedback_decoration_kind::custom;
		}

		/// The header to write: its label text, its decoration tokens and the
		/// form the author used.
		struct header_plan
		{
			std::string_view              Label;
			std::optional<feedback_label> Known;
			std::vector<std::string_view> Decorations;
			feedback_header_form          Form = feedback_header_form::plain;
		};

		auto render(const header_plan& aPlan) -> std::string
		{
			std::string decorations;
			for (const auto token : aPlan.Decorations) {
				decorations += decorations.empty() ? " (" : ", ";
				decorations += token;
			}
			if (!decorations.empty()) {
				decorations += ')';
			}
			std::string result;
			switch (aPlan.Form) {
			case feedback_header_form::plain:
				result.append(aPlan.Label).append(decorations).append(": ");
				break;
			case feedback_header_form::strong_header:
				result.append("**").append(aPlan.Label).append(decorations).append(":** ");
				break;
			case feedback_header_form::strong_label:
				result.append("**").append(aPlan.Label).append("**").append(decorations).append(": ");
				break;
			}
			return result;
		}

		/// Rewrite only the standard decorations: the first becomes the
		/// target, the rest go, custom ones stay in place. Returns nothing
		/// when the tokens already declare exactly the target.
		auto rewrite_decorations(std::string_view aBody, const std::vector<feedback_decoration>& aDecorations, feedback_expectation aTarget)
			-> std::optional<std::vector<std::string_view>>
		{
			const auto standard = [](const feedback_decoration& aDecoration) { return aDecoration.Kind != feedback_decoration_kind::custom; };
			const bool exact    = aTarget == feedback_expectation::unspecified
				? std::ranges::none_of(aDecorations, standard)
				: std::ranges::any_of(aDecorations, standard) && std::ranges::all_of(aDecorations, [&](const feedback_decoration& aDecoration) { return !standard(aDecoration) || expectation_of(aDecoration.Kind) == aTarget; });
			if (exact) {
				return std::nullopt;
			}
			std::vector<std::string_view> tokens;
			bool                          written = false;
			for (const auto& decoration : aDecorations) {
				if (!standard(decoration)) {
					tokens.push_back(feedback_text(aBody, decoration.Raw));
				} else if (!written && aTarget != feedback_expectation::unspecified) {
					tokens.push_back(standard_text(aTarget));
					written = true;
				}
			}
			if (!written && aTarget != feedback_expectation::unspecified) {
				tokens.push_back(standard_text(aTarget));
			}
			return tokens;
		}

		/// Write or take out one decoration; every other token stays as
		/// written, in place. Returns nothing when the list would not change.
		auto change_decoration(std::string_view aBody, const std::vector<feedback_decoration>& aDecorations, const feedback_decoration_change& aChange)
			-> std::optional<std::vector<std::string_view>>
		{
			const auto kind     = kind_of(aChange.Token);
			const auto opposite = kind == feedback_decoration_kind::blocking ? std::optional{ feedback_decoration_kind::non_blocking }
				: kind == feedback_decoration_kind::non_blocking             ? std::optional{ feedback_decoration_kind::blocking }
																			 : std::nullopt;
			const auto same     = [&](const feedback_decoration& aDecoration) {
				return kind == feedback_decoration_kind::custom ? aDecoration.Kind == kind && ascii_equal(feedback_text(aBody, aDecoration.Raw), aChange.Token) : aDecoration.Kind == kind;
			};
			std::vector<std::string_view> tokens;
			bool                          changed = false;
			bool                          present = false;
			for (const auto& decoration : aDecorations) {
				const auto drop = same(decoration) ? !aChange.Written : aChange.Written && opposite == decoration.Kind;
				present         = present || same(decoration);
				changed         = changed || drop;
				if (!drop) {
					tokens.push_back(feedback_text(aBody, decoration.Raw));
				}
			}
			if (aChange.Written && !present) {
				tokens.push_back(kind == feedback_decoration_kind::custom ? std::string_view{ aChange.Token } : standard_text(expectation_of(kind)));
				changed = true;
			}
			return changed ? std::optional{ std::move(tokens) } : std::nullopt;
		}

		auto line_end_or_space(std::string_view aBody, std::size_t aAt, std::size_t aEnd) noexcept -> bool
		{
			return aAt >= aEnd || aBody[aAt] == ' ' || aBody[aAt] == '\t' || aBody[aAt] == '\r' || aBody[aAt] == '\n';
		}
		auto run_of(std::string_view aBody, std::size_t aAt, std::size_t aEnd, char aByte) noexcept -> std::size_t
		{
			std::size_t count = 0U;
			while (aAt + count < aEnd && aBody[aAt + count] == aByte) {
				++count;
			}
			return count;
		}
		/// A line of three or more `-`, `*` or `_` with only spaces between.
		auto thematic_break(std::string_view aLine, char aMark) noexcept -> bool
		{
			const auto marks = std::ranges::count(aLine, aMark);
			return marks >= 3 && std::ranges::all_of(aLine, [aMark](char aByte) { return aByte == aMark || aByte == ' ' || aByte == '\t' || aByte == '\r'; });
		}
		/// Whether the line starting at `aAt` opens a block that a header
		/// written in front of it would turn into paragraph text.
		auto opens_block(std::string_view aBody, std::size_t aAt, std::size_t aEnd) -> bool
		{
			const auto newline = aBody.find('\n', aAt);
			const auto line    = aBody.substr(aAt, std::min(newline, aEnd) - aAt);
			const char first   = aBody[aAt];
			switch (first) {
			case '>':
			case '<':
			case '|':
				return true;
			case '#': {
				const auto marks = run_of(aBody, aAt, aEnd, '#');
				return marks <= 6U && line_end_or_space(aBody, aAt + marks, aEnd);
			}
			case '`':
			case '~':
				return run_of(aBody, aAt, aEnd, first) >= 3U;
			case '-':
			case '+':
			case '*':
				return line_end_or_space(aBody, aAt + 1U, aEnd) || thematic_break(line, first);
			case '_':
				return thematic_break(line, first);
			case '[':
				return line.find("]:") != std::string_view::npos;
			default:
				break;
			}
			if (first >= '0' && first <= '9') {
				std::size_t digits = 0U;
				while (aAt + digits < aEnd && digits < 10U && aBody[aAt + digits] >= '0' && aBody[aAt + digits] <= '9') {
					++digits;
				}
				const auto after = aAt + digits;
				return digits <= 9U && after < aEnd && (aBody[after] == '.' || aBody[after] == ')') && line_end_or_space(aBody, after + 1U, aEnd);
			}
			return false;
		}
		/// Where a header for a plain body goes: the first byte of its first
		/// nonblank line, after an optional BOM and at most three spaces. Not
		/// when that line is indented code, empty, or opens another block.
		auto insertion_point(std::string_view aBody, feedback_span aUserBody) -> std::optional<std::size_t>
		{
			auto at = aUserBody.Begin;
			if (aBody.substr(at, std::min<std::size_t>(3U, aUserBody.End - at)) == "\xEF\xBB\xBF") {
				at += 3U;
			}
			while (true) {
				auto end = at;
				while (end < aUserBody.End && (aBody[end] == ' ' || aBody[end] == '\t' || aBody[end] == '\r')) {
					++end;
				}
				if (end >= aUserBody.End) {
					return std::nullopt;
				}
				if (aBody[end] != '\n') {
					break;
				}
				at = end + 1U;
			}
			const auto spaces = run_of(aBody, at, std::min(aUserBody.End, at + 4U), ' ');
			if (spaces > 3U || aBody[at + spaces] == '\t') {
				return std::nullopt;
			}
			at += spaces;
			return opens_block(aBody, at, aUserBody.End) ? std::nullopt : std::optional{ at };
		}

		/// The result must read back as exactly the header that was written,
		/// in the place it was written, with the old subject after it.
		auto reads_back(const feedback_header_edit_decision& aDecision, const header_plan& aPlan, const feedback_header_edit& aEdit) -> bool
		{
			const auto& result = aDecision.Annotation;
			if (aEdit.Change == feedback_label_change::remove) {
				return result.Status != feedback_prefix_status::recognized;
			}
			const auto header = feedback_span{ aDecision.Replaced.Begin, aDecision.Replaced.Begin + aDecision.Replacement.size() };
			if (result.Status != (aPlan.Known ? feedback_prefix_status::recognized : feedback_prefix_status::unknown_label)
				|| result.Header != header || result.Label != aPlan.Known || result.Decorations.size() != aPlan.Decorations.size()) {
				return false;
			}
			for (std::size_t at = 0U; at < aPlan.Decorations.size(); ++at) {
				if (feedback_text(aDecision.Body, result.Decorations[at].Raw) != aPlan.Decorations[at]) {
					return false;
				}
			}
			return !aPlan.Known || !aEdit.Expectation || result.Expectation == *aEdit.Expectation;
		}

		auto refuse(feedback_header_edit_decision aDecision, feedback_edit_refusal aRefusal) -> feedback_header_edit_decision
		{
			aDecision.Refusal = aRefusal;
			return aDecision;
		}

		/// The span to replace and the header to write there, or a refusal.
		struct edit_plan
		{
			feedback_edit_refusal Refusal = feedback_edit_refusal::none;
			bool                  Noop    = false;
			feedback_span         Replaced;
			header_plan           Header;
		};
		auto plan_structured(std::string_view aBody, const feedback_annotation& aCurrent, const feedback_header_edit& aEdit) -> edit_plan
		{
			edit_plan plan{ .Replaced = *aCurrent.Header };
			if (aEdit.Change == feedback_label_change::remove) {
				return plan;
			}
			const bool relabel = aEdit.Change == feedback_label_change::set && aCurrent.Label != aEdit.Label;
			plan.Header.Form   = aCurrent.Form;
			plan.Header.Known  = relabel ? std::optional{ aEdit.Label } : aCurrent.Label;
			plan.Header.Label  = relabel ? stable_name(aEdit.Label) : feedback_text(aBody, *aCurrent.RawLabel);
			auto rewritten     = aEdit.Expectation ? rewrite_decorations(aBody, aCurrent.Decorations, *aEdit.Expectation)
				: aEdit.Decoration                 ? change_decoration(aBody, aCurrent.Decorations, *aEdit.Decoration)
												   : std::nullopt;
			if (!relabel && !rewritten) {
				plan.Noop = true;
				return plan;
			}
			if (rewritten) {
				plan.Header.Decorations = std::move(*rewritten);
			} else {
				std::ranges::transform(aCurrent.Decorations, std::back_inserter(plan.Header.Decorations), [&](const feedback_decoration& aDecoration) { return feedback_text(aBody, aDecoration.Raw); });
			}
			return plan;
		}
		auto plan_plain(std::string_view aBody, const feedback_annotation& aCurrent, const feedback_header_edit& aEdit, feedback_span aUserBody) -> edit_plan
		{
			if (has(aCurrent, feedback_diagnostic::classification_unavailable)) {
				return { .Refusal = feedback_edit_refusal::unsupported_header };
			}
			if (aEdit.Change == feedback_label_change::keep) {
				return { .Refusal = feedback_edit_refusal::invalid_edit };
			}
			if (aEdit.Change == feedback_label_change::remove) {
				return { .Noop = true };
			}
			const auto at = insertion_point(aBody, aUserBody);
			if (!at) {
				return { .Refusal = feedback_edit_refusal::restructuring_required };
			}
			edit_plan plan{ .Replaced = { *at, *at } };
			plan.Header.Known = aEdit.Label;
			plan.Header.Label = stable_name(aEdit.Label);
			if (aEdit.Expectation && *aEdit.Expectation != feedback_expectation::unspecified) {
				plan.Header.Decorations.push_back(standard_text(*aEdit.Expectation));
			}
			if (aEdit.Decoration && aEdit.Decoration->Written) {
				const auto kind = kind_of(aEdit.Decoration->Token);
				plan.Header.Decorations.push_back(kind == feedback_decoration_kind::custom ? std::string_view{ aEdit.Decoration->Token } : standard_text(expectation_of(kind)));
			}
			return plan;
		}
		auto plan_edit(std::string_view aBody, const feedback_annotation& aCurrent, const feedback_header_edit& aEdit, feedback_span aUserBody) -> edit_plan
		{
			if (aEdit.Change == feedback_label_change::keep && !aEdit.Expectation && !aEdit.Decoration) {
				return { .Noop = true };
			}
			if ((aEdit.Expectation && *aEdit.Expectation == feedback_expectation::conflicting)
				|| (aEdit.Change == feedback_label_change::remove && (aEdit.Expectation || aEdit.Decoration))
				|| (aEdit.Expectation && aEdit.Decoration) || (aEdit.Decoration && !valid_feedback_decoration(aEdit.Decoration->Token))) {
				return { .Refusal = feedback_edit_refusal::invalid_edit };
			}
			switch (aCurrent.Status) {
			case feedback_prefix_status::recognized:
			case feedback_prefix_status::unknown_label:
				return plan_structured(aBody, aCurrent, aEdit);
			case feedback_prefix_status::plain:
				return plan_plain(aBody, aCurrent, aEdit, aUserBody);
			case feedback_prefix_status::malformed:
			case feedback_prefix_status::limited:
				break;
			}
			return { .Refusal = feedback_edit_refusal::unsupported_header };
		}
	}

	auto valid_feedback_decoration(std::string_view aToken) noexcept -> bool
	{
		return !aToken.empty() && aToken.size() <= maximum_feedback_label_bytes && std::ranges::all_of(aToken, [](char aByte) {
			return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z') || (aByte >= '0' && aByte <= '9') || aByte == '-' || aByte == '_';
		});
	}

	auto stable_name(feedback_edit_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case feedback_edit_refusal::none:
			return "none";
		case feedback_edit_refusal::stale_revision:
			return "stale_revision";
		case feedback_edit_refusal::invalid_edit:
			return "invalid_edit";
		case feedback_edit_refusal::unsupported_header:
			return "unsupported_header";
		case feedback_edit_refusal::restructuring_required:
			return "restructuring_required";
		case feedback_edit_refusal::body_limit:
			return "body_limit";
		}
		return "unknown";
	}

	auto decide_feedback_header_edit(const feedback_header_edit_request& aRequest) -> feedback_header_edit_decision
	{
		const auto                    body = aRequest.Body;
		feedback_header_edit_decision decision{ .Segment = aRequest.Segment, .Annotation = recognize_feedback_body(body, aRequest.Segment) };
		if (decision.Annotation.BodyDigest != aRequest.ExpectedDigest || aRequest.ExpectedParserRevision != feedback_parser_revision) {
			return refuse(std::move(decision), feedback_edit_refusal::stale_revision);
		}
		// Absent only when the envelope's owner found no user text, which
		// plan_edit refuses before it reads the span.
		const auto userBody = decision.Annotation.UserBody.value_or(feedback_span{ });
		const auto plan     = plan_edit(body, decision.Annotation, aRequest.Edit, userBody);
		if (plan.Refusal != feedback_edit_refusal::none || plan.Noop) {
			return refuse(std::move(decision), plan.Refusal);
		}
		auto       replacement = aRequest.Edit.Change == feedback_label_change::remove ? std::string{ } : render(plan.Header);
		const auto size        = body.size() - (plan.Replaced.End - plan.Replaced.Begin) + replacement.size();
		if (size > maximum_shared_draft_body_bytes) {
			return refuse(std::move(decision), feedback_edit_refusal::body_limit);
		}
		feedback_header_edit_decision changed{ .Changed = true, .Replaced = plan.Replaced, .Replacement = std::move(replacement), .Segment = aRequest.Segment };
		changed.Body.reserve(size);
		changed.Body.append(body.substr(0U, plan.Replaced.Begin)).append(changed.Replacement).append(body.substr(plan.Replaced.End));
		if (changed.Segment.Managed) {
			changed.Segment.UserBody->End = changed.Segment.UserBody->End - (plan.Replaced.End - plan.Replaced.Begin) + changed.Replacement.size();
		}
		changed.Annotation = recognize_feedback_body(changed.Body, changed.Segment);
		if (!reads_back(changed, plan.Header, aRequest.Edit)) {
			return refuse(std::move(decision), feedback_edit_refusal::restructuring_required);
		}
		return changed;
	}
}
