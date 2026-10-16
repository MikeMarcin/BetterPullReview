// [[BPR-1481]] Intraline ranges come from dtl's comparison of each similar
// pair (RFC-BPR-0082). They are counted through the engine's step meter, not
// charged to the matrix budget, which pairing then has to itself; a pair too
// large for dtl keeps the budgeted table, and a stop inside dtl abandons the
// computation like any other.
//
// [[BPR-1537]] dtl compares the pair's words, spaces and punctuation, and the
// edit script is cleaned up into changed words. A golden corpus of realistic
// changed lines pins what a reader sees, and a character-level comparison,
// what the engine drew before, is shown to fail it.

#include "bpr/core/diff_compute.hpp"
#include "product_diff.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;
	using bpr::tests::product_file_diff;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto product_matching() -> bpr::application::diff_line_matching&
	{
		static auto matching = bpr::application::diff_line_matching{ *bpr::tests::product_line_matcher() };
		return matching;
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
			.LeftAnchor  = { .Authority = scope, .Incarnation = incarnation, .Kind = comparison_anchor_kind::epoch_base, .Epoch = epoch_ordinal{ 1 } },
			.RightAnchor = { .Authority = scope, .Incarnation = incarnation, .Kind = comparison_anchor_kind::epoch_head, .Epoch = epoch_ordinal{ 2 } },
			.LeftBlob    = blob(21U),
			.RightBlob   = blob(51U),
			.File        = opaque<file_identity>(31U),
			.Options     = *options_digest(aOptions),
			.Algorithm   = diff_algorithm_revision{ 1U },
		};
	}

	/// Two texts and the input that compares them; the input points into it.
	struct comparison
	{
		std::vector<std::uint8_t> Left;
		std::vector<std::uint8_t> Right;
		diff_compute_limits       Limits;

		[[nodiscard]] auto input() const -> diff_compute_input
		{
			return {
				.Key        = comparison_key({ }),
				.Options    = { },
				.LeftBytes  = std::span<const std::uint8_t>{ Left },
				.RightBytes = std::span<const std::uint8_t>{ Right },
				.Limits     = Limits,
			};
		}
	};

	[[nodiscard]] auto bytes(const std::string_view aText) -> std::vector<std::uint8_t>
	{
		return { aText.begin(), aText.end() };
	}

	/// One line of aWords words, two tokens each with the space after it;
	/// every aStride-th word differs when aEdited is set, so the two versions
	/// pair as similar.
	[[nodiscard]] auto long_line(const std::size_t aWords, const std::size_t aStride, const bool aEdited) -> std::string
	{
		auto line = std::string{ };
		for (std::size_t index = 0U; index < aWords; ++index) {
			if (index != 0U) {
				line.push_back(' ');
			}
			if (aEdited && index % aStride == aStride / 2U) {
				line += "ZZ";
			} else {
				line.push_back(static_cast<char>('a' + static_cast<char>(index % 23U)));
				line.push_back(static_cast<char>('a' + static_cast<char>(index % 19U)));
			}
		}
		line.push_back('\n');
		return line;
	}

	/// A line with every intraline edit of one side marked: ⟦ before and ⟧
	/// after the emphasised bytes.
	[[nodiscard]] auto marked(const std::string_view aLine, const std::vector<byte_span>& aSpans) -> std::string
	{
		auto result = std::string{ };
		auto cursor = std::size_t{ 0U };
		for (const auto& span : aSpans) {
			result += aLine.substr(cursor, static_cast<std::size_t>(span.Begin) - cursor);
			result += "⟦";
			result += aLine.substr(static_cast<std::size_t>(span.Begin), static_cast<std::size_t>(span.End - span.Begin));
			result += "⟧";
			cursor = static_cast<std::size_t>(span.End);
		}
		result += aLine.substr(cursor);
		return result;
	}

	/// What the engine drew before [[BPR-1537]] (and before [[BPR-1481]]): the
	/// gaps between a minimal character LCS of the two lines, ties broken
	/// toward the earliest match, as `lcs_matches` traced its table.
	[[nodiscard]] auto character_marks(const std::string_view aLeft, const std::string_view aRight) -> std::pair<std::string, std::string>
	{
		// The lines' scalars, by byte position.
		const auto scalars = [](const std::string_view aLine) {
			auto result = std::vector<std::pair<std::size_t, std::size_t>>{ };
			for (std::size_t index = 0U; index < aLine.size();) {
				const auto lead   = static_cast<unsigned char>(aLine[index]);
				const auto length = lead < 0x80U ? 1U : lead < 0xE0U ? 2U
					: lead < 0xF0U                                   ? 3U
																	 : 4U;
				result.emplace_back(index, index + length);
				index += length;
			}
			return result;
		};
		const auto left  = scalars(aLeft);
		const auto right = scalars(aRight);
		const auto equal = [&](const std::size_t aRow, const std::size_t aColumn) {
			return aLeft.substr(left[aRow].first, left[aRow].second - left[aRow].first) == aRight.substr(right[aColumn].first, right[aColumn].second - right[aColumn].first);
		};
		const auto columns = right.size() + 1U;
		auto       table   = std::vector<std::uint32_t>((left.size() + 1U) * columns, 0U);
		for (auto row = left.size(); row != 0U; --row) {
			for (auto column = right.size(); column != 0U; --column) {
				table[(row - 1U) * columns + column - 1U] = equal(row - 1U, column - 1U) ? 1U + table[row * columns + column]
																						 : std::max(table[(row - 1U) * columns + column], table[row * columns + column - 1U]);
			}
		}
		auto       leftSpans  = std::vector<byte_span>{ };
		auto       rightSpans = std::vector<byte_span>{ };
		auto       priorLeft  = std::size_t{ 0U };
		auto       priorRight = std::size_t{ 0U };
		const auto gap        = [&](const std::size_t aLeftEnd, const std::size_t aRightEnd) {
			if (priorLeft != aLeftEnd || priorRight != aRightEnd) {
				leftSpans.push_back(byte_span{ priorLeft, aLeftEnd });
				rightSpans.push_back(byte_span{ priorRight, aRightEnd });
			}
		};
		auto row    = std::size_t{ 0U };
		auto column = std::size_t{ 0U };
		while (row < left.size() && column < right.size()) {
			if (equal(row, column)) {
				gap(left[row].first, right[column].first);
				priorLeft  = left[row].second;
				priorRight = right[column].second;
				++row;
				++column;
			} else if (table[(row + 1U) * columns + column] >= table[row * columns + column + 1U]) {
				++row;
			} else {
				++column;
			}
		}
		gap(aLeft.size(), aRight.size());
		return { marked(aLeft, leftSpans), marked(aRight, rightSpans) };
	}

	[[nodiscard]] auto computed(const diff_compute_outcome& aOutcome, const std::string_view aMessage) -> const computed_file_diff&
	{
		expect(aOutcome.Reason == diff_reason_code::none && aOutcome.Result.has_value(), aMessage);
		expect(validate_computed_file_diff(*aOutcome.Result).valid(), aMessage);
		return *aOutcome.Result;
	}

	[[nodiscard]] auto all_similar(const computed_file_diff& aDiff) -> bool
	{
		for (const auto& pair : aDiff.Alignment.Pairs) {
			if (pair.Kind != line_pair_kind::similar) {
				return false;
			}
		}
		return !aDiff.Alignment.Pairs.empty();
	}

	/// Asks for a stop once the computation has counted aStep steps.
	struct stop_at_step final : diff_compute_stop
	{
		explicit stop_at_step(const std::uint64_t aStep) noexcept
			: Step{ aStep }
		{
		}

		[[nodiscard]] auto stop_requested(const std::uint64_t aSteps) -> bool override
		{
			if (aSteps >= Step && !StoppedAt.has_value()) {
				StoppedAt = aSteps;
			}
			return StoppedAt.has_value();
		}

		std::uint64_t                Step = 0U;
		std::optional<std::uint64_t> StoppedAt;
	};

	/// The emphasis of a one-line comparison, marked on both lines.
	[[nodiscard]] auto product_marks(const std::string_view aLeft, const std::string_view aRight, const std::string_view aMessage)
		-> std::pair<std::string, std::string>
	{
		auto       pair = comparison{ .Left = bytes(std::string{ aLeft } + "\n"), .Right = bytes(std::string{ aRight } + "\n") };
		const auto diff = computed(product_file_diff(pair.input()), aMessage);
		expect(diff.Alignment.Pairs.size() == 1U && diff.Alignment.Pairs.front().Kind == line_pair_kind::similar && !diff.AlignmentApproximate, aMessage);
		auto left  = std::vector<byte_span>{ };
		auto right = std::vector<byte_span>{ };
		for (const auto& edit : diff.Intraline) {
			left.push_back(edit.Left);
			right.push_back(edit.Right);
		}
		return { marked(aLeft, left), marked(aRight, right) };
	}

	auto test_intraline_ranges_are_changed_words() -> void
	{
		const auto [left, right] = product_marks("prefix alpha suffix", "prefix beta suffix", "a similar pair computes");
		expect(left == "prefix ⟦alpha⟧ suffix" && right == "prefix ⟦beta⟧ suffix",
			   "[[BPR-1537]] the changed word is emphasised whole: alpha and beta share an \"a\", but no reader sees one");
	}

	/// [[BPR-1537]] One realistic changed line, what the engine emphasises on
	/// each side, and whether that differs from the character-level emphasis
	/// the engine drew before.
	struct golden_pair
	{
		std::string_view Kind;
		std::string_view Left;
		std::string_view Right;
		std::string_view MarkedLeft;
		std::string_view MarkedRight;
		bool             DiffersFromCharacters = false;
	};

	auto test_golden_corpus_reads_as_changed_words() -> void
	{
		// Renamed identifiers, inserted and removed arguments, edited strings
		// and prose, reordered operands, operators, spacing, numbers and
		// hashes, rewritten lines, case, symbols, CJK, accents and emoji.
		const auto corpus = std::array{
			golden_pair{ "rename", "\tconst auto result = compute_file_diff(input, components);", "\tconst auto outcome = compute_file_diff(input, components);",
						 "\tconst auto ⟦result⟧ = compute_file_diff(input, components);",
						 "\tconst auto ⟦outcome⟧ = compute_file_diff(input, components);", true },
			golden_pair{ "rename", "\tauto conversationCount = model->rowCount();", "\tauto threadCount = model->rowCount();",
						 "\tauto ⟦conversation⟧Count = model->rowCount();",
						 "\tauto ⟦thread⟧Count = model->rowCount();", true },
			golden_pair{ "rename", "\tvoid set_dispatch(private_dispatch dispatch) { Dispatch = std::move(dispatch); }", "\tvoid set_dispatch(private_dispatch aDispatch) { Dispatch = std::move(aDispatch); }",
						 "\tvoid set_dispatch(private_dispatch ⟦dispatch⟧) { Dispatch = std::move(⟦dispatch⟧); }",
						 "\tvoid set_dispatch(private_dispatch ⟦aDispatch⟧) { Dispatch = std::move(⟦aDispatch⟧); }", true },
			golden_pair{ "rename", "\tconst auto leftScalars = decode(leftBytes);", "\tconst auto leftValues = decode(leftBytes);",
						 "\tconst auto left⟦Scalars⟧ = decode(leftBytes);",
						 "\tconst auto left⟦Values⟧ = decode(leftBytes);", true },
			golden_pair{ "rename", "#include <QKeyEvent>", "#include <QKeySequence>",
						 "#include <QKey⟦Event⟧>",
						 "#include <QKey⟦Sequence⟧>", true },
			golden_pair{ "rename", "\tauto recieved = queue.take();", "\tauto received = queue.take();",
						 "\tauto ⟦recieved⟧ = queue.take();",
						 "\tauto ⟦received⟧ = queue.take();", true },
			golden_pair{ "rename", "\tconst auto published = count_published();", "\tconst auto published = countPublished();",
						 "\tconst auto published = count⟦_published⟧();",
						 "\tconst auto published = count⟦Published⟧();", true },
			golden_pair{ "rename", "\tusing size_type = std::size_t;", "\tusing size_type = std::uint64_t;",
						 "\tusing size_type = std::⟦size_t⟧;",
						 "\tusing size_type = std::⟦uint64_t⟧;", true },
			golden_pair{ "arguments", "\tconst auto result = run_tool(process, configuration, std::move(invocation), stop);", "\tconst auto result = run_tool(process, configuration, std::move(invocation), stop, credential);",
						 "\tconst auto result = run_tool(process, configuration, std::move(invocation), stop⟦⟧);",
						 "\tconst auto result = run_tool(process, configuration, std::move(invocation), stop⟦, credential⟧);", false },
			golden_pair{ "arguments", "\ttransport{ *cloud, *configuration, process };", "\ttransport{ *cloud, scoped(*configuration, *cloud), process, credential() };",
						 "\ttransport{ *cloud, ⟦⟧*configuration, ⟦⟧process⟦⟧ };",
						 "\ttransport{ *cloud, ⟦scoped(⟧*configuration, ⟦*cloud), ⟧process⟦, credential()⟧ };", true },
			golden_pair{ "arguments", "\tsetSizePolicy(QSizePolicy::Expanding, expanded ? QSizePolicy::Expanding : QSizePolicy::Fixed);", "\tsetSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);",
						 "\tsetSizePolicy(QSizePolicy::Expanding, ⟦expanded ? ⟧QSizePolicy::Expanding⟦ : QSizePolicy::Fixed⟧);",
						 "\tsetSizePolicy(QSizePolicy::Expanding, ⟦⟧QSizePolicy::Expanding⟦⟧);", false },
			golden_pair{ "arguments", "\tfor (unsigned mode = 0; mode < 8; ++mode) {", "\tfor (unsigned mode = 0; mode < count; ++mode) {",
						 "\tfor (unsigned mode = 0; mode < ⟦8⟧; ++mode) {",
						 "\tfor (unsigned mode = 0; mode < ⟦count⟧; ++mode) {", false },
			golden_pair{ "string", "\tlabel->setText(tr(\"Show the conversation drawer at its saved width.\"));", "\tlabel->setText(tr(\"Show the comments drawer at its saved width.\"));",
						 "\tlabel->setText(tr(\"Show the ⟦conversation⟧ drawer at its saved width.\"));",
						 "\tlabel->setText(tr(\"Show the ⟦comments⟧ drawer at its saved width.\"));", true },
			golden_pair{ "string", "\texpect(window.modality() == review_modality::read, \"a file location is shown in Read\");", "\texpect(window.modality() == review_modality::review, \"a file location is shown in Review\");",
						 "\texpect(window.modality() == review_modality::⟦read⟧, \"a file location is shown in ⟦Read⟧\");",
						 "\texpect(window.modality() == review_modality::⟦review⟧, \"a file location is shown in ⟦Review⟧\");", true },
			golden_pair{ "string", "If an export fails, the report shows an error and you can try again.", "If an export fails, the report says which step failed and why, and you can try again.",
						 "If an export fails, the report ⟦shows an error⟧ and you can try again.",
						 "If an export fails, the report ⟦says which step failed and why,⟧ and you can try again.", true },
			golden_pair{ "string", "\tview->setToolTip(QStringLiteral(\"Only conversations with a label\"));", "\tview->setToolTip(QStringLiteral(\"Only threads with a label\"));",
						 "\tview->setToolTip(QStringLiteral(\"Only ⟦conversations⟧ with a label\"));",
						 "\tview->setToolTip(QStringLiteral(\"Only ⟦threads⟧ with a label\"));", true },
			golden_pair{ "reorder", "\treturn a.width() < b.width() && a.height() < b.height();", "\treturn a.height() < b.height() && a.width() < b.width();",
						 "\treturn a.⟦width⟧() < b.⟦width⟧() && a.⟦height⟧() < b.⟦height⟧();",
						 "\treturn a.⟦height⟧() < b.⟦height⟧() && a.⟦width⟧() < b.⟦width⟧();", true },
			golden_pair{ "reorder", "\tconst auto [left, right] = std::pair{ first, second };", "\tconst auto [right, left] = std::pair{ second, first };",
						 "\tconst auto [⟦left, right⟧] = std::pair{ ⟦first, second⟧ };",
						 "\tconst auto [⟦right, left⟧] = std::pair{ ⟦second, first⟧ };", true },
			golden_pair{ "punctuation", "\tif (index != 0U && match.Left <= previous.Left) {", "\tif (index != 0U && match.Left < previous.Left) {",
						 "\tif (index != 0U && match.Left ⟦<=⟧ previous.Left) {",
						 "\tif (index != 0U && match.Left ⟦<⟧ previous.Left) {", true },
			golden_pair{ "punctuation", "\tif (result->Publication != published && observed) {", "\tif (result->Publication == published && observed) {",
						 "\tif (result->Publication ⟦!=⟧ published && observed) {",
						 "\tif (result->Publication ⟦==⟧ published && observed) {", true },
			golden_pair{ "punctuation", "\tvalues.push_back(value)", "\tvalues.push_back(value);",
						 "\tvalues.push_back(value)⟦⟧",
						 "\tvalues.push_back(value)⟦;⟧", false },
			golden_pair{ "whitespace", "\tstd::uint64_t     LargestPollDelay = 0U;", "\tstd::uint64_t LargestPollDelay = 0U;",
						 "\tstd::uint64_t ⟦    ⟧LargestPollDelay = 0U;",
						 "\tstd::uint64_t ⟦⟧LargestPollDelay = 0U;", false },
			golden_pair{ "whitespace", "\tauto* tree = panel.tree();", "\tauto* tree      = panel.tree();",
						 "\tauto* tree ⟦⟧= panel.tree();",
						 "\tauto* tree ⟦     ⟧= panel.tree();", false },
			golden_pair{ "whitespace", "\t\treturn value;", "\t\t\treturn value;",
						 "\t\t⟦⟧return value;",
						 "\t\t⟦\t⟧return value;", false },
			golden_pair{ "number", "#define _APS_NEXT_RESOURCE_VALUE        136", "#define _APS_NEXT_RESOURCE_VALUE        137",
						 "#define _APS_NEXT_RESOURCE_VALUE        ⟦136⟧",
						 "#define _APS_NEXT_RESOURCE_VALUE        ⟦137⟧", true },
			golden_pair{ "number", "\tconstexpr auto StoreFiles = std::array<std::string_view, 11U>{", "\tconstexpr auto StoreFiles = std::array<std::string_view, 12U>{",
						 "\tconstexpr auto StoreFiles = std::array<std::string_view, ⟦11U⟧>{",
						 "\tconstexpr auto StoreFiles = std::array<std::string_view, ⟦12U⟧>{", true },
			golden_pair{ "number", "\t\"source_revision\": \"9013e16833a6076e26120a12e5e98dd119b2ac39\",", "\t\"source_revision\": \"fb2e72778d3c61de6ec6723dffc16a1212ab5c31\",",
						 "\t\"source_revision\": \"⟦9013e16833a6076e26120a12e5e98dd119b2ac39⟧\",",
						 "\t\"source_revision\": \"⟦fb2e72778d3c61de6ec6723dffc16a1212ab5c31⟧\",", true },
			golden_pair{ "number", "\t\"retries\": 2,", "\t\"retries\": 3,",
						 "\t\"retries\": ⟦2⟧,",
						 "\t\"retries\": ⟦3⟧,", false },
			golden_pair{ "rewrite", "\t// The checks that need no text, then a fresh progress holding copies", "\t// The checks that need no text. A comparison they finish yields its",
						 "\t// The checks that need no text⟦, then a fresh progress holding copies⟧",
						 "\t// The checks that need no text⟦. A comparison they finish yields its⟧", true },
			golden_pair{ "rewrite", "\tif (pattern.front() == '/') {", "\tif (path.Pattern.empty()) {",
						 "\tif (⟦pattern.front() == '/'⟧) {",
						 "\tif (⟦path.Pattern.empty()⟧) {", true },
			golden_pair{ "rewrite", "\t\"This is the last change in this comparison.\"", "\t\"Last change in the pull request.\"",
						 "\t\"⟦This is the last change in this comparison⟧.\"",
						 "\t\"⟦Last change in the pull request⟧.\"", true },
			golden_pair{ "rewrite", "Pending drafts are notes at their lines named \"Pending in your review\".", "Pending drafts are notes at their lines tagged \"Pending in your review\" in the writing blue.",
						 "Pending drafts are notes at their lines ⟦named⟧ \"Pending in your review\"⟦⟧.",
						 "Pending drafts are notes at their lines ⟦tagged⟧ \"Pending in your review\"⟦ in the writing blue⟧.", true },
			golden_pair{ "case", "\t\"My pull request; Nothing about it has been read yet.\"", "\t\"My pull request; nothing about it has been read yet.\"",
						 "\t\"My pull request; ⟦Nothing⟧ about it has been read yet.\"",
						 "\t\"My pull request; ⟦nothing⟧ about it has been read yet.\"", true },
			golden_pair{ "unicode", "\tlabel = \"Next change (⌘↓)\";", "\tlabel = \"Next change (⌥↓)\";",
						 "\tlabel = \"Next change (⟦⌘⟧↓)\";",
						 "\tlabel = \"Next change (⟦⌥⟧↓)\";", false },
			golden_pair{ "unicode", "\ttitle = \"保存されたビュー\";", "\ttitle = \"保存した表示\";",
						 "\ttitle = \"保存⟦されたビュー⟧\";",
						 "\ttitle = \"保存⟦した表示⟧\";", true },
			golden_pair{ "unicode", "\tdrink = \"café crème\";", "\tdrink = \"café noir\";",
						 "\tdrink = \"café ⟦crème⟧\";",
						 "\tdrink = \"café ⟦noir⟧\";", true },
			golden_pair{ "unicode", "\tstatus = \"done 👩‍💻 today\";", "\tstatus = \"done 🧑‍🔧 today\";",
						 "\tstatus = \"done ⟦👩‍💻⟧ today\";",
						 "\tstatus = \"done ⟦🧑‍🔧⟧ today\";", true },
		};
		auto differing = std::size_t{ 0U };
		for (const auto& golden : corpus) {
			const auto label          = std::string{ "golden " } + std::string{ golden.Kind } + ": " + std::string{ golden.Left };
			const auto [left, right]  = product_marks(golden.Left, golden.Right, label);
			const auto [oldL, oldR]   = character_marks(golden.Left, golden.Right);
			const auto matchesGolden  = left == golden.MarkedLeft && right == golden.MarkedRight;
			const auto charactersPass = oldL == golden.MarkedLeft && oldR == golden.MarkedRight;
			if (!matchesGolden) {
				std::cerr << "  left:     " << left << "\n  expected: " << golden.MarkedLeft << "\n  right:    " << right << "\n  expected: " << golden.MarkedRight << '\n';
			}
			expect(matchesGolden, "[[BPR-1537]] " + label);
			// Negative control: a character-level comparison fails the golden
			// wherever the corpus says it differs, and matches it elsewhere.
			expect(charactersPass != golden.DiffersFromCharacters, "[[BPR-1537]] the character-level control decides " + label);
			differing += golden.DiffersFromCharacters ? 1U : 0U;
		}
		expect(differing * 4U >= corpus.size() * 3U, "[[BPR-1537]] the character-level control fails at least three quarters of the corpus");
		std::cout << "BPR-1537 golden corpus: " << corpus.size() << " pairs, " << differing << " differ from character-level emphasis\n";
	}

	auto test_intraline_does_not_charge_the_matrix_budget() -> void
	{
		auto left  = std::string{ };
		auto right = std::string{ };
		for (std::size_t line = 0U; line < 20U; ++line) {
			left += long_line(40U + line, 7U, false);
			right += long_line(40U + line, 7U, true);
		}
		auto       pair    = comparison{ .Left = bytes(left), .Right = bytes(right) };
		const auto full    = computed(product_file_diff(pair.input()), "the twenty pairs compute");
		const auto pairing = full.MatrixCellsConsumed;
		expect(all_similar(full) && full.Alignment.Pairs.size() == 20U && !full.AlignmentApproximate && !full.Intraline.empty(), "every line pairs as similar");
		pair.Limits.MaximumMatrixCells = pairing;
		const auto exact               = computed(product_file_diff(pair.input()), "the pairs compute in pairing's budget");
		expect(exact.MatrixCellsConsumed == pairing,
			   "[[BPR-1481]] the matrix budget counts pairing only: intraline comparison charges nothing");
		expect(!exact.AlignmentApproximate && exact.Intraline == full.Intraline && exact.Alignment == full.Alignment,
			   "[[BPR-1481]] a budget pairing exactly fills still gives every pair its intraline emphasis");
		std::cout << "BPR-1481 twenty pairs: pairing " << pairing << " matrix cells, " << full.Intraline.size() << " intraline edits\n";
	}

	auto test_a_pair_too_large_for_dtl_keeps_the_budgeted_table() -> void
	{
		// 1,000 words a side, about 2,000 tokens: the token grid is past
		// dtl's bound even between the common prefix and suffix, and with
		// pairing's table it fits the default budget.
		auto       large    = comparison{ .Left = bytes(long_line(1'000U, 9U, false)), .Right = bytes(long_line(1'000U, 9U, true)) };
		const auto full     = computed(product_file_diff(large.input()), "the long pair computes");
		const auto scalars  = std::uint64_t{ large.Left.size() - 1U };
		const auto pairing  = (scalars + 1U) * (scalars + 1U);
		const auto consumed = full.MatrixCellsConsumed;
		expect(all_similar(full) && !full.Intraline.empty() && !full.AlignmentApproximate, "the long lines pair with intraline ranges");
		expect(consumed > pairing + 2'000'000U, "[[BPR-1481]] [[BPR-1537]] the long pair's token table is charged to the matrix budget, as before");
		large.Limits.MaximumMatrixCells = consumed - 1U;
		const auto tight                = computed(product_file_diff(large.input()), "the long pair computes in a tighter budget");
		expect(all_similar(tight) && tight.Intraline.empty() && tight.AlignmentApproximate,
			   "[[BPR-0719]] [[BPR-1481]] past dtl's bound an exhausted budget still omits the pass instead of refusing the file");

		// 600 words a side, about 1,200 tokens, is inside the bound: pairing's
		// table is charged, a token table of 1.4 million cells is not.
		auto       inside        = comparison{ .Left = bytes(long_line(600U, 9U, false)), .Right = bytes(long_line(600U, 9U, true)) };
		const auto within        = computed(product_file_diff(inside.input()), "the shorter long pair computes");
		const auto insideScalars = std::uint64_t{ inside.Left.size() - 1U };
		expect(all_similar(within) && !within.Intraline.empty() && within.MatrixCellsConsumed < (insideScalars + 1U) * (insideScalars + 1U) + 1'000'000U,
			   "[[BPR-1481]] inside dtl's bound the pair is compared by dtl, uncharged");
	}

	auto test_a_stop_inside_dtl_abandons_the_computation() -> void
	{
		auto left  = std::string{ };
		auto right = std::string{ };
		// 330 words a line: each token grid is inside dtl's bound, and
		// pairing's nine scored tables fit the default budget.
		for (std::size_t line = 0U; line < 3U; ++line) {
			left += long_line(330U + line, 5U, false);
			right += long_line(330U + line, 5U, true);
		}
		const auto pair  = comparison{ .Left = bytes(left), .Right = bytes(right) };
		const auto full  = compute_file_diff(pair.input(), diff_compute_control{ }, product_matching());
		const auto total = full.Statistics.Steps;
		expect(full.Outcome.has_value() && full.Outcome->Result.has_value() && all_similar(*full.Outcome->Result), "the three long pairs compute");
		const auto assemble = full.Statistics.PhaseStarts[static_cast<std::size_t>(diff_compute_phase::assemble)];
		const auto validate = full.Statistics.PhaseStarts[static_cast<std::size_t>(diff_compute_phase::validate)];
		expect(validate > assemble + 100'000U, "[[BPR-1481]] dtl's comparisons are counted: the assemble phase does most of the work");
		expect(full.Statistics.LargestPollDelay <= 4'096U + 1'024U,
			   "[[BPR-1481]] dtl's comparisons are counted in increments of at most 4,096");
		for (std::size_t sample = 1U; sample < 16U; ++sample) {
			const auto request = assemble + (validate - assemble) * sample / 16U;
			auto       stop    = stop_at_step{ request };
			const auto slice   = compute_file_diff(pair.input(), diff_compute_control{ .Stop = &stop, .CheckInterval = 1U }, product_matching());
			expect(!slice.Outcome.has_value() && stop.StoppedAt.has_value(), "[[BPR-1481]] a stop inside dtl abandons the computation");
			expect(*stop.StoppedAt - request <= 4'096U + 1'024U && *stop.StoppedAt < total,
				   "[[BPR-1481]] a stop inside dtl is honoured within one increment of its comparisons");
			const auto again = compute_file_diff(pair.input(), diff_compute_control{ }, product_matching());
			expect(again.Outcome.has_value() && again.Outcome->Result == full.Outcome->Result && again.Statistics == full.Statistics,
				   "[[BPR-1485]] [[BPR-1481]] computed again after a stop inside dtl, the result and steps are the uninterrupted ones");
		}
	}
} // namespace

int main()
{
	test_intraline_ranges_are_changed_words();
	test_golden_corpus_reads_as_changed_words();
	test_intraline_does_not_charge_the_matrix_budget();
	test_a_pair_too_large_for_dtl_keeps_the_budgeted_table();
	test_a_stop_inside_dtl_abandons_the_computation();
	std::cout << "BPR-1481 and BPR-1537 intraline tests passed\n";
	return EXIT_SUCCESS;
}
