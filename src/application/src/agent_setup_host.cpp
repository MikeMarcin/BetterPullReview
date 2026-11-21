#include "bpr/application/agent_setup_host.hpp"
#include "bpr/application/operation_diagnostics.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <initializer_list>
#include <random>
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

		/// [[BPR-2348]]. What a custom agent's check ran: its program by path
		/// and its arguments as the entry lists them, then, when the check
		/// added its own, that it did. Nothing here is not already in the entry.
		[[nodiscard]] auto ran_words(const std::string& aProgram, const std::vector<std::string>& aArguments, const bool aCheckAdds) -> std::string
		{
			auto words = "Ran: " + aProgram;
			for (const auto& argument : aArguments) {
				words += ' ';
				words += argument;
			}
			if (aCheckAdds) {
				words += " …and the check's own";
			}
			return words;
		}

		/// The details: what ran first, then the run's own, bounded together.
		[[nodiscard]] auto ran_with(std::string aRan, std::string aDetail) -> std::string
		{
			if (aRan.size() + 1U >= maximum_agent_check_detail_bytes) {
				aRan.resize(maximum_agent_check_detail_bytes / 2U);
			}
			const auto room = maximum_agent_check_detail_bytes - aRan.size() - 1U;
			if (aDetail.size() > room) {
				aDetail.erase(0U, aDetail.size() - room);
			}
			return aDetail.empty() ? aRan : aRan + "\n" + aDetail;
		}

		[[nodiscard]] auto detail_of(const agent_run_result& aResult) -> std::string
		{
			auto outcome = std::string{ name(aResult.Outcome) };
			if (aResult.ExitCode) {
				outcome += " (exit " + std::to_string(*aResult.ExitCode) + ")";
			}
			auto output = std::string{ };
			for (const auto& event : aResult.Events) {
				output += '\n';
				output += bounded(event.Text);
				output = bounded(std::move(output));
			}
			if (!aResult.StandardError.empty()) {
				output += "\nstderr:\n";
				output += bounded(aResult.StandardError);
				output = bounded(std::move(output));
			}
			const auto available = maximum_agent_check_detail_bytes - outcome.size();
			return outcome + output.substr(output.size() > available ? output.size() - available : 0U);
		}

		/// [[BPR-1828]]. What a check's last process run said, for its
		/// diagnostic line: counts and an exit code, never its output.
		struct check_run
		{
			core::operation_process Process = core::operation_process::none;
			std::optional<int>      ExitCode;
			std::uint32_t           Runs = 0U;
		};

		[[nodiscard]] auto run_of(const agent_run_result& aResult, const std::uint32_t aRuns) -> check_run
		{
			return { .Process = operation_process_of(aResult.Outcome), .ExitCode = aResult.ExitCode, .Runs = aRuns };
		}

		/// [[BPR-2305]]. What a custom Codex JSONL agent's instructions probe
		/// runs with; [[BPR-2320]] and a custom Claude agent's stream JSON
		/// turn.
		struct codex_probe
		{
			core::agent_profile              Profile;
			std::string                      Program;
			std::vector<std::string>         Leading;
			core::agent_instruction_delivery Delivery = core::agent_instruction_delivery::system_prompt;
			environment_reader               Parent;
			agent_run_checks                 Checks = agent_run_checks::strict;
		};

		/// [[BPR-2305]]. A model answers in about this long; more is a check
		/// that did not answer.
		constexpr auto codex_probe_timeout_milliseconds = 120'000;

		/// The first line a check printed, bounded: a launcher's version.
		[[nodiscard]] auto first_line_of(const agent_run_result& aResult) -> std::string
		{
			for (const auto& event : aResult.Events) {
				const auto text = std::string_view{ event.Text };
				const auto line = text.substr(0U, text.find('\n'));
				if (!line.empty()) {
					return std::string{ line.substr(0U, 120U) };
				}
			}
			return { };
		}

		struct probe_answer
		{
			core::setup_check Check;
			std::string       Detail;
			check_run         Run;
		};

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

		/// [[BPR-2305]]. Asks a custom Codex JSONL agent the probe's question
		/// with the rule given as its Instructions setting says. Followed: the
		/// check passes. An answer that is not Codex's JSON lines is a program
		/// that runs Codex under a terminal (a launcher before its pipe mode),
		/// named with the version its own check printed. Ignored as developer
		/// instructions: asked again with the rule at the start of the prompt,
		/// and when that is followed the check says so (`instructions_dropped`)
		/// so setup can offer the switch.
		[[nodiscard]] auto probe_codex_instructions(agent_process_port& aProcess, const codex_probe& aProbe, const std::string& aVersion, const std::stop_token& aStop) -> probe_answer
		{
			const auto run = [&](const core::agent_instruction_delivery aDelivery) {
				return aProcess.run(build_codex_instructions_probe(aProbe.Profile, aProbe.Program, aProbe.Leading, aDelivery, aProbe.Parent), codex_probe_timeout_milliseconds, { }, aStop);
			};
			const auto first = run(aProbe.Delivery);
			auto       ran   = run_of(first, 1U);
			if (codex_instructions_followed(first)) {
				return { .Check = core::setup_check{ .State = core::setup_check_state::passed, .Version = aVersion }, .Run = ran };
			}
			if (first.Outcome == agent_run_outcome::protocol_failure
				&& (first.ProtocolFailure == agent_protocol_failure::malformed_frame || first.ProtocolFailure == agent_protocol_failure::incomplete_frame)) {
				return { .Check = core::fail_check(core::agent_setup_failure::output_unreadable), .Detail = (aVersion.empty() ? std::string{ } : "version: " + aVersion + "\n") + detail_of(first), .Run = ran };
			}
			if (first.Outcome != agent_run_outcome::success) {
				return { .Check = core::fail_check(unanswered(first.Outcome).value_or(core::agent_setup_failure::status_unreadable)), .Detail = detail_of(first), .Run = ran };
			}
			if (aProbe.Delivery == core::agent_instruction_delivery::system_prompt && !aStop.stop_requested()) {
				const auto second = run(core::agent_instruction_delivery::message);
				ran               = run_of(second, 2U);
				if (codex_instructions_followed(second)) {
					return { .Check = core::fail_check(core::agent_setup_failure::instructions_dropped), .Detail = detail_of(first), .Run = ran };
				}
			}
			return { .Check = core::fail_check(core::agent_setup_failure::instructions_not_followed), .Detail = detail_of(first), .Run = ran };
		}

		/// [[BPR-2320]]. Asks a custom Claude agent one short tool-free turn
		/// through its program. It passes only when the answer arrives as
		/// stream JSON with Claude's init frame. A program that writes
		/// nothing, or text that is not the protocol (a double-click entry
		/// that opens a terminal, a launcher's banner), is named with the
		/// first line it wrote and the version its own check printed.
		[[nodiscard]] auto probe_claude_stream(agent_process_port& aProcess, const codex_probe& aProbe, const std::string& aVersion, const std::stop_token& aStop) -> probe_answer
		{
			const auto answer = aProcess.run(build_claude_stream_probe(aProbe.Profile, aProbe.Program, aProbe.Leading, aProbe.Parent, aProbe.Checks), codex_probe_timeout_milliseconds, { }, aStop);
			const auto ran    = run_of(answer, 1U);
			if (claude_stream_answered(answer)) {
				return { .Check = core::setup_check{ .State = core::setup_check_state::passed, .Version = aVersion }, .Run = ran };
			}
			const auto unreadable = answer.Outcome == agent_run_outcome::success
				|| (answer.Outcome == agent_run_outcome::protocol_failure
					&& (answer.ProtocolFailure == agent_protocol_failure::malformed_frame || answer.ProtocolFailure == agent_protocol_failure::incomplete_frame));
			if (!unreadable) {
				return { .Check = core::fail_check(unanswered(answer.Outcome).value_or(core::agent_setup_failure::status_unreadable)), .Detail = detail_of(answer), .Run = ran };
			}
			auto wrote = answer.RejectedOutputLine.empty() ? first_line_of(answer) : answer.RejectedOutputLine.substr(0U, 120U);
			std::ranges::replace(wrote, '\n', ' ');
			auto detail = aVersion.empty() ? std::string{ } : "version: " + aVersion + "\n";
			detail += "wrote: " + (wrote.empty() ? std::string{ "nothing" } : wrote) + "\n";
			return { .Check = core::fail_check(core::agent_setup_failure::output_unreadable), .Detail = detail + detail_of(answer), .Run = ran };
		}
	} // namespace

	namespace {
		auto mint_agent_identity() -> std::string
		{
			auto           random   = std::random_device{ };
			auto           identity = std::string{ };
			constexpr auto digits   = std::string_view{ "0123456789abcdef" };
			for (auto index = 0U; index < 32U; ++index) {
				identity += digits[random() & 15U];
			}
			return identity;
		}
	}

	struct agent_setup_host::implementation final : std::enable_shared_from_this<implementation>
	{
		explicit implementation(agent_setup_dependencies aDependencies)
			: Dependencies{ std::move(aDependencies) }
		{
			Projection.PolicyPending = true;
		}

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

		/// The rows, choice and step from what was found and saved.
		void derive()
		{
			auto& state              = Projection.State;
			state.Rows               = core::agent_setup_rows(Found, Dependencies.Profiles, Policy, Projection.Preferences.CodexProtocol);
			const auto saved         = Picked ? Picked : Projection.Preferences.Chosen;
			state.Chosen             = core::choose_setup_agent(state.Rows, saved);
			const auto* const row    = chosen_row();
			const auto* const entry  = preferred_entry();
			Projection.ChecksSignIn  = row != nullptr && row->Agent != core::setup_agent::plain_text_harness && (entry == nullptr || entry->Preset.has_value());
			Projection.ChecksWrapper = row != nullptr && row->Agent == core::setup_agent::claude_code && row->How == core::agent_found_how::saved_program
				&& (entry == nullptr || entry->Preset);
			Projection.Searched = core::searched_setup_agents(Dependencies.Profiles, Policy, Projection.Preferences.CodexProtocol);
			Projection.Step     = core::current_setup_step(state);
			Projection.Names    = Dependencies.Names;
			// Owner report: the Agent step shows the agent list as it is now,
			// whichever surface changed it.
			Projection.Choices = core::agent_setup_choices({
				.Entries   = Projection.Preferences.Agents,
				.Preferred = Projection.Preferences.PreferredAgent,
				.Chosen    = state.Chosen,
				.Found     = Found,
				.Located   = Located,
				.Profiles  = Dependencies.Profiles,
				.Policy    = Policy,
				.Codex     = Projection.Preferences.CodexProtocol,
			});
		}

		void publish()
		{
			derive();
			// [[BPR-2215]]. A running workspace check keeps the instant it
			// started across every projection until it ends.
			if (Projection.State.WorkspaceCheck.State != core::setup_check_state::running) {
				Projection.WorkspaceCheckStartedMs.reset();
			} else if (!Projection.WorkspaceCheckStartedMs) {
				Projection.WorkspaceCheckStartedMs = Dependencies.SteadyMilliseconds ? Dependencies.SteadyMilliseconds()
																					 : std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
			}
			++Projection.Generation;
			if (Dependencies.Publish) {
				try {
					Dependencies.Publish(Projection);
				} catch (...) { }
			}
		}

		/// [[BPR-2254]]. While the saved settings cannot be read, nothing is
		/// written over them and no change is kept.
		[[nodiscard]] auto settings_unreadable() const noexcept -> bool { return Projection.SettingsUnreadable; }

		void save()
		{
			if (Dependencies.Store == nullptr || settings_unreadable() || !valid(Projection.Preferences)) {
				return;
			}
			off_lane(
				Dependencies.Effects, [store = Dependencies.Store, preferences = Projection.Preferences] { return store->store(preferences); }, [](bool) { });
		}

		/// [[BPR-1753]]. Agents are a list. Settings kept before they were
		/// are not read into it (owner, 2026-11-20: no legacy support): an
		/// unmoved profile starts with the list it has.
		void move_into_list()
		{
			auto& preferences       = Projection.Preferences;
			auto  changed           = !preferences.AgentsMoved;
			preferences.AgentsMoved = true;
			for (auto& entry : preferences.Agents) {
				if (entry.Identity.empty()) {
					entry.Identity = mint_agent_identity();
					changed        = true;
				}
			}
			if (changed) {
				save();
			}
		}

		/// The entry setup chose, when there is one; while Check all runs,
		/// the entry it checks now.
		[[nodiscard]] auto preferred_entry() const -> const core::agent_entry*
		{
			const auto& agents = Projection.Preferences.Agents;
			if (CheckingOverride && Projection.CheckingEntry) {
				const auto checking = std::ranges::find(agents, *Projection.CheckingEntry, &core::agent_entry::Id);
				return checking == agents.end() ? nullptr : &*checking;
			}
			const auto found = std::ranges::find(agents, Projection.Preferences.PreferredAgent, &core::agent_entry::Id);
			if (Projection.Preferences.PreferredAgent != 0U && found != agents.end()) {
				return &*found;
			}
			// [[BPR-1769]]. Detection can choose the sole installed agent
			// before a preferred entry was saved (including a migrated wrapper).
			// Its existing preset is still the effective program and arguments.
			const auto chosen = Projection.State.Chosen ? Projection.State.Chosen : Projection.Preferences.Chosen;
			return chosen ? core::find_preset_entry(agents, *chosen) : nullptr;
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
			// [[BPR-1828]]. A check still running is cancelled here; one
			// still waiting for the agents to be looked for never began.
			if (Projection.Checking && !PendingCheck) {
				record_operation(Dependencies.Log.get(), { .Action = CheckAction, .Failure = core::operation_failure::cancelled, .DurationMilliseconds = CheckWatch.milliseconds() });
			}
			++CheckGeneration;
			PendingCheck     = false;
			CheckingOverride = false;
			CheckQueue.clear();
			Projection.CheckingEntry.reset();
			Projection.CheckedEntry.reset();
			Projection.CheckStep        = agent_check_step::unknown;
			Projection.State.AgentCheck = { };
			Projection.Checking         = false;
			Projection.CheckDetail.clear();
		}

		[[nodiscard]] auto chosen_row() const -> const core::agent_setup_row*
		{
			const auto& state = Projection.State;
			auto        agent = state.Chosen;
			// [[BPR-2051]]. Check all checks each entry's own kind of agent.
			if (CheckingOverride) {
				const auto* const entry = preferred_entry();
				agent                   = entry == nullptr ? std::nullopt : std::optional{ entry->Preset ? *entry->Preset : core::setup_agent_of(entry->Protocol) };
			}
			if (!agent) {
				return nullptr;
			}
			const auto row = std::ranges::find(state.Rows, *agent, &core::agent_setup_row::Agent);
			return row == state.Rows.end() || !row->Verdict.Permitted ? nullptr : &*row;
		}

		/// \brief [[BPR-2348]]. The entry being checked when it names its own
		/// Program and its host's policy permits it: it is checked with that
		/// Program, whatever detection found for its kind. None otherwise, and
		/// the check takes the program found for the entry's kind.
		[[nodiscard]] auto own_program_entry() const -> const core::agent_entry*
		{
			const auto* const entry = preferred_entry();
			if (entry == nullptr || entry->Program.empty()) {
				return nullptr;
			}
			const auto choice = std::ranges::find(Projection.Choices, entry->Id, &core::agent_setup_choice::Entry);
			return choice != Projection.Choices.end() && choice->Verdict.Permitted ? entry : nullptr;
		}

		/// [[BPR-1828]]. A check's start: its kind and its stopwatch.
		void begin_check(const core::operation_action aAction)
		{
			CheckAction = aAction;
			CheckWatch  = operation_stopwatch{ };
			record_operation(Dependencies.Log.get(), { .Action = aAction }, true);
		}

		/// [[BPR-2254]]. A check while the saved settings cannot be read
		/// checks nothing: none of the reviewer's agents is known, so it says
		/// that, naming the file, instead of an agent that is not found.
		void refuse_check()
		{
			CheckQueue.clear();
			CheckingOverride = false;
			Projection.CheckingEntry.reset();
			const auto generation = ++CheckGeneration;
			const auto agent      = Picked ? *Picked : Projection.Preferences.Chosen.value_or(core::setup_agent::claude_code);
			begin_check(core::operation_action::agent_status_check);
			settle_check(generation, agent, core::fail_check(core::agent_setup_failure::settings_unreadable), Projection.SettingsFile, { }, agent_check_step::unknown);
		}

		void settle_check(const std::uint64_t aGeneration, const core::setup_agent aAgent, core::setup_check aCheck, std::string aDetail, const check_run aRun = { },
						  const agent_check_step aStep = agent_check_step::sign_in)
		{
			if (aGeneration != CheckGeneration) {
				return;
			}
			const auto failure = aCheck.State == core::setup_check_state::passed ? core::operation_failure::none
																				 : operation_failure_of(aCheck.Failure.value_or(core::agent_setup_failure::failed_to_start));
			record_operation(Dependencies.Log.get(), { .Action = CheckAction, .Failure = failure, .Process = aRun.Process, .ExitCode = aRun.Process == core::operation_process::none ? std::nullopt : aRun.ExitCode, .DurationMilliseconds = CheckWatch.milliseconds(), .Count = aRun.Runs });
			Projection.Checking         = false;
			Projection.State.AgentCheck = std::move(aCheck);
			Projection.CheckDetail      = Projection.State.AgentCheck.State == core::setup_check_state::passed ? std::string{ } : std::move(aDetail);
			// [[BPR-2051]]. The entry's last check is kept across restarts.
			// [[BPR-2320]]. The result names the entry it checked.
			Projection.CheckStep = aStep;
			Projection.CheckedEntry.reset();
			if (const auto* const entry = preferred_entry(); entry != nullptr) {
				keep_entry_check(entry->Id, Projection.State.AgentCheck);
				Projection.CheckedEntry = entry->Id;
			}
			if (CheckingOverride) {
				if (!CheckQueue.empty()) {
					Projection.CheckingEntry = CheckQueue.front();
					CheckQueue.erase(CheckQueue.begin());
					publish();
					start_check();
					return;
				}
				// Check all is done: the chosen agent's own result again.
				CheckingOverride = false;
				Projection.CheckingEntry.reset();
				const auto* const chosen = preferred_entry();
				const auto* const kept   = chosen != nullptr ? entry_check(Projection.Preferences, chosen->Id) : nullptr;
				if (kept != nullptr) {
					Projection.State.AgentCheck = *kept;
					Projection.CheckedEntry     = chosen->Id;
					Projection.CheckStep        = agent_check_step::unknown;
				}
				publish();
				return;
			}
			Projection.CheckingEntry.reset();
			if (Projection.State.AgentCheck.State == core::setup_check_state::passed) {
				const auto* const entry     = preferred_entry();
				const auto        preferred = entry != nullptr ? entry->Id : Projection.Preferences.PreferredAgent;
				if (Projection.Preferences.Chosen != aAgent || Projection.Preferences.PreferredAgent != preferred) {
					Projection.Preferences.Chosen         = aAgent;
					Projection.Preferences.PreferredAgent = preferred;
					save();
				}
			}
			publish();
		}

		/// [[BPR-2051]]. Keeps an entry's settled check; a running or
		/// unrun check is not a result.
		void keep_entry_check(const std::uint32_t aId, core::setup_check aCheck)
		{
			if (aCheck.State != core::setup_check_state::passed && aCheck.State != core::setup_check_state::failed) {
				return;
			}
			if (aCheck.State == core::setup_check_state::passed) {
				aCheck.Failure.reset();
			} else if (!aCheck.Failure) {
				aCheck.Failure = core::agent_setup_failure::status_unreadable;
			}
			if (aCheck.Version.size() > maximum_agent_check_version_bytes) {
				aCheck.Version.resize(maximum_agent_check_version_bytes);
				while (!aCheck.Version.empty() && (static_cast<unsigned char>(aCheck.Version.back()) & 0xC0U) == 0x80U) {
					aCheck.Version.pop_back();
				}
				if (!aCheck.Version.empty() && (static_cast<unsigned char>(aCheck.Version.back()) & 0x80U) != 0U) {
					aCheck.Version.pop_back();
				}
			}
			auto&      records = Projection.Preferences.EntryChecks;
			const auto found   = std::ranges::find(records, aId, &agent_entry_check_record::Id);
			if (found != records.end()) {
				if (found->Check == aCheck) {
					return;
				}
				found->Check = std::move(aCheck);
			} else {
				records.push_back({ .Id = aId, .Check = std::move(aCheck) });
				std::ranges::sort(records, { }, &agent_entry_check_record::Id);
			}
			save();
		}

		/// Runs Check it works' agent row on what was found.
		void start_check();

		/// Setup chooses the entry `aId`: its kind is the pick and the checks
		/// start over. A running check is stopped first.
		void prefer_entry(const std::uint32_t aId)
		{
			const auto& agents = Projection.Preferences.Agents;
			const auto  found  = std::ranges::find(agents, aId, &core::agent_entry::Id);
			if (found == agents.end()) {
				return;
			}
			CheckStop.request_stop();
			if (Projection.Checking && Dependencies.Process != nullptr) {
				Dependencies.Process->cancel();
			}
			Projection.Preferences.PreferredAgent = aId;
			Picked                                = core::setup_agent_of(found->Protocol);
			Projection.Preferences.Chosen         = Picked;
			reset_checks();
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
			auto snapshot = inputs{ };
			// [[BPR-1769]]. The list (including an empty reset list) decides
			// whether a saved wrapper exists.
			const auto* const claude = core::find_preset_entry(host.Projection.Preferences.Agents, core::setup_agent::claude_code);
			snapshot.Wrapper         = claude != nullptr ? claude->Program : std::string{ };
			snapshot.Codex           = host.Projection.Preferences.CodexProgram;
			snapshot.Harness         = host.Projection.Preferences.HarnessProgram;
			snapshot.Inspect         = host.Dependencies.Inspect;
			snapshot.Locate          = host.Dependencies.Locate;
			// [[BPR-1753]]. A chosen entry with its own program is the
			// program setup checks for its kind, as a saved wrapper was.
			if (const auto* const entry = host.preferred_entry(); entry != nullptr && !entry->Program.empty()) {
				switch (core::setup_agent_of(entry->Protocol)) {
				case core::setup_agent::claude_code:        snapshot.Wrapper = entry->Program; break;
				case core::setup_agent::codex:              snapshot.Codex = entry->Program; break;
				case core::setup_agent::plain_text_harness: snapshot.Harness = entry->Program; break;
				}
			}
			host.off_lane(
				host.Dependencies.Effects,
				[given = std::move(snapshot)] {
					auto       found       = std::vector<core::agent_detection>{ };
					auto       searchedFor = std::vector<core::agent_detection>{ };
					const auto runnable    = [&given](const std::string& aPath) { return given.Inspect && !given.Inspect(aPath); };
					const auto searched    = [&given, &found](const core::setup_agent aAgent, const core::agent_program aProgram) {
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
					// Where each popular agent's own program is, for the Agent
					// step's cards, whatever program its kind is checked with.
					for (const auto& [agent, program] : { std::pair{ core::setup_agent::claude_code, core::agent_program::claude }, std::pair{ core::setup_agent::codex, core::agent_program::codex } }) {
						const auto searchedAlready = std::ranges::find_if(found, [agent](const core::agent_detection& aFound) {
							return aFound.Agent == agent && aFound.How != core::agent_found_how::saved_program;
						});
						if (searchedAlready != found.end()) {
							searchedFor.push_back(*searchedAlready);
						} else if (given.Locate) {
							if (const auto candidate = given.Locate(program)) {
								searchedFor.push_back({ .Agent = agent, .Path = candidate->Path, .How = found_how(candidate->Origin) });
							}
						}
					}
					return std::pair{ std::move(found), std::move(searchedFor) };
				},
				[self = shared_from_this(), generation](std::pair<std::vector<core::agent_detection>, std::vector<core::agent_detection>> aFound) {
					if (generation != self->DetectGeneration) {
						return;
					}
					self->Found                = std::move(aFound.first);
					self->Located              = std::move(aFound.second);
					self->Projection.Detecting = false;
					self->Projection.Detected  = true;
					// [[BPR-1859]]. A check asked for before the agents were
					// looked for runs now, on what was found.
					if (std::exchange(self->PendingCheck, false)) {
						self->Projection.Checking         = false;
						self->Projection.State.AgentCheck = { };
						self->derive();
						self->start_check();
						return;
					}
					self->publish();
				});
		}

		agent_setup_dependencies               Dependencies;
		agent_setup_projection                 Projection;
		std::optional<core::agent_host_policy> Policy;
		std::vector<core::agent_detection>     Found;
		/// Where each popular agent's own program was found by searching.
		std::vector<core::agent_detection> Located;
		/// The reviewer's own pick in this window, before or after it is saved.
		std::optional<core::setup_agent> Picked;
		std::uint64_t                    DetectGeneration  = 0U;
		std::uint64_t                    CheckGeneration   = 0U;
		std::uint64_t                    ProgramGeneration = 0U;
		std::stop_source                 CheckStop;
		/// [[BPR-1859]]. A check waits for the agents to be looked for.
		bool PendingCheck = false;
		/// [[BPR-2051]]. Check all: the entries still to check after the one
		/// `Projection.CheckingEntry` names, which a check of one entry
		/// sets too ("CheckingAlone").
		std::vector<std::uint32_t> CheckQueue;
		bool                       CheckingOverride = false;
		/// [[BPR-1828]]. The running check's kind and when it started.
		core::operation_action CheckAction = core::operation_action::agent_status_check;
		operation_stopwatch    CheckWatch;
		std::atomic_bool       Closing{ false };
		std::atomic_size_t     Outstanding{ 0U };
	};

	auto agent_check_facts_of(const agent_setup_projection& aProjection, const bool aReviewOpen) -> core::agent_check_facts
	{
		const auto& state       = aProjection.State;
		const auto  saved       = aProjection.Preferences.Chosen;
		const auto  row         = [&state](const core::setup_agent aAgent) { return std::ranges::find(state.Rows, aAgent, &core::agent_setup_row::Agent); };
		const auto  savedAt     = saved ? row(*saved) : state.Rows.end();
		auto        facts       = core::agent_check_facts{ };
		facts.AgentsAdded       = !aProjection.Preferences.Agents.empty();
		facts.Detected          = aProjection.Detected;
		facts.Chosen            = state.Chosen && row(*state.Chosen) != state.Rows.end() && row(*state.Chosen)->Verdict.Permitted;
		facts.SavedChoice       = saved.has_value();
		facts.SavedForbidden    = savedAt != state.Rows.end() && !savedAt->Verdict.Permitted;
		facts.AgentChecking     = aProjection.Checking;
		facts.WorkspaceChecking = state.WorkspaceCheck.State == core::setup_check_state::running;
		facts.ReviewOpen        = aReviewOpen;
		return facts;
	}

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
				} else {
					// [[BPR-2254]]. Not a fresh profile: the defaults stand in
					// for settings that exist and could not be read.
					self->Projection.SettingsUnreadable = true;
					self->Projection.SettingsFile       = self->Dependencies.StoreFile;
				}
				const auto searched = self->Projection.Detecting || self->Projection.Detected;
				if (!self->settings_unreadable()) {
					self->move_into_list();
				}
				self->apply_programs();
				if (searched) {
					// A first look may have snapshotted the legacy/default values
					// while this read was pending. Only loaded settings may land.
					self->detect();
				} else {
					self->publish();
				}
			});
	}

	void agent_setup_host::observe_policy(std::optional<core::agent_host_policy> aPolicy)
	{
		auto& host = *Implementation;
		if (!host.Projection.PolicyPending && host.Policy == aPolicy) {
			return;
		}
		host.Projection.PolicyPending = false;
		host.Policy                   = std::move(aPolicy);
		host.publish();
	}

	void agent_setup_host::forget_policy()
	{
		auto& host = *Implementation;
		if (host.Projection.PolicyPending && !host.Policy) {
			return;
		}
		host.Projection.PolicyPending = true;
		host.Policy.reset();
		host.publish();
	}

	void agent_setup_host::detect() { Implementation->detect(); }

	void agent_setup_host::choose(const core::setup_agent aAgent)
	{
		auto& host = *Implementation;
		if (host.settings_unreadable()) {
			return;
		}
		const auto* const current  = host.preferred_entry();
		const auto        previous = current != nullptr ? current->Id : 0U;
		// Another entry of the same kind (a custom agent) is not this popular
		// agent: picking the popular one still switches to it.
		const auto popular = current == nullptr || current->Preset == aAgent
			|| (aAgent == core::setup_agent::plain_text_harness && current->Protocol == core::agent_protocol::external_terminal && current->Program == host.Projection.Preferences.HarnessProgram);
		if (host.Picked == aAgent && host.Projection.Preferences.Chosen == aAgent && popular) {
			return;
		}
		const auto kindChanged             = host.Projection.State.Chosen != aAgent;
		host.Picked                        = aAgent;
		host.Projection.Preferences.Chosen = aAgent;
		// [[BPR-1753]]. Picking a popular agent adds it to the list once
		// and chooses that entry; a harness is the entry with its program.
		auto&       preferences = host.Projection.Preferences;
		const auto* entry       = aAgent == core::setup_agent::plain_text_harness
			? [&preferences]() -> const core::agent_entry* {
			const auto found = std::ranges::find_if(preferences.Agents, [&preferences](const core::agent_entry& aEntry) {
				return aEntry.Protocol == core::agent_protocol::external_terminal && aEntry.Program == preferences.HarnessProgram;
			});
			return found == preferences.Agents.end() ? nullptr : &*found;
		}()
			: core::find_preset_entry(preferences.Agents, aAgent);
		if (entry == nullptr && (aAgent != core::setup_agent::plain_text_harness || !preferences.HarnessProgram.empty())) {
			auto preset     = core::preset_agent_entry(aAgent, preferences.CodexProtocol, aAgent == core::setup_agent::plain_text_harness ? preferences.HarnessProgram : std::string{ });
			preset.Identity = mint_agent_identity();
			if (auto added = core::add_agent_entry(preferences.Agents, std::move(preset))) {
				preferences.Agents = std::move(*added);
				entry              = &preferences.Agents.back();
			}
		}
		preferences.PreferredAgent = entry != nullptr ? entry->Id : 0U;
		// A different entry's program is what setup checks now: the checks
		// start over and, once setup has looked, the agents are looked for
		// again with it.
		const auto switched = previous != 0U && previous != preferences.PreferredAgent;
		if (kindChanged || switched) {
			host.reset_checks();
		}
		host.save();
		host.publish();
		if (switched && (host.Projection.Detected || host.Projection.Detecting)) {
			host.detect();
		}
	}

	void agent_setup_host::choose_entry(const std::uint32_t aId)
	{
		auto& host = *Implementation;
		if (host.settings_unreadable()) {
			return;
		}
		const auto&       agents  = host.Projection.Preferences.Agents;
		const auto        found   = std::ranges::find(agents, aId, &core::agent_entry::Id);
		const auto* const current = host.preferred_entry();
		if (found == agents.end()
			|| (current != nullptr && current->Id == aId && host.Projection.Preferences.PreferredAgent == aId && host.Projection.State.Chosen == core::setup_agent_of(found->Protocol))) {
			return;
		}
		host.prefer_entry(aId);
		host.save();
		host.publish();
		host.detect();
	}

	void agent_setup_host::add_agent(core::agent_entry aEntry, const bool aPrefer, agent_entry_answer aAnswered)
	{
		auto& host = *Implementation;
		if (host.settings_unreadable()) {
			if (aAnswered) {
				aAnswered(core::agent_entry_refusal::settings_unreadable);
			}
			return;
		}
		aEntry.Identity = mint_agent_identity();
		auto added      = core::add_agent_entry(host.Projection.Preferences.Agents, std::move(aEntry));
		if (!added) {
			if (aAnswered) {
				aAnswered(added.error());
			}
			return;
		}
		host.Projection.Preferences.Agents = std::move(*added);
		if (aPrefer) {
			host.prefer_entry(host.Projection.Preferences.Agents.back().Id);
		}
		host.save();
		host.publish();
		if (aPrefer) {
			host.detect();
		}
		if (aAnswered) {
			aAnswered(std::nullopt);
		}
	}

	void agent_setup_host::update_agent(core::agent_entry aEntry, agent_entry_answer aAnswered)
	{
		auto& host = *Implementation;
		if (host.settings_unreadable()) {
			if (aAnswered) {
				aAnswered(core::agent_entry_refusal::settings_unreadable);
			}
			return;
		}
		const auto* const current   = host.preferred_entry();
		const auto        preferred = current != nullptr && current->Id == aEntry.Id;
		const auto        original  = std::ranges::find(host.Projection.Preferences.Agents, aEntry.Id, &core::agent_entry::Id);
		if (original != host.Projection.Preferences.Agents.end()) {
			aEntry.Identity = original->Protocol == aEntry.Protocol && original->Program == aEntry.Program && original->Arguments == aEntry.Arguments
					&& original->StrictRunChecks == aEntry.StrictRunChecks
				? original->Identity
				: mint_agent_identity();
		}
		auto updated = core::update_agent_entry(host.Projection.Preferences.Agents, std::move(aEntry));
		if (!updated) {
			if (aAnswered) {
				aAnswered(updated.error());
			}
			return;
		}
		host.Projection.Preferences.Agents = std::move(*updated);
		if (preferred) {
			host.CheckStop.request_stop();
			if (host.Projection.Checking && host.Dependencies.Process != nullptr) {
				host.Dependencies.Process->cancel();
			}
			host.reset_checks();
		}
		host.save();
		host.publish();
		if (preferred) {
			host.detect();
		}
		if (aAnswered) {
			aAnswered(std::nullopt);
		}
	}

	void agent_setup_host::remove_agent(const std::uint32_t aId)
	{
		if (Implementation->settings_unreadable()) {
			return;
		}
		auto& host    = *Implementation;
		auto  removed = core::remove_agent_entry(host.Projection.Preferences.Agents, aId);
		if (!removed) {
			return;
		}
		const auto* const current          = host.preferred_entry();
		const auto        preferred        = current != nullptr && current->Id == aId;
		host.Projection.Preferences.Agents = std::move(*removed);
		if (host.Projection.Preferences.PreferredAgent == aId) {
			host.Projection.Preferences.PreferredAgent = 0U;
		}
		std::erase_if(host.Projection.Preferences.EntryChecks, [aId](const agent_entry_check_record& aRecord) { return aRecord.Id == aId; });
		// The agent setup chose is gone: its program is no longer what the
		// check runs, so the check starts over and the agents are looked for
		// again, as after an edit.
		if (preferred) {
			host.CheckStop.request_stop();
			if (host.Projection.Checking && host.Dependencies.Process != nullptr) {
				host.Dependencies.Process->cancel();
			}
			host.reset_checks();
		}
		host.save();
		host.publish();
		if (preferred) {
			host.detect();
		}
	}

	void agent_setup_host::move_agent(const std::uint32_t aId, const int aDelta)
	{
		if (Implementation->settings_unreadable()) {
			return;
		}
		auto& host  = *Implementation;
		auto  moved = core::move_agent_entry(host.Projection.Preferences.Agents, aId, aDelta);
		if (!moved || *moved == host.Projection.Preferences.Agents) {
			return;
		}
		host.Projection.Preferences.Agents = std::move(*moved);
		host.save();
		host.publish();
	}

	auto agent_setup_host::set_run_limit(const std::uint32_t aMinutes) -> bool
	{
		auto& host = *Implementation;
		if (host.settings_unreadable()) {
			return false;
		}
		if (aMinutes > maximum_run_limit_minutes) {
			return false;
		}
		if (host.Projection.Preferences.RunLimitMinutes != aMinutes) {
			host.Projection.Preferences.RunLimitMinutes = aMinutes;
			host.save();
			host.publish();
		}
		return true;
	}

	auto agent_setup_host::set_scratch_policy(const std::uint32_t aLimitMiB, const std::uint32_t aKeepDays) -> bool
	{
		auto& host = *Implementation;
		if (host.settings_unreadable()) {
			return false;
		}
		if (aLimitMiB < core::minimum_agent_scratch_limit_mib || aLimitMiB > core::maximum_agent_scratch_limit_mib || aKeepDays > core::maximum_agent_scratch_keep_days) {
			return false;
		}
		auto& preferences = host.Projection.Preferences;
		if (preferences.ScratchLimitMiB != aLimitMiB || preferences.ScratchKeepDays != aKeepDays) {
			preferences.ScratchLimitMiB = aLimitMiB;
			preferences.ScratchKeepDays = aKeepDays;
			host.save();
			host.publish();
		}
		return true;
	}

	auto agent_setup_host::set_workspace_policy(const std::uint32_t aKeepDays, const std::uint32_t aCapGb) -> bool
	{
		auto& host = *Implementation;
		if (host.settings_unreadable()) {
			return false;
		}
		if (aKeepDays > core::maximum_agent_workspace_keep_days || aCapGb < core::minimum_agent_workspace_cap_gb || aCapGb > core::maximum_agent_workspace_cap_gb) {
			return false;
		}
		auto& preferences = host.Projection.Preferences;
		if (preferences.WorkspaceKeepDays != aKeepDays || preferences.WorkspaceCapGb != aCapGb) {
			preferences.WorkspaceKeepDays = aKeepDays;
			preferences.WorkspaceCapGb    = aCapGb;
			host.save();
			host.publish();
		}
		return true;
	}

	void agent_setup_host::set_suggestions_hidden(const bool aHidden)
	{
		auto& host = *Implementation;
		if (host.settings_unreadable()) {
			return;
		}
		if (host.Projection.Preferences.SuggestionsHidden != aHidden) {
			host.Projection.Preferences.SuggestionsHidden = aHidden;
			host.save();
			host.publish();
		}
	}

	void agent_setup_host::set_follow_up_steer(const bool aSteer)
	{
		auto& host = *Implementation;
		if (host.settings_unreadable()) {
			return;
		}
		if (host.Projection.Preferences.FollowUpSteer != aSteer) {
			host.Projection.Preferences.FollowUpSteer = aSteer;
			host.save();
			host.publish();
		}
	}

	void agent_setup_host::load_catalog(const std::uint32_t aEntry)
	{
		auto&       host   = *Implementation;
		const auto& agents = host.Projection.Preferences.Agents;
		const auto  found  = std::ranges::find(agents, aEntry, &core::agent_entry::Id);
		if (found == agents.end() || host.Projection.Catalogs.contains(aEntry) || std::ranges::contains(host.Projection.Cataloguing, aEntry) || host.Dependencies.Process == nullptr) {
			return;
		}
		const auto& entry   = *found;
		const auto  program = core::entry_settings_program(entry.Protocol);
		const auto  profile = std::ranges::find(host.Dependencies.Profiles, entry.Protocol, &core::agent_profile::Protocol);
		if (!program || profile == host.Dependencies.Profiles.end()) {
			return;
		}
		// A preset runs the program setup found; an added agent its own.
		auto path = entry.Program;
		if (path.empty()) {
			const auto agent = core::setup_agent_of(entry.Protocol);
			const auto row   = std::ranges::find(host.Found, agent, &core::agent_detection::Agent);
			path             = row == host.Found.end() ? std::string{ } : row->Path;
		}
		if (path.empty()) {
			return;
		}
		const auto launch = *program == core::agent_program::codex ? build_agent_model_catalog_launch(*profile, path, entry.Arguments, host.Dependencies.Parent)
																   : build_agent_help_launch(*profile, path, entry.Arguments, host.Dependencies.Parent);
		host.Projection.Cataloguing.push_back(aEntry);
		host.publish();
		host.off_lane(
			host.Dependencies.Checks,
			[process = host.Dependencies.Process, launch, codex = *program == core::agent_program::codex] {
				const auto ran     = process->run(launch, agent_status_timeout_milliseconds);
				auto       catalog = agent_harness_catalog{ };
				if (codex) {
					catalog.Models = decode_agent_model_catalog(ran);
					// The efforts any of its models take, in the order listed.
					for (const auto& model : catalog.Models) {
						for (const auto& effort : model.Efforts) {
							if (std::ranges::find(catalog.Efforts, effort) == catalog.Efforts.end() && catalog.Efforts.size() < maximum_catalog_efforts) {
								catalog.Efforts.push_back(effort);
							}
						}
					}
				} else {
					catalog.Efforts = decode_agent_help_efforts(ran);
				}
				return catalog;
			},
			[self = Implementation, aEntry](agent_harness_catalog aCatalog) {
				std::erase(self->Projection.Cataloguing, aEntry);
				self->Projection.Catalogs[aEntry] = std::move(aCatalog);
				self->publish();
			});
	}

	void agent_setup_host::reset()
	{
		auto& host = *Implementation;
		if (host.settings_unreadable()) {
			return;
		}
		if (host.Projection.Checking) {
			stop();
		}
		// The list stays moved: the old settings are not read into it again.
		auto fresh                  = agent_setup_preferences{ };
		fresh.AgentsMoved           = true;
		host.Projection.Preferences = std::move(fresh);
		++host.DetectGeneration;
		host.Found.clear();
		host.Located.clear();
		host.Projection.Detecting = false;
		host.Projection.Detected  = false;
		host.Projection.WorkspaceDetail.clear();
		host.Projection.Catalogs.clear();
		host.Picked.reset();
		host.reset_checks();
		host.apply_programs();
		host.save();
		host.publish();
	}

	void agent_setup_host::prefer_agent(const std::uint32_t aId)
	{
		auto& host = *Implementation;
		if (host.settings_unreadable()) {
			return;
		}
		if (host.Projection.Preferences.PreferredAgent == aId || std::ranges::find(host.Projection.Preferences.Agents, aId, &core::agent_entry::Id) == host.Projection.Preferences.Agents.end()) {
			return;
		}
		host.Projection.Preferences.PreferredAgent = aId;
		host.save();
		host.publish();
	}

	void agent_setup_host::check()
	{
		auto& host = *Implementation;
		if (host.Projection.Checking) {
			return;
		}
		if (host.settings_unreadable()) {
			host.refuse_check();
			return;
		}
		// [[BPR-1859]]. Settings may ask before the Agent tab looked for the
		// agents: look first, then check what was found.
		if (!host.Projection.Detected) {
			++host.CheckGeneration;
			host.PendingCheck                = true;
			host.Projection.Checking         = true;
			host.Projection.State.AgentCheck = { .State = core::setup_check_state::running };
			host.Projection.CheckDetail.clear();
			if (host.Projection.Detecting) {
				host.publish();
			} else {
				host.detect();
			}
			return;
		}
		host.start_check();
	}

	void agent_setup_host::check_all()
	{
		auto& host = *Implementation;
		if (host.Projection.Checking || host.Projection.Preferences.Agents.empty()) {
			return;
		}
		auto ids = std::vector<std::uint32_t>{ };
		for (const auto& entry : host.Projection.Preferences.Agents) {
			ids.push_back(entry.Id);
		}
		host.CheckQueue.assign(ids.begin() + 1, ids.end());
		host.CheckingOverride         = true;
		host.Projection.CheckingEntry = ids.front();
		if (!host.Projection.Detected) {
			// Looked for first, as Check agent does, then checked in turn.
			++host.CheckGeneration;
			host.PendingCheck                = true;
			host.Projection.Checking         = true;
			host.Projection.State.AgentCheck = { .State = core::setup_check_state::running };
			host.Projection.CheckDetail.clear();
			if (host.Projection.Detecting) {
				host.publish();
			} else {
				host.detect();
			}
			return;
		}
		host.start_check();
	}

	void agent_setup_host::check_entry(const std::uint32_t aId)
	{
		auto& host = *Implementation;
		if (host.Projection.Checking || std::ranges::find(host.Projection.Preferences.Agents, aId, &core::agent_entry::Id) == host.Projection.Preferences.Agents.end()) {
			return;
		}
		host.CheckQueue.clear();
		host.CheckingOverride         = true;
		host.Projection.CheckingEntry = aId;
		if (!host.Projection.Detected) {
			++host.CheckGeneration;
			host.PendingCheck                = true;
			host.Projection.Checking         = true;
			host.Projection.State.AgentCheck = { .State = core::setup_check_state::running };
			host.Projection.CheckDetail.clear();
			if (host.Projection.Detecting) {
				host.publish();
			} else {
				host.detect();
			}
			return;
		}
		host.start_check();
	}

	void agent_setup_host::implementation::start_check()
	{
		auto& host = *this;
		if (host.settings_unreadable()) {
			host.refuse_check();
			return;
		}
		// [[BPR-2348]]. An entry with its own Program is checked with that
		// Program and its arguments, as `agent_entry_process` runs its turns:
		// not the program detection found for its kind, which may be another
		// install or a shim on the app's PATH, and which may be none.
		const auto* const own = host.own_program_entry();
		const auto* const row = own != nullptr ? nullptr : host.chosen_row();
		if (own == nullptr && row == nullptr && host.CheckingOverride) {
			// [[BPR-2051]]. An entry whose agent was not found here, or is not
			// allowed: its check says so, and Check all goes on.
			const auto* const entry      = host.preferred_entry();
			const auto        agent      = entry == nullptr ? core::setup_agent::claude_code : entry->Preset ? *entry->Preset
																											 : core::setup_agent_of(entry->Protocol);
			const auto        generation = ++host.CheckGeneration;
			host.begin_check(core::operation_action::agent_status_check);
			host.settle_check(generation, agent, core::fail_check(core::agent_setup_failure::agent_not_found), { }, { }, agent_check_step::unknown);
			return;
		}
		if (own == nullptr && row == nullptr) {
			// [[BPR-1859]]. The saved agent is not found any more: the check
			// says so, with what to do. Any other missing choice is the
			// offer's to explain; nothing runs.
			const auto saved = host.Picked ? host.Picked : host.Projection.Preferences.Chosen;
			if (saved && std::ranges::find(host.Projection.State.Rows, *saved, &core::agent_setup_row::Agent) == host.Projection.State.Rows.end()) {
				const auto generation = ++host.CheckGeneration;
				host.begin_check(core::operation_action::agent_status_check);
				host.settle_check(generation, *saved, core::fail_check(core::agent_setup_failure::agent_not_found), { }, { }, agent_check_step::unknown);
				return;
			}
			host.publish();
			return;
		}
		const auto generation            = ++host.CheckGeneration;
		const auto agent                 = own != nullptr ? (own->Preset ? *own->Preset : core::setup_agent_of(own->Protocol)) : row->Agent;
		const auto path                  = own != nullptr ? own->Program : row->Path;
		host.CheckStop                   = std::stop_source{ };
		host.Projection.Checking         = true;
		host.Projection.State.AgentCheck = { .State = core::setup_check_state::running };
		if (!host.CheckingOverride) {
			const auto* const checked     = host.preferred_entry();
			host.Projection.CheckingEntry = checked != nullptr ? std::optional{ checked->Id } : std::nullopt;
		}
		host.Projection.CheckDetail.clear();
		host.Projection.InstructionsSwitch.reset();
		host.publish();
		// Owner decision 2026-10-23. A custom agent's check is that its
		// program can be launched and, when it names one, that its own check
		// exits 0; a popular agent keeps its sign-in status check.
		if (const auto* const entry = host.preferred_entry(); entry != nullptr && !entry->Preset && (entry == own || core::setup_agent_of(entry->Protocol) == agent)) {
			const auto profile  = std::ranges::find(host.Dependencies.Profiles, entry->Protocol, &core::agent_profile::Protocol);
			auto       ownCheck = std::optional<agent_launch_request>{ };
			if (!entry->CheckArguments.empty() && profile != host.Dependencies.Profiles.end()) {
				ownCheck              = agent_launch_request{ };
				ownCheck->Protocol    = core::agent_protocol::external_terminal;
				ownCheck->Session     = core::agent_session_id{ 1U };
				ownCheck->Executable  = path;
				ownCheck->Arguments   = entry->CheckArguments;
				ownCheck->WritingOnly = true;
				ownCheck->Path        = core::agent_run_path::agent_check;
				ownCheck->Environment = materialize_agent_environment(*profile, host.Dependencies.Parent);
			}
			// [[BPR-2305]]. A custom Codex JSONL agent is also asked one short
			// question with a rule, given the way its Instructions setting
			// says, to show the rule reaches the model through its program.
			// [[BPR-2320]]. A custom Claude agent is asked one short turn, to
			// show its program answers in stream JSON.
			auto       instructionsProbe = std::optional<codex_probe>{ };
			const auto probed            = entry->Protocol == core::agent_protocol::codex_jsonl_exec || entry->Protocol == core::agent_protocol::claude_stream_json;
			if (probed && profile != host.Dependencies.Profiles.end()) {
				instructionsProbe = codex_probe{ .Profile  = *profile,
												 .Program  = path,
												 .Leading  = entry->Arguments,
												 .Delivery = entry->Instructions,
												 .Parent   = host.Dependencies.Parent,
												 .Checks   = entry->StrictRunChecks ? agent_run_checks::strict : agent_run_checks::relaxed };
			}
			struct custom
			{
				core::setup_check Check;
				std::string       Detail;
				check_run         Run;
				agent_check_step  Step = agent_check_step::program;
			};
			host.begin_check(core::operation_action::agent_custom_check);
			host.off_lane(
				host.Dependencies.Checks,
				[inspect = host.Dependencies.Inspect, process = host.Dependencies.Process, path, probeRan = ran_words(path, entry->Arguments, true), check = std::move(ownCheck), probe = std::move(instructionsProbe), stop = host.CheckStop.get_token()]() mutable {
					const auto refusal = inspect ? inspect(path) : std::optional{ core::agent_wrapper_refusal::missing };
					if (refusal) {
						// [[BPR-2348]]. Refused by name: the path, then why.
						return custom{ .Check = core::fail_check(core::agent_setup_failure::program_refused), .Detail = path + ": " + std::string{ core::describe(*refusal) } };
					}
					if (process == nullptr || (!check && !probe)) {
						return custom{ .Check = core::setup_check{ .State = core::setup_check_state::passed } };
					}
					auto said = std::string{ };
					auto runs = std::uint32_t{ 0U };
					auto last = check_run{ };
					if (check) {
						const auto ran = process->run(*check, agent_status_timeout_milliseconds, { }, stop);
						runs           = 1U;
						last           = run_of(ran, runs);
						if (ran.Outcome != agent_run_outcome::success) {
							return custom{ .Check = core::fail_check(unanswered(ran.Outcome).value_or(core::agent_setup_failure::status_unreadable)), .Detail = ran_with(ran_words(check->Executable, check->Arguments, false), detail_of(ran)), .Run = last, .Step = agent_check_step::check_command };
						}
						said = first_line_of(ran);
					}
					if (!probe || stop.stop_requested()) {
						return custom{ .Check = core::setup_check{ .State = core::setup_check_state::passed }, .Run = last, .Step = check ? agent_check_step::check_command : agent_check_step::program };
					}
					const auto claude = probe->Profile.Protocol == core::agent_protocol::claude_stream_json;
					auto       answer = claude ? probe_claude_stream(*process, *probe, said, stop) : probe_codex_instructions(*process, *probe, said, stop);
					answer.Run.Runs += runs;
					return custom{ .Check = std::move(answer.Check), .Detail = ran_with(probeRan, std::move(answer.Detail)), .Run = answer.Run, .Step = claude ? agent_check_step::protocol_probe : agent_check_step::instructions_probe };
				},
				[self = shared_from_this(), generation, agent, id = entry->Id](custom aAnswer) {
					if (generation == self->CheckGeneration) {
						self->Projection.InstructionsSwitch = aAnswer.Check.Failure == core::agent_setup_failure::instructions_dropped ? std::optional{ id } : std::nullopt;
					}
					self->settle_check(generation, agent, std::move(aAnswer.Check), std::move(aAnswer.Detail), aAnswer.Run, aAnswer.Step);
				});
			return;
		}
		if (agent == core::setup_agent::plain_text_harness) {
			// No status command: the check is that its program can run now.
			host.begin_check(core::operation_action::agent_program_check);
			host.off_lane(
				host.Dependencies.Effects, [inspect = host.Dependencies.Inspect, path] { return inspect ? inspect(path) : std::optional{ core::agent_wrapper_refusal::missing }; },
				[self = shared_from_this(), generation, agent](std::optional<core::agent_wrapper_refusal> aRefusal) {
					self->settle_check(generation, agent, aRefusal ? core::fail_check(core::agent_setup_failure::program_refused) : core::setup_check{ .State = core::setup_check_state::passed },
									   aRefusal ? std::string{ core::describe(*aRefusal) } : std::string{ }, { }, agent_check_step::program);
				});
			return;
		}
		const auto profile = std::ranges::find(host.Dependencies.Profiles, core::setup_profile(agent, host.Projection.Preferences.CodexProtocol), &core::agent_profile::Id);
		auto       built   = profile == host.Dependencies.Profiles.end() ? std::nullopt : build_agent_status_launches(agent, *profile, path, host.Dependencies.Parent);
		// [[BPR-1753]]. The chosen entry's fixed arguments come first,
		// as they do at every launch.
		if (const auto* const entry = host.preferred_entry(); built && entry != nullptr && core::setup_agent_of(entry->Protocol) == agent && !entry->Arguments.empty()) {
			built->Status.Arguments.insert(built->Status.Arguments.begin(), entry->Arguments.begin(), entry->Arguments.end());
			built->Version.Arguments.insert(built->Version.Arguments.begin(), entry->Arguments.begin(), entry->Arguments.end());
		}
		host.begin_check(core::operation_action::agent_status_check);
		if (!built || host.Dependencies.Process == nullptr) {
			host.settle_check(generation, agent, core::fail_check(core::agent_setup_failure::failed_to_start), std::string{ "status_launch_unavailable" });
			return;
		}
		struct answer
		{
			core::setup_check Check;
			std::string       Detail;
			check_run         Run;
		};
		host.off_lane(
			host.Dependencies.Checks,
			[process = host.Dependencies.Process, launches = std::move(*built), agent, stop = host.CheckStop.get_token()]() mutable {
				const auto status = process->run(launches.Status, agent_status_timeout_milliseconds, { }, stop);
				if (const auto failure = unanswered(status.Outcome)) {
					return answer{ .Check = core::fail_check(*failure), .Detail = detail_of(status), .Run = run_of(status, 1U) };
				}
				// The version is asked only of a signed-in agent.
				auto decoded = decode_agent_status(agent, status, agent_run_result{ });
				auto run     = run_of(status, 1U);
				if (decoded.Status == core::agent_status::signed_in && !stop.stop_requested()) {
					const auto version = process->run(launches.Version, agent_status_timeout_milliseconds, { }, stop);
					decoded            = decode_agent_status(agent, status, version);
					run                = run_of(version, 2U);
				}
				return answer{ .Check = core::apply_status_check(decoded), .Detail = detail_of(status), .Run = run };
			},
			[self = shared_from_this(), generation, agent](answer aAnswer) { self->settle_check(generation, agent, std::move(aAnswer.Check), std::move(aAnswer.Detail), aAnswer.Run); });
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
		if (aAgent == core::setup_agent::claude_code || host.settings_unreadable()) {
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
		if (host.settings_unreadable()) {
			return;
		}
		if (host.Projection.Preferences.CodexProtocol == aChoice) {
			return;
		}
		host.Projection.Preferences.CodexProtocol = aChoice;
		host.save();
		host.publish();
	}

	auto agent_setup_host::set_run_settings(const core::setup_agent aAgent, std::string aModel, std::string aEffort) -> bool
	{
		auto& host = *Implementation;
		if (host.settings_unreadable()) {
			return false;
		}
		auto next = host.Projection.Preferences;
		switch (aAgent) {
		case core::setup_agent::claude_code:
			next.ClaudeEffort = std::move(aEffort);
			break;
		case core::setup_agent::codex:
			next.CodexModel  = std::move(aModel);
			next.CodexEffort = std::move(aEffort);
			break;
		case core::setup_agent::plain_text_harness:
			return false;
		}
		if (!valid(next)) {
			return false;
		}
		if (next == host.Projection.Preferences) {
			return true;
		}
		host.Projection.Preferences = std::move(next);
		host.save();
		host.publish();
		return true;
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

	void agent_setup_host::set_workspace_check(core::setup_check aCheck, std::string aDetail)
	{
		auto& host = *Implementation;
		aDetail    = aCheck.State == core::setup_check_state::failed ? bounded(std::move(aDetail)) : std::string{ };
		if (host.Projection.State.WorkspaceCheck == aCheck && host.Projection.WorkspaceDetail == aDetail) {
			return;
		}
		host.Projection.State.WorkspaceCheck = std::move(aCheck);
		host.Projection.WorkspaceDetail      = std::move(aDetail);
		host.publish();
	}

	void agent_setup_host::settle_workspace_check(const std::string& aKey, core::setup_check aCheck, std::string aDetail)
	{
		auto&      host   = *Implementation;
		auto&      kept   = host.Projection.Preferences.WorkspaceChecks;
		const auto before = kept;
		if (!aKey.empty() && aKey.size() <= maximum_workspace_check_key_bytes && (aCheck.State == core::setup_check_state::passed || aCheck.State == core::setup_check_state::failed)) {
			std::erase(kept, aKey);
			if (aCheck.State == core::setup_check_state::passed) {
				kept.push_back(aKey);
				if (kept.size() > maximum_kept_workspace_checks) {
					kept.erase(kept.begin(), kept.end() - static_cast<std::ptrdiff_t>(maximum_kept_workspace_checks));
				}
			}
		}
		if (kept != before) {
			host.save();
		}
		set_workspace_check(std::move(aCheck), std::move(aDetail));
	}

	void agent_setup_host::restore_workspace_check(const std::string& aKey)
	{
		auto& host = *Implementation;
		if (host.Projection.State.WorkspaceCheck.State != core::setup_check_state::not_run || std::ranges::find(host.Projection.Preferences.WorkspaceChecks, aKey) == host.Projection.Preferences.WorkspaceChecks.end()) {
			return;
		}
		set_workspace_check({ .State = core::setup_check_state::passed });
	}

	void agent_setup_host::restart()
	{
		auto& host = *Implementation;
		host.reset_checks();
		host.Projection.State.WorkspaceCheck = { };
		host.Projection.WorkspaceDetail.clear();
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
