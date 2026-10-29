/// \file
/// \brief [[BPR-1583]]. One avatar and one colour per person: the same login is
/// drawn in the same fill on a Threads note, an Overview participant row, a
/// Home account chip and group header and the Settings account card, in all
/// four built-in themes, and every fill and its initials clear the contrast
/// minimums. Also renders Threads and Overview's participants in Light and Dark.

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/feedback_label_marks.hpp"
#include "bpr/qt/home_panel.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/identity_avatar.hpp"
#include "bpr/qt/identity_avatar_pictures.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/server_card.hpp"

#include "bpr/core/appearance_contrast.hpp"
#include "bpr/core/identity_avatar.hpp"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
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
				// [[BPR-1589]]. The roster is People's details.
				page.status_section_header(qt::overview_status_section::people)->setChecked(true);
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
	/// \brief [[BPR-1807]]. A decoded picture as the host publishes it: premultiplied
	/// ARGB32 pixels, from a file when `aFile` names one, otherwise one colour.
	[[nodiscard]] auto decoded_picture(const QColor& aColour, const QString& aFile = { }, const int aSide = 96) -> std::shared_ptr<const application::markdown_decoded_image>
	{
		auto source = aFile.isEmpty() ? QImage{ } : QImage{ aFile };
		if (source.isNull()) {
			source = QImage{ aSide, aSide, QImage::Format_ARGB32_Premultiplied };
			source.fill(aColour);
		}
		source          = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
		auto decoded    = std::make_shared<application::markdown_decoded_image>();
		decoded->Width  = static_cast<std::uint32_t>(source.width());
		decoded->Height = static_cast<std::uint32_t>(source.height());
		auto frame      = application::markdown_image_frame{ };
		for (auto y = 0; y < source.height(); ++y) {
			const auto* const line = source.constScanLine(y);
			frame.Pixels.insert(frame.Pixels.end(), line, line + (static_cast<std::ptrdiff_t>(source.width()) * 4));
		}
		decoded->Frames.push_back(std::move(frame));
		return decoded;
	}

	/// \brief A window that serves pictures, recording what its faces ask for.
	struct picture_window
	{
		QWidget                                   Window;
		qt::identity_avatar_pictures*             Pictures = nullptr;
		std::vector<core::identity_avatar_intent> Asked;

		picture_window()
			: Pictures{ new qt::identity_avatar_pictures{ &Window } }
		{
			Pictures->set_dispatch([this](std::vector<core::identity_avatar_intent> aIntents) {
				Asked.insert(Asked.end(), aIntents.begin(), aIntents.end());
			});
		}

		[[nodiscard]] auto asked_for(const std::string& aLogin) const -> std::size_t
		{
			return static_cast<std::size_t>(std::ranges::count(Asked, aLogin, &core::identity_avatar_intent::Login));
		}

		/// The host's answer for every face asked so far.
		void answer(const std::string& aLogin, const application::identity_avatar_projection& aAnswer)
		{
			for (const auto& intent : Asked) {
				if (intent.Login == aLogin) {
					auto reply   = aAnswer;
					reply.Login  = intent.Login;
					reply.Pixels = core::identity_avatar_picture_pixels(intent.DevicePixels);
					Pictures->apply(reply);
				}
			}
		}
	};

	/// \brief One face painted by the shared painter, with a ring, on a widget.
	class face_probe final : public QWidget
	{
	public:
		face_probe(QWidget* aParent, QString aIdentity, const bool aGenerated)
			: QWidget{ aParent }
			, Identity{ std::move(aIdentity) }
			, Generated{ aGenerated }
		{
			setFixedSize(52, 52);
		}

		static constexpr auto box = QRectF{ 6.0, 6.0, 40.0, 40.0 };

	protected:
		void paintEvent(QPaintEvent* aEvent) override
		{
			static_cast<void>(aEvent);
			auto painter = QPainter{ this };
			painter.fillRect(rect(), palette().color(QPalette::Window));
			qt::paint_identity_avatar(painter, box, { .Identity = Identity, .Ring = QColor{ 0x10, 0x80, 0x10 }, .Generated = Generated }, qt::identity_avatar_grounds_of(palette()), font());
		}

	private:
		QString Identity;
		bool    Generated = false;
	};

	/// \brief A point in a face's disc above its initials.
	[[nodiscard]] auto above_initials() -> QPointF
	{
		return face_probe::box.center() + QPointF{ 0.0, -13.0 };
	}

	/// \brief [[BPR-1807]]. A face shows its person's picture once the host has
	/// published it, the initials until then and whenever it cannot be had; the
	/// ring stays; Generated never asks.
	void test_a_picture_replaces_the_initials()
	{
		const auto     palette = palette_of(core::appearance_mode::light);
		const auto     grounds = qt::identity_avatar_grounds_of(palette);
		const auto     tone    = qt::identity_avatar_colours_for(QStringLiteral("tbutler"), grounds);
		const auto     picture = QColor{ 0xD0, 0x30, 0xA0 };
		picture_window host;
		host.Window.setPalette(palette);
		host.Window.resize(140, 60);
		auto* const face  = new face_probe{ &host.Window, QStringLiteral("tbutler"), false };
		auto* const fixed = new face_probe{ &host.Window, QStringLiteral("rcaloca"), true };
		fixed->move(60, 0);
		host.Window.show();
		settle();
		expect(host.asked_for("tbutler") == 1U && host.asked_for("rcaloca") == 0U, "a person's face asks once; a Generated face never asks");
		const auto ratio = face->devicePixelRatioF();
		expect(host.Asked.front().DevicePixels == static_cast<std::uint32_t>(std::ceil(40.0 * ratio)), "a face asks for the device pixels it covers");
		expect(colour_at(image_of(*face), above_initials(), ratio) == tone.Fill, "the initials are drawn while it loads");
		face->update();
		settle();
		expect(host.asked_for("tbutler") == 1U, "a face still waiting does not ask again");
		host.answer("tbutler", { .State = core::markdown_image_state::loaded, .Image = decoded_picture(picture) });
		settle();
		const auto shown = image_of(*face);
		expect(colour_at(shown, face_probe::box.center(), ratio) == picture, "the picture replaces the initials once it arrives");
		expect(colour_at(shown, QPointF{ face_probe::box.center().x(), face_probe::box.top() - 2.0 }, ratio) != palette.color(QPalette::Window), "the ring is drawn around a picture too");
		expect(colour_at(shown, QPointF{ face_probe::box.left() + 1.0, face_probe::box.top() + 1.0 }, ratio) == palette.color(QPalette::Window), "the picture is masked to the disc");
		host.Pictures->set_style(core::identity_avatar_style::generated);
		settle();
		expect(colour_at(image_of(*face), above_initials(), ratio) == tone.Fill, "Coloured initials draws the initials again");
		host.Pictures->set_style(core::identity_avatar_style::github);
		settle();
		expect(colour_at(image_of(*face), face_probe::box.center(), ratio) == picture && host.asked_for("tbutler") == 1U, "back on GitHub pictures the held picture is drawn without asking");
		host.Pictures->reset();
		settle();
		expect(host.asked_for("tbutler") == 2U, "a reset forgets the pictures and the face asks again");
		host.answer("tbutler", { .State = core::markdown_image_state::failed });
		face->update();
		settle();
		expect(colour_at(image_of(*face), above_initials(), ratio) == tone.Fill && host.asked_for("tbutler") == 2U,
			   "a picture that cannot be had keeps the initials and is not asked again");
		// Without pictures in the window the painter draws initials and asks nothing.
		QWidget alone;
		alone.setPalette(palette);
		face_probe lone{ &alone, QStringLiteral("tbutler"), false };
		alone.show();
		settle();
		expect(colour_at(image_of(lone), above_initials(), lone.devicePixelRatioF()) == tone.Fill, "a window without pictures keeps the initials");
	}

	/// \brief [[BPR-1807]]. Threads and Overview pick pictures up through the
	/// shared painter; Home's group header and the Settings card do not ask.
	void test_every_person_surface_asks()
	{
		const auto palette = palette_of(core::appearance_mode::light);
		{
			picture_window host;
			host.Window.setPalette(palette);
			host.Window.resize(380, 440);
			auto* const view = new qt::conversation_cards_view{ &host.Window };
			view->resize(360, 420);
			host.Window.show();
			view->adopt(forest());
			settle();
			for (const auto* const login : { "tbutler", "jiayin", "rcaloca" }) {
				expect(host.asked_for(login) >= 1U, std::string{ "Threads asks for " } + login);
			}
			const auto picture = QColor{ 0x20, 0x90, 0xC0 };
			host.answer("tbutler", { .State = core::markdown_image_state::loaded, .Image = decoded_picture(picture) });
			settle();
			auto image = QImage{ view->viewport()->size() * view->devicePixelRatio(), QImage::Format_RGB32 };
			image.setDevicePixelRatio(view->devicePixelRatio());
			view->viewport()->render(&image);
			const auto note = view->note_rect("thread-1");
			expect(colour_at(image, QRectF{ note.left() + 12.0, note.top() + 7.0, 18.0, 18.0 }.center(), view->devicePixelRatio()) == picture,
				   "a Threads note draws its author's picture");
		}
		{
			picture_window host;
			host.Window.setPalette(palette);
			host.Window.resize(1000, 640);
			auto* const page = new qt::pull_request_overview_page{ &host.Window };
			page->resize(1000, 640);
			host.Window.show();
			page->present(overview());
			page->status_section_header(qt::overview_status_section::people)->setChecked(true);
			settle();
			expect(host.asked_for("tbutler") >= 1U && host.asked_for("jiayin") >= 1U, "Overview's people ask for their pictures");
			expect(std::ranges::none_of(host.Asked, [](const auto& aIntent) { return aIntent.Login.find('/') != std::string::npos; }), "a team never asks");
		}
		{
			picture_window host;
			host.Window.setPalette(palette);
			auto* const panel = new qt::home_panel{ std::nullopt, qt::home_panel_style::panel, &host.Window };
			panel->set_title(QStringLiteral("tbutler"));
			panel->set_avatar(QStringLiteral("tbutler"), true);
			panel->resize(360, 120);
			auto* const card = new qt::server_card{ application::github_profile_option{ .Profile = core::host_profile_id{ 1U }, .Label = "GitHub", .Hostname = "github.com" }, qt::server_card_gestures{ }, &host.Window };
			card->apply({ .State = qt::server_card_state::connected, .Current = true, .Session = application::github_session_projection{ .Profile = core::host_profile_id{ 1U }, .Login = "tbutler" } });
			card->move(0, 130);
			card->resize(480, 240);
			host.Window.resize(500, 400);
			host.Window.show();
			settle();
			expect(host.Asked.empty(), "a Home header and a server's account keep the initials of their own host");
		}
	}

	/// \brief Saves Threads and Overview with pictures, in Light and Dark and in
	/// both styles, when `BPR_AVATAR_PICTURE_RENDER_DIR` names a folder. Pictures
	/// come from `<login>.png` in `BPR_AVATAR_PICTURE_DIR` when it has one.
	void save_picture_renders()
	{
		const auto folder = qEnvironmentVariable("BPR_AVATAR_PICTURE_RENDER_DIR");
		if (folder.isEmpty()) {
			return;
		}
		const auto pictures = qEnvironmentVariable("BPR_AVATAR_PICTURE_DIR");
		const auto file_for = [&pictures](const std::string& aLogin) {
			const auto path = QDir{ pictures }.filePath(QString::fromStdString(aLogin) + QStringLiteral(".png"));
			return !pictures.isEmpty() && QFileInfo::exists(path) ? path : QString{ };
		};
		const auto fallback = std::array{ QColor{ 0x6A, 0x8C, 0xAF }, QColor{ 0xC0, 0x7A, 0x52 }, QColor{ 0x5F, 0x9E, 0x6E }, QColor{ 0x9A, 0x6F, 0xB0 } };
		for (const auto& [mode, name] : { std::pair{ core::appearance_mode::light, "light" }, std::pair{ core::appearance_mode::dark, "dark" } }) {
			for (const auto style : { core::identity_avatar_style::github, core::identity_avatar_style::generated }) {
				const auto styleName = QString::fromLatin1(style == core::identity_avatar_style::github ? "github" : "generated");
				const auto palette   = palette_of(mode);
				const auto answer    = [&](picture_window& aHost) {
					auto index = std::size_t{ 0U };
					for (const auto* const login : { "tbutler", "jiayin", "rcaloca", "mike", "reviewer-four" }) {
						aHost.answer(login, { .State = core::markdown_image_state::loaded, .Image = decoded_picture(fallback.at(index++ % fallback.size()), file_for(login)) });
					}
					settle();
				};
				{
					picture_window host;
					host.Pictures->set_style(style);
					host.Window.setPalette(palette);
					host.Window.resize(360, 420);
					auto* const view = new qt::conversation_cards_view{ &host.Window };
					view->set_label_marks(qt::feedback_mark_theme{ .Tints = qt::feedback_label_tints_of(qt::builtin_appearance_palette(mode == core::appearance_mode::light ? core::appearance_variant::light : core::appearance_variant::dark)) });
					view->set_viewer_login(QStringLiteral("mike"));
					view->resize(360, 420);
					host.Window.show();
					view->adopt(forest());
					settle();
					answer(host);
					static_cast<void>(view->grab().save(QStringLiteral("%1/threads-%2-%3.png").arg(folder, styleName, QString::fromLatin1(name))));
				}
				{
					picture_window host;
					host.Pictures->set_style(style);
					host.Window.setPalette(palette);
					host.Window.resize(1000, 640);
					auto* const page = new qt::pull_request_overview_page{ &host.Window };
					page->resize(1000, 640);
					host.Window.show();
					page->present(overview());
					page->status_section_header(qt::overview_status_section::people)->setChecked(true);
					settle();
					answer(host);
					auto* const side = page->findChild<QWidget*>(QStringLiteral("overviewSideColumn"));
					auto*       shot = side != nullptr ? side : static_cast<QWidget*>(page);
					static_cast<void>(shot->grab().save(QStringLiteral("%1/overview-%2-%3.png").arg(folder, styleName, QString::fromLatin1(name))));
				}
			}
		}
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_one_initials_rule();
	test_one_person_is_one_colour_everywhere();
	test_a_picture_replaces_the_initials();
	test_every_person_surface_asks();
	save_renders();
	save_picture_renders();
	std::cout << "identity-avatar-tests:pass\n";
	return 0;
}
