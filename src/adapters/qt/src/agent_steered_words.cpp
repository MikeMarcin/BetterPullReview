#include "bpr/qt/agent_steered_words.hpp"

#include <QCoreApplication>

namespace bpr::qt {
	auto agent_steered_state_words(const core::agent_steered_message& aMessage) -> QString
	{
		const auto after = aMessage.AfterSteps == 0U ? QCoreApplication::translate("bpr::qt::agent_steered", "Steered before the agent's first step")
													 : QCoreApplication::translate("bpr::qt::agent_steered", "Steered after step %1").arg(aMessage.AfterSteps);
		switch (aMessage.Disposition) {
		case core::agent_steer_disposition::refused:  return QCoreApplication::translate("bpr::qt::agent_steered", "%1. The agent refused it, so it went back to the queue.").arg(after);
		case core::agent_steer_disposition::unknown:  return QCoreApplication::translate("bpr::qt::agent_steered", "%1. The agent may not have taken it.").arg(after);
		case core::agent_steer_disposition::unsent:   return QCoreApplication::translate("bpr::qt::agent_steered", "%1. It was not sent.").arg(after);
		case core::agent_steer_disposition::sent:
		case core::agent_steer_disposition::consumed: break;
		}
		return after;
	}

	auto agent_steered_accessible_name(const core::agent_steered_message& aMessage) -> QString
	{
		return QCoreApplication::translate("bpr::qt::agent_steered", "You steered: %1. %2").arg(QString::fromStdString(aMessage.Text), agent_steered_state_words(aMessage));
	}
} // namespace bpr::qt
