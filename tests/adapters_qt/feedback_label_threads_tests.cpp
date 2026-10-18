/// \file
/// \brief [[BPR-1083]]. Proves the thread marks (owner, BPR-1079 v4; RFC-BPR-0050
/// R31, R32): every post's 16 px mark under its 18 px avatar, the rail in the
/// label's tint that adds no width, blocking's fill and wider rail, the reply's
/// own mark, every body read as written with its decorations (BPR-1083 v004),
/// the names "tbutler, issue (blocking)", the two mark styles, and tooltips
/// that sit right next to their marks.

#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/feedback_label_marks.hpp"
#include "bpr/qt/github_conversation_model.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include "bpr/core/review_tour_markdown.hpp"

#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHelpEvent>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QScrollBar>
#include <QSize>
#include <QTextEdit>
#include <QToolButton>
#include <QToolTip>
#include <QTreeView>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
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

	/// \brief [[BPR-1083]] v004. Sends a tooltip event at `aPoint` of `aOwner`
	/// and expects `aText` in an anchored tooltip right next to `aAnchor`:
	/// directly below it with their left edges aligned, or directly above it
	/// when below has no room (owner, preview-tooltips-v003: "The tooltip
	/// feels strangely far away.").
	void expect_tooltip_at(QWidget& aOwner, const QPoint aPoint, const QRect& aAnchor, const QString& aText, const char* const aWhat)
	{
		qt::hide_anchored_tooltip();
		QHelpEvent help{ QEvent::ToolTip, aPoint, aOwner.mapToGlobal(aPoint) };
		QApplication::sendEvent(&aOwner, &help);
		const auto* const tip = qt::anchored_tooltip();
		if (tip == nullptr) {
			std::cerr << aWhat << ": no anchored tooltip\n";
		}
		expect(tip != nullptr && !QToolTip::isVisible(), "a mark's tooltip is the anchored one, not Qt's at the pointer");
		const auto* const label = qobject_cast<const QLabel*>(tip);
		if (label == nullptr || label->text() != aText) {
			std::cerr << aWhat << ": tooltip text '" << (label != nullptr ? label->text().toStdString() : std::string{ }) << "'\n";
		}
		expect(label != nullptr && label->text() == aText, "the tooltip names what is under the pointer");
		const auto anchor = QRect{ aOwner.mapToGlobal(aAnchor.topLeft()), aAnchor.size() };
		const auto box    = tip->geometry();
		const auto below  = box.top() == anchor.bottom() + 1 + qt::anchored_tooltip_gap;
		const auto above  = box.bottom() + 1 + qt::anchored_tooltip_gap == anchor.top();
		// Left edges aligned unless the screen's edge moved it left; it still
		// spans the mark.
		const auto* const screen  = QGuiApplication::screenAt(anchor.center()) != nullptr ? QGuiApplication::screenAt(anchor.center()) : aOwner.screen();
		const auto        aligned = box.left() == anchor.left()
			|| (box.left() < anchor.left() && box.right() >= anchor.right() && box == qt::anchored_tooltip_geometry(anchor, box.size(), screen->availableGeometry()));
		if (!(below || above) || !aligned) {
			std::cerr << aWhat << ": anchor " << anchor.x() << ',' << anchor.y() << ' ' << anchor.width() << 'x' << anchor.height() << ", tooltip " << box.x() << ',' << box.y() << ' ' << box.width() << 'x' << box.height() << '\n';
		}
		expect((below || above) && aligned, "the tooltip sits right below or above its mark, left edges aligned");
		qt::hide_anchored_tooltip();
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
			post("c2", "MikeMarcin", "question (if-minor, ux): Can the view settings be null on secondary views?"),
			post("c3", "jiayin", "**praise:** Good catch."),
			post("c4", "tbutler", "Thanks, resolving after CI."),
		};
	}

	[[nodiscard]] auto theme() -> qt::feedback_mark_theme
	{
		return { .Tints = qt::feedback_label_tints_of(qt::builtin_appearance_palette(core::appearance_variant::light)) };
	}

	/// \brief Where one post of the open thread draws its 16 px mark, in the
	/// open card: centred under the 18 px avatar, 3 px below it.
	[[nodiscard]] auto post_mark(const QWidget& aCard, const QWidget& aPost) -> QRect
	{
		return QRect{ aPost.mapTo(&aCard, QPoint{ 12 + 1, 8 + 1 + 18 + 3 }), QSize{ 16, 16 } };
	}

	[[nodiscard]] auto posts_of(const QWidget& aCard) -> std::vector<QWidget*>
	{
		auto posts = std::vector<QWidget*>{ };
		for (auto* const post : aCard.findChildren<QWidget*>(QStringLiteral("threadPost"))) {
			posts.push_back(post);
		}
		std::ranges::sort(posts, { }, [](const QWidget* aPost) { return aPost->y(); });
		return posts;
	}

	/// \brief [[BPR-1083]], [[BPR-1290]]. The open thread draws every post's
	/// mark under its avatar and its rail in its own tint, its body as written.
	void test_the_open_thread_marks_every_post_under_its_avatar()
	{
		qt::conversation_cards_view view;
		view.set_label_marks(theme());
		view.resize(420, 900);
		view.show();
		view.adopt(forest());
		settle();
		expect(view.cards().size() == 1U && view.cards().front().Posts.size() == 4U, "one thread of four posts");
		const auto& posts = view.cards().front().Posts;
		expect(posts[0].Label == core::feedback_label::issue && posts[0].Emphasis == qt::feedback_mark_emphasis{ .Weight = qt::feedback_mark_weight::blocking } && posts[0].Name == QStringLiteral("tbutler, issue (blocking)"),
			   "the root is a blocking issue named with its author first");
		expect(posts[1].Label == core::feedback_label::question && posts[1].Emphasis == qt::feedback_mark_emphasis{ .IfMinor = true } && posts[2].Label == core::feedback_label::praise
				   && posts[1].Name == QStringLiteral("MikeMarcin, question (if-minor, ux)"),
			   "each reply shows its own mark, which may differ from its root's, with if-minor's corner and its custom decoration named");
		expect(!posts[3].Label.has_value() && posts[3].Name == QStringLiteral("tbutler"), "a plain post has no mark");
		view.open_thread("thread-1");
		settle();
		auto* const card  = view.open_card();
		const auto  shown = card != nullptr ? posts_of(*card) : std::vector<QWidget*>{ };
		expect(shown.size() == 4U, "the open thread shows its four posts");
		// [[BPR-1083]] v004 (owner, decorations-v003: "I think we always
		// display the decorators text out in the comments").
		const auto bodyOf = [](const QWidget* aPost) { return aPost->findChild<QTextEdit*>(QStringLiteral("threadPostBody"))->toPlainText(); };
		expect(bodyOf(shown[0]) == QStringLiteral("issue (blocking): LightScale ignores the view's post-process settings.")
				   && bodyOf(shown[1]) == QStringLiteral("question (if-minor, ux): Can the view settings be null on secondary views?")
				   && bodyOf(shown[2]) == QStringLiteral("praise: Good catch.") && bodyOf(shown[3]) == QStringLiteral("Thanks, resolving after CI."),
			   "every post reads with its label and decorations, its Markdown rendered (RFC-BPR-0036)");
		const auto readerX = [](const QWidget* aPost) { return aPost->findChild<QWidget*>(QStringLiteral("threadPostReader"))->x(); };
		const auto left    = readerX(shown.front());
		expect(std::ranges::all_of(shown, [left, &readerX](const QWidget* aPost) { return readerX(aPost) == left; }),
			   "every post's text keeps the same left edge: the rail adds no width");
		expect(std::ranges::all_of(std::span{ shown }.first(3U), [](const QWidget* aPost) { return aPost->height() >= 8 + 18 + 3 + 16; }),
			   "a marked post is tall enough for its mark");
		auto       image = card->grab().toImage();
		const auto ratio = image.devicePixelRatio();
		const auto pixel = [&image, ratio](const int aX, const int aY) {
			return QColor{ image.pixel(static_cast<int>(aX * ratio), static_cast<int>(aY * ratio)) };
		};
		const auto tints = theme().Tints;
		const auto issue = post_mark(*card, *shown[0]);
		expect(pixel(issue.left() + 2, issue.center().y()) == tints[3], "the blocking issue mark is filled with the issue tint on screen");
		const auto question = post_mark(*card, *shown[1]);
		expect(pixel(question.center().x(), question.top()) == tints[5], "the question mark's border is the question tint");
		const auto rail = shown[1]->mapTo(card, QPoint{ 7, shown[1]->height() / 2 });
		expect(pixel(rail.x(), rail.y()) == tints[5], "the reply's rail is in its own tint");
		const auto plain = post_mark(*card, *shown[3]);
		expect(pixel(plain.left() + 2, plain.center().y()) != tints[3], "negative control: a plain post draws no mark under its avatar");
		save_render(*card, "open-thread-icons");
		auto letters  = theme();
		letters.Style = qt::feedback_mark_style::letters;
		view.set_label_marks(letters);
		settle();
		auto* const lettered = view.open_card();
		const auto  after    = lettered->grab().toImage();
		const auto  box      = QRect{ static_cast<int>(question.x() * ratio), static_cast<int>(question.y() * ratio), static_cast<int>(16 * ratio), static_cast<int>(16 * ratio) };
		expect(after.copy(box) != image.copy(box) && QColor{ after.pixel(static_cast<int>(question.center().x() * ratio), static_cast<int>(question.top() * ratio)) } == tints[5],
			   "letters change the mark's middle and keep its tint and border");
		save_render(*lettered, "open-thread-letters");
	}

	/// \brief The owner, rejecting `thread-marks-v002`: "Marks don't show on
	/// inline comments." [[BPR-1290]]: every closed note carries its root's mark
	/// under its avatar and its root's rail, thread and draft alike.
	void test_closed_notes_carry_the_root_mark()
	{
		qt::conversation_cards_view view;
		view.set_label_marks(theme());
		view.resize(320, 600);
		view.show();
		view.adopt(forest());
		settle();
		save_render(view, "notes-closed");
		const auto& card = view.cards().front();
		const auto  rail = view.rail_rect(card);
		const auto  note = view.note_rect(card.Id);
		const auto  mark = view.mark_rect(card);
		expect(rail.width() == 4.0 && rail.left() >= note.left() && rail.right() <= note.left() + 8,
			   "a note has its root's blocking rail at its left edge");
		expect(mark.width() == 16 && mark.height() == 16 && mark.left() == note.left() + 13 && mark.top() == note.top() + 6 + 1 + 18 + 3,
			   "and its root's 16 px mark under the 18 px avatar, 3 px below it");
		auto image = QImage{ view.viewport()->size() * view.devicePixelRatio(), QImage::Format_ARGB32_Premultiplied };
		image.setDevicePixelRatio(view.devicePixelRatio());
		view.viewport()->render(&image);
		const auto ratio = view.devicePixelRatio();
		expect(QColor{ image.pixel(static_cast<int>((mark.left() + 2) * ratio), static_cast<int>(mark.center().y() * ratio)) } == theme().Tints[3],
			   "the blocking issue mark is drawn, filled with the issue tint");
		expect_tooltip_at(*view.viewport(), mark.center(), mark, QStringLiteral("tbutler, issue (blocking)"), "the note's mark");
		auto plainForest    = forest();
		plainForest[1].Body = "LightScale ignores the view's post-process settings.";
		qt::conversation_cards_view plain;
		plain.set_label_marks(theme());
		plain.resize(320, 600);
		plain.show();
		plain.adopt(plainForest);
		settle();
		expect(plain.mark_rect(plain.cards().front()).isEmpty() && plain.rail_rect(plain.cards().front()).width() == 2.0,
			   "a plain root's note has no mark and a 2 px grey rail");

		// My draft's note carries its own mark, in its own decoration.
		const auto comparison = core::file_comparison_digest{ };
		view.set_draft(core::review_comment_draft{
						   .Anchor = core::source_line_ref{ .Comparison = comparison, .Side = core::diff_side::right, .Line = 11U },
						   .Body   = "suggestion (non-blocking, if-minor): Clamp the width.",
					   },
					   comparison);
		settle();
		const auto draft = std::ranges::find_if(view.cards(), [](const qt::thread_card& aCard) { return aCard.Draft; });
		expect(draft != view.cards().end() && draft->Posts.front().Label == core::feedback_label::suggestion
				   && draft->Posts.front().Emphasis == qt::feedback_mark_emphasis{ .Weight = qt::feedback_mark_weight::non_blocking, .IfMinor = true } && !view.mark_rect(*draft).isEmpty()
				   && draft->Posts.front().Body == QStringLiteral("suggestion (non-blocking, if-minor): Clamp the width."),
			   "a draft note carries its non-blocking, if-minor suggestion mark, and its text says so");
		expect(view.rail_rect(*draft).width() == 2.0, "and keeps a 2 px rail");
		save_render(view, "notes-draft");
	}

	void test_the_model_and_the_overview_reader()
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
		expect(spoken.startsWith(QStringLiteral("Review comment by @tbutler, issue (blocking)\nissue (blocking): LightScale")), "the post's accessible name is its author and then its mark, and its text is as written");
		expect(first.data(qt::github_conversation_model::body_role).toString().startsWith(QStringLiteral("issue (blocking): ")), "the body role keeps the prefix");
		expect(!model.index(3, 0, root).data(qt::github_conversation_model::feedback_label_role).isValid(), "a plain post has no label");

		auto reader = qt::markdown_body_view{ };
		reader.set_body("issue (blocking): LightScale **ignores** the view.", "c1");
		expect(reader.rendered_view()->toPlainText().startsWith(QStringLiteral("issue (blocking):")), "without marks a reader shows the text as written");
		reader.set_feedback_marks(theme());
		reader.resize(420, 200);
		save_render(reader, "reader");
		expect(reader.rendered_view()->toPlainText().startsWith(QStringLiteral("issue (blocking): LightScale ignores")), "with marks the rendered body still reads as written, its decorations included");
		expect(reader.feedback_mark_label()->isVisibleTo(&reader) && reader.feedback_mark_label()->accessibleName() == QStringLiteral("issue (blocking)")
				   && reader.feedback_mark_label()->toolTip() == QStringLiteral("issue (blocking)"),
			   "and the mark leads it, named issue (blocking)");
		expect(reader.source_view()->toPlainText() == QStringLiteral("issue (blocking): LightScale **ignores** the view."), "View source keeps the prefix byte for byte");
		reader.copy_source();
		expect(QApplication::clipboard()->text() == QStringLiteral("issue (blocking): LightScale **ignores** the view."), "and so does Copy source");
		reader.set_body("Plain text.", "c4");
		expect(!reader.feedback_mark_label()->isVisibleTo(&reader) && reader.rendered_view()->toPlainText().startsWith(QStringLiteral("Plain text.")), "a plain body has no mark");
	}
	/// \brief [[BPR-1086]]. A published tour stop is plain unless its author
	/// labelled it (RFC-BPR-0048, "Tour compatibility"; R19). The mark comes
	/// from the narrative the tour owner maps; the generated heading, the
	/// pinned source and the navigation are never read as a label, nor set as
	/// a header; its closed note reads from the narrative; and every byte of
	/// the stop is kept as written.
	void test_a_tour_stop_is_plain_unless_its_author_labelled_it()
	{
		const std::array navigation{ core::tour_markdown_link{ "Tour overview", "https://github.com/o/r/pull/7#pullrequestreview-9" },
									 core::tour_markdown_link{ "Next", "https://github.com/o/r/pull/7#discussion_r12" } };
		const auto       labelled = core::render_tour_stop("Why the width is clamped", 2, "question (non-blocking): Should the clamp live in the caller?\n\nThe renderer trusts it today.",
														   navigation, "https://github.com/o/r/blob/1111111111111111111111111111111111111111/src/light.cpp?plain=1#L12");
		const auto       plain    = core::render_tour_stop("Where the frame starts", 1, "The renderer owns the frame from here.", navigation);
		expect(labelled.has_value() && plain.has_value(), "the tour owner renders both stops");
		// A generated heading whose envelope does not map: nothing in it is
		// the author's to classify, so no label is read past the heading.
		const auto broken = std::string{ "### Author's tour · 3 — Teardown\n\nissue: The navigation paragraph is missing." };
		auto       stops  = std::vector<conversation>{ };
		const auto add    = [&stops](const std::string& aThread, const std::string& aBody, const std::uint32_t aLine) {
			auto thread               = conversation{ };
			thread.Id                 = aThread;
			thread.Kind               = application::github_conversation_kind::review_thread;
			thread.Path               = "src/light.cpp";
			thread.Line               = aLine;
			thread.DiffSide           = application::github_conversation_diff_side::right;
			thread.AnchorAvailability = application::github_conversation_anchor_availability::current;
			stops.push_back(std::move(thread));
			auto root        = conversation{ };
			root.Id          = aThread + "-root";
			root.ParentId    = aThread;
			root.Kind        = application::github_conversation_kind::review_comment;
			root.AuthorLogin = "tbutler";
			root.Body        = aBody;
			root.Path        = "src/light.cpp";
			root.Line        = aLine;
			stops.push_back(std::move(root));
		};
		add("stop-1", *plain, 4U);
		add("stop-2", *labelled, 12U);
		add("stop-3", broken, 20U);

		qt::conversation_cards_view view;
		view.set_label_marks(theme());
		view.resize(420, 900);
		view.show();
		view.adopt(stops);
		settle();
		const auto card = [&view](const std::string_view aId) -> const qt::thread_card* {
			const auto found = std::ranges::find(view.cards(), std::string{ aId }, &qt::thread_card::Id);
			return found == view.cards().end() || found->Posts.empty() ? nullptr : &*found;
		};
		const auto* const first  = card("stop-1");
		const auto* const second = card("stop-2");
		const auto* const third  = card("stop-3");
		expect(first != nullptr && second != nullptr && third != nullptr, "three tour stops, three notes");
		const auto& asked = second->Posts.front();
		if (asked.Label != core::feedback_label::question) {
			std::cerr << "labelled stop: name '" << asked.Name.toStdString() << "'\n";
		}
		expect(asked.Label == core::feedback_label::question && asked.Emphasis == qt::feedback_mark_emphasis{ .Weight = qt::feedback_mark_weight::non_blocking }
				   && asked.Name == QStringLiteral("tbutler, question (non-blocking)") && !view.mark_rect(*second).isEmpty(),
			   "a stop its author labelled carries the label of its narrative");
		// The closed note reads from the narrative, so the label sits beside
		// its mark and the pinned source and heading are not its first words.
		const auto lead = asked.Body.mid(asked.Lead);
		expect(asked.Lead > 0 && lead.startsWith(QStringLiteral("question (non-blocking): Should the clamp"))
				   && asked.HeaderLength == QStringLiteral("question (non-blocking): ").size(),
			   "the labelled stop's note starts at its narrative, its label set as the header");
		expect(first->Posts.front().Body.mid(first->Posts.front().Lead).startsWith(QStringLiteral("The renderer owns the frame")),
			   "a plain stop's note also starts at its narrative");
		expect(third->Posts.front().Lead == 0, "a stop whose envelope does not map reads from its first byte");
		expect(asked.Body == QString::fromStdString(*labelled), "the labelled stop reads as written: pinned source, heading, narrative and navigation");
		expect(!first->Posts.front().Label.has_value() && view.mark_rect(*first).isEmpty() && first->Posts.front().HeaderLength == 0
				   && first->Posts.front().Body == QString::fromStdString(*plain),
			   "an explanatory stop stays plain: no label is forced on it");
		expect(!third->Posts.front().Label.has_value() && view.mark_rect(*third).isEmpty(), "a stop whose envelope does not map is not classified");

		auto model = qt::github_conversation_model{ };
		model.apply(stops);
		const auto rootOf = [&model](const int aRow) { return model.index(0, 0, model.index(aRow, 0)); };
		expect(rootOf(1).data(qt::github_conversation_model::feedback_label_role).toInt() == static_cast<int>(core::feedback_label::question)
				   && !rootOf(0).data(qt::github_conversation_model::feedback_label_role).isValid()
				   && !rootOf(2).data(qt::github_conversation_model::feedback_label_role).isValid(),
			   "the model reads the same marks from the same accepted bodies");

		auto reader = qt::markdown_body_view{ };
		reader.set_feedback_marks(theme());
		reader.set_body(*labelled, "stop-2-root");
		expect(reader.feedback_mark_label()->isVisibleTo(&reader) && reader.feedback_mark_label()->accessibleName() == QStringLiteral("question (non-blocking)")
				   && reader.source_view()->toPlainText() == QString::fromStdString(*labelled),
			   "the reader marks the labelled stop and View source keeps every byte");
		reader.set_body(*plain, "stop-1-root");
		expect(!reader.feedback_mark_label()->isVisibleTo(&reader), "and shows no mark for a plain stop");
		save_render(view, "tour-stops");

		// [[BPR-1086]] owner look. Open, the stop's mark sits beside its
		// narrative's label line, below the pinned source and heading, and the
		// label reads as a header there, as an ordinary post's does.
		view.open_thread("stop-2");
		settle();
		auto* const opened = view.open_card();
		const auto  shown  = opened != nullptr ? posts_of(*opened) : std::vector<QWidget*>{ };
		expect(!shown.empty(), "the labelled stop opens in place");
		auto* const inPlace = dynamic_cast<qt::markdown_body_view*>(shown.front()->findChild<QWidget*>(QStringLiteral("threadPostReader")));
		auto* const body    = shown.front()->findChild<QTextEdit*>(QStringLiteral("threadPostBody"));
		auto        label   = body->document()->find(QStringLiteral("question (non-blocking):"));
		expect(!label.isNull() && inPlace != nullptr && inPlace->header_length() == QStringLiteral("question (non-blocking):").size(), "the narrative's label is set as the header in place");
		label.setPosition(label.selectionStart());
		const auto line   = body->cursorRect(label);
		const auto centre = body->viewport()->mapTo(opened, line.center()).y();
		const auto image  = opened->grab().toImage();
		const auto ratio  = image.devicePixelRatio();
		const auto pixel  = [&image, ratio](const int aX, const int aY) { return QColor{ image.pixel(static_cast<int>(aX * ratio), static_cast<int>(aY * ratio)) }; };
		const auto under  = post_mark(*opened, *shown.front());
		const auto beside = QRect{ under.x(), centre - 8 + 1, 16, 16 };
		expect(beside.top() > under.bottom() + 20, "the label line is well below the avatar: the pinned source and heading come first");
		// The non-blocking mark is dashed, so its tint is looked for anywhere
		// in its box.
		const auto tinted = [&pixel](const QRect& aBox) {
			for (auto y = aBox.top(); y <= aBox.bottom(); ++y) {
				for (auto x = aBox.left(); x <= aBox.right(); ++x) {
					if (pixel(x, y) == theme().Tints[5]) {
						return true;
					}
				}
			}
			return false;
		};
		expect(tinted(beside), "the question mark is drawn beside the narrative's label line");
		expect(!tinted(under), "negative control: nothing is drawn under the avatar beside the pinned source");
		save_render(*opened, "tour-stop-open");
	}

	/// \brief [[BPR-1083]] v004 (owner, preview-tooltips-v003: "The tooltip
	/// feels strangely far away."). Every mark's tooltip sits right next to it.
	void test_tooltips_sit_next_to_their_marks()
	{
		const auto screen = QRect{ 0, 0, 1000, 800 };
		expect(qt::anchored_tooltip_geometry(QRect{ 100, 100, 16, 16 }, QSize{ 200, 40 }, screen) == QRect{ 100, 116 + qt::anchored_tooltip_gap, 200, 40 },
			   "below the mark, left edges aligned, a few pixels apart");
		expect(qt::anchored_tooltip_geometry(QRect{ 100, 770, 16, 16 }, QSize{ 200, 40 }, screen) == QRect{ 100, 770 - qt::anchored_tooltip_gap - 40, 200, 40 },
			   "above the mark when below would leave the screen");
		expect(qt::anchored_tooltip_geometry(QRect{ 950, 100, 16, 16 }, QSize{ 200, 40 }, screen).left() == 800, "moved left only to stay on the screen");

		qt::conversation_cards_view view;
		view.set_label_marks(theme());
		view.resize(420, 900);
		view.show();
		view.adopt(forest());
		settle();
		const auto note = view.mark_rect(view.cards().front());
		expect_tooltip_at(*view.viewport(), note.center(), note, QStringLiteral("tbutler, issue (blocking)"), "a note's mark");
		const auto rail = view.rail_rect(view.cards().front()).toAlignedRect();
		expect_tooltip_at(*view.viewport(), rail.center(), note, QStringLiteral("tbutler, issue (blocking)"), "a note's rail, named at its mark");
		view.open_thread("thread-1");
		settle();
		auto* const reply     = posts_of(*view.open_card())[1];
		const auto  replyMark = QRect{ QPoint{ 13, 30 }, QSize{ 16, 16 } };
		expect_tooltip_at(*reply, replyMark.center(), replyMark, QStringLiteral("MikeMarcin, question (if-minor, ux)"), "an open thread's reply mark");

		auto reader = qt::markdown_body_view{ };
		reader.set_feedback_marks(theme());
		reader.set_body("question (if-minor, ux): Is it null?", "c2");
		reader.resize(420, 200);
		reader.show();
		settle();
		auto* const mark = reader.feedback_mark_label();
		expect_tooltip_at(*mark, mark->rect().center(), mark->rect(), QStringLiteral("question (if-minor, ux)"), "the reader's mark");
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
		expect(icons != nullptr && letters != nullptr && reset != nullptr && window.settings()->page_widget(qt::settings_page::comments)->isAncestorOf(letters),
			   "Comments offers Label marks, Icons or Letters, in a Labels group");
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
		auto* const page    = window.settings()->page_widget(qt::settings_page::comments);
		auto* const letters = window.findChild<QToolButton*>(QStringLiteral("feedbackMarksLetters"));
		auto* const preview = window.feedback_marks_preview();
		expect(preview != nullptr && letters != nullptr && page->isAncestorOf(preview)
				   && preview->objectName() == QStringLiteral("feedbackMarksPreview"),
			   "Comments shows the marks preview");
		window.settings()->resize(1'100, 760);
		window.settings()->present(qt::settings_page::comments);
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
				   && preview->accessibleDescription().contains(QStringLiteral("suggestion (non-blocking, if-minor)"))
				   && preview->accessibleDescription().contains(QStringLiteral("todo (if-minor)")),
			   "it starts in icons and names what it shows, each decoration included");
		// The owner, mark-preview-v002: "There should be individual tooltips
		// instead of one giant tooltip."
		expect(preview->toolTip().isEmpty(), "the preview has no one tooltip for all its marks");
		const auto entries = qt::feedback_mark_preview::entries();
		for (std::size_t index = 0U; index < entries.size(); ++index) {
			const auto mark     = preview->entry_rect(index).toAlignedRect();
			const auto expected = qt::feedback_mark_preview::entry_tooltip(entries[index]);
			// Right below its own mark (owner, preview-tooltips-v003: "The
			// tooltip feels strangely far away.").
			expect_tooltip_at(*preview, mark.center(), mark, expected, "a preview mark");
			for (std::size_t other = 0U; other < entries.size(); ++other) {
				expect(other == index || !expected.contains(qt::feedback_label_text(entries[other].Label)), "each mark has its own tooltip: its name and description, nothing about the others");
			}
			expect(expected.startsWith(qt::feedback_label_text(entries[index].Label)), "the tooltip starts with the mark's label");
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
		const auto issueIndex = std::distance(entries.begin(), std::ranges::find_if(entries, [](const auto& aEntry) { return aEntry.Emphasis.Weight == qt::feedback_mark_weight::blocking; }));
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
	test_the_open_thread_marks_every_post_under_its_avatar();
	test_closed_notes_carry_the_root_mark();
	test_the_model_and_the_overview_reader();
	test_a_tour_stop_is_plain_unless_its_author_labelled_it();
	test_tooltips_sit_next_to_their_marks();
	test_the_mark_style_setting_reaches_every_surface();
	test_the_mark_style_preview_follows_the_setting();
	std::cout << "feedback-label-threads-tests:pass\n";
	return EXIT_SUCCESS;
}
