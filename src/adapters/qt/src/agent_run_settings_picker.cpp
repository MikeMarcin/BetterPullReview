#include "bpr/qt/agent_run_settings_picker.hpp"

#include "bpr/qt/agent_launch_panel.hpp"

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QEnterEvent>
#include <QEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QTimer>

#include <algorithm>
#include <array>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto mixed(const QColor& aFrom, const QColor& aTo, const qreal aShare) -> QColor
		{
			const auto keep = 1.0 - aShare;
			return QColor::fromRgbF(static_cast<float>(aFrom.redF() * keep + aTo.redF() * aShare), static_cast<float>(aFrom.greenF() * keep + aTo.greenF() * aShare),
									static_cast<float>(aFrom.blueF() * keep + aTo.blueF() * aShare));
		}
	}

	agent_run_settings_source::agent_run_settings_source(agent_launch_panel& aLaunch)
		: Launch{ &aLaunch }
	{
		// The choice is the launch panel's; every picker follows it.
		QObject::connect(aLaunch.profile_selector(), &QComboBox::currentIndexChanged, &Context, [this] { refresh(); });
	}

	void agent_run_settings_source::present(std::vector<agent_run_settings> aSettings)
	{
		Shown = std::move(aSettings);
		refresh();
	}

	void agent_run_settings_source::report_model(const std::uint64_t aProfile, QString aModel)
	{
		ReportedModels[aProfile] = std::move(aModel);
		refresh();
	}

	auto agent_run_settings_source::current() const -> const agent_run_settings*
	{
		if (!Launch) {
			return nullptr;
		}
		const auto profile = Launch->selected_profile();
		if (!profile) {
			return nullptr;
		}
		const auto found = std::ranges::find_if(Shown, [&profile](const agent_run_settings& aSettings) { return aSettings.Profile == profile->value(); });
		return found == Shown.end() ? nullptr : &*found;
	}

	auto agent_run_settings_source::reported(const std::uint64_t aProfile) const -> QString
	{
		const auto found = ReportedModels.find(aProfile);
		return found == ReportedModels.end() ? QString{ } : found->second;
	}

	auto agent_run_settings_source::unavailable_name() const -> QString { return Launch ? Launch->unavailable_agent_name() : QString{ }; }

	void agent_run_settings_source::refresh()
	{
		for (auto* const picker : Pickers) {
			picker->present();
		}
	}

	void agent_run_settings_source::attach(agent_run_settings_picker* aPicker) { Pickers.push_back(aPicker); }
	void agent_run_settings_source::detach(agent_run_settings_picker* aPicker) { std::erase(Pickers, aPicker); }

	agent_run_settings_picker::agent_run_settings_picker(std::shared_ptr<agent_run_settings_source> aSource, QWidget* aParent)
		: QToolButton{ aParent }
		, Source{ std::move(aSource) }
	{
		setObjectName(QStringLiteral("agentRunSettings"));
		setAccessibleName(tr("Agent, model and effort"));
		setAutoRaise(true);
		setPopupMode(QToolButton::InstantPopup);
		setMenu(new QMenu{ this });
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
		connect(menu(), &QMenu::aboutToShow, this, [this] {
			// Owner look v006: the harness is asked what it offers when the
			// reviewer opens its menu, once per window.
			if (const auto* const settings = Source->current(); settings != nullptr && Source->Catalog) {
				Source->Catalog(settings->Profile);
			}
			present();
		});
		connect(menu(), &QMenu::aboutToHide, this, [this] {
			update();
			if (MenuStale) {
				QTimer::singleShot(0, this, [this] { present(); });
			}
		});
		Source->attach(this);
		present();
	}

	agent_run_settings_picker::~agent_run_settings_picker() { Source->detach(this); }

	void agent_run_settings_picker::choose(const std::uint64_t aProfile)
	{
		if (Source->Choose) {
			static_cast<void>(Source->Choose(aProfile));
		}
	}

	void agent_run_settings_picker::fit()
	{
		// [[BPR-1924]]. The whole words always: a narrow field gives the
		// picker a row of its own rather than an icon. The tooltip leads with
		// them, for the rare row narrower still.
		setText(Full);
		setToolTip(tr("Choose the agent and model: %1").arg(Full) + QStringLiteral("\n") + Tip);
		updateGeometry();
		update();
	}

	void agent_run_settings_picker::present()
	{
		const auto* const settings = Source->current();
		if (settings == nullptr && !Source->unavailable_name().isEmpty()) {
			present_unavailable();
			return;
		}
		setVisible(settings != nullptr);
		if (settings == nullptr) {
			return;
		}
		// Short enough for the field's row: the model, or "Default model",
		// and the effort when one is chosen.
		// Owner look v004: a model by its name and version. The agent's own
		// default is named by what it last reported running, when it did.
		const auto reported = Source->reported(settings->Profile);
		const auto ranAs    = reported.isEmpty() ? QString{ } : agent_model_display_name(reported);
		const auto model    = settings->Model.empty() ? tr("default model") : agent_model_display_name(QString::fromStdString(settings->Model));
		auto       shown    = !settings->Model.empty() ? model : ranAs.isEmpty() ? tr("Default model")
																				 : ranAs;
		if (!settings->Effort.empty()) {
			shown += QStringLiteral(" · ") + tr("%1 effort").arg(QString::fromStdString(settings->Effort));
		}
		// Owner amendment 2026-10-23: agent · model · effort.
		Full = settings->Agent + QStringLiteral(" · ") + (!(settings->ModelChoosable || settings->EffortChoosable) ? tr("its own model") : shown);
		setProperty("agentRunSettingsText", Full);
		// The whole words, shortened when the row is narrow.
		setAccessibleName(tr("Agent, model and effort: %1").arg(Full));
		auto tip = tr("%1 runs %2").arg(settings->Agent, settings->Program.isEmpty() ? tr("the program setup found") : settings->Program);
		if (settings->ModelChoosable) {
			tip += QStringLiteral("\n") + (settings->Model.empty() ? tr("Model: the agent's own default") : tr("Model: %1").arg(model));
		}
		if (settings->EffortChoosable) {
			tip += QStringLiteral("\n") + (settings->Effort.empty() ? tr("Effort: the agent's own default") : tr("Effort: %1").arg(QString::fromStdString(settings->Effort)));
		}
		if (!settings->Note.isEmpty()) {
			tip += QStringLiteral("\n") + settings->Note;
		}
		if (!ranAs.isEmpty()) {
			tip += QStringLiteral("\n") + tr("Last answer ran %1 (%2)").arg(ranAs, reported);
		}
		Tip = tip;
		fit();
		auto* const choices = menu();
		// A menu open now is rebuilt once it closes, never under the pointer.
		if (choices->isVisible()) {
			MenuStale = true;
			return;
		}
		MenuStale = false;
		// `clear` keeps the Model and Effort menus it made; each rebuild
		// makes its own, so the old ones go.
		for (auto* const submenu : choices->findChildren<QMenu*>(Qt::FindDirectChildrenOnly)) {
			submenu->deleteLater();
		}
		choices->clear();
		// Every agent added, one press away; each keeps its own conversation
		// about this pull request (owner decision 2026-10-23).
		auto* const agents = new QActionGroup{ choices };
		for (const auto& offered : Source->shown()) {
			auto* const item = choices->addAction(offered.Agent);
			item->setObjectName(QStringLiteral("agentRunSettingsAgent"));
			item->setCheckable(true);
			item->setChecked(offered.Profile == settings->Profile);
			agents->addAction(item);
			connect(item, &QAction::triggered, this, [this, profile = offered.Profile] { choose(profile); });
		}
		choices->addSeparator();
		auto* const what = choices->addAction(tr("%1 runs %2").arg(settings->Agent, settings->Program.isEmpty() ? tr("the program setup found") : settings->Program));
		what->setEnabled(false);
		if (!settings->Note.isEmpty()) {
			choices->addAction(settings->Note)->setEnabled(false);
		}
		const auto profile = settings->Profile;
		const auto apply   = [this, profile](std::optional<std::string> aModel, std::optional<std::string> aEffort) {
			if (Source->Dispatch && !Source->Dispatch(profile, std::move(aModel), std::move(aEffort)) && Source->Refused) {
				Source->Refused(tr("The agent doesn't take that setting."));
			}
		};
		if (settings->ModelChoosable) {
			auto* const models = choices->addMenu(tr("Model"));
			models->setObjectName(QStringLiteral("agentRunSettingsModels"));
			auto* const group = new QActionGroup{ models };
			const auto  add   = [&](const std::string& aValue, const QString& aLabel) {
				auto* const action = models->addAction(aLabel);
				action->setCheckable(true);
				action->setChecked(settings->Model == aValue);
				group->addAction(action);
				connect(action, &QAction::triggered, this, [apply, aValue] { apply(aValue, std::nullopt); });
			};
			add({ }, ranAs.isEmpty() ? tr("Default (the agent's own setting)") : tr("Default (the agent's own; last ran %1)").arg(ranAs));
			if (settings->Models.empty() && settings->Listing) {
				models->addAction(tr("Asking %1 for its models…").arg(settings->Agent))->setEnabled(false);
			}
			for (const auto& [value, label] : settings->Models) {
				add(value, label);
			}
			const auto known = settings->Model.empty() || std::ranges::any_of(settings->Models, [&settings](const auto& aModel) { return aModel.first == settings->Model; });
			if (!known) {
				add(settings->Model, agent_model_display_name(QString::fromStdString(settings->Model)));
			}
			connect(models->addAction(tr("Other…")), &QAction::triggered, this, [this, apply, current = settings->Model] {
				auto       accepted = false;
				const auto name     = QInputDialog::getText(this, tr("Model"), tr("The model's name, as the agent's --model takes it:"), QLineEdit::Normal, QString::fromStdString(current), &accepted).trimmed();
				if (accepted) {
					apply(name.toStdString(), std::nullopt);
				}
			});
		}
		if (settings->EffortChoosable) {
			auto* const efforts = choices->addMenu(tr("Effort"));
			efforts->setObjectName(QStringLiteral("agentRunSettingsEfforts"));
			auto* const group = new QActionGroup{ efforts };
			const auto  add   = [&](const std::string& aValue, const QString& aLabel) {
				auto* const action = efforts->addAction(aLabel);
				action->setCheckable(true);
				action->setChecked(settings->Effort == aValue);
				group->addAction(action);
				connect(action, &QAction::triggered, this, [apply, aValue] { apply(std::nullopt, aValue); });
			};
			add({ }, tr("Default (the agent's own setting)"));
			for (const auto& level : settings->Efforts) {
				add(level, QString::fromStdString(level));
			}
		}
		choices->addSeparator();
		choices->addAction(tr("Type /model NAME or /effort LEVEL in the question field"))->setEnabled(false);
		// Owner look v006 ("This model picker dropdown should have a link to
		// open the agent settings page").
		auto* const open = choices->addAction(tr("Agent settings…"));
		open->setObjectName(QStringLiteral("agentRunSettingsOpenSettings"));
		connect(open, &QAction::triggered, this, [this] {
			if (Source->Setup) {
				Source->Setup();
			}
		});
	}

	void agent_run_settings_picker::present_unavailable()
	{
		// [[BPR-1823]], owner ruling 2026-10-29. The chosen agent went (removed
		// in Settings, or not allowed here) and none was put in its place: the
		// pill says so, state first so a narrow pill keeps it, and its menu is
		// where another is chosen.
		const auto name = Source->unavailable_name();
		Full            = tr("Unavailable: %1").arg(name);
		setVisible(true);
		setProperty("agentRunSettingsText", Full);
		setAccessibleName(tr("Agent: %1 is unavailable. Choose an agent.").arg(name));
		Tip = tr("%1 isn't in your agents, or this pull request's server doesn't allow it. Nothing is sent until you choose an agent.").arg(name);
		fit();
		auto* const choices = menu();
		if (choices->isVisible()) {
			MenuStale = true;
			return;
		}
		MenuStale = false;
		for (auto* const submenu : choices->findChildren<QMenu*>(Qt::FindDirectChildrenOnly)) {
			submenu->deleteLater();
		}
		choices->clear();
		choices->addAction(tr("%1 isn't available. Choose an agent:").arg(name))->setEnabled(false);
		for (const auto& offered : Source->shown()) {
			auto* const item = choices->addAction(offered.Agent);
			item->setObjectName(QStringLiteral("agentRunSettingsAgent"));
			connect(item, &QAction::triggered, this, [this, profile = offered.Profile] { choose(profile); });
		}
		choices->addSeparator();
		auto* const open = choices->addAction(tr("Agent settings…"));
		open->setObjectName(QStringLiteral("agentRunSettingsOpenSettings"));
		connect(open, &QAction::triggered, this, [this] {
			if (Source->Setup) {
				Source->Setup();
			}
		});
	}

	/// Owner look v003: a word that may shorten, but never below a few
	/// letters, so a narrow row keeps every other control whole.
	auto agent_run_settings_picker::minimumSizeHint() const -> QSize
	{
		return { std::min(sizeHint().width(), fontMetrics().horizontalAdvance(QStringLiteral("Mmmm…")) + chrome_width), sizeHint().height() };
	}

	auto agent_run_settings_picker::sizeHint() const -> QSize
	{
		const auto hint = QToolButton::sizeHint();
		return { fontMetrics().horizontalAdvance(text()) + chrome_width, std::max(hint.height(), fontMetrics().height() + 10) };
	}

	/// [[BPR-1924]], owner look on BPR-1914 v004 ("this box around the model
	/// picker looks more like a text entry field than a dropdown/picker"). A
	/// filled pill with the up-and-down chevron the scope menu beside it
	/// carries, so the two pickers in the row read as one kind of control
	/// and neither as a place to type. It deepens under the pointer and
	/// deeper again while pressed or open, and shows the keyboard focus as a
	/// ring in the highlight. Every colour comes from the palette.
	void agent_run_settings_picker::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto       painter = QPainter{ this };
		const auto group   = isEnabled() ? QPalette::Active : QPalette::Disabled;
		const auto ink     = palette().color(group, QPalette::WindowText);
		const auto base    = palette().color(QPalette::Active, QPalette::Base);
		const auto open    = isDown() || (menu() != nullptr && menu()->isVisible());
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(Qt::NoPen);
		painter.setBrush(mixed(base, ink, open ? 0.26 : underMouse() ? 0.18
																	 : 0.11));
		const auto box = QRectF{ rect() }.adjusted(1.0, 1.5, -1.0, -1.5);
		painter.drawRoundedRect(box, box.height() / 2.0, box.height() / 2.0);
		if (hasFocus()) {
			painter.setBrush(Qt::NoBrush);
			painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Highlight), 1.5 });
			painter.drawRoundedRect(box, box.height() / 2.0, box.height() / 2.0);
		}
		// The chevron: two strokes meeting above the middle and two below.
		const auto quiet  = underMouse() || open ? ink : mixed(base, ink, 0.7);
		const auto middle = QPointF{ width() - chevron_right - chevron_width / 2.0, height() / 2.0 };
		const auto half   = chevron_width / 2.0;
		painter.setPen(QPen{ quiet, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
		painter.drawPolyline(std::array{ middle + QPointF{ -half, -1.5 }, middle + QPointF{ 0.0, -1.5 - half }, middle + QPointF{ half, -1.5 } }.data(), 3);
		painter.drawPolyline(std::array{ middle + QPointF{ -half, 1.5 }, middle + QPointF{ 0.0, 1.5 + half }, middle + QPointF{ half, 1.5 } }.data(), 3);
		painter.setPen(ink);
		const auto words = rect().adjusted(text_left, 0, -(chrome_width - text_left), 0);
		painter.drawText(words, Qt::AlignVCenter | Qt::AlignLeft, fontMetrics().elidedText(text(), Qt::ElideRight, std::max(0, words.width())));
	}

	void agent_run_settings_picker::enterEvent(QEnterEvent* aEvent)
	{
		QToolButton::enterEvent(aEvent);
		update();
	}

	void agent_run_settings_picker::leaveEvent(QEvent* aEvent)
	{
		QToolButton::leaveEvent(aEvent);
		update();
	}

}
