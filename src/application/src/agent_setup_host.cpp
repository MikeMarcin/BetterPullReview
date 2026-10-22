#include "bpr/application/agent_setup_host.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <stop_token>
#include <utility>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto found_how(const core::agent_found_in aOrigin) noexcept -> core::agent_found_how
		{
			return aOrigin == core::agent_found_in::path ? core::agent_found_how::on_path : core::agent_found_how::usual_folder;
		}

		[[nodiscard]] auto bounded(std::string aText) -> std::string
		{
			if (aText.size() > maximum_agent_check_detail_bytes) {
				aText.erase(0U, aText.size() - maximum_agent_check_detail_bytes);
			}
			return aText;
		}

		[[nodiscard]] auto detail_of(const agent_run_result& aResult) -> std::string
		{
			auto text = std::string{ name(aResult.Outcome) };
			if (aResult.ExitCode) {
				text += " (exit " + std::to_string(*aResult.ExitCode) + ")";
			}
			for (const auto& event : aResult.Events) {
				text += '\n';
				text += event.Text;
			}
			if (!aResult.StandardError.empty()) {
				text += '\n';
				text += aResult.StandardError;
			}
			return bounded(std::move(text));
		}

		/// What a check run that did not answer means for the agent row.
		[[nodiscard]] auto unanswered(const agent_run_outcome aOutcome) -> std::optional<core::agent_setup_failure>
		{
			switch (aOutcome) {
			case agent_run_outcome::success:
			case agent_run_outcome::nonzero_exit:            return std::nullopt;
			case agent_run_outcome::timeout:                 return core::agent_setup_failure::timed_out;
			case agent_run_outcome::workspace_unavailable:   return core::agent_setup_failure::workspace_unavailable;
			case agent_run_outcome::cancelled:
			case agent_run_outcome::failed_to_start:
			case agent_run_outcome::protocol_failure:
			case agent_run_outcome::private_storage_failure:
			case agent_run_outcome::stale_result:            return core::agent_setup_failure::failed_to_start;
			}
			return core::agent_setup_failure::failed_to_start;
		}
	} // namespace

	struct agent_setup_host::implementation final : std::enable_shared_from_this<implementation>
	{
		explicit implementation(agent_setup_dependencies aDependencies)
			: Dependencies{ std::move(aDependencies) }
		{ }

		/// Runs `aTask` on `aLane`, then `aSettle` with its answer on the
		/// decision lane, unless the host has shut down. Counted, so the host
		/// is quiescent only when nothing is waiting to settle.
		template <typename Task, typename Settle>
		void off_lane(effect_executor* const aLane, Task aTask, Settle aSettle)
		{
			if (Closing || aLane == nullptr || Dependencies.Decision == nullptr) {
				return;
			}
			++Outstanding;
			try {
				aLane->post([self = shared_from_this(), task = std::move(aTask), settle = std::move(aSettle)]() mutable {
					if (self->Closing) {
						--self->Outstanding;
						return;
					}
					auto result = std::optional<decltype(task())>{ };
					try {
						result.emplace(task());
					} catch (...) { }
					try {
						self->Dependencies.Decision->post([owner = self, reply = std::move(settle), answer = std::move(result)]() mutable {
							--owner->Outstanding;
							if (!owner->Closing && answer) {
								reply(std::move(*answer));
							}
						});
					} catch (...) {
						--self->Outstanding;
					}
				});
			} catch (...) {
				--Outstanding;
			}
		}

		void publish()
		{
			auto& state         = Projection.State;
			state.Rows          = core::agent_setup_rows(Found, Dependencies.Profiles, Policy, Projection.Preferences.CodexProtocol);
			const auto saved    = Picked ? Picked : Projection.Preferences.Chosen;
			state.Chosen        = core::choose_setup_agent(state.Rows, saved);
			Projection.Searched = core::searched_setup_agents(Dependencies.Profiles, Policy, Projection.Preferences.CodexProtocol);
			Projection.Step     = core::current_setup_step(state);
			++Projection.Generation;
			if (Dependencies.Publish) {
				try {
					Dependencies.Publish(Projection);
				} catch (...) { }
			}
		}

		void save()
		{
			if (Dependencies.Store == nullptr || !valid(Projection.Preferences)) {
				return;
			}
			off_lane(
				Dependencies.Effects, [store = Dependencies.Store, preferences = Projection.Preferences] { return store->store(preferences); }, [](bool) { });
		}

		void apply_programs()
		{
			if (Dependencies.Programs) {
				Dependencies.Programs->set(core::setup_agent::codex, Projection.Preferences.CodexProgram);
				Dependencies.Programs->set(core::setup_agent::plain_text_harness, Projection.Preferences.HarnessProgram);
			}
		}

		void reset_checks()
		{
			++CheckGeneration;
			Projection.State.AgentCheck = { };
			Projection.Checking         = false;
			Projection.CheckDetail.clear();
		}

		[[nodiscard]] auto chosen_row() const -> const core::agent_setup_row*
		{
			const auto& state = Projection.State;
			if (!state.Chosen) {
				return nullptr;
			}
			const auto row = std::ranges::find(state.Rows, *state.Chosen, &core::agent_setup_row::Agent);
			return row == state.Rows.end() || !row->Verdict.Permitted ? nullptr : &*row;
		}

		void settle_check(const std::uint64_t aGeneration, const core::setup_agent aAgent, core::setup_check aCheck, std::string aDetail)
		{
			if (aGeneration != CheckGeneration) {
				return;
			}
			Projection.Checking         = false;
			Projection.State.AgentCheck = std::move(aCheck);
			Projection.CheckDetail      = Projection.State.AgentCheck.State == core::setup_check_state::passed ? std::string{ } : std::move(aDetail);
			if (Projection.State.AgentCheck.State == core::setup_check_state::passed && Projection.Preferences.Chosen != aAgent) {
				Projection.Preferences.Chosen = aAgent;
				save();
			}
			publish();
		}

		void detect()
		{
			auto&      host           = *this;
			const auto generation     = ++host.DetectGeneration;
			host.Projection.Detecting = true;
			host.publish();
			struct inputs
			{
				std::string             Wrapper;
				std::string             Codex;
				std::string             Harness;
				agent_wrapper_inspector Inspect;
				agent_program_locator   Locate;
			};
			// Assigned member by member: GCC's optimized build reports a
			// designated-initializer temporary's defaulted strings as
			// maybe-uninitialized once it is moved.
			auto snapshot    = inputs{ };
			snapshot.Wrapper = host.Dependencies.ClaudeWrapper ? host.Dependencies.ClaudeWrapper->current() : std::string{ };
			snapshot.Codex   = host.Projection.Preferences.CodexProgram;
			snapshot.Harness = host.Projection.Preferences.HarnessProgram;
			snapshot.Inspect = host.Dependencies.Inspect;
			snapshot.Locate  = host.Dependencies.Locate;
			host.off_lane(
				host.Dependencies.Effects,
				[given = std::move(snapshot)] {
					auto       found    = std::vector<core::agent_detection>{ };
					const auto runnable = [&given](const std::string& aPath) { return given.Inspect && !given.Inspect(aPath); };
					const auto searched = [&given, &found](const core::setup_agent aAgent, const core::agent_program aProgram) {
						if (!given.Locate) {
							return;
						}
						if (const auto located = given.Locate(aProgram)) {
							found.push_back({ .Agent = aAgent, .Path = located->Path, .How = found_how(located->Origin) });
						}
					};
					if (!given.Wrapper.empty()) {
						// A saved wrapper always wins; a refused one is not replaced
						// by a search, exactly as at launch.
						if (runnable(given.Wrapper)) {
							found.push_back({ .Agent = core::setup_agent::claude_code, .Path = given.Wrapper, .How = core::agent_found_how::saved_program });
						}
					} else {
						searched(core::setup_agent::claude_code, core::agent_program::claude);
					}
					if (!given.Codex.empty()) {
						if (runnable(given.Codex)) {
							found.push_back({ .Agent = core::setup_agent::codex, .Path = given.Codex, .How = core::agent_found_how::saved_program });
						}
					} else {
						searched(core::setup_agent::codex, core::agent_program::codex);
					}
					if (!given.Harness.empty() && runnable(given.Harness)) {
						found.push_back({ .Agent = core::setup_agent::plain_text_harness, .Path = given.Harness, .How = core::agent_found_how::saved_program });
					}
					return found;
				},
				[self = shared_from_this(), generation](std::vector<core::agent_detection> aFound) {
					if (generation != self->DetectGeneration) {
						return;
					}
					self->Found                = std::move(aFound);
					self->Projection.Detecting = false;
					self->Projection.Detected  = true;
					self->publish();
				});
		}

		agent_setup_dependencies               Dependencies;
		agent_setup_projection                 Projection;
		std::optional<core::agent_host_policy> Policy;
		std::vector<core::agent_detection>     Found;
		/// The reviewer's own pick in this window, before or after it is saved.
		std::optional<core::setup_agent> Picked;
		std::uint64_t                    DetectGeneration  = 0U;
		std::uint64_t                    CheckGeneration   = 0U;
		std::uint64_t                    ProgramGeneration = 0U;
		std::stop_source                 CheckStop;
		std::atomic_bool                 Closing{ false };
		std::atomic_size_t               Outstanding{ 0U };
	};

	agent_setup_host::agent_setup_host(agent_setup_dependencies aDependencies)
		: Implementation{ std::make_shared<implementation>(std::move(aDependencies)) }
	{ }

	agent_setup_host::~agent_setup_host() { shutdown(); }

	void agent_setup_host::load()
	{
		auto& host = *Implementation;
		if (host.Dependencies.Store == nullptr) {
			host.publish();
			return;
		}
		host.off_lane(
			host.Dependencies.Effects, [store = host.Dependencies.Store] { return store->load(); },
			[self = Implementation](std::optional<agent_setup_preferences> aLoaded) {
				if (aLoaded && valid(*aLoaded)) {
					self->Projection.Preferences = std::move(*aLoaded);
				}
				self->apply_programs();
				self->publish();
			});
	}

	void agent_setup_host::observe_policy(std::optional<core::agent_host_policy> aPolicy)
	{
		auto& host = *Implementation;
		if (host.Policy == aPolicy) {
			return;
		}
		host.Policy = std::move(aPolicy);
		host.publish();
	}

	void agent_setup_host::detect() { Implementation->detect(); }

	void agent_setup_host::choose(const core::setup_agent aAgent)
	{
		auto& host = *Implementation;
		if (host.Picked == aAgent && host.Projection.Preferences.Chosen == aAgent) {
			return;
		}
		host.Picked = aAgent;
		if (host.Projection.State.Chosen != aAgent) {
			host.reset_checks();
		}
		host.Projection.Preferences.Chosen = aAgent;
		host.save();
		host.publish();
	}

	void agent_setup_host::check()
	{
		auto&             host = *Implementation;
		const auto* const row  = host.chosen_row();
		if (row == nullptr || host.Projection.Checking) {
			return;
		}
		const auto generation            = ++host.CheckGeneration;
		const auto agent                 = row->Agent;
		const auto path                  = row->Path;
		host.CheckStop                   = std::stop_source{ };
		host.Projection.Checking         = true;
		host.Projection.State.AgentCheck = { .State = core::setup_check_state::running };
		host.Projection.CheckDetail.clear();
		host.publish();
		if (agent == core::setup_agent::plain_text_harness) {
			// No status command: the check is that its program can run now.
			host.off_lane(
				host.Dependencies.Effects, [inspect = host.Dependencies.Inspect, path] { return inspect ? inspect(path) : std::optional{ core::agent_wrapper_refusal::missing }; },
				[self = Implementation, generation, agent](std::optional<core::agent_wrapper_refusal> aRefusal) {
					self->settle_check(generation, agent, aRefusal ? core::fail_check(core::agent_setup_failure::program_refused) : core::setup_check{ .State = core::setup_check_state::passed },
									   aRefusal ? std::string{ core::describe(*aRefusal) } : std::string{ });
				});
			return;
		}
		const auto profile = std::ranges::find(host.Dependencies.Profiles, core::setup_profile(agent, host.Projection.Preferences.CodexProtocol), &core::agent_profile::Id);
		auto       built   = profile == host.Dependencies.Profiles.end() ? std::nullopt : build_agent_status_launches(agent, *profile, path, host.Dependencies.Parent);
		if (!built || host.Dependencies.Process == nullptr) {
			host.settle_check(generation, agent, core::fail_check(core::agent_setup_failure::failed_to_start), std::string{ "status_launch_unavailable" });
			return;
		}
		struct answer
		{
			core::setup_check Check;
			std::string       Detail;
		};
		host.off_lane(
			host.Dependencies.Checks,
			[process = host.Dependencies.Process, launches = std::move(*built), agent, stop = host.CheckStop.get_token()]() mutable {
				const auto status = process->run(launches.Status, agent_status_timeout_milliseconds, { }, stop);
				if (const auto failure = unanswered(status.Outcome)) {
					return answer{ .Check = core::fail_check(*failure), .Detail = detail_of(status) };
				}
				// The version is asked only of a signed-in agent.
				auto decoded = decode_agent_status(agent, status, agent_run_result{ });
				if (decoded.Status == core::agent_status::signed_in && !stop.stop_requested()) {
					decoded = decode_agent_status(agent, status, process->run(launches.Version, agent_status_timeout_milliseconds, { }, stop));
				}
				return answer{ .Check = core::apply_status_check(decoded), .Detail = detail_of(status) };
			},
			[self = Implementation, generation, agent](answer aAnswer) { self->settle_check(generation, agent, std::move(aAnswer.Check), std::move(aAnswer.Detail)); });
	}

	void agent_setup_host::stop()
	{
		auto& host = *Implementation;
		if (!host.Projection.Checking) {
			return;
		}
		host.CheckStop.request_stop();
		if (host.Dependencies.Process != nullptr) {
			host.Dependencies.Process->cancel();
		}
		host.reset_checks();
		host.publish();
	}

	void agent_setup_host::set_program(const core::setup_agent aAgent, std::string aPath, std::function<void(std::optional<core::agent_wrapper_refusal>)> aAnswered)
	{
		auto& host = *Implementation;
		if (aAgent == core::setup_agent::claude_code) {
			return;
		}
		const auto generation = ++host.ProgramGeneration;
		const auto apply      = [self = Implementation, aAgent](std::string aSaved) {
			auto& preferences                                                                            = self->Projection.Preferences;
			(aAgent == core::setup_agent::codex ? preferences.CodexProgram : preferences.HarnessProgram) = std::move(aSaved);
			self->apply_programs();
			self->save();
			self->publish();
		};
		if (aPath.empty()) {
			apply({ });
			if (aAnswered) {
				aAnswered(std::nullopt);
			}
			host.detect();
			return;
		}
		if (!core::storable_agent_wrapper(aPath)) {
			if (aAnswered) {
				aAnswered(core::inspect_agent_wrapper_text(aPath, core::agent_wrapper_platform::posix));
			}
			return;
		}
		// The inspection's own copy, made before either lambda is built.
		auto inspected = aPath;
		host.off_lane(
			host.Dependencies.Effects,
			[inspect = host.Dependencies.Inspect, path = std::move(inspected)] { return inspect ? inspect(path) : std::optional{ core::agent_wrapper_refusal::missing }; },
			[self = Implementation, generation, apply, path = std::move(aPath), answered = std::move(aAnswered)](std::optional<core::agent_wrapper_refusal> aRefusal) mutable {
				if (generation != self->ProgramGeneration) {
					return;
				}
				if (!aRefusal) {
					apply(std::move(path));
				}
				if (answered) {
					answered(aRefusal);
				}
				if (!aRefusal) {
					self->detect();
				}
			});
	}

	void agent_setup_host::set_codex_protocol(const core::codex_protocol_choice aChoice)
	{
		auto& host = *Implementation;
		if (host.Projection.Preferences.CodexProtocol == aChoice) {
			return;
		}
		host.Projection.Preferences.CodexProtocol = aChoice;
		host.save();
		host.publish();
	}

	void agent_setup_host::set_repository_chosen(const bool aChosen)
	{
		auto& host = *Implementation;
		if (host.Projection.State.RepositoryChosen == aChosen) {
			return;
		}
		host.Projection.State.RepositoryChosen = aChosen;
		host.publish();
	}

	void agent_setup_host::set_workspace_check(core::setup_check aCheck)
	{
		auto& host = *Implementation;
		if (host.Projection.State.WorkspaceCheck == aCheck) {
			return;
		}
		host.Projection.State.WorkspaceCheck = std::move(aCheck);
		host.publish();
	}

	void agent_setup_host::restart()
	{
		auto& host = *Implementation;
		host.reset_checks();
		host.Projection.State.WorkspaceCheck = { };
		host.detect();
	}

	auto agent_setup_host::projection() const -> agent_setup_projection { return Implementation->Projection; }

	void agent_setup_host::shutdown() noexcept
	{
		auto& host   = *Implementation;
		host.Closing = true;
		host.CheckStop.request_stop();
		if (host.Dependencies.Process != nullptr) {
			host.Dependencies.Process->cancel();
		}
	}

	auto agent_setup_host::is_quiescent() const noexcept -> bool { return Implementation->Outstanding == 0U; }
} // namespace bpr::application
