#include "bpr/core/file_progress_mark.hpp"
#include "bpr/qt/draft_state_chip.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/identity_avatar.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "pull_request_overview_support.hpp"

#include <QAbstractButton>
#include <QBoxLayout>
#include <QBrush>
#include <QChar>
#include <QColor>
#include <QFlags>
#include <QFont>
#include <QFontMetrics>
#include <QHash>
#include <QLabel>
#include <QLatin1Char>
#include <QLatin1StringView>
#include <QLayout>
#include <QLayoutItem>
#include <QList>
#include <QListSpecialMethods>
#include <QObject>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QStringDecoder>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto plural(const std::size_t aCount, const QString& aOne, const QString& aMany) -> QString
		{
			return aCount == 1U ? aOne : aMany.arg(count_text(aCount));
		}

		/// \brief Names in order: one, two joined by "and", or the first two
		/// and how many more.
		[[nodiscard]] auto name_list(const std::vector<std::string>& aNames) -> QString
		{
			if (aNames.empty()) {
				return { };
			}
			if (aNames.size() == 1U) {
				return text(aNames.front());
			}
			if (aNames.size() == 2U) {
				return QStringLiteral("%1 and %2").arg(text(aNames[0]), text(aNames[1]));
			}
			return QStringLiteral("%1, %2 and %3 more").arg(text(aNames[0]), text(aNames[1]), count_text(aNames.size() - 2U));
		}

		[[nodiscard]] auto review_words(const core::my_review_state aState) -> QString
		{
			switch (aState) {
			case core::my_review_state::approved:
				return QStringLiteral("Reviewer · approved");
			case core::my_review_state::changes_requested:
				return QStringLiteral("Reviewer · changes requested");
			case core::my_review_state::dismissed:
				return QStringLiteral("Reviewer · dismissed");
			case core::my_review_state::commented:
				break;
			}
			return QStringLiteral("Reviewer · commented");
		}

		[[nodiscard]] auto team_names(const std::vector<std::string>& aTeams) -> QString
		{
			auto names = QStringList{ };
			for (const auto& team : aTeams) {
				names.push_back(QLatin1Char{ '@' } + text(team));
			}
			return names.join(QStringLiteral(", "));
		}

		/// \brief [[BPR-1589]] v002. One role as a short label. It never says
		/// my review is required beyond GitHub's code-owner marking
		/// (RFC-BPR-0069), and it names "your team" only when GitHub said so.
		[[nodiscard]] auto role_label(const core::review_role& aRole) -> QString
		{
			switch (aRole.Kind) {
			case core::review_role_kind::author:
				return QStringLiteral("Author");
			case core::review_role_kind::requested:
				return QStringLiteral("Review requested");
			case core::review_role_kind::team_requested:
				return aRole.Membership == core::team_membership::member ? QStringLiteral("Review requested") : QStringLiteral("Team review requested");
			case core::review_role_kind::reviewed:
				return aRole.Review ? review_words(*aRole.Review) : QStringLiteral("Reviewer");
			case core::review_role_kind::mentioned:
				break;
			}
			return QStringLiteral("Mentioned");
		}

		/// \brief [[BPR-1589]] v002. The team a team role names, as a chip that
		/// moves whole to the next line rather than breaking a team's name.
		[[nodiscard]] auto role_team_chip(const core::review_role& aRole) -> QString
		{
			if (aRole.Kind != core::review_role_kind::team_requested) {
				return { };
			}
			return aRole.Membership == core::team_membership::member ? QStringLiteral("Your team: %1").arg(team_names(aRole.Teams)) : team_names(aRole.Teams);
		}

		/// \brief The tooltip's line for one role: what the label means and,
		/// for a team GitHub would not say I am in, why.
		[[nodiscard]] auto role_explanation(const core::review_role& aRole) -> QString
		{
			switch (aRole.Kind) {
			case core::review_role_kind::author:
				return QStringLiteral("You opened this pull request.");
			case core::review_role_kind::requested:
				return aRole.CodeOwner ? QStringLiteral("GitHub requested your review as a code owner.") : QStringLiteral("GitHub requested your review.");
			case core::review_role_kind::team_requested: {
				if (aRole.Membership == core::team_membership::member) {
					return QStringLiteral("GitHub requested a review from %1, a team you are in.").arg(team_names(aRole.Teams));
				}
				auto why = QStringLiteral("GitHub did not answer whether you are in it.");
				if (aRole.Reason == core::team_membership_reason::needs_read_org) {
					why = QStringLiteral("Your GitHub sign-in lacks the read:org scope, so GitHub will not say whether you are in it.");
				} else if (aRole.Reason == core::team_membership_reason::not_visible) {
					why = QStringLiteral("GitHub does not show you this organization's teams.");
				}
				return QStringLiteral("GitHub requested a review from %1. %2").arg(team_names(aRole.Teams), why);
			}
			case core::review_role_kind::reviewed:
				return QStringLiteral("Your latest review: %1.").arg(aRole.Review ? review_words(*aRole.Review).section(QStringLiteral(" · "), 1) : QStringLiteral("submitted"));
			case core::review_role_kind::mentioned:
				break;
			}
			return aRole.Mention == core::mention_place::description ? QStringLiteral("Someone mentioned you in the description.") : QStringLiteral("Someone mentioned you in a comment.");
		}

		constexpr int role_face = 22;

		constexpr int role_indent = 34;

		constexpr int role_gap = 6;

		constexpr int role_pad = 7;

		[[nodiscard]] auto role_font(const QFont& aBase, const bool aPrimary) -> QFont
		{
			auto font = aBase;
			font.setWeight(aPrimary ? QFont::DemiBold : QFont::Normal);
			return font;
		}

		constexpr int face_side = 22;

		constexpr int face_step = 26;

		constexpr int most_faces = 8;
	}

	void overview_role_line::present(const QString& aViewer, const QString& aPrimary, const QStringList& aChips, const QString& aExplanation)
	{
		Viewer  = aViewer;
		Primary = aPrimary;
		Chips   = aChips;
		setToolTip(aExplanation);
		setAccessibleName(Chips.isEmpty() ? QStringLiteral("My role: %1").arg(Primary) : QStringLiteral("My role: %1; also %2").arg(Primary, Chips.join(QStringLiteral(", "))));
		setAccessibleDescription(aExplanation);
		updateGeometry();
		update();
	}

	auto overview_role_line::primary() const -> const QString&
	{
		return Primary;
	}

	auto overview_role_line::chips() const -> const QStringList&
	{
		return Chips;
	}

	auto overview_role_line::layout_boxes(const int aWidth) const -> std::vector<QRect>
	{
		auto       boxes  = std::vector<QRect>{ };
		const auto room   = std::max(role_face, aWidth - role_indent - 4);
		const auto strong = QFontMetrics{ role_font(font(), true) };
		const auto plain  = QFontMetrics{ role_font(font(), false) };
		const auto line   = std::max(role_face, plain.height() + 6);
		// The label wraps rather than hiding a team's name; chips follow it on
		// its line when there is room, or start the next.
		const auto label  = strong.boundingRect(QRect{ 0, 0, room, 1'000 }, Qt::TextWordWrap, Primary);
		const auto height = std::max(line, label.height() + 4);
		boxes.emplace_back(role_indent, 2, std::min(room, label.width() + 2), height);
		const auto wrapped = label.height() > strong.height() + 2;
		auto       x       = wrapped ? role_indent + room + 1 : role_indent + boxes.back().width() + role_gap + 2;
		auto       y       = 2 + height - line;
		for (const auto& chip : Chips) {
			const auto width = std::min(room, plain.horizontalAdvance(chip) + (2 * role_pad) + 2);
			if (x + width > role_indent + room) {
				x = role_indent;
				y += line + 2;
			}
			boxes.emplace_back(x, y + 2, width, line - 4);
			x += width + role_gap;
		}
		return boxes;
	}

	auto overview_role_line::hasHeightForWidth() const -> bool
	{
		return true;
	}

	auto overview_role_line::heightForWidth(const int aWidth) const -> int
	{
		const auto boxes  = layout_boxes(aWidth);
		auto       bottom = 0;
		for (const auto& box : boxes) {
			bottom = std::max(bottom, box.bottom() + 1);
		}
		return std::max(bottom, role_face + 2) + 6;
	}

	auto overview_role_line::sizeHint() const -> QSize
	{
		return { 300, heightForWidth(300) };
	}

	auto overview_role_line::minimumSizeHint() const -> QSize
	{
		return { role_indent + role_face, heightForWidth(std::max(width(), role_indent + role_face)) };
	}

	void overview_role_line::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto& colours = palette();
		const auto  boxes   = layout_boxes(width());
		const auto  middle  = std::min(boxes.front().center().y(), 2 + (role_face / 2) + 2);
		paint_identity_avatar(painter, QRectF{ 4.0, middle - (role_face / 2.0), static_cast<qreal>(role_face), static_cast<qreal>(role_face) },
							  { .Identity = Viewer, .Ring = colours.color(QPalette::Active, QPalette::PlaceholderText) }, identity_avatar_grounds_of(colours), font());
		painter.setFont(role_font(font(), true));
		painter.setPen(colours.color(QPalette::Active, QPalette::WindowText));
		painter.drawText(boxes.front(), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, Primary);
		painter.setFont(role_font(font(), false));
		// A chip is the ink at a low alpha, so it reads on Light and Dark alike.
		auto fill = colours.color(QPalette::Active, QPalette::WindowText);
		fill.setAlpha(26);
		for (std::size_t index = 0U; index < static_cast<std::size_t>(Chips.size()); ++index) {
			const auto& box = boxes[index + 1U];
			painter.setPen(Qt::NoPen);
			painter.setBrush(fill);
			painter.drawRoundedRect(QRectF{ box }, box.height() / 2.0, box.height() / 2.0);
			painter.setPen(colours.color(QPalette::Active, QPalette::WindowText));
			painter.drawText(box.adjusted(role_pad, 0, -role_pad, 0), Qt::AlignCenter, painter.fontMetrics().elidedText(Chips[static_cast<qsizetype>(index)], Qt::ElideRight, box.width() - (2 * role_pad)));
		}
	}

	overview_faces::overview_faces(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("overviewPeopleFaces"));
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	}

	void overview_faces::set_faces(std::vector<overview_face> aFaces)
	{
		if (Faces == aFaces) {
			return;
		}
		Faces = std::move(aFaces);
		updateGeometry();
		update();
	}

	auto overview_faces::faces() const -> const std::vector<overview_face>&
	{
		return Faces;
	}

	auto overview_faces::sizeHint() const -> QSize
	{
		const auto shown = static_cast<int>(std::min<std::size_t>(Faces.size(), most_faces));
		return QSize{ shown == 0 ? 0 : ((shown - 1) * face_step) + face_side + 2, face_side + 4 };
	}

	auto overview_faces::minimumSizeHint() const -> QSize
	{
		return sizeHint();
	}

	void overview_faces::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto grounds = identity_avatar_grounds_of(palette());
		auto       left    = 1.0;
		for (std::size_t index = 0U; index < Faces.size() && index < most_faces; ++index) {
			const auto& face = Faces[index];
			const auto  box  = QRectF{ left, 2.0, static_cast<qreal>(face_side), static_cast<qreal>(face_side) };
			paint_identity_avatar(painter, box, { .Identity = face.Identity, .InitialsFrom = face.InitialsFrom, .Shape = face.Team ? identity_avatar_shape::group : identity_avatar_shape::person }, grounds, font());
			if (face.Badge.isValid()) {
				// The standing's mark at the corner, cut from the drawer's ground.
				const auto mark = QRectF{ box.right() - 7.0, box.bottom() - 7.0, 9.0, 9.0 };
				painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::AlternateBase), 2.0 });
				painter.setBrush(face.Badge);
				painter.drawEllipse(mark);
			}
			left += face_step;
		}
	}

	auto pull_request_overview_page::status_section_header(const overview_status_section aSection) const noexcept -> drawer_section_header*
	{
		return Sections[static_cast<std::size_t>(aSection)].Header;
	}

	auto pull_request_overview_page::status_section(const overview_status_section aSection) const noexcept -> QWidget*
	{
		return Sections[static_cast<std::size_t>(aSection)].Frame;
	}

	auto pull_request_overview_page::role_line() const noexcept -> overview_role_line*
	{
		return RoleRow;
	}

	auto pull_request_overview_page::people_faces() const noexcept -> overview_faces*
	{
		return Faces;
	}

	void pull_request_overview_page::set_reviewer_requests_offered(const bool aOffered)
	{
		ReviewersOffered = aOffered;
		RequestReview->setVisible(aOffered);
		if (!aOffered && ReviewerPicker != nullptr && ReviewerPicker->isVisible()) {
			// Withdrawn while open: the picker closes and says so.
			ReviewerPicker->hide();
		}
	}

	auto pull_request_overview_page::reviewer_requests_offered() const noexcept -> bool
	{
		return ReviewersOffered;
	}

	void pull_request_overview_page::set_reviewer_picker_dispatch(std::function<void(const reviewer_picker_gesture&)> aDispatch)
	{
		ReviewerDispatch = std::move(aDispatch);
		if (ReviewerPicker != nullptr) {
			ReviewerPicker->set_dispatch(ReviewerDispatch);
		}
	}

	void pull_request_overview_page::present_reviewer_picker(const core::reviewer_picker_state& aState)
	{
		if (ReviewerPicker == nullptr) {
			if (!aState.Open) {
				return;
			}
			ReviewerPicker = new reviewer_picker{ this };
			ReviewerPicker->set_dispatch(ReviewerDispatch);
		}
		ReviewerPicker->present(aState, RequestReview);
	}

	auto pull_request_overview_page::request_review_button() const noexcept -> QPushButton*
	{
		return RequestReview;
	}

	auto pull_request_overview_page::reviewer_picker_popup() const noexcept -> reviewer_picker*
	{
		return ReviewerPicker;
	}

	auto pull_request_overview_page::people_summary() const noexcept -> QLabel*
	{
		return PeopleSummary;
	}

	auto pull_request_overview_page::new_threads_button() const noexcept -> QToolButton*
	{
		return NewThreadsGo;
	}

	auto pull_request_overview_page::new_threads() const noexcept -> std::size_t
	{
		return NewThreads;
	}

	void pull_request_overview_page::set_new_threads_dispatch(std::function<void()> aDispatch)
	{
		NewThreadsDispatch = std::move(aDispatch);
	}

	auto pull_request_overview_page::new_conversation_button() const noexcept -> QToolButton*
	{
		return NewConversationGo;
	}

	void pull_request_overview_page::set_new_conversation_dispatch(std::function<void()> aDispatch)
	{
		NewConversationDispatch = std::move(aDispatch);
	}

	void pull_request_overview_page::set_new_conversation_comments(const std::optional<std::size_t> aCount)
	{
		NewConversation = aCount;
		present_my_review();
	}

	void pull_request_overview_page::set_new_threads(const std::size_t aCount)
	{
		NewThreads = aCount;
		present_my_review();
	}

	auto pull_request_overview_page::attention_rows() const -> QStringList
	{
		auto rows = QStringList{ };
		for (int index = 0; index < AttentionList->count(); ++index) {
			if (auto* const row = AttentionList->itemAt(index)->widget(); row != nullptr && !row->isHidden()) {
				rows.push_back(row->property("statusText").toString());
			}
		}
		return rows;
	}

	void pull_request_overview_page::show_value(const overview_value aValue, const bool aShown)
	{
		if (auto* const label = Values[static_cast<std::size_t>(aValue)]; label != nullptr && label->parentWidget() != nullptr) {
			label->parentWidget()->setVisible(aShown);
		}
	}

	void pull_request_overview_page::hide_status()
	{
		StatusNote->hide();
		RoleRow->hide();
		for (const auto& section : Sections) {
			section.Frame->hide();
		}
		NewThreadsRow->hide();
		NewConversationRow->hide();
	}

	void pull_request_overview_page::remember_sections()
	{
		if (SectionKey.isEmpty()) {
			return;
		}
		auto open = std::uint8_t{ 0U };
		for (std::size_t index = 0U; index < overview_status_section_count; ++index) {
			if (Sections[index].Header->isChecked()) {
				open = static_cast<std::uint8_t>(open | (1U << index));
			}
		}
		SectionMemory.insert(SectionKey, open);
	}

	void pull_request_overview_page::restore_status_sections(const QString& aKey)
	{
		SectionKey       = aKey;
		const auto open  = SectionMemory.value(SectionKey, 0U);
		const auto saved = SectionKey;
		SectionKey.clear();
		for (std::size_t index = 0U; index < overview_status_section_count; ++index) {
			Sections[index].Header->setChecked((open & (1U << index)) != 0U);
			Sections[index].Details->setVisible(Sections[index].Header->isChecked());
		}
		SectionKey = saved;
	}

	void pull_request_overview_page::present_status(const bool aSamePullRequest)
	{
		const auto& shown  = *Projection;
		const auto& status = shown.Status;
		StatusNote->hide();
		if (!aSamePullRequest) {
			restore_status_sections(QStringLiteral("%1/%2#%3").arg(text(shown.Owner), text(shown.RepositoryName), QString::number(shown.Number.value())));
		}

		// My role: the strongest leads, the others follow faintly.
		RoleRow->setVisible(status.Roles.ViewerKnown);
		if (status.Roles.ViewerKnown) {
			const auto  viewer = std::ranges::find_if(shown.Participants, &application::overview_participant::Viewer);
			const auto& roles  = status.Roles.Roles;
			auto        chips  = QStringList{ };
			auto        why    = QStringList{ };
			for (std::size_t index = 0U; index < roles.size(); ++index) {
				if (index != 0U) {
					chips.push_back(role_label(roles[index]));
				}
				if (const auto team = role_team_chip(roles[index]); !team.isEmpty()) {
					chips.push_back(team);
				}
				if (roles[index].CodeOwner && roles[index].Kind != core::review_role_kind::author) {
					chips.push_back(QStringLiteral("Code owner"));
				}
				why.push_back(role_explanation(roles[index]));
			}
			chips.removeDuplicates();
			RoleRow->present(viewer != shown.Participants.end() ? text(viewer->Login) : text(status.Viewer), roles.empty() ? QStringLiteral("No role") : role_label(roles.front()), chips,
							 roles.empty() ? QStringLiteral("You are not the author, no review was requested from you or a team you are in, and no one mentioned you.") : why.join(QLatin1Char{ '\n' }));
		}
		for (auto section : { overview_status_section::attention, overview_status_section::my_review, overview_status_section::people }) {
			Sections[static_cast<std::size_t>(section)].Frame->show();
		}
		present_attention();
		present_my_review();
		present_people();
	}

	void pull_request_overview_page::present_attention()
	{
		if (!Projection) {
			return;
		}
		const auto& shown  = *Projection;
		auto* const header = Sections[static_cast<std::size_t>(overview_status_section::attention)].Header;
		while (AttentionList->count() != 0) {
			auto* const item = AttentionList->takeAt(0);
			delete item->widget();
			delete item;
		}
		const auto& items = shown.Status.Attention;
		if (items.empty()) {
			// Nothing needs me: the section is Merge, and says its state.
			header->setText(QStringLiteral("Merge"));
			header->set_count(text(shown.MergeState));
			header->setAccessibleName(QStringLiteral("Merge: %1").arg(text(shown.MergeState)));
			return;
		}
		header->setText(QStringLiteral("Needs attention"));
		header->set_count(count_text(items.size()));
		header->setAccessibleName(QStringLiteral("Needs attention: %1").arg(plural(items.size(), QStringLiteral("1 item"), QStringLiteral("%1 items"))));
		// Each failed check is named once: here, unless Checks is open and
		// lists them.
		const auto checksOpen = Sections[static_cast<std::size_t>(overview_status_section::checks)].Header->isChecked()
			&& !Sections[static_cast<std::size_t>(overview_status_section::checks)].Frame->isHidden();
		auto* const parent = AttentionList->parentWidget();
		const auto  faint  = palette().color(QPalette::Active, QPalette::PlaceholderText);
		for (const auto& item : items) {
			auto title  = QString{ };
			auto detail = QString{ };
			auto tone   = faint;
			switch (item.Kind) {
			case core::status_attention_kind::conflicts:
				title  = QStringLiteral("Conflicts with %1").arg(text(shown.BaseRef));
				detail = QStringLiteral("GitHub cannot merge until they are resolved");
				tone   = Colours.Defect;
				break;
			case core::status_attention_kind::failed_checks:
				title = plural(item.Count, QStringLiteral("1 check failed"), QStringLiteral("%1 checks failed"));
				if (!checksOpen) {
					auto names = QStringList{ };
					for (const auto& name : item.Names) {
						names.push_back(text(name));
					}
					detail = names.join(QStringLiteral(", "));
				}
				tone = Colours.Defect;
				break;
			case core::status_attention_kind::waiting_on: {
				title            = QStringLiteral("Waiting on %1").arg(name_list(item.Names));
				const auto owner = std::ranges::any_of(shown.ReviewRequests, [&](const application::overview_review_request& aRequest) {
					return aRequest.CodeOwner && std::ranges::contains(item.Names, aRequest.Name);
				});
				detail           = owner ? QStringLiteral("Code-owner review requested") : plural(item.Count, QStringLiteral("Review requested"), QStringLiteral("%1 reviews requested"));
				break;
			}
			case core::status_attention_kind::replies_to_me:
				title = plural(item.Count, QStringLiteral("1 reply to you"), QStringLiteral("%1 replies to you"));
				tone  = Colours.Unread;
				break;
			}
			// One wrapping label: the tone's dot hangs beside the title and its
			// second line, so a long line wraps under its own text.
			const auto ink  = palette().color(QPalette::Active, QPalette::WindowText);
			auto       html = QStringLiteral("<table cellspacing=\"0\" cellpadding=\"0\"><tr><td width=\"16\" valign=\"top\"><span style=\"color:%1\">&#9679;</span></td>"
											 "<td valign=\"top\"><span style=\"color:%2\">%3</span>")
								  .arg(tone.name(), ink.name(), title.toHtmlEscaped());
			if (!detail.isEmpty()) {
				html += QStringLiteral("<br><span style=\"color:%1\">%2</span>").arg(faint.name(), detail.toHtmlEscaped());
			}
			html += QStringLiteral("</td></tr></table>");
			auto* const row = new QLabel{ parent };
			row->setObjectName(QStringLiteral("overviewAttentionRow"));
			row->setTextFormat(Qt::RichText);
			row->setText(html);
			row->setWordWrap(true);
			row->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
			row->setContentsMargins(0, 2, 0, 2);
			row->setProperty("statusText", detail.isEmpty() ? title : title + QLatin1Char{ '\n' } + detail);
			row->setAccessibleName(detail.isEmpty() ? title : title + QStringLiteral(": ") + detail);
			AttentionList->addWidget(row);
		}
	}

	void pull_request_overview_page::present_my_review()
	{
		if (!Projection) {
			NewThreadsRow->hide();
			NewConversationRow->hide();
			return;
		}
		const auto& progress = Projection->Progress;
		using readiness      = application::overview_readiness;
		const auto value     = [](const readiness aReadiness, const QString& aReady) {
			switch (aReadiness) {
			case readiness::loading:     return QStringLiteral("Loading…");
			case readiness::unavailable: return QStringLiteral("Unavailable");
			case readiness::ready:       return aReady;
			}
			return QStringLiteral("Unavailable");
		};
		const auto filesReady = progress.FilesReadiness == readiness::ready;
		const auto notesReady = progress.NotesReadiness == readiness::ready;
		const auto filesRead  = QStringLiteral("%1 of %2").arg(count_text(progress.FilesRead), count_text(progress.FilesTotal));
		set_value(overview_value::files_read, value(progress.FilesReadiness, filesRead), false);
		show_value(overview_value::files_read, progress.FilesTotal != 0U || !filesReady);
		ReadMeter->set_segments({ { static_cast<double>(progress.FilesRead), Colours.Accepted } }, static_cast<double>(progress.FilesTotal));
		ReadMeter->setAccessibleName(filesReady ? filesRead + QStringLiteral(" files read") : QString{ });
		ReadMeter->setVisible(filesReady && progress.FilesTotal != 0U);
		set_value(overview_value::private_notes, value(progress.NotesReadiness, count_text(progress.PrivateNotes)), false);
		show_value(overview_value::private_notes, progress.PrivateNotes != 0U || !notesReady);
		set_value(overview_value::drafted_comments, count_text(progress.DraftedComments.value_or(0U)), false);
		show_value(overview_value::drafted_comments, progress.DraftedComments.value_or(0U) != 0U);
		NewThreadsText->setText(plural(NewThreads, QStringLiteral("1 thread with new posts"), QStringLiteral("%1 threads with new posts")));
		NewThreadsGo->setAccessibleName(QStringLiteral("Go to the %1").arg(NewThreadsText->text()));
		auto unread = palette();
		unread.setColor(QPalette::WindowText, Colours.Unread);
		NewThreadsText->setPalette(unread);
		for (auto* const go : { NewThreadsGo, NewConversationGo }) {
			auto link = go->palette();
			link.setColor(QPalette::ButtonText, Colours.Unread);
			go->setPalette(link);
		}
		NewThreadsRow->setVisible(NewThreads != 0U);
		const auto conversation = NewConversation.value_or(0U);
		NewConversationText->setText(plural(conversation, QStringLiteral("1 new comment in the conversation"), QStringLiteral("%1 new comments in the conversation")));
		NewConversationText->setPalette(unread);
		NewConversationGo->setAccessibleName(QStringLiteral("Go to the %1 in Discuss").arg(NewConversationText->text()));
		NewConversationRow->setVisible(conversation != 0U);
		auto* const header = Sections[static_cast<std::size_t>(overview_status_section::my_review)].Header;
		// Readiness stays visible even when the section is collapsed.
		const auto loading = progress.FilesReadiness == readiness::loading || progress.NotesReadiness == readiness::loading;
		if (loading) {
			header->set_count(QStringLiteral("Loading…"));
		} else if (!filesReady || !notesReady) {
			header->set_count(QStringLiteral("Some data unavailable"));
		} else {
			header->set_count({ });
		}
		auto summary = filesReady ? filesRead + QStringLiteral(" files read") : QStringLiteral("File progress: %1").arg(value(progress.FilesReadiness, { }));
		if (!notesReady) {
			summary += QStringLiteral("; private notes: %1").arg(value(progress.NotesReadiness, { }));
		}
		header->setAccessibleName(QStringLiteral("My review: %1").arg(summary));
	}

	void pull_request_overview_page::present_request_rows()
	{
		const auto& requests = Projection ? Projection->ReviewRequests : ObservedPeople->ReviewRequests;
		const auto& note     = Projection ? Projection->ReviewRequestsNote : ObservedPeople->ReviewRequestsNote;
		// Rows are rebuilt only when the roster changed, so a refresh that
		// settles the same requests leaves them, and their focus, alone.
		const auto current = request_rows();
		if (!std::ranges::equal(current, requests, { }, [](const overview_request_row* aRow) -> const auto& { return aRow->request(); })) {
			while (Requests->count() != 0) {
				auto* const item = Requests->takeAt(0);
				delete item->widget();
				delete item;
			}
			for (const auto& request : requests) {
				Requests->addWidget(new overview_request_row{ request, Colours });
			}
			++RequestRowBuilds;
		}
		RequestsNote->setText(text(note));
		RequestsNote->setAccessibleName(RequestsNote->text());
		RequestsNote->setVisible(!note.empty());
	}

	auto pull_request_overview_page::present_review_requests(const application::pull_request_overview_projection& aProjection) -> bool
	{
		const auto samePullRequest = Projection.has_value() && !Cached && Projection->Owner == aProjection.Owner && Projection->RepositoryName == aProjection.RepositoryName
			&& Projection->Number == aProjection.Number;
		if (!samePullRequest) {
			return false;
		}
		Projection->ReviewRequests     = aProjection.ReviewRequests;
		Projection->ReviewRequestsNote = aProjection.ReviewRequestsNote;
		Projection->Status             = aProjection.Status;
		present_request_rows();
		present_status(true);
		++ReviewRequestUpdates;
		return true;
	}

	void pull_request_overview_page::set_draft_state_offer(const std::optional<core::draft_state_change> aOffered)
	{
		if (DraftStateOffer == aOffered) {
			return;
		}
		DraftStateOffer = aOffered;
		present_draft_controls();
	}

	auto pull_request_overview_page::draft_state_offer() const noexcept -> std::optional<core::draft_state_change>
	{
		return DraftStateOffer;
	}

	void pull_request_overview_page::set_draft_state_dispatch(std::function<void(core::draft_state_change)> aDispatch)
	{
		DraftStateDispatch = std::move(aDispatch);
	}

	void pull_request_overview_page::present_draft_state_status(const core::draft_state_change aChange, const QString& aText, const bool aBusy)
	{
		auto* const status                                                                = aChange == core::draft_state_change::ready_for_review ? ReadyStatus : ConvertStatus;
		(aChange == core::draft_state_change::ready_for_review ? ReadyBusy : ConvertBusy) = aBusy;
		status->setText(aText);
		// What GitHub refused is said in the defect ink, as Needs attention
		// says a conflict; nothing else is coloured.
		auto ink = palette();
		if (!aText.isEmpty() && !aBusy) {
			ink.setColor(QPalette::WindowText, Colours.Defect);
		}
		status->setPalette(ink);
		present_draft_controls();
	}

	auto pull_request_overview_page::present_draft_state(const application::pull_request_overview_projection& aProjection) -> bool
	{
		const auto samePullRequest = Projection.has_value() && !Cached && Projection->Owner == aProjection.Owner && Projection->RepositoryName == aProjection.RepositoryName
			&& Projection->Number == aProjection.Number;
		if (!samePullRequest) {
			return false;
		}
		Projection->Draft              = aProjection.Draft;
		Projection->DraftPending       = aProjection.DraftPending;
		Projection->MergeState         = aProjection.MergeState;
		Projection->MergeTone          = aProjection.MergeTone;
		Projection->RequirementDetails = aProjection.RequirementDetails;
		ShownDraft                     = aProjection.Draft;
		ShownDraftPending              = aProjection.DraftPending;
		present_draft_controls();
		present_merge(*Projection);
		++DraftStateUpdates;
		return true;
	}

	void pull_request_overview_page::present_draft_controls()
	{
		if (DraftChip == nullptr || ReadyForReview == nullptr || ConvertToDraft == nullptr) {
			return;
		}
		const auto openDraft = ShownDraft && ShownOpen;
		DraftChip->setVisible(openDraft);
		// The action is the author's, on a review that has loaded.
		const auto loaded = Projection.has_value() && !Cached;
		// Marked ready and waiting on GitHub: the chip has gone at once.
		const auto readying = loaded && ShownOpen && ReadyBusy && !ShownDraft;
		const auto ready    = loaded && openDraft && DraftStateOffer == core::draft_state_change::ready_for_review;
		const auto convert  = loaded && !ShownDraft && ShownOpen && DraftStateOffer == core::draft_state_change::convert_to_draft;
		// Pressed, it stays where it was, disabled, until GitHub answers; the
		// chip has already gone.
		ReadyForReview->setVisible(ready || readying);
		ReadyForReview->setEnabled(!ReadyBusy && !ShownDraftPending);
		ReadyForReview->setText(ReadyBusy ? QStringLiteral("Marking ready…") : QStringLiteral("Ready for review"));
		ReadyStatus->setVisible(!ReadyStatus->text().isEmpty());
		// While a conversion waits on GitHub the button stays, disabled, where
		// it was pressed, and says so.
		ConvertToDraft->setVisible(convert || (ConvertBusy && loaded && ShownOpen));
		ConvertToDraft->setEnabled(!ConvertBusy);
		ConvertToDraft->setText(ConvertBusy ? QStringLiteral("Converting to draft…") : QStringLiteral("Convert to draft"));
		ConvertStatus->setVisible(!ConvertStatus->text().isEmpty());
	}

	auto pull_request_overview_page::draft_chip() const noexcept -> QWidget*
	{
		return DraftChip;
	}

	auto pull_request_overview_page::ready_for_review_button() const noexcept -> QAbstractButton*
	{
		return ReadyForReview;
	}

	auto pull_request_overview_page::convert_to_draft_button() const noexcept -> QAbstractButton*
	{
		return ConvertToDraft;
	}

	auto pull_request_overview_page::draft_state_status(const core::draft_state_change aChange) const noexcept -> QLabel*
	{
		return aChange == core::draft_state_change::ready_for_review ? ReadyStatus : ConvertStatus;
	}

	auto pull_request_overview_page::draft_state_updates() const noexcept -> std::size_t
	{
		return DraftStateUpdates;
	}

	auto pull_request_overview_page::request_row_builds() const noexcept -> std::size_t
	{
		return RequestRowBuilds;
	}

	auto pull_request_overview_page::review_request_updates() const noexcept -> std::size_t
	{
		return ReviewRequestUpdates;
	}

	void pull_request_overview_page::present_people()
	{
		const auto& participants = Projection ? Projection->Participants : ObservedPeople->Participants;
		const auto& requests     = Projection ? Projection->ReviewRequests : ObservedPeople->ReviewRequests;
		auto        faces        = std::vector<overview_face>{ };
		auto        tally        = std::array<std::size_t, 3>{ };
		for (const auto& person : participants) {
			auto face = overview_face{ .Identity = text(person.Login) };
			switch (person.Signoff) {
			case application::overview_signoff::approved:
				face.Badge = Colours.Accepted;
				++tally[0];
				break;
			case application::overview_signoff::changes_requested:
				face.Badge = Colours.Defect;
				++tally[1];
				break;
			case application::overview_signoff::commented:
				++tally[2];
				break;
			case application::overview_signoff::none:
			case application::overview_signoff::uploaded:
			case application::overview_signoff::dismissed:
			case application::overview_signoff::pending:
				break;
			}
			faces.push_back(std::move(face));
		}
		for (const auto& request : requests) {
			const auto identity = text(request.Name);
			faces.push_back({ .Identity = identity, .InitialsFrom = request.Team ? identity.mid(identity.indexOf(QLatin1Char{ '/' }) + 1) : QString{ }, .Team = request.Team });
		}
		Faces->set_faces(std::move(faces));
		auto words = QStringList{ };
		if (tally[0] != 0U) {
			words.push_back(QStringLiteral("%1 approved").arg(count_text(tally[0])));
		}
		if (tally[1] != 0U) {
			words.push_back(QStringLiteral("%1 requested changes").arg(count_text(tally[1])));
		}
		if (tally[2] != 0U) {
			words.push_back(QStringLiteral("%1 commented").arg(count_text(tally[2])));
		}
		if (!requests.empty()) {
			words.push_back(QStringLiteral("%1 requested").arg(count_text(requests.size())));
		}
		if (words.isEmpty()) {
			words.push_back(plural(participants.size(), QStringLiteral("1 person"), QStringLiteral("%1 people")));
		}
		PeopleSummary->setText(words.join(QStringLiteral(", ")));
		Faces->setAccessibleName(QStringLiteral("People: %1").arg(PeopleSummary->text()));
		const auto  people = participants.size() + requests.size();
		auto* const header = Sections[static_cast<std::size_t>(overview_status_section::people)].Header;
		header->set_count(count_text(people));
		header->setAccessibleName(QStringLiteral("People: %1").arg(PeopleSummary->text()));
	}
}
