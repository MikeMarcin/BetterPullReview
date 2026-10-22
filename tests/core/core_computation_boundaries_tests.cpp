#include "../support/test_expectation.hpp"
#include "bpr/core/cpp_identifier_context.hpp"
#include "bpr/core/diff_line_matcher.hpp"
#include "bpr/core/known_namespaces.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stop_token>
#include <string>
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

	/// [[BPR-1710]]: namespace qualifiers are decided in core, without Qt.
	void test_namespace_qualifiers()
	{
		constexpr auto text  = std::string_view{ "using namespace outer::inner; auto a = std::ranges::find(x); auto b = outer::inner::make(); auto c = Foo::bar(); auto d = ::std::size_t{ };" };
		auto           spans = token_span_list{ };
		expect(refine_cpp_identifiers(text, spans, 100U, { }) == syntax_classification_status::available, "namespace refinement is available without Qt");
		expect(role_at(spans, text.find("std")) == appearance_role::syntax_namespace, "std qualifies as the standard library namespace");
		expect(role_at(spans, text.find("ranges")) == appearance_role::syntax_namespace, "a namespace the standard nests in std is a namespace");
		expect(role_at(spans, text.find("find")) == appearance_role::syntax_normal, "the name after the namespaces keeps its own role");
		expect(role_at(spans, text.rfind("outer")) == appearance_role::syntax_namespace && role_at(spans, text.rfind("inner")) == appearance_role::syntax_namespace, "a using-directive's segments qualify as namespaces elsewhere");
		expect(role_at(spans, text.find("Foo")) == appearance_role::syntax_type, "an undeclared qualifier keeps the generic scope role");
		expect(role_at(spans, text.rfind("std")) == appearance_role::syntax_namespace, "a globally qualified ::std is the namespace");
		constexpr auto shadowed = std::string_view{ "namespace lib { struct std { static int x; }; auto v = std::x; }" };
		auto           hidden   = token_span_list{ };
		expect(refine_cpp_identifiers(shadowed, hidden, 100U, { }) == syntax_classification_status::available && role_at(hidden, shadowed.rfind("std")) == appearance_role::syntax_type, "a class the blob names std hides the namespace in its scope");
		// [[BPR-1711]]: a trailing return type after `->`, and a qualified class head that declares nothing.
		constexpr auto trailing = std::string_view{ "template <> struct std::hash<int> { }; auto f() -> std::size_t; auto g = p->field;" };
		auto           returned = token_span_list{ };
		expect(refine_cpp_identifiers(trailing, returned, 100U, { }) == syntax_classification_status::available, "trailing return refinement is available");
		expect(role_at(returned, trailing.find("std")) == appearance_role::syntax_namespace, "a specialization's std qualifier is the namespace");
		expect(role_at(returned, trailing.rfind("std")) == appearance_role::syntax_namespace, "std after -> is the namespace");
		expect(role_at(returned, trailing.find("field")) == appearance_role::syntax_normal, "unqualified member access is unchanged");
	}

	/// [[BPR-1714]]: the known-namespace list is an explicit input to the
	/// refinement, decided in core without Qt or a grammar.
	void test_known_namespaces_classify()
	{
		constexpr auto text = std::string_view{
			"auto a = std::chrono::seconds{ 1 }; auto b = Qt::AlignLeft; auto c = boost::asio::ip::tcp::v4(); "
			"auto d = boost::filesystem::path{ }; auto e = mylib::make();"
		};
		const auto classify = [&](const known_namespaces& aNamespaces) {
			auto spans = token_span_list{ };
			expect(refine_cpp_identifiers(text, spans, 1'000U, { }, aNamespaces) == syntax_classification_status::available,
				   "the refinement answers under any list");
			return spans;
		};
		const auto at = [&](const token_span_list& aSpans, const std::string_view aWord) {
			return role_at(aSpans, text.find(aWord));
		};
		constexpr auto ns = appearance_role::syntax_namespace;

		const auto none = classify(known_namespaces{ .Names = { } });
		expect(at(none, "std") == appearance_role::syntax_type && at(none, "chrono") == appearance_role::syntax_type,
			   "an empty list predeclares nothing: std is a scope like any other");
		expect(at(none, "Qt") == appearance_role::syntax_type && at(none, "boost") == appearance_role::syntax_type,
			   "an empty list names no other namespace");

		const auto standard = classify(known_namespaces{ });
		expect(standard == classify(known_namespaces{ .Names = { "std" } }), "the default list is std alone");
		auto implicit = token_span_list{ };
		expect(refine_cpp_identifiers(text, implicit, 1'000U, { }) == syntax_classification_status::available && implicit == standard,
			   "a caller that names no list classifies under the default");
		expect(at(standard, "std") == ns && at(standard, "chrono") == ns, "std brings the namespaces the standard nests in it");
		expect(at(standard, "Qt") == appearance_role::syntax_type && at(standard, "boost") == appearance_role::syntax_type,
			   "the default names nothing but std");

		const auto added = classify(known_namespaces{ .Names = { "std", "Qt", "boost::asio" } });
		expect(at(added, "std") == ns && at(added, "chrono") == ns, "std keeps its nested namespaces beside the reviewer's own");
		expect(at(added, "Qt") == ns, "a listed top-level name is a namespace");
		expect(at(added, "boost") == ns && at(added, "asio") == ns, "every segment of a nested entry is a namespace");
		expect(at(added, "ip") == appearance_role::syntax_type, "a scope under a listed namespace that the list does not name keeps the scope role");
		expect(role_at(added, text.find("filesystem")) == appearance_role::syntax_type,
			   "std's nested names belong to std: boost::filesystem is not predeclared");
		expect(at(added, "mylib") == appearance_role::syntax_type, "an unlisted qualifier is unchanged");

		expect(classify(known_namespaces{ .Names = { "boost::asio", "Qt", "std" } }) == added
				   && classify(known_namespaces{ .Names = { "Qt", "boost", "boost::asio", "std" } }) == added,
			   "equal sets of namespaces yield equal spans whatever their order");
		expect(classify(known_namespaces{ .Names = { "std", "Qt", "boost::asio" } }) == added, "an equal list yields equal spans");

		const auto refused = classify(known_namespaces{ .Names = { "3d", "my-lib", "int", "boost::", "::Qt", "Qt" } });
		expect(at(refused, "Qt") == ns && at(refused, "std") == appearance_role::syntax_type,
			   "refused entries are ignored and the legal one still applies");

		constexpr auto shadowed = std::string_view{ "struct Qt { static int x; }; auto v = Qt::x;" };
		auto           hidden   = token_span_list{ };
		expect(refine_cpp_identifiers(shadowed, hidden, 100U, { }, known_namespaces{ .Names = { "Qt" } }) == syntax_classification_status::available
				   && role_at(hidden, shadowed.rfind("Qt")) == appearance_role::syntax_type,
			   "a class the blob declares wins over a listed namespace of that name");
		constexpr auto enumerated = std::string_view{ "enum class Qt { AlignLeft }; auto v = Qt::AlignLeft; auto w = std::chrono::seconds{ };" };
		auto           members    = token_span_list{ };
		expect(refine_cpp_identifiers(enumerated, members, 100U, { }, known_namespaces{ .Names = { "Qt", "std" } }) == syntax_classification_status::available
				   && role_at(members, enumerated.rfind("Qt")) == appearance_role::syntax_type
				   && role_at(members, enumerated.rfind("AlignLeft")) == appearance_role::syntax_enum_member
				   && role_at(members, enumerated.find("chrono")) == ns,
			   "an enum the blob declares in the global scope replaces the listed name there, its enumerators resolve, "
			   "and the other listed names stay");
	}

	/// [[BPR-1714]]: what the settings field and the store accept.
	void test_known_namespace_rules()
	{
		using problem = known_namespace_problem;
		expect(known_namespaces{ }.Names == std::vector<std::string>{ "std" }, "the default setting is std alone");
		expect(!check_known_namespace("std") && !check_known_namespace("boost::asio") && !check_known_namespace("_detail")
				   && !check_known_namespace("v2") && !check_known_namespace("final") && !check_known_namespace("module"),
			   "identifiers, nested paths and contextual names are namespace names");
		expect(check_known_namespace("") == problem::empty_part && check_known_namespace("::boost") == problem::empty_part
				   && check_known_namespace("boost::") == problem::empty_part && check_known_namespace("a::::b") == problem::empty_part,
			   "every part around :: must be there");
		expect(check_known_namespace("my-lib") == problem::invalid_character && check_known_namespace("a:b") == problem::invalid_character
				   && check_known_namespace("na\xC3\xAFve") == problem::invalid_character,
			   "only ASCII letters, digits, underscores and :: are names");
		expect(check_known_namespace("3d") == problem::leading_digit && check_known_namespace("boost::2") == problem::leading_digit,
			   "no part starts with a digit");
		expect(check_known_namespace("int") == problem::keyword && check_known_namespace("boost::class") == problem::keyword
				   && check_known_namespace("and") == problem::keyword,
			   "no part is a keyword or alternative token");
		expect(check_known_namespace(std::string(maximum_known_namespace_bytes, 'a')) == std::nullopt
				   && check_known_namespace(std::string(maximum_known_namespace_bytes + 1U, 'a')) == problem::too_long,
			   "an entry is bounded");

		const auto typed = split_known_namespaces(" std, Qt\tboost::asio ,,3d\nQt ");
		expect(typed.Names == std::vector<std::string>{ "std", "Qt", "boost::asio", "3d", "Qt" },
			   "commas and white space separate entries and empty ones are dropped");
		expect(validate_known_namespaces(typed)
				   == std::vector<known_namespace_rejection>{ { .Index = 3U, .Problem = problem::leading_digit }, { .Index = 4U, .Problem = problem::duplicate } },
			   "every refused entry is named by its position");
		expect(validate_known_namespaces(known_namespaces{ .Names = { } }).empty(), "an empty list is legal");
		auto many = known_namespaces{ .Names = { } };
		for (auto index = std::size_t{ 0U }; index <= maximum_known_namespaces; ++index) {
			many.Names.push_back("n" + std::to_string(index));
		}
		expect(validate_known_namespaces(many) == std::vector<known_namespace_rejection>{ { .Index = maximum_known_namespaces, .Problem = problem::too_many } },
			   "the list is bounded");
		expect(split_known_namespaces("boost :: asio").Names == std::vector<std::string>{ "boost", "::", "asio" }
				   && check_known_namespace("::") == problem::empty_part,
			   "a spaced :: is refused rather than silently joined");

		const auto legal = known_namespaces{ .Names = { "std", "Qt", "boost::asio" } };
		expect(join_known_namespaces(legal) == "std, Qt, boost::asio", "the field shows the list joined by commas");
		expect(split_known_namespaces(join_known_namespaces(legal)) == legal, "joining and splitting round-trip a legal list");
		expect(join_known_namespaces(known_namespaces{ .Names = { } }).empty(), "an empty list is an empty field");
		expect(canonical_known_namespaces(known_namespaces{ .Names = { "std", "3d", "Qt", "std" } }).Names == std::vector<std::string>{ "Qt", "std" },
			   "the canonical list is the legal entries, sorted and unique");
	}
}

auto main() -> int
try {
	test_real_matcher();
	test_identifier_refinement();
	test_namespace_qualifiers();
	test_known_namespaces_classify();
	test_known_namespace_rules();
	std::cout << "core-computation-boundaries:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
