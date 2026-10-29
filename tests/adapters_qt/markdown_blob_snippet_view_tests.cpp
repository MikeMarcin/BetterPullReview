// [[BPR-1802]]. A blob line permalink in the pull request's repository,
// alone on a line, reads as GitHub's snippet box once the host answers with
// its lines: header `repo/path`, "Lines A to B in" and the short commit, the
// lines numbered and coloured through the composed classifier. Until then,
// for another repository, inside a sentence or out of bound it keeps the
// compact link; a late answer for a body that no longer holds it is ignored.
// A published tour stop's pinned source shows only as its box, above the
// narrative.

#include "../../src/adapters/qt/src/markdown_code.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/syntax/packaged_syntax_classifier.hpp"
#include "bpr/application/markdown_blob_snippet_host.hpp"
#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QImage>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFragment>
#include <QTextLayout>

#include <array>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using bpr::tests::expect;

	constexpr auto ordinal_property = QTextFormat::UserProperty + 0x0B60;
	constexpr auto row_property     = QTextFormat::UserProperty + 0x0B62;
	constexpr auto number_property  = QTextFormat::UserProperty + 0x0B63;

	void settle()
	{
		QApplication::processEvents();
		QApplication::processEvents();
	}

	struct theme
	{
		core::appearance_mode    Mode;
		core::appearance_variant Variant;
		const char*              Name;
	};
	constexpr auto themes = std::array{
		theme{ core::appearance_mode::light, core::appearance_variant::light, "light" },
		theme{ core::appearance_mode::dark, core::appearance_variant::dark, "dark" },
		theme{ core::appearance_mode::high_contrast_light, core::appearance_variant::high_contrast_light, "high-contrast-light" },
		theme{ core::appearance_mode::high_contrast_dark, core::appearance_variant::high_contrast_dark, "high-contrast-dark" },
	};

	auto palette_of(const core::appearance_mode aMode) -> QPalette
	{
		auto profile = qt::appearance_profile{ };
		profile.select_mode(aMode);
		return profile.derive(QApplication::palette());
	}

	auto code_theme(const core::appearance_variant aVariant) -> qt::markdown_code_theme
	{
		return { .Colors = qt::code_role_colors_of(qt::builtin_appearance_palette(aVariant)), .Variant = aVariant };
	}

	class held_texts final : public adapters::syntax::blob_source
	{
	public:
		explicit held_texts(application::markdown_code_texts& aTexts)
			: Texts{ aTexts }
		{
		}
		auto blob_bytes(const core::git_blob_oid& aBlob) -> std::optional<std::string> override { return Texts.bytes(aBlob); }

	private:
		application::markdown_code_texts& Texts;
	};

	class manual_executor final : public application::decision_executor, public application::effect_executor
	{
	public:
		void                              post(std::function<void()> aTask) override { Tasks.push_back(std::move(aTask)); }
		std::deque<std::function<void()>> Tasks;
	};

	/// The file the fixture's permalinks name, at their commit.
	constexpr auto frame_file = std::string_view{ "#include <algorithm>\n"
												  "\n"
												  "int clamp_width(int aWidth, int aMaximum)\n"
												  "{\n"
												  "\t// Never wider than the frame allows.\n"
												  "\treturn std::clamp(aWidth, 0, aMaximum);\n"
												  "}\n" };

	/// Answers every read from `frame_file`, as the review's Git cache would.
	class fixture_source final : public application::markdown_blob_source_port
	{
	public:
		explicit fixture_source(manual_executor& aDecisions)
			: Decisions{ aDecisions }
		{
		}

		auto read(const application::markdown_blob_source_read& aRead, std::stop_token /*aStop*/, std::function<void(application::markdown_blob_source_result)> aCompletion) -> bool override
		{
			Paths.push_back(aRead.Permalink.Path);
			Decisions.post([path = aRead.Permalink.Path, completion = std::move(aCompletion)] {
				auto result   = application::markdown_blob_source_result{ };
				result.Status = path == "src/frame.cpp" ? application::markdown_blob_source_status::read : application::markdown_blob_source_status::absent;
				result.Bytes  = path == "src/frame.cpp" ? std::string{ frame_file } : std::string{ };
				completion(std::move(result));
			});
			return true;
		}

		manual_executor&         Decisions;
		std::vector<std::string> Paths;
	};

	constexpr auto commit_text   = std::string_view{ "0123456789abcdef0123456789abcdef01234567" };
	constexpr auto standing_text = std::string_view{ "https://github.com/octo-org/widgets/blob/0123456789abcdef0123456789abcdef01234567/src/frame.cpp#L3-L7" };

	auto own_repository() -> core::markdown_repository
	{
		return { .Host = "github.com", .Owner = "octo-org", .Repository = "widgets" };
	}

	/// The composed path: the snippet host over a fixture file, the
	/// reader's code classification over the packaged definitions.
	struct composed
	{
		manual_executor                                             Effects;
		manual_executor                                             Decisions;
		fixture_source                                              Source{ Decisions };
		std::optional<application::markdown_blob_snippet_scope>     Scope = application::markdown_blob_snippet_scope{ .Host = "github.com", .Owner = "octo-org", .Repository = "widgets", .Identity = "pr-7@head" };
		std::vector<application::markdown_blob_snippet_publication> Snippets;
		application::markdown_blob_snippet_host                     Host{ Source, Decisions, [this] { return Scope; },
																		  [this](const application::markdown_blob_snippet_publication& aSnippet) { Snippets.push_back(aSnippet); } };
		application::markdown_code_texts                            Texts;
		held_texts                                                  Held{ Texts };
		adapters::syntax::packaged_definition_catalog               Catalog;
		adapters::syntax::packaged_syntax_classifier                Classifier{ Catalog, Held };
		std::vector<application::markdown_code_syntax_publication>  Spans;
		application::markdown_code_syntax_host                      Code{ Classifier, Texts, Effects, Decisions,
																		  [this](const std::string_view aSelector) -> std::optional<application::syntax_definition_version> {
														 auto definition = Catalog.definition_for_path(aSelector);
														 if (definition.Name == adapters::syntax::plain_text_definition_name) {
															 return std::nullopt;
														 }
														 return definition;
													 },
													 [this](const application::markdown_code_syntax_publication& aPublication) { Spans.push_back(aPublication); } };
		std::size_t Asked = 0U;

		void bind(qt::markdown_body_view& aView)
		{
			aView.set_permalink_repository(own_repository());
			aView.set_code_syntax_dispatch([this](std::vector<application::markdown_code_syntax_request> aRequests) { Code.request(aRequests); });
			aView.set_permalink_dispatch([this](std::vector<application::markdown_blob_snippet_request> aRequests) {
				Asked += aRequests.size();
				Host.request(aRequests);
			});
		}

		void deliver(const std::vector<qt::markdown_body_view*>& aViews)
		{
			for (auto round = 0; round < 8; ++round) {
				for (auto* lane : { &Effects, &Decisions }) {
					while (!lane->Tasks.empty()) {
						auto task = std::move(lane->Tasks.front());
						lane->Tasks.pop_front();
						task();
					}
				}
				for (const auto& snippet : std::exchange(Snippets, { })) {
					for (auto* view : aViews) {
						view->apply_permalink_snippet(snippet);
					}
				}
				for (const auto& spans : std::exchange(Spans, { })) {
					for (auto* view : aViews) {
						view->apply_code_syntax(spans);
					}
				}
				settle();
			}
		}
	};

	auto document_of(qt::markdown_body_view& aView) -> QTextDocument& { return *aView.rendered_view()->document(); }

	struct link_fact
	{
		QString Text;
		QString Href;
		auto    operator==(const link_fact&) const -> bool = default;
	};

	auto links_of(QTextDocument& aDocument) -> std::vector<link_fact>
	{
		auto found = std::vector<link_fact>{ };
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			for (auto it = block.begin(); !it.atEnd(); ++it) {
				const auto fragment = it.fragment();
				const auto href     = fragment.charFormat().anchorHref();
				if (href.isEmpty()) {
					continue;
				}
				if (!found.empty() && found.back().Href == href && fragment.position() > 0 && aDocument.characterAt(fragment.position() - 1) != QChar::ParagraphSeparator) {
					found.back().Text += fragment.text();
				} else {
					found.push_back({ fragment.text(), href });
				}
			}
		}
		return found;
	}

	auto rows_of(QTextDocument& aDocument) -> std::vector<QTextBlock>
	{
		auto rows = std::vector<QTextBlock>{ };
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			if (block.blockFormat().intProperty(row_property) == static_cast<int>(qt::detail::markdown_code_row::snippet)) {
				rows.push_back(block);
			}
		}
		return rows;
	}

	auto header_of(QTextDocument& aDocument) -> QTextBlock
	{
		for (auto block = aDocument.begin(); block.isValid(); block = block.next()) {
			if (block.blockFormat().hasProperty(ordinal_property) && block.blockFormat().intProperty(row_property) == static_cast<int>(qt::detail::markdown_code_row::header)) {
				return block;
			}
		}
		return { };
	}

	/// A permalink alone on a line, prose, another repository's permalink and
	/// the first one again inside a sentence.
	auto fixture_body() -> std::string
	{
		auto text = std::string{ standing_text };
		text += "\nThe width is clamped here, so a wide frame never overflows.\n\nCompare https://github.com/octo-org/gadgets/blob/";
		text += commit_text;
		text += "/src/frame.cpp#L3 and, in a sentence, ";
		text += standing_text;
		text += " too.\n";
		return text;
	}

	void check_box(composed& aPath)
	{
		const auto commit   = std::string{ commit_text };
		const auto standing = std::string{ standing_text };
		const auto body     = fixture_body();
		auto       view     = qt::markdown_body_view{ };
		view.resize(720, 600);
		view.show();
		aPath.bind(view);
		view.set_body(body, "comment:1");
		settle();
		// Before the answer: the compact link, without the owner in the
		// pull request's repository; another repository keeps its owner.
		auto links = links_of(document_of(view));
		expect(links.size() == 3U && links[0].Text == QStringLiteral("widgets/src/frame.cpp#L3-L7 at 0123456") && links[0].Href == QString::fromStdString(standing),
			   "until its lines arrive the permalink is the compact link, its owner dropped");
		expect(links[1].Text == QStringLiteral("octo-org/gadgets/src/frame.cpp#L3 at 0123456"), "another repository's link keeps its owner");
		expect(view.permalink_snippets_shown() == 0U && aPath.Asked == 1U, "only the standing permalink in the repository is asked for");
		aPath.deliver({ &view });
		auto& document = document_of(view);
		expect(view.permalink_snippets_shown() == 1U, "the answer shows one box");
		const auto header = header_of(document);
		expect(header.isValid() && header.text() == QStringLiteral("widgets/src/frame.cpp") + QChar{ QChar::LineSeparator } + QStringLiteral("Lines 3 to 7 in 0123456"),
			   "the header is GitHub's: repo/path, then Lines 3 to 7 in the short commit");
		links = links_of(document);
		expect(links.size() == 4U && links[0] == link_fact{ QStringLiteral("widgets/src/frame.cpp"), QString::fromStdString(standing) }
				   && links[1] == link_fact{ QStringLiteral("0123456"), QStringLiteral("https://github.com/octo-org/widgets/commit/") + QString::fromStdString(commit) },
			   "the title links the permalink and the commit links its page");
		expect(links[3].Text == QStringLiteral("widgets/src/frame.cpp#L3-L7 at 0123456"), "a permalink inside a sentence keeps the compact link");
		const auto rows = rows_of(document);
		expect(rows.size() == 5U && rows[0].text() == QStringLiteral("int clamp_width(int aWidth, int aMaximum)") && rows[4].text() == QStringLiteral("}"), "the box holds lines three to seven");
		expect(rows[0].blockFormat().intProperty(number_property) == 3 && rows[4].blockFormat().intProperty(number_property) == 7, "each line is numbered as in the file");
		expect(rows[0].layout() != nullptr && !rows[0].layout()->formats().isEmpty(), "the lines are coloured by the file's language");
		expect(document.toRawText().contains(QStringLiteral("The width is clamped here")), "the prose after the box stays");
		expect(view.original_body() == body, "the body's source is unchanged");
		// An answer for lines the body does not hold changes nothing.
		auto other       = *core::recognize_markdown_blob_permalink("https://github.com/octo-org/widgets/blob/" + commit + "/src/frame.cpp#L1");
		auto publication = application::markdown_blob_snippet_publication{ .Permalink = other, .Lines = std::make_shared<const std::vector<std::string>>(std::vector<std::string>{ "x" }) };
		view.apply_permalink_snippet(publication);
		expect(view.permalink_snippets_shown() == 1U, "an answer for other lines is ignored");
		// A new body drops what the old one held; a late answer for it is ignored.
		view.set_body("Nothing to embed.", "comment:2");
		auto late = application::markdown_blob_snippet_publication{ .Permalink = *core::recognize_markdown_blob_permalink(standing), .Lines = std::make_shared<const std::vector<std::string>>(std::vector<std::string>{ "late" }) };
		view.apply_permalink_snippet(late);
		expect(view.permalink_snippets_shown() == 0U && !document_of(view).toRawText().contains(QStringLiteral("late")), "a late answer for a replaced body is dropped");
		// Without the repository nothing is asked and nothing embeds.
		auto bare = qt::markdown_body_view{ };
		bare.set_permalink_dispatch([&aPath](std::vector<application::markdown_blob_snippet_request>) { ++aPath.Asked; });
		const auto asked = aPath.Asked;
		bare.set_body(body, "comment:3");
		bare.apply_permalink_snippet(late);
		expect(aPath.Asked == asked && bare.permalink_snippets_shown() == 0U, "without the pull request's repository nothing is asked or shown");
	}

	void check_lead(composed& aPath)
	{
		const auto standing = std::string{ standing_text };
		const auto stop     = standing + "\n\nThe width is clamped here.\n";
		const auto start    = stop.find("The width");
		auto       view     = qt::markdown_body_view{ };
		aPath.bind(view);
		view.set_rendered_range(std::pair{ start, stop.size() });
		view.set_rendered_lead(std::pair{ std::size_t{ 0U }, standing.size() });
		view.set_body(stop, "stop:1");
		settle();
		expect(view.permalink_snippets_shown() == 0U && !document_of(view).toRawText().contains(QStringLiteral("frame.cpp")), "until its lines arrive a pinned source shows nothing");
		aPath.deliver({ &view });
		expect(view.permalink_snippets_shown() == 1U && header_of(document_of(view)).position() == 0, "with its lines it is the box above the narrative");
		expect(view.original_body() == stop, "the stop's source is unchanged");
	}

	/// The box in every theme, as a post reads in place in the Comments drawer.
	void capture(composed& aPath)
	{
		const auto body = fixture_body();
		const auto root = qEnvironmentVariable("BPR_PERMALINK_SNIPPET_RENDER_DIR");
		if (root.isEmpty()) {
			return;
		}
		expect(QDir{ }.mkpath(root), "create capture directory");
		auto code = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		if (QFontDatabase::hasFamily(QStringLiteral("Menlo"))) {
			code = QFont{ QStringLiteral("Menlo"), 12 };
		}
		const auto tab = 4.0 * QFontMetricsF{ code }.horizontalAdvance(QLatin1Char{ ' ' });
		for (const auto& theme : themes) {
			for (const auto loaded : { false, true }) {
				auto holder = QWidget{ };
				holder.setPalette(palette_of(theme.Mode));
				holder.setAutoFillBackground(true);
				holder.show();
				auto* const post = new qt::markdown_body_view{ &holder };
				post->set_in_place(true);
				post->set_code_font(code, tab);
				post->set_code_theme(code_theme(theme.Variant));
				aPath.bind(*post);
				post->set_body(body, std::string{ "capture:" } + theme.Name + (loaded ? ":loaded" : ":loading"));
				post->show();
				if (loaded) {
					aPath.deliver({ post });
				}
				settle();
				const auto height = post->heightForWidth(480);
				holder.resize(496, height + 16);
				post->setGeometry(8, 8, 480, height);
				settle();
				const auto name = QStringLiteral("permalink-snippet-") + (loaded ? QStringLiteral("box-") : QStringLiteral("link-")) + QString::fromLatin1(theme.Name) + QStringLiteral(".png");
				expect(holder.grab().save(root + QLatin1Char{ '/' } + name), "save capture");
			}
		}
	}
} // namespace

int main(int aArgc, char** aArgv)
try {
	QApplication app{ aArgc, aArgv };
	auto         path = composed{ };
	check_box(path);
	check_lead(path);
	capture(path);
	std::cout << "markdown-blob-snippet-view-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
