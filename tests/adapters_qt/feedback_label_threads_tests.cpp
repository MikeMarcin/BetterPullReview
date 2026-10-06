/// \file
/// \brief [[BPR-1083]]. Proves the thread marks (owner, BPR-1079 v4; RFC-BPR-0050
/// R31, R32): every post's 16 px mark under its 18 px avatar, the rail in the
/// label's tint that adds no width, blocking's fill and wider rail, the reply's
/// own mark, the body starting at the subject while the source keeps the prefix,
/// the names "tbutler, issue (blocking)", and the two mark styles.

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/feedback_label_marks.hpp"
#include "bpr/qt/github_conversation_delegate.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QClipboard>
#include <QHelpEvent>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPoint>
#include <QRect>
#include <QScrollBar>
#include <QTextEdit>
#include <QToolButton>
#include <QToolTip>
#include <QTreeView>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using conversation = application::github_conversation_projection;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		for (auto round = 0; round < 4; ++round) {
			QApplication::processEvents();
		}
	}

	/// \brief Saves a widget's render when `BPR_FEEDBACK_RENDER_DIR` names a
	/// folder: layout evidence only, never a substitute for a native look.
	void save_render(QWidget& aWidget, const char* const aName)
	{
		const auto folder = qEnvironmentVariable("BPR_FEEDBACK_RENDER_DIR");
		if (!folder.isEmpty()) {
			static_cast<void>(aWidget.grab().save(folder + QLatin1Char{ '/' } + QString::fromLatin1(aName) + QStringLiteral(".png")));
		}
	}

	[[nodiscard]] auto forest() -> std::vector<conversation>
	{
		const auto post = [](std::string aId, std::string aAuthor, std::string aBody) {
			return conversation{
				.Id          = std::move(aId),
				.ParentId    = std::string{ "thread-1" },
				.Kind        = application::github_conversation_kind::review_comment,
				.AuthorLogin = std::move(aAuthor),
				.Body        = std::move(aBody),
				.Path        = "src/light.cpp",
				.Line        = 12U,
			};
		};
		return {
			conversation{ .Id = "thread-1", .Kind = application::github_conversation_kind::review_thread, .Path = "src/light.cpp", .Line = 12U, .DiffSide = application::github_conversation_diff_side::right, .AnchorAvailability = application::github_conversation_anchor_availability::current },
			post("c1", "tbutler", "issue (blocking): LightScale ignores the view's post-process settings."),
			post("c2", "MikeMarcin", "question: Can the view settings be null on secondary views?"),
			post("c3", "jiayin", "**praise:** Good catch."),
			post("c4", "tbutler", "Thanks, resolving after CI."),
		};
	}

	[[nodiscard]] auto theme() -> qt::feedback_mark_theme
	{
		return { .Tints = qt::feedback_label_tints_of(qt::builtin_appearance_palette(core::appearance_variant::light)) };
	}

	void test_cards_mark_every_post_under_its_avatar()
	{
		qt::conversation_cards_view view;
		view.set_label_marks(theme());
		view.resize(420, 900);
		view.show();
		view.adopt(forest());
		settle();
		expect(view.cards().size() == 1U && view.cards().front().Posts.size() == 4U, "one thread of four posts");
		const auto& posts = view.cards().front().Posts;
		expect(posts[0].Label == core::feedback_label::issue && posts[0].Emphasis == qt::feedback_mark_emphasis::blocking && posts[0].Name == QStringLiteral("tbutler, issue (blocking)"),
			   "the root is a blocking issue named with its author first");
		expect(posts[1].Label == core::feedback_label::question && posts[1].Emphasis == qt::feedback_mark_emphasis::undecorated && posts[2].Label == core::feedback_label::praise,
			   "each reply shows its own mark, which may differ from its root's");
		expect(!posts[3].Label.has_value() && posts[3].Name == QStringLiteral("tbutler"), "a plain post has no mark");
		expect(posts[0].Body == QStringLiteral("LightScale ignores the view's post-process settings.") && posts[2].Body == QStringLiteral("Good catch.")
				   && posts[3].Body == QStringLiteral("Thanks, resolving after CI."),
			   "the mark stands in for the prefix, so the body starts at the subject");
		for (const auto& post : posts) {
			expect(post.Rect.left() == posts.front().Rect.left(), "every post's text keeps the same left edge: the rail adds no width");
			const auto mark = view.mark_rect(post);
			if (post.Label.has_value()) {
				expect(mark.width() == 16 && mark.height() == 16 && mark.left() == post.Rect.left() + 1 && mark.top() == post.Rect.top() + 18 + 3,
					   "a 16 px mark is centred under the 18 px avatar, 3 px below it");
				expect(post.Rect.height() >= 18 + 3 + 16, "a marked post is tall enough for its mark");
			} else {
				expect(mark.isEmpty(), "a plain post has no mark rect");
			}
			const auto rail = view.rail_rect(view.cards().front(), post);
			expect(rail.right() == post.Rect.left() - 3 && rail.width() == (post.Emphasis == qt::feedback_mark_emphasis::blocking ? 4.0 : 2.0) && rail.left() >= view.cards().front().Rect.left(),
				   "the rail runs 3 px left of the avatar, 2 px or 4 px when blocking, growing outward inside the card");
		}

		auto image = QImage{ view.viewport()->size(), QImage::Format_ARGB32_Premultiplied };
		view.viewport()->render(&image);
		const auto tints  = theme().Tints;
		const auto issue  = view.mark_rect(posts[0]).translated(0, -view.verticalScrollBar()->value());
		const auto filled = QColor{ image.pixel(issue.left() + 2, issue.center().y()) };
		expect(filled == tints[3], "the blocking issue mark is filled with the issue tint on screen");
		const auto question = view.mark_rect(posts[1]).translated(0, -view.verticalScrollBar()->value());
		expect(QColor{ image.pixel(question.center().x(), question.top()) } == tints[5], "the question mark's border is the question tint");
		const auto rail = view.rail_rect(view.cards().front(), posts[1]).translated(0, -view.verticalScrollBar()->value());
		expect(QColor{ image.pixel(static_cast<int>(rail.center().x()), static_cast<int>(rail.center().y())) } == tints[5], "the reply's rail is in its own tint");

		save_render(view, "cards-icons");
		auto letters  = theme();
		letters.Style = qt::feedback_mark_style::letters;
		view.set_label_marks(letters);
		settle();
		save_render(view, "cards-letters");
		auto lettered = QImage{ view.viewport()->size(), QImage::Format_ARGB32_Premultiplied };
		view.viewport()->render(&lettered);
		expect(lettered.copy(question) != image.copy(question) && QColor{ lettered.pixel(question.center().x(), question.top()) } == tints[5],
			   "letters change the mark's middle and keep its tint and border");
	}

	/// \brief The owner, rejecting `thread-marks-v002`: "Marks don't show on
	/// inline comments." The drawer's entries are compact; each leads with its
	/// root's mark, inline thread and draft alike, and keeps the root's rail.
	void test_compact_entries_lead_with_the_root_mark()
	{
		qt::conversation_cards_view view;
		view.set_compact(true);
		view.set_label_marks(theme());
		view.resize(320, 600);
		view.show();
		view.adopt(forest());
		settle();
		save_render(view, "cards-compact");
		const auto& card = view.cards().front();
		const auto  rail = view.rail_rect(card, card.Posts.front());
		expect(view.mark_rect(card.Posts.front()).isEmpty() && rail.width() == 4.0 && rail.left() > card.Rect.left() && rail.right() < card.Rect.left() + 10,
			   "a compact entry has its root's blocking rail in its own padding");
		const auto mark = view.compact_mark_rect(card);
		expect(mark.width() == 16 && mark.height() == 16 && mark.left() > rail.right() && card.Rect.contains(mark),
			   "and its root's 16 px mark leads the entry, right of the rail, inside the card");
		auto image = QImage{ view.viewport()->size(), QImage::Format_ARGB32_Premultiplied };
		view.viewport()->render(&image);
		const auto shown = mark.translated(0, -view.verticalScrollBar()->value());
		expect(QColor{ image.pixel(shown.left() + 2, shown.center().y()) } == theme().Tints[3], "the blocking issue mark is drawn, filled with the issue tint");
		// The entry's text starts after the mark: nothing of the text is
		// drawn over it, and the mark leaves the line's first glyph unmoved
		// relative to the other lines of the entry.
		auto plainForest    = forest();
		plainForest[1].Body = "LightScale ignores the view's post-process settings.";
		qt::conversation_cards_view plain;
		plain.set_compact(true);
		plain.set_label_marks(theme());
		plain.resize(320, 600);
		plain.show();
		plain.adopt(plainForest);
		settle();
		expect(plain.compact_mark_rect(plain.cards().front()).isEmpty(), "a plain root's entry has no mark");
		auto plainImage = QImage{ plain.viewport()->size(), QImage::Format_ARGB32_Premultiplied };
		plain.viewport()->render(&plainImage);
		const auto firstInk = [](const QImage& aImage, const QRect& aCard) {
			const auto background = QColor{ aImage.pixel(aCard.right() - 4, aCard.top() + 4) };
			for (auto x = aCard.left() + 10; x < aCard.right(); ++x) {
				for (auto y = aCard.top() + 6; y < aCard.top() + 22; ++y) {
					const auto colour = QColor{ aImage.pixel(x, y) };
					if (std::abs(colour.lightness() - background.lightness()) > 60) {
						return x;
					}
				}
			}
			return -1;
		};
		const auto markedCard = card.Rect.translated(0, -view.verticalScrollBar()->value());
		const auto plainCard  = plain.cards().front().Rect.translated(0, -plain.verticalScrollBar()->value());
		expect(firstInk(plainImage, plainCard) >= 0 && firstInk(image, markedCard) == shown.left(),
			   "the marked entry's first ink is its mark; the plain entry starts with its text");
		auto       tip = QString{ };
		QHelpEvent help{ QEvent::ToolTip, shown.center(), view.viewport()->mapToGlobal(shown.center()) };
		QApplication::sendEvent(view.viewport(), &help);
		tip = QToolTip::text();
		expect(tip == QStringLiteral("tbutler, issue (blocking)"), "the mark's tooltip names the root post and its label");
		QToolTip::hideText();

		// My draft's entry leads with its own mark, in its own decoration.
		const auto comparison = core::file_comparison_digest{ };
		view.set_draft(core::review_comment_draft{
						   .Anchor = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 11U },
						   .Body   = "suggestion (non-blocking): Clamp the width.",
					   },
					   comparison);
		settle();
		const auto draft = std::ranges::find_if(view.cards(), [](const qt::thread_card& aCard) { return aCard.Draft; });
		expect(draft != view.cards().end() && draft->Posts.front().Label == core::feedback_label::suggestion
				   && draft->Posts.front().Emphasis == qt::feedback_mark_emphasis::non_blocking && !view.compact_mark_rect(*draft).isEmpty(),
			   "a draft entry leads with its non-blocking suggestion mark");
		expect(view.rail_rect(*draft, draft->Posts.front()).width() == 2.0, "and keeps a 2 px rail");
		save_render(view, "cards-compact-draft");
	}

	void test_the_post_list_and_the_body_reader()
	{
		auto model = qt::github_conversation_model{ };
		model.apply(forest());
		const auto root  = model.index(0, 0);
		const auto first = model.index(0, 0, root);
		expect(first.data(qt::github_conversation_model::feedback_label_role).toInt() == static_cast<int>(core::feedback_label::issue)
				   && first.data(qt::github_conversation_model::feedback_blocking_role).toBool()
				   && first.data(qt::github_conversation_model::feedback_name_role).toString() == QStringLiteral("issue (blocking)"),
			   "the model reads each post's mark once from its accepted body");
		const auto spoken = first.data(Qt::AccessibleTextRole).toString();
		expect(spoken.startsWith(QStringLiteral("Review comment by @tbutler, issue (blocking)\nLightScale")), "the post's accessible name is its author and then its mark");
		expect(first.data(qt::github_conversation_model::body_role).toString().startsWith(QStringLiteral("issue (blocking): ")), "the body role keeps the prefix");
		expect(!model.index(3, 0, root).data(qt::github_conversation_model::feedback_label_role).isValid(), "a plain post has no label");

		auto view     = QTreeView{ };
		auto delegate = new qt::github_conversation_delegate{ &view, true };
		delegate->set_label_marks(theme());
		view.setModel(&model);
		view.setItemDelegate(delegate);
		view.setRootIsDecorated(false);
		view.setIndentation(0);
		view.expandAll();
		view.resize(420, 600);
		view.show();
		settle();
		save_render(view, "post-list");
		auto image = QImage{ view.viewport()->size(), QImage::Format_ARGB32_Premultiplied };
		view.viewport()->render(&image);
		const auto row   = view.visualRect(first);
		auto       found = false;
		for (auto x = row.left(); x < row.left() + 60 && !found; ++x) {
			for (auto y = row.top(); y < row.top() + 24 && !found; ++y) {
				found = QColor{ image.pixel(x, y) } == theme().Tints[3];
			}
		}
		expect(found, "the post list draws the issue mark and rail at the start of the row");

		auto reader = qt::markdown_body_view{ };
		reader.set_body("issue (blocking): LightScale **ignores** the view.", "c1");
		expect(reader.rendered_view()->toPlainText().startsWith(QStringLiteral("issue (blocking):")), "without marks a reader shows the text as written");
		reader.set_feedback_marks(theme());
		reader.resize(420, 200);
		save_render(reader, "reader");
		expect(reader.rendered_view()->toPlainText().startsWith(QStringLiteral("LightScale ignores")), "with marks the rendered body starts at the subject");
		expect(reader.feedback_mark_label()->isVisibleTo(&reader) && reader.feedback_mark_label()->accessibleName() == QStringLiteral("issue (blocking)")
				   && reader.feedback_mark_label()->toolTip() == QStringLiteral("issue (blocking)"),
			   "and the mark leads it, named issue (blocking)");
		expect(reader.source_view()->toPlainText() == QStringLiteral("issue (blocking): LightScale **ignores** the view."), "View source keeps the prefix byte for byte");
		reader.copy_source();
		expect(QApplication::clipboard()->text() == QStringLiteral("issue (blocking): LightScale **ignores** the view."), "and so does Copy source");
		reader.set_body("Plain text.", "c4");
		expect(!reader.feedback_mark_label()->isVisibleTo(&reader) && reader.rendered_view()->toPlainText().startsWith(QStringLiteral("Plain text.")), "a plain body has no mark");
	}
	/// \brief The owner's toggle (2026-10-05): Label marks, Icons or Letters,
	/// on Reviews & queue beside the Comment labels settings, persisted with the
	/// profile's review behaviour, and applied to every surface of the window.
	void test_the_mark_style_setting_reaches_every_surface()
	{
		qt::workbench_window window;
		auto                 records = std::vector<application::review_behaviour_settings>{ };
		window.set_behaviour_dispatch([&records](const application::review_behaviour_settings& aSettings) { records.push_back(aSettings); });
		auto* const icons   = window.findChild<QToolButton*>(QStringLiteral("feedbackMarksIcons"));
		auto* const letters = window.findChild<QToolButton*>(QStringLiteral("feedbackMarksLetters"));
		auto* const reset   = window.findChild<QToolButton*>(QStringLiteral("resetCommentLabels"));
		expect(icons != nullptr && letters != nullptr && reset != nullptr && window.settings()->page_widget(qt::settings_page::reviews_and_queue)->isAncestorOf(letters),
			   "Reviews & queue offers Label marks, Icons or Letters, in a Comment labels group");
		expect(icons->isChecked() && window.label_marks().Style == qt::feedback_mark_style::icons
				   && window.review_editor()->label_controls()->theme().Style == qt::feedback_mark_style::icons
				   && window.conversation_panel()->cards()->label_marks().Style == qt::feedback_mark_style::icons,
			   "icons are the default everywhere");
		letters->click();
		expect(!records.empty() && records.back().FeedbackMarks == application::review_feedback_marks::letters,
			   "choosing Letters dispatches the whole behaviour record with letters");
		auto projection      = application::workbench_projection{ };
		projection.Behaviour = records.back();
		window.publish(projection);
		settle();
		expect(window.label_marks().Style == qt::feedback_mark_style::letters
				   && window.review_editor()->label_controls()->theme().Style == qt::feedback_mark_style::letters
				   && window.conversation_panel()->cards()->label_marks().Style == qt::feedback_mark_style::letters,
			   "the published choice reaches the editor's chip, menu and completion, and the threads");
		window.set_label_tints(qt::feedback_label_tints_of(qt::builtin_appearance_palette(core::appearance_variant::dark)));
		expect(window.conversation_panel()->cards()->label_marks().Tints[3] == QColor{ 0xFF, 0x7B, 0x72 }
				   && window.review_editor()->label_controls()->theme().Style == qt::feedback_mark_style::letters,
			   "a theme's tints arrive without changing the style");
		reset->click();
		expect(records.back().FeedbackMarks == application::review_feedback_marks::icons, "Reset returns the Comment labels group to icons");
	}
	/// \brief The owner, accepting the toggle (mark-style-v001): "it'd be nice
	/// to see a preview". A row of marks sits on the Label marks line, after
	/// the choices, and draws what the published style and tints draw.
	void test_the_mark_style_preview_follows_the_setting()
	{
		qt::workbench_window window;
		auto                 records = std::vector<application::review_behaviour_settings>{ };
		window.set_behaviour_dispatch([&records](const application::review_behaviour_settings& aSettings) { records.push_back(aSettings); });
		auto* const page    = window.settings()->page_widget(qt::settings_page::reviews_and_queue);
		auto* const letters = window.findChild<QToolButton*>(QStringLiteral("feedbackMarksLetters"));
		auto* const preview = window.feedback_marks_preview();
		expect(preview != nullptr && letters != nullptr && page->isAncestorOf(preview)
				   && preview->objectName() == QStringLiteral("feedbackMarksPreview"),
			   "Reviews & queue shows the marks preview");
		window.settings()->resize(1'100, 760);
		window.settings()->present(qt::settings_page::reviews_and_queue);
		settle();
		const auto lettersBox = QRect{ letters->mapTo(page, QPoint{ 0, 0 }), letters->size() };
		const auto previewBox = QRect{ preview->mapTo(page, QPoint{ 0, 0 }), preview->size() };
		expect(previewBox.left() > lettersBox.right() && previewBox.left() - lettersBox.right() < 40
				   && std::abs(previewBox.center().y() - lettersBox.center().y()) <= 2,
			   "the preview sits on the Label marks line, just after Letters");
		expect(preview->theme().Style == qt::feedback_mark_style::icons
				   && preview->accessibleName() == QStringLiteral("Label marks preview, icons")
				   && preview->accessibleDescription().contains(QStringLiteral("issue (blocking)"))
				   && preview->accessibleDescription().contains(QStringLiteral("nitpick (non-blocking)"))
				   && preview->accessibleDescription().contains(QStringLiteral("suggestion (if-minor)")),
			   "it starts in icons and names what it shows, each decoration included");
		// The owner, mark-preview-v002: "There should be individual tooltips
		// instead of one giant tooltip."
		expect(preview->toolTip().isEmpty(), "the preview has no one tooltip for all its marks");
		const auto entries = qt::feedback_mark_preview::entries();
		for (std::size_t index = 0U; index < entries.size(); ++index) {
			const auto centre = preview->entry_rect(index).center().toPoint();
			QHelpEvent help{ QEvent::ToolTip, centre, preview->mapToGlobal(centre) };
			QApplication::sendEvent(preview, &help);
			const auto expected = qt::feedback_mark_preview::entry_tooltip(entries[index]);
			expect(QToolTip::isVisible() && QToolTip::text() == expected && !expected.contains(QStringLiteral(", ")),
				   "each mark has its own tooltip: its name and description, nothing about the others");
			expect(expected.startsWith(qt::feedback_label_text(entries[index].Label)), "the tooltip starts with the mark's label");
			QToolTip::hideText();
		}
		const auto between = QPoint{ static_cast<int>(preview->entry_rect(0U).right()) + 3, preview->height() / 2 };
		expect(!preview->entry_at(QPointF{ between }).has_value() && preview->entry_at(preview->entry_rect(2U).center()) == std::optional<std::size_t>{ 2U },
			   "the gap between marks belongs to no mark, so it shows no tooltip");
		const auto paint = [preview] {
			auto image = QImage{ preview->size(), QImage::Format_ARGB32 };
			image.fill(Qt::transparent);
			preview->render(&image);
			return image;
		};
		const auto icons      = paint();
		const auto issueIndex = std::distance(entries.begin(), std::ranges::find_if(entries, [](const auto& aEntry) { return aEntry.Emphasis == qt::feedback_mark_emphasis::blocking; }));
		const auto issueX     = static_cast<int>(preview->entry_rect(static_cast<std::size_t>(issueIndex)).left());
		const auto issueTint  = window.label_marks().Tints[static_cast<std::size_t>(core::feedback_label::issue)];
		const auto corner     = icons.pixelColor(issueX + 3, (preview->height() / 2) - 5);
		expect(std::abs(corner.red() - issueTint.red()) < 24 && std::abs(corner.green() - issueTint.green()) < 24 && std::abs(corner.blue() - issueTint.blue()) < 24,
			   "the blocking issue mark is filled with the issue tint");
		letters->click();
		auto projection      = application::workbench_projection{ };
		projection.Behaviour = records.back();
		window.publish(projection);
		settle();
		const auto lettered = paint();
		expect(preview->theme().Style == qt::feedback_mark_style::letters
				   && preview->accessibleName() == QStringLiteral("Label marks preview, letters") && lettered != icons,
			   "choosing Letters redraws the preview in letters once it is published");
		save_render(*page, "reviews-and-queue-letters-preview");
		window.set_label_tints(qt::feedback_label_tints_of(qt::builtin_appearance_palette(core::appearance_variant::dark)));
		expect(preview->theme().Tints[3] == QColor{ 0xFF, 0x7B, 0x72 } && preview->theme().Style == qt::feedback_mark_style::letters,
			   "a theme's tints reach the preview without changing its style");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_cards_mark_every_post_under_its_avatar();
	test_compact_entries_lead_with_the_root_mark();
	test_the_post_list_and_the_body_reader();
	test_the_mark_style_setting_reaches_every_surface();
	test_the_mark_style_preview_follows_the_setting();
	std::cout << "feedback-label-threads-tests:pass\n";
	return EXIT_SUCCESS;
}
