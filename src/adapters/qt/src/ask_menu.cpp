#include "bpr/qt/ask_menu.hpp"

#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/drawer_parts.hpp"

#include <QContextMenuEvent>
#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QProxyStyle>

namespace bpr::qt {
	namespace {
		/// A greyed row is reached by the arrow keys like any other, so its
		/// reason can be read; it still cannot be pressed.
		class ask_menu_style final : public QProxyStyle
		{
		public:
			using QProxyStyle::QProxyStyle;

			[[nodiscard]] auto styleHint(const StyleHint aHint, const QStyleOption* aOption, const QWidget* aWidget, QStyleHintReturn* aReturn) const -> int override
			{
				if (aHint == SH_Menu_AllowActiveAndDisabled) {
					return 1;
				}
				return QProxyStyle::styleHint(aHint, aOption, aWidget, aReturn);
			}
		};

		[[nodiscard]] auto section_title(const core::prompt_scope aScope, const ask_menu_request& aRequest) -> QString
		{
			switch (aScope) {
			case core::prompt_scope::pull_request: return QObject::tr("Pull request");
			case core::prompt_scope::file:         return QFileInfo{ aRequest.File }.fileName();
			case core::prompt_scope::thread:       return aRequest.ThreadTitle.isEmpty() ? QObject::tr("Thread") : aRequest.ThreadTitle;
			}
			return { };
		}
		/// What a row's own menu acts on.
		struct row_binding
		{
			ask_menu_request    Request;
			core::quick_ask_row Row;
			ask_menu_dispatch   Dispatch;
		};

		/// RFC-BPR-0128, owner look ask-v001: "I don't think right click
		/// should send the message ... in fact there should be a context
		/// menu." A right click on a row opens the row's own menu and is never
		/// a press; the Menu key does the same for the row in focus.
		class ask_row_filter final : public QObject
		{
		public:
			using QObject::QObject;

			auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				auto* const menu = qobject_cast<QMenu*>(aWatched);
				if (menu == nullptr) {
					return false;
				}
				if (aEvent->type() == QEvent::MouseButtonPress || aEvent->type() == QEvent::MouseButtonRelease || aEvent->type() == QEvent::MouseButtonDblClick) {
					auto* const mouse = static_cast<QMouseEvent*>(aEvent);
					if (mouse->button() != Qt::RightButton) {
						// Shift on the click that presses a row drafts it.
						menu->setProperty("bprAskShift", mouse->modifiers().testFlag(Qt::ShiftModifier));
						return false;
					}
					if (aEvent->type() == QEvent::MouseButtonRelease) {
						if (auto* const action = menu->actionAt(mouse->position().toPoint()); action != nullptr) {
							open(*menu, *action, mouse->globalPosition().toPoint());
						}
					}
					return true;
				}
				if (aEvent->type() == QEvent::ContextMenu) {
					auto* const context = static_cast<QContextMenuEvent*>(aEvent);
					auto* const action  = context->reason() == QContextMenuEvent::Keyboard ? menu->activeAction() : menu->actionAt(context->pos());
					if (action != nullptr) {
						open(*menu, *action, context->reason() == QContextMenuEvent::Keyboard ? menu->mapToGlobal(menu->actionGeometry(action).bottomLeft()) : context->globalPos());
					}
					return true;
				}
				return false;
			}

