#include "bpr/application/description_draft_host.hpp"

#include <atomic>
#include <utility>

namespace bpr::application {
	struct description_draft_host::implementation : std::enable_shared_from_this<implementation>
	{
		implementation(effect_executor& aAgentLane, decision_executor& aDecisions, std::function<void(const description_draft_projection&)> aPublish)
			: AgentLane{ aAgentLane }
			, Decisions{ aDecisions }
			, Publish{ std::move(aPublish) }
		{ }

		effect_executor&                                         AgentLane;
		decision_executor&                                       Decisions;
		std::function<void(const description_draft_projection&)> Publish;
		description_draft_runner                                 Runner;
		description_draft_projection                             Projection;
		/// Each run's number; an answer for another is dropped.
		std::uint64_t      Run = 0U;
		std::stop_source   Stop;
		bool               Stopped = false;
		std::atomic_size_t Outstanding{ 0U };

		void publish()
		{
			++Projection.Revision;
			if (Publish) {
				Publish(Projection);
			}
		}
	};

	description_draft_host::description_draft_host(effect_executor& aAgentLane, decision_executor& aDecisions, std::function<void(const description_draft_projection&)> aPublish)
		: Implementation{ std::make_shared<implementation>(aAgentLane, aDecisions, std::move(aPublish)) }
	{ }

	description_draft_host::~description_draft_host() { shutdown(); }

	void description_draft_host::set_runner(description_draft_runner aRunner) { Implementation->Runner = std::move(aRunner); }

	auto description_draft_host::available() const noexcept -> bool { return static_cast<bool>(Implementation->Runner); }

	auto description_draft_host::draft(description_draft_agent aAgent, const core::description_draft_material& aMaterial) -> bool
	{
		auto& self = *Implementation;
		if (self.Stopped || self.Projection.Running || !self.Runner) {
			return false;
		}
		const auto run          = ++self.Run;
		self.Stop               = std::stop_source{ };
		self.Projection.Running = true;
		self.Projection.Failure.clear();
		self.Projection.Stopped = false;
		self.publish();
		++self.Outstanding;
		auto composed = core::compose_description_draft(aMaterial);
		self.AgentLane.post([host = Implementation, runner = self.Runner, agent = std::move(aAgent), message = std::move(composed), token = self.Stop.get_token(), run]() mutable {
			auto       reply   = runner(agent, message, token);
			const auto stopped = token.stop_requested();
			host->Decisions.post([host, answer = std::move(reply), stopped, run]() mutable {
				--host->Outstanding;
				if (host->Stopped || run != host->Run) {
					return;
				}
				host->Projection.Running = false;
				if (stopped) {
					host->Projection.Stopped = true;
				} else if (!answer) {
					host->Projection.Failure = answer.error().empty() ? std::string{ "The agent did not answer." } : answer.error();
				} else if (auto description = core::read_description_draft(*answer); description) {
					host->Projection.Draft = std::move(*description);
					++host->Projection.DraftRevision;
				} else {
					host->Projection.Failure = "The agent's answer was empty or longer than GitHub keeps. Nothing changed.";
				}
				host->publish();
			});
		});
		return true;
	}

	void description_draft_host::refuse(std::string aWhy)
	{
		auto& self = *Implementation;
		if (self.Stopped || self.Projection.Running) {
			return;
		}
		self.Projection.Failure = std::move(aWhy);
		self.Projection.Stopped = false;
		self.publish();
	}

	void description_draft_host::stop()
	{
		auto& self = *Implementation;
		if (!self.Projection.Running) {
			return;
		}
		self.Stop.request_stop();
	}

	auto description_draft_host::projection() const noexcept -> const description_draft_projection& { return Implementation->Projection; }

	void description_draft_host::shutdown()
	{
		if (!Implementation || Implementation->Stopped) {
			return;
		}
		Implementation->Stop.request_stop();
		Implementation->Stopped = true;
	}

	auto description_draft_host::is_quiescent() const noexcept -> bool { return Implementation->Outstanding.load() == 0U; }
} // namespace bpr::application
