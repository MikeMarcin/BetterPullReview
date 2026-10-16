// [[BPR-1465]] The diff engine's component seam: supplied primary matches,
// pair scores, intraline matches and a post-pass over the primary matches. The
// engine validates every supplied value and refuses what breaks its rules.
// [[BPR-1485]] The primary matches are required; the product's come from the
// linked xdiff line matcher, which components here use when they supply
// another part.

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_compute_components.hpp"
#include "product_diff.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr::core;
	using bpr::tests::product_file_diff;
	using bpr::tests::product_material_diff;

	/// [[BPR-1485]] The product's primary matches, for a component that
	/// supplies another part.
	[[nodiscard]] auto product_primary(const diff_primary_request& aRequest, diff_component_meter& aMeter)
		-> std::optional<std::vector<diff_match>>
	{
		auto matching = bpr::application::diff_line_matching{ *bpr::tests::product_line_matcher() };
		return matching.primary_matches(aRequest, aMeter);
	}

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed) -> git_blob_oid
	{
		std::array<std::uint8_t, 32> bytes{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + static_cast<std::uint8_t>(index));
		}
		return git_blob_oid{ git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto comparison_key(const diff_options& aOptions) -> file_comparison_key
	{
		const auto digest = options_digest(aOptions);
		expect(digest.has_value(), "test options are normalized");
		const auto scope       = opaque<pull_request_authority_scope_id>(11U);
		const auto incarnation = opaque<epoch_catalog_incarnation_id>(12U);
		const auto authority   = pull_request_authority{
			.Profile         = host_profile_id{ 10U },
			.ProfileRevision = github_profile_revision{ 20U },
			.Host            = git_host_id{ 30U },
			.HostAuthority   = opaque<host_authority_fingerprint>(13U),
			.Account         = github_account_id{ 40U },
			.Viewer          = github_viewer_actor_id{ 50U },
			.Repository      = repository_id{ 101U },
			.Cache           = opaque<repository_cache_scope_id>(14U),
			.PullRequest     = pull_request_id{ 202U },
			.Number          = pull_request_number{ 17U },
			.Scope           = scope,
		};
		return file_comparison_key{
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = comparison_anchor_identity{
				.Authority   = scope,
				.Incarnation = incarnation,
				.Kind        = comparison_anchor_kind::epoch_base,
				.Epoch       = epoch_ordinal{ 1 },
			},
			.RightAnchor = comparison_anchor_identity{
				.Authority   = scope,
				.Incarnation = incarnation,
				.Kind        = comparison_anchor_kind::epoch_head,
				.Epoch       = epoch_ordinal{ 2 },
			},
			.LeftBlob  = blob(21U),
			.RightBlob = blob(51U),
			.File      = opaque<file_identity>(31U),
			.Options   = *digest,
			.Algorithm = diff_algorithm_revision{ 1U },
		};
	}

	[[nodiscard]] auto copy_bytes(const std::string_view aText) -> std::vector<std::uint8_t>
	{
		return { aText.begin(), aText.end() };
	}

	/// One comparison, with the storage its input points into.
	struct test_case
	{
		std::vector<std::uint8_t> Left;
		std::vector<std::uint8_t> Right;
		diff_options              Options;
		diff_compute_limits       Limits;

		[[nodiscard]] auto input() const -> diff_compute_input
		{
			return {
				.Key        = comparison_key(Options),
				.Options    = Options,
				.LeftBytes  = std::span<const std::uint8_t>{ Left },
				.RightBytes = std::span<const std::uint8_t>{ Right },
				.Limits     = Limits,
			};
		}

		[[nodiscard]] auto material_input() const -> material_diff_input
		{
			const auto digest = options_digest(Options);
			return {
				.Key = material_comparison_key{
					.Comparison   = opaque<file_comparison_digest>(61U),
					.LeftPresent  = true,
					.RightPresent = true,
					.Options      = *digest,
					.Algorithm    = supported_diff_algorithm_revision,
				},
				.Options    = Options,
				.LeftBytes  = std::span<const std::uint8_t>{ Left },
				.RightBytes = std::span<const std::uint8_t>{ Right },
				.Limits     = Limits,
			};
		}
	};

	[[nodiscard]] auto make_case(const std::string_view aLeft, const std::string_view aRight, const diff_options& aOptions = { })
		-> test_case
	{
		return { .Left = copy_bytes(aLeft), .Right = copy_bytes(aRight), .Options = aOptions };
	}

	[[nodiscard]] auto text_result(const diff_compute_outcome& aOutcome, const std::string_view aMessage)
		-> computed_file_diff
	{
		expect(aOutcome.Reason == diff_reason_code::none && aOutcome.Result.has_value(), aMessage);
		expect(validate_computed_file_diff(*aOutcome.Result).valid(), aMessage);
		return *aOutcome.Result;
	}

	/// The primary matches of a result computed without moves: every line of
	/// its equal segments.
	[[nodiscard]] auto equal_matches(const computed_file_diff& aDiff) -> std::vector<diff_match>
	{
		auto result = std::vector<diff_match>{ };
		for (const auto& segment : aDiff.Alignment.Segments) {
			if (segment.Kind != alignment_kind::equal) {
				continue;
			}
			for (std::uint32_t offset = 0U; offset < segment.Left.extent(); ++offset) {
				result.push_back({ .Left = segment.Left.Begin + offset, .Right = segment.Right.Begin + offset });
			}
		}
		return result;
	}

	[[nodiscard]] auto sample_cases() -> std::vector<test_case>
	{
		auto moves         = diff_options{ };
		moves.Moves        = move_detection_policy::deterministic_exact;
		auto spaces        = diff_options{ };
		spaces.Whitespace  = whitespace_policy::ignore_all;
		spaces.LineEndings = line_ending_policy::normalize;
		auto cases         = std::vector<test_case>{
			make_case("", ""),
			make_case("alpha\nbeta\n", "alpha\nbeta\n"),
			make_case("alpha\nbeta\ngamma\n", "alpha\nbeta changed\ngamma\ndelta\n"),
			make_case("prefix one\nkeep\nprefix two\n", "prefix uno\nkeep\nprefix dos\nextra"),
			make_case("a b\r\nc\n", "ab\nc\r\n", spaces),
			make_case("x\ny\nz\nmoved block one\nmoved block two\n", "moved block one\nmoved block two\nx\ny changed\nz\n", moves),
			make_case("caf\xC3\xA9 \xF0\x9F\x98\x80 tail\n", "cafe \xF0\x9F\x98\x81 tail\n"),
		};
		auto left  = std::string{ };
		auto right = std::string{ };
		for (int line = 0; line < 400; ++line) {
			left += "    value_" + std::to_string(line) + " = compute(" + std::to_string(line % 7) + ");\n";
			if (line % 37 == 5) {
				right += "    value_" + std::to_string(line) + " = compute(" + std::to_string(line % 7) + ") + 1;\n";
			} else if (line % 53 != 9) {
				right += "    value_" + std::to_string(line) + " = compute(" + std::to_string(line % 7) + ");\n";
			}
			if (line % 61 == 3) {
				right += "    // inserted\n";
			}
		}
		cases.push_back(make_case(left, right));
		return cases;
	}

	/// A component that supplies nothing.
	class no_parts final : public diff_components
	{
	};

	/// Supplies only the product's primary matches.
	class product_only final : public diff_components
	{
	public:
		[[nodiscard]] auto parts() const noexcept -> diff_component_parts override
		{
			return { .Primary = true };
		}

		[[nodiscard]] auto primary_matches(const diff_primary_request& aRequest, diff_component_meter& aMeter)
			-> std::optional<std::vector<diff_match>> override
		{
			return product_primary(aRequest, aMeter);
		}
	};

	/// Supplies fixed primary matches, or fixed slid matches.
	class fixed_matches final : public diff_components
	{
	public:
		fixed_matches(std::vector<diff_match> aMatches, const bool aSlide)
			: Matches{ std::move(aMatches) }
			, Slide{ aSlide }
		{
		}

		[[nodiscard]] auto parts() const noexcept -> diff_component_parts override
		{
			return { .Primary = true, .Slide = Slide };
		}

		[[nodiscard]] auto primary_matches(const diff_primary_request& aRequest, diff_component_meter& aMeter)
			-> std::optional<std::vector<diff_match>> override
		{
			LeftClasses  = { aRequest.LeftClasses.begin(), aRequest.LeftClasses.end() };
			RightClasses = { aRequest.RightClasses.begin(), aRequest.RightClasses.end() };
			return Slide ? product_primary(aRequest, aMeter) : std::optional{ Matches };
		}

		[[nodiscard]] auto slide_matches(const diff_primary_request& /*aRequest*/, std::span<const diff_match> aMatches,
										 diff_component_meter& /*aMeter*/) -> std::optional<std::vector<diff_match>> override
		{
			Seen = { aMatches.begin(), aMatches.end() };
			return Matches;
		}

		std::vector<diff_match>    Matches;
		bool                       Slide = false;
		std::vector<diff_match>    Seen;
		std::vector<std::uint32_t> LeftClasses;
		std::vector<std::uint32_t> RightClasses;
	};

	/// The engine's own similarity, written out: 2·LCS/(L+R) per mille.
	[[nodiscard]] auto reference_score(const std::span<const std::uint32_t> aLeft, const std::span<const std::uint32_t> aRight)
		-> std::uint16_t
	{
		if (std::ranges::equal(aLeft, aRight)) {
			return maximum_similarity_threshold_permille;
		}
		if (aLeft.empty() || aRight.empty()) {
			return 0U;
		}
		auto previous = std::vector<std::uint32_t>(aRight.size() + 1U, 0U);
		auto current  = previous;
		for (const auto& value : aLeft) {
			for (std::size_t column = 0U; column < aRight.size(); ++column) {
				current[column + 1U] = value == aRight[column] ? previous[column] + 1U : std::max(previous[column + 1U], current[column]);
			}
			std::swap(previous, current);
		}
		return static_cast<std::uint16_t>(static_cast<std::uint64_t>(previous.back()) * 2U * maximum_similarity_threshold_permille / (aLeft.size() + aRight.size()));
	}

	enum class score_mode : std::uint8_t {
		reference,
		too_large,
		wrong_size,
		decline,
	};

	class scores final : public diff_components
	{
	public:
		explicit scores(const score_mode aMode)
			: Mode{ aMode }
		{
		}

		[[nodiscard]] auto parts() const noexcept -> diff_component_parts override
		{
			return { .Primary = true, .Pairing = true };
		}

		[[nodiscard]] auto primary_matches(const diff_primary_request& aRequest, diff_component_meter& aMeter)
			-> std::optional<std::vector<diff_match>> override
		{
			return product_primary(aRequest, aMeter);
		}

		[[nodiscard]] auto pair_scores(const diff_pairing_request& aRequest, diff_component_meter& aMeter)
			-> std::optional<std::vector<std::uint16_t>> override
		{
			++Calls;
			if (Mode == score_mode::decline) {
				return std::nullopt;
			}
			auto result = std::vector<std::uint16_t>{ };
			for (const auto& left : aRequest.Left) {
				for (const auto& right : aRequest.Right) {
					aMeter.tick(1U);
					result.push_back(reference_score(left, right));
				}
			}
			if (Mode == score_mode::too_large) {
				result.front() = maximum_similarity_threshold_permille + 1U;
			} else if (Mode == score_mode::wrong_size) {
				result.push_back(0U);
			}
			return result;
		}

		score_mode  Mode  = score_mode::reference;
		std::size_t Calls = 0U;
	};

	enum class intraline_mode : std::uint8_t {
		reference,
		decline,
		none_matched,
		unequal,
	};

	class intraline final : public diff_components
	{
	public:
		explicit intraline(const intraline_mode aMode)
			: Mode{ aMode }
		{
		}

		[[nodiscard]] auto parts() const noexcept -> diff_component_parts override
		{
			return { .Primary = true, .Intraline = true };
		}

		[[nodiscard]] auto primary_matches(const diff_primary_request& aRequest, diff_component_meter& aMeter)
			-> std::optional<std::vector<diff_match>> override
		{
			return product_primary(aRequest, aMeter);
		}

		[[nodiscard]] auto intraline_matches(const std::span<const std::uint32_t> aLeft, const std::span<const std::uint32_t> aRight,
											 diff_component_meter& /*aMeter*/) -> std::optional<std::vector<diff_match>> override
		{
			++Calls;
			if (Mode == intraline_mode::decline) {
				return std::nullopt;
			}
			if (Mode == intraline_mode::none_matched) {
				return std::vector<diff_match>{ };
			}
			if (Mode == intraline_mode::unequal) {
				for (std::uint32_t left = 0U; left < aLeft.size(); ++left) {
					for (std::uint32_t right = 0U; right < aRight.size(); ++right) {
						if (aLeft[left] != aRight[right]) {
							return std::vector<diff_match>{ { .Left = left, .Right = right } };
						}
					}
				}
				return std::vector<diff_match>{ };
			}
			// A minimal character LCS, with the tie-breaking the engine used
			// before [[BPR-1537]] compared words.
			const auto rows    = aLeft.size() + 1U;
			const auto columns = aRight.size() + 1U;
			auto       table   = std::vector<std::uint32_t>(rows * columns, 0U);
			const auto at      = [&](const std::size_t aRow, const std::size_t aColumn) -> std::uint32_t& { return table[aRow * columns + aColumn]; };
			for (auto row = aLeft.size(); row != 0U; --row) {
				for (auto column = aRight.size(); column != 0U; --column) {
					at(row - 1U, column - 1U) = aLeft[row - 1U] == aRight[column - 1U] ? 1U + at(row, column) : std::max(at(row, column - 1U), at(row - 1U, column));
				}
			}
			auto result = std::vector<diff_match>{ };
			auto row    = std::size_t{ 0U };
			auto column = std::size_t{ 0U };
			while (row < aLeft.size() && column < aRight.size()) {
				if (aLeft[row] == aRight[column]) {
					result.push_back({ .Left = static_cast<std::uint32_t>(row), .Right = static_cast<std::uint32_t>(column) });
					++row;
					++column;
				} else if (at(row + 1U, column) >= at(row, column + 1U)) {
					++row;
				} else {
					++column;
				}
			}
			return result;
		}

		intraline_mode Mode  = intraline_mode::reference;
		std::size_t    Calls = 0U;
	};

	auto test_bpr1465_components_that_supply_nothing_are_refused() -> void
	{
		for (const auto& testCase : sample_cases()) {
			auto       none     = no_parts{ };
			const auto expected = product_file_diff(testCase.input());
			const auto refused  = compute_file_diff(testCase.input(), none);
			expect(refused.Reason == diff_reason_code::host_failure && !refused.Result.has_value(),
				   "[[BPR-1485]] a component without primary matches refuses the comparison: the engine has no matcher of its own");
			auto       primary = product_only{ };
			const auto actual  = compute_file_diff(testCase.input(), primary);
			expect(actual.Reason == expected.Reason && actual.Result == expected.Result,
				   "[[BPR-1485]] the product's primary matches alone compute the product's outcome, counters included");
			auto       declined = scores{ score_mode::decline };
			const auto fallback = compute_file_diff(testCase.input(), declined);
			expect(fallback.Reason == expected.Reason && fallback.Result == expected.Result,
				   "[[BPR-1465]] a declined part keeps the engine's own algorithm");
			const auto material = compute_material_diff(testCase.material_input(), none);
			expect(material.Reason == diff_reason_code::host_failure && !material.Result.has_value(),
				   "[[BPR-1485]] a material comparison without primary matches is refused too");
		}
	}

	auto test_bpr1465_supplied_baseline_matches_reproduce_the_alignment() -> void
	{
		for (const auto& testCase : sample_cases()) {
			if (testCase.Options.Moves != move_detection_policy::disabled) {
				continue;
			}
			const auto expected    = product_file_diff(testCase.input());
			const auto baseline    = text_result(expected, "[[BPR-1465]] the baseline computes");
			const auto observation = diff_primary_observation{ .Matches = equal_matches(baseline) };
			const auto observed    = compute_file_diff(testCase.input(), observation);
			const auto supplied    = text_result(observed, "[[BPR-1465]] the baseline's own matches are accepted");
			expect(supplied.Alignment == baseline.Alignment && supplied.Intraline == baseline.Intraline
					   && supplied.LeftDocument == baseline.LeftDocument && supplied.RightDocument == baseline.RightDocument,
				   "[[BPR-1465]] supplying the baseline's matches reproduces its alignment and intraline edits");
			const auto material         = compute_material_diff(testCase.material_input(), observation);
			const auto materialBaseline = product_material_diff(testCase.material_input());
			expect(material.Result.has_value() && materialBaseline.Result.has_value()
					   && material.Result->Alignment == materialBaseline.Result->Alignment && material.Result->Intraline == materialBaseline.Result->Intraline,
				   "[[BPR-1465]] a material comparison accepts the same observation");
		}
	}

	auto test_bpr1465_line_classes_follow_normalization() -> void
	{
		auto spaces       = diff_options{ };
		spaces.Whitespace = whitespace_policy::ignore_all;
		const auto cases  = std::array{
			std::pair{ make_case("a b\nab\nc\n", "ab\nc\nc\n", spaces), std::array<bool, 3U>{ true, true, false } },
			std::pair{ make_case("a b\nab\nc\n", "ab\nc\nc\n"), std::array<bool, 3U>{ false, true, false } },
		};
		for (const auto& [testCase, leftEqualsRightFirst] : cases) {
			auto component = fixed_matches{ { }, false };
			static_cast<void>(compute_file_diff(testCase.input(), component));
			expect(component.LeftClasses.size() == 3U && component.RightClasses.size() == 3U, "[[BPR-1465]] one class per line");
			for (std::size_t line = 0U; line < 3U; ++line) {
				expect((component.LeftClasses[line] == component.RightClasses[0]) == leftEqualsRightFirst[line],
					   "[[BPR-1465]] two lines share a class exactly when their normalized forms are equal");
			}
			expect(component.RightClasses[1] == component.RightClasses[2] && component.LeftClasses[2] == component.RightClasses[1],
				   "[[BPR-1465]] repeated lines share a class");
		}
	}

	auto test_bpr1465_invalid_supplied_values_are_refused() -> void
	{
		const auto testCase = make_case("alpha\nbeta\ngamma\n", "alpha\nbeta changed\ngamma\ndelta\n");
		const auto refused  = [](const diff_compute_outcome& aOutcome) {
			return aOutcome.Reason == diff_reason_code::invalid_alignment && !aOutcome.Result.has_value();
		};
		const auto invalid = std::array{
			std::vector<diff_match>{ { .Left = 2U, .Right = 2U }, { .Left = 0U, .Right = 0U } },
			std::vector<diff_match>{ { .Left = 0U, .Right = 0U }, { .Left = 0U, .Right = 2U } },
			std::vector<diff_match>{ { .Left = 3U, .Right = 3U } },
			std::vector<diff_match>{ { .Left = 1U, .Right = 1U } },
			std::vector<diff_match>{ { .Left = 0U, .Right = 9U } },
		};
		for (const auto& matches : invalid) {
			expect(refused(compute_file_diff(testCase.input(), diff_primary_observation{ .Matches = matches })),
				   "[[BPR-1465]] an out-of-order, out-of-range or unequal observation is refused");
			auto slide = fixed_matches{ matches, true };
			expect(refused(compute_file_diff(testCase.input(), slide)), "[[BPR-1465]] an invalid post-pass is refused");
		}
		const auto material = compute_material_diff(testCase.material_input(), diff_primary_observation{ .Matches = invalid.front() });
		expect(material.Reason == diff_reason_code::invalid_alignment && !material.Result.has_value(),
			   "[[BPR-1465]] a material comparison refuses an invalid observation");
		// An empty list is valid: everything changes.
		const auto empty = text_result(compute_file_diff(testCase.input(), diff_primary_observation{ }), "[[BPR-1465]] no matches is a valid alignment");
		expect(std::ranges::none_of(empty.Alignment.Segments, [](const alignment_segment& aSegment) { return aSegment.Kind == alignment_kind::equal; }),
			   "[[BPR-1465]] no supplied matches leaves no equal segment");
		for (const auto& mode : { score_mode::too_large, score_mode::wrong_size }) {
			auto component = scores{ mode };
			expect(refused(compute_file_diff(testCase.input(), component)), "[[BPR-1465]] an out-of-range or misshapen score table is refused");
		}
		auto unequal = intraline{ intraline_mode::unequal };
		expect(refused(compute_file_diff(make_case("prefix alpha\n", "prefix beta\n").input(), unequal)) && unequal.Calls == 1U, "[[BPR-1465]] intraline matches that join unequal scalars are refused");
	}

	auto test_bpr1465_scores_and_intraline_matches_are_used() -> void
	{
		for (const auto& testCase : sample_cases()) {
			const auto expected  = product_file_diff(testCase.input());
			auto       reference = scores{ score_mode::reference };
			const auto scored    = compute_file_diff(testCase.input(), reference);
			expect(scored.Reason == expected.Reason && scored.Result.has_value() == expected.Result.has_value(),
				   "[[BPR-1465]] reference scores compute");
			if (expected.Result.has_value()) {
				expect(scored.Result->Alignment == expected.Result->Alignment && scored.Result->Intraline == expected.Result->Intraline
						   && scored.Result->MatrixCellsConsumed == expected.Result->MatrixCellsConsumed
						   && scored.Result->AlignmentApproximate == expected.Result->AlignmentApproximate,
					   "[[BPR-1465]] the engine's own scores, supplied, pair identically and charge the same budget");
			}
			auto       declined   = intraline{ intraline_mode::decline };
			const auto emphasized = compute_file_diff(testCase.input(), declined);
			if (expected.Result.has_value()) {
				expect(emphasized.Result.has_value() && emphasized.Result->Alignment == expected.Result->Alignment
						   && emphasized.Result->Intraline == expected.Result->Intraline,
					   "[[BPR-1465]] a declined intraline part keeps the engine's own edits");
			}
			auto       characters = intraline{ intraline_mode::reference };
			const auto supplied   = compute_file_diff(testCase.input(), characters);
			if (expected.Result.has_value()) {
				expect(supplied.Result.has_value() && supplied.Result->Alignment == expected.Result->Alignment,
					   "[[BPR-1465]] supplied character matches leave the alignment as it is");
			}
		}
		// [[BPR-1537]] Supplied matches are used as they are: a minimal
		// character LCS keeps the "a" alpha and beta share, where the engine's
		// own comparison emphasises the whole changed word.
		{
			const auto testCase   = make_case("prefix alpha\n", "prefix beta\n");
			auto       characters = intraline{ intraline_mode::reference };
			const auto supplied   = text_result(compute_file_diff(testCase.input(), characters), "[[BPR-1465]] a character-level intraline answer is valid");
			const auto own        = text_result(product_file_diff(testCase.input()), "the engine's own intraline computes");
			expect(supplied.Intraline.size() == 1U && supplied.Intraline.front().Left == byte_span{ 7U, 11U } && supplied.Intraline.front().Right == byte_span{ 7U, 10U },
				   "[[BPR-1465]] the supplied character matches decide the edits: \"alph\" became \"bet\" before the shared \"a\"");
			expect(own.Intraline.size() == 1U && own.Intraline.front().Left == byte_span{ 7U, 12U } && own.Intraline.front().Right == byte_span{ 7U, 11U },
				   "[[BPR-1537]] the engine's own edit is the whole word");
		}
		// A different valid answer is used: no matched scalar makes each
		// similar pair one edit.
		const auto testCase = make_case("prefix alpha\n", "prefix beta\n");
		auto       none     = intraline{ intraline_mode::none_matched };
		const auto whole    = text_result(compute_file_diff(testCase.input(), none), "[[BPR-1465]] a whole-line intraline answer is valid");
		expect(none.Calls == 1U && whole.Intraline.size() == 1U && whole.Intraline.front().Left.Begin == 0U && whole.Intraline.front().Right.Begin == 0U,
			   "[[BPR-1465]] the supplied intraline matches decide the edits");
	}

	auto test_bpr1465_a_post_pass_moves_an_ambiguous_insertion() -> void
	{
		// "b\na\n" inserted after the first "a": the engine's choice and the
		// slid one are both minimal.
		const auto testCase = make_case("a\nb\nc\n", "a\nb\na\nb\nc\n");
		const auto baseline = text_result(product_file_diff(testCase.input()), "[[BPR-1465]] the ambiguous insertion computes");
		const auto own      = equal_matches(baseline);
		auto       slid     = std::vector<diff_match>{ { .Left = 0U, .Right = 2U }, { .Left = 1U, .Right = 3U }, { .Left = 2U, .Right = 4U } };
		if (own == slid) {
			slid = { { .Left = 0U, .Right = 0U }, { .Left = 1U, .Right = 1U }, { .Left = 2U, .Right = 4U } };
		}
		auto       component = fixed_matches{ slid, true };
		const auto moved     = text_result(compute_file_diff(testCase.input(), component), "[[BPR-1465]] the slid matches compute");
		expect(component.Seen == own, "[[BPR-1465]] the post-pass sees the sorted primary matches");
		expect(equal_matches(moved) == slid && moved.Alignment.Segments != baseline.Alignment.Segments,
			   "[[BPR-1465]] the post-pass decides the equal lines");
	}

	/// A primary matcher that counts many steps before answering.
	class slow_primary final : public diff_components
	{
	public:
		[[nodiscard]] auto parts() const noexcept -> diff_component_parts override
		{
			return { .Primary = true };
		}

		[[nodiscard]] auto primary_matches(const diff_primary_request& /*aRequest*/, diff_component_meter& aMeter)
			-> std::optional<std::vector<diff_match>> override
		{
			++Calls;
			for (int step = 0; step < 1'000'000; ++step) {
				aMeter.tick(1U);
			}
			return std::vector<diff_match>{ };
		}

		std::size_t Calls = 0U;
	};

	class stop_at_step final : public diff_compute_stop
	{
	public:
		explicit stop_at_step(const std::uint64_t aStep) noexcept
			: Step{ aStep }
		{
		}

		[[nodiscard]] auto stop_requested(const std::uint64_t aSteps) -> bool override
		{
			Seen = aSteps;
			return aSteps >= Step;
		}

		std::uint64_t Step = 0U;
		std::uint64_t Seen = 0U;
	};

	auto test_bpr1465_a_metered_component_is_stopped() -> void
	{
		const auto     testCase = make_case("alpha\nbeta\n", "alpha\ngamma\n");
		auto           slow     = slow_primary{ };
		const auto     full     = text_result(compute_file_diff(testCase.input(), slow), "[[BPR-1465]] the slow component finishes uninterrupted");
		const auto     start    = compute_file_diff(testCase.input(), diff_compute_control{ }, slow).Statistics.PhaseStarts[static_cast<std::size_t>(diff_compute_phase::primary)];
		constexpr auto interval = std::uint64_t{ 1'024U };
		auto           stop     = stop_at_step{ start + 300'000U };
		auto           slice    = compute_file_diff(testCase.input(), diff_compute_control{ .Stop = &stop, .CheckInterval = interval }, slow);
		expect(!slice.Outcome.has_value() && slice.Statistics.Phase == diff_compute_phase::primary,
			   "[[BPR-1465]] a stop inside a component abandons the computation in the primary phase");
		expect(stop.Seen >= stop.Step && stop.Seen < stop.Step + interval, "[[BPR-1465]] the stop is honoured within one check interval of steps");
		// [[BPR-1485]] The stopped computation kept nothing: the next one
		// starts again from the beginning and finishes with the same result.
		const auto callsBefore = slow.Calls;
		auto       restarted   = compute_file_diff(testCase.input(), diff_compute_control{ }, slow);
		expect(slow.Calls == callsBefore + 1U && restarted.Outcome.has_value() && restarted.Outcome->Result.has_value()
				   && restarted.Outcome->Result->Alignment == full.Alignment,
			   "[[BPR-1485]] a stopped computation is computed again from the start, the component asked again, with the same alignment");
	}
} // namespace

auto main() -> int
{
	test_bpr1465_components_that_supply_nothing_are_refused();
	test_bpr1465_supplied_baseline_matches_reproduce_the_alignment();
	test_bpr1465_line_classes_follow_normalization();
	test_bpr1465_invalid_supplied_values_are_refused();
	test_bpr1465_scores_and_intraline_matches_are_used();
	test_bpr1465_a_post_pass_moves_an_ambiguous_insertion();
	test_bpr1465_a_metered_component_is_stopped();
	return EXIT_SUCCESS;
}
