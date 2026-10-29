#include "bpr/qt/reviewer_picker.hpp"

#include "bpr/core/review_role.hpp"
#include "bpr/qt/identity_avatar.hpp"

#include <QAccessible>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QScreen>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		constexpr auto login_role    = Qt::UserRole;
		constexpr auto name_role     = Qt::UserRole + 1;
		constexpr auto standing_role = Qt::UserRole + 2;
		constexpr auto heading_role  = Qt::UserRole + 3;

		[[nodiscard]] auto text(const std::string& aText) -> QString
		{
			return QString::fromStdString(aText);
		}

		/// \brief One person as the People roster draws one: the shared
		/// avatar, the login, the profile name in the quiet ink, and a short
		/// word at the right. A heading is a small bold line.
		class reviewer_row_delegate final : public QStyledItemDelegate
		{
		public:
			using QStyledItemDelegate::QStyledItemDelegate;

			void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
			{
				auto option = aOption;
				initStyleOption(&option, aIndex);
				const auto& colours = option.palette;
				aPainter->save();
				aPainter->setRenderHint(QPainter::Antialiasing, true);
				if (aIndex.data(heading_role).toBool()) {
					auto font = option.font;
					font.setWeight(QFont::DemiBold);
					font.setPointSizeF(font.pointSizeF() * 0.92);
					aPainter->setFont(font);
					aPainter->setPen(colours.color(QPalette::Active, QPalette::PlaceholderText));
					aPainter->drawText(option.rect.adjusted(8, 0, -8, 0), Qt::AlignLeft | Qt::AlignBottom, option.text);
					aPainter->restore();
					return;
				}
				const auto selected = (option.state & QStyle::State_Selected) != 0;
				if (selected) {
					aPainter->fillRect(option.rect, colours.color(QPalette::Active, QPalette::Highlight));
				} else if ((option.state & QStyle::State_MouseOver) != 0) {
					aPainter->fillRect(option.rect, colours.color(QPalette::Active, QPalette::AlternateBase));
				}
				const auto login  = aIndex.data(login_role).toString();
				const auto name   = aIndex.data(name_role).toString();
				const auto word   = aIndex.data(standing_role).toString();
				const auto ink    = colours.color(QPalette::Active, selected ? QPalette::HighlightedText : QPalette::Text);
				const auto faint  = selected ? ink : colours.color(QPalette::Active, QPalette::PlaceholderText);
				const auto centre = QPointF{ option.rect.left() + 18.0, option.rect.center().y() + 0.5 };
				paint_identity_avatar(*aPainter, QRectF{ centre.x() - 10.0, centre.y() - 10.0, 20.0, 20.0 }, identity_avatar_face{ .Identity = login },
									  identity_avatar_grounds_of(colours), option.font);
				const auto metrics   = QFontMetrics{ option.font };
				const auto wordWidth = word.isEmpty() ? 0 : metrics.horizontalAdvance(word) + 12;
				auto       area      = option.rect.adjusted(36, 0, -8 - wordWidth, 0);
				aPainter->setFont(option.font);
				aPainter->setPen(ink);
				const auto loginText = metrics.elidedText(login, Qt::ElideRight, area.width());
				aPainter->drawText(area, Qt::AlignLeft | Qt::AlignVCenter, loginText);
				const auto used = metrics.horizontalAdvance(loginText) + 8;
				if (!name.isEmpty() && used < area.width()) {
					aPainter->setPen(faint);
					aPainter->drawText(area.adjusted(used, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, metrics.elidedText(name, Qt::ElideRight, area.width() - used));
				}
				if (!word.isEmpty()) {
					aPainter->setPen(faint);
					aPainter->drawText(option.rect.adjusted(0, 0, -8, 0), Qt::AlignRight | Qt::AlignVCenter, word);
				}
				aPainter->restore();
			}

			[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize override
			{
				const auto line = QFontMetrics{ aOption.font }.height();
				return { 240, aIndex.data(heading_role).toBool() ? line + 8 : std::max(30, line + 12) };
			}
		};

		[[nodiscard]] auto standing_word(const core::reviewer_row& aRow, const bool aPending) -> QString
		{
			if (aPending) {
				return QStringLiteral("Asking…");
			}
			switch (aRow.Standing) {
			case core::reviewer_standing::requested: return QStringLiteral("Requested");
			case core::reviewer_standing::reviewed:  return QStringLiteral("Ask again");
			case core::reviewer_standing::available: break;
			}
			return { };
		}

		[[nodiscard]] auto standing_words(const core::reviewer_row& aRow, const bool aPending) -> QString
		{
			if (aPending) {
				return QStringLiteral("asking GitHub now");
			}
			switch (aRow.Standing) {
			case core::reviewer_standing::requested: return QStringLiteral("review requested");
			case core::reviewer_standing::reviewed:  return QStringLiteral("reviewed, choose to ask again");
			case core::reviewer_standing::available: break;
			}
			return QStringLiteral("can be asked to review");
		}
	} // namespace

	reviewer_picker::reviewer_picker(QWidget* const aParent)
		: QFrame{ aParent, Qt::Popup }
	{
		setObjectName(QStringLiteral("overviewReviewerPicker"));
		// Its own window: it shows the theme the page shows.
		setAttribute(Qt::WA_WindowPropagation);
		setAccessibleName(QStringLiteral("Request a review"));
		setAccessibleDescription(QStringLiteral("Search for a person, then choose them to ask GitHub for their review."));
		setFrameShape(QFrame::StyledPanel);
		auto* const layout = new QVBoxLayout{ this };
		layout->setContentsMargins(6, 6, 6, 6);
		layout->setSpacing(4);
		Search = new QLineEdit{ this };
		Search->setObjectName(QStringLiteral("overviewReviewerSearch"));
		Search->setAccessibleName(QStringLiteral("Search people"));
		Search->setPlaceholderText(QStringLiteral("Search by login or name"));
		Search->setClearButtonEnabled(true);
		Search->installEventFilter(this);
		layout->addWidget(Search);
		List = new QListWidget{ this };
		List->setObjectName(QStringLiteral("overviewReviewerList"));
		List->setAccessibleName(QStringLiteral("People who can review"));
		List->setFrameShape(QFrame::NoFrame);
		List->setSelectionMode(QAbstractItemView::SingleSelection);
		List->setMouseTracking(true);
		List->setUniformItemSizes(false);
		List->setItemDelegate(new reviewer_row_delegate{ List });
		List->installEventFilter(this);
		layout->addWidget(List, 1);
		Status = new QLabel{ this };
		Status->setObjectName(QStringLiteral("overviewReviewerStatus"));
		Status->setWordWrap(true);
		Status->setForegroundRole(QPalette::PlaceholderText);
		Status->setTextFormat(Qt::PlainText);
		layout->addWidget(Status);
		setFixedWidth(340);
		connect(Search, &QLineEdit::textEdited, this, [this](const QString& aText) { dispatch(reviewer_picker_gesture::kind::query, aText.toStdString()); });
		connect(List, &QListWidget::itemClicked, this, [this](QListWidgetItem* const aItem) {
			if (aItem != nullptr && !aItem->data(heading_role).toBool()) {
				dispatch(reviewer_picker_gesture::kind::choose, aItem->data(login_role).toString().toStdString());
			}
		});
	}

	void reviewer_picker::set_dispatch(reviewer_picker_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void reviewer_picker::dispatch(const reviewer_picker_gesture::kind aKind, std::string aText)
	{
		if (Dispatch) {
			Dispatch({ .Kind = aKind, .Text = std::move(aText) });
		}
	}

	auto reviewer_picker::status_text(const core::reviewer_picker_state& aState) -> QString
	{
		const auto who = text(aState.NoticeLogin);
		if (aState.Pending) {
			return QStringLiteral("Asking GitHub for a review from %1…").arg(text(*aState.Pending));
		}
		switch (aState.Notice) {
		case core::reviewer_notice::requested:         return QStringLiteral("Requested a review from %1.").arg(who);
		case core::reviewer_notice::rerequested:       return QStringLiteral("Asked %1 to review again.").arg(who);
		case core::reviewer_notice::already_requested: return QStringLiteral("%1 already has a review request.").arg(who);
		case core::reviewer_notice::not_added:         return QStringLiteral("GitHub did not add %1. Only people with access to this repository can review it.").arg(who);
		case core::reviewer_notice::busy:              return QStringLiteral("Waiting for GitHub to answer the last request.");
		case core::reviewer_notice::failed:
			switch (aState.NoticeFailure.value_or(core::reviewer_request_failure::connection)) {
			case core::reviewer_request_failure::refused:        return QStringLiteral("GitHub did not let you request a review from %1.").arg(who);
			case core::reviewer_request_failure::target_changed: return QStringLiteral("The pull request changed on GitHub, so nothing was requested. Refresh and try again.");
			case core::reviewer_request_failure::cancelled:      return QStringLiteral("The request for %1 was stopped.").arg(who);
			case core::reviewer_request_failure::connection:     break;
			}
			return QStringLiteral("GitHub's answer for %1 was lost and it was not sent twice. People shows it after the next refresh.").arg(who);
		case core::reviewer_notice::none: break;
		}
		if (aState.Searching) {
			return QStringLiteral("Searching…");
		}
		if (aState.SearchFailure) {
			switch (*aState.SearchFailure) {
			case core::reviewer_request_failure::refused:        return QStringLiteral("GitHub did not let this account list reviewers.");
			case core::reviewer_request_failure::target_changed: return QStringLiteral("The pull request changed on GitHub. Refresh and try again.");
			case core::reviewer_request_failure::cancelled:      return { };
			case core::reviewer_request_failure::connection:     break;
			}
			return QStringLiteral("GitHub could not be asked. Type again to retry.");
		}
		if (aState.Rows.empty()) {
			return aState.Query.empty() ? QStringLiteral("GitHub suggests no one, and no one else can be asked here.")
										: QStringLiteral("No one who can review matches “%1”.").arg(text(aState.Query));
		}
		return { };
	}

	void reviewer_picker::rebuild(const core::reviewer_picker_state& aState)
	{
		const auto* const current = List->currentItem();
		const auto        keep    = current != nullptr ? current->data(login_role).toString() : QString{ };
		List->clear();
		const auto heading = [this](const QString& aText) {
			auto* const item = new QListWidgetItem{ aText, List };
			item->setData(heading_role, true);
			item->setFlags(Qt::ItemIsEnabled);
			item->setData(Qt::AccessibleTextRole, aText);
		};
		const auto anySuggested = std::ranges::any_of(aState.Rows, [](const core::reviewer_row& aRow) { return aRow.Candidate.Suggested; });
		auto       others       = false;
		auto*      restore      = static_cast<QListWidgetItem*>(nullptr);
		if (anySuggested) {
			heading(QStringLiteral("Suggested"));
		}
		for (const auto& row : aState.Rows) {
			if (anySuggested && !row.Candidate.Suggested && !others) {
				heading(QStringLiteral("Everyone who can review"));
				others = true;
			}
			const auto  pending = aState.Pending && core::same_login(*aState.Pending, row.Candidate.Login);
			auto* const item    = new QListWidgetItem{ List };
			const auto  login   = text(row.Candidate.Login);
			const auto  name    = text(row.Candidate.Name);
			item->setData(login_role, login);
			item->setData(name_role, name);
			item->setData(standing_role, standing_word(row, pending));
			auto spoken = name.isEmpty() ? login : QStringLiteral("%1, %2").arg(login, name);
			if (row.Candidate.Suggested) {
				spoken += QStringLiteral(", suggested");
			}
			item->setData(Qt::AccessibleTextRole, QStringLiteral("%1, %2").arg(spoken, standing_words(row, pending)));
			item->setToolTip(item->data(Qt::AccessibleTextRole).toString());
			if (login == keep) {
				restore = item;
			}
		}
		if (restore != nullptr) {
			List->setCurrentItem(restore);
		}
		auto rows = 0;
		for (int index = 0; index < List->count(); ++index) {
			rows += List->sizeHintForRow(index);
		}
		List->setFixedHeight(std::clamp(rows + 4, 40, 320));
	}

	void reviewer_picker::present(const core::reviewer_picker_state& aState, QWidget* const aAnchor)
	{
		Presenting = true;
		if (!aState.Open) {
			hide();
			Presenting = false;
			return;
		}
		rebuild(aState);
		const auto words = status_text(aState);
		if (Status->text() != words) {
			Status->setText(words);
			Status->setAccessibleName(words);
			QAccessibleEvent changed{ Status, QAccessible::NameChanged };
			QAccessible::updateAccessibility(&changed);
		}
		Status->setVisible(!words.isEmpty());
		adjustSize();
		if (!isVisible()) {
			Search->setText(text(aState.Query));
			if (aAnchor != nullptr) {
				auto place = aAnchor->mapToGlobal(QPoint{ 0, aAnchor->height() + 2 });
				if (const auto* const screen = aAnchor->screen(); screen != nullptr) {
					const auto room = screen->availableGeometry();
					place.setX(std::clamp(place.x(), room.left(), std::max(room.left(), room.right() - width())));
					if (place.y() + height() > room.bottom()) {
						place.setY(aAnchor->mapToGlobal(QPoint{ 0, 0 }).y() - height() - 2);
					}
				}
				move(place);
			}
			show();
			Search->setFocus(Qt::PopupFocusReason);
		}
		Presenting = false;
	}

	auto reviewer_picker::search_field() const noexcept -> QLineEdit*
	{
		return Search;
	}

	auto reviewer_picker::people() const noexcept -> QListWidget*
	{
		return List;
	}

	auto reviewer_picker::status() const noexcept -> QLabel*
	{
		return Status;
	}

	auto reviewer_picker::listed_logins() const -> QStringList
	{
		auto logins = QStringList{ };
		for (int index = 0; index < List->count(); ++index) {
			if (!List->item(index)->data(heading_role).toBool()) {
				logins.push_back(List->item(index)->data(login_role).toString());
			}
		}
		return logins;
	}

	void reviewer_picker::move_highlight(const int aStep)
	{
		const auto count = List->count();
		if (count == 0 || aStep == 0) {
			return;
		}
		auto index = List->currentRow();
		if (index < 0) {
			index = aStep > 0 ? -1 : count;
		}
		for (auto next = index + aStep; next >= 0 && next < count; next += aStep) {
			if (!List->item(next)->data(heading_role).toBool()) {
				List->setCurrentRow(next);
				List->scrollToItem(List->item(next));
				return;
			}
		}
	}

	void reviewer_picker::choose_highlighted()
	{
		const auto* const item = List->currentItem();
		if (item != nullptr && !item->data(heading_role).toBool()) {
			dispatch(reviewer_picker_gesture::kind::choose, item->data(login_role).toString().toStdString());
		}
	}

	bool reviewer_picker::eventFilter(QObject* const aObject, QEvent* const aEvent)
	{
		if ((aObject == Search || aObject == List) && aEvent->type() == QEvent::KeyPress) {
			const auto* const key = static_cast<QKeyEvent*>(aEvent);
			switch (key->key()) {
			case Qt::Key_Down:
				move_highlight(1);
				return true;
			case Qt::Key_Up:
				move_highlight(-1);
				return true;
			case Qt::Key_Return:
			case Qt::Key_Enter:
				if (List->currentItem() == nullptr) {
					move_highlight(1);
				}
				choose_highlighted();
				return true;
			case Qt::Key_Escape:
				hide();
				return true;
			default:
				break;
			}
		}
		return QFrame::eventFilter(aObject, aEvent);
	}

	void reviewer_picker::keyPressEvent(QKeyEvent* const aEvent)
	{
		if (aEvent->key() == Qt::Key_Escape) {
			hide();
			return;
		}
		QFrame::keyPressEvent(aEvent);
	}

	void reviewer_picker::hideEvent(QHideEvent* const aEvent)
	{
		QFrame::hideEvent(aEvent);
		if (!Presenting) {
			dispatch(reviewer_picker_gesture::kind::close);
		}
	}
} // namespace bpr::qt
