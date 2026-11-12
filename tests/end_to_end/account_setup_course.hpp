#ifndef BPR_TESTS_END_TO_END_ACCOUNT_SETUP_COURSE_HPP
#define BPR_TESTS_END_TO_END_ACCOUNT_SETUP_COURSE_HPP

// RFC-BPR-0032 amendment r1. Courses sign in as a reviewer does, through
// Home's account setup: the server, the way, the account (and Connect, which
// is the gh consent), then Start reviewing. The driver presses each next
// control the setup offers for the asked way, from a timer, so a course
// written as a gesture then a wait stays one. It replaces the timers that
// answered the shared dialog's gh consent question, and counts Connect
// presses where those counted the questions.

#include "bpr/qt/account_setup_view.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QKeyEvent>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace bpr::e2e {
	class account_setup_driver final
	{
	public:
		struct plan
		{
			core::account_setup_server Server   = core::account_setup_server::github_com;
			std::string                Hostname = "github.com";
			core::account_setup_way    Way      = core::account_setup_way::github_cli;
			/// The gh account to connect; empty keeps the one the setup chose.
			std::string Login;
			/// Press Start reviewing once connected.
			bool Finish = true;
		};

		account_setup_driver(qt::workbench_window& aWindow, plan aPlan)
			: Window{ &aWindow }
			, Plan{ std::move(aPlan) }
		{
			QObject::connect(&Timer, &QTimer::timeout, [this] { step(); });
			Timer.start(5);
		}

		/// \brief Opens the setup as Home's and Settings' ways in do.
		void begin()
		{
			if (Window) {
				Window->begin_account_setup(Plan.Server, Plan.Hostname, Plan.Way);
			}
		}

		/// \brief Connect presses, one per account consented to.
		[[nodiscard]] auto connects() const noexcept -> std::size_t { return Connects; }
		void               stop() { Timer.stop(); }

	private:
		[[nodiscard]] auto press(const QString& aName) const -> bool
		{
			auto* const setup = Window->account_setup();
			for (auto* const button : setup->findChildren<QPushButton*>(aName)) {
				if (button->isVisibleTo(setup) && button->isEnabled()) {
					button->click();
					return true;
				}
			}
			return false;
		}

		void choose_login(const qt::account_setup_view& aSetup) const
		{
			const auto& state = aSetup.projection().State;
			for (const auto& account : state.Accounts) {
				if (account.Login == Plan.Login && state.Chosen != account.Profile) {
					for (auto* const card : aSetup.findChildren<QWidget*>(QStringLiteral("accountSetupAccount"))) {
						if (card->accessibleName() == QString::fromStdString(Plan.Login)) {
							auto key = QKeyEvent{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier };
							QApplication::sendEvent(card, &key);
							return;
						}
					}
				}
			}
		}

		void step()
		{
			// A course may restart the application while the driver lives on.
			if (!Window) {
				return;
			}
			auto* const setup = Window->account_setup();
			if (setup == nullptr || !setup->isVisible()) {
				return;
			}
			const auto& state = setup->projection().State;
			switch (state.Phase) {
			case core::account_setup_phase::choosing_server: static_cast<void>(press(QStringLiteral("accountSetupServerContinue"))); break;
			case core::account_setup_phase::choosing_way:
				if (setup->picked_way() != Plan.Way) {
					const auto name = Plan.Way == core::account_setup_way::github_cli ? QStringLiteral("accountSetupWayCli")
						: Plan.Way == core::account_setup_way::browser                ? QStringLiteral("accountSetupWayBrowser")
																					  : QStringLiteral("accountSetupWayToken");
					if (auto* const card = setup->findChild<QWidget*>(name); card != nullptr) {
						auto key = QKeyEvent{ QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier };
						QApplication::sendEvent(card, &key);
					}
					return;
				}
				static_cast<void>(press(QStringLiteral("accountSetupContinue")));
				break;
			case core::account_setup_phase::choosing_account:
				if (state.Problem != core::account_setup_problem::none && state.Problem != core::account_setup_problem::connection_failed) {
					return;
				}
				if (!Plan.Login.empty()) {
					choose_login(*setup);
				}
				if (Pressed != state.Request && press(QStringLiteral("accountSetupConnect"))) {
					Pressed = state.Request;
					++Connects;
				}
				break;
			case core::account_setup_phase::connected:
				if (Plan.Finish) {
					static_cast<void>(press(QStringLiteral("accountSetupFinish")));
				}
				break;
			default: break;
			}
		}

		QPointer<qt::workbench_window> Window;
		plan                           Plan;
		QTimer                         Timer;
		std::size_t                    Connects = 0U;
		std::optional<std::uint64_t>   Pressed;
	};
} // namespace bpr::e2e

#endif // BPR_TESTS_END_TO_END_ACCOUNT_SETUP_COURSE_HPP
