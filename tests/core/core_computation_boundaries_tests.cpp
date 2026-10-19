#include "../support/test_expectation.hpp"
#include "bpr/core/cpp_identifier_context.hpp"
#include "bpr/core/diff_line_matcher.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stop_token>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;
	using bpr::tests::expect;

	[[nodiscard]] auto document(const std::string_view aText) -> decoded_text_document
	{
		auto result = decoded_text_document{ };
		result.Bytes.assign(aText.begin(), aText.end());
		result.HasFinalNewline = aText.ends_with('\n');
		for (auto begin = std::size_t{ 0U }; begin < aText.size();) {
			const auto lf   = aText.find('\n', begin);
			const auto end  = lf == std::string_view::npos ? aText.size() : lf;
			const auto crlf = end > begin && aText[end - 1U] == '\r' && lf != std::string_view::npos;
			result.Lines.push_back({ .Content = { begin, end - (crlf ? 1U : 0U) }, .Terminator = { end - (crlf ? 1U : 0U), lf == std::string_view::npos ? end : lf + 1U }, .Ending = lf == std::string_view::npos ? text_line_ending::none : crlf ? text_line_ending::crlf
																																																												  : text_line_ending::lf });
			begin = lf == std::string_view::npos ? aText.size() : lf + 1U;
		}
		return result;
	}

	void test_real_matcher()
	{
		const auto matcher = make_xdiff_line_matcher();
		const auto left    = document("same\nold\ntail\n");
		const auto right   = document("same\nnew\ntail\n");
		const auto matched = matcher->match({ .Left = &left, .Right = &right });
		expect(matched && *matched == std::vector<diff_match>{ { 0U, 0U }, { 2U, 2U } }, "core factory runs real xdiff with exact line identities");
		const auto crlf  = document("line \r\n ");
		const auto lf    = document("line\n\t");
		const auto exact = matcher->match({ .Left = &crlf, .Right = &lf });
		expect(exact && exact->empty(), "exact comparison keeps endings and whitespace");
		auto options          = diff_options{ };
		options.Whitespace    = whitespace_policy::ignore_trailing;
		options.LineEndings   = line_ending_policy::normalize;
		const auto normalized = matcher->match({ .Left = &crlf, .Right = &lf, .Options = options });
		expect(normalized && *normalized == std::vector<diff_match>{ { 0U, 0U }, { 1U, 1U } }, "normalization keeps empty final record and CRLF correction");
		const auto absent = matcher->match({ });
		expect(absent && absent->empty(), "absent sides compare as empty");
	}

	[[nodiscard]] auto role_at(const token_span_list& aSpans, const std::size_t aOffset) -> appearance_role
	{
		for (const auto& span : aSpans) {
			if (span.Line == 0U && aOffset >= span.Column && aOffset < span.Column + span.Length) {
				return span.Role;
			}
		}
		return appearance_role::syntax_normal;
	}

	void test_identifier_refinement()
	{
		constexpr auto text  = std::string_view{ "namespace GCT { enum class Reason { ID, DNA }; } auto value = GCT::Reason::DNA;" };
		auto           spans = token_span_list{ };
		expect(refine_cpp_identifiers(text, spans, 100U, { }) == syntax_classification_status::available, "core refinement is available without Qt");
		expect(role_at(spans, text.find("GCT")) == appearance_role::syntax_namespace, "namespace declaration role");
		expect(role_at(spans, text.find("ID")) == appearance_role::syntax_enum_member, "enumerator declaration role");
		expect(role_at(spans, text.rfind("DNA")) == appearance_role::syntax_enum_member, "qualified enum member role");
		const auto expected = spans;
		expect(refine_cpp_identifiers(text, spans, 100U, { }) == syntax_classification_status::available && spans == expected, "repeat refinement preserves exact spans");
		expect(refine_cpp_identifiers(text, spans, 0U, { }) == syntax_classification_status::limit_exceeded && spans.empty(), "span bound publishes nothing");
		auto stop = std::stop_source{ };
		stop.request_stop();
		expect(refine_cpp_identifiers(text, spans, 100U, stop.get_token()) == syntax_classification_status::cancelled && spans.empty(), "explicit stop publishes nothing");
	}
}

auto main() -> int
try {
	test_real_matcher();
	test_identifier_refinement();
	std::cout << "core-computation-boundaries:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
