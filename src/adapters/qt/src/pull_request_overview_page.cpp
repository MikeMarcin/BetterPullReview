#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/commit_filter_view.hpp"
#include "bpr/qt/disclosure_button.hpp"
#include "bpr/qt/draft_state_chip.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/generated_files_view.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pending_veil.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/scoped_prose_editor.hpp"
#include "bpr/qt/steady_columns.hpp"
#include "conversation_glyphs.hpp"
#include "overview_summary_splitter.hpp"
#include "pull_request_overview_support.hpp"

#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QBoxLayout>
#include <QCursor>
#include <QFlags>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLatin1StringView>
#include <QLayout>
#include <QMenu>
#include <QMetaObject>
#include <QModelIndex>
#include <QObject>
#include <QPainter>
#include <QPalette>
#include <QPersistentModelIndex>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QRectF>
#include <QScrollArea>
#include <QShortcut>
#include <QSize>
#include <QSizePolicy>
#include <QSplitter>
#include <QString>
#include <QStringDecoder>
#include <QStyleOption>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QTreeView>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
		class check_result_delegate final : public QStyledItemDelegate
		{
		public:
			explicit check_result_delegate(QTreeWidget* aView)
				: QStyledItemDelegate{ aView }
				, View{ aView }
			{ }
			auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize override
			{
				const auto available = std::max(1, View->viewport()->width() - 8);
				const auto bounds    = aOption.fontMetrics.boundingRect(QRect{ 0, 0, available, 10000 }, Qt::TextWordWrap, aIndex.data().toString());
				return { available, bounds.height() + 6 };
			}
			void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
			{
				auto option = aOption;
				option.features |= QStyleOptionViewItem::WrapText;
				option.textElideMode = Qt::ElideNone;
				QStyledItemDelegate::paint(aPainter, option, aIndex);
			}

		private:
			QTreeWidget* View;
		};

		[[nodiscard]] auto heading_font(const QWidget& aWidget) -> QFont
		{
			auto font = aWidget.font();
			font.setPointSizeF(font.pointSizeF() * 0.9);
			font.setWeight(QFont::DemiBold);
			return font;
		}

		[[nodiscard]] auto make_heading(const QString& aText, const QString& aName, QWidget* const aParent)
			-> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setObjectName(aName);
			label->setTextFormat(Qt::PlainText);
			label->setFont(heading_font(*aParent));
			label->setForegroundRole(QPalette::PlaceholderText);
			return label;
		}

		[[nodiscard]] auto value_name(const overview_value aValue) -> QString
		{
			switch (aValue) {
			case overview_value::uploads:
				return QStringLiteral("Uploads");
			case overview_value::files_changed:
				return QStringLiteral("Files changed");
			case overview_value::new_comments:
				return QStringLiteral("New comments");
			case overview_value::replies_to_me:
				return QStringLiteral("Replies to me");
			case overview_value::my_unresolved_threads:
				return QStringLiteral("My unresolved threads");
			case overview_value::checks:
				return QStringLiteral("Checks");
			case overview_value::files_read:
				return QStringLiteral("Files read");
			case overview_value::private_notes:
				return QStringLiteral("Private notes");
			case overview_value::drafted_comments:
				break;
			}
			// [[BPR-1589]]. My comments not yet published, as Compose review
			// lists them.
			return QStringLiteral("Pending comments");
		}

		[[nodiscard]] auto value_object_name(const overview_value aValue) -> QString
		{
			switch (aValue) {
			case overview_value::uploads:
				return QStringLiteral("overviewUploadsValue");
			case overview_value::files_changed:
				return QStringLiteral("overviewFilesChangedValue");
			case overview_value::new_comments:
				return QStringLiteral("overviewNewCommentsValue");
			case overview_value::replies_to_me:
				return QStringLiteral("overviewRepliesToMeValue");
			case overview_value::my_unresolved_threads:
				return QStringLiteral("overviewMyUnresolvedThreadsValue");
			case overview_value::checks:
				return QStringLiteral("overviewChecksValue");
			case overview_value::files_read:
				return QStringLiteral("overviewFilesReadValue");
			case overview_value::private_notes:
				return QStringLiteral("overviewPrivateNotesValue");
			case overview_value::drafted_comments:
				break;
			}
			return QStringLiteral("overviewDraftedCommentsValue");
		}

		void expand_new_folders(QTreeView& aView, const QModelIndex& aParent, const int aFirst, const int aLast)
		{
			auto pending = std::vector<QModelIndex>{ };
			for (auto row = aFirst; row <= aLast; ++row) {
				pending.push_back(aView.model()->index(row, 0, aParent));
			}
			while (!pending.empty()) {
				const auto index = pending.back();
				pending.pop_back();
				if (!index.data(overview_file_model::folder_role).toBool()) {
					continue;
				}
				aView.expand(index);
				for (auto row = 0; row < aView.model()->rowCount(index); ++row) {
					pending.push_back(aView.model()->index(row, 0, index));
				}
			}
		}
	}

	pull_request_overview_page::pull_request_overview_page(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("workbenchOverviewPage"));
		setAccessibleName(QStringLiteral("Overview"));
		setAccessibleDescription(QStringLiteral(
			"What changed in the pull request and who has signed off, with my "
			"review progress and the way in"));

		auto* const main = new QWidget{ this };
		main->setObjectName(QStringLiteral("overviewMainColumn"));
		auto* const mainColumn = new QVBoxLayout{ main };
		mainColumn->setContentsMargins(18, 12, 18, 12);
		mainColumn->setSpacing(4);
		Header             = new QWidget{ main };
		auto* headerLayout = new QVBoxLayout{ Header };
		headerLayout->setContentsMargins(0, 0, 0, 0);
		headerLayout->setSpacing(4);
		mainColumn->addWidget(Header);

		Title = new QLabel{ main };
		Title->setObjectName(QStringLiteral("overviewTitle"));
		Title->setAccessibleName(QStringLiteral("Pull request title"));
		Title->setTextFormat(Qt::PlainText);
		Title->setWordWrap(true);
		auto titleFont = Title->font();
		titleFont.setPointSizeF(titleFont.pointSizeF() * 1.3);
		titleFont.setWeight(QFont::DemiBold);
		Title->setFont(titleFont);
		// [[BPR-1852]]. A draft says so before its title, as GitHub's page
		// does: the chip leads the title's first line. For its author, Ready
		// for review ends that line: the state and the way out of it read as
		// one sentence, and nothing is added between the title and the
		// description (owner, v001: the banner was "Ugly UX").
		auto* const titleRow = new QHBoxLayout;
		titleRow->setContentsMargins(0, 0, 0, 0);
		titleRow->setSpacing(8);
		DraftChip = new draft_state_chip{ main };
		DraftChip->setObjectName(QStringLiteral("overviewDraftChip"));
		DraftChip->hide();
		titleRow->addWidget(DraftChip, 0, Qt::AlignTop);
		titleRow->addWidget(Title, 1);
		ReadyForReview = new QPushButton{ QStringLiteral("Ready for review"), main };
		ReadyForReview->setObjectName(QStringLiteral("overviewReadyForReview"));
		ReadyForReview->setAccessibleName(QStringLiteral("Ready for review"));
		ReadyForReview->setAccessibleDescription(QStringLiteral("Marks this draft ready for review on GitHub, so reviewers are asked for their review"));
		ReadyForReview->setToolTip(QStringLiteral("Mark this pull request ready for review on GitHub"));
		ReadyForReview->hide();
		titleRow->addWidget(ReadyForReview, 0, Qt::AlignTop);
		headerLayout->addLayout(titleRow);

		Summary = new QLabel{ main };
		Summary->setObjectName(QStringLiteral("overviewSummary"));
		Summary->setAccessibleName(QStringLiteral("Pull request author and branches"));
		Summary->setTextFormat(Qt::PlainText);
		Summary->setWordWrap(true);
		Summary->setForegroundRole(QPalette::PlaceholderText);
		headerLayout->addWidget(Summary);
		// What GitHub said when it refused, under the summary.
		ReadyStatus = new QLabel{ main };
		ReadyStatus->setObjectName(QStringLiteral("overviewReadyForReviewStatus"));
		ReadyStatus->setTextFormat(Qt::PlainText);
		ReadyStatus->setWordWrap(true);
		ReadyStatus->hide();
		headerLayout->addWidget(ReadyStatus);
		connect(ReadyForReview, &QPushButton::clicked, this, [this] {
			if (DraftStateOffer == core::draft_state_change::ready_for_review && !ReadyBusy && DraftStateDispatch) {
				DraftStateDispatch(core::draft_state_change::ready_for_review);
			}
		});
		// [[BPR-0671]]. The commit oids behind the branches, disclosed. They
		// were in the files drawer's summary box, which the overview replaces;
		// they belong beside the branches they identify.
		TechnicalRow = new QHBoxLayout;
		TechnicalRow->setContentsMargins(0, 0, 0, 0);
		TechnicalRow->setSpacing(6);
		headerLayout->addLayout(TechnicalRow);

		SummarySplit = new overview_summary_splitter{ main };
		SummarySplit->setObjectName(QStringLiteral("overviewSummarySplitter"));
		mainColumn->addWidget(SummarySplit, 1);
		SummaryPane = new QWidget{ SummarySplit };
		SummaryPane->setObjectName(QStringLiteral("overviewSummaryPane"));
		auto* summaryColumn = new QVBoxLayout{ SummaryPane };
		summaryColumn->setContentsMargins(0, 0, 0, 0);
		summaryColumn->setSpacing(0);
		Body = new overview_description_view{ SummaryPane };
		Body->setObjectName(QStringLiteral("overviewBody"));
		Body->setAccessibleName(QStringLiteral("Pull request description"));
		Body->Reader->setAccessibleName(QStringLiteral("Pull request description"));
		summaryColumn->addWidget(Body, 1);
		// [[BPR-1774]]. "No description", and beside it, for the author,
		// Add a description.
		auto* const emptyRow    = new QWidget{ main };
		auto* const emptyLayout = new QHBoxLayout{ emptyRow };
		emptyLayout->setContentsMargins(0, 0, 0, 0);
		emptyLayout->setSpacing(8);
		EmptyBody = new QLabel{ QStringLiteral("No description"), emptyRow };
		EmptyBody->setObjectName(QStringLiteral("overviewNoDescription"));
		emptyLayout->addWidget(EmptyBody);
		DescriptionAdd = new quiet_push_button{ QStringLiteral("Add a description"), emptyRow };
		DescriptionAdd->setObjectName(QStringLiteral("overviewAddDescription"));
		DescriptionAdd->setAccessibleDescription(QStringLiteral("Write this pull request's description. Nothing is sent until you save."));
		DescriptionAdd->hide();
		emptyLayout->addWidget(DescriptionAdd);
		emptyLayout->addStretch(1);
		mainColumn->insertWidget(mainColumn->indexOf(SummarySplit), emptyRow);
		// [[BPR-1774]]. The reader's corner holds Edit, for the author, beside
		// Full view: both act on the description they sit on.
		DescriptionCorner = new QWidget{ Body };
		DescriptionCorner->setObjectName(QStringLiteral("overviewDescriptionCorner"));
		auto* const cornerLayout = new QHBoxLayout{ DescriptionCorner };
		cornerLayout->setContentsMargins(0, 0, 0, 0);
		cornerLayout->setSpacing(0);
		DescriptionEdit = new QToolButton{ DescriptionCorner };
		DescriptionEdit->setObjectName(QStringLiteral("overviewEditDescription"));
		DescriptionEdit->setAutoRaise(true);
		DescriptionEdit->setFocusPolicy(Qt::StrongFocus);
		DescriptionEdit->setText(QStringLiteral("Edit description"));
		DescriptionEdit->setAccessibleName(DescriptionEdit->text());
		DescriptionEdit->setAccessibleDescription(QStringLiteral("Edit this pull request's description in Markdown. Nothing is sent until you save."));
		DescriptionEdit->setToolTip(DescriptionEdit->text());
		DescriptionEdit->setIcon(conversation_icon(conversation_glyph::edit, palette()));
		DescriptionEdit->setBackgroundRole(QPalette::Base);
		DescriptionEdit->setAutoFillBackground(true);
		DescriptionEdit->hide();
		cornerLayout->addWidget(DescriptionEdit);
		FullView = new QToolButton{ DescriptionCorner };
		cornerLayout->addWidget(FullView);
		FullView->setObjectName(QStringLiteral("overviewFullView"));
		// Not checkable: Exit keeps Enter's quiet auto-raised style and only the
		// glyph, name and tooltip change (owner, overview-v003 rejection).
		FullView->setAutoRaise(true);
		FullView->setFocusPolicy(Qt::StrongFocus);
		FullView->setText(QStringLiteral("Enter full view"));
		FullView->setAccessibleName(FullView->text());
		FullView->setToolTip(FullView->text());
		FullView->setIcon(full_view_icon(palette(), false));
		// The reader's own paper behind the glyph, so text scrolling under the
		// corner never shows through it.
		FullView->setBackgroundRole(QPalette::Base);
		FullView->setAutoFillBackground(true);
		Body->set_corner(DescriptionCorner);
		connect(FullView, &QToolButton::clicked, this, [this] { set_description_expanded(!Body->expanded()); });
		connect(DescriptionEdit, &QToolButton::clicked, this, [this] { static_cast<void>(begin_description_edit()); });
		connect(DescriptionAdd, &QPushButton::clicked, this, [this] { static_cast<void>(begin_description_edit()); });
		// [[BPR-1774]]. The editor takes the reader's place in the summary
		// pane: the description is edited where it is read. The app's Markdown
		// prose editor keeps the body's untouched bytes (CRLF included), so a
		// save changes only what was typed.
		DescriptionPanel = new QWidget{ SummaryPane };
		DescriptionPanel->setObjectName(QStringLiteral("overviewDescriptionEditor"));
		DescriptionPanel->setAccessibleName(QStringLiteral("Edit description"));
		auto* const panelColumn = new QVBoxLayout{ DescriptionPanel };
		panelColumn->setContentsMargins(0, 2, 0, 2);
		panelColumn->setSpacing(6);
		auto* const hint = new QLabel{ QStringLiteral("Markdown. Nothing is sent to GitHub until you save."), DescriptionPanel };
		hint->setObjectName(QStringLiteral("overviewDescriptionHint"));
		hint->setForegroundRole(QPalette::PlaceholderText);
		hint->setTextFormat(Qt::PlainText);
		panelColumn->addWidget(hint);
		DescriptionText = new scoped_prose_editor{ DescriptionPanel };
		DescriptionText->setObjectName(QStringLiteral("overviewDescriptionText"));
		DescriptionText->setAccessibleName(QStringLiteral("Description Markdown source"));
		DescriptionText->setPlaceholderText(QStringLiteral("Describe what this pull request changes and why."));
		DescriptionText->setTabChangesFocus(true);
		DescriptionText->setMinimumHeight(6 * DescriptionText->fontMetrics().lineSpacing());
		panelColumn->addWidget(DescriptionText, 1);
		auto* const foot = new QHBoxLayout{ };
		foot->setSpacing(8);
		DescriptionStatus = new QLabel{ DescriptionPanel };
		DescriptionStatus->setObjectName(QStringLiteral("overviewDescriptionStatus"));
		DescriptionStatus->setTextFormat(Qt::PlainText);
		DescriptionStatus->setWordWrap(true);
		DescriptionStatus->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		foot->addWidget(DescriptionStatus, 1);
		DescriptionNewer = new quiet_push_button{ QStringLiteral("Edit GitHub's newer text"), DescriptionPanel };
		DescriptionNewer->setObjectName(QStringLiteral("overviewDescriptionNewer"));
		DescriptionNewer->setAccessibleDescription(QStringLiteral("Replaces your text with the description GitHub has now, so you can edit from it."));
		DescriptionNewer->hide();
		foot->addWidget(DescriptionNewer);
		DescriptionCancel = new quiet_push_button{ QStringLiteral("Cancel"), DescriptionPanel };
		DescriptionCancel->setObjectName(QStringLiteral("overviewDescriptionCancel"));
		DescriptionCancel->setAccessibleDescription(QStringLiteral("Stops editing. Nothing is sent."));
		foot->addWidget(DescriptionCancel);
		DescriptionSave = new QPushButton{ QStringLiteral("Save"), DescriptionPanel };
		DescriptionSave->setObjectName(QStringLiteral("overviewDescriptionSave"));
		DescriptionSave->setAccessibleDescription(QStringLiteral("Saves the description to GitHub."));
		DescriptionSave->setDefault(true);
		DescriptionSave->setToolTip(QStringLiteral("Save (%1)").arg(shortcut_key(shortcut_command::description_save).toString(QKeySequence::NativeText)));
		foot->addWidget(DescriptionSave);
		panelColumn->addLayout(foot);
		summaryColumn->addWidget(DescriptionPanel, 1);
		DescriptionPanel->hide();
		const auto save = [this] {
			if (!DescriptionEditing || DescriptionSaving || !DescriptionSaveDispatch) {
				return;
			}
			DescriptionSaveDispatch(overview_description_save{ .Original = DescriptionOriginal, .Body = DescriptionText->raw() });
		};
		connect(DescriptionSave, &QPushButton::clicked, this, save);
		auto* const saveKey = new QShortcut{ shortcut_key(shortcut_command::description_save), DescriptionPanel };
		saveKey->setObjectName(QStringLiteral("overviewDescriptionSaveKey"));
		saveKey->setContext(Qt::WidgetWithChildrenShortcut);
		connect(saveKey, &QShortcut::activated, this, save);
		connect(DescriptionCancel, &QPushButton::clicked, this, [this] { close_description_editor(); });
		auto* const cancelKey = new QShortcut{ shortcut_key(shortcut_command::description_cancel), DescriptionPanel };
		cancelKey->setObjectName(QStringLiteral("overviewDescriptionCancelKey"));
		cancelKey->setContext(Qt::WidgetWithChildrenShortcut);
		connect(cancelKey, &QShortcut::activated, this, [this] {
			if (!DescriptionSaving) {
				close_description_editor();
			}
		});
		connect(DescriptionNewer, &QPushButton::clicked, this, [this] {
			if (!DescriptionNewerText.has_value()) {
				return;
			}
			DescriptionOriginal = std::move(*DescriptionNewerText);
			DescriptionNewerText.reset();
			DescriptionText->present(DescriptionOriginal, true);
			DescriptionNewer->hide();
			present_description_status(QStringLiteral("Editing GitHub's newer text."));
			DescriptionText->setFocus(Qt::OtherFocusReason);
		});
		Body->LayoutChanged = [this] { SummarySplit->queue_layout(); };
		FilesPane           = new QWidget{ SummarySplit };
		FilesPane->setObjectName(QStringLiteral("overviewFilesPane"));
		auto* filesColumn = new QVBoxLayout{ FilesPane };
		filesColumn->setContentsMargins(0, 0, 0, 0);
		filesColumn->setSpacing(4);
		SummarySplit->addWidget(SummaryPane);
		SummarySplit->addWidget(FilesPane);
		SummarySplit->LineHeight    = [this] { return Body->line_height(); };
		SummarySplit->NaturalHeight = [this] {
			// [[BPR-1774]]. The editor asks for room to write in.
			if (DescriptionEditing) {
				return std::max(DescriptionPanel->sizeHint().height(), 16 * DescriptionText->fontMetrics().lineSpacing());
			}
			return std::max(Body->natural_height(), FullView->sizeHint().height() + Body->chrome_height());
		};
		SummarySplit->SummaryMinimum = [this] {
			if (DescriptionEditing) {
				return DescriptionPanel->minimumSizeHint().height();
			}
			return std::max(3 * Body->line_height(), FullView->sizeHint().height()) + Body->chrome_height();
		};
		SummarySplit->FilesMinimum = [this] { return FilesHeading->sizeHint().height() + 4 + Files->header()->sizeHint().height() + 3 * std::max(Files->fontMetrics().height() + 8, Files->sizeHintForRow(0)); };
		auto* escape               = new QShortcut{ shortcut_key(shortcut_command::fold_description), this };
		escape->setContext(Qt::WidgetWithChildrenShortcut);
		escape->setEnabled(false);
		connect(escape, &QShortcut::activated, this, [this] { set_description_expanded(false); });
		escape->setObjectName(QStringLiteral("overviewDescriptionEscape"));

		{
			auto* const heading = new eliding_heading{ main };
			heading->setObjectName(QStringLiteral("overviewFilesHeading"));
			heading->setTextFormat(Qt::PlainText);
			heading->setFont(heading_font(*main));
			heading->setForegroundRole(QPalette::PlaceholderText);
			heading->present(QStringLiteral("Files"), { });
			FilesHeading = heading;
		}

		// [[BPR-1269]]. Collapse all and expand all, as glyphs beside the heading
		// with their names in tooltips; local to the page, like one folder's.
		auto* const headingRow = new QHBoxLayout{ };
		headingRow->setContentsMargins(0, 0, 0, 0);
		headingRow->setSpacing(2);
		headingRow->addWidget(FilesHeading);
		headingRow->addStretch(1);
		const auto folderButton = [main](const QString& aName, const QString& aObjectName) {
			auto* const button = new QToolButton{ main };
			button->setObjectName(aObjectName);
			button->setAccessibleName(aName);
			button->setToolTip(aName);
			button->setAutoRaise(true);
			button->setFocusPolicy(Qt::StrongFocus);
			button->setIconSize(QSize{ 16, 16 });
			button->hide();
			return button;
		};
		CollapseFolders = folderButton(QStringLiteral("Collapse all folders"), QStringLiteral("overviewCollapseFolders"));
		ExpandFolders   = folderButton(QStringLiteral("Expand all folders"), QStringLiteral("overviewExpandFolders"));
		connect(CollapseFolders, &QToolButton::clicked, this, [this] { collapse_all_folders(); });
		connect(ExpandFolders, &QToolButton::clicked, this, [this] { expand_all_folders(); });
		// [[BPR-1089]]. Open or All, the ? chip and the Label key (owner,
		// BPR-1079 Overview states), with their names in tooltips.
		// [[BPR-1577]]. Each half is drawn by the page: the chosen one a quiet
		// chip, the other its word alone.
		const auto countsButton = [main](const QString& aText, const QString& aName, const QString& aAccessible, const QString& aTip) {
			auto* const button = new counts_mode_button{ main };
			button->setObjectName(aName);
			button->setText(aText);
			button->setAccessibleName(aAccessible);
			button->setToolTip(aTip);
			button->setCheckable(true);
			button->setAutoExclusive(true);
			button->setAutoRaise(true);
			button->setFocusPolicy(Qt::StrongFocus);
			button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			button->setIconSize(QSize{ 16, 16 });
			button->hide();
			anchor_tooltips(*button);
			return button;
		};
		OpenCounts = countsButton(QStringLiteral("Open"), QStringLiteral("overviewModeOpen"), QStringLiteral("Open threads"), QStringLiteral("Unresolved threads (default)"));
		AllCounts  = countsButton(QStringLiteral("All"), QStringLiteral("overviewModeAll"), QStringLiteral("All threads"), QStringLiteral("Every thread: open, resolved, unknown"));
		AllCounts->setToolButtonStyle(Qt::ToolButtonTextOnly);
		connect(OpenCounts, &QToolButton::toggled, this, [this](const bool aOn) {
			if (aOn) {
				choose_mode(core::feedback_resolution_mode::unresolved);
			}
		});
		connect(AllCounts, &QToolButton::toggled, this, [this](const bool aOn) {
			if (aOn) {
				choose_mode(core::feedback_resolution_mode::all);
			}
		});
		Uncertain = new QToolButton{ main };
		Uncertain->setObjectName(QStringLiteral("overviewUncertain"));
		Uncertain->setAutoRaise(true);
		Uncertain->setFocusPolicy(Qt::StrongFocus);
		Uncertain->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		Uncertain->setIconSize(QSize{ 16, 16 });
		Uncertain->hide();
		anchor_tooltips(*Uncertain);
		connect(Uncertain, &QToolButton::clicked, this, [this] { drill(application::overview_feedback_row_kind::pull_request, { }, { .Kind = core::feedback_count_kind::uncertain }); });
		LabelKey = folderButton(QStringLiteral("Label key"), QStringLiteral("overviewLabelKey"));
		LabelKey->setToolTip(QStringLiteral("Label key (%1)").arg(shortcut_key(shortcut_command::label_key).toString(QKeySequence::NativeText)));
		LabelKey->setAccessibleDescription(QStringLiteral("Lists every label mark with its name, and the state marks"));
		anchor_tooltips(*LabelKey);
		connect(LabelKey, &QToolButton::clicked, this, [this] { show_label_key(); });
		auto* const keyShortcut = new QShortcut{ shortcut_key(shortcut_command::label_key), this };
		keyShortcut->setContext(Qt::WidgetWithChildrenShortcut);
		keyShortcut->setObjectName(QStringLiteral("overviewLabelKeyShortcut"));
		connect(keyShortcut, &QShortcut::activated, this, [this] {
			if (Model->feedback()) {
				show_label_key();
			}
		});
		headingRow->addWidget(OpenCounts);
		headingRow->addWidget(AllCounts);
		headingRow->addSpacing(4);
		headingRow->addWidget(Uncertain);
		headingRow->addWidget(LabelKey);
		headingRow->addSpacing(6);
		headingRow->addWidget(CollapseFolders);
		headingRow->addWidget(ExpandFolders);
		filesColumn->addLayout(headingRow);
		// RFC-BPR-0089. What the commit filter shows, and Clear all filters.
		CommitChip = new commit_filter_chip{ false, main };
		CommitChip->set_clear_dispatch([this] {
			if (CommitFilterDispatch) {
				CommitFilterDispatch(application::commit_filter_gesture{ .Kind = core::commit_filter_operation_kind::clear, .Commits = { } });
			}
		});
		filesColumn->addWidget(CommitChip);
		refresh_folder_icons();

		Model = new overview_file_model{ this };
		Files = new QTreeView{ main };
		Files->setObjectName(QStringLiteral("overviewFiles"));
		Files->setAccessibleName(QStringLiteral("Files table"));
		const auto keysHelp = QStringLiteral(
			"Every file with its operation, line counts, whether it changed "
			"since my last review, my read state, comment counts and my note; "
			"activate a file to read it; click a folder, or use Left and Right, to hide and show its files; "
			"%1; "
			"Tab steps from a row onto its comment chips, Enter or Space lists a chip's threads and Escape returns to the row; "
			"the context-menu key lists every chip of the row");
		Files->setAccessibleDescription(keysHelp.arg(files_ends_help()));
		Files->setModel(Model);
		RowDelegate = new overview_row_delegate{ Colours, Files };
		Files->setItemDelegate(RowDelegate);
		// [[BPR-1213]]. Flat rows: no indentation, so every file aligns at the
		// same left column; the folder cell draws its own disclosure.
		Files->setRootIsDecorated(false);
		Files->setIndentation(0);
		Files->setItemsExpandable(true);
		Files->setExpandsOnDoubleClick(false);
		Files->setTreePosition(overview_file_model::file_column);
		Files->setUniformRowHeights(true);
		Files->setAlternatingRowColors(false);
		Files->setSelectionBehavior(QAbstractItemView::SelectRows);
		Files->setSelectionMode(QAbstractItemView::SingleSelection);
		Files->setFrameShape(QFrame::NoFrame);
		Files->setEditTriggers(QAbstractItemView::NoEditTriggers);
		// [[BPR-1363]]. Each column as wide as the widest cell of the whole
		// tree, every file whether its folder is open or not, so opening or
		// closing a folder, a scroll or a refresh moves no column; Note takes
		// the rest. Delegate measures stay bounded on a large review: only the
		// widest few cells by their text are measured.
		FileColumns = new steady_columns{ *Files, overview_file_model::note_column, [](const QModelIndex& aIndex) {
											 return aIndex.data(overview_file_model::folder_role).toBool() ? 1 : 0;
										 } };
		connect(Model, &QAbstractItemModel::rowsInserted, this,
				[this](const QModelIndex& aModelParent, const int aFirst, const int aLast) {
					expand_new_folders(*Files, aModelParent, aFirst, aLast);
				});
		connect(Model, &QAbstractItemModel::rowsInserted, this, [this] { update_folder_controls(); });
		connect(Model, &QAbstractItemModel::rowsRemoved, this, [this] { update_folder_controls(); });
		connect(Model, &QAbstractItemModel::modelReset, this, [this] { update_folder_controls(); });
		connect(Files, &QTreeView::collapsed, this, [this](const QModelIndex& aFolder) {
			for (auto parent = Files->currentIndex().parent(); parent.isValid(); parent = parent.parent()) {
				if (parent == aFolder) {
					Files->setCurrentIndex(aFolder);
					break;
				}
			}
		});
		// A click on a folder row shows or hides its files; files open on
		// activation as before. Neither dispatches anything for a folder.
		connect(Files, &QAbstractItemView::clicked, this, [this](const QModelIndex& aIndex) {
			if (aIndex.data(overview_file_model::folder_role).toBool()) {
				const auto folder = aIndex.siblingAtColumn(0);
				Files->setExpanded(folder, !Files->isExpanded(folder));
			}
		});
		connect(Files, &QAbstractItemView::activated, this, &pull_request_overview_page::open_file_at);
		// [[BPR-1089]]. A chip opens the conversations it counted: a click on
		// it, Enter or Space once Tab has stepped onto it, or the context-menu
		// key or a right-click on its row, whose menu reaches every chip,
		// Other, "+n" and ?. Another row leaves the chip.
		connect(Files->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this](const QModelIndex& aCurrent) {
			if (ChipFocus && ChipFocus->Row != aCurrent.siblingAtColumn(0)) {
				focus_chip({ }, nullptr);
			}
		});
		Files->viewport()->installEventFilter(this);
		Files->installEventFilter(this);
		Files->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(Files, &QWidget::customContextMenuRequested, this, [this](const QPoint& aPosition) {
			auto index = Files->indexAt(aPosition);
			if (!index.isValid()) {
				index = Files->currentIndex();
			}
			if (auto* const menu = make_file_menu(index); menu != nullptr) {
				menu->setAttribute(Qt::WA_DeleteOnClose);
				menu->popup(Files->viewport()->mapToGlobal(aPosition));
			}
		});
		Files->header()->setSectionsClickable(true);
		connect(Files->header(), &QHeaderView::sectionClicked, this, [this](const int aSection) {
			if (aSection != overview_file_model::comments_column || !Model->feedback()) {
				return;
			}
			auto* const menu = make_sort_menu();
			menu->setAttribute(Qt::WA_DeleteOnClose);
			const auto left = Files->header()->sectionViewportPosition(aSection);
			menu->popup(Files->header()->mapToGlobal(QPoint{ left, Files->header()->height() }));
		});
		anchor_tooltips(*Files->viewport(), [this](const QPoint aPosition) -> std::optional<anchored_tooltip_target> {
			if (const auto chip = chip_at(aPosition)) {
				const auto* counts = Model->feedback_at(chip->first);
				return anchored_tooltip_target{ .Anchor = chip->second.Box.toAlignedRect(), .Text = chip_tooltip(chip->second, *counts, any_columns(Model->feedback())) };
			}
			const auto index = Files->indexAt(aPosition);
			const auto text  = index.isValid() ? index.data(Qt::ToolTipRole).toString() : QString{ };
			if (text.isEmpty()) {
				return std::nullopt;
			}
			return anchored_tooltip_target{ .Anchor = Files->visualRect(index), .Text = text };
		});
		filesColumn->addWidget(Files, 1);
		// RFC-BPR-0089 M2. The tree ends with the generated files set aside.
		GeneratedRow = new generated_files_row{ main };
		GeneratedRow->set_gesture_dispatch([this](const application::generated_file_gesture& aGesture) {
			if (GeneratedDispatch) {
				GeneratedDispatch(aGesture);
			}
		});
		filesColumn->addWidget(GeneratedRow, 0);
		CachedThreadsHeading = make_heading(QString{ }, QStringLiteral("overviewCachedThreadsHeading"), main);
		mainColumn->addWidget(CachedThreadsHeading);
		CachedThreads = new QPlainTextEdit{ main };
		CachedThreads->setObjectName(QStringLiteral("overviewCachedThreads"));
		CachedThreads->setAccessibleName(QStringLiteral("Historical review threads; freshness unknown"));
		CachedThreads->setAccessibleDescription(QStringLiteral("Read-only historical text. Anchors and links cannot be opened."));
		CachedThreads->setReadOnly(true);
		CachedThreads->setLineWrapMode(QPlainTextEdit::WidgetWidth);
		mainColumn->addWidget(CachedThreads, 1);
		CachedDiscussion = new QPlainTextEdit{ main };
		CachedDiscussion->setObjectName(QStringLiteral("overviewCachedDiscussion"));
		CachedDiscussion->setAccessibleName(QStringLiteral("Historical general comments and review summaries; freshness unknown"));
		CachedDiscussion->setReadOnly(true);
		CachedDiscussion->setLineWrapMode(QPlainTextEdit::WidgetWidth);
		mainColumn->addWidget(CachedDiscussion, 1);
		CachedSourceBack = new quiet_push_button{ QStringLiteral("Back to cached overview"), main };
		CachedSourceBack->setObjectName(QStringLiteral("overviewCachedSourceBack"));
		connect(CachedSourceBack, &QPushButton::clicked, this, [this] { show_cached_source(false); Files->setFocus(Qt::OtherFocusReason); });
		mainColumn->addWidget(CachedSourceBack);
		CachedSource = new QPlainTextEdit{ main };
		CachedSource->setObjectName(QStringLiteral("overviewCachedSource"));
		CachedSource->setAccessibleName(QStringLiteral("Historical exact base and head source; freshness unknown"));
		CachedSource->setReadOnly(true);
		CachedSource->setLineWrapMode(QPlainTextEdit::NoWrap);
		mainColumn->addWidget(CachedSource, 2);

		auto* const sideScroll = new QScrollArea{ this };
		sideScroll->setObjectName(QStringLiteral("overviewSideScroll"));
		sideScroll->setAccessibleName(QStringLiteral("Participants and review summary"));
		sideScroll->setWidgetResizable(true);
		// [[BPR-1588]]. The Status drawer sets the width; beneath the column's
		// own minimum it scrolls sideways rather than clipping.
		sideScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		sideScroll->setFrameShape(QFrame::NoFrame);
		auto* const side = new QWidget{ sideScroll };
		Side             = sideScroll;
		side->setObjectName(QStringLiteral("overviewSideColumn"));
		side->setAutoFillBackground(true);
		side->setBackgroundRole(QPalette::AlternateBase);
		auto* const sideColumn = new QVBoxLayout{ side };
		sideColumn->setContentsMargins(14, 12, 14, 12);
		sideColumn->setSpacing(2);
		sideColumn->setSizeConstraint(QLayout::SetMinimumSize);

		// [[BPR-1589]]. RFC-BPR-0085 D7, the owner's "attention first": my role,
		// then what needs me, my review, the checks and the people, each a
		// section that opens in place to its details. Nothing is drawn to say
		// 0, — or not available; each failed check is named in one place.
		StatusNote = new QLabel{ side };
		StatusNote->setObjectName(QStringLiteral("overviewStatusNote"));
		StatusNote->setTextFormat(Qt::PlainText);
		StatusNote->setWordWrap(true);
		StatusNote->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		StatusNote->setForegroundRole(QPalette::PlaceholderText);
		StatusNote->hide();
		sideColumn->addWidget(StatusNote);

		const auto makeText = [](QWidget* const aOwner, const QString& aName) {
			auto* label = new QLabel{ aOwner };
			label->setObjectName(aName);
			label->setTextFormat(Qt::PlainText);
			label->setWordWrap(true);
			label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
			label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
			return label;
		};

		// [[BPR-1589]] v002. My role: my face, one short label, and any other
		// roles as chips; the reasons are in the tooltip.
		RoleRow = new overview_role_line{ side };
		RoleRow->setObjectName(QStringLiteral("overviewStatusRole"));
		sideColumn->addWidget(RoleRow);
		struct section_layouts
		{
			QVBoxLayout* Body    = nullptr;
			QVBoxLayout* Details = nullptr;
		};
		const auto makeSection = [this, side, sideColumn](const overview_status_section aSection, const QString& aTitle, const QString& aName) {
			auto& parts  = Sections[static_cast<std::size_t>(aSection)];
			parts.Frame  = new QWidget{ side };
			auto* column = new QVBoxLayout{ parts.Frame };
			column->setContentsMargins(0, 6, 0, 2);
			column->setSpacing(2);
			parts.Header = new drawer_section_header{ parts.Frame };
			parts.Header->setObjectName(aName);
			parts.Header->setText(aTitle);
			parts.Header->setAccessibleName(aTitle);
			column->addWidget(parts.Header);
			auto* const body       = new QWidget{ parts.Frame };
			auto* const bodyLayout = new QVBoxLayout{ body };
			bodyLayout->setContentsMargins(4, 0, 4, 0);
			bodyLayout->setSpacing(2);
			column->addWidget(body);
			parts.Details             = new QWidget{ parts.Frame };
			auto* const detailsLayout = new QVBoxLayout{ parts.Details };
			detailsLayout->setContentsMargins(4, 8, 4, 2);
			detailsLayout->setSpacing(2);
			parts.Details->hide();
			column->addWidget(parts.Details);
			sideColumn->addWidget(parts.Frame);
			connect(parts.Header, &QToolButton::toggled, this, [this, aSection](const bool aOpen) {
				Sections[static_cast<std::size_t>(aSection)].Details->setVisible(aOpen);
				remember_sections();
				if (aSection == overview_status_section::checks) {
					present_attention();
				}
			});
			return section_layouts{ bodyLayout, detailsLayout };
		};
		const auto addValue = [this](QVBoxLayout* const aColumn, const overview_value aValue) {
			auto* const row       = new QWidget{ aColumn->parentWidget() };
			auto* const rowLayout = new QHBoxLayout{ row };
			rowLayout->setContentsMargins(0, 2, 0, 2);
			auto* const name = new QLabel{ value_name(aValue), row };
			name->setTextFormat(Qt::PlainText);
			auto* const value = new QLabel{ row };
			value->setObjectName(value_object_name(aValue));
			value->setAccessibleName(value_name(aValue));
			value->setTextFormat(Qt::PlainText);
			value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
			rowLayout->addWidget(name, 1);
			rowLayout->addWidget(value, 0);
			aColumn->addWidget(row);
			Values[static_cast<std::size_t>(aValue)] = value;
		};

		// Needs attention, opening to Merge's conditions and the requirement
		// details. With nothing to attend to, it is Merge, and says its state.
		const auto attention         = makeSection(overview_status_section::attention, QStringLiteral("Needs attention"), QStringLiteral("overviewAttentionSection"));
		AttentionList                = attention.Body;
		auto* const attentionDetails = Sections[static_cast<std::size_t>(overview_status_section::attention)].Details;
		MergeState                   = makeText(attentionDetails, QStringLiteral("overviewMergeState"));
		auto stateFont               = MergeState->font();
		stateFont.setWeight(QFont::DemiBold);
		MergeState->setFont(stateFont);
		attention.Details->addWidget(MergeState);
		MergeConditions = makeText(attentionDetails, QStringLiteral("overviewMergeConditions"));
		attention.Details->addWidget(MergeConditions);
		// [[BPR-1579]]. A disclosure bar drawn from the theme, not the style.
		RequirementDisclosure = new disclosure_button{ attentionDetails };
		RequirementDisclosure->setObjectName(QStringLiteral("overviewRequirementDisclosure"));
		RequirementDisclosure->setText(QStringLiteral("Requirement details"));
		RequirementDisclosure->setAccessibleDescription(QStringLiteral("Show applicable rule origins, approval restrictions and auto-merge observation"));
		attention.Details->addSpacing(4);
		attention.Details->addWidget(RequirementDisclosure);
		RequirementDetails = makeText(attentionDetails, QStringLiteral("overviewRequirementDetails"));
		RequirementDetails->setForegroundRole(QPalette::PlaceholderText);
		RequirementDetails->hide();
		attention.Details->addWidget(RequirementDetails);
		connect(RequirementDisclosure, &QToolButton::toggled, this, [this](bool aOpen) { RequirementDetails->setVisible(aOpen); });

		// My review: files read with a bar, my notes and pending comments, and
		// the threads with posts I have not read, with Go to them. It opens to
		// what happened since my last review that has a value.
		const auto  myReview = makeSection(overview_status_section::my_review, QStringLiteral("My review"), QStringLiteral("overviewMyReviewSection"));
		auto* const myBody   = myReview.Body->parentWidget();
		addValue(myReview.Body, overview_value::files_read);
		ReadMeter = new drawer_meter{ myBody };
		ReadMeter->setObjectName(QStringLiteral("overviewReadMeter"));
		myReview.Body->addWidget(ReadMeter);
		myReview.Body->addSpacing(4);
		addValue(myReview.Body, overview_value::private_notes);
		addValue(myReview.Body, overview_value::drafted_comments);
		NewThreadsRow = new QWidget{ myBody };
		NewThreadsRow->setObjectName(QStringLiteral("overviewNewThreads"));
		auto* const newLayout = new QHBoxLayout{ NewThreadsRow };
		newLayout->setContentsMargins(0, 0, 0, 0);
		NewThreadsText = new QLabel{ NewThreadsRow };
		NewThreadsText->setObjectName(QStringLiteral("overviewNewThreadsText"));
		NewThreadsText->setTextFormat(Qt::PlainText);
		NewThreadsGo = new QToolButton{ NewThreadsRow };
		NewThreadsGo->setObjectName(QStringLiteral("overviewGoToNewThreads"));
		NewThreadsGo->setText(QStringLiteral("Go to them"));
		NewThreadsGo->setAutoRaise(true);
		NewThreadsGo->setCursor(Qt::PointingHandCursor);
		NewThreadsGo->setToolTip(QStringLiteral("Open Threads with Only new on"));
		newLayout->addWidget(NewThreadsText, 1);
		newLayout->addWidget(NewThreadsGo, 0);
		NewThreadsRow->hide();
		myReview.Body->addWidget(NewThreadsRow);
		connect(NewThreadsGo, &QToolButton::clicked, this, [this] {
			if (NewThreadsDispatch) {
				NewThreadsDispatch();
			}
		});
		// The conversation's new comments, which open in Discuss. Hidden until
		// the ledger keeps read state for comments that are not on a file.
		NewConversationRow = new QWidget{ myBody };
		NewConversationRow->setObjectName(QStringLiteral("overviewNewConversation"));
		auto* const conversationLayout = new QHBoxLayout{ NewConversationRow };
		conversationLayout->setContentsMargins(0, 0, 0, 0);
		NewConversationText = new QLabel{ NewConversationRow };
		NewConversationText->setObjectName(QStringLiteral("overviewNewConversationText"));
		NewConversationText->setTextFormat(Qt::PlainText);
		NewConversationGo = new QToolButton{ NewConversationRow };
		NewConversationGo->setObjectName(QStringLiteral("overviewGoToNewConversation"));
		NewConversationGo->setText(QStringLiteral("Go to them"));
		NewConversationGo->setAutoRaise(true);
		NewConversationGo->setCursor(Qt::PointingHandCursor);
		NewConversationGo->setToolTip(QStringLiteral("Open Discuss on the pull request"));
		conversationLayout->addWidget(NewConversationText, 1);
		conversationLayout->addWidget(NewConversationGo, 0);
		NewConversationRow->hide();
		myReview.Body->addWidget(NewConversationRow);
		connect(NewConversationGo, &QToolButton::clicked, this, [this] {
			if (NewConversationDispatch) {
				NewConversationDispatch();
			}
		});
		SinceHeading = make_heading(QStringLiteral("Since my last review"), QStringLiteral("overviewSinceHeading"), Sections[static_cast<std::size_t>(overview_status_section::my_review)].Details);
		myReview.Details->addWidget(SinceHeading);
		addValue(myReview.Details, overview_value::uploads);
		addValue(myReview.Details, overview_value::files_changed);
		addValue(myReview.Details, overview_value::new_comments);
		addValue(myReview.Details, overview_value::replies_to_me);
		addValue(myReview.Details, overview_value::my_unresolved_threads);

		// [[BPR-1588]]. No Start review: the owner, 2026-10-17, "Start review is
		// probably unnecessary now that the Review tab is so convenient, and
		// double-clicking a file or clicking Next file also works." Its one
		// job, Review at the first unseen change, is the first entry into
		// Review from here (`entry_path`).

		// Checks: one bar and its counts, opening to every result, the selected
		// result's card and Open check details (RFC-BPR-0068).
		const auto checks      = makeSection(overview_status_section::checks, QStringLiteral("Checks"), QStringLiteral("overviewChecksDisclosure"));
		ChecksDisclosure       = Sections[static_cast<std::size_t>(overview_status_section::checks)].Header;
		CheckDetails           = Sections[static_cast<std::size_t>(overview_status_section::checks)].Details;
		auto* const checksBody = checks.Body->parentWidget();
		ChecksMeter            = new drawer_meter{ checksBody };
		ChecksMeter->setObjectName(QStringLiteral("overviewChecksMeter"));
		checks.Body->addWidget(ChecksMeter);
		auto* const checksValue = makeText(checksBody, value_object_name(overview_value::checks));
		checksValue->setAccessibleName(value_name(overview_value::checks));
		checks.Body->addWidget(checksValue);
		Values[static_cast<std::size_t>(overview_value::checks)] = checksValue;
		ChecksNote                                               = makeText(CheckDetails, QStringLiteral("overviewChecksNote"));
		ChecksNote->setForegroundRole(QPalette::PlaceholderText);
		checks.Details->addWidget(ChecksNote);
		CheckList = new QTreeWidget{ CheckDetails };
		CheckList->setObjectName(QStringLiteral("overviewCheckList"));
		CheckList->setAccessibleName(QStringLiteral("Check results"));
		CheckList->setHeaderHidden(true);
		CheckList->setRootIsDecorated(false);
		CheckList->setWordWrap(true);
		CheckList->setItemDelegate(new check_result_delegate{ CheckList });
		CheckList->setTextElideMode(Qt::ElideNone);
		CheckList->header()->setSectionResizeMode(QHeaderView::Stretch);
		CheckList->setMinimumWidth(0);
		CheckList->setFixedHeight(180);
		checks.Details->addWidget(CheckList);
		CheckHeading = makeText(CheckDetails, QStringLiteral("overviewSelectedCheck"));
		checks.Details->addWidget(CheckHeading);
		CheckDescription = new QPlainTextEdit{ CheckDetails };
		CheckDescription->setObjectName(QStringLiteral("overviewCheckDescription"));
		CheckDescription->setAccessibleName(QStringLiteral("Check explanation"));
		CheckDescription->setReadOnly(true);
		CheckDescription->setLineWrapMode(QPlainTextEdit::WidgetWidth);
		CheckDescription->setFixedHeight(140);
		checks.Details->addWidget(CheckDescription);
		CheckOpen = new quiet_push_button{ QStringLiteral("Open check details"), CheckDetails };
		CheckOpen->setObjectName(QStringLiteral("overviewOpenCheck"));
		CheckOpen->setAccessibleName(QStringLiteral("Open check details"));
		checks.Details->addWidget(CheckOpen, 0, Qt::AlignLeft);

		// People: faces with their standing and one line, opening to the
		// roster with each person's request, review and code-owner marking.
		const auto  people     = makeSection(overview_status_section::people, QStringLiteral("People"), QStringLiteral("overviewPeopleSection"));
		auto* const peopleBody = people.Body->parentWidget();
		auto* const facesRow   = new QHBoxLayout;
		facesRow->setContentsMargins(0, 2, 0, 2);
		facesRow->setSpacing(10);
		Faces = new overview_faces{ peopleBody };
		facesRow->addWidget(Faces, 0);
		PeopleSummary = makeText(peopleBody, QStringLiteral("overviewPeopleSummary"));
		PeopleSummary->setForegroundRole(QPalette::PlaceholderText);
		facesRow->addWidget(PeopleSummary, 1);
		people.Body->addLayout(facesRow);
		// [[BPR-1804]]. Request review opens the people picker under it.
		RequestReview = new quiet_push_button{ QStringLiteral("Request review"), peopleBody };
		RequestReview->setObjectName(QStringLiteral("overviewRequestReview"));
		RequestReview->setAccessibleName(QStringLiteral("Request review"));
		RequestReview->setAccessibleDescription(QStringLiteral("Opens a search of the people who can review this pull request"));
		RequestReview->setToolTip(QStringLiteral("Ask someone to review this pull request"));
		RequestReview->hide();
		people.Body->addWidget(RequestReview, 0, Qt::AlignLeft);
		connect(RequestReview, &QPushButton::clicked, this, [this] {
			if (ReviewersOffered && ReviewerDispatch) {
				ReviewerDispatch({ .Kind = reviewer_picker_gesture::kind::open });
			}
		});
		// [[BPR-1852]]. Convert to draft sits with who is asked to review,
		// where GitHub's own page keeps it: making a pull request a draft
		// again is about the reviewers being asked.
		ConvertToDraft = new QToolButton{ peopleBody };
		ConvertToDraft->setText(QStringLiteral("Convert to draft"));
		ConvertToDraft->setAutoRaise(true);
		ConvertToDraft->setCursor(Qt::PointingHandCursor);
		ConvertToDraft->setObjectName(QStringLiteral("overviewConvertToDraft"));
		ConvertToDraft->setAccessibleName(QStringLiteral("Convert to draft"));
		ConvertToDraft->setAccessibleDescription(QStringLiteral("Makes this pull request a draft again on GitHub; it is not ready for review until it is marked ready"));
		ConvertToDraft->setToolTip(QStringLiteral("Still in progress? Make this pull request a draft again"));
		ConvertToDraft->hide();
		people.Body->addWidget(ConvertToDraft, 0, Qt::AlignLeft);
		ConvertStatus = makeText(peopleBody, QStringLiteral("overviewConvertToDraftStatus"));
		ConvertStatus->hide();
		people.Body->addWidget(ConvertStatus);
		connect(ConvertToDraft, &QToolButton::clicked, this, [this] {
			if (DraftStateOffer == core::draft_state_change::convert_to_draft && !ConvertBusy && DraftStateDispatch) {
				DraftStateDispatch(core::draft_state_change::convert_to_draft);
			}
		});
		Participants = new QVBoxLayout;
		Participants->setContentsMargins(0, 0, 0, 0);
		Participants->setSpacing(0);
		people.Details->addLayout(Participants);
		Requests = new QVBoxLayout;
		Requests->setContentsMargins(0, 0, 0, 0);
		Requests->setSpacing(0);
		people.Details->addLayout(Requests);
		RequestsNote = makeText(Sections[static_cast<std::size_t>(overview_status_section::people)].Details, QStringLiteral("overviewReviewRequestsNote"));
		RequestsNote->setForegroundRole(QPalette::PlaceholderText);
		people.Details->addWidget(RequestsNote);
		connect(CheckList, &QTreeWidget::currentItemChanged, this, [this] { present_selected_check(); });
		connect(CheckOpen, &QPushButton::clicked, this, [this] {
			if (!Projection || !CheckList->currentItem() || !CheckLinkDispatch) {
				return;
			}
			const auto identity = CheckList->currentItem()->data(0, Qt::UserRole).toString().toStdString();
			const auto row      = std::ranges::find(Projection->CheckRows, identity, &application::overview_check_row::Identity);
			if (row != Projection->CheckRows.end() && row->LinkAvailable) {
				CheckLinkDispatch(row->Identity, row->DetailsUrl);
			}
		});
		sideColumn->addStretch(1);
		sideScroll->setWidget(side);

		// [[BPR-1588]]. The column is the Status drawer, beside the main column
		// on a splitter, so it resizes, collapses and is remembered as the
		// workbench drawers are (RFC-BPR-0085 principle 7).
		StatusSplit  = new overview_status_splitter{ this };
		StatusDrawer = new overview_status_drawer{ sideScroll, StatusSplit };
		StatusSplit->addWidget(main);
		StatusSplit->addWidget(StatusDrawer);
		StatusSplit->setStretchFactor(0, 1);
		StatusSplit->setStretchFactor(1, 0);
		// A width leaves only from a handle drag, as the workbench's does.
		connect(StatusSplit, &QSplitter::splitterMoved, this, [this](int, int) { StatusDrawer->observe_drag(StatusDrawer->width()); });
		auto* const columns = new QHBoxLayout{ this };
		columns->setContentsMargins(0, 0, 0, 0);
		columns->setSpacing(0);
		columns->addWidget(StatusSplit, 1);

		Colours.Marks  = default_feedback_mark_theme(palette());
		Colours.States = default_feedback_state_colours(palette());
		refresh_feedback_icons();
		// [[BPR-1576]]. Each section says its own wait, over its own place, so
		// a section that is ready shows while the others still load.
		Veils.Header      = new pending_veil{ *this, pending_shape::text, QStringLiteral("Pull request"), Header };
		Veils.Description = new pending_veil{ *this, pending_shape::text, QStringLiteral("Description"), SummaryPane };
		// Over the rows only: the column names stay readable above them.
		Veils.Files = new pending_veil{ *this, pending_shape::rows, QStringLiteral("Files"), Files->viewport() };
		Veils.Side  = new pending_veil{ *this, pending_shape::pairs, QStringLiteral("Review summary"), sideScroll };
		Veils.Side->set_placeholder_count(overview_value_count + 3);
		// One voice for the page: the files say when the Overview is ready.
		Veils.Header->set_announces(false);
		Veils.Description->set_announces(false);
		Veils.Side->set_announces(false);
		clear();
	}
}
