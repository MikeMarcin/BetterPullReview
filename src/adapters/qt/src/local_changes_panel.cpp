#include "bpr/qt/local_changes_panel.hpp"

#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/enhance_control.hpp"

#include <QAbstractButton>
#include <QCheckBox>
#include <QEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace bpr::qt {
	namespace {
		constexpr int path_role = Qt::UserRole + 1;

		[[nodiscard]] auto text_of(const std::string& aText) -> QString { return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())); }
		[[nodiscard]] auto utf8(const QString& aText) -> std::string { return aText.toUtf8().toStdString(); }
		[[nodiscard]] auto dark(const QPalette& aPalette) -> bool { return aPalette.color(QPalette::Window).lightness() < 128; }
		[[nodiscard]] auto green(const QPalette& aPalette) -> QColor { return dark(aPalette) ? QColor{ 86, 211, 100 } : QColor{ 26, 127, 55 }; }
		[[nodiscard]] auto red(const QPalette& aPalette) -> QColor { return dark(aPalette) ? QColor{ 248, 81, 73 } : QColor{ 207, 34, 46 }; }
		[[nodiscard]] auto amber(const QPalette& aPalette) -> QColor { return dark(aPalette) ? QColor{ 210, 153, 34 } : QColor{ 154, 103, 0 }; }

		/// A file's status letter in a small box; only added and deleted take a colour.
		[[nodiscard]] auto badge_icon(const char aLetter, const QPalette& aPalette) -> QIcon
		{
			auto pixmap = QPixmap{ 32, 32 };
			pixmap.setDevicePixelRatio(2.0);
			pixmap.fill(Qt::transparent);
			auto painter = QPainter{ &pixmap };
			painter.setRenderHint(QPainter::Antialiasing);
			const auto ink = aLetter == 'A' ? green(aPalette) : aLetter == 'D' ? red(aPalette)
																			   : aPalette.color(QPalette::WindowText);
			auto       box = ink;
			box.setAlpha(110);
			painter.setPen(QPen{ box, 1.0 });
			painter.drawRoundedRect(QRectF{ 1.5, 1.5, 13.0, 13.0 }, 3.0, 3.0);
			auto letter = QFont{ };
			letter.setPixelSize(10);
			letter.setWeight(QFont::DemiBold);
			painter.setFont(letter);
			painter.setPen(ink);
			painter.drawText(QRectF{ 1.0, 1.0, 14.0, 14.0 }, Qt::AlignCenter, QString{ QLatin1Char{ aLetter } });
			painter.end();
			return QIcon{ pixmap };
		}

		/// A text segment of the three-way pick, raised and neutral when picked.
		class pick_segment final : public QAbstractButton
		{
		public:
			explicit pick_segment(QWidget* const aParent)
				: QAbstractButton{ aParent }
			{
				setCheckable(true);
				setAutoExclusive(true);
				setFocusPolicy(Qt::TabFocus);
				setAttribute(Qt::WA_Hover);
			}
			[[nodiscard]] auto sizeHint() const -> QSize override { return { fontMetrics().horizontalAdvance(text()) + 18, fontMetrics().height() + 10 }; }

		protected:
			void paintEvent(QPaintEvent*) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing);
				const auto& colours = palette();
				const auto  box     = QRectF{ rect() }.adjusted(2.0, 2.0, -2.0, -2.0);
				if (isChecked()) {
					auto shadow = colours.color(QPalette::Shadow);
					shadow.setAlpha(40);
					painter.setPen(Qt::NoPen);
					painter.setBrush(shadow);
					painter.drawRoundedRect(box.translated(0.0, 0.5), 4.0, 4.0);
					painter.setBrush(colours.color(QPalette::Button).lighter(dark(colours) ? 140 : 108));
					auto edge = colours.color(QPalette::Mid);
					edge.setAlpha(150);
					painter.setPen(QPen{ edge, 0.5 });
					painter.drawRoundedRect(box, 4.0, 4.0);
				}
				auto ink = colours.color(QPalette::WindowText);
				if (!isEnabled()) {
					ink.setAlpha(90);
				} else if (!isChecked()) {
					ink.setAlpha(190);
				}
				painter.setPen(ink);
				painter.drawText(rect(), Qt::AlignCenter, text());
			}
		};

		[[nodiscard]] auto heading(const QString& aText, QWidget* const aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			auto        font  = label->font();
			font.setWeight(QFont::DemiBold);
			label->setFont(font);
			return label;
		}

		[[nodiscard]] auto against(const application::local_changes_projection& aView) -> QString
		{
			if (aView.Hosted.PullRequest) {
				return QStringLiteral("#%1").arg(*aView.Hosted.PullRequest);
			}
			return QStringLiteral("GitHub");
		}
	}

	local_changes_panel::local_changes_panel(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("localChangesPanel"));
		setAccessibleName(QStringLiteral("Local Changes"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(10, 8, 10, 10);
		column->setSpacing(6);

		// The three-way pick: one comparison at a time.
		auto* const pickRow   = new QWidget{ this };
		auto* const pickWells = new QHBoxLayout{ pickRow };
		pickWells->setContentsMargins(2, 2, 2, 2);
		pickWells->setSpacing(0);
		const auto names = std::array{ QStringLiteral("Uncommitted"), QStringLiteral("Not pushed"), QStringLiteral("Whole PR") };
		const auto tips  = std::array{ QStringLiteral("Changes since your last commit."),
									   QStringLiteral("Changes GitHub does not have yet."),
									   QStringLiteral("Everything the pull request contains.") };
		const auto ids   = std::array{ QStringLiteral("localChangesUncommitted"), QStringLiteral("localChangesNotPushed"), QStringLiteral("localChangesWholePullRequest") };
		for (std::size_t index = 0U; index < Picks.size(); ++index) {
			auto* const segment = new pick_segment{ pickRow };
			segment->setObjectName(ids[index]);
			segment->setText(names[index]);
			segment->setToolTip(tips[index]);
			segment->setAccessibleName(names[index]);
			Picks[index] = segment;
			pickWells->addWidget(segment);
			connect(segment, &QAbstractButton::clicked, this, [this, index] {
				if (!Presenting && Dispatch.Choose) {
					Dispatch.Choose(static_cast<core::local_comparison>(index));
				}
			});
		}
		Picks[0]->setChecked(true);
		pickWells->addStretch(1);
		column->addWidget(pickRow);

		Pages                   = new QStackedWidget{ this };
		auto* const uncommitted = new QWidget{ Pages };
		auto* const lists       = new QVBoxLayout{ uncommitted };
		lists->setContentsMargins(0, 0, 0, 0);
		lists->setSpacing(4);
		const auto tree = [this](QWidget* const aOwner, const QString& aName, const QString& aId) {
			auto* const made = new QTreeWidget{ aOwner };
			made->setObjectName(aId);
			made->setAccessibleName(aName);
			made->setColumnCount(2);
			made->setHeaderHidden(true);
			made->setRootIsDecorated(true);
			made->setIndentation(14);
			made->setUniformRowHeights(true);
			made->setSelectionMode(QAbstractItemView::ExtendedSelection);
			made->setFrameShape(QFrame::NoFrame);
			made->header()->setStretchLastSection(false);
			made->header()->setSectionResizeMode(0, QHeaderView::Stretch);
			made->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
			connect(made, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* const aItem) {
				if (!Presenting && aItem != nullptr && !aItem->data(0, path_role).toString().isEmpty() && Dispatch.OpenFile) {
					Dispatch.OpenFile(utf8(aItem->data(0, path_role).toString()));
				}
			});
			return made;
		};
		const auto header = [](QLabel* const aTitle, QPushButton* const aButton) {
			auto* const row = new QHBoxLayout{ };
			row->setContentsMargins(0, 0, 0, 0);
			row->addWidget(aTitle, 1);
			row->addWidget(aButton);
			return row;
		};
		UnstagedTitle = heading(QStringLiteral("Unstaged"), uncommitted);
		Stage         = new QPushButton{ QStringLiteral("Stage"), uncommitted };
		Stage->setObjectName(QStringLiteral("localChangesStage"));
		Stage->setToolTip(QStringLiteral("Stage the selected files."));
		Unstaged    = tree(uncommitted, QStringLiteral("Unstaged files"), QStringLiteral("localChangesUnstaged"));
		StagedTitle = heading(QStringLiteral("Staged"), uncommitted);
		Unstage     = new QPushButton{ QStringLiteral("Unstage"), uncommitted };
		Unstage->setObjectName(QStringLiteral("localChangesUnstage"));
		Unstage->setToolTip(QStringLiteral("Unstage the selected files."));
		Staged             = tree(uncommitted, QStringLiteral("Staged files"), QStringLiteral("localChangesStaged"));
		const auto buttons = std::max(Stage->sizeHint().width(), Unstage->sizeHint().width());
		Stage->setFixedWidth(buttons);
		Unstage->setFixedWidth(buttons);
		lists->addLayout(header(UnstagedTitle, Stage));
		lists->addWidget(Unstaged, 1);
		lists->addLayout(header(StagedTitle, Unstage));
		lists->addWidget(Staged, 1);
		Pages->addWidget(uncommitted);
		Listed = tree(Pages, QStringLiteral("Changed files"), QStringLiteral("localChangesListed"));
		Pages->addWidget(Listed);
		column->addWidget(Pages, 1);

		connect(Stage, &QPushButton::clicked, this, [this] {
			if (Dispatch.Stage) {
				Dispatch.Stage(selected(*Unstaged));
			}
		});
		connect(Unstage, &QPushButton::clicked, this, [this] {
			if (Dispatch.Unstage) {
				Dispatch.Unstage(selected(*Staged));
			}
		});
		connect(Unstaged, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* const aItem) {
			if (aItem != nullptr && !aItem->data(0, path_role).toString().isEmpty() && Dispatch.Stage) {
				Dispatch.Stage({ utf8(aItem->data(0, path_role).toString()) });
			}
		});
		connect(Staged, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* const aItem) {
			if (aItem != nullptr && !aItem->data(0, path_role).toString().isEmpty() && Dispatch.Unstage) {
				Dispatch.Unstage({ utf8(aItem->data(0, path_role).toString()) });
			}
		});
		connect(Unstaged, &QTreeWidget::itemSelectionChanged, this, [this] { Stage->setEnabled(!selected(*Unstaged).empty() && Projection.Running == application::local_changes_operation::none); });
		connect(Staged, &QTreeWidget::itemSelectionChanged, this, [this] { Unstage->setEnabled(!selected(*Staged).empty() && Projection.Running == application::local_changes_operation::none); });

		// The commit box, pinned under the lists.
		auto* const box       = new QWidget{ this };
		auto* const boxColumn = new QVBoxLayout{ box };
		boxColumn->setContentsMargins(0, 6, 0, 0);
		boxColumn->setSpacing(4);
		auto* const subjectRow = new QHBoxLayout{ };
		subjectRow->setSpacing(4);
		Subject = new QLineEdit{ box };
		Subject->setObjectName(QStringLiteral("localChangesSubject"));
		Subject->setAccessibleName(QStringLiteral("Commit subject"));
		Subject->setPlaceholderText(QStringLiteral("Subject"));
		subjectRow->addWidget(Subject, 1);
		boxColumn->addLayout(subjectRow);
		Description = new QPlainTextEdit{ box };
		Description->setObjectName(QStringLiteral("localChangesDescription"));
		Description->setAccessibleName(QStringLiteral("Commit description"));
		Description->setPlaceholderText(QStringLiteral("Description"));
		Description->setTabChangesFocus(true);
		Description->setFixedHeight(Description->fontMetrics().lineSpacing() * 3 + 12);
		boxColumn->addWidget(Description);
		auto* const commitRow = new QHBoxLayout{ };
		Amend                 = new QCheckBox{ QStringLiteral("Amend"), box };
		Amend->setObjectName(QStringLiteral("localChangesAmend"));
		commitRow->addWidget(Amend);
		commitRow->addStretch(1);
		// Owner, 2026-11-13: the agent's nib belongs next to Commit, in the
		// agent's accent, as every agent run's control is.
		Suggest = new QToolButton{ box };
		Suggest->setObjectName(QStringLiteral("localChangesSuggest"));
		Suggest->setAccessibleName(QStringLiteral("Write the commit message with the agent"));
		Suggest->setAutoRaise(true);
		wear_agent_accent(*Suggest, palette());
		Suggest->setIcon(enhance_icon(Suggest->palette()));
		commitRow->addWidget(Suggest);
		Commit = new QPushButton{ QStringLiteral("Commit"), box };
		Commit->setObjectName(QStringLiteral("localChangesCommit"));
		Commit->setToolTip(QStringLiteral("Commit what is staged."));
		commitRow->addWidget(Commit);
		boxColumn->addLayout(commitRow);
		Banner = new QLabel{ box };
		Banner->setObjectName(QStringLiteral("localChangesBanner"));
		Banner->setWordWrap(true);
		Banner->setTextInteractionFlags(Qt::TextSelectableByMouse);
		Banner->hide();
		boxColumn->addWidget(Banner);
		auto* const rule = new QFrame{ box };
		rule->setFrameShape(QFrame::HLine);
		rule->setFrameShadow(QFrame::Plain);
		boxColumn->addWidget(rule);
		auto* const pushRow = new QHBoxLayout{ };
		PushDot             = new QLabel{ QStringLiteral("●"), box };
		PushState           = new QLabel{ box };
		PushState->setObjectName(QStringLiteral("localChangesPushState"));
		pushRow->addWidget(PushDot);
		pushRow->addWidget(PushState, 1);
		Push = new QPushButton{ QStringLiteral("Push"), box };
		Push->setObjectName(QStringLiteral("localChangesPush"));
		pushRow->addWidget(Push);
		boxColumn->addLayout(pushRow);
		Command = new QLabel{ box };
		Command->setObjectName(QStringLiteral("localChangesCommand"));
		Command->setAccessibleName(QStringLiteral("Push command"));
		Command->setToolTip(QStringLiteral("The push as it will run; never forced."));
		Command->setWordWrap(true);
		Command->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		Command->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
		Command->setFrameShape(QFrame::StyledPanel);
		Command->setMargin(6);
		boxColumn->addWidget(Command);
		column->addWidget(box);

		// Buttons whose words change keep one width.
		const auto commitWidth = Commit->fontMetrics().horizontalAdvance(QStringLiteral("Commit 999 Files")) + 32;
		Commit->setFixedWidth(commitWidth);
		Push->setFixedWidth(commitWidth);

		connect(Subject, &QLineEdit::textChanged, this, [this] { present_commit(); });
		connect(Amend, &QCheckBox::toggled, this, [this] { present_commit(); });
		connect(Commit, &QPushButton::clicked, this, [this] {
			if (Dispatch.Commit) {
				Dispatch.Commit({ .Subject = utf8(Subject->text()), .Description = utf8(Description->toPlainText()), .Amend = Amend->isChecked() });
			}
		});
		connect(Suggest, &QToolButton::clicked, this, [this] {
			if (Dispatch.Suggest) {
				Dispatch.Suggest();
			}
		});
		connect(Push, &QPushButton::clicked, this, [this] {
			if (Projection.Running == application::local_changes_operation::pushing) {
				if (Dispatch.Stop) {
					Dispatch.Stop();
				}
			} else if (Projection.Push.Offered) {
				if (Dispatch.Push) {
					Dispatch.Push();
				}
			} else if (Dispatch.CheckAgain) {
				Dispatch.CheckAgain();
			}
		});
		restyle();
		present({ });
	}

	void local_changes_panel::set_dispatch(local_changes_dispatch aDispatch) { Dispatch = std::move(aDispatch); }
	void local_changes_panel::set_agent_name(const QString& aName)
	{
		AgentName = aName;
		present_commit();
	}
	void local_changes_panel::set_consent_question(std::function<bool()> aQuestion) { ConsentQuestion = std::move(aQuestion); }

	auto local_changes_panel::pick(const core::local_comparison aComparison) const -> QAbstractButton* { return Picks[static_cast<std::size_t>(aComparison)]; }
	auto local_changes_panel::unstaged() const noexcept -> QTreeWidget* { return Unstaged; }
	auto local_changes_panel::staged() const noexcept -> QTreeWidget* { return Staged; }
	auto local_changes_panel::listed() const noexcept -> QTreeWidget* { return Listed; }
	auto local_changes_panel::stage_button() const noexcept -> QPushButton* { return Stage; }
	auto local_changes_panel::unstage_button() const noexcept -> QPushButton* { return Unstage; }
	auto local_changes_panel::subject() const noexcept -> QLineEdit* { return Subject; }
	auto local_changes_panel::description() const noexcept -> QPlainTextEdit* { return Description; }
	auto local_changes_panel::amend() const noexcept -> QCheckBox* { return Amend; }
	auto local_changes_panel::suggest_button() const noexcept -> QToolButton* { return Suggest; }
	auto local_changes_panel::commit_button() const noexcept -> QPushButton* { return Commit; }
	auto local_changes_panel::push_state() const noexcept -> QLabel* { return PushState; }
	auto local_changes_panel::push_button() const noexcept -> QPushButton* { return Push; }
	auto local_changes_panel::command() const noexcept -> QLabel* { return Command; }
	auto local_changes_panel::banner() const noexcept -> QLabel* { return Banner; }
	auto local_changes_panel::projection() const noexcept -> const application::local_changes_projection& { return Projection; }

	void local_changes_panel::present(const application::local_changes_projection& aProjection)
	{
		Projection         = aProjection;
		const auto hold    = qScopeGuard([this, was = Presenting] { Presenting = was; });
		Presenting         = true;
		const auto& view   = Projection;
		const auto  counts = std::array{ core::uncommitted_paths(view.Status).size(), view.NotPushed.size(), view.WholePullRequest.size() };
		const auto  names  = std::array{ QStringLiteral("Uncommitted"), QStringLiteral("Not pushed"), QStringLiteral("Whole PR") };
		for (std::size_t index = 0U; index < Picks.size(); ++index) {
			Picks[index]->setText(QStringLiteral("%1  %2").arg(names[index]).arg(counts[index]));
		}
		Picks[1]->setEnabled(view.NotPushedAvailable);
		Picks[1]->setToolTip(view.NotPushedAvailable ? QStringLiteral("Changes GitHub does not have yet.")
													 : QStringLiteral("Not on GitHub yet."));
		Picks[static_cast<std::size_t>(view.Comparison)]->setChecked(true);
		Pages->setCurrentIndex(view.Comparison == core::local_comparison::uncommitted ? 0 : 1);
		present_lists();
		present_commit();
		present_push();
		if (view.ConsentNeeded) {
			const auto allow = ConsentQuestion ? ConsentQuestion()
											   : QMessageBox::question(this, QStringLiteral("Run Git Hooks and Configuration?"),
																	   QStringLiteral("Commit and Push run this repository's Git hooks and your Git configuration on this computer, with your account's rights. Allow for this local draft?"),
																	   QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
					== QMessageBox::Yes;
			if (Dispatch.Consent) {
				Dispatch.Consent(allow);
			}
		}
		if (view.Suggestion && view.SuggestionRevision != SuggestionShown) {
			// An editable suggestion: it fills the fields and commits nothing.
			SuggestionShown = view.SuggestionRevision;
			Subject->setText(text_of(view.Suggestion->Subject));
			Description->setPlainText(text_of(view.Suggestion->Description));
		}
	}

	void local_changes_panel::fill(QTreeWidget& aTree, const std::vector<core::local_status_entry>& aEntries, const std::string& aCurrent)
	{
		const auto blocker     = QSignalBlocker{ &aTree };
		auto       wasSelected = QStringList{ };
		for (const auto& path : selected(aTree)) {
			wasSelected << text_of(path);
		}
		aTree.clear();
		auto folders = QMap<QString, QTreeWidgetItem*>{ };
		for (const auto& entry : aEntries) {
			const auto path   = text_of(entry.Path);
			const auto slash  = path.lastIndexOf(QLatin1Char{ '/' });
			const auto folder = slash < 0 ? QString{ } : path.left(slash);
			auto*      parent = static_cast<QTreeWidgetItem*>(nullptr);
			if (!folder.isEmpty()) {
				parent = folders.value(folder);
				if (parent == nullptr) {
					parent = new QTreeWidgetItem{ &aTree, QStringList{ folder } };
					parent->setFlags(Qt::ItemIsEnabled);
					parent->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
					folders.insert(folder, parent);
				}
			}
			auto* const item = parent != nullptr ? new QTreeWidgetItem{ parent } : new QTreeWidgetItem{ &aTree };
			item->setText(0, slash < 0 ? path : path.mid(slash + 1));
			item->setData(0, path_role, path);
			item->setIcon(0, badge_icon(core::badge(entry.Status), palette()));
			item->setToolTip(0, entry.Original.empty() ? path : QStringLiteral("%1, renamed from %2").arg(path, text_of(entry.Original)));
			if (entry.Binary) {
				item->setText(1, QStringLiteral("binary"));
			} else if (entry.Added + entry.Removed > 0U) {
				item->setText(1, entry.Removed == 0U ? QStringLiteral("+%1").arg(entry.Added) : entry.Added == 0U ? QStringLiteral("−%1").arg(entry.Removed)
																												  : QStringLiteral("+%1 −%2").arg(entry.Added).arg(entry.Removed));
			}
			item->setForeground(1, entry.Removed > entry.Added ? red(palette()) : green(palette()));
			if (wasSelected.contains(path)) {
				item->setSelected(true);
			}
			if (entry.Path == aCurrent) {
				aTree.setCurrentItem(item, 0, QItemSelectionModel::NoUpdate);
			}
		}
		aTree.expandAll();
	}

	void local_changes_panel::present_lists()
	{
		const auto& view    = Projection;
		const auto  current = view.File ? view.File->Path : std::string{ };
		fill(*Unstaged, view.Status.Unstaged, view.Comparison == core::local_comparison::uncommitted ? current : std::string{ });
		fill(*Staged, view.Status.Staged, view.Comparison == core::local_comparison::uncommitted ? current : std::string{ });
		fill(*Listed, view.Comparison == core::local_comparison::not_pushed ? view.NotPushed : view.WholePullRequest, current);
		UnstagedTitle->setText(QStringLiteral("Unstaged  %1").arg(view.Status.Unstaged.size()));
		StagedTitle->setText(QStringLiteral("Staged  %1").arg(view.Status.Staged.size()));
		const auto idle = view.Running == application::local_changes_operation::none;
		Stage->setEnabled(idle && !selected(*Unstaged).empty());
		Unstage->setEnabled(idle && !selected(*Staged).empty());
	}

	void local_changes_panel::present_commit()
	{
		const auto& view    = Projection;
		const auto  idle    = view.Running == application::local_changes_operation::none;
		const auto  request = core::local_commit_request{ .Subject = utf8(Subject->text()), .Description = utf8(Description->toPlainText()), .Amend = Amend->isChecked() };
		const auto  plan    = core::decide_commit(view.Status, request, view.HeadOnHost);
		const auto  files   = view.Status.Staged.size();
		Commit->setText(Amend->isChecked() && files == 0U ? QStringLiteral("Amend Commit") : files == 1U ? QStringLiteral("Commit 1 File")
																										 : QStringLiteral("Commit %1 Files").arg(files));
		Commit->setEnabled(idle && view.Active && plan.has_value());
		// The blue button is the next step: Commit once it can run.
		Commit->setDefault(Commit->isEnabled());
		Amend->setEnabled(view.AmendOffered || Amend->isChecked());
		Amend->setToolTip(view.AmendOffered ? QStringLiteral("Replace your last commit.")
											: QStringLiteral("GitHub already has your last commit."));
		const auto agent = AgentName.isEmpty() ? QStringLiteral("the agent") : AgentName;
		Suggest->setEnabled(idle && view.AgentAvailable && !view.Status.Staged.empty());
		Suggest->setToolTip(!view.AgentAvailable             ? QStringLiteral("Choose an agent in the footer first.")
								: view.Status.Staged.empty() ? QStringLiteral("Stage a change first.")
															 : QStringLiteral("Write the message with %1.").arg(agent));
		if (view.Running == application::local_changes_operation::suggesting) {
			Suggest->setToolTip(QStringLiteral("%1 is writing the message…").arg(agent));
		}
		if (!view.SuggestFailure.empty()) {
			Suggest->setToolTip(text_of(view.SuggestFailure));
		}
		wear_agent_accent(*Suggest, palette());
		Suggest->setIcon(enhance_icon(Suggest->palette()));
	}

	void local_changes_panel::present_push()
	{
		const auto& view    = Projection;
		const auto  colours = palette();
		auto        dot     = colours.color(QPalette::Mid);
		auto        state   = QString{ };
		auto        label   = QStringLiteral("Push");
		switch (view.Push.State) {
		case core::local_push_state::unobserved:
			state = QStringLiteral("Not checked with GitHub");
			label = QStringLiteral("Check Again");
			break;
		case core::local_push_state::missing:
			state = QStringLiteral("Not on GitHub yet");
			dot   = amber(colours);
			break;
		case core::local_push_state::ahead:
			state = QStringLiteral("%1 ahead of %2").arg(view.Hosted.Ahead).arg(against(view));
			dot   = amber(colours);
			break;
		case core::local_push_state::up_to_date:
			state = QStringLiteral("Up to date with %1").arg(against(view));
			dot   = green(colours);
			label = QStringLiteral("Check Again");
			break;
		case core::local_push_state::behind:
			state = QStringLiteral("%1 behind %2").arg(view.Hosted.Behind).arg(against(view));
			dot   = red(colours);
			label = QStringLiteral("Check Again");
			break;
		case core::local_push_state::diverged:
			state = QStringLiteral("%1 ahead, %2 behind %3").arg(view.Hosted.Ahead).arg(view.Hosted.Behind).arg(against(view));
			dot   = red(colours);
			label = QStringLiteral("Check Again");
			break;
		case core::local_push_state::unknown_tip:
			state = QStringLiteral("%1 has commits this branch does not").arg(against(view));
			dot   = red(colours);
			label = QStringLiteral("Check Again");
			break;
		}
		if (view.Running == application::local_changes_operation::pushing) {
			label = QStringLiteral("Stop");
			state = QStringLiteral("Pushing…");
		} else if (view.Running == application::local_changes_operation::checking) {
			state = QStringLiteral("Checking GitHub…");
		}
		auto dotPalette = PushDot->palette();
		dotPalette.setColor(QPalette::WindowText, dot);
		PushDot->setPalette(dotPalette);
		auto statePalette = PushState->palette();
		statePalette.setColor(QPalette::WindowText, dot == colours.color(QPalette::Mid) || dot == green(colours) ? colours.color(QPalette::WindowText) : dot);
		PushState->setPalette(statePalette);
		PushState->setText(state);
		Push->setText(label);
		const auto idle = view.Running == application::local_changes_operation::none;
		Push->setEnabled(view.Running == application::local_changes_operation::pushing || (idle && view.Active));
		Push->setDefault(idle && view.Push.Default);
		Push->setToolTip(view.Push.Offered ? QStringLiteral("Push the branch to GitHub.") : QStringLiteral("Read GitHub's branch again."));
		Command->setText(view.Command ? text_of(view.Command->Text) : QString{ });
		Command->setVisible(view.Command.has_value());
		auto banner = QString{ };
		if (view.Failure) {
			if (view.Failure->RefusedBeforeRunning) {
				banner = QStringLiteral("Push refused: %1 has commits this branch does not. Nothing was sent; nothing is forced.").arg(against(view));
			} else {
				const auto what = view.Failure->Operation == application::local_changes_operation::committing ? QStringLiteral("Commit stopped")
					: view.Failure->Operation == application::local_changes_operation::pushing                ? QStringLiteral("Push stopped")
					: view.Failure->Operation == application::local_changes_operation::staging                ? QStringLiteral("Staging stopped")
					: view.Failure->Operation == application::local_changes_operation::checking               ? QStringLiteral("GitHub could not be read")
																											  : QStringLiteral("Git could not be read");
				auto       said = text_of(view.Failure->Message).trimmed();
				if (said.size() > 600) {
					said = said.left(600) + QStringLiteral("…");
				}
				banner = said.isEmpty() ? what + QLatin1Char{ '.' } : QStringLiteral("%1: %2").arg(what, said);
			}
		} else if (view.Push.State == core::local_push_state::behind || view.Push.State == core::local_push_state::diverged || view.Push.State == core::local_push_state::unknown_tip) {
			// Refused before anything runs: no control here forces.
			banner = QStringLiteral("Push refused: %1 has commits this branch does not. Nothing was sent; nothing is forced.").arg(against(view));
		} else if (view.Declined) {
			banner = QStringLiteral("Nothing ran. To commit or push yourself:\n%1").arg(text_of([&] {
				auto joined = std::string{ };
				for (const auto& line : view.Commands) {
					joined += (joined.empty() ? "" : "\n") + line;
				}
				return joined;
			}()));
		}
		auto       bannerPalette = Banner->palette();
		const auto refused       = view.Push.State == core::local_push_state::behind || view.Push.State == core::local_push_state::diverged || view.Push.State == core::local_push_state::unknown_tip;
		bannerPalette.setColor(QPalette::WindowText, view.Failure || refused ? red(colours) : colours.color(QPalette::WindowText));
		Banner->setPalette(bannerPalette);
		Banner->setText(banner);
		Banner->setVisible(!banner.isEmpty());
	}

	auto local_changes_panel::selected(const QTreeWidget& aTree) const -> std::vector<std::string>
	{
		auto paths = std::vector<std::string>{ };
		for (const auto* const item : aTree.selectedItems()) {
			if (const auto path = item->data(0, path_role).toString(); !path.isEmpty()) {
				paths.push_back(utf8(path));
			}
		}
		if (paths.empty() && aTree.currentItem() != nullptr && !aTree.currentItem()->data(0, path_role).toString().isEmpty()) {
			paths.push_back(utf8(aTree.currentItem()->data(0, path_role).toString()));
		}
		return paths;
	}

	void local_changes_panel::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange) {
			restyle();
		}
	}

	void local_changes_panel::restyle()
	{
		if (Suggest != nullptr) {
			wear_agent_accent(*Suggest, palette());
			Suggest->setIcon(enhance_icon(Suggest->palette()));
		}
		if (Unstaged != nullptr && !Presenting) {
			present_lists();
			present_push();
		}
	}
}
