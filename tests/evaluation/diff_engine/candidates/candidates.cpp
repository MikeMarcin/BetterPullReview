// [[RFC-BPR-0082]] [[BPR-1466]]. The candidate registry: each candidate's
// name, the part of the engine it replaces, its shape, and its components.

#include "candidates/candidates.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::evaluation {
	namespace {
		inline constexpr auto algorithms = std::array{ git_algorithm::myers, git_algorithm::minimal, git_algorithm::patience, git_algorithm::histogram };

		/// A match list the engine always refuses: a candidate that failed to
		/// produce an answer is refused visibly instead of falling back.
		[[nodiscard]] auto refused_matches() -> std::vector<core::diff_match>
		{
			constexpr auto beyond = std::numeric_limits<std::uint32_t>::max();
			return { { .Left = beyond, .Right = beyond } };
		}

		[[nodiscard]] auto line_count(const core::decoded_text_document* aDocument) -> std::uint32_t
		{
			return aDocument == nullptr ? 0U : static_cast<std::uint32_t>(aDocument->Lines.size());
		}

		[[nodiscard]] auto bytes_of(const core::decoded_text_document* aDocument) -> std::span<const std::uint8_t>
		{
			return aDocument == nullptr ? std::span<const std::uint8_t>{ } : std::span<const std::uint8_t>{ aDocument->Bytes };
		}

		/// P: an installed or pinned Git, run per comparison.
		class process_primary final : public candidate_components
		{
		public:
			process_primary(std::string aGit, const git_algorithm aAlgorithm, const bool aIndent)
				: Git{ std::move(aGit) }
				, Algorithm{ aAlgorithm }
				, Indent{ aIndent }
			{
			}

			[[nodiscard]] auto parts() const noexcept -> core::diff_component_parts override
			{
				return { .Primary = true };
			}

			[[nodiscard]] auto primary_matches(const core::diff_primary_request& aRequest, core::diff_component_meter& aMeter)
				-> std::optional<std::vector<core::diff_match>> override
			{
				++counters().Calls;
				aMeter.tick(1U);
				const auto output = run_git_no_index(Git, bytes_of(aRequest.Left), bytes_of(aRequest.Right), git_diff_arguments(Algorithm, Indent, aRequest.Options));
				aMeter.tick(1U + aRequest.LeftClasses.size() + aRequest.RightClasses.size());
				if (!output.has_value()) {
					++counters().Failures;
					return refused_matches();
				}
				const auto hunks = parse_zero_context_hunks(*output);
				auto       found = hunks.has_value() ? matches_from_hunks(*hunks, line_count(aRequest.Left), line_count(aRequest.Right)) : std::nullopt;
				if (!found.has_value()) {
					++counters().Failures;
					return refused_matches();
				}
				return found;
			}

		private:
			std::string   Git;
			git_algorithm Algorithm = git_algorithm::myers;
			bool          Indent    = false;
		};

		/// L: xdiff, linked dynamically as its own shared library.
		class linked_primary final : public candidate_components
		{
		public:
			linked_primary(const git_algorithm aAlgorithm, const bool aIndent) noexcept
				: Algorithm{ aAlgorithm }
				, Indent{ aIndent }
			{
			}

			[[nodiscard]] auto parts() const noexcept -> core::diff_component_parts override
			{
				return { .Primary = true };
			}

			[[nodiscard]] auto primary_matches(const core::diff_primary_request& aRequest, core::diff_component_meter& aMeter)
				-> std::optional<std::vector<core::diff_match>> override
			{
				++counters().Calls;
				aMeter.tick(1U);
				const auto hunks = xdiff_hunks(aRequest.Left, aRequest.Right, Algorithm, Indent, aRequest.Options);
				aMeter.tick(1U + aRequest.LeftClasses.size() + aRequest.RightClasses.size());
				auto found = hunks.has_value() ? matches_from_hunks(*hunks, line_count(aRequest.Left), line_count(aRequest.Right)) : std::nullopt;
				if (!found.has_value()) {
					++counters().Failures;
					return refused_matches();
				}
				return found;
			}

		private:
			git_algorithm Algorithm = git_algorithm::myers;
			bool          Indent    = false;
		};

		enum class core_primary_kind : std::uint8_t {
			onp,
			histogram,
		};

		/// C: a standard-only matcher over the engine's classes.
		class core_primary final : public candidate_components
		{
		public:
			explicit core_primary(const core_primary_kind aKind) noexcept
				: Kind{ aKind }
			{
			}

			[[nodiscard]] auto parts() const noexcept -> core::diff_component_parts override
			{
				return { .Primary = true };
			}

			[[nodiscard]] auto primary_matches(const core::diff_primary_request& aRequest, core::diff_component_meter& aMeter)
				-> std::optional<std::vector<core::diff_match>> override
			{
				++counters().Calls;
				return Kind == core_primary_kind::onp ? onp_matches(aRequest.LeftClasses, aRequest.RightClasses, &aMeter)
													  : histogram_matches(aRequest.LeftClasses, aRequest.RightClasses, &aMeter);
			}

		private:
			core_primary_kind Kind = core_primary_kind::onp;
		};

		class core_slide final : public candidate_components
		{
		public:
			[[nodiscard]] auto parts() const noexcept -> core::diff_component_parts override
			{
				return { .Slide = true };
			}

			[[nodiscard]] auto slide_matches(const core::diff_primary_request& aRequest, const std::span<const core::diff_match> aMatches,
											 core::diff_component_meter& aMeter) -> std::optional<std::vector<core::diff_match>> override
			{
				++counters().Calls;
				return evaluation::slide_matches(aRequest, aMatches, &aMeter);
			}
		};

		class rapidfuzz_pairing final : public candidate_components
		{
		public:
			[[nodiscard]] auto parts() const noexcept -> core::diff_component_parts override
			{
				return { .Pairing = true };
			}

			[[nodiscard]] auto pair_scores(const core::diff_pairing_request& aRequest, core::diff_component_meter& aMeter)
				-> std::optional<std::vector<std::uint16_t>> override
			{
				++counters().Calls;
				return rapidfuzz_scores(aRequest, &aMeter);
			}
		};

		enum class intraline_kind : std::uint8_t {
			dmp,
			onp,
		};

		class intraline_candidate final : public candidate_components
		{
		public:
			explicit intraline_candidate(const intraline_kind aKind) noexcept
				: Kind{ aKind }
			{
			}

			[[nodiscard]] auto parts() const noexcept -> core::diff_component_parts override
			{
				return { .Intraline = true };
			}

			[[nodiscard]] auto intraline_matches(const std::span<const std::uint32_t> aLeft, const std::span<const std::uint32_t> aRight,
												 core::diff_component_meter& aMeter) -> std::optional<std::vector<core::diff_match>> override
			{
				++counters().Calls;
				if (Kind == intraline_kind::onp) {
					return onp_matches(aLeft, aRight, &aMeter);
				}
				aMeter.tick(1U);
				auto result = dmp_matches(aLeft, aRight);
				aMeter.tick(1U + aLeft.size() + aRight.size());
				return result;
			}

		private:
			intraline_kind Kind = intraline_kind::dmp;
		};

		[[nodiscard]] auto build_descriptions() -> std::vector<candidate_description>
		{
			auto result = std::vector<candidate_description>{
				{ .Name = "baseline", .Component = component_kind::baseline, .Shape = shape_kind::baseline, .Source = "BPR engine at RFC_BASE_SHA" },
			};
			for (const auto& program : git_programs()) {
				for (const auto algorithm : algorithms) {
					for (const auto indent : { false, true }) {
						result.push_back({
							.Name      = "p-" + program.Version + "-" + std::string{ name(algorithm) } + (indent ? "-indent" : ""),
							.Component = indent ? component_kind::slide : component_kind::primary,
							.Shape     = shape_kind::process,
							.Source    = "git " + program.Version,
						});
					}
				}
			}
			for (const auto algorithm : algorithms) {
				for (const auto indent : { false, true }) {
					result.push_back({
						.Name      = "l-" + std::string{ name(algorithm) } + (indent ? "-indent" : ""),
						.Component = indent ? component_kind::slide : component_kind::primary,
						.Shape     = shape_kind::linked,
						.Source    = "xdiff from git 2.56.0",
					});
				}
			}
			result.push_back({ .Name = "c-onp", .Component = component_kind::primary, .Shape = shape_kind::core, .Source = "dtl 1.21" });
			result.push_back({ .Name = "c-histogram", .Component = component_kind::primary, .Shape = shape_kind::core, .Source = "first-party histogram" });
			result.push_back({ .Name = "c-slide", .Component = component_kind::slide, .Shape = shape_kind::core, .Source = "first-party post-pass" });
			result.push_back({ .Name = "c-rapidfuzz", .Component = component_kind::pairing, .Shape = shape_kind::core, .Source = "rapidfuzz-cpp 3.3.4" });
			result.push_back({ .Name = "c-dmp", .Component = component_kind::intraline, .Shape = shape_kind::core, .Source = "diff-match-patch-cpp-stl ba3bbe3" });
			result.push_back({ .Name = "c-onp-intraline", .Component = component_kind::intraline, .Shape = shape_kind::core, .Source = "dtl 1.21" });
			return result;
		}

		[[nodiscard]] auto parse_algorithm(const std::string_view aText) -> std::optional<git_algorithm>
		{
			for (const auto algorithm : algorithms) {
				if (aText == name(algorithm)) {
					return algorithm;
				}
			}
			return std::nullopt;
		}
	} // namespace

	auto git_programs() -> std::vector<git_program>
	{
		return {
			{ .Version = "2.39.5", .Path = BPR_EVAL_GIT_2_39 },
			{ .Version = "2.56.0", .Path = BPR_EVAL_GIT_2_56 },
			{ .Version = "installed", .Path = BPR_EVAL_GIT_INSTALLED },
		};
	}

	auto candidate_descriptions() -> const std::vector<candidate_description>&
	{
		static const auto descriptions = build_descriptions();
		return descriptions;
	}

	auto find_candidate(const std::string_view aName) -> std::optional<candidate_description>
	{
		for (const auto& description : candidate_descriptions()) {
			if (description.Name == aName) {
				return description;
			}
		}
		return std::nullopt;
	}

	auto make_candidate(const std::string_view aName) -> std::unique_ptr<candidate_components>
	{
		const auto description = find_candidate(aName);
		if (!description.has_value() || description->Shape == shape_kind::baseline) {
			return nullptr;
		}
		auto       text   = std::string_view{ description->Name };
		const auto indent = text.ends_with("-indent");
		if (indent) {
			text.remove_suffix(std::string_view{ "-indent" }.size());
		}
		switch (description->Shape) {
		case shape_kind::process: {
			text.remove_prefix(2U);
			const auto dash      = text.rfind('-');
			const auto version   = text.substr(0U, dash);
			const auto algorithm = parse_algorithm(text.substr(dash + 1U));
			for (const auto& program : git_programs()) {
				if (program.Version == version && algorithm.has_value()) {
					return std::make_unique<process_primary>(program.Path, *algorithm, indent);
				}
			}
			return nullptr;
		}
		case shape_kind::linked: {
			text.remove_prefix(2U);
			const auto algorithm = parse_algorithm(text);
			return algorithm.has_value() ? std::make_unique<linked_primary>(*algorithm, indent) : nullptr;
		}
		case shape_kind::core:
			if (aName == "c-onp") {
				return std::make_unique<core_primary>(core_primary_kind::onp);
			}
			if (aName == "c-histogram") {
				return std::make_unique<core_primary>(core_primary_kind::histogram);
			}
			if (aName == "c-slide") {
				return std::make_unique<core_slide>();
			}
			if (aName == "c-rapidfuzz") {
				return std::make_unique<rapidfuzz_pairing>();
			}
			if (aName == "c-dmp") {
				return std::make_unique<intraline_candidate>(intraline_kind::dmp);
			}
			if (aName == "c-onp-intraline") {
				return std::make_unique<intraline_candidate>(intraline_kind::onp);
			}
			return nullptr;
		case shape_kind::baseline:
			return nullptr;
		}
		return nullptr;
	}
} // namespace bpr::evaluation
