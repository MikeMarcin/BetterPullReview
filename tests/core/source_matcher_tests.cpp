#include "../support/source_regex_engine_fixture.hpp"
#include "../support/source_search_fixture.hpp"
#include "bpr/core/source_matcher.hpp"
#include "product_diff.hpp"
#include <cstdlib>
#include <iostream>
using namespace bpr::core;
using namespace bpr::tests;
using namespace std::string_view_literals;
namespace {
	auto program(std::string_view aQuery, source_search_options aOptions = { }, source_match_limits aLimits = { }) -> source_match_program
	{
		auto compiled = compile_source_match(search_engine(), aQuery, aOptions, aLimits);
		search_expect(compiled.Status == source_search_status::complete && compiled.Program.has_value(), "public query compile");
		return *compiled.Program;
	}
	void expect_spans(std::string_view aQuery, std::string_view aText, const std::vector<byte_span>& aExpected, source_search_options aOptions = { })
	{
		const auto found = scan_source_match(program(aQuery, aOptions), aText);
		if (found.Status != source_search_status::complete || found.Matches != aExpected) {
			std::cerr << "span mismatch for " << aQuery << ": ";
			for (const auto& span : found.Matches) {
				std::cerr << span.Begin << ',' << span.End << ' ';
			}
			std::cerr << '\n';
			std::exit(1);
		}
	}
	void test_public_matching()
	{
		expect_spans("CAFÉ", "café CAFÉ", { { 0U, 5U }, { 6U, 11U } });
		expect_spans("Σ", "Σσς", { { 0U, 2U }, { 2U, 4U }, { 4U, 6U } });
		expect_spans("k", "K K k", { { 0U, 3U }, { 4U, 5U }, { 6U, 7U } });
		expect_spans("ss", "ß ss ẞ", { { 3U, 5U } });
		// [[BPR-1432]]. CRLF reads as LF; a lone CR is content, as Git counts lines.
		expect_spans("a\nb", "a\r\nb a\rb a\nb", { { 0U, 4U }, { 9U, 12U } });
		expect_spans("a\rb", "a\r\nb a\rb", { { 5U, 8U } });
		expect_spans("aba", "ababa", { { 0U, 3U } });
		expect_spans("cat", "cat 猫cat cat́ cat_ cat2 cat", { { 0U, 3U }, { 27U, 30U } }, { .WholeWord = true });
		// [[BPR-1431]]. A refused earlier start must not hide a later whole word
		// (the hand-written engine returned no match here).
		expect_spans("a.*", "ba a", { { 3U, 4U } }, { .WholeWord = true, .RegularExpression = true });
		expect_spans("TODO.*", "xTODO; TODO fix", { { 7U, 15U } }, { .WholeWord = true, .RegularExpression = true });
		expect_spans("aa", "aaa aa", { { 4U, 6U } }, { .WholeWord = true });
		const auto regex = source_search_options{ .RegularExpression = true };
		expect_spans("a+", "baaaacaa", { { 1U, 5U }, { 6U, 8U } }, regex);
		expect_spans("a+?", "aaa", { { 0U, 1U }, { 1U, 2U }, { 2U, 3U } }, regex);
		expect_spans("a|ab", "ab", { { 0U, 1U } }, regex);
		expect_spans("ab|a", "ab", { { 0U, 2U } }, regex);
		expect_spans("(ab){2,3}", "ababab abab", { { 0U, 6U }, { 7U, 11U } }, regex);
		expect_spans("^x$", "x\r\nx\nx", { { 0U, 1U }, { 3U, 4U }, { 5U, 6U } }, regex);
		expect_spans("(?-m)^x$", "x\nx", { }, regex);
		expect_spans("a.b", "a\nb a b", { { 4U, 7U } }, regex);
		expect_spans("(?s)a.b", "a\r\nb a b", { { 0U, 4U }, { 5U, 8U } }, regex);
		expect_spans("[K]", "K k K", { { 0U, 3U }, { 4U, 5U }, { 6U, 7U } }, regex);
		expect_spans("\\p{L}+", "猫Σ9😀", { { 0U, 5U } }, regex);
		expect_spans("(?:)", "😀a", { { 0U, 0U }, { 4U, 4U }, { 5U, 5U } }, regex);
		expect_spans("a*", "aa", { { 0U, 2U }, { 2U, 2U } }, regex);
		expect_spans("(a*)*b", "aaab", { { 0U, 4U } }, regex);
		expect_spans("\\bcat\\b", "猫cat😀", { { 3U, 6U } }, regex);
	}

