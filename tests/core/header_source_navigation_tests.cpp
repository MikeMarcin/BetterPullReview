#include "bpr/core/header_source_navigation.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void supported_paths_keep_exact_stems_and_fold_only_extensions()
	{
		for (const auto suffix : { ".h", ".hh", ".hpp", ".hxx", ".H", ".HH", ".HpP", ".HXX", ".c", ".cc", ".cpp", ".cxx", ".C", ".CC", ".CpP", ".CXX" }) {
			const auto path  = std::string{ "src/Widget.part" } + suffix;
			const auto parts = classify_header_source_path(path);
			expect(parts && parts->Directory == "src" && parts->Stem == "Widget.part", "only the final extension is removed; stem spelling stays exact");
			const auto isHeader = suffix[1] == 'h' || suffix[1] == 'H';
			expect(parts->Family == (isHeader ? header_source_family::header : header_source_family::source), "eight supported extensions classify in both ASCII cases");
		}
		for (const auto path : { ".h", "src/.cpp", "src/widget", "src/widget.cpp.old", "src/widget.m", "src/widget.", "src/", "" }) {
			expect(!classify_header_source_path(path), "unsupported suffix or empty basename is refused");
		}
		const auto root = classify_header_source_path("Widget.cpp");
		expect(root && root->Directory.empty() && root->Stem == "Widget", "root-level paths remain supported");
	}

	void directory_components_rank_the_owners_cross_root_examples()
	{
		auto source = std::string_view{ "src/a/b/c" };
		auto header = std::string_view{ "inc/project/a/b/c" };
		for (const auto expected : { "c", "b", "a" }) {
			expect(take_header_source_directory_component(source) == expected
					   && take_header_source_directory_component(header) == expected,
				   "source and inc/project share the three closest trailing components");
		}
		expect(take_header_source_directory_component(source) == "src" && source.empty()
				   && take_header_source_directory_component(header) == "project",
			   "distinct roots do not become a folder substitution");
		auto other = std::string_view{ "inc/a/b/d" };
		expect(take_header_source_directory_component(other) == "d", "d differs before any ancestor can create a false proximity match");
	}

	void exact_directory_precedes_suffix_and_equal_best_never_picks_arbitrarily()
	{
		const auto first  = file_identity_of("src/file.h");
		const auto second = file_identity_of("inc/file.hpp");
		auto       exact  = header_source_candidates{ };
		auto       suffix = header_source_candidates{ };
		suffix.include(second);
		auto result = resolve_header_source_match(header_source_family::source, exact, suffix);
		expect(result.Status == header_source_status::matched && result.File == second, "sole candidate may have zero shared directory components");
		exact.include(first);
		expect(resolve_header_source_match(header_source_family::source, exact, suffix).File == first, "same directory takes precedence over the closest suffix");
		exact.include(first);
		expect(!exact.Ambiguous && exact.Unique == first, "duplicate identity is not a second candidate");
		exact.include(second);
		result = resolve_header_source_match(header_source_family::source, exact, suffix);
		expect(result.Status == header_source_status::ambiguous && !result.File, "same-directory ambiguity cannot fall through to another group");
		suffix.include(first);
		result = resolve_header_source_match(header_source_family::header, { }, suffix);
		expect(result.Status == header_source_status::ambiguous && !result.File && result.SelectedFamily == header_source_family::header, "equal best suffix candidates leave the view unchanged in either direction");
		expect(resolve_header_source_match(header_source_family::source, { }, { }).Status == header_source_status::no_match, "observed empty group is distinct from unavailability");
	}
}

int main()
{
	supported_paths_keep_exact_stems_and_fold_only_extensions();
	directory_components_rank_the_owners_cross_root_examples();
	exact_directory_precedes_suffix_and_equal_best_never_picks_arbitrarily();
	return EXIT_SUCCESS;
}
