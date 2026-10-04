#include "bpr/qt/review_cycle_promote_sheet.hpp"

#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>
#include <type_traits>
#include <variant>

namespace bpr::qt {
	namespace {
		/// A row's note, as the item's data, so a check names exactly it.
		constexpr int item_role = Qt::UserRole + 1;
		/// A row's two lines as one text, for a reader or a course.
		constexpr int text_role = Qt::UserRole + 2;

		auto text_of(const std::string& aText) -> QString
		{
			return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size()));
		}

		auto words_label(QWidget* aParent, const QString& aName) -> QLabel*
		{
			auto* const label = new QLabel{ aParent };
			label->setObjectName(aName);
			label->setTextFormat(Qt::PlainText);
			label->setWordWrap(true);
			return label;
		}

		auto section(QWidget* aParent, const QString& aText) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			auto        font  = label->font();
			font.setBold(true);
			label->setFont(font);
			return label;
		}

		/// Where the note is, in the cycle's words.
		auto location(const core::cycle_feedback_target& aTarget) -> QString
		{
			return std::visit(
				[](const auto& aValue) -> QString {
					using target = std::decay_t<decltype(aValue)>;
					if constexpr (std::is_same_v<target, core::cycle_whole_target>) {
						return QStringLiteral("The whole change");
					} else if constexpr (std::is_same_v<target, core::cycle_module_target>) {
						return aValue.Path.empty() ? QStringLiteral("The whole change") : QStringLiteral("Folder %1/").arg(text_of(aValue.Path));
					} else {
						return text_of(aValue.Path);
					}
				},
				aTarget);
		}

		auto first_line(const std::string& aBody) -> QString
		{
			auto text = text_of(aBody).trimmed();
			if (const auto end = text.indexOf(QLatin1Char('\n')); end >= 0) {
				text = text.left(end).trimmed() + QStringLiteral(" …");
			}
			return text;
		}

		auto lines_words(const core::tour_code_destination& aLines) -> QString
		{
			return aLines.FirstLine == aLines.LastLine ? QStringLiteral("Line %1").arg(aLines.FirstLine + 1U) : QStringLiteral("Lines %1 to %2").arg(aLines.FirstLine + 1U).arg(aLines.LastLine + 1U);
		}

		/// Why a stop has no lines yet, or which lines it keeps.
		auto source_words(const application::review_cycle_promotion_row& aRow) -> QString
		{
			if (aRow.Refusal) {
				return *aRow.Refusal == core::cycle_promotion_refusal::empty ? QStringLiteral("Cannot be added: the note has no words.")
					: *aRow.Refusal == core::cycle_promotion_refusal::bounds ? QStringLiteral("Cannot be added: the note is longer than a tour stop holds.")
																			  : QStringLiteral("Cannot be added: the pull request's publication could not be read.");
			}
			const auto later = QStringLiteral(" You choose its lines when you complete it in the pull request.");
			switch (aRow.Source.value_or(core::cycle_promotion_source::whole_change)) {
			case core::cycle_promotion_source::exact_lines:
				return aRow.Lines ? QStringLiteral("%1, exactly as the pull request's commit holds them.").arg(lines_words(*aRow.Lines)) : QString{ };
			case core::cycle_promotion_source::earlier_revision: return QStringLiteral("No lines: written on an earlier capture than the one published.") + later;
			case core::cycle_promotion_source::other_bytes:      return QStringLiteral("No lines: the pull request holds this file with other bytes.") + later;
			case core::cycle_promotion_source::outside_changes:  return QStringLiteral("No lines: this file is not among the pull request's changes.") + later;
			case core::cycle_promotion_source::unread:           return QStringLiteral("No lines: its file could not be read back to prove them.") + later;
			case core::cycle_promotion_source::whole_file:       return QStringLiteral("No lines: the note is about the whole file.") + later;
			case core::cycle_promotion_source::folder:           return QStringLiteral("No lines: the note is about a folder.") + later;
			case core::cycle_promotion_source::whole_change:     return QStringLiteral("No lines: the note is about the whole change.") + later;
			case core::cycle_promotion_source::linked_by_hand:
				return QStringLiteral("No lines: this pull request was linked by hand, so which lines it holds is not known here.") + later;
			}
			return { };
		}

		/// What the tour's own policy makes of the stop on GitHub.
		auto planned_words(const application::review_cycle_promotion_row& aRow) -> QString
		{
			if (!aRow.Lines || !aRow.Planned) {
				return { };
			}
			switch (aRow.Planned->Representation) {
			case core::tour_representation::native_line:            return QStringLiteral(" When the tour is published: a comment on these lines.");
			case core::tour_representation::conversation_permalink: return QStringLiteral(" When the tour is published: a comment linking these lines, since the pull request has moved on.");
			case core::tour_representation::file_permalink:         return QStringLiteral(" When the tour is published: a comment linking this file.");
			case core::tour_representation::unresolved:             return QStringLiteral(" How it appears on GitHub is decided when the tour is published.");
			}
			return { };
		}

		auto outcome_words(const application::review_cycle_promotion_outcome aOutcome) -> QString
		{
			switch (aOutcome) {
			case application::review_cycle_promotion_outcome::none:       return { };
			case application::review_cycle_promotion_outcome::added:      return QStringLiteral("Added to the tour. ");
			case application::review_cycle_promotion_outcome::already:    return QStringLiteral("Already in the tour. ");
			case application::review_cycle_promotion_outcome::refused:    return QStringLiteral("The tour refused it. ");
			case application::review_cycle_promotion_outcome::unanswered: return QStringLiteral("No answer: add it again. ");
			}
			return { };
		}
	}

	auto review_cycle_promote_sheet::row_text(const application::review_cycle_promotion_row& aRow) -> QString
	{
		return QStringLiteral("%1: %2\n%3%4%5").arg(location(aRow.Primary), first_line(aRow.Body), outcome_words(aRow.Outcome), source_words(aRow), planned_words(aRow));
	}

	review_cycle_promote_sheet::review_cycle_promote_sheet(QWidget* aParent)
		: QDialog{ aParent }
	{
		setObjectName(QStringLiteral("cyclePromoteSheet"));
		setWindowTitle(QStringLiteral("Add Notes to Review Tour"));
		setWindowModality(Qt::WindowModal);
#ifdef Q_OS_MACOS
		setWindowFlag(Qt::Sheet, true);
#endif
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(20, 16, 20, 14);
		column->setSpacing(8);

		Title     = words_label(this, QStringLiteral("cyclePromoteTitle"));
		auto font = Title->font();
		font.setBold(true);
		font.setPointSizeF(font.pointSizeF() * 1.25);
		Title->setFont(font);
		column->addWidget(Title);
		Intro = words_label(this, QStringLiteral("cyclePromoteIntro"));
		column->addWidget(Intro);

		// The one thing the sheet spends its weight on: each note, and where its stop points.
		Notes = new QListWidget{ this };
		Notes->setObjectName(QStringLiteral("cyclePromoteNotes"));
		Notes->setAccessibleName(QStringLiteral("Notes to add"));
		Notes->setWordWrap(true);
		Notes->setSelectionMode(QAbstractItemView::NoSelection);
		Notes->setTextElideMode(Qt::ElideNone);
		Notes->setSpacing(3);
		column->addWidget(Notes, 1);

		auto* const ledger = new QFrame{ this };
		ledger->setObjectName(QStringLiteral("cyclePromoteLedger"));
		ledger->setFrameShape(QFrame::StyledPanel);
		auto* const ledgerRow = new QHBoxLayout{ ledger };
		ledgerRow->setContentsMargins(12, 8, 12, 8);
		ledgerRow->setSpacing(12);
		const auto ledgerColumn = [&](const QString& aHeading, QLabel*& aWords, const QString& aName, const QString& aText) {
			auto* const part   = new QVBoxLayout{ };
			auto* const header = section(ledger, aHeading);
			header->setObjectName(aName + QStringLiteral("Heading"));
			part->addWidget(header);
			aWords = words_label(ledger, aName);
			aWords->setText(aText);
			part->addWidget(aWords);
			part->addStretch(1);
			ledgerRow->addLayout(part, 1);
		};
		ledgerColumn(QStringLiteral("Goes to GitHub"), Outgoing, QStringLiteral("cyclePromoteOutgoingWords"),
					 QStringLiteral("Nothing now. The tour is published only from the pull request, through its own preview and confirmation."));
		auto* const divider = new QFrame{ ledger };
		divider->setFrameShape(QFrame::VLine);
		divider->setFrameShadow(QFrame::Sunken);
		ledgerRow->addWidget(divider);
		ledgerColumn(QStringLiteral("Stays on this computer"), Private, QStringLiteral("cyclePromotePrivateWords"),
					 QStringLiteral("Replies, decisions, suggestions, rounds and earlier wordings of each note, and every note you leave unchecked."));
		column->addWidget(ledger);

		Status = words_label(this, QStringLiteral("cyclePromoteStatus"));
		Status->setAccessibleName(QStringLiteral("Add Notes to Review Tour status"));
		// Room for its longest account, so the list gives way and the words never clip.
		Status->setMinimumHeight(Status->fontMetrics().lineSpacing() * 3 + 4);
		Status->setAlignment(Qt::AlignLeft | Qt::AlignTop);
		column->addWidget(Status);

		auto* const actions = new QHBoxLayout{ };
		Add                 = new QPushButton{ QStringLiteral("Add Notes"), this };
		Add->setObjectName(QStringLiteral("cyclePromoteAdd"));
		Add->setDefault(true);
		actions->addWidget(Add);
		OpenPull = new QPushButton{ QStringLiteral("Open Pull Request"), this };
		OpenPull->setObjectName(QStringLiteral("cyclePromoteOpenPullRequest"));
		OpenPull->setAutoDefault(false);
		OpenPull->setToolTip(QStringLiteral("Open the pull request, where its Review Tour is completed and published"));
		actions->addWidget(OpenPull);
		actions->addStretch(1);
		Close = new QPushButton{ QStringLiteral("Done"), this };
		Close->setObjectName(QStringLiteral("cyclePromoteClose"));
		Close->setAutoDefault(false);
		Close->setToolTip(QStringLiteral("Close; what was added stays in the tour (Esc)"));
		actions->addWidget(Close);
		column->addLayout(actions);

		connect(Notes, &QListWidget::itemChanged, this, [this](QListWidgetItem* aItem) {
			if (Presenting || aItem == nullptr || !Dispatch.Choose || (aItem->flags() & Qt::ItemIsUserCheckable) == 0) {
				return;
			}
			Dispatch.Choose(core::cycle_feedback_item_id{ aItem->data(item_role).toULongLong() }, aItem->checkState() == Qt::Checked);
		});
		connect(Add, &QPushButton::clicked, this, [this] {
			if (Dispatch.Add) {
				Dispatch.Add();
			}
		});
		connect(OpenPull, &QPushButton::clicked, this, [this] {
			if (Dispatch.OpenPullRequest) {
				Dispatch.OpenPullRequest();
			}
		});
		connect(Close, &QPushButton::clicked, this, [this] { reject(); });
	}

	void review_cycle_promote_sheet::set_dispatch(review_cycle_promote_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void review_cycle_promote_sheet::present(const application::review_cycle_promotion_view& aView)
	{
		Presenting = true;
		Shown      = aView;
		const auto number = static_cast<qulonglong>(aView.Link.Address.Number);
		Title->setText(QStringLiteral("Add notes to the Review Tour of #%1").arg(number));
		Intro->setText(QStringLiteral("%1/%2 #%3. Each note you check becomes one stop, in its own words. A stop waits for you to complete it in the pull request.")
						   .arg(text_of(aView.Link.Address.Owner), text_of(aView.Link.Address.Repository))
						   .arg(number));

		// Rebuilt in place: the same note keeps its row and its check.
		const auto ready = aView.Target == application::review_cycle_promotion_target::ready;
		while (Notes->count() > static_cast<int>(aView.Rows.size())) {
			delete Notes->takeItem(Notes->count() - 1);
		}
		for (std::size_t index = 0; index < aView.Rows.size(); ++index) {
			const auto& row  = aView.Rows[index];
			auto*       item = index < static_cast<std::size_t>(Notes->count()) ? Notes->item(static_cast<int>(index)) : new QListWidgetItem{ Notes };
			const auto  text = row_text(row);
			item->setText(text);
			item->setData(text_role, text);
			item->setData(item_role, static_cast<qulonglong>(row.Item.Id.value()));
			const auto addable = row.Source.has_value() && ready && !aView.Busy;
			item->setFlags(addable ? (Qt::ItemIsEnabled | Qt::ItemIsUserCheckable) : Qt::ItemIsEnabled);
			item->setCheckState(row.Chosen ? Qt::Checked : Qt::Unchecked);
			item->setToolTip(text_of(row.Body));
		}

		const auto chosen = static_cast<std::size_t>(std::ranges::count_if(aView.Rows, [](const auto& aRow) { return aRow.Chosen && aRow.Source; }));
		Add->setText(chosen == 1U ? QStringLiteral("Add 1 Note") : chosen == 0U ? QStringLiteral("Add Notes") : QStringLiteral("Add %1 Notes").arg(chosen));
		Add->setVisible(ready || aView.Target == application::review_cycle_promotion_target::looking);
		Add->setEnabled(ready && !aView.Busy && chosen > 0U);
		OpenPull->setEnabled(!aView.Busy);
		Status->setText(text_of(aView.Status));
		Status->setVisible(!aView.Status.empty());
		Presenting = false;
	}

	void review_cycle_promote_sheet::set_anchor(const QPoint aGlobal)
	{
		Anchor = aGlobal;
	}

	void review_cycle_promote_sheet::focus_first()
	{
		if (Notes->count() > 0) {
			Notes->setFocus(Qt::OtherFocusReason);
			Notes->setCurrentRow(0);
		} else {
			Close->setFocus(Qt::OtherFocusReason);
		}
	}

	void review_cycle_promote_sheet::done(const int aResult)
	{
		QDialog::done(aResult);
		if (Dispatch.Back) {
			Dispatch.Back();
		}
	}

	void review_cycle_promote_sheet::showEvent(QShowEvent* aEvent)
	{
		place();
		QDialog::showEvent(aEvent);
	}

	void review_cycle_promote_sheet::place()
	{
		const auto* const host = parentWidget() != nullptr ? parentWidget()->window() : nullptr;
		if (host == nullptr) {
			return;
		}
		const auto area   = host->geometry();
		const auto top    = Anchor ? Anchor->y() : area.top() + 64;
		const auto room   = std::max(320, area.bottom() - top - 24);
		const auto width  = std::clamp(area.width() - 96, 520, 720);
		const auto height = std::min(room, 620);
		resize(width, height);
		const auto centre = Anchor ? Anchor->x() : area.center().x();
		move(centre - width / 2, top);
	}

	auto review_cycle_promote_sheet::title() const noexcept -> QLabel*
	{
		return Title;
	}
	auto review_cycle_promote_sheet::intro() const noexcept -> QLabel*
	{
		return Intro;
	}
	auto review_cycle_promote_sheet::notes() const noexcept -> QListWidget*
	{
		return Notes;
	}
	auto review_cycle_promote_sheet::outgoing_words() const noexcept -> QLabel*
	{
		return Outgoing;
	}
	auto review_cycle_promote_sheet::private_words() const noexcept -> QLabel*
	{
		return Private;
	}
	auto review_cycle_promote_sheet::status() const noexcept -> QLabel*
	{
		return Status;
	}
	auto review_cycle_promote_sheet::add_button() const noexcept -> QPushButton*
	{
		return Add;
	}
	auto review_cycle_promote_sheet::open_pull_request_button() const noexcept -> QPushButton*
	{
		return OpenPull;
	}
	auto review_cycle_promote_sheet::close_button() const noexcept -> QPushButton*
	{
		return Close;
	}
}