	class counted_stop final : public diff_compute_stop
	{
	public:
		std::uint64_t At   = 1024U;
		std::uint64_t Last = 0U;
		auto          stop_requested(std::uint64_t aSteps) -> bool override
		{
			Last = aSteps;
			return aSteps >= At;
		}
	};
	void test_limits_and_invalid_input()
	{
		const auto regex = source_search_options{ .RegularExpression = true };
		for (const auto query : { "[", "(a", "a{1001}", "[z-a]", "a++", "\\q", "(?:a{100}){100}" }) {
			search_expect(compile_source_match(search_engine(), query, regex).Status == source_search_status::invalid_pattern, "invalid syntax refused");
		}
		// [[BPR-1431]]. Lookaround, backreferences and the single-byte \C stay
		// refused before RE2 sees them; brackets and \Q...\E quote them.
		for (const auto query : { "(?=x)", "(?!x)", "(?<=x)", "(?<!x)", "(x)\\1", "a\\Cb", "[\\C]" }) {
			search_expect(compile_source_match(search_engine(), query, regex).Status == source_search_status::unsupported_pattern, "unsupported syntax refused");
		}
		search_expect(compile_source_match(search_engine(), "x(?", regex).ErrorOffset == 1U, "error offset names the query, not the line-anchor prefix");
		expect_spans("[(?=]", "(?=", { { 0U, 1U }, { 1U, 2U }, { 2U, 3U } }, regex);
		expect_spans("\\Q(?=\\E", "a(?=b", { { 1U, 4U } }, regex);
		expect_spans("\\p{Greek}+", "abc \u03B1\u03B2\u03B3", { { 4U, 10U } }, regex);
		expect_spans("[[:alpha:]]+", "ab1", { { 0U, 2U } }, regex);
		search_expect(compile_source_match(search_engine(), "").Status == source_search_status::idle, "empty compiles no program");
		search_expect(compile_source_match(search_engine(), std::string(4097U, 'x')).Status == source_search_status::invalid_input, "4 KiB query ceiling");
		search_expect(compile_source_match(search_engine(), "\xC0\xAF").Status == source_search_status::invalid_input, "invalid query scalar");
		expect_spans("a\r\nb", "a\nb", { { 0U, 3U } }, regex);
		expect_spans("a\r\nb", "a\rb", { });
		expect_spans("(?i:K)(?-i:k)", "Kk Kk kk KK", { { 0U, 4U }, { 5U, 7U }, { 8U, 10U } }, regex);
		expect_spans("é", "é", { });
		expect_spans("𐐀", "𐐨", { { 0U, 4U } });
		const auto bounded = program("cat", { .WholeWord = true });
		search_expect(scan_source_match(bounded, "scat", byte_span{ 1U, 4U }).Matches.empty(), "selection edges do not invent word boundaries");
		search_expect(scan_source_match(program("x"), "😀x", byte_span{ 1U, 5U }).Status == source_search_status::invalid_input, "split scalar bounds refused");
		search_expect(scan_source_match(program("x"), "a\r\nx", byte_span{ 2U, 4U }).Status == source_search_status::invalid_input, "split CRLF bounds refused");
		search_expect(scan_source_match(program("x"), "x\0x"sv).Status == source_search_status::invalid_input, "binary source refused");
		auto limits                = source_match_limits{ };
		limits.MaximumProgramBytes = 64U * 1024U;
		search_expect(compile_source_match(search_engine(), "(?:abcdefghij){1000}", regex, limits).Status == source_search_status::resource_limit, "engine program memory bounded");
		limits                     = { };
		limits.MaximumCompileSteps = 20U;
		search_expect(compile_source_match(search_engine(), std::string(30U, 'x'), { }, limits).Status == source_search_status::resource_limit, "query admission work bounded");
		const auto adversarial = scan_source_match(program("(a|aa)*b", regex), std::string(100000U, 'a'));
		search_expect(adversarial.Status == source_search_status::complete && adversarial.Matches.empty(), "ambiguous regex runs in linear time without backtracking");
		limits                  = { };
		limits.MaximumScanSteps = 100U;
		search_expect(scan_source_match(program("x", { }, limits), std::string(1500U, 'a')).Status == source_search_status::resource_limit, "scan work bounded");
		limits                   = { };
		limits.MaximumOwnedBytes = 32U;
		search_expect(compile_source_match(search_engine(), "x", { }, limits).Status == source_search_status::resource_limit, "compile memory reserved before allocation");
		limits              = { };
		limits.MaximumBytes = 10U;
		search_expect(scan_source_match(program("x", { }, limits), std::string(11U, 'x')).Status == source_search_status::resource_limit, "file bytes ceiling");
		limits              = { };
		limits.MaximumLines = 2U;
		search_expect(scan_source_match(program("x", { }, limits), "x\nx\nx").Status == source_search_status::resource_limit, "file lines ceiling");
		limits                = { };
		limits.MaximumMatches = 2U;
		const auto capped     = scan_source_match(program("x", { }, limits), "xxx");
		search_expect(capped.Status == source_search_status::result_limit && capped.Matches.size() == 2U && capped.AdditionalMatchObserved, "extra observed match qualifies cap");
		const auto exact = scan_source_match(program("x", { }, limits), "xx");
		search_expect(exact.Status == source_search_status::complete && !exact.AdditionalMatchObserved, "exact cap is not invented plus");
		counted_stop stop;
		auto         cancelled = scan_source_match(program("needle"), std::string(10000U, 'a'), { }, &stop);
		search_expect(cancelled.Status == source_search_status::cancelled && cancelled.Steps == 1024U && stop.Last == 1024U, "scan cancellation within counted interval");
		stop.At                   = 1501U;
		const auto cancelledRegex = scan_source_match(program("(a|aa)*b", regex), std::string(1500U, 'a'), { }, &stop);
		search_expect(cancelledRegex.Status == source_search_status::cancelled && cancelledRegex.Steps == 1501U, "each engine search polls cancellation first");
		stop.At = 0U;
		search_expect(compile_source_match(search_engine(), "x", { }, { }, &stop).Status == source_search_status::cancelled, "compile initial cancellation");
		stop.At = 1024U;
		search_expect(compile_source_match(search_engine(), std::string(3000U, 'x'), { }, { }, &stop).Status == source_search_status::cancelled, "compile cancellation counted");
		const auto preview = source_match_preview(std::string(1000U, 'a'), { 500U, 501U });
		search_expect(preview.size() == 240U, "preview scalar ceiling");
		auto astral = std::string{ };
		for (auto index = 0U; index < 1000U; ++index) {
			astral += "😀";
		}
		search_expect(source_match_preview(astral, { 2000U, 2004U }).size() == 960U, "preview preserves whole four-byte scalars");
		const auto line = std::string{ "first\n  const auto needle = 1;\n" };
		const auto span = source_match_preview_span(line, { 19U, 25U });
		search_expect(source_match_preview(line, { 19U, 25U }).substr(static_cast<std::size_t>(span.Begin), static_cast<std::size_t>(span.End - span.Begin)) == "needle", "preview span names the exact match bytes");
		search_expect(source_match_preview_span(std::string(1000U, 'a'), { 500U, 900U }) == byte_span{ 80U, 240U }, "preview span clips at the preview end");
		search_expect(source_match_preview_span(astral, { 2001U, 2004U }).empty(), "malformed span has no preview span");
		constexpr auto leftOnly  = source_side_filter{ .Left = true, .Right = false };
		constexpr auto rightOnly = source_side_filter{ .Left = false, .Right = true };
		auto           record    = source_match_record{ };
		record.Locations[0]      = source_match_location{ };
		search_expect(shown(record, leftOnly) && shown(record, source_side_filter{ }) && !shown(record, rightOnly), "side filter keeps a left-only record off Right only");
		record.Locations[1] = source_match_location{ .Side = diff_side::right };
		search_expect(shown(record, rightOnly) && shown(record, leftOnly), "a both-sides record shows under either single side");
		search_expect(toggled(source_side_filter{ }, diff_side::left) == rightOnly && toggled(rightOnly, diff_side::left) == source_side_filter{ }, "a side toggles independently");
		search_expect(toggled(rightOnly, diff_side::right) == rightOnly, "the last enabled side stays on");
	}
	void test_globs()
	{
		const auto filter = compile_source_paths(search_engine(), "*.{cpp,h},src/[a,b]?.txt,./root\\,file", "**/generated/**,*.tmp");
		search_expect(filter.Status == source_search_status::complete && filter.Filter, "glob parses braces classes commas and escape");
		search_expect(filter.Filter->includes("deep/name.cpp", { }), "slashless basename at depth");
		search_expect(filter.Filter->includes("old/name.c", "new/name.h"), "either rename side included");
		search_expect(!filter.Filter->includes("old/a.h", "new/a.tmp"), "exclude wins from either side");
		search_expect(!filter.Filter->includes("src/generated/a.cpp", { }), "globstar spans directories");
		search_expect(filter.Filter->includes("src/a1.txt", { }), "comma inside class not alternative");
		search_expect(filter.Filter->includes("root,file", { }), "escaped comma is literal root name");
		search_expect(!filter.Filter->includes("sub/root,file", { }), "dot slash anchors root");
		search_expect(!filter.Filter->includes("deep/name.CPP", { }), "paths case-sensitive");
		search_expect(compile_source_paths(search_engine(), "**/a?.{h,{c,cpp}}", "").Filter->includes("a猫.cpp", { }), "nested brace and Unicode wildcard");
		search_expect(compile_source_paths(search_engine(), "", "").Filter->includes({ }, "unopened/anything"), "empty filters include all");
		search_expect(!compile_source_paths(search_engine(), "src/*.h", "").Filter->includes("src/deep/a.h", { }), "star never crosses slash");
		search_expect(compile_source_paths(search_engine(), "src/**/a.h", "").Filter->includes("src/a.h", { }), "globstar zero directory");
		auto pathLimits             = source_match_limits{ };
		pathLimits.MaximumScanSteps = 8U;
		const auto pathLimited      = compile_source_paths(search_engine(), "*.cpp", "", pathLimits);
		search_expect(pathLimited.Filter && pathLimited.Filter->evaluate("long-directory/name.cpp", { }).Status == source_search_status::resource_limit, "path work refusal stays explicit for corpus coverage");
		for (const auto text : { "a,", "{a,}", "[a", "{a,b", "a\\", "[z-a]", "a,,b" }) {
			search_expect(compile_source_paths(search_engine(), text, "").Status == source_search_status::invalid_pattern, "invalid glob refused before reads");
		}
	}
	auto unified(std::string_view aLeft, std::string_view aRight, diff_options aOptions = { }) -> diff_semantic_index
	{
		const auto scope    = search_scope();
		auto       key      = file_comparison_key{ };
		key.Authority       = scope.Authority;
		key.Repository      = scope.Authority.Repository;
		key.PullRequest     = scope.Authority.PullRequest;
		key.LeftAnchor      = scope.Comparison.Left.Identity;
		key.RightAnchor     = scope.Comparison.Right.Identity;
		key.LeftBlob        = scope.Document->LeftBlob;
		key.RightBlob       = scope.Document->RightBlob;
		key.File            = scope.Document->File;
		key.Options         = *options_digest(aOptions);
		key.Algorithm       = supported_diff_algorithm_revision;
		const auto bytes    = [](std::string_view aText) { return std::span{ reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() }; };
		const auto computed = bpr::tests::product_file_diff({ key, aOptions, bytes(aLeft), bytes(aRight), { } });
		search_expect(computed.Result.has_value(), "real source comparison computed");
		auto index = build_diff_semantic_index(std::make_shared<const computed_file_diff>(*computed.Result));
		search_expect(index.has_value(), "existing semantic index proves immutable source plan");
		return *index;
	}
	void test_unified_and_identity()
	{
		const auto index = unified("same\nold\r\nend\n", "same\nnew\nend\n");
		auto       entry = source_match_entry{ };
		entry.File       = index.diff().Alignment.Key.File;
		entry.Target     = source_search_target::unified;
		entry.Unified    = &index;
		const auto same  = match_source_entry(program("same"), entry);
		search_expect(same.Status == source_search_status::complete && same.Matches.size() == 1U && same.Matches[0].Locations[0] && same.Matches[0].Locations[1], "unchanged hit appears once with exact two-side locations");
		search_expect(match_source_entry(program("old\nnew"), entry).Matches.empty(), "visual removed/added adjacency cannot fabricate multiline match");
		const auto left = match_source_entry(program("same\nold"), entry);
		search_expect(left.Matches.size() == 1U && left.Matches[0].Locations[0]->Span == byte_span{ 0U, 8U } && !left.Matches[0].Locations[1], "multiline left actual version mapped to CRLF bytes");
		const auto right = match_source_entry(program("new\nend"), entry);
		search_expect(right.Matches.size() == 1U && !right.Matches[0].Locations[0] && right.Matches[0].Locations[1], "multiline right actual version");
		const auto lines = match_source_entry(program("(?m)^", { .RegularExpression = true }), entry);
		search_expect(lines.Status == source_search_status::complete && lines.Matches.size() == 5U, "unified order and zero-width unchanged dedup including terminal");
		const auto ignored = unified("hidden  text\nold\n", "hidden text\nnew\n", { .Whitespace = whitespace_policy::ignore_all });
		entry.Unified      = &ignored;
		search_expect(match_source_entry(program("hidden  text"), entry).Matches.empty(), "unified never searches left text hidden by an equal row projected from right");
		search_expect(match_source_entry(program("hidden  text\nold"), entry).Matches.empty(), "multiline plan cannot bridge an unrepresented source line");
		entry.Unified        = &index;
		const auto scope     = search_scope();
		entry                = { };
		entry.File           = scope.Document->File;
		entry.Left           = source_match_side{ *scope.Document->LeftBlob, "same same" };
		entry.Right          = source_match_side{ *scope.Document->LeftBlob, "same same" };
		const auto identical = match_source_entry(program("same"), entry);
		search_expect(identical.Matches.size() == 2U && identical.ScannedSides == 1U && identical.Matches[0].Locations[1], "same-entry same-blob scans once and binds both sides");
		entry.Right->Blob = *scope.Document->RightBlob;
		search_expect(match_source_entry(program("same"), entry).Matches.size() == 4U, "different blobs stay separate despite equal previews");
		entry.Target = source_search_target::left;
		search_expect(match_source_entry(program("same"), entry).Matches.size() == 2U, "split pane scans own whole source only");
		entry.Selection     = std::pair{ diff_side::left, byte_span{ 5U, 9U } };
		const auto selected = match_source_entry(program("same"), entry);
		search_expect(selected.Matches.size() == 1U && selected.Matches[0].Locations[0]->Span == byte_span{ 5U, 9U }, "fixed bounds search exact captured range");
		auto limits               = source_match_limits{ };
		limits.MaximumResultBytes = sizeof(source_match_record) + 1024U;
		entry.Selection.reset();
		const auto resource = match_source_entry(program("same", { }, limits), entry);
		search_expect(resource.Status == source_search_status::resource_limit && resource.Matches.size() == 1U, "preview and record memory cap keeps honest usable prefix");
		search_expect(identical.PeakOwnedBytes <= source_match_limits{ }.MaximumOwnedBytes, "entry owned memory bounded");
		const auto large = std::string(1024U * 1024U, 'a') + "needle";
		entry.Selection.reset();
		entry.Left->Bytes = large;
		const auto warm   = match_source_entry(program("needle"), entry);
		search_expect(warm.Status == source_search_status::complete && warm.Matches.size() == 1U && warm.Steps < 4U * 1024U * 1024U, "large-line column conversion does bounded local work");
		std::cout << "source_matcher: state_bytes=" << sizeof(source_search_state) << " large_bytes=" << large.size() << " steps=" << warm.Steps << " peak_owned_bound=" << warm.PeakOwnedBytes << '\n';
	}
}

