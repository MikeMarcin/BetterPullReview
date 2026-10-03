#include "bpr/application/private_review_application_host.hpp"
#include <array>
#include <utility>
namespace bpr::application {
	namespace {
		using namespace core;
		struct refused
		{
			private_run_error Error;
		};
		template <typename T>
		auto take(private_store_result<T> aValue) -> T
		{
			if (!aValue) {
				throw refused{ { .Store = aValue.error() } };
			}
			return std::move(*aValue);
		}
		template <typename T>
		auto decide(std::expected<T, private_review_refusal> aValue) -> T
		{
			if (!aValue) {
				throw refused{ { .Policy = aValue.error() } };
			}
			return std::move(*aValue);
		}
		void decide(std::expected<void, private_review_refusal> aValue)
		{
			if (!aValue) {
				throw refused{ { .Policy = aValue.error() } };
			}
		}
		void require(bool aValue, private_review_refusal aReason = private_review_refusal::invalid_provenance)
		{
			if (!aValue) {
				throw refused{ { .Policy = aReason } };
			}
		}
		template <typename F>
		auto attempt(F aWork) -> private_run_result<decltype(aWork())>
		{
			try {
				return aWork();
			} catch (const refused& failure) {
				return std::unexpected(failure.Error);
			} catch (...) {
				return std::unexpected(private_run_error{ .Store = private_store_failure::unavailable });
			}
		}
		// At namespace scope: MSVC refuses `-> bool override` in a class local to a lambda.
		struct input_claim final : agent_input_claim
		{
			std::unique_ptr<private_source_use> Source;
			std::stop_token                     Stop;
			bool                                Released = false;
			auto                                permitted() const noexcept -> bool override { return !Released && !Stop.stop_requested(); }
			void                                release() noexcept override
			{
				Source.reset();
				Released = true;
			}
		};
		auto full_body(private_review_store_port& aStore, const private_source_reference& aSource) -> agent_private_source_body
		{
			agent_private_source_body result{ .Source = aSource };
			if (std::holds_alternative<private_finding_source>(aSource.Key)) {
				result.Finding = take(aStore.load_finding(aSource));
				result.Body    = serialize_private_finding(*result.Finding);
				return result;
			}
			std::optional<private_body_cursor> cursor{ private_body_cursor{ aSource, 0 } };
			while (cursor && aSource.BodyBytes > 0) {
				auto page = take(aStore.read_revision_body(*cursor));
				require(!page.Bytes.empty() && page.Bytes.size() <= maximum_private_page_bytes && page.Bytes.size() <= aSource.BodyBytes - result.Body.size(), private_review_refusal::bounds);
				result.Body += page.Bytes;
				if (page.Next) {
					require(page.Next->Source == aSource && page.Next->Offset == result.Body.size());
				}
				cursor = page.Next;
			}
			require(result.Body.size() == aSource.BodyBytes && private_digest(result.Body) == aSource.Digest, private_review_refusal::digest_mismatch);
			return result;
		}
		auto terminal_outcome(agent_run_outcome aOutcome) -> private_run_outcome
		{
			switch (aOutcome) {
			case agent_run_outcome::success:          return private_run_outcome::succeeded;
			case agent_run_outcome::cancelled:        return private_run_outcome::cancelled;
			case agent_run_outcome::timeout:          return private_run_outcome::timed_out;
			case agent_run_outcome::protocol_failure: return private_run_outcome::protocol_failure;
			case agent_run_outcome::stale_result:     return private_run_outcome::interrupted;
			default:                                  return private_run_outcome::failed;
			}
		}
	}
	auto private_review_application_host::admit(const private_run_intent& aIntent, const core::agent_profile& aProfile, const core::agent_session& aSession, const agent_context_observation& aContext, const core::agent_setting_facts& aSettings) -> private_run_result<private_run_admission>
	{
		return attempt([&] {
			auto frontier = take(Store.frontier());
			require(aIntent.Binding.Scope == frontier.Scope && aIntent.Binding.Revision == aSession.Binding && aContext.Binding == aSession.Binding && aProfile.Id == aSession.Profile, core::private_review_refusal::invalid_binding);
			require(aContext.IncludedNotes.empty() && !aContext.IncludedDraft, core::private_review_refusal::invalid_provenance);
			require(aIntent.Sources.size() <= core::maximum_private_sources, core::private_review_refusal::bounds);
			auto refs = std::vector<core::private_source_reference>{ };
			for (const auto& selected : aIntent.Sources) {
				refs.push_back(selected.Source);
			}
			auto                  observed = take(Store.load_sources(refs));
			auto                  digest   = decide(core::admit_private_run_sources(aIntent.Binding, aIntent.Sources, observed));
			private_run_admission result;
			if (aIntent.Purpose == core::private_run_purpose::composition) {
				require(aIntent.Composition.has_value() && aContext.EntryPoint == core::agent_entry_point::workshop_comment && aContext.ChatMessage.empty() && !aIntent.Thread);
				auto composition = take(Store.load_composition(*aIntent.Composition));
				require(composition.Version == aIntent.ExpectedCompositionVersion && composition.State.Binding == aIntent.Binding && composition.State.Mode == core::review_composition_mode::agent_assisted && composition.State.Lifecycle == core::review_composition_lifecycle::editing && !composition.State.Candidate && composition.Sources == aIntent.Sources && composition.State.SourceSetDigest == digest, core::private_review_refusal::conflict);
				result.Composition = std::move(composition);
			} else {
				require(!aIntent.Composition && aIntent.ExpectedCompositionVersion == 0);
			}
			// [[BPR-1245]]. A workshop press outside a composition asks about the one
			// reviewer revision that recorded its draft, for findings, and nothing else.
			if (aContext.EntryPoint == core::agent_entry_point::workshop_comment && aIntent.Purpose != core::private_run_purpose::composition) {
				require(aIntent.Purpose == core::private_run_purpose::findings && aContext.ChatMessage.empty() && aContext.WorkshopOperation.has_value());
				decide(core::admit_workshop_run_sources(aIntent.Sources, observed));
			}

			std::size_t bytes = 0;
			for (const auto& source : refs) {
				auto body = full_body(Store, source);
				require(body.Body.size() <= core::maximum_selected_private_source_bytes - bytes, core::private_review_refusal::bounds);
				bytes += body.Body.size();
				result.Launch.PrivateSources.push_back(std::move(body));
			}
			auto next = frontier.NextIdentity;
			require(next > 0 && next <= core::maximum_private_identity - 4, core::private_review_refusal::bounds);
			private_store_write write{ .ExpectedNextIdentity = next };
			if (!aContext.ChatMessage.empty()) {
				require(aIntent.Purpose == core::private_run_purpose::conversation);
				auto                          input = core::private_revision_input{ .Actor = core::private_reviewer_actor{ frontier.Scope.Account }, .Body = aContext.ChatMessage };
				core::private_revision_commit proposal;
				std::uint64_t                 expected = 0;
				if (aIntent.Thread) {
					auto        thread  = take(Store.load_thread(*aIntent.Thread));
					const auto* subject = std::get_if<core::exact_private_subject>(&thread.Binding);
					require(subject && subject->Binding == aIntent.Binding, core::private_review_refusal::invalid_binding);
					expected       = aIntent.ExpectedThreadVersion;
					input.Turn     = core::private_review_turn_id{ next++ };
					input.Revision = core::private_review_turn_revision_id{ next++ };
					input.Order    = core::private_vertex_order{ input.Revision.value() };
					proposal       = decide(core::append_private_turn(thread, expected, input));
				} else {
					require(aIntent.ExpectedThreadVersion == 0, core::private_review_refusal::conflict);
					auto thread    = core::private_review_thread_id{ next++ };
					input.Turn     = core::private_review_turn_id{ next++ };
					input.Revision = core::private_review_turn_revision_id{ next++ };
					input.Order    = core::private_vertex_order{ input.Revision.value() };
					proposal       = decide(core::create_private_thread(thread, { aIntent.Binding, core::private_pr_subject{ } }, input));
				}
				write.Threads         = { { proposal.Thread, expected } };
				write.Revisions       = { proposal.Revision };
				result.Prompt         = proposal.Revision;
				result.Delta.Thread   = proposal.Thread.Id;
				result.Delta.Revision = proposal.Revision.Identity.Revision;
			} else {
				require(!aIntent.Thread);
			}
			auto run = core::agent_run_provenance{ .Run = core::agent_run_id{ next }, .Session = aSession.Id, .Order = core::private_vertex_order{ next }, .Profile = aProfile.Id, .ProfileDisplayName = aProfile.DisplayName, .Protocol = aProfile.Protocol, .EntryPoint = aContext.EntryPoint, .Purpose = aIntent.Purpose, .Binding = aIntent.Binding, .Metadata = { .Requested = aSettings, .Configured = aProfile.DefaultSettings }, .SelectedPrivateSources = aIntent.Sources, .InputSelectionDigest = digest };
			require(core::valid_run_provenance(run));
			write.Runs         = { run };
			write.SourceGuards = aIntent.Sources;
			take(Store.commit(write));
			result.Launch.Requested   = aSettings;
			result.Launch.EntryPoint  = aContext.EntryPoint;
			result.Launch.AdmittedRun = run;
			result.Delta.Scope        = frontier.Scope;
			result.Delta.Run          = run.Run;
			return result;
		});
	}
	auto private_review_application_host::claim(const private_run_admission& aAdmission, std::stop_token aStop) -> private_run_result<std::shared_ptr<agent_input_claim>>
	{
		return attempt([&]() -> std::shared_ptr<agent_input_claim> {
			require(aAdmission.Launch.AdmittedRun.has_value());
			auto claim    = std::make_shared<input_claim>();
			claim->Stop   = aStop;
			claim->Source = take(Store.claim_sources(aAdmission.Launch.AdmittedRun->SelectedPrivateSources));
			return claim;
		});
	}
	auto private_review_application_host::finish(const private_run_admission& aAdmission, const agent_run_result& aResult, const core::agent_finding_snapshot& aSnapshot) -> private_run_result<private_review_delta>
	{
		return attempt([&] {
			require(aAdmission.Launch.AdmittedRun.has_value());
			auto run      = *aAdmission.Launch.AdmittedRun;
			auto retained = take(Store.load_run_details(std::array{ run.Run }));
			require(retained.size() == 1 && retained.front() == run, core::private_review_refusal::conflict);
			const auto correlated = aResult.Run == run.Run && aResult.Session == run.Session && aSnapshot.Binding == run.Binding.Revision;
			run.Outcome           = correlated ? terminal_outcome(aResult.Outcome) : core::private_run_outcome::protocol_failure;
			run.Completion        = run.Outcome == core::private_run_outcome::succeeded ? core::private_revision_completion::complete : core::private_revision_completion::incomplete;
			if (correlated && aResult.Metadata.Requested == run.Metadata.Requested && aResult.Metadata.Configured == run.Metadata.Configured && core::inspect_runtime_metadata(aResult.Metadata)) {
				run.Metadata = aResult.Metadata;
			} else if (aResult.Outcome == agent_run_outcome::success) {
				run.Outcome    = core::private_run_outcome::protocol_failure;
				run.Completion = core::private_revision_completion::incomplete;
			}
			auto invalid_output = [&] {
				run.Outcome    = core::private_run_outcome::protocol_failure;
				run.Completion = core::private_revision_completion::incomplete;
			};
			std::string body;
			if (aResult.AssistantItems.size() > 256) {
				invalid_output();
			} else if (correlated) {
				for (const auto& item : aResult.AssistantItems) {
					if (item.size() > core::maximum_private_assistant_body_bytes - body.size()) {
						invalid_output();
						body.clear();
						run.OutputItemBytes.clear();
						break;
					}
					body += item;
					run.OutputItemBytes.push_back(item.size());
				}
			}
			if (!body.empty() || run.Outcome == core::private_run_outcome::succeeded) {
				run.OutputDigest = core::private_digest(body);
			} else {
				run.OutputItemBytes.clear();
			}
			auto                frontier = take(Store.frontier());
			private_store_write write{ .ExpectedNextIdentity = frontier.NextIdentity };
			auto                next  = frontier.NextIdentity;
			auto                delta = private_review_delta{ .Scope = run.Binding.Scope, .Run = run.Run };
			if (run.Outcome == core::private_run_outcome::succeeded && aResult.Findings) {
				if (aResult.Findings->Session != run.Session || aResult.Findings->Findings.size() > maximum_agent_findings_per_turn) {
					invalid_output();
				} else {
					for (std::size_t i = 0; i < aResult.Findings->Findings.size(); ++i) {
						auto finding = core::retain_agent_finding(run, static_cast<std::uint32_t>(i), core::private_vertex_order{ next++ }, aResult.Findings->Findings[i], aSnapshot);
						if (!finding) {
							invalid_output();
							break;
						}
						delta.Findings.push_back(finding->Source);
						write.Findings.push_back(std::move(*finding));
					}
				}
				if (run.Outcome != core::private_run_outcome::succeeded) {
					write.Findings.clear();
					delta.Findings.clear();
					next = frontier.NextIdentity;
				}
			}
			delta.Outcome = run.Outcome;
			if (aAdmission.Prompt && correlated && (!body.empty() || run.Outcome == core::private_run_outcome::succeeded)) {
				auto thread        = take(Store.load_thread(aAdmission.Prompt->Identity.Thread));
				auto authorization = decide(core::authorize_private_run(run));
				auto revision      = decide(core::append_private_turn(thread, thread.Version, { core::private_review_turn_id{ next }, core::private_review_turn_revision_id{ next + 1 }, core::private_vertex_order{ next + 1 }, core::private_agent_actor{ run.Run }, body, run.Completion }, &*aAdmission.Prompt, &authorization));
				write.Threads      = { { revision.Thread, thread.Version } };
				write.Revisions    = { revision.Revision };
				delta.Thread       = thread.Id;
				delta.Revision     = revision.Revision.Identity.Revision;
			}
			write.Runs = { run };
			if (aAdmission.Composition && run.Outcome == core::private_run_outcome::succeeded) {
				// A changed/deleted workspace or source still receives its terminal run,
				// but never gains a candidate through an obsolete invocation.
				auto current = Store.load_composition(aAdmission.Composition->State.Id);
				if (current && current->Version == aAdmission.Composition->Version && current->State.Lifecycle == core::review_composition_lifecycle::editing && !current->State.Candidate) {
					std::vector<core::private_source_reference> refs;
					for (const auto& source : current->Sources) {
						refs.push_back(source.Source);
					}
					auto observed = Store.load_sources(refs);
					if (observed) {
						auto proposal = core::propose_composition_revision(current->State, core::review_composition_revision_id{ next }, core::private_vertex_order{ next }, body, current->Sources, *observed, &run);
						if (proposal) {
							current->State = proposal->State;
							write.Compositions.push_back({ *current, current->Version });
							++write.Compositions.back().Record.Version;
							write.Candidates.push_back(proposal->Revision);
							delta.Candidate = proposal->Revision.Identity;
						}
					}
				}
			}
			take(Store.commit(write));
			return delta;
		});
	}
	auto private_review_application_host::reply_to_finding(const core::private_source_selection& aSelected, std::string aReviewerBody) -> private_run_result<private_review_delta>
	{
		return attempt([&] {
			require(std::holds_alternative<core::private_finding_source>(aSelected.Source.Key), core::private_review_refusal::invalid_subject);
			auto frontier = take(Store.frontier());
			auto observed = take(Store.load_sources(std::array{ aSelected.Source }));
			decide(core::admit_composition_sources(frontier.Scope, std::array{ aSelected }, observed));
			auto finding = take(Store.load_finding(aSelected.Source));
			auto key     = std::get<core::private_finding_source>(finding.Source.Key);
			auto runs    = take(Store.load_run_details(std::array{ key.Run }));
			require(runs.size() == 1);
			auto next = frontier.NextIdentity;
			require(next > 0 && next <= core::maximum_private_identity - 2, core::private_review_refusal::bounds);
			auto revision = decide(core::create_private_thread(core::private_review_thread_id{ next }, { runs.front().Binding, core::private_finding_reference{ key.Run, key.Ordinal, aSelected.Source.Digest } }, { core::private_review_turn_id{ next + 1 }, core::private_review_turn_revision_id{ next + 2 }, core::private_vertex_order{ next + 2 }, core::private_reviewer_actor{ frontier.Scope.Account }, std::move(aReviewerBody) }));
			take(Store.commit({ .ExpectedNextIdentity = next, .Threads = { { revision.Thread, 0 } }, .Revisions = { revision.Revision }, .SourceGuards = { aSelected } }));
			return private_review_delta{ .Scope = frontier.Scope, .Thread = revision.Thread.Id, .Revision = revision.Revision.Identity.Revision, .Run = key.Run, .Outcome = runs.front().Outcome };
		});
	}
	auto private_review_application_host::record_workshop_press(const core::private_review_binding& aBinding, core::review_draft_id aDraft, const core::diff_semantic_index& aIndex) -> private_run_result<private_workshop_press>
	{
		return attempt([&] {
			auto frontier = take(Store.frontier());
			require(aBinding.Scope == frontier.Scope, core::private_review_refusal::invalid_binding);
			const auto  record = take(Store.load_draft(aDraft));
			const auto& draft  = record.Draft.Draft;
			require(record.Binding == aBinding, core::private_review_refusal::invalid_binding);
			require(draft.State == core::review_draft_state::drafting && !draft.Body.empty(), core::private_review_refusal::invalid_target);
			const auto next = frontier.NextIdentity;
			require(next > 0 && next <= core::maximum_private_identity - 2, core::private_review_refusal::bounds);
			auto revision = decide(core::create_private_thread(core::private_review_thread_id{ next }, { aBinding, draft.Anchor }, { core::private_review_turn_id{ next + 1 }, core::private_review_turn_revision_id{ next + 2 }, core::private_vertex_order{ next + 2 }, core::private_reviewer_actor{ frontier.Scope.Account }, draft.Body }, &aIndex));
			take(Store.commit({ .ExpectedNextIdentity = next, .Threads = { { revision.Thread, 0 } }, .Revisions = { revision.Revision } }));
			const auto source = core::source_of(revision.Revision);
			return private_workshop_press{
				.Run    = { .Binding = aBinding, .Purpose = core::private_run_purpose::findings, .Sources = { { source, source.Key, false } } },
				.Thread = revision.Thread.Id,
			};
		});
	}
	auto private_review_application_host::recover(std::optional<core::agent_run_id> aAfter) -> private_run_result<private_run_page>
	{
		return attempt([&] {
			auto                page = take(Store.list_runs(aAfter));
			private_store_write write{ .ExpectedNextIdentity = take(Store.frontier()).NextIdentity };
			for (auto& run : page.Runs) {
				if (run.Outcome == core::private_run_outcome::admitted) {
					run.Outcome    = core::private_run_outcome::interrupted;
					run.Completion = core::private_revision_completion::incomplete;
					write.Runs.push_back(run);
				}
			}
			if (!write.Runs.empty()) {
				take(Store.commit(write));
			}
			return page;
		});
	}
}
