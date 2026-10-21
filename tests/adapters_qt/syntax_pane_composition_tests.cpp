#include "diff_publication_fixture.hpp"
#include "product_diff.hpp"

/// \file
/// \brief Proves the packaged classifier's own answer reaches a real pane.
///
/// \details Every other suite proves one half. `syntax_classification_contract`
/// shows the adapter classifies a complete blob correctly, and
/// `qt_diff_workbench_*` shows a pane paints the spans it is handed. Neither
/// notices if the two are never joined, which is exactly the composition gap
/// BPR-0180 found six RFCs' worth of.
///
/// So this links both and asserts on rendered formats: the classifier reads a
/// blob, the workbench paints it, and the assertion reads the colour off the
/// block layout rather than off the classification that was supposed to arrive.

#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/diff_workbench.hpp"

#include <QApplication>
#include <QColor>
#include <QElapsedTimer>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextLayout>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	namespace syntax = bpr::adapters::syntax;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void process_events()
	{
		for (auto count = 0; count < 4; ++count) {
			QApplication::processEvents();
		}
	}

	// [[BPR-0870]]. The shared scrollbar moves the panes only once the
	// publication's native measurement has given the controller an axis;
	// before that its value changes and the panes stay where they are. Four
	// rounds of events were enough on macOS and Linux, and not on Windows.
	void settle_geometry(qt::diff_workbench& aWorkbench)
	{
		auto timer = QElapsedTimer{ };
		timer.start();
		do {
			QApplication::processEvents();
		} while (aWorkbench.geometry_pending() && timer.elapsed() < 5'000);
		expect(!aWorkbench.geometry_pending(), "the publication's native measurement completes before the course scrolls");
		process_events();
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes    = std::array<std::uint8_t, 32U>{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed)
		-> core::git_blob_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto comparison_key(const core::diff_options& aOptions)
		-> core::file_comparison_key
	{
		const auto digest = core::options_digest(aOptions);
		expect(digest.has_value(), "fixture options normalize");
		const auto scope     = opaque<core::pull_request_authority_scope_id>(11U);
		const auto authority = core::pull_request_authority{
			.Profile         = core::host_profile_id{ 10U },
			.ProfileRevision = core::github_profile_revision{ 20U },
			.Host            = core::git_host_id{ 30U },
			.HostAuthority   = opaque<core::host_authority_fingerprint>(13U),
			.Account         = core::github_account_id{ 40U },
			.Viewer          = core::github_viewer_actor_id{ 50U },
			.Repository      = core::repository_id{ 101U },
			.Cache           = opaque<core::repository_cache_scope_id>(14U),
			.PullRequest     = core::pull_request_id{ 202U },
			.Number          = core::pull_request_number{ 17U },
			.Scope           = scope,
		};
		return {
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = {
				.Authority   = scope,
				.Incarnation = opaque<core::epoch_catalog_incarnation_id>(12U),
				.Kind        = core::comparison_anchor_kind::epoch_base,
				.Epoch       = core::epoch_ordinal{ 1 },
			},
			.RightAnchor = {
				.Authority   = scope,
				.Incarnation = opaque<core::epoch_catalog_incarnation_id>(12U),
				.Kind        = core::comparison_anchor_kind::epoch_head,
				.Epoch       = core::epoch_ordinal{ 2 },
			},
			.LeftBlob  = blob(21U),
			.RightBlob = blob(51U),
			.File      = opaque<core::file_identity>(31U),
			.Options   = *digest,
			.Algorithm = core::supported_diff_algorithm_revision,
		};
	}

	/// \brief Serves the one blob this suite classifies.
	class single_blob final : public syntax::blob_source
	{
	public:
		explicit single_blob(std::string aBytes)
			: Bytes{ std::move(aBytes) }
		{ }

		[[nodiscard]] auto blob_bytes(const core::git_blob_oid&)
			-> std::optional<std::string> override
		{
			++Reads;
			return Bytes;
		}

		std::size_t Reads = 0U;

	private:
		std::string Bytes;
	};

	/// The marker sits far enough inside a block comment that the line which
	/// opened the comment cannot be on screen with it. A pane that reasoned
	/// about the lines it can see would classify the marker as ordinary code.
	constexpr std::string_view comment_marker = "BPR-COMMENT-BODY";
	constexpr std::size_t      filler_lines   = 400U;

	[[nodiscard]] auto classified_source() -> std::string
	{
		auto text = std::string{ "#include <string>\n\n/*\n" };
		for (auto line = std::size_t{ 0U }; line < filler_lines; ++line) {
			text += " * filler line " + std::to_string(line) + '\n';
		}
		text += " * ";
		text += comment_marker;
		text += " carries no comment marker of its own.\n */\n";
		text += "namespace bpr::fixture {\n"
				"\tconstexpr int Answer = 42;\n"
				"}\n";
		return text;
	}

	[[nodiscard]] auto line_of(
		const std::string_view aText,
		const std::string_view aMarker) -> std::uint32_t
	{
		auto line  = std::uint32_t{ 0U };
		auto begin = std::size_t{ 0U };
		while (begin <= aText.size()) {
			const auto end = aText.find('\n', begin);
			const auto row = aText.substr(
				begin,
				end == std::string_view::npos ? std::string_view::npos
											  : end - begin);
			if (row.find(aMarker) != std::string_view::npos) {
				return line;
			}
			if (end == std::string_view::npos) {
				break;
			}
			begin = end + 1U;
			++line;
		}
		expect(false, "marker line is present in the fixture");
		return 0U;
	}

	[[nodiscard]] auto block_formats(QPlainTextEdit& aPane, const int aLine)
		-> QList<QTextLayout::FormatRange>
	{
		const auto block = aPane.document()->findBlockByNumber(aLine);
		if (!block.isValid() || block.layout() == nullptr) {
			return { };
		}
		return block.layout()->formats();
	}

	void test_classified_blob_reaches_the_pane()
	{
		const auto left  = classified_source();
		auto       right = left;
		right += "// a trailing difference so the comparison has one\n";

		auto       leftBytes  = std::vector<std::uint8_t>{ left.begin(), left.end() };
		auto       rightBytes = std::vector<std::uint8_t>{ right.begin(), right.end() };
		const auto options    = core::diff_options{ };
		const auto outcome    = bpr::tests::product_file_diff({
			.Key        = comparison_key(options),
			.Options    = options,
			.LeftBytes  = std::span<const std::uint8_t>{ leftBytes },
			.RightBytes = std::span<const std::uint8_t>{ rightBytes },
		});
		expect(
			outcome.Reason == core::diff_reason_code::none
				&& outcome.Result.has_value(),
			"the classified comparison computes");

		auto catalog    = syntax::packaged_definition_catalog{ };
		auto blobs      = single_blob{ left };
		auto classifier = syntax::packaged_syntax_classifier{ catalog, blobs };
		// The epoch-history fixture's own file name, so this asserts the thing
		// BPR-0197 changed: the surface every attended review looks at selects
		// a real definition rather than plain text. Read from the catalog
		// rather than from the extension, because the extension is not what
		// decides.
		const auto definition = catalog.definition_for_path("review.cpp");
		expect(
			definition.Name != syntax::plain_text_definition_name,
			"the review fixture's path selects a packaged definition rather "
			"than plain text");

		const auto classification = classifier.classify(
			{
				.Context      = { },
				.Key          = { .Blob = blob(21U), .Definition = definition },
				.MaximumSpans = 1'000'000U,
			},
			std::stop_token{ });
		expect(
			classification.Status
				== application::syntax_classification_status::available,
			"the packaged classifier answers for the fixture blob");
		expect(blobs.Reads == 1U, "the classifier read the blob exactly once");

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		auto profile = qt::appearance_profile{ };
		static_cast<void>(profile.select_mode(core::appearance_mode::light));
		workbench.adopt_appearance(profile);

		const auto context = core::diff_request_context{
			.Request            = core::request_id{ 1U },
			.ApplicationSession = core::application_session_id{ 1U },
			.Key                = outcome.Result->Alignment.Key,
			.Generation         = core::request_generation{ 1U },
		};
		workbench.set_loading(context);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = context,
			.Result  = std::make_shared<const core::computed_file_diff>(
				*outcome.Result),
		}));
		process_events();
		expect(
			workbench.adopt_token_spans(
				core::diff_side::left,
				std::make_shared<const application::token_span_list>(
					classification.Spans)),
			"the classifier's spans arrive in the order the pane requires");
		process_events();

		settle_geometry(workbench);
		const auto  marker = line_of(left, comment_marker);
		auto* const bar    = workbench.shared_scrollbar();
		bar->setValue(
			bar->minimum()
			+ static_cast<int>(
				static_cast<double>(bar->maximum() - bar->minimum())
				* static_cast<double>(marker)
				/ static_cast<double>(line_of(left, "constexpr int Answer"))));
		process_events();

		const auto top = workbench.left_pane()
							 ->cursorForPosition(QPoint{ 0, 0 })
							 .blockNumber();
		expect(
			top > 2,
			"the line that opened the block comment has scrolled off the top, "
			"so nothing on screen says the marker line is a comment");

		const auto painted = block_formats(
			*workbench.left_pane(), static_cast<int>(marker));
		const auto comment = profile.color(core::appearance_role::syntax_comment);
		expect(!painted.isEmpty(), "the marker line is painted at all");
		expect(
			painted.front().format.foreground().color() == comment,
			"a line the complete-blob pass classified as comment is painted as "
			"one on the composed surface, with its opening delimiter hundreds "
			"of lines off screen");
	}

	void test_an_oversize_blob_paints_nothing_rather_than_a_prefix()
	{
		auto oversize = std::string{ };
		oversize.reserve(syntax::maximum_classified_blob_bytes + 64U);
		while (oversize.size() <= syntax::maximum_classified_blob_bytes) {
			oversize += "constexpr int filler = 0;\n";
		}

		auto       catalog    = syntax::packaged_definition_catalog{ };
		auto       blobs      = single_blob{ oversize };
		auto       classifier = syntax::packaged_syntax_classifier{ catalog, blobs };
		const auto definition = catalog.definition_for_path("review.cpp");
		const auto refused    = classifier.classify(
			{
				.Context      = { },
				.Key          = { .Blob = blob(21U), .Definition = definition },
				.MaximumSpans = 1'000'000U,
			},
			std::stop_token{ });
		expect(
			refused.Status
					== application::syntax_classification_status::limit_exceeded
				&& refused.Spans.empty(),
			"a blob above the bound is refused whole rather than classified as "
			"far as the bound reaches");

		// The pane's half of the same claim: nothing to adopt means unpainted
		// text, which is honest, rather than text whose tail is silently
		// unclassified, which is not.
		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		workbench.adopt_appearance(qt::appearance_profile{ });
		expect(
			!workbench.token_spans(core::diff_side::left),
			"a refused classification leaves the pane with no spans to paint");
	}

	/// \brief [[BPR-1559]]: a template call's name and argument are painted in
	/// their roles on the composed surface.
	void test_template_call_roles_reach_the_pane()
	{
		const auto left  = std::string{ "void calls()\n{\n\tauto a = getComponent<Type>();\n}\n" };
		const auto right = left + "// a trailing difference so the comparison has one\n";

		auto       leftBytes  = std::vector<std::uint8_t>{ left.begin(), left.end() };
		auto       rightBytes = std::vector<std::uint8_t>{ right.begin(), right.end() };
		const auto options    = core::diff_options{ };
		const auto outcome    = bpr::tests::product_file_diff({
			.Key        = comparison_key(options),
			.Options    = options,
			.LeftBytes  = std::span<const std::uint8_t>{ leftBytes },
			.RightBytes = std::span<const std::uint8_t>{ rightBytes },
		});
		expect(outcome.Reason == core::diff_reason_code::none && outcome.Result.has_value(),
			   "the template comparison computes");

		auto       catalog        = syntax::packaged_definition_catalog{ };
		auto       blobs          = single_blob{ left };
		auto       classifier     = syntax::packaged_syntax_classifier{ catalog, blobs };
		const auto classification = classifier.classify(
			{
				.Context      = { },
				.Key          = { .Blob = blob(21U), .Definition = catalog.definition_for_path("review.cpp") },
				.MaximumSpans = 1'000'000U,
			},
			std::stop_token{ });
		expect(classification.Status == application::syntax_classification_status::available,
			   "the packaged classifier answers for the template blob");

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		auto profile = qt::appearance_profile{ };
		static_cast<void>(profile.select_mode(core::appearance_mode::light));
		workbench.adopt_appearance(profile);
		const auto context = core::diff_request_context{
			.Request            = core::request_id{ 1U },
			.ApplicationSession = core::application_session_id{ 1U },
			.Key                = outcome.Result->Alignment.Key,
			.Generation         = core::request_generation{ 1U },
		};
		workbench.set_loading(context);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = context,
			.Result  = std::make_shared<const core::computed_file_diff>(*outcome.Result),
		}));
		process_events();
		expect(workbench.adopt_token_spans(core::diff_side::left,
										   std::make_shared<const application::token_span_list>(classification.Spans)),
			   "the template spans arrive in the order the pane requires");
		process_events();

		const auto line    = static_cast<int>(line_of(left, "getComponent"));
		const auto row     = std::string_view{ "\tauto a = getComponent<Type>();" };
		const auto painted = block_formats(*workbench.left_pane(), line);
		const auto colour  = [&](const std::string_view aWord) -> std::optional<QColor> {
			const auto column = static_cast<int>(row.find(aWord));
			for (const auto& range : painted) {
				if (range.start <= column && range.start + range.length >= column + static_cast<int>(aWord.size())) {
					return range.format.foreground().color();
				}
			}
			return std::nullopt;
		};
		const auto function = profile.color(core::appearance_role::syntax_function);
		const auto type     = profile.color(core::appearance_role::syntax_type);
		expect(function != type, "the light profile paints functions and types apart");
		expect(colour("getComponent") == function, "the template call's name is painted as a function");
		expect(colour("Type") == type, "the template argument is painted as a type");
	}

	/// \brief [[BPR-1710]]: `std` and a namespace the standard nests in it
	/// are painted in the namespace colour on the composed surface, in Light
	/// and Dark, while the standard name after them keeps the built-in colour.
	///
	/// \details With `BPR_STD_NAMESPACE_RENDER_ROOT` set, the composed
	/// workbench is also saved there as `light.png` and `dark.png` before the
	/// colours are judged, which is how the card's before and after renders
	/// were made.
	void test_std_namespace_reaches_the_pane()
	{
		const auto left = std::string{
			"#include <vector>\n"
			"\n"
			"namespace fs = std::filesystem;\n"
			"\n"
			"auto load(const fs::path& aPath) -> std::vector<std::string>\n"
			"{\n"
			"\tusing namespace std::chrono_literals;\n"
			"\tauto lines = std::vector<std::string>{ };\n"
			"\tauto found = std::ranges::find(lines, \"x\");\n"
			"\tauto limit = std::numeric_limits<int>::max();\n"
			"\tauto align = Qt::AlignLeft;\n"
			"\treturn lines;\n"
			"}\n"
		};
		auto right = left;
		right.replace(right.find("\"x\""), 3U, "aPath.string()");

		auto       leftBytes  = std::vector<std::uint8_t>{ left.begin(), left.end() };
		auto       rightBytes = std::vector<std::uint8_t>{ right.begin(), right.end() };
		const auto options    = core::diff_options{ };
		const auto outcome    = bpr::tests::product_file_diff({
			.Key        = comparison_key(options),
			.Options    = options,
			.LeftBytes  = std::span<const std::uint8_t>{ leftBytes },
			.RightBytes = std::span<const std::uint8_t>{ rightBytes },
		});
		expect(outcome.Reason == core::diff_reason_code::none && outcome.Result.has_value(),
			   "the std comparison computes");

		auto       catalog    = syntax::packaged_definition_catalog{ };
		const auto definition = catalog.definition_for_path("review.cpp");
		const auto classify   = [&](const std::string& aText) {
			auto       blobs  = single_blob{ aText };
			auto       reader = syntax::packaged_syntax_classifier{ catalog, blobs };
			const auto result = reader.classify(
				{
					.Context      = { },
					.Key          = { .Blob = blob(21U), .Definition = definition },
					.MaximumSpans = 1'000'000U,
				},
				std::stop_token{ });
			expect(result.Status == application::syntax_classification_status::available,
				   "the packaged classifier answers for the std blob");
			return std::make_shared<const application::token_span_list>(result.Spans);
		};

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'120, 560);
		workbench.show();
		process_events();
		const auto context = core::diff_request_context{
			.Request            = core::request_id{ 1U },
			.ApplicationSession = core::application_session_id{ 1U },
			.Key                = outcome.Result->Alignment.Key,
			.Generation         = core::request_generation{ 1U },
		};
		workbench.set_loading(context);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = context,
			.Result  = std::make_shared<const core::computed_file_diff>(*outcome.Result),
		}));
		process_events();
		expect(workbench.adopt_token_spans(core::diff_side::left, classify(left))
				   && workbench.adopt_token_spans(core::diff_side::right, classify(right)),
			   "both sides' spans arrive in the order the pane requires");
		settle_geometry(workbench);

		const auto line  = static_cast<int>(line_of(left, "std::ranges::find"));
		const auto row   = std::string_view{ "\tauto found = std::ranges::find(lines, \"x\");" };
		const auto modes = std::array{ core::appearance_mode::light, core::appearance_mode::dark };
		const auto root  = qEnvironmentVariable("BPR_STD_NAMESPACE_RENDER_ROOT");
		for (const auto mode : modes) {
			if (root.isEmpty()) {
				break;
			}
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			workbench.adopt_appearance(profile);
			process_events();
			const auto name = mode == core::appearance_mode::light ? QStringLiteral("/light.png") : QStringLiteral("/dark.png");
			expect(workbench.grab().save(root + name), "the composed workbench is saved offscreen");
		}
		for (const auto mode : modes) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			workbench.adopt_appearance(profile);
			process_events();
			const auto painted = block_formats(*workbench.left_pane(), line);
			const auto colour  = [&](const std::string_view aWord) -> std::optional<QColor> {
				const auto column = static_cast<int>(row.find(aWord));
				for (const auto& range : painted) {
					if (range.start <= column && range.start + range.length >= column + static_cast<int>(aWord.size())) {
						return range.format.foreground().color();
					}
				}
				return std::nullopt;
			};
			const auto namespaceColour = profile.color(core::appearance_role::syntax_namespace);
			const auto builtin         = profile.color(core::appearance_role::syntax_builtin);
			expect(namespaceColour != builtin, "the profile paints namespaces and built-ins apart");
			expect(colour("std") == namespaceColour, "std is painted in the namespace colour");
			expect(colour("ranges") == namespaceColour, "std::ranges is painted in the namespace colour");
			expect(colour("find") == builtin, "the standard name after the namespaces keeps the built-in colour");
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(
		QStringLiteral("BetterPullReviewSyntaxPaneCompositionTests"));
	test_classified_blob_reaches_the_pane();
	test_an_oversize_blob_paints_nothing_rather_than_a_prefix();
	test_template_call_roles_reach_the_pane();
	test_std_namespace_reaches_the_pane();
	std::cout << "syntax-pane-composition-tests:pass\n";
	return EXIT_SUCCESS;
}