int main(int aCount, char** aArguments)
{
	if (aCount == 2 && std::string_view{ aArguments[1] } == "--reference") {
		auto       line  = std::string{ };
		const auto unhex = [](std::string_view aHex) {
			auto       result = std::string{ };
			const auto digit  = [](char aValue) { return static_cast<unsigned int>(aValue <= '9' ? aValue - '0' : aValue - 'a' + 10); };
			for (auto index = std::size_t{ 0U }; index < aHex.size(); index += 2U) {
				result += static_cast<char>((digit(aHex[index]) << 4U) | digit(aHex[index + 1U]));
			}
			return result;
		};
		while (std::getline(std::cin, line)) {
			const auto split  = line.find(':');
			const auto query  = unhex(std::string_view{ line }.substr(0U, split));
			const auto text   = unhex(std::string_view{ line }.substr(split + 1U));
			const auto result = scan_source_match(program(query, { .MatchCase = true, .RegularExpression = true }), text);
			std::cout << static_cast<unsigned int>(result.Status);
			for (const auto& span : result.Matches) {
				std::cout << ' ' << span.Begin << ',' << span.End;
			}
			std::cout << '\n';
		}
		return 0;
	}
	test_public_matching();
	test_limits_and_invalid_input();
	test_globs();
	test_unified_and_identity();
}