		private:
			static void open(QMenu& aMenu, QAction& aAction, const QPoint& aAt)
			{
				if (auto* const rowMenu = make_ask_row_menu(aMenu, aAction, &aMenu); rowMenu != nullptr) {
					rowMenu->setAttribute(Qt::WA_DeleteOnClose);
					rowMenu->popup(aAt);
				}
			}
		};
	} // namespace

	auto make_ask_row_menu(QMenu& aMenu, QAction& aRow, QWidget* const aParent) -> QMenu*
	{
		const auto binding = aRow.property("bprAskRow").value<std::shared_ptr<row_binding>>();
		if (!binding) {
			return nullptr;
		}
		auto* const menu = new QMenu{ aParent };
		menu->setObjectName(QStringLiteral("askRowMenu"));
		const auto close = [outer = QPointer<QMenu>{ &aMenu }] {
			if (outer) {
				outer->close();
			}
		};
		auto* const queue = menu->addAction(QObject::tr("Queue"));
		queue->setObjectName(QStringLiteral("askRowQueue"));
		queue->setEnabled(binding->Row.Enabled && static_cast<bool>(binding->Dispatch.Ask));
		QObject::connect(queue, &QAction::triggered, menu, [binding, close] {
			close();
			binding->Dispatch.Ask(binding->Request, binding->Row);
		});
		auto* const draft = menu->addAction(QObject::tr("Draft in Agent Tab"));
		draft->setObjectName(QStringLiteral("askRowDraft"));
		draft->setEnabled(static_cast<bool>(binding->Dispatch.Draft));
		QObject::connect(draft, &QAction::triggered, menu, [binding, close] {
			close();
			binding->Dispatch.Draft(binding->Request, binding->Row);
		});
		return menu;
	}

	auto agent_glyph(const QPalette& aGround) -> QIcon
	{
		auto palette = aGround;
		palette.setColor(QPalette::Active, QPalette::ButtonText, agent_accent(aGround));
		palette.setColor(QPalette::Inactive, QPalette::ButtonText, agent_accent(aGround));
		palette.setColor(QPalette::Disabled, QPalette::ButtonText, agent_accent_disabled(aGround));
		return drawer_icon(drawer_glyph::agent, palette);
	}

	auto quick_ask_reason_words(const core::quick_ask_reason aReason, const core::prompt_scope aScope) -> QString
	{
		using enum core::quick_ask_reason;
		switch (aReason) {
		case none:                     return { };
		case agent_not_set_up:         return QObject::tr("Set up an agent first.");
		case agent_busy:               return QObject::tr("The agent is answering another question.");
		case not_reviewed:             return QObject::tr("You haven't reviewed this pull request yet.");
		case no_commits_since_review:  return QObject::tr("No commits since your review.");
		case reading_commits:          return QObject::tr("Reading the commits since your review.");
		case earlier_unreadable:       return QObject::tr("The code you reviewed can't be read here.");
		case no_threads:               return aScope == core::prompt_scope::file ? QObject::tr("No review threads on this file.") : QObject::tr("No review threads on this pull request.");
		case no_commits_since_threads: return QObject::tr("No commits since these threads started.");
		case no_commits_since_thread:  return QObject::tr("No commits since this thread started.");
		case thread_commit_unknown:    return QObject::tr("The commit this thread started on isn't in this pull request.");
		case no_threads_match:         return QObject::tr("No thread here meets its conditions.");
		case no_files_match:           return QObject::tr("No file meets its conditions.");
		}
		return { };
	}

	auto quick_ask_count_words(const core::quick_ask_row& aRow) -> QString
	{
		const auto count = static_cast<qulonglong>(aRow.Count);
		switch (aRow.Unit) {
		case core::quick_ask_unit::none:    return { };
		case core::quick_ask_unit::commits: return count == 1U ? QObject::tr("1 commit") : QObject::tr("%1 commits").arg(count);
		case core::quick_ask_unit::threads: return count == 1U ? QObject::tr("1 thread") : QObject::tr("%1 threads").arg(count);
		case core::quick_ask_unit::files:   return count == 1U ? QObject::tr("1 file") : QObject::tr("%1 files").arg(count);
		}
		return { };
	}

	void fill_ask_menu(QMenu& aMenu, const core::quick_ask_menu& aRows, const ask_menu_request& aRequest, const ask_menu_dispatch& aDispatch)
	{
		aMenu.clear();
		aMenu.setToolTipsVisible(true);
		if (aMenu.property("bprAskMenuStyled").isNull()) {
			auto* const style = new ask_menu_style{ };
			style->setParent(&aMenu);
			aMenu.setStyle(style);
			aMenu.setProperty("bprAskMenuStyled", true);
			aMenu.installEventFilter(new ask_row_filter{ &aMenu });
			// The keyboard reaches a greyed row too; its reason shows beside it.
			QObject::connect(&aMenu, &QMenu::hovered, &aMenu, [menu = &aMenu](QAction* const aAction) {
				if (aAction != nullptr && !aAction->isEnabled() && !aAction->toolTip().isEmpty()) {
					show_anchored_tooltip(*menu, menu->actionGeometry(aAction), aAction->toolTip(), anchored_tooltip_hold::focus);
				} else {
					hide_anchored_tooltip();
				}
			});
		}
		const auto glyph  = agent_glyph(aMenu.palette());
		const auto titled = aRows.Sections.size() > 1U;
		for (const auto& section : aRows.Sections) {
			if (titled) {
				aMenu.addSection(section_title(section.Scope, aRequest));
			}
			for (const auto& row : section.Rows) {
				const auto  count  = quick_ask_count_words(row);
				const auto  label  = QString::fromStdString(row.Label);
				auto* const action = aMenu.addAction(glyph, count.isEmpty() ? label : label + QLatin1Char('\t') + count);
				action->setObjectName(QStringLiteral("quickAsk_%1_%2").arg(QString::fromStdString(row.Id), QString::fromLatin1(core::stable_name(section.Scope))));
				action->setData(QString::fromStdString(row.Id));
				action->setEnabled(row.Enabled && static_cast<bool>(aDispatch.Ask));
				const auto reason = quick_ask_reason_words(row.Reason, row.Scope);
				const auto waits  = row.Waits == core::quick_ask_reason::agent_not_set_up ? QObject::tr(" It waits until the agent is set up.")
					: row.Waits == core::quick_ask_reason::agent_busy                     ? QObject::tr(" It waits until the agent's current answer finishes.")
																						  : QString{ };
				const auto tip    = row.Enabled ? (count.isEmpty() ? QObject::tr("%1. Click queues it; Shift+click drafts it.").arg(label) : QObject::tr("%1: reads %2. Click queues it; Shift+click drafts it.").arg(label, count)) + waits : reason;
				action->setToolTip(tip);
				action->setStatusTip(tip);
				action->setWhatsThis(tip);
				action->setProperty("bprAskRow", QVariant::fromValue(std::make_shared<row_binding>(row_binding{ .Request = aRequest, .Row = row, .Dispatch = aDispatch })));
				// Owner look ask-v001: Shift+click drafts the message; a plain
				// click queues it (owner, 2026-11-14).
				QObject::connect(action, &QAction::triggered, &aMenu, [menu = &aMenu, dispatch = aDispatch, request = aRequest, decided = row] {
					const auto shift = menu->property("bprAskShift").toBool() || QGuiApplication::keyboardModifiers().testFlag(Qt::ShiftModifier);
					menu->setProperty("bprAskShift", false);
					if (shift) {
						if (dispatch.Draft) {
							dispatch.Draft(request, decided);
						}
					} else if (dispatch.Ask) {
						dispatch.Ask(request, decided);
					}
				});
			}
		}
		if (!aRows.Sections.empty()) {
			aMenu.addSeparator();
		}
		auto* const custom = aMenu.addAction(QObject::tr("Custom question…"));
		custom->setObjectName(QStringLiteral("quickAskCustomQuestion"));
		custom->setEnabled(static_cast<bool>(aDispatch.Custom));
		custom->setToolTip(QObject::tr("Open the Agent tab to ask your own question; nothing is sent"));
		QObject::connect(custom, &QAction::triggered, &aMenu, [dispatch = aDispatch.Custom, request = aRequest] {
			if (dispatch) {
				dispatch(request);
			}
		});
		auto* const edit = aMenu.addAction(QObject::tr("Edit Quick Asks…"));
		edit->setObjectName(QStringLiteral("quickAskEdit"));
		edit->setEnabled(static_cast<bool>(aDispatch.Edit));
		edit->setToolTip(QObject::tr("Choose which suggested prompts are quick asks, in Settings"));
		QObject::connect(edit, &QAction::triggered, &aMenu, [dispatch = aDispatch.Edit] {
			if (dispatch) {
				dispatch();
			}
		});
	}

	ask_menu_button::ask_menu_button(QWidget* const aParent)
		: menu_tool_button{ aParent }
	{
		setObjectName(QStringLiteral("askMenuButton"));
		setText(tr("Ask"));
		setAccessibleName(tr("Ask"));
		setAccessibleDescription(tr("Ask the agent a quick question about the pull request, the file or the thread"));
		setToolTip(tr("Ask the agent"));
		setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		setPopupMode(QToolButton::InstantPopup);
		setFocusPolicy(Qt::StrongFocus);
		setAutoRaise(true);
		auto* const menu = new QMenu{ this };
		menu->setObjectName(QStringLiteral("askMenu"));
		setMenu(menu);
		connect(menu, &QMenu::aboutToShow, this, [this] { refresh_menu(); });
		wear_glyph();
	}

	void ask_menu_button::set_source(ask_menu_source aSource) { Source = std::move(aSource); }

	void ask_menu_button::set_dispatch(ask_menu_dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	void ask_menu_button::set_request(std::function<ask_menu_request()> aRequest) { Request = std::move(aRequest); }

	void ask_menu_button::set_filler(std::function<void(QMenu&)> aFiller) { Filler = std::move(aFiller); }

	void ask_menu_button::refresh_menu()
	{
		if (Filler) {
			menu()->clear();
			Filler(*menu());
			return;
		}
		const auto request = Request ? Request() : ask_menu_request{ };
		const auto rows    = Source ? Source(request) : std::nullopt;
		fill_ask_menu(*menu(), rows.value_or(core::quick_ask_menu{ }), request, Dispatch);
	}

	void ask_menu_button::changeEvent(QEvent* const aEvent)
	{
		menu_tool_button::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) {
			wear_glyph();
		}
	}

	void ask_menu_button::wear_glyph() { setIcon(agent_glyph(palette())); }
} // namespace bpr::qt
