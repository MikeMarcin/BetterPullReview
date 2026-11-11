#include "bpr/qt/agent_setup_settings.hpp"

#include "bpr/qt/agent_run_settings_picker.hpp"
#include "bpr/qt/agent_setup_view.hpp"
#include "bpr/qt/disclosure_button.hpp"
#include "bpr/qt/quiet_push_button.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto text_of(const std::string_view aText) -> QString { return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())); }

		[[nodiscard]] auto heading(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			auto        bold  = label->font();
			bold.setWeight(QFont::DemiBold);
			label->setFont(bold);
			return label;
		}

		[[nodiscard]] auto note(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setWordWrap(true);
			label->setTextFormat(Qt::PlainText);
			label->setForegroundRole(QPalette::PlaceholderText);
			return label;
		}

		[[nodiscard]] auto refusal_words(const core::agent_wrapper_refusal aRefusal) -> QString { return text_of(core::describe(aRefusal)); }

		[[nodiscard]] auto skill_refusal_words(const core::review_skill_refusal aRefusal) -> QString
		{
			switch (aRefusal) {
			case core::review_skill_refusal::too_large:       return QObject::tr("The skill is larger than 64 KiB. Shorten it, then save.");
			case core::review_skill_refusal::not_text:        return QObject::tr("The skill is not UTF-8 text.");
			case core::review_skill_refusal::no_front_matter: return QObject::tr("The skill needs its front matter: a --- block at the top with its name and description.");
			case core::review_skill_refusal::name_changed:    return QObject::tr("The skill's name must stay bpr-pr-review.");
			case core::review_skill_refusal::no_description:  return QObject::tr("The skill needs a description in its front matter.");
			}
			return { };
		}

		/// [[BPR-2051]]. The roster row's roles.
		enum roster_role : int {
			ring_role = Qt::UserRole + 1,
			name_role,
			line_role,
			state_role,
			failed_role,
		};

		[[nodiscard]] auto mixed(const QColor& aFrom, const QColor& aTo, const qreal aShare) -> QColor
		{
			const auto keep = 1.0 - aShare;
			return QColor::fromRgbF(static_cast<float>(aFrom.redF() * keep + aTo.redF() * aShare), static_cast<float>(aFrom.greenF() * keep + aTo.greenF() * aShare),
									static_cast<float>(aFrom.blueF() * keep + aTo.blueF() * aShare));
		}

		/// \brief [[BPR-2051]], owner decision 2026-11-11 (mockup "A ·
		/// Roster"). One added agent: its check ring, its name over its model
		/// and effort (or what to do), its state at the right and a chevron,
		/// for the sheet a click opens. Native selection and focus are drawn
		/// first; every colour is the palette's or the setup's own.
		class roster_delegate final : public QStyledItemDelegate
		{
		public:
			using QStyledItemDelegate::QStyledItemDelegate;

			[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& /*aIndex*/) const -> QSize override
			{
				const auto metrics = QFontMetrics{ aOption.font };
				return { 320, metrics.height() * 2 + 22 };
			}

			void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
			{
				auto option = aOption;
				initStyleOption(&option, aIndex);
				option.text.clear();
				const auto* const widget = option.widget;
				auto* const       style  = widget != nullptr ? widget->style() : QApplication::style();
				style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, aPainter, widget);
				const auto selected = (option.state & QStyle::State_Selected) != 0;
				const auto palette  = option.palette;
				const auto ink      = palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
				const auto quiet    = selected ? ink : palette.color(QPalette::PlaceholderText);
				const auto rect     = option.rect;
				aPainter->save();
				aPainter->setRenderHint(QPainter::Antialiasing, true);
				// The row's rule, under every row but the last.
				if (aIndex.row() + 1 < aIndex.model()->rowCount()) {
					aPainter->setPen(QPen{ mixed(palette.color(QPalette::Base), palette.color(QPalette::Text), 0.12), 1.0 });
					aPainter->drawLine(QPointF{ rect.left() + 48.0, rect.bottom() + 0.5 }, QPointF{ static_cast<qreal>(rect.right()), rect.bottom() + 0.5 });
				}
				// The ring: passed, failed, checking or not checked.
				const auto state = static_cast<core::setup_check_state>(aIndex.data(ring_role).toInt());
				const auto ring  = QRectF{ rect.left() + 14.0, rect.center().y() - 10.0, 20.0, 20.0 };
				auto       mark  = quiet;
				switch (state) {
				case core::setup_check_state::passed:  mark = setup_passed_colour(palette); break;
				case core::setup_check_state::failed:  mark = setup_failed_colour(palette); break;
				case core::setup_check_state::running: mark = palette.color(QPalette::Highlight); break;
				case core::setup_check_state::not_run: break;
				}
				if (selected) {
					mark = ink;
				}
				aPainter->setPen(QPen{ mark, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
				aPainter->setBrush(Qt::NoBrush);
				if (state == core::setup_check_state::running) {
					aPainter->drawArc(ring.adjusted(1, 1, -1, -1), 90 * 16, -270 * 16);
				} else {
					aPainter->drawEllipse(ring.adjusted(1, 1, -1, -1));
				}
				const auto c = ring.center();
				if (state == core::setup_check_state::passed) {
					aPainter->drawPolyline(std::array{ QPointF{ c.x() - 4.5, c.y() + 0.2 }, QPointF{ c.x() - 1.4, c.y() + 3.2 }, QPointF{ c.x() + 4.6, c.y() - 3.4 } }.data(), 3);
				} else if (state == core::setup_check_state::failed) {
					aPainter->drawLine(QPointF{ c.x() - 3.5, c.y() - 3.5 }, QPointF{ c.x() + 3.5, c.y() + 3.5 });
					aPainter->drawLine(QPointF{ c.x() + 3.5, c.y() - 3.5 }, QPointF{ c.x() - 3.5, c.y() + 3.5 });
				}
				// The chevron, then the state, right-aligned.
				const auto chevron = QPointF{ rect.right() - 16.0, static_cast<qreal>(rect.center().y()) };
				aPainter->setPen(QPen{ quiet, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
				aPainter->drawPolyline(std::array{ chevron + QPointF{ -2.5, -5.0 }, chevron + QPointF{ 2.5, 0.0 }, chevron + QPointF{ -2.5, 5.0 } }.data(), 3);
				const auto metrics  = QFontMetrics{ option.font };
				const auto said     = aIndex.data(state_role).toString();
				const auto failed   = aIndex.data(failed_role).toBool();
				const auto stateW   = std::min(metrics.horizontalAdvance(said) + 2, rect.width() / 3);
				const auto stateBox = QRect{ rect.right() - 30 - stateW, rect.top(), stateW, rect.height() };
				aPainter->setFont(option.font);
				aPainter->setPen(failed && !selected ? setup_failed_colour(palette) : quiet);
				aPainter->drawText(stateBox, Qt::AlignRight | Qt::AlignVCenter, metrics.elidedText(said, Qt::ElideRight, stateW));
				// The name over its line.
				const auto left  = rect.left() + 48;
				const auto right = stateBox.left() - 12;
				auto       bold  = option.font;
				bold.setWeight(QFont::DemiBold);
				const auto top = rect.top() + 9;
				aPainter->setFont(bold);
				aPainter->setPen(ink);
				aPainter->drawText(QRect{ left, top, std::max(0, right - left), metrics.height() }, Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics{ bold }.elidedText(aIndex.data(name_role).toString(), Qt::ElideRight, std::max(0, right - left)));
				aPainter->setFont(option.font);
				aPainter->setPen(quiet);
				aPainter->drawText(QRect{ left, top + metrics.height() + 2, std::max(0, right - left), metrics.height() }, Qt::AlignLeft | Qt::AlignVCenter, metrics.elidedText(aIndex.data(line_role).toString(), Qt::ElideRight, std::max(0, right - left)));
				aPainter->restore();
			}
		};
	} // namespace

	agent_programs_panel::agent_programs_panel(QWidget* aParent)
		: QWidget{ aParent }
		, Editor{ default_agent_entry_editor() }
	{
		setObjectName(QStringLiteral("agentProgramsPanel"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		// [[BPR-2051]], owner decision 2026-11-11 (mockup "A · Roster"): the
		// agents as a roster. Each row says its last check by its ring and in
		// words, and opens its sheet; Check all and Add agent… follow it.
		auto* const top = new QHBoxLayout{ };
		top->setSpacing(10);
		top->addWidget(heading(tr("Agents"), this), 0, Qt::AlignBaseline);
		auto* const lead = note(tr("A pull request picks one of these in its footer. Drag to reorder."), this);
		lead->setObjectName(QStringLiteral("settingsAgentsLead"));
		top->addWidget(lead, 1, Qt::AlignBaseline);
		column->addLayout(top);
		List = new QListWidget{ this };
		List->setObjectName(QStringLiteral("settingsAgentList"));
		List->setAccessibleName(tr("Agents"));
		List->setItemDelegate(new roster_delegate{ List });
		List->setUniformItemSizes(true);
		List->setFrameShape(QFrame::StyledPanel);
		List->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		List->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		List->setDragDropMode(QAbstractItemView::InternalMove);
		List->setDefaultDropAction(Qt::MoveAction);
		List->setContextMenuPolicy(Qt::CustomContextMenu);
		List->setMouseTracking(true);
		connect(List, &QListWidget::currentRowChanged, this, [this] { settle_buttons(); });
		// A row opens its sheet; its menu holds the rest.
		connect(List, &QListWidget::itemClicked, this, [this] { EditButton->click(); });
		connect(List, &QListWidget::itemActivated, this, [this] { EditButton->click(); });
		connect(List, &QWidget::customContextMenuRequested, this, [this](const QPoint& aAt) { open_row_menu(aAt); });
		// Dragged to a new place: moved there one step at a time, as Move up
		// and Move down do, so the saved order is the one shown.
		connect(List->model(), &QAbstractItemModel::rowsMoved, this, [this](const QModelIndex&, const int aStart, int, const QModelIndex&, const int aRow) {
			const auto destination = aRow > aStart ? aRow - 1 : aRow;
			if (aStart < 0 || static_cast<std::size_t>(aStart) >= Agents.size() || destination == aStart || !Dispatch.Move) {
				return;
			}
			const auto id    = Agents[static_cast<std::size_t>(aStart)].Id;
			const auto delta = destination > aStart ? 1 : -1;
			for (auto at = aStart; at != destination; at += delta) {
				Dispatch.Move(id, delta);
			}
		});
		column->addWidget(List);
		auto* const row = new QHBoxLayout{ };
		row->setSpacing(8);
		row->addStretch(1);
		AddButton = new quiet_push_button{ tr("Add agent…"), this };
		AddButton->setObjectName(QStringLiteral("settingsAddAgent"));
		AddButton->setMenu(new QMenu{ AddButton });
		connect(AddButton->menu(), &QMenu::aboutToShow, this, [this] {
			auto* const menu = AddButton->menu();
			menu->clear();
			// Claude Code and Codex when found here and not added yet.
			for (const auto preset : { core::setup_agent::claude_code, core::setup_agent::codex }) {
				const auto found = std::ranges::find(Found, preset, &core::agent_detection::Agent);
				if (found == Found.end() || core::find_preset_entry(Agents, preset) != nullptr) {
					continue;
				}
				auto* const item = menu->addAction(tr("%1 (found at %2)").arg(text_of(core::plain_name(preset)), QDir::toNativeSeparators(text_of(found->Path))));
				item->setObjectName(preset == core::setup_agent::claude_code ? QStringLiteral("settingsAddClaude") : QStringLiteral("settingsAddCodex"));
				connect(item, &QAction::triggered, this, [this, preset] { add_agent(preset); });
			}
			if (!menu->actions().isEmpty()) {
				menu->addSeparator();
			}
			auto* const custom = menu->addAction(tr("Custom agent…"));
			custom->setObjectName(QStringLiteral("settingsAddCustom"));
			connect(custom, &QAction::triggered, this, [this] { add_agent(std::nullopt); });
		});
		// Edit…, Remove, Move up and Move down are the rows' own: a click
		// opens the sheet, the row's menu and a drag do the rest.
		EditButton = new quiet_push_button{ tr("Edit…"), this };
		EditButton->setObjectName(QStringLiteral("settingsEditAgent"));
		EditButton->hide();
		connect(EditButton, &QPushButton::clicked, this, [this] {
			const auto* const entry = selected();
			if (entry == nullptr || !Editor || !Dispatch.Update) {
				return;
			}
			if (auto edited = Editor(*entry, this)) {
				Dispatch.Update(std::move(*edited), [this](const std::optional<core::agent_entry_refusal> aRefusal) {
					say(aRefusal ? text_of(core::describe(*aRefusal)) : tr("Saved."));
				});
			}
		});
		RemoveButton = new quiet_push_button{ tr("Remove"), this };
		RemoveButton->setObjectName(QStringLiteral("settingsRemoveAgent"));
		RemoveButton->hide();
		connect(RemoveButton, &QPushButton::clicked, this, [this] {
			if (const auto* const entry = selected(); entry != nullptr && Dispatch.Remove) {
				Dispatch.Remove(entry->Id);
			}
		});
		UpButton = new quiet_push_button{ tr("Move up"), this };
		UpButton->setObjectName(QStringLiteral("settingsAgentUp"));
		UpButton->hide();
		connect(UpButton, &QPushButton::clicked, this, [this] {
			if (const auto* const entry = selected(); entry != nullptr && Dispatch.Move) {
				Dispatch.Move(entry->Id, -1);
			}
		});
		DownButton = new quiet_push_button{ tr("Move down"), this };
		DownButton->setObjectName(QStringLiteral("settingsAgentDown"));
		DownButton->hide();
		connect(DownButton, &QPushButton::clicked, this, [this] {
			if (const auto* const entry = selected(); entry != nullptr && Dispatch.Move) {
				Dispatch.Move(entry->Id, 1);
			}
		});
		build_check(*column, *row);
		row->addWidget(AddButton);
		column->addLayout(row);
		Status = note({ }, this);
		Status->setObjectName(QStringLiteral("settingsAgentStatus"));
		Status->hide();
		column->addWidget(Status);
		column->addWidget(CheckRows);
		// Owner decision 2026-10-23 ("I guess we can keep a timeout as an
		// option, but i wouldnt want it on by default"): off unless chosen.
		column->addSpacing(8);
		auto* const limitRow = new QHBoxLayout{ };
		RunLimitOn           = new QCheckBox{ tr("Stop a run after"), this };
		RunLimitOn->setObjectName(QStringLiteral("settingsRunLimitOn"));
		RunLimitOn->setToolTip(tr("Off, a run goes on until the agent finishes or you press Stop."));
		limitRow->addWidget(RunLimitOn);
		limitRow->addStretch(1);
		RunLimit = new QSpinBox{ this };
		RunLimit->setObjectName(QStringLiteral("settingsRunLimitMinutes"));
		RunLimit->setRange(1, static_cast<int>(application::maximum_run_limit_minutes));
		RunLimit->setValue(10);
		RunLimit->setSuffix(tr(" min"));
		RunLimit->setEnabled(false);
		limitRow->addWidget(RunLimit);
		column->addLayout(limitRow);
		connect(RunLimitOn, &QCheckBox::toggled, this, [this](const bool aOn) {
			RunLimit->setEnabled(aOn);
			send_run_limit();
		});
		connect(RunLimit, &QSpinBox::editingFinished, this, [this] { send_run_limit(); });
		// RFC-BPR-0104 as amended 2026-11-11 (owner look e97c6a3d: "we need to
		// make sure that our app doesn't just consume disk space without
		// bounds"). The scratch folders' limit, how long an unused one is
		// kept, what they hold and Clear now. The explanations are tooltips.
		auto* const scratchRow = new QHBoxLayout{ };
		auto* const limitSays  = new QLabel{ tr("Scratch folder limit"), this };
		limitSays->setToolTip(tr("The most an agent's scratch folder may hold. A question that starts with it full may run what is there but not write; one that fills it is stopped."));
		scratchRow->addWidget(limitSays);
		scratchRow->addStretch(1);
		ScratchLimit = new QSpinBox{ this };
		ScratchLimit->setObjectName(QStringLiteral("settingsScratchLimit"));
		ScratchLimit->setRange(static_cast<int>(core::minimum_agent_scratch_limit_mib), static_cast<int>(core::maximum_agent_scratch_limit_mib));
		ScratchLimit->setValue(static_cast<int>(core::default_agent_scratch_limit_mib));
		ScratchLimit->setSuffix(tr(" MiB"));
		ScratchLimit->setToolTip(limitSays->toolTip());
		limitSays->setBuddy(ScratchLimit);
		scratchRow->addWidget(ScratchLimit);
		column->addLayout(scratchRow);
		auto* const keepRow = new QHBoxLayout{ };
		ScratchKeepOn       = new QCheckBox{ tr("Remove scratch folders unused for"), this };
		ScratchKeepOn->setObjectName(QStringLiteral("settingsScratchKeepOn"));
		ScratchKeepOn->setToolTip(tr("Checked when BetterPullReview starts. Off, a folder stays until you clear it."));
		keepRow->addWidget(ScratchKeepOn);
		keepRow->addStretch(1);
		ScratchKeepDays = new QSpinBox{ this };
		ScratchKeepDays->setObjectName(QStringLiteral("settingsScratchKeepDays"));
		ScratchKeepDays->setRange(1, static_cast<int>(core::maximum_agent_scratch_keep_days));
		ScratchKeepDays->setValue(static_cast<int>(core::default_agent_scratch_keep_days));
		ScratchKeepDays->setSuffix(tr(" days"));
		keepRow->addWidget(ScratchKeepDays);
		column->addLayout(keepRow);
		auto* const usageRow = new QHBoxLayout{ };
		ScratchUsage         = note({ }, this);
		ScratchUsage->setObjectName(QStringLiteral("settingsScratchUsage"));
		usageRow->addWidget(ScratchUsage, 1);
		ScratchClear = new quiet_push_button{ tr("Clear scratch folders"), this };
		ScratchClear->setObjectName(QStringLiteral("settingsScratchClear"));
		ScratchClear->setToolTip(tr("Remove every agent's scratch folder now, except those of this pull request's agents. Not while a question runs. The agents' conversations stay."));
		usageRow->addWidget(ScratchClear);
		column->addLayout(usageRow);
		connect(ScratchLimit, &QSpinBox::editingFinished, this, [this] { send_scratch_policy(); });
		connect(ScratchKeepDays, &QSpinBox::editingFinished, this, [this] { send_scratch_policy(); });
		connect(ScratchKeepOn, &QCheckBox::toggled, this, [this](const bool aOn) {
			ScratchKeepDays->setEnabled(aOn);
			send_scratch_policy();
		});
		connect(ScratchClear, &QPushButton::clicked, this, [this] {
			if (Dispatch.ClearScratch) {
				Dispatch.ClearScratch();
			}
		});
		// Owner look v006: the setup is not entered again except to start
		// afresh; Reset agents… asks first, then clears every agent setting.
		auto* const resetRow  = new QHBoxLayout{ };
		auto* const resetSays = new QLabel{ tr("Remove every agent and set up again"), this };
		resetRow->addWidget(resetSays, 1);
		auto* const reset = new quiet_push_button{ tr("Reset agents…"), this };
		reset->setObjectName(QStringLiteral("settingsResetAgents"));
		reset->setToolTip(tr("Remove every agent and its settings, the clone folders and linked clones, and the edited review skill, then set up again"));
		connect(reset, &QPushButton::clicked, this, [this] {
			if (Dispatch.Reset) {
				Dispatch.Reset();
			}
		});
		resetRow->addWidget(reset);
		column->addLayout(resetRow);
		settle_buttons();
	}

	void agent_programs_panel::set_dispatch(dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	void agent_programs_panel::send_run_limit()
	{
		if (Dispatch.SetRunLimit) {
			static_cast<void>(Dispatch.SetRunLimit(RunLimitOn->isChecked() ? static_cast<std::uint32_t>(RunLimit->value()) : 0U));
		}
	}

	void agent_programs_panel::send_scratch_policy()
	{
		if (Dispatch.SetScratchPolicy) {
			static_cast<void>(Dispatch.SetScratchPolicy(static_cast<std::uint32_t>(ScratchLimit->value()), ScratchKeepOn->isChecked() ? static_cast<std::uint32_t>(ScratchKeepDays->value()) : 0U));
		}
	}

	void agent_programs_panel::present_scratch_usage(const std::optional<std::pair<std::uint64_t, std::size_t>> aUsage, const QString& aSaid)
	{
		auto words = QString{ };
		if (aUsage) {
			const auto size = QLocale{ }.formattedDataSize(static_cast<qint64>(aUsage->first));
			words           = aUsage->second == 0U ? tr("No scratch folders.")
				: aUsage->second == 1U             ? tr("Scratch folders hold %1 in 1 folder.").arg(size)
												   : tr("Scratch folders hold %1 in %2 folders.").arg(size).arg(aUsage->second);
		}
		if (!aSaid.isEmpty()) {
			words += (words.isEmpty() ? QString{ } : QStringLiteral(" ")) + aSaid;
		}
		ScratchUsage->setText(words);
		ScratchClear->setEnabled(!aUsage || aUsage->second != 0U);
	}

	void agent_programs_panel::set_editor(agent_entry_editor aEditor) { Editor = std::move(aEditor); }

	void agent_programs_panel::add_agent(const std::optional<core::setup_agent> aPreset)
	{
		if (!Editor || !Dispatch.Add) {
			return;
		}
		auto start = aPreset ? core::preset_agent_entry(*aPreset, core::codex_protocol_choice::app_server, { }) : core::agent_entry{ };
		if (auto chosen = Editor(start, this)) {
			Dispatch.Add(std::move(*chosen), [this](const std::optional<core::agent_entry_refusal> aRefusal) {
				say(aRefusal ? text_of(core::describe(*aRefusal)) : tr("Added."));
			});
		}
	}

	auto agent_programs_panel::selected() const -> const core::agent_entry*
	{
		const auto row = List->currentRow();
		return row < 0 || static_cast<std::size_t>(row) >= Agents.size() ? nullptr : &Agents[static_cast<std::size_t>(row)];
	}

	void agent_programs_panel::settle_buttons()
	{
		const auto row = List->currentRow();
		EditButton->setEnabled(row >= 0);
		RemoveButton->setEnabled(row >= 0);
		UpButton->setEnabled(row > 0);
		DownButton->setEnabled(row >= 0 && row + 1 < List->count());
	}

	void agent_programs_panel::present(const application::agent_setup_preferences& aPreferences, std::vector<core::agent_detection> aFound)
	{
		Found = std::move(aFound);
		{
			const auto blockToggle  = QSignalBlocker{ RunLimitOn };
			const auto blockMinutes = QSignalBlocker{ RunLimit };
			RunLimitOn->setChecked(aPreferences.RunLimitMinutes != 0U);
			RunLimit->setEnabled(aPreferences.RunLimitMinutes != 0U);
			if (aPreferences.RunLimitMinutes != 0U && !RunLimit->hasFocus()) {
				RunLimit->setValue(static_cast<int>(aPreferences.RunLimitMinutes));
			}
		}
		{
			const auto blockLimit  = QSignalBlocker{ ScratchLimit };
			const auto blockToggle = QSignalBlocker{ ScratchKeepOn };
			const auto blockDays   = QSignalBlocker{ ScratchKeepDays };
			if (!ScratchLimit->hasFocus()) {
				ScratchLimit->setValue(static_cast<int>(aPreferences.ScratchLimitMiB));
			}
			ScratchKeepOn->setChecked(aPreferences.ScratchKeepDays != 0U);
			ScratchKeepDays->setEnabled(aPreferences.ScratchKeepDays != 0U);
			if (aPreferences.ScratchKeepDays != 0U && !ScratchKeepDays->hasFocus()) {
				ScratchKeepDays->setValue(static_cast<int>(aPreferences.ScratchKeepDays));
			}
		}
		const auto keptId = selected() != nullptr ? selected()->Id : 0U;
		Agents            = aPreferences.Agents;
		const auto block  = QSignalBlocker{ List };
		List->clear();
		auto current = -1;
		for (const auto& entry : Agents) {
			auto line = text_of(entry.Name) + QStringLiteral(" — ") + QDir::toNativeSeparators(entry.Program.empty() ? tr("its own program") : text_of(entry.Program));
			for (const auto& argument : entry.Arguments) {
				line += QStringLiteral(" ") + text_of(argument);
			}
			if (!entry.Model.empty() || !entry.Effort.empty()) {
				line += QStringLiteral(" · ") + (entry.Model.empty() ? tr("default model") : text_of(entry.Model)) + (entry.Effort.empty() ? QString{ } : QStringLiteral(" · ") + text_of(entry.Effort));
			}
			auto* const item = new QListWidgetItem{ line, List };
			item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(entry.Id));
			if (entry.Id == keptId) {
				current = List->count() - 1;
			}
		}
		List->setCurrentRow(current);
		Kept = aPreferences.EntryChecks;
		present_roster();
		settle_buttons();
	}

	void agent_programs_panel::say(const QString& aText)
	{
		// The roster's answer line takes room only while it says something.
		Status->setText(aText);
		Status->setVisible(!aText.isEmpty());
	}

	void agent_programs_panel::present_roster()
	{
		for (int row = 0; row < List->count() && static_cast<std::size_t>(row) < Agents.size(); ++row) {
			const auto& entry = Agents[static_cast<std::size_t>(row)];
			auto* const item  = List->item(row);
			const auto  found = std::ranges::find(Kept, entry.Id, &application::agent_entry_check_record::Id);
			const auto  kept  = found == Kept.end() ? std::optional<core::setup_check>{ } : std::optional{ found->Check };
			const auto  agent = entry.Preset ? *entry.Preset : core::setup_agent_of(entry.Protocol);
			auto        ring  = kept ? kept->State : core::setup_check_state::not_run;
			if (Checking == std::optional{ entry.Id }) {
				ring = core::setup_check_state::running;
			}
			// Under the name: what it runs with, or what to do.
			auto line = QStringList{ };
			if (entry.Preset || !entry.Model.empty() || !entry.Effort.empty()) {
				line << (entry.Model.empty() ? tr("default model") : agent_model_display_name(text_of(entry.Model)));
				if (!entry.Effort.empty()) {
					line << tr("%1 effort").arg(text_of(entry.Effort));
				}
			} else {
				line << QDir::toNativeSeparators(entry.Program.empty() ? tr("its own program") : text_of(entry.Program));
			}
			auto under = line.join(QStringLiteral(" · "));
			auto said  = tr("Not checked");
			auto bad   = false;
			if (ring == core::setup_check_state::running) {
				said = tr("Checking…");
			} else if (kept && kept->State == core::setup_check_state::passed) {
				said = entry.Preset && agent != core::setup_agent::plain_text_harness ? (kept->Version.empty() ? tr("Signed in") : tr("Signed in, version %1").arg(text_of(kept->Version)))
																					  : (kept->Version.empty() ? tr("Works") : tr("Works, version %1").arg(text_of(kept->Version)));
			} else if (kept && kept->State == core::setup_check_state::failed) {
				bad              = true;
				const auto which = kept->Failure.value_or(core::agent_setup_failure::status_unreadable);
				said             = which == core::agent_setup_failure::not_signed_in ? tr("Not signed in")
					: which == core::agent_setup_failure::agent_not_found            ? tr("Not found")
					: which == core::agent_setup_failure::timed_out                  ? tr("Didn't answer")
																					 : tr("Check failed");
				if (which == core::agent_setup_failure::not_signed_in && !sign_in_command(agent).isEmpty()) {
					under = tr("Run %1 in a terminal, then check again.").arg(sign_in_command(agent));
				}
			}
			item->setData(ring_role, static_cast<int>(ring));
			item->setData(name_role, text_of(entry.Name));
			item->setData(line_role, under);
			item->setData(state_role, said);
			item->setData(failed_role, bad);
			item->setData(Qt::AccessibleTextRole, tr("%1, %2. %3").arg(text_of(entry.Name), said, under));
			item->setToolTip(item->text());
		}
		// As tall as its rows: the roster never scrolls inside the page.
		const auto rows = std::max(1, List->count());
		const auto each = List->sizeHintForRow(0) > 0 ? List->sizeHintForRow(0) : List->fontMetrics().height() * 2 + 22;
		List->setFixedHeight(rows * each + 2 * List->frameWidth());
	}

	auto agent_programs_panel::roster_row_text(const int aRow) const -> QString
	{
		const auto* const item = List->item(aRow);
		return item == nullptr ? QString{ } : item->data(name_role).toString() + QLatin1Char{ '\n' } + item->data(line_role).toString() + QLatin1Char{ '\n' } + item->data(state_role).toString();
	}

	void agent_programs_panel::open_row_menu(const QPoint& aAt)
	{
		auto* const item = List->itemAt(aAt);
		if (item == nullptr) {
			return;
		}
		List->setCurrentItem(item);
		const auto* const entry = selected();
		if (entry == nullptr) {
			return;
		}
		auto menu = QMenu{ this };
		menu.setObjectName(QStringLiteral("settingsAgentRowMenu"));
		menu.addAction(tr("Edit…"), EditButton, &QPushButton::click);
		auto* const check = menu.addAction(tr("Check %1").arg(text_of(entry->Name)), this, [this, id = entry->Id] {
			if (Dispatch.CheckEntry) {
				Dispatch.CheckEntry(id);
			}
		});
		check->setEnabled(static_cast<bool>(Dispatch.CheckEntry) && !Checking);
		menu.addSeparator();
		menu.addAction(tr("Move up"), UpButton, &QPushButton::click)->setEnabled(UpButton->isEnabled());
		menu.addAction(tr("Move down"), DownButton, &QPushButton::click)->setEnabled(DownButton->isEnabled());
		menu.addSeparator();
		menu.addAction(tr("Remove %1").arg(text_of(entry->Name)), RemoveButton, &QPushButton::click);
		menu.exec(List->viewport()->mapToGlobal(aAt));
	}

	void agent_programs_panel::build_check(QVBoxLayout& /*aColumn*/, QHBoxLayout& aRow)
	{
		// [[BPR-2051]]. Stop and the check's progress, then Check all, on the
		// roster's own row; what the check does is its tooltip.
		StopCheckButton = new quiet_push_button{ tr("Stop"), this };
		StopCheckButton->setObjectName(QStringLiteral("settingsStopAgentCheck"));
		StopCheckButton->hide();
		connect(StopCheckButton, &QPushButton::clicked, this, [this] {
			if (Dispatch.StopCheck) {
				Dispatch.StopCheck();
			}
		});
		auto* const progress = new QProgressBar{ this };
		progress->setObjectName(QStringLiteral("settingsAgentCheckProgress"));
		progress->setRange(0, 0);
		progress->setTextVisible(false);
		progress->setMaximumWidth(64);
		progress->hide();
		CheckProgress = progress;
		aRow.addWidget(progress, 0, Qt::AlignVCenter);
		aRow.addWidget(StopCheckButton, 0, Qt::AlignVCenter);
		CheckButton = new quiet_push_button{ tr("Check all"), this };
		CheckButton->setObjectName(QStringLiteral("settingsCheckAgent"));
		connect(CheckButton, &QPushButton::clicked, this, [this] {
			if (Dispatch.CheckAll) {
				Dispatch.CheckAll();
			} else if (Dispatch.Check) {
				Dispatch.Check();
			}
		});
		aRow.addWidget(CheckButton, 0, Qt::AlignVCenter);
		// What the check does, or why it cannot run: the button's tooltip and
		// the check's accessible description, not a line on the page.
		CheckNote = note({ }, this);
		CheckNote->setObjectName(QStringLiteral("settingsAgentCheckNote"));
		CheckNote->hide();

		// The rows, as Check it works draws them: a mark and what it proves.
		CheckRows        = new QWidget{ this };
		auto* const rows = new QVBoxLayout{ CheckRows };
		rows->setContentsMargins(4, 2, 0, 0);
		rows->setSpacing(4);
		const auto line = [this, rows](QLabel*& aMark, QLabel*& aText, const QString& aName) {
			auto* const holder = new QWidget{ CheckRows };
			auto* const across = new QHBoxLayout{ holder };
			across->setContentsMargins(0, 0, 0, 0);
			across->setSpacing(8);
			aMark = new QLabel{ holder };
			aMark->setObjectName(aName + QStringLiteral("Mark"));
			aMark->setAlignment(Qt::AlignLeft | Qt::AlignTop);
			aMark->setMinimumWidth(aMark->fontMetrics().horizontalAdvance(QChar{ 0x2713 }) + 2);
			across->addWidget(aMark, 0, Qt::AlignTop);
			aText = new QLabel{ holder };
			aText->setObjectName(aName);
			aText->setWordWrap(true);
			aText->setTextFormat(Qt::PlainText);
			aText->setAlignment(Qt::AlignLeft | Qt::AlignTop);
			across->addWidget(aText, 1);
			rows->addWidget(holder);
			return holder;
		};
		static_cast<void>(line(AgentMark, AgentRow, QStringLiteral("settingsAgentCheckRow")));
		WorkspaceLine = line(WorkspaceMark, WorkspaceRow, QStringLiteral("settingsWorkspaceCheckRow"));
		// What failed, what to do and the details sit under the rows' words.
		auto* const follow = new QWidget{ CheckRows };
		auto* const under  = new QVBoxLayout{ follow };
		under->setContentsMargins(AgentMark->minimumWidth() + 8, 0, 0, 0);
		under->setSpacing(4);
		rows->addWidget(follow);
		CheckFailure = new QLabel{ CheckRows };
		CheckFailure->setObjectName(QStringLiteral("settingsAgentCheckFailure"));
		CheckFailure->setWordWrap(true);
		CheckFailure->setTextFormat(Qt::PlainText);
		under->addWidget(CheckFailure);
		SignInLine             = new QWidget{ CheckRows };
		auto* const signInLine = new QHBoxLayout{ SignInLine };
		signInLine->setContentsMargins(0, 0, 0, 0);
		SignInCommand = new QLabel{ SignInLine };
		SignInCommand->setObjectName(QStringLiteral("settingsAgentSignInCommand"));
		SignInCommand->setTextInteractionFlags(Qt::TextSelectableByMouse);
		SignInCommand->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
		signInLine->addWidget(SignInCommand);
		auto* const copy = new quiet_push_button{ tr("Copy"), SignInLine };
		copy->setObjectName(QStringLiteral("settingsAgentCopyCommand"));
		connect(copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(SignInCommand->text()); });
		signInLine->addWidget(copy);
		signInLine->addStretch(1);
		under->addWidget(SignInLine);
		WorkspaceReason = new QLabel{ CheckRows };
		WorkspaceReason->setObjectName(QStringLiteral("settingsWorkspaceCheckFailure"));
		WorkspaceReason->setWordWrap(true);
		WorkspaceReason->setTextFormat(Qt::PlainText);
		under->addWidget(WorkspaceReason);
		auto* const toggle = new disclosure_button{ CheckRows };
		toggle->setObjectName(QStringLiteral("settingsAgentCheckDetails"));
		toggle->setText(tr("Show details"));
		toggle->setAccessibleName(tr("Show details"));
		DetailsToggle = toggle;
		under->addWidget(toggle, 0, Qt::AlignLeft);
		DetailsText = new QPlainTextEdit{ CheckRows };
		DetailsText->setObjectName(QStringLiteral("settingsAgentCheckDetailsText"));
		DetailsText->setAccessibleName(tr("Check details"));
		DetailsText->setReadOnly(true);
		DetailsText->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
		DetailsText->setMinimumHeight(96);
		DetailsText->setMaximumHeight(144);
		DetailsText->hide();
		under->addWidget(DetailsText);
		connect(toggle, &QToolButton::toggled, DetailsText, &QWidget::setVisible);
		CheckRows->hide();
	}

	void agent_programs_panel::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if ((aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) && ShownCheck) {
			const auto shown = *ShownCheck;
			present_check(shown);
		}
	}

	void agent_programs_panel::present_check(const agent_check_presentation& aCheck)
	{
		Kept     = aCheck.Setup.Preferences.EntryChecks;
		Checking = aCheck.Setup.Checking ? aCheck.Setup.CheckingEntry : std::nullopt;
		present_roster();
		ShownCheck                   = aCheck;
		const auto& setup            = aCheck.Setup;
		const auto& state            = setup.State;
		const auto  saved            = state.Chosen ? state.Chosen : setup.Preferences.Chosen;
		const auto  name             = saved ? setup_agent_name(setup.Preferences, *saved) : tr("The agent");
		const auto  offer            = aCheck.Decision.Offer;
		const auto  running          = offer == core::agent_check_offer::running;
		const auto  workspaceRunning = state.WorkspaceCheck.State == core::setup_check_state::running;
		const auto  ran              = state.AgentCheck.State != core::setup_check_state::not_run || state.WorkspaceCheck.State != core::setup_check_state::not_run;
		static_cast<void>(ran);
		// [[BPR-2051]]. Check all checks each added agent in turn; only a
		// running check or a host that forbids the agent holds it back.
		CheckButton->setText(tr("Check all"));
		CheckButton->setEnabled(!running && !Agents.empty() && offer != core::agent_check_offer::forbidden && (Dispatch.CheckAll || offer == core::agent_check_offer::available));
		StopCheckButton->setVisible(running);
		CheckProgress->setVisible(running);

		// What it does, what runs now, or why it cannot run.
		auto said = QString{ };
		switch (offer) {
		case core::agent_check_offer::available: {
			const auto agent = saved.value_or(core::setup_agent::claude_code);
			said             = !setup.Detected ? tr("Looks for your agents, then checks the chosen one as the setup does. No question or code is sent.")
				: !setup.ChecksSignIn          ? tr("Checks that %1's program can run, and runs its own check if it names one. No question or code is sent.").arg(name)
				: setup.ChecksWrapper          ? tr("Asks your Claude process wrapper for its sign-in status and version. No question or code is sent.")
											   : tr("Runs %1, which sends no question and no code.").arg(status_command(agent));
			said += QStringLiteral(" ")
				+ (aCheck.Decision.ChecksWorkspace ? tr("It also makes the open pull request's workspace once, then removes it.") : tr("Open a pull request to check its workspace too."));
			break;
		}
		case core::agent_check_offer::running:
			said = setup.Checking && workspaceRunning ? tr("Checking %1 and preparing the workspace…").arg(name)
				: setup.Checking                      ? (setup.Detected ? tr("Checking %1…").arg(name) : tr("Looking for your agents…"))
													  : tr("Preparing the workspace…");
			break;
		case core::agent_check_offer::no_agent:   said = tr("Add an agent, then check it here."); break;
		case core::agent_check_offer::not_chosen: said = tr("Several agents were found. Choose one in the Agent tab's setup, then check it here."); break;
		case core::agent_check_offer::forbidden:  said = tr("The open pull request's host doesn't allow %1, so it can't be checked now.").arg(name); break;
		}
		CheckNote->setText(said);
		CheckButton->setToolTip(running ? tr("A check is running. Stop it to start again.") : tr("Checks every agent in the list in turn. No question or code is sent."));
		CheckButton->setAccessibleDescription(offer == core::agent_check_offer::available ? QString{ } : CheckButton->toolTip());
		CheckProgress->setAccessibleName(said);

		// The rows once a check has run this session.
		const auto mark = [this](QLabel* aMark, const core::setup_check_state aState) {
			const auto [glyph, colour] = setup_check_mark(aState, palette());
			aMark->setText(glyph);
			auto shade = aMark->palette();
			shade.setColor(QPalette::WindowText, colour);
			aMark->setPalette(shade);
		};
		const auto agentRan     = state.AgentCheck.State != core::setup_check_state::not_run;
		const auto workspaceRan = state.WorkspaceCheck.State != core::setup_check_state::not_run;
		// [[BPR-2051]]. A pass is the roster's ring and words; the rows show
		// here only to explain a failure and what to do.
		const auto anyFailed = state.AgentCheck.State == core::setup_check_state::failed || state.WorkspaceCheck.State == core::setup_check_state::failed;
		CheckRows->setVisible((agentRan || workspaceRan) && anyFailed);
		mark(AgentMark, state.AgentCheck.State);
		auto proves = agent_check_words(setup, name);
		if (state.AgentCheck.State == core::setup_check_state::passed && !state.AgentCheck.Version.empty()) {
			proves = tr("%1, version %2").arg(proves, text_of(state.AgentCheck.Version));
		}
		AgentRow->setText(proves);
		AgentMark->parentWidget()->setVisible(agentRan);
		WorkspaceLine->setVisible(workspaceRan);
		mark(WorkspaceMark, state.WorkspaceCheck.State);
		WorkspaceRow->setText(tr("The open pull request's workspace can be made"));

		const auto agentFailed = state.AgentCheck.State == core::setup_check_state::failed && state.AgentCheck.Failure.has_value();
		CheckFailure->setVisible(agentFailed);
		CheckFailure->setText(agentFailed ? setup_failure_words(*state.AgentCheck.Failure, name, text_of(setup.CheckDetail)) : QString{ });
		const auto signIn = agentFailed && *state.AgentCheck.Failure == core::agent_setup_failure::not_signed_in && saved ? sign_in_command(*saved) : QString{ };
		SignInCommand->setText(signIn);
		SignInLine->setVisible(!signIn.isEmpty());
		const auto workspaceFailed = state.WorkspaceCheck.State == core::setup_check_state::failed;
		WorkspaceReason->setVisible(workspaceFailed);
		WorkspaceReason->setText(!workspaceFailed             ? QString{ }
									 : aCheck.WorkspaceReason ? workspace_unavailable_words(*aCheck.WorkspaceReason)
															  : setup_failure_words(core::agent_setup_failure::workspace_unavailable, name, { }));
		// A failed check's own output, and the workspace's log, behind one
		// disclosure; a new detail closes it, as in the setup.
		auto detail = agentFailed ? setup.CheckDetail : std::string{ };
		if (workspaceFailed && !setup.WorkspaceDetail.empty()) {
			detail += (detail.empty() ? std::string{ } : std::string{ "\n\n" }) + setup.WorkspaceDetail;
		}
		DetailsToggle->setVisible(agentFailed || workspaceFailed);
		if (detail != ShownDetail || !(agentFailed || workspaceFailed)) {
			DetailsToggle->setChecked(false);
		}
		ShownDetail = detail;
		DetailsText->setPlainText(detail.empty() ? tr("No process output was returned.") : text_of(detail));
	}

	auto agent_programs_panel::check_result_text() const -> QString
	{
		auto lines = QStringList{ };
		for (const auto* label : { AgentRow, WorkspaceRow, CheckFailure, SignInCommand, WorkspaceReason }) {
			// A pass is said by the roster's ring; its rows still read here.
			const auto shown = label->isVisibleTo(CheckRows);
			if (shown && !label->text().isEmpty()) {
				const auto* const mark = label == AgentRow ? AgentMark : label == WorkspaceRow ? WorkspaceMark
																							   : nullptr;
				lines << (mark != nullptr ? mark->text() + QStringLiteral(" ") : QString{ }) + label->text();
			}
		}
		return lines.join(QLatin1Char{ '\n' });
	}

	void agent_programs_panel::show_answer(const core::setup_agent /*aAgent*/, const std::optional<core::agent_wrapper_refusal>& aRefusal)
	{
		say(aRefusal ? refusal_words(*aRefusal) : tr("Saved."));
	}

	clone_folders_panel::clone_folders_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("cloneFoldersPanel"));
		auto* const column = new QVBoxLayout{ this };
		// [[BPR-1856]]. The panel follows Add repository on Repositories; the
		// margin above its heading starts a new section rather than reading as
		// part of the button's row.
		column->setContentsMargins(0, 16, 0, 0);
		column->addWidget(heading(tr("Clone folders"), this));
		column->addWidget(note(tr("Where your clones live. The agent setup looks here, two levels down, for a clone of each pull request's repository. It reads only each clone's Git configuration and current branch, runs nothing and writes nothing."), this));
		// Owner look v002: Visual Studio's folder list, edited in place.
		Folders = new folder_list_editor{ this };
		Folders->setObjectName(QStringLiteral("settingsCloneFolders"));
		Folders->set_change([this](std::vector<std::string> aFolders) {
			if (Dispatch.SetFolders) {
				Dispatch.SetFolders(std::move(aFolders));
			}
		});
		column->addWidget(Folders);

		column->addSpacing(8);
		column->addWidget(heading(tr("Linked clones"), this));
		column->addWidget(note(tr("The clone each repository's agent works with, or none. Forget it to choose again."), this));
		Links = new QListWidget{ this };
		Links->setObjectName(QStringLiteral("settingsLinkedClones"));
		Links->setMaximumHeight(140);
		column->addWidget(Links);
		Forget = new QPushButton{ tr("Forget"), this };
		Forget->setObjectName(QStringLiteral("settingsForgetLink"));
		connect(Forget, &QPushButton::clicked, this, [this] {
			const auto at = Links->currentRow();
			if (at >= 0 && static_cast<std::size_t>(at) < Shown.Links.size() && Dispatch.Forget) {
				Dispatch.Forget(Shown.Links[static_cast<std::size_t>(at)]);
			}
		});
		column->addWidget(Forget, 0, Qt::AlignLeft);
		connect(Links, &QListWidget::currentRowChanged, this, [this](const int aRow) { Forget->setEnabled(aRow >= 0); });
		Forget->setEnabled(false);
	}

	void clone_folders_panel::set_dispatch(dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	void clone_folders_panel::present(const application::clone_link_projection& aProjection)
	{
		Shown = aProjection;
		Folders->present(Shown.Folders);
		Links->clear();
		for (const auto& link : Shown.Links) {
			// [[BPR-1873]]. No clone: a private copy, or for a review read
			// without its blobs only the pull request's files.
			// [[BPR-1879]]. The clone BetterPullReview keeps, and where.
			const auto target = link.PrivateCopy ? tr("no clone") : link.Managed ? tr("kept by BetterPullReview in %1").arg(QDir::toNativeSeparators(text_of(link.Path)))
																				 : QDir::toNativeSeparators(text_of(link.Path));
			Links->addItem(tr("%1/%2 on %3: %4").arg(text_of(link.Owner), text_of(link.Name), text_of(link.Host), target));
		}
		Forget->setEnabled(Links->currentRow() >= 0);
	}

	review_skill_panel::review_skill_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("reviewSkillPanel"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		Heading = heading(tr("Review skill"), this);
		column->addWidget(Heading);
		Intro = note(tr("How the agent reviews: sent with every review request, the same for every agent, and written to no harness's configuration. It grants nothing; the agent's own tools and permissions still govern."), this);
		column->addWidget(Intro);
		Source = note({ }, this);
		Source->setObjectName(QStringLiteral("reviewSkillSource"));
		column->addWidget(Source);
		Offer                = new QWidget{ this };
		auto* const offerRow = new QHBoxLayout{ Offer };
		offerRow->setContentsMargins(0, 0, 0, 0);
		OfferText = new QLabel{ Offer };
		OfferText->setObjectName(QStringLiteral("reviewSkillOfferText"));
		offerRow->addWidget(OfferText, 1);
		auto* const compare = new quiet_push_button{ tr("Compare"), Offer };
		compare->setObjectName(QStringLiteral("reviewSkillCompare"));
		compare->setCheckable(true);
		offerRow->addWidget(compare);
		UseVersion = new QPushButton{ Offer };
		UseVersion->setObjectName(QStringLiteral("reviewSkillUseVersion"));
		connect(UseVersion, &QPushButton::clicked, this, [this] {
			if (Dispatch.UseBuiltIn) {
				Dispatch.UseBuiltIn(true);
			}
		});
		offerRow->addWidget(UseVersion);
		auto* const keep = new quiet_push_button{ tr("Keep mine"), Offer };
		keep->setObjectName(QStringLiteral("reviewSkillKeepMine"));
		connect(keep, &QPushButton::clicked, this, [this] {
			if (Dispatch.KeepMine) {
				Dispatch.KeepMine();
			}
		});
		offerRow->addWidget(keep);
		Offer->hide();
		column->addWidget(Offer);
		auto* const texts = new QHBoxLayout{ };
		Editor            = new QPlainTextEdit{ this };
		Editor->setObjectName(QStringLiteral("reviewSkillEditor"));
		Editor->setAccessibleName(tr("Review skill text"));
		Editor->setMinimumHeight(220);
		Editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
		texts->addWidget(Editor, 1);
		Default = new QPlainTextEdit{ this };
		Default->setObjectName(QStringLiteral("reviewSkillDefault"));
		Default->setAccessibleName(tr("The newer default"));
		Default->setReadOnly(true);
		Default->hide();
		texts->addWidget(Default, 1);
		column->addLayout(texts);
		connect(compare, &QPushButton::toggled, Default, &QWidget::setVisible);
		auto* const row  = new QHBoxLayout{ };
		auto* const save = new QPushButton{ tr("Save"), this };
		save->setObjectName(QStringLiteral("reviewSkillSave"));
		connect(save, &QPushButton::clicked, this, [this] {
			if (Dispatch.Save) {
				Dispatch.Save(Editor->toPlainText().toStdString());
			}
		});
		row->addWidget(save);
		auto* const restore = new quiet_push_button{ tr("Restore default"), this };
		Restore             = restore;
		restore->setObjectName(QStringLiteral("reviewSkillRestore"));
		connect(restore, &QPushButton::clicked, this, [this] {
			if (Dispatch.UseBuiltIn) {
				Dispatch.UseBuiltIn(KeepCopy->isChecked());
			}
		});
		row->addWidget(restore);
		KeepCopy = new QCheckBox{ tr("Save my copy to Downloads first"), this };
		KeepCopy->setObjectName(QStringLiteral("reviewSkillKeepCopy"));
		KeepCopy->setChecked(true);
		row->addWidget(KeepCopy);
		row->addStretch(1);
		column->addLayout(row);
		Status = note({ }, this);
		Status->setObjectName(QStringLiteral("reviewSkillStatus"));
		column->addWidget(Status);
	}

	void review_skill_panel::set_dispatch(dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	void review_skill_panel::show_as_built_in_prompt(const QFont& aEditorFont)
	{
		Heading->hide();
		Intro->hide();
		Editor->setFont(aEditorFont);
		Default->setFont(aEditorFont);
		Editor->setMinimumHeight(300);
		// The other prompts' words for the same action.
		Restore->setText(tr("Reset to built-in"));
	}

	auto review_skill_panel::status_text() const -> QString { return Status->text(); }

	void review_skill_panel::show_answer(const application::review_skill_answer aAnswer)
	{
		switch (aAnswer) {
		case application::review_skill_answer::done:
		case application::review_skill_answer::refused:     return;
		case application::review_skill_answer::copy_failed: Status->setText(tr("Your copy could not be saved to Downloads, so nothing was removed.")); return;
		case application::review_skill_answer::not_saved:   Status->setText(tr("The skill could not be saved. Your text is still here.")); return;
		}
	}

	void review_skill_panel::present(const application::review_skill_projection& aProjection, const std::string& aBuiltIn)
	{
		if (aProjection.Generation == Generation) {
			return;
		}
		Generation = aProjection.Generation;
		Changed    = aProjection.Source == core::review_skill_source::profile_copy;
		// A refused save keeps what the reviewer wrote.
		if (!aProjection.Refusal) {
			Editor->setPlainText(text_of(aProjection.Text));
		}
		Source->setText(aProjection.Source == core::review_skill_source::built_in
							? tr("The built-in bpr-pr-review, version %1. Editing it makes your own copy, which updates never change.").arg(aProjection.BuiltInVersion)
							: tr("Your copy, made from version %1 of the built-in skill.").arg(aProjection.CopyBaseVersion.value_or(0U)));
		Offer->setVisible(aProjection.Offer.has_value());
		if (aProjection.Offer) {
			OfferText->setText(tr("Version %1 is available").arg(*aProjection.Offer));
			UseVersion->setText(tr("Use version %1").arg(*aProjection.Offer));
			Default->setPlainText(text_of(aBuiltIn));
		}
		auto status = QString{ };
		if (aProjection.Refusal) {
			status = skill_refusal_words(*aProjection.Refusal);
		} else if (aProjection.StoredProblem) {
			status = tr("Your saved copy can't be used: %1 Requests carry the built-in skill until it is fixed.").arg(skill_refusal_words(*aProjection.StoredProblem));
		} else if (aProjection.SavedCopy) {
			status = tr("Your copy was saved to %1.").arg(QDir::toNativeSeparators(text_of(*aProjection.SavedCopy)));
		}
		Status->setText(status);
	}
} // namespace bpr::qt
