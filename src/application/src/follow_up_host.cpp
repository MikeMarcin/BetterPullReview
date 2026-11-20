#include "bpr/application/follow_up_host.hpp"

#include <atomic>
#include <map>
#include <utility>

namespace bpr::application {
	struct follow_up_host::implementation : std::enable_shared_from_this<implementation>
	{
		implementation(effect_executor& aAgentLane, decision_executor& aDecisions, std::function<void(const follow_up_projection&)> aPublish)
			: AgentLane{ aAgentLane }
			, Decisions{ aDecisions }
			, Publish{ std::move(aPublish) }
		{ }

		effect_executor&                                 AgentLane;
		decision_executor&                               Decisions;
		std::function<void(const follow_up_projection&)> Publish;
		follow_up_runner                                 Runner;
		/// Every follow-up the window asked, by its item's key.
		std::map<std::string, core::follow_up_state> Held;
		/// The open one's key; empty when none is open.
		std::string Open;
		/// Each follow-up's run going, by key.
		std::map<std::string, std::stop_source> Stops;
		std::map<std::string, std::string>      Failures;
		follow_up_projection                    Projection;
		bool                                    Stopped = false;
		std::atomic_size_t                      Outstanding{ 0U };

		void publish()
		{
			const auto found      = Held.find(Open);
			Projection.Open       = found == Held.end() ? std::nullopt : std::optional{ found->second };
			const auto failure    = Failures.find(Open);
			Projection.RunFailure = failure == Failures.end() ? std::string{ } : failure->second;
			++Projection.Revision;
			if (Publish) {
				Publish(Projection);
			}
		}

		auto open_state() -> core::follow_up_state*
		{
			const auto found = Held.find(Open);
			return found == Held.end() ? nullptr : &found->second;
		}

		/// Runs `aRequest` for the follow-up `aKey`; its answer settles there.
		void run(const std::string& aKey, follow_up_agent aAgent, core::follow_up_request aRequest)
		{
			auto stop   = std::stop_source{ };
			Stops[aKey] = stop;
			Failures.erase(aKey);
			++Outstanding;
			AgentLane.post([host = shared_from_this(), runner = Runner, agent = std::move(aAgent), request = std::move(aRequest), key = aKey, token = stop.get_token()]() mutable {
				auto       reply   = runner(agent, request.Message, token);
				const auto stopped = token.stop_requested();
				host->Decisions.post([host, settled = std::move(key), id = request.Id, answer = std::move(reply), stopped]() mutable {
					--host->Outstanding;
					if (host->Stopped) {
						return;
					}
					const auto found = host->Held.find(settled);
					if (found == host->Held.end()) {
						return;
					}
					auto text = stopped || !answer ? std::optional<std::string>{ } : std::optional{ std::move(*answer) };
					if (core::settle_follow_up(found->second, id, std::move(text)) == core::follow_up_refusal::stale) {
						return;
					}
					host->Stops.erase(settled);
					if (stopped) {
						host->Failures[settled] = "Stopped.";
					} else if (!answer) {
						host->Failures[settled] = answer.error().empty() ? std::string{ "The agent did not answer." } : answer.error();
					}
					if (settled == host->Open) {
						host->publish();
					}
				});
			});
		}
	};

	follow_up_host::follow_up_host(effect_executor& aAgentLane, decision_executor& aDecisions, std::function<void(const follow_up_projection&)> aPublish)
		: Implementation{ std::make_shared<implementation>(aAgentLane, aDecisions, std::move(aPublish)) }
	{ }

	follow_up_host::~follow_up_host() { shutdown(); }

	void follow_up_host::set_runner(follow_up_runner aRunner) { Implementation->Runner = std::move(aRunner); }

	auto follow_up_host::available() const noexcept -> bool { return static_cast<bool>(Implementation->Runner); }

	auto follow_up_host::open(core::follow_up_subject aSubject) -> bool
	{
		auto& self = *Implementation;
		if (self.Stopped || !core::valid(aSubject)) {
			return false;
		}
		auto key = aSubject.Key;
		if (auto found = self.Held.find(key); found != self.Held.end()) {
			if (!core::refresh_follow_up(found->second, std::move(aSubject))) {
				return false;
			}
		} else {
			self.Held.emplace(key, core::open_follow_up(std::move(aSubject)));
		}
		self.Open = std::move(key);
		self.publish();
		return true;
	}

	void follow_up_host::close()
	{
		auto& self = *Implementation;
		if (self.Open.empty()) {
			return;
		}
		self.Open.clear();
		self.publish();
	}

	auto follow_up_host::ask(follow_up_agent aAgent, const std::string_view aQuestion) -> core::follow_up_refusal
	{
		auto&       self  = *Implementation;
		auto* const state = self.open_state();
		if (self.Stopped || state == nullptr || !self.Runner) {
			return core::follow_up_refusal::failed;
		}
		auto step = core::ask_follow_up(*state, aQuestion);
		if (step.Request) {
			self.run(self.Open, std::move(aAgent), std::move(*step.Request));
		}
		self.publish();
		return step.Refusal;
	}

	auto follow_up_host::propose(follow_up_agent aAgent, const core::follow_up_apply aApply, std::string aBase) -> core::follow_up_refusal
	{
		auto&       self  = *Implementation;
		auto* const state = self.open_state();
		if (self.Stopped || state == nullptr || !self.Runner) {
			return core::follow_up_refusal::failed;
		}
		auto step = core::propose_follow_up(*state, aApply, std::move(aBase));
		if (step.Request) {
			self.run(self.Open, std::move(aAgent), std::move(*step.Request));
		}
		self.publish();
		return step.Refusal;
	}

	auto follow_up_host::apply(const std::uint64_t aProposal, const std::string_view aCurrent) -> core::follow_up_application
	{
		auto&       self  = *Implementation;
		auto* const state = self.open_state();
		if (self.Stopped || state == nullptr) {
			return { .Refusal = core::follow_up_refusal::no_proposal };
		}
		auto decided = core::apply_follow_up(*state, aProposal, aCurrent);
		self.publish();
		return decided;
	}

	auto follow_up_host::discard(const std::uint64_t aProposal) -> bool
	{
		auto&       self  = *Implementation;
		auto* const state = self.open_state();
		if (self.Stopped || state == nullptr || !core::discard_follow_up(*state, aProposal)) {
			return false;
		}
		self.publish();
		return true;
	}

	void follow_up_host::stop()
	{
		auto& self = *Implementation;
		if (const auto found = self.Stops.find(self.Open); found != self.Stops.end()) {
			found->second.request_stop();
		}
	}

	auto follow_up_host::projection() const noexcept -> const follow_up_projection& { return Implementation->Projection; }

	auto follow_up_host::held(const std::string& aKey) const -> const core::follow_up_state*
	{
		const auto found = Implementation->Held.find(aKey);
		return found == Implementation->Held.end() ? nullptr : &found->second;
	}

	void follow_up_host::shutdown()
	{
		if (!Implementation || Implementation->Stopped) {
			return;
		}
		for (auto& entry : Implementation->Stops) {
			entry.second.request_stop();
		}
		Implementation->Stopped = true;
	}

	auto follow_up_host::is_quiescent() const noexcept -> bool { return Implementation->Outstanding.load() == 0U; }
} // namespace bpr::application
