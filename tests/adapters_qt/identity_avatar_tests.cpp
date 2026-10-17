/// \file
/// \brief [[BPR-1583]]. One avatar and one colour per person: the same login is
/// drawn in the same fill on a Threads note, an Overview participant row, a
/// Home account chip and group header and the Settings account card, in all
/// four built-in themes, and every fill and its initials clear the contrast
/// minimums. Also renders Threads and Overview's participants in Light and Dark.

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/feedback_label_marks.hpp"
#include "bpr/qt/home_panel.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/identity_avatar.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/server_card.hpp"

#include "bpr/core/appearance_contrast.hpp"
#include "bpr/core/identity_avatar.hpp"

#include <QApplication>
#include <QImage>
#include <QPainter>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
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

	[[nodiscard]] auto palette_of(const core::appearance_mode aMode) -> QPalette
	{
		auto profile = qt::appearance_profile{ };
		profile.select_mode(aMode);
		return profile.derive(QApplication::palette());
	}

	[[nodiscard]] auto forest() -> std::vector<conversation>
	{
		const auto post = [](std::string aParent, std::string aId, std::string aAuthor, std::string aBody, const std::uint32_t aLine) {
			return conversation{ .Id = std::move(aId), .ParentId = std::move(aParent), .Kind = application::github_conversation_kind::review_comment, .AuthorLogin = std::move(aAuthor), .Body = std::move(aBody), .Path = "src/light.cpp", .Line = aLine };
		};
		const auto thread = [](std::string aId, const std::uint32_t aLine) {
			return conversation{ .Id = std::move(aId), .Kind = application::github_conversation_kind::review_thread, .Path = "src/light.cpp", .Line = aLine, .DiffSide = application::github_conversation_diff_side::right, .AnchorAvailability = application::github_conversation_anchor_availability::current };
		};
		return {
			thread("thread-1", 12U),
			post("thread-1", "c1", "tbutler", "issue (blocking): LightScale ignores the view's post-process settings.", 12U),
			post("thread-1", "c2", "jiayin", "question: Can the view settings be null on secondary views?", 12U),
			post("thread-1", "c3", "rcaloca", "Good catch.", 12U),
			thread("thread-2", 40U),
			post("thread-2", "d1", "jiayin", "suggestion: Clamp the exposure before the bloom pass.", 40U),
			post("thread-2", "d2", "mike", "Done in the next upload.", 40U),
			thread("thread-3", 71U),
			post("thread-3", "e1", "rcaloca", "nitpick: This comment still names the old buffer.", 71U),
		};
	}

	[[nodiscard]] auto overview() -> application::pull_request_overview_projection
	{
		auto projection           = application::pull_request_overview_projection{ };
		projection.Owner          = "EpicGames";
		projection.RepositoryName = "UnrealEngine";
		projection.Number         = core::pull_request_number{ 14366U };
		projection.Title          = "Hair strand lighting fixes";
		projection.AuthorLogin    = "jiayin";
		projection.BaseRef        = "release";
		projection.HeadRef        = "hair-strand";
		projection.Participants   = {
			{ .Login = "jiayin", .Role = application::overview_participant_role::author, .Signoff = application::overview_signoff::uploaded },
			{ .Login = "mike", .Role = application::overview_participant_role::reviewer, .Signoff = application::overview_signoff::changes_requested, .Viewer = true },
			{ .Login = "rcaloca", .Role = application::overview_participant_role::reviewer, .Signoff = application::overview_signoff::approved },
			{ .Login = "tbutler", .Role = application::overview_participant_role::reviewer, .Signoff = application::overview_signoff::commented },
		};
		projection.ReviewRequests = {
			{ .Name = "reviewer-four" },
			{ .Name = "EpicGames/rendering", .Team = true, .CodeOwner = true },
		};
		return projection;
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

	[[nodiscard]] auto image_of(QWidget& aWidget) -> QImage
	{
		return aWidget.grab().toImage().convertToFormat(QImage::Format_RGB32);
	}

	[[nodiscard]] auto colour_at(const QImage& aImage, const QPointF aPoint, const qreal aRatio) -> QColor
	{
		return QColor{ aImage.pixel(static_cast<int>(aPoint.x() * aRatio), static_cast<int>(aPoint.y() * aRatio)) };
	}

	/// \brief How many pixels of an image are exactly this colour.
	[[nodiscard]] auto pixels_of(const QImage& aImage, const QColor& aColour) -> int
	{
		auto count = 0;
		for (auto y = 0; y < aImage.height(); ++y) {
			for (auto x = 0; x < aImage.width(); ++x) {
				count += QColor{ aImage.pixel(x, y) }.rgb() == aColour.rgb() ? 1 : 0;
			}
		}
		return count;
	}

	/// \brief A point inside an avatar's disc above its initials: an eighth of
	/// the way down, on the vertical centre line.
	[[nodiscard]] auto disc_point(const QRectF& aBox) -> QPointF
	{
		return { aBox.center().x(), aBox.top() + (aBox.height() / 6.0) };
	}

	/// \brief The fill drawn for `aLogin` on a Threads note, read from screen.
	[[nodiscard]] auto threads_fill(const QPalette& aPalette, const std::string& aThread) -> QColor
	{
		qt::conversation_cards_view view;
		view.setPalette(aPalette);
		view.resize(360, 420);
		view.show();
		view.adopt(forest());
		settle();
		auto image = QImage{ view.viewport()->size() * view.devicePixelRatio(), QImage::Format_RGB32 };
		image.setDevicePixelRatio(view.devicePixelRatio());
		view.viewport()->render(&image);
		const auto note = view.note_rect(aThread);
		return colour_at(image, disc_point(QRectF{ note.left() + 12.0, note.top() + 7.0, 18.0, 18.0 }), view.devicePixelRatio());
	}

	/// \brief The fill drawn for `aLogin` on its Overview participant row.
	[[nodiscard]] auto overview_fill(qt::pull_request_overview_page& aPage, const QString& aLogin) -> QColor
	{
		for (auto* const row : aPage.participant_rows()) {
			if (row->login() == aLogin) {
				const auto image = image_of(*row);
				const auto box   = QRectF{ 2.0, (row->height() / 2.0) - 9.0, 18.0, 18.0 };
				return colour_at(image, disc_point(box), image.width() / static_cast<qreal>(row->width()));
			}
		}
		return { };
	}

	/// \brief AC4: the same login is the same fill on every surface, in every
	/// theme, and that fill is the rule's; AC3: it reads.
	void test_one_person_is_one_colour_everywhere()
	{
		// Two logins the rule puts in different slots, for the negative control.
		const auto tbutler = core::identity_tone_slot("tbutler");
		const auto other   = std::string{ core::identity_tone_slot("rcaloca") != tbutler ? "rcaloca" : "jiayin" };
		expect(core::identity_tone_slot(other) != tbutler, "the negative control has a login in another slot");
		const auto otherThread = other == "rcaloca" ? std::string{ "thread-3" } : std::string{ "thread-2" };
		for (const auto& theme : themes) {
			const auto palette = palette_of(theme.Mode);
			const auto grounds = qt::identity_avatar_grounds_of(palette);
			const auto mine    = qt::identity_avatar_colours_for(QStringLiteral("tbutler"), grounds);
			const auto theirs  = qt::identity_avatar_colours_for(QString::fromStdString(other), grounds);
			const auto label   = std::string{ theme.Name } + ": ";
			// AC3.
			for (const auto& ground : { grounds.Window, grounds.Base }) {
				expect(core::contrast_ratio_hundredths(qt::to_appearance_color(mine.Fill), qt::to_appearance_color(ground)) >= core::minimum_contrast_hundredths(core::contrast_kind::indicator, theme.Variant),
					   label + "the disc stands out from the window and the base at 3:1");
			}
			expect(core::contrast_ratio_hundredths(qt::to_appearance_color(mine.Ink), qt::to_appearance_color(mine.Fill)) >= core::minimum_contrast_hundredths(core::contrast_kind::text, theme.Variant),
				   label + "the initials read at the theme's text minimum");
			expect(mine.Fill != theirs.Fill, label + "negative control: another slot is another colour");
			expect(mine.Fill != palette.color(QPalette::Active, QPalette::Mid), label + "negative control: the fill is not the neutral tile Overview used to draw");
			// Threads.
			expect(threads_fill(palette, "thread-1") == mine.Fill, label + "Threads draws tbutler in the rule's fill");
			expect(threads_fill(palette, otherThread) == theirs.Fill, label + "and the other person in theirs");
			// Overview.
			{
				qt::pull_request_overview_page page;
				page.setPalette(palette);
				page.resize(1000, 640);
				page.show();
				page.present(overview());
				settle();
				expect(overview_fill(page, QStringLiteral("tbutler")) == mine.Fill, label + "Overview draws tbutler in the same fill as Threads");
				expect(overview_fill(page, QString::fromStdString(other)) == theirs.Fill, label + "and the other person in the same fill as Threads");
				expect(overview_fill(page, QStringLiteral("mike")) == qt::identity_avatar_colours_for(QStringLiteral("mike"), grounds).Fill,
					   label + "mine is my colour, ringed rather than filled with the accent");
			}
			// Home: an account chip and an Account group's header.
			{
				const auto font  = QApplication::font();
				const auto size  = qt::home_chip_size(font, QStringLiteral("tbutler"));
				auto       image = QImage{ size, QImage::Format_RGB32 };
				image.fill(grounds.Base);
				{
					auto painter = QPainter{ &image };
					qt::draw_home_chip(painter, QRect{ QPoint{ 0, 0 }, size }, QStringLiteral("tbutler on github.example.com"), font, qt::home_chip_ink_for(palette, QPalette::Active, false), grounds);
				}
				expect(pixels_of(image, mine.Fill) >= 20 && pixels_of(image, theirs.Fill) == 0, label + "a Home account chip draws tbutler in the same fill");
				qt::home_panel panel{ std::nullopt };
				panel.setPalette(palette);
				panel.set_title(QStringLiteral("tbutler"));
				panel.set_avatar(QStringLiteral("tbutler"), true);
				panel.resize(360, 120);
				panel.show();
				settle();
				const auto header = image_of(panel);
				expect(pixels_of(header, mine.Fill) >= 20 && pixels_of(header, theirs.Fill) == 0, label + "a Home group header draws its account in the same fill");
			}
			// Settings: the account card.
			{
				qt::server_card card{ application::github_profile_option{ .Profile = core::host_profile_id{ 1U }, .Label = "GitHub", .Hostname = "github.com" }, qt::server_card_gestures{ } };
				card.setPalette(palette);
				card.resize(480, 240);
				card.apply({ .State = qt::server_card_state::connected, .Current = true, .Session = application::github_session_projection{ .Profile = core::host_profile_id{ 1U }, .Login = "tbutler" } });
				card.show();
				settle();
				auto* const avatar = dynamic_cast<qt::identity_avatar_view*>(card.findChild<QWidget*>(QStringLiteral("serverCardAvatar")));
				expect(avatar != nullptr && avatar->identity() == QStringLiteral("tbutler"), label + "the account card's avatar is the shared one, for the signed-in login");
				const auto face = image_of(*avatar);
				expect(pixels_of(face, mine.Fill) >= 100, label + "the Settings account card draws tbutler in the same fill");
			}
		}
	}

	/// \brief The initials rule is the one rule on every surface.
	void test_one_initials_rule()
	{
		expect(qt::identity_avatar_initials(QStringLiteral("reviewer-four")) == QStringLiteral("RF") && qt::identity_avatar_initials(QStringLiteral("tbutler")) == QStringLiteral("T")
				   && qt::identity_avatar_initials(QString{ }) == QStringLiteral("?"),
			   "reviewer-four is RF and tbutler is T wherever they are drawn");
	}

	/// \brief Saves Threads' notes and Overview's side column in Light and Dark
	/// when `BPR_AVATAR_RENDER_DIR` names a folder, each file prefixed by
	/// `BPR_AVATAR_RENDER_TAG`: evidence for the owner's look, not a test.
	void save_renders()
	{
		const auto folder = qEnvironmentVariable("BPR_AVATAR_RENDER_DIR");
		if (folder.isEmpty()) {
			return;
		}
		const auto tag = qEnvironmentVariable("BPR_AVATAR_RENDER_TAG", QStringLiteral("after"));
		for (const auto& [mode, name] : { std::pair{ core::appearance_mode::light, "light" }, std::pair{ core::appearance_mode::dark, "dark" } }) {
			const auto palette = palette_of(mode);
			auto       marks   = qt::feedback_mark_theme{ .Tints = qt::feedback_label_tints_of(qt::builtin_appearance_palette(mode == core::appearance_mode::light ? core::appearance_variant::light : core::appearance_variant::dark)) };
			{
				qt::conversation_cards_view view;
				view.setPalette(palette);
				view.set_label_marks(marks);
				view.resize(360, 420);
				view.show();
				view.adopt(forest());
				settle();
				static_cast<void>(view.grab().save(QStringLiteral("%1/%2-threads-%3.png").arg(folder, tag, QString::fromLatin1(name))));
			}
			{
				qt::pull_request_overview_page page;
				page.setPalette(palette);
				page.resize(1000, 640);
				page.show();
				page.present(overview());
				settle();
				auto* const side = page.findChild<QWidget*>(QStringLiteral("overviewSideColumn"));
				auto*       shot = side != nullptr ? side : static_cast<QWidget*>(&page);
				static_cast<void>(shot->grab().save(QStringLiteral("%1/%2-overview-%3.png").arg(folder, tag, QString::fromLatin1(name))));
			}
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_one_initials_rule();
	test_one_person_is_one_colour_everywhere();
	save_renders();
	std::cout << "identity-avatar-tests:pass\n";
	return 0;
}
