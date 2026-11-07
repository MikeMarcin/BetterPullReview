#include "bpr/qt/commit_filter_view.hpp"
#include "bpr/qt/generated_files_view.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/scoped_prose_editor.hpp"
#include "conversation_glyphs.hpp"
#include "overview_summary_splitter.hpp"
#include "painted_glyph_icon.hpp"
#include "pull_request_overview_support.hpp"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QAction>
#include <QBoxLayout>
#include <QChar>
#include <QColor>
#include <QEvent>
#include <QFlags>
#include <QFont>
#include <QFrame>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLatin1Char>
#include <QLatin1StringView>
#include <QLayout>
#include <QLayoutItem>
#include <QList>
#include <QListSpecialMethods>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QModelIndex>
#include <QObject>
#include <QPaintDevice>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointF>
#include <QPushButton>
#include <QRectF>
#include <QSize>
#include <QSizePolicy>
#include <QSplitter>
#include <QString>
#include <QStringDecoder>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		/// [[BPR-1089]]. The Label key: every mark with its name,
		/// and what the other numbers and the round marks mean. A list, so the
		/// keyboard and a screen reader can walk it; Escape closes it.
		class overview_label_key final : public QFrame
		{
		public:
			explicit overview_label_key(QWidget* aParent)
				: QFrame{ aParent, Qt::Popup }
			{
				setObjectName(QStringLiteral("overviewLabelKeyPopup"));
				// Its own window: it shows the theme the page shows, not the
				// application's palette.
				setAttribute(Qt::WA_WindowPropagation);
				setAccessibleName(QStringLiteral("Label key"));
				setFrameShape(QFrame::StyledPanel);
				auto* const layout = new QVBoxLayout{ this };
				layout->setContentsMargins(4, 4, 4, 4);
				List = new QListWidget{ this };
				List->setObjectName(QStringLiteral("overviewLabelKeyList"));
				List->setAccessibleName(QStringLiteral("Label key"));
				List->setFrameShape(QFrame::NoFrame);
				List->setIconSize(QSize{ 16, 16 });
				List->setSelectionMode(QAbstractItemView::SingleSelection);
				layout->addWidget(List);
			}
			QListWidget* List = nullptr;

		protected:
			void keyPressEvent(QKeyEvent* aEvent) override
			{
				if (aEvent->key() == Qt::Key_Escape) {
					close();
					return;
				}
				QFrame::keyPressEvent(aEvent);
			}
		};
	}

	void pull_request_overview_page::activate_chip(const QModelIndex& aCell, const overview_comment_chip& aChip)
	{
		const auto row = Model->feedback_row(aCell);
		if (!row) {
			return;
		}
		switch (aChip.Kind) {
		case overview_comment_chip::kind::label:
			drill(row->first, row->second, { .Kind = core::feedback_count_kind::dedicated, .Label = aChip.Label });
			break;
		case overview_comment_chip::kind::other:
			drill(row->first, row->second, { .Kind = core::feedback_count_kind::catchall });
			break;
		case overview_comment_chip::kind::uncertain:
			if (aChip.Count != 0U) {
				drill(row->first, row->second, { .Kind = core::feedback_count_kind::uncertain });
			}
			break;
		case overview_comment_chip::kind::more:
			open_comments_menu(aCell, Files->viewport()->mapToGlobal(aChip.Box.bottomLeft().toPoint()));
			break;
		}
	}

	void pull_request_overview_page::show_label_key()
	{
		auto* popup = static_cast<overview_label_key*>(LabelKeyPopup);
		if (popup == nullptr) {
			popup         = new overview_label_key{ this };
			LabelKeyPopup = popup;
		}
		auto* const list = popup->List;
		list->clear();
		const auto ratio   = devicePixelRatioF();
		const auto heading = [list](const QString& aText) {
			auto* const item = new QListWidgetItem{ aText, list };
			auto        font = item->font();
			font.setWeight(QFont::DemiBold);
			item->setFont(font);
			item->setFlags(Qt::ItemIsEnabled);
		};
		const auto entry = [list](const QIcon& aIcon, const QString& aText, const QString& aAccessible, const QString& aTip) {
			auto* const item = new QListWidgetItem{ aIcon, aText, list };
			item->setData(Qt::AccessibleTextRole, aAccessible);
			item->setToolTip(aTip);
		};
		heading(QStringLiteral("Labels (the first comment of a thread)"));
		for (std::size_t index = 0U; index < core::feedback_label_count; ++index) {
			const auto label = static_cast<core::feedback_label>(index);
			entry(QIcon{ feedback_mark_pixmap(label, { }, feedback_mark_colours_for(Colours.Marks, label, palette()), ratio) }, feedback_label_text(label),
				  QStringLiteral("%1: %2").arg(feedback_label_text(label), feedback_label_description(label)), feedback_label_description(label));
		}
		heading(QStringLiteral("Other numbers"));
		const auto ink    = palette().color(QPalette::Text);
		const auto faded  = palette().color(QPalette::Disabled, QPalette::Text);
		const auto bubble = [ink, faded](const bool aOther) {
			return painted_glyph_icon([aOther](QPainter& aPainter, const qreal aSide, const QColor& aInk) { paint_bubble(aPainter, QRectF{ 0.0, 0.0, aSide, aSide }, aInk, aOther); }, ink, faded);
		};
		entry(bubble(true), QStringLiteral("Other labels"), QStringLiteral("Other labels: labels not ticked in Settings' Overview chips, and comments with no label"),
			  QStringLiteral("Labels not ticked in Settings' Overview chips, and comments with no label"));
		entry(bubble(false), QStringLiteral("Comments: all, when no label is ticked"), QStringLiteral("Comments: every thread, when no label is ticked in Settings"),
			  QStringLiteral("Every thread, when no label is ticked in Settings' Overview chips"));
		auto state  = Colours.States;
		state.Faint = feedback_state_faint(palette());
		for (const auto resolution : { core::feedback_resolution::unresolved, core::feedback_resolution::resolved, core::feedback_resolution::unknown, core::feedback_resolution::not_applicable }) {
			auto words = feedback_state_text(resolution);
			words[0]   = words[0].toUpper();
			entry(QIcon{ feedback_state_mark_pixmap(resolution, state, ratio) }, words, words, words);
		}
		entry(QIcon{ feedback_state_mark_pixmap(core::feedback_resolution::unknown, state, ratio) }, QStringLiteral("? beside a count: some not counted"),
			  QStringLiteral("Question mark beside a count: some threads are not counted, such as those whose state is unknown"),
			  QStringLiteral("Its tooltip says why. Click it or press Enter on it to list them"));
		// The keys, where someone looking up the marks will see them.
		heading(QStringLiteral("Keys"));
		// No mark of their own, but indented as the entries above are.
		auto blank = QPixmap{ QSize{ 16, 16 } * ratio };
		blank.setDevicePixelRatio(ratio);
		blank.fill(Qt::transparent);
		const auto keys = [&](const QString& aText) { entry(QIcon{ blank }, aText, aText, aText); };
		keys(QStringLiteral("Tab: from a row onto its chips, then along them"));
		keys(QStringLiteral("Left and Right: along the chips, once on one"));
		keys(QStringLiteral("Enter or Space: list a chip's threads"));
		keys(QStringLiteral("Esc: back to the row"));
		list->setCurrentRow(1);
		const auto rows = list->count();
		list->setMinimumSize(QSize{ list->sizeHintForColumn(0) + 24, std::min(rows, 32) * list->sizeHintForRow(1) + 8 });
		popup->adjustSize();
		const auto anchor = LabelKey->mapToGlobal(QPoint{ LabelKey->width() - popup->width(), LabelKey->height() + 2 });
		popup->move(anchor);
		popup->show();
		list->setFocus(Qt::PopupFocusReason);
	}

	void pull_request_overview_page::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) {
			refresh_folder_icons();
			refresh_feedback_icons();
			if (FullView) {
				FullView->setIcon(full_view_icon(palette(), Body->expanded()));
			}
			if (DescriptionEdit) {
				DescriptionEdit->setIcon(conversation_icon(conversation_glyph::edit, palette()));
			}
		}
	}

	auto pull_request_overview_page::label_key() const noexcept -> QWidget*
	{
		return LabelKeyPopup;
	}

	auto pull_request_overview_page::open_counts_button() const noexcept -> QToolButton*
	{
		return OpenCounts;
	}

	auto pull_request_overview_page::all_counts_button() const noexcept -> QToolButton*
	{
		return AllCounts;
	}

	auto pull_request_overview_page::uncertain_button() const noexcept -> QToolButton*
	{
		return Uncertain;
	}

	auto pull_request_overview_page::label_key_button() const noexcept -> QToolButton*
	{
		return LabelKey;
	}

	auto pull_request_overview_page::count_mark(const core::feedback_count_target& aTarget, const bool aAnyColumns) const -> QPixmap
	{
		const auto ratio = devicePixelRatioF();
		switch (aTarget.Kind) {
		case core::feedback_count_kind::dedicated:
			if (aTarget.Label) {
				return feedback_mark_pixmap(*aTarget.Label, { }, feedback_mark_colours_for(Colours.Marks, *aTarget.Label, palette()), ratio);
			}
			break;
		case core::feedback_count_kind::uncertain: {
			auto state  = Colours.States;
			state.Faint = feedback_state_faint(palette());
			return feedback_state_mark_pixmap(core::feedback_resolution::unknown, state, ratio);
		}
		case core::feedback_count_kind::catchall:
		case core::feedback_count_kind::included:
			break;
		}
		const auto other = aTarget.Kind == core::feedback_count_kind::catchall && aAnyColumns;
		return painted_glyph_icon([other](QPainter& aPainter, const qreal aSide, const QColor& aInk) { paint_bubble(aPainter, QRectF{ 0.0, 0.0, aSide, aSide }, aInk, other); },
								  palette().color(QPalette::Text), palette().color(QPalette::Disabled, QPalette::Text))
			.pixmap(QSize{ 16, 16 }, ratio);
	}

	void pull_request_overview_page::set_open_on_github_action(QAction* aAction)
	{
		StatusDrawer->set_tools(aAction);
	}

	void pull_request_overview_page::present_commit_filter(const application::commit_filter_projection& aProjection)
	{
		StatusDrawer->commits()->present(aProjection);
		CommitChip->present(aProjection);
		GeneratedRow->present(aProjection);
		FilterProjection = aProjection;
	}

	void pull_request_overview_page::set_file_filter_dispatch(std::function<void(const std::vector<std::string>&)> aDispatch)
	{
		StatusDrawer->commits()->set_file_filter_dispatch(std::move(aDispatch));
	}

	void pull_request_overview_page::set_generated_dispatch(std::function<void(const application::generated_file_gesture&)> aDispatch)
	{
		GeneratedDispatch = std::move(aDispatch);
	}

	auto pull_request_overview_page::generated_row() const noexcept -> generated_files_row*
	{
		return GeneratedRow;
	}

	auto pull_request_overview_page::make_file_menu(const QModelIndex& aIndex) -> QMenu*
	{
		auto*      menu   = make_comments_menu(aIndex);
		const auto file   = aIndex.siblingAtColumn(overview_file_model::file_column);
		const auto path   = file.data(overview_file_model::path_role).toString();
		const auto folder = file.data(overview_file_model::folder_role).toBool();
		if (folder || path.isEmpty() || !FilterProjection || !Projection) {
			return menu;
		}
		if (menu == nullptr) {
			menu = new QMenu{ this };
			menu->setAttribute(Qt::WA_WindowPropagation);
			menu->setObjectName(QStringLiteral("overviewFileMenu"));
		}
		const auto chosen = path.toStdString();
		add_generated_actions(*menu, chosen, generated_gestures_for(*FilterProjection, chosen), GeneratedDispatch);
		return menu;
	}

	void pull_request_overview_page::set_commit_filter_dispatch(std::function<void(const application::commit_filter_gesture&)> aDispatch)
	{
		CommitFilterDispatch = aDispatch;
		StatusDrawer->commits()->set_gesture_dispatch(std::move(aDispatch));
	}

	void pull_request_overview_page::set_commit_filter_shown_dispatch(std::function<void()> aDispatch)
	{
		StatusDrawer->commits()->set_shown_dispatch(std::move(aDispatch));
	}

	void pull_request_overview_page::set_commit_open_dispatch(std::function<void(const std::string&)> aDispatch)
	{
		StatusDrawer->commits()->set_open_dispatch(std::move(aDispatch));
	}

	auto pull_request_overview_page::commit_chip() const noexcept -> commit_filter_chip*
	{
		return CommitChip;
	}

	auto pull_request_overview_page::status_drawer() const noexcept -> overview_status_drawer*
	{
		return StatusDrawer;
	}

	auto pull_request_overview_page::status_splitter() const noexcept -> QSplitter*
	{
		return StatusSplit;
	}

	auto pull_request_overview_page::entry_path() const -> std::optional<QString>
	{
		if (!Projection || !Projection->EntryPath) {
			return std::nullopt;
		}
		return text(*Projection->EntryPath);
	}

	void pull_request_overview_page::set_file_dispatch(overview_path_dispatch aDispatch)
	{
		FileDispatch = std::move(aDispatch);
	}

	void pull_request_overview_page::present_description_controls()
	{
		if (DescriptionCorner == nullptr) {
			return;
		}
		const auto hasBody  = !Body->Reader->original_body().empty();
		const auto expanded = Body->expanded();
		const auto offered  = DescriptionEditable && Projection.has_value() && !Cached && !DescriptionEditing;
		DescriptionEdit->setVisible(offered && hasBody);
		DescriptionCorner->setVisible(!FullView->isHidden() || !DescriptionEdit->isHidden());
		DescriptionCorner->adjustSize();
		DescriptionAdd->setVisible(offered && !hasBody && !expanded);
		DescriptionPanel->setVisible(DescriptionEditing);
		if (DescriptionEditing) {
			Body->setVisible(false);
			EmptyBody->setVisible(false);
			SummaryPane->setVisible(true);
		}
		SummarySplit->queue_layout();
	}

	void pull_request_overview_page::set_description_editable(const bool aEditable)
	{
		if (DescriptionEditable == aEditable) {
			return;
		}
		DescriptionEditable = aEditable;
		if (!aEditable && DescriptionEditing) {
			close_description_editor();
		}
		present_description_controls();
	}

	auto pull_request_overview_page::description_editable() const noexcept -> bool
	{
		return DescriptionEditable;
	}

	void pull_request_overview_page::set_description_save_dispatch(std::function<void(const overview_description_save&)> aDispatch)
	{
		DescriptionSaveDispatch = std::move(aDispatch);
	}

	auto pull_request_overview_page::begin_description_edit() -> bool
	{
		if (!DescriptionEditable || !Projection.has_value() || Cached) {
			return false;
		}
		if (!DescriptionEditing) {
			if (Body->expanded()) {
				set_description_expanded(false);
			}
			DescriptionEditing  = true;
			DescriptionSaving   = false;
			DescriptionOriginal = Projection->Body;
			DescriptionNewerText.reset();
			DescriptionText->setReadOnly(false);
			DescriptionText->present(DescriptionOriginal, true);
			DescriptionNewer->hide();
			present_description_status({ });
			present_description_controls();
		}
		DescriptionText->setFocus(Qt::OtherFocusReason);
		return true;
	}

	auto pull_request_overview_page::description_editing() const noexcept -> bool
	{
		return DescriptionEditing;
	}

	void pull_request_overview_page::present_description_status(const QString& aText, const bool aSaving)
	{
		DescriptionSaving = aSaving && DescriptionEditing;
		DescriptionStatus->setText(aText);
		DescriptionStatus->setAccessibleName(aText);
		DescriptionText->setReadOnly(DescriptionSaving);
		DescriptionSave->setEnabled(!DescriptionSaving);
		DescriptionCancel->setEnabled(!DescriptionSaving);
		DescriptionNewer->setEnabled(!DescriptionSaving);
		if (!aText.isEmpty() && DescriptionEditing) {
			QAccessibleAnnouncementEvent event{ DescriptionStatus, aText };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
	}

	void pull_request_overview_page::offer_newer_description(std::string aNewer)
	{
		if (!DescriptionEditing) {
			return;
		}
		DescriptionNewerText = std::move(aNewer);
		DescriptionNewer->show();
	}

	void pull_request_overview_page::close_description_editor()
	{
		const auto wasEditing = DescriptionEditing;
		DescriptionEditing    = false;
		DescriptionSaving     = false;
		DescriptionNewerText.reset();
		DescriptionNewer->hide();
		DescriptionStatus->clear();
		DescriptionText->setReadOnly(false);
		DescriptionText->present({ }, true);
		DescriptionPanel->hide();
		if (wasEditing) {
			const auto hasBody = !Body->Reader->original_body().empty();
			Body->setVisible(hasBody);
			SummaryPane->setVisible(hasBody);
			EmptyBody->setVisible(Projection.has_value() && Projection->Body.empty() && !Body->expanded());
		}
		present_description_controls();
		if (wasEditing && isVisible()) {
			auto* const next = DescriptionEdit->isVisible() ? static_cast<QWidget*>(DescriptionEdit) : DescriptionAdd->isVisible() ? static_cast<QWidget*>(DescriptionAdd)
																																   : static_cast<QWidget*>(Files);
			next->setFocus(Qt::OtherFocusReason);
		}
	}

	auto pull_request_overview_page::description_edit_button() const noexcept -> QToolButton* { return DescriptionEdit; }

	auto pull_request_overview_page::description_add_button() const noexcept -> QPushButton* { return DescriptionAdd; }

	auto pull_request_overview_page::description_editor() const noexcept -> scoped_prose_editor* { return DescriptionText; }

	auto pull_request_overview_page::description_save_button() const noexcept -> QPushButton* { return DescriptionSave; }

	auto pull_request_overview_page::description_cancel_button() const noexcept -> QPushButton* { return DescriptionCancel; }

	auto pull_request_overview_page::description_newer_button() const noexcept -> QPushButton* { return DescriptionNewer; }

	auto pull_request_overview_page::description_status() const noexcept -> QLabel* { return DescriptionStatus; }

	auto pull_request_overview_page::description_original() const noexcept -> const std::string& { return DescriptionOriginal; }

	auto pull_request_overview_page::title_label() const noexcept -> QLabel*
	{
		return Title;
	}

	auto pull_request_overview_page::summary_label() const noexcept -> QLabel*
	{
		return Summary;
	}

	auto pull_request_overview_page::body_view() const noexcept -> markdown_body_view*
	{
		return Body->Reader;
	}

	auto pull_request_overview_page::files_heading() const noexcept -> QLabel*
	{
		return FilesHeading;
	}

	auto pull_request_overview_page::files_view() const noexcept -> QTreeView*
	{
		return Files;
	}

	auto pull_request_overview_page::file_model() const noexcept -> overview_file_model*
	{
		return Model;
	}

	auto pull_request_overview_page::participant_rows() const -> std::vector<overview_participant_row*>
	{
		auto rows = std::vector<overview_participant_row*>{ };
		for (int index = 0; index < Participants->count(); ++index) {
			if (auto* const row = dynamic_cast<overview_participant_row*>(Participants->itemAt(index)->widget());
				row != nullptr) {
				rows.push_back(row);
			}
		}
		return rows;
	}

	auto pull_request_overview_page::request_rows() const -> std::vector<overview_request_row*>
	{
		auto rows = std::vector<overview_request_row*>{ };
		for (int index = 0; index < Requests->count(); ++index) {
			if (auto* const row = dynamic_cast<overview_request_row*>(Requests->itemAt(index)->widget()); row != nullptr) {
				rows.push_back(row);
			}
		}
		return rows;
	}

	void pull_request_overview_page::present_merge(const application::pull_request_overview_projection& aProjection)
	{
		present_merge_details(aProjection.MergeState, aProjection.MergeTone, aProjection.Status.OpenConditions, aProjection.RequirementDetails);
		present_request_rows();
	}

	void pull_request_overview_page::present_merge_details(const std::string& aState, const application::overview_merge_tone aTone, const std::vector<std::string>& aConditions, const std::string& aDetails)
	{
		auto tone = palette().color(QPalette::Active, QPalette::PlaceholderText);
		switch (aTone) {
		case application::overview_merge_tone::blocked:
			tone = Colours.Defect;
			break;
		case application::overview_merge_tone::caution:
			tone = Colours.Caution;
			break;
		case application::overview_merge_tone::mergeable:
			tone = Colours.Accepted;
			break;
		case application::overview_merge_tone::unknown:
			break;
		}
		auto statePalette = MergeState->palette();
		statePalette.setColor(QPalette::WindowText, tone);
		MergeState->setPalette(statePalette);
		MergeState->setText(text(aState));
		MergeState->setAccessibleName(QStringLiteral("Merge state: %1").arg(MergeState->text()));
		MergeState->show();

		// [[BPR-1589]]. Each failed check is named in Needs attention; the
		// details carry Merge's other conditions.
		auto conditions = QStringList{ };
		for (const auto& condition : aConditions) {
			conditions.push_back(text(condition));
		}
		MergeConditions->setText(conditions.join(QLatin1Char{ '\n' }));
		MergeConditions->setAccessibleName(conditions.join(QStringLiteral(". ")));
		MergeConditions->setVisible(!conditions.isEmpty());

		RequirementDetails->setText(text(aDetails));
		RequirementDetails->setAccessibleName(RequirementDetails->text());
		RequirementDisclosure->show();
	}

	auto pull_request_overview_page::since_heading() const noexcept -> QLabel*
	{
		return SinceHeading;
	}

	auto pull_request_overview_page::value_label(const overview_value aValue) const noexcept -> QLabel*
	{
		return Values[static_cast<std::size_t>(aValue)];
	}

	auto pull_request_overview_page::minimumSizeHint() const -> QSize
	{
		// A stacked widget's minimum is the largest of its pages', shown or
		// not. The comparison decides the centre's minimum; this page takes
		// what it is given and scrolls its table.
		return { 0, 0 };
	}

	void pull_request_overview_page::set_value(const overview_value aValue, const QString& aText, const bool aHighlight)
	{
		auto* const label = Values[static_cast<std::size_t>(aValue)];
		if (label == nullptr) {
			return;
		}
		label->setText(aText);
		// A count that means new activity is in the unread colour; everything
		// else steps back, as the accepted design draws the side column.
		auto colours = palette();
		colours.setColor(QPalette::WindowText, aHighlight ? Colours.Unread : palette().color(QPalette::Active, QPalette::PlaceholderText));
		label->setPalette(colours);
		label->setForegroundRole(QPalette::WindowText);
	}

	void pull_request_overview_page::rebuild_participants(
		const std::vector<application::overview_participant>& aParticipants)
	{
		while (Participants->count() != 0) {
			auto* const item = Participants->takeAt(0);
			delete item->widget();
			delete item;
		}
		for (const auto& participant : aParticipants) {
			Participants->addWidget(new overview_participant_row{ participant, Colours });
		}
		if (aParticipants.empty()) {
			auto* const none = new QLabel{ QStringLiteral("No one yet"), this };
			none->setObjectName(QStringLiteral("overviewNoParticipants"));
			none->setTextFormat(Qt::PlainText);
			none->setForegroundRole(QPalette::PlaceholderText);
			Participants->addWidget(none);
		}
	}

	overview_role_line::overview_role_line(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
		setAttribute(Qt::WA_Hover, true);
	}
}
