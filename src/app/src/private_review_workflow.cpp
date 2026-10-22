#include "private_review_workflow.hpp"
#include "private_review_operations.hpp"

#include "bpr/core/pull_request_suggestion.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <map>
namespace bpr::app {
	namespace {
		using namespace application;
		using namespace application::private_review_operations;
		template <typename TValue>
		auto accepted(private_run_result<TValue> aValue) -> TValue
		{
			if (!aValue) {
				throw refused{ aValue.error() };
			}
			return std::move(*aValue);
		}
		auto message(const private_run_error& aError) -> std::string
		{
			if (aError.Policy) {
				return "Private operation refused: " + std::string(core::stable_name(*aError.Policy)) + ". Refresh sources or choose the exact current target.";
			}
			switch (aError.Store.value_or(private_store_failure::unavailable)) {
			case private_store_failure::references_present: return "This history is still used by a run, composition, draft, or publication. Archive the thread to keep its sources available.";
			case private_store_failure::quota:              return "Private storage is full. No content was truncated or removed.";
			case private_store_failure::stale_cursor:
			case private_store_failure::conflict:           return "Private history changed. Refresh and select the exact sources again.";
			case private_store_failure::busy:               return "Private storage is busy. Retry when the current operation finishes.";
			default:                                        return "Private storage refused the operation. Prior committed content is retained; refresh before retrying.";
			}
		}
		auto revision(private_review_store_port& aStore, const core::private_source_selection& aSelection) -> core::private_review_revision
		{
			auto observed = take(aStore.load_sources(std::array{ aSelection.Source }));
			decide(core::admit_composition_sources(aSelection.Source.Scope, std::array{ aSelection }, observed));
			const auto* key = std::get_if<core::private_turn_source>(&aSelection.Source.Key);
			require(key != nullptr);
			return { { aSelection.Source.Scope, key->Thread, key->Turn, key->Revision, aSelection.Source.Order, aSelection.Source.Digest }, observed.front().Actor, body(aStore, aSelection.Source), observed.front().Completion, { }, { }, { } };
		}
	}
	struct private_review_workflow::context
	{
		adapters::sqlite::private_review_configuration          Configuration;
		std::unique_ptr<application::private_review_store_port> Store;
		core::private_review_scope                              Scope;
		std::optional<application::private_catalog_cursor>      Catalog;
		std::optional<application::private_catalog_cursor>      NextCatalog;
		std::optional<application::private_revision_cursor>     Revisions;
		std::optional<application::private_revision_cursor>     NextRevisions;
		std::optional<core::private_review_thread_id>           Selected;
		std::vector<application::private_body_cursor>           Bodies;
		std::optional<application::private_finding_cursor>      Findings;
		std::optional<application::private_finding_cursor>      NextFindings;
		std::vector<core::private_source_selection>             Sources;
		std::vector<core::review_composition_id>                Workspaces;
		std::optional<core::review_composition_id>              Composition;
		bool                                                    Composing       = false;
		bool                                                    IncludeArchived = false;
	};
	struct private_review_workflow::result
	{
		std::optional<application::private_conversation_projection> Conversation;
		std::optional<application::private_composition_projection>  Composition;
		std::optional<application::durable_draft_commit>            Drafts;
		std::optional<core::agent_run_provenance>                   Provenance;
		std::optional<std::string>                                  FindingDetail;
		std::optional<core::private_source_reference>               BodySource;
		std::optional<application::private_body_page>               Body;
		std::optional<application::private_run_intent>              Run;
		std::optional<application::private_run_error>               Error;
		core::agent_profile_id                                      Profile;
		core::agent_setting_facts                                   Requested;
		std::string                                                 Prompt;
		bool                                                        UnsubmittedPublication = false;
		bool                                                        OpenDraft              = false;
		bool                                                        Refresh                = true;
	};
	private_review_workflow::private_review_workflow(application::effect_executor& aEffects, application::decision_executor& aDecisions, std::mutex& aLedgerFile, private_review_recovery_registry& aRecovery, qt::private_review_panel& aPanel, QWidget* aParent, draft_sink aDrafts, run_sink aLaunch)
		: Effects(aEffects)
		, Decisions(aDecisions)
		, Recovery(aRecovery)
		, Panel(aPanel)
		, Editor(new qt::private_composition_editor(aParent))
		, Drafts(std::move(aDrafts))
		, Launch(std::move(aLaunch))
		, Gate(aLedgerFile)
	{
		Panel.set_dispatch([this](auto aIntent) { dispatch(std::move(aIntent)); });
		Editor->set_dispatch([this](auto aIntent) { compose(std::move(aIntent)); });
	}
	private_review_workflow::~private_review_workflow() { delete Editor.data(); }
	void private_review_workflow::open(adapters::sqlite::private_review_configuration aConfiguration, std::function<void()> aReady)
	{
		if (Closing) {
			return;
		}
		invalidate();
		const auto generation = Generation;
		++Pending;
		Effects.post([this, generation, config = std::move(aConfiguration), ready = std::move(aReady)]() mutable {
			std::shared_ptr<context> opened;
			{
				const auto lock  = std::scoped_lock{ Gate };
				auto       prior = std::ranges::find_if(Contexts, [&](const auto& aContext) { return aContext->Configuration.Profile == config.Profile && aContext->Configuration.Host == config.Host && aContext->Configuration.Account == config.Account && aContext->Configuration.Repository == config.Repository && aContext->Configuration.PullRequest == config.PullRequest; });
				if (prior != Contexts.end()) {
					opened = *prior;
				} else if (Contexts.size() < 32) {
					auto store = adapters::sqlite::make_private_review_store(config);
					if (store) {
						auto frontier = (*store)->frontier();
						if (frontier) {
							auto recovered = std::ranges::find(Recovery.Scopes, frontier->Scope) != Recovery.Scopes.end();
							if (!recovered) {
								application::private_review_application_host host(**store);
								std::optional<core::agent_run_id>            after;
								for (;;) {
									auto page = host.recover(after);
									if (!page) {
										break;
									}
									if (!page->Next) {
										recovered = true;
										break;
									}
									after = page->Next;
								}
								if (recovered) {
									Recovery.Scopes.push_back(frontier->Scope);
								}
							}
							if (recovered) {
								opened                = std::make_shared<context>();
								opened->Configuration = config;
								opened->Scope         = frontier->Scope;
								opened->Store         = std::move(*store);
								Contexts.push_back(opened);
							}
						}
					}
				}
			}
			Decisions.post([this, generation, adopted = std::move(opened), notify = std::move(ready)]() mutable {
				--Pending;
				if (Closing || generation != Generation) {
					return;
				}
				Current = std::move(adopted);
				if (!Current) {
					Panel.present({ .Generation = Generation, .Status = "Private storage unavailable. No private change or agent run can begin." });
					return;
				}
				if (notify) {
					notify();
				}
				refresh();
			});
		});
	}
	void private_review_workflow::observe(core::private_review_binding aBinding, std::shared_ptr<const core::diff_semantic_index> aIndex, std::optional<core::source_line_ref> aLine)
	{
		if (Closing || !Current) {
			return;
		}
		aBinding.Scope = Current->Scope;
		if (!core::valid_private_binding(aBinding)) {
			return;
		}
		const auto changed          = Binding != aBinding;
		const auto selectionChanged = SelectedLine != aLine;
		Binding                     = aBinding;
		Index                       = std::move(aIndex);
		SelectedLine                = aLine;
		if (std::ranges::find(KnownBindings, aBinding) == KnownBindings.end()) {
			if (KnownBindings.size() == 128) {
				KnownBindings.erase(KnownBindings.begin());
			}
			KnownBindings.push_back(aBinding);
		}
		if (changed) {
			++Generation;
		}
		if (changed || selectionChanged) {
			refresh();
		}
	}
	void private_review_workflow::profiles(std::span<const core::agent_profile> aProfiles)
	{
		Profiles.clear();
		for (const auto& profile : aProfiles) {
			Profiles.emplace_back(profile.Id, profile.DisplayName);
		}
	}
	auto private_review_workflow::binding() const -> std::optional<core::private_review_binding> { return Binding; }
	auto private_review_workflow::default_run(const core::agent_revision_binding& aRevision, bool aConversation) const -> std::optional<application::private_run_intent>
	{
		if (Closing || !Current || !Binding) {
			return { };
		}
		auto found = std::ranges::find_if(KnownBindings.rbegin(), KnownBindings.rend(), [&](const auto& aBinding) { return aBinding.Scope == Current->Scope && aBinding.Revision == aRevision; });
		if (found == KnownBindings.rend()) {
			return { };
		}
		return application::private_run_intent{ .Binding = *found, .Purpose = aConversation ? core::private_run_purpose::conversation : core::private_run_purpose::findings };
	}
	void private_review_workflow::invalidate()
	{
		++Generation;
		Current.reset();
		Binding.reset();
		Index.reset();
		SelectedLine.reset();
		Editor->hide();
		Panel.present({ .Generation = Generation });
	}
	void private_review_workflow::shutdown()
	{
		Closing = true;
		invalidate();
	}
	auto private_review_workflow::is_quiescent() const noexcept -> bool { return Pending == 0; }
	void private_review_workflow::post(operation aOperation, draft_completion aCompletion)
	{
		if (Closing || !Current || !Binding) {
			if (aCompletion) {
				aCompletion(std::unexpected(application::private_run_error{ .Store = application::private_store_failure::unavailable }));
			}
			return;
		}
		const auto generation = Generation;
		auto       context    = Current;
		++Pending;
		Effects.post([this, generation, context, operation = std::move(aOperation), completion = std::move(aCompletion)]() mutable {
			result output;
			{
				const auto lock = std::scoped_lock{ Gate };
				try {
					output = operation(*context);
				} catch (const application::private_review_operations::refused& error) {
					output.Error = error.Error;
				} catch (...) {
					output.Error = application::private_run_error{ .Store = application::private_store_failure::unavailable };
				}
			}
			Decisions.post([this, generation, context, delivered = std::move(output), notify = std::move(completion)]() mutable {
				--Pending;
				if (Closing || generation != Generation || context != Current) {
					if (delivered.UnsubmittedPublication && delivered.Drafts) {
						++Pending;
						Effects.post([this, context, abandoned = std::move(*delivered.Drafts)] {
							{
								const auto                                       lock = std::scoped_lock{ Gate };
								application::review_composition_application_host host(*context->Store);
								for (const auto& record : abandoned.records()) {
									(void)host.change_draft({ { record.Draft.Draft.Id, core::review_draft_command_kind::fail_publish, { } }, record.Draft.Revision, false, { }, core::review_draft_scope_id{ 1 } });
								}
							}
							Decisions.post([this] { --Pending; });
						});
					}
					if (notify) {
						notify(std::unexpected(application::private_run_error{ .Policy = core::private_review_refusal::conflict }));
					}
					return;
				}
				if (notify) {
					notify(delivered.Drafts ? application::private_run_result<application::durable_draft_commit>{ std::move(*delivered.Drafts) } : std::unexpected(delivered.Error.value_or(application::private_run_error{ .Store = application::private_store_failure::unavailable })));
				} else if (delivered.Drafts && Drafts) {
					Drafts(*delivered.Drafts, delivered.OpenDraft);
				}
				if (delivered.Conversation) {
					delivered.Conversation->Generation = Generation;
					delivered.Conversation->Profiles   = Profiles;
					Panel.present(*delivered.Conversation);
				}
				if (delivered.Composition) {
					delivered.Composition->Generation = Generation;
					delivered.Composition->Profiles   = Profiles;
					Editor->present(*delivered.Composition);
					Editor->show();
				}
				if (delivered.BodySource && delivered.Body) {
					Panel.present_body(*delivered.BodySource, delivered.Body->Bytes, delivered.Body->Next.has_value());
				}
				if (delivered.FindingDetail) {
					Panel.present_finding(*delivered.FindingDetail);
				}
				if (delivered.Provenance) {
					Panel.present_provenance(*delivered.Provenance);
				}
				if (delivered.Error) {
					Panel.show_status(QString::fromStdString(message(*delivered.Error)));
					if (Editor->isVisible()) {
						Editor->show_status(message(*delivered.Error));
					}
				}
				if (delivered.Run && Launch) {
					if (!Launch(delivered.Profile, std::move(*delivered.Run), std::move(delivered.Requested), std::move(delivered.Prompt))) {
						Panel.show_status(QStringLiteral("Agent launch refused. No source was retargeted; choose current context and retry."));
					}
				}
				if (delivered.OpenDraft) {
					Editor->hide();
				}
				if (delivered.Refresh && !delivered.Error) {
					refresh();
				}
			});
		});
	}
	void private_review_workflow::refresh()
	{
		if (!Binding) {
			return;
		}
		const auto binding      = *Binding;
		const auto index        = Index;
		const auto selectedLine = SelectedLine;
		post([binding, index, selectedLine](context& aContext) {
			result output;
			output.Refresh = false;
			auto page      = aContext.Store->list_threads(aContext.IncludeArchived, aContext.Catalog);
			if (!page && page.error() == application::private_store_failure::stale_cursor) {
				aContext.Catalog.reset();
				page = aContext.Store->list_threads(aContext.IncludeArchived);
			}
			auto catalog         = take(std::move(page));
			aContext.NextCatalog = catalog.Next;
			auto projection      = application::private_conversation_projection{ .Available = true, .SelectedThread = aContext.Selected, .MoreThreads = catalog.Next.has_value(), .Status = "Private history: one bounded page. Source revisions remain immutable." };
			for (const auto& thread : catalog.Threads) {
				const auto* exact     = std::get_if<core::exact_private_subject>(&thread.Binding);
				const auto  available = exact && core::valid_private_subject(*exact, index.get());
				projection.Threads.push_back({ thread, core::observe_private_subject(thread.Binding, binding, available) });
			}
			if (!aContext.Selected && !catalog.Threads.empty()) {
				aContext.Selected         = catalog.Threads.front().Id;
				projection.SelectedThread = aContext.Selected;
			}
			aContext.Bodies.clear();
			if (aContext.Selected) {
				auto thread                = take(aContext.Store->load_thread(*aContext.Selected));
				projection.SelectedVersion = thread.Version;
				auto revisions             = aContext.Store->list_revisions(*aContext.Selected, aContext.Revisions);
				if (!revisions && revisions.error() == application::private_store_failure::stale_cursor) {
					aContext.Revisions.reset();
					revisions = aContext.Store->list_revisions(*aContext.Selected);
				}
				auto turns             = take(std::move(revisions));
				aContext.NextRevisions = turns.Next;
				projection.MoreTurns   = turns.Next.has_value();
				std::vector<core::private_source_reference> refs;
				for (const auto& turn : turns.Revisions) {
					auto source      = core::source_of(turn.Revision);
					source.BodyBytes = turn.BodyBytes;
					refs.push_back(source);
				}
				auto observed = take(aContext.Store->load_sources(refs));
				for (std::size_t i = 0; i < turns.Revisions.size(); ++i) {
					const auto& turn         = turns.Revisions[i];
					const auto& source       = observed[i];
					auto        availability = source.Availability;
					if (source.Binding && *source.Binding != binding) {
						availability = core::private_subject_availability::stale;
					}
					const auto  lineage = take(aContext.Store->source_lineage(source.Source));
					std::string badge   = lineage.Published ? "Composed · accepted as draft · published" : lineage.Draft ? "Composed · accepted as local draft"
						: lineage.Composition                                                                            ? "Referenced by local composition"
																														 : "";
					projection.Turns.push_back({ turn.Revision, { source.Source, source.Latest, source.Source.Key != source.Latest }, availability, turn.BodyBytes, turn.BodyContinuation.has_value(), std::move(badge) });
					if (turn.BodyContinuation) {
						aContext.Bodies.push_back(*turn.BodyContinuation);
					}
				}
			}
			auto findings = aContext.Store->list_findings(aContext.Findings);
			if (!findings && findings.error() == application::private_store_failure::stale_cursor) {
				aContext.Findings.reset();
				findings = aContext.Store->list_findings();
			}
			auto found              = take(std::move(findings));
			aContext.NextFindings   = found.Next;
			projection.MoreFindings = found.Next.has_value();
			projection.Findings     = take(aContext.Store->load_sources(found.Sources));
			for (auto& finding : projection.Findings) {
				if (finding.Binding && *finding.Binding != binding) {
					finding.Availability = core::private_subject_availability::stale;
				}
			}
			projection.IncludeArchived = aContext.IncludeArchived;
			projection.SavedScope      = aContext.Scope;
			for (const auto& draft : take(aContext.Store->list_drafts())) {
				projection.Drafts.push_back({ draft.Draft.Draft.Id, draft.Draft.Draft.Anchor, draft.Draft.Draft.State, std::string{ core::canonical_hex(draft.Binding.Revision.HeadOid).view() }, draft.Draft.Draft.InReview ? draft.Draft.Draft.InReview->Path : std::string{ }, draft.Draft.Draft.Body, draft.Binding == binding, core::unused_review_draft(draft.Draft), draft.Draft.Revision });
			}
			output.Conversation = std::move(projection);
			if (aContext.Composing) {
				auto                                        composition = application::private_composition_projection{ };
				std::vector<core::private_source_reference> sources;
				sources.reserve(aContext.Sources.size());
				for (const auto& source : aContext.Sources) {
					sources.push_back(source.Source);
				}
				composition.Sources = take(aContext.Store->load_sources(sources));
				for (auto& source : composition.Sources) {
					if (source.Binding && *source.Binding != binding) {
						source.Availability = core::private_subject_availability::stale;
					}
					if (source.Target && std::ranges::find(composition.Targets, *source.Target) == composition.Targets.end()) {
						composition.Targets.push_back(*source.Target);
					}
				}
				if (selectedLine && index) {
					auto target = core::make_composition_target(binding, *selectedLine, *index);
					if (target && std::ranges::find(composition.Targets, *target) == composition.Targets.end()) {
						composition.Targets.push_back(*target);
					}
				}
				for (auto id : aContext.Workspaces) {
					composition.Workspaces.push_back(take(aContext.Store->load_composition(id)).State);
				}
				if (aContext.Composition) {
					auto record         = take(aContext.Store->load_composition(*aContext.Composition));
					composition.Current = record.State;
					composition.Version = record.Version;
					if (record.State.Candidate) {
						composition.Candidate = take(aContext.Store->load_candidate(*record.State.Candidate));
					}
				}
				composition.Status = "Inspect exact sources and target. Multiple targets require an explicit choice or Split. Missing/stale targets require selecting current code; nothing is reanchored.";
				output.Composition = std::move(composition);
			}
			return output;
		});
	}
	void private_review_workflow::dispatch(application::private_conversation_intent aIntent)
	{
		const auto savedAction = aIntent.Action == application::private_conversation_action::discard_saved_draft || aIntent.Action == application::private_conversation_action::clear_unused_drafts;
		const auto admitted    = savedAction ? Current && aIntent.SavedScope && *aIntent.SavedScope == Current->Scope : aIntent.Generation == Generation;
		if (Closing || !admitted || !Binding) {
			if (!Closing && savedAction) {
				Panel.show_status(QStringLiteral("The review scope changed. Reopen Saved comments with its original account and pull request."));
			}
			return;
		}
		const auto binding = *Binding;
		const auto index   = Index;
		const auto line    = SelectedLine;

		post([intent = std::move(aIntent), binding, index, line](context& aContext) mutable {
			using enum application::private_conversation_action;
			result output;
			switch (intent.Action) {
			case discard_saved_draft: {
				require(intent.Draft.has_value());
				const auto record = take(aContext.Store->load_draft(*intent.Draft));
				require(record.Draft.Revision == intent.ExpectedVersion && record.Draft.Draft.State == core::review_draft_state::drafting);
				auto committed = accepted(application::review_composition_application_host(*aContext.Store).change_draft({ { *intent.Draft, core::review_draft_command_kind::discard, { } }, intent.ExpectedVersion, false, { }, core::review_draft_scope_id{ 1 } }));
				if (record.Binding.Revision.HeadOid == binding.Revision.HeadOid) {
					output.Drafts = std::move(committed);
				}
				break;
			}
			case clear_unused_drafts:
				output.Drafts = accepted(application::review_composition_application_host(*aContext.Store).clear_unused_drafts());
				break;
			case inspect_finding: {
				require(intent.Revision.has_value());
				const auto finding   = take(aContext.Store->load_finding(intent.Revision->Source));
				output.FindingDetail = application::serialize_private_finding(finding);
				const auto run       = std::get<core::private_finding_source>(finding.Source.Key).Run;
				auto       details   = take(aContext.Store->load_run_details(std::array{ run }));
				require(details.size() == 1);
				output.Provenance = details.front();
				output.Refresh    = false;
				break;
			}
			case include_archived:
				aContext.IncludeArchived = intent.IncludeArchived;
				aContext.Catalog.reset();
				aContext.Selected.reset();
				aContext.Revisions.reset();
				break;
			case open_draft: {
				require(intent.Draft.has_value());
				auto record = take(aContext.Store->load_draft(*intent.Draft));
				require(!core::unused_review_draft(record.Draft) && index && record.Binding == binding && core::make_composition_target(binding, record.Draft.Draft.Anchor, *index).has_value(), core::private_review_refusal::invalid_target);
				output.Drafts    = accepted(application::review_composition_application_host(*aContext.Store).restore(binding.Revision.HeadOid, intent.Draft));
				output.OpenDraft = true;
				break;
			}
			case next_findings:
				require(aContext.NextFindings.has_value());
				aContext.Findings = aContext.NextFindings;
				break;
			case refresh:
				aContext.Selected.reset();
				aContext.Findings.reset();
				aContext.Catalog.reset();
				aContext.Revisions.reset();
				break;
			case next_threads:
				require(aContext.NextCatalog.has_value());
				aContext.Catalog = aContext.NextCatalog;
				aContext.Selected.reset();
				aContext.Revisions.reset();
				break;
			case select_thread:
				require(intent.Thread.has_value());
				take(aContext.Store->load_thread(*intent.Thread));
				aContext.Selected = intent.Thread;
				aContext.Revisions.reset();
				break;
			case next_turns:
				require(aContext.NextRevisions.has_value());
				aContext.Revisions = aContext.NextRevisions;
				break;
			case read_body: {
				require(intent.Revision.has_value());
				auto cursor = std::ranges::find_if(aContext.Bodies, [&](const auto& aCursor) { return aCursor.Source == intent.Revision->Source; });
				require(cursor != aContext.Bodies.end());
				output.BodySource = cursor->Source;
				output.Body       = take(aContext.Store->read_revision_body(*cursor));
				if (output.Body->Next) {
					*cursor = *output.Body->Next;
				} else {
					aContext.Bodies.erase(cursor);
				}
				output.Refresh = false;
				break;
			}
			case provenance: {
				require(intent.Revision.has_value());
				auto        sources = take(aContext.Store->load_sources(std::array{ intent.Revision->Source }));
				const auto* actor   = std::get_if<core::private_agent_actor>(&sources.front().Actor);
				require(actor != nullptr, core::private_review_refusal::invalid_actor);
				auto runs = take(aContext.Store->load_run_details(std::array{ actor->Run }));
				require(runs.size() == 1);
				output.Provenance = runs.front();
				output.Refresh    = false;
				break;
			}
			case compose:
				require(!intent.Sources.empty());
				aContext.Sources = std::move(intent.Sources);
				aContext.Composition.reset();
				aContext.Workspaces.clear();
				aContext.Composing = true;
				break;
			case ask_agent: {
				require(intent.Thread.has_value());
				auto  thread  = take(aContext.Store->load_thread(*intent.Thread));
				auto* subject = std::get_if<core::exact_private_subject>(&thread.Binding);
				require(subject && subject->Binding == binding, core::private_review_refusal::invalid_binding);
				output.Run       = application::private_run_intent{ .Binding = subject->Binding, .Thread = thread.Id, .ExpectedThreadVersion = intent.ExpectedVersion, .Sources = intent.Sources };
				output.Profile   = intent.Profile;
				output.Requested = intent.Requested;
				output.Prompt    = std::move(intent.Body);
				break;
			}
			case erase:
				require(intent.Thread.has_value());
				require(aContext.Store->erase_thread(*intent.Thread, intent.ExpectedVersion).has_value());
				aContext.Selected.reset();
				aContext.Revisions.reset();
				aContext.Catalog.reset();
				break;
			case archive: {
				require(intent.Thread.has_value());
				auto thread  = take(aContext.Store->load_thread(*intent.Thread));
				auto changed = decide(core::set_private_lifecycle(thread, intent.ExpectedVersion, core::private_thread_lifecycle::archived));
				auto f       = take(aContext.Store->frontier());
				take(aContext.Store->commit({ .ExpectedNextIdentity = f.NextIdentity, .Threads = { { changed, intent.ExpectedVersion } } }));
				aContext.Selected.reset();
				aContext.Revisions.reset();
				aContext.Catalog.reset();
				break;
			}
			case create_pr:
			case create_selected:
			case reply:
			case edit:            {
				auto       f    = take(aContext.Store->frontier());
				const auto next = f.NextIdentity;
				require(next > 0 && next <= core::maximum_private_identity - 3, core::private_review_refusal::bounds);
				core::private_revision_commit               proposal;
				std::uint64_t                               expected = 0;
				std::vector<core::private_source_selection> guards;
				if (intent.Action == create_pr || intent.Action == create_selected) {
					core::private_thread_subject subject = core::private_pr_subject{ };
					if (intent.Action == create_selected) {
						require(line && index, core::private_review_refusal::invalid_target);
						subject = *line;
					}
					proposal = decide(core::create_private_thread(core::private_review_thread_id{ next }, { binding, subject }, { core::private_review_turn_id{ next + 1 }, core::private_review_turn_revision_id{ next + 2 }, core::private_vertex_order{ next + 2 }, core::private_reviewer_actor{ f.Scope.Account }, std::move(intent.Body) }, index.get()));
				} else {
					require(intent.Thread.has_value());
					auto thread = take(aContext.Store->load_thread(*intent.Thread));
					expected    = intent.ExpectedVersion;
					if (intent.Action == edit) {
						require(intent.Revision.has_value() && !intent.Revision->Historical);
						auto prior = revision(*aContext.Store, *intent.Revision);
						proposal   = decide(core::edit_private_revision(thread, expected, prior, prior.Identity.Revision, { prior.Identity.Turn, core::private_review_turn_revision_id{ next }, core::private_vertex_order{ next }, core::private_reviewer_actor{ f.Scope.Account }, std::move(intent.Body) }));
						guards.push_back(*intent.Revision);
					} else {
						proposal = decide(core::append_private_turn(thread, expected, { core::private_review_turn_id{ next }, core::private_review_turn_revision_id{ next + 1 }, core::private_vertex_order{ next + 1 }, core::private_reviewer_actor{ f.Scope.Account }, std::move(intent.Body) }));
					}
				}
				take(aContext.Store->commit({ .ExpectedNextIdentity = next, .Threads = { { proposal.Thread, expected } }, .Revisions = { proposal.Revision }, .SourceGuards = guards }));
				aContext.Selected = proposal.Thread.Id;
				aContext.Revisions.reset();
				aContext.Catalog.reset();
				break;
			}
			}
			return output;
		});
	}
	void private_review_workflow::compose(application::private_composition_intent aIntent)
	{
		if (Closing || aIntent.Generation != Generation || !Binding) {
			return;
		}
		const auto binding = *Binding;
		const auto index   = Index;
		post([intent = std::move(aIntent), binding, index](context& aContext) mutable {
			using enum application::private_composition_action;
			result                                           output;
			application::review_composition_application_host host(*aContext.Store);
			switch (intent.Action) {
			case create: {
				auto record          = accepted(host.create({ binding, intent.Mode, aContext.Sources, intent.Target, std::move(intent.Body) }));
				aContext.Composition = record.State.Id;
				aContext.Workspaces  = { record.State.Id };
				break;
			}
			case split: {
				auto split = accepted(host.split({ binding, intent.Mode, aContext.Sources, { }, std::move(intent.Body) }));
				aContext.Workspaces.clear();
				for (const auto& record : split.Compositions) {
					aContext.Workspaces.push_back(record.State.Id);
				}
				if (!split.Compositions.empty()) {
					aContext.Composition = split.Compositions.front().State.Id;
				}
				break;
			}
			case select:
				require(intent.Composition.has_value() && std::ranges::find(aContext.Workspaces, *intent.Composition) != aContext.Workspaces.end());
				aContext.Composition = intent.Composition;
				break;
			case revise:
				require(intent.Composition.has_value());
				accepted(host.revise(*intent.Composition, intent.ExpectedVersion, std::move(intent.Body)));
				break;
			case target:
				require(intent.Composition && intent.Target && index);
				accepted(host.target(*intent.Composition, intent.ExpectedVersion, *intent.Target, *index));
				break;
			case ask_agent:
				require(intent.Composition.has_value());
				output.Run       = accepted(host.agent_intent(*intent.Composition, intent.ExpectedVersion));
				output.Profile   = intent.Profile;
				output.Requested = intent.Requested;
				break;
			case accept:
				require(intent.Composition && intent.Candidate && index);
				output.Drafts      = accepted(host.accept(std::array{ application::review_composition_acceptance{ *intent.Composition, intent.ExpectedVersion, *intent.Candidate } }, binding, *index, core::review_draft_scope_id{ 1 }));
				output.OpenDraft   = true;
				aContext.Composing = false;
				break;
			case cancel: aContext.Composing = false; break;
			}
			return output;
		});
	}
	void private_review_workflow::changed(const application::private_review_delta& aDelta)
	{
		if (!Current || aDelta.Scope != Current->Scope) {
			return;
		}
		post([aDelta](context& aContext) {
			// A completion selects its immutable result rather than appending it again.
			if (aDelta.Thread) {
				aContext.Selected = aDelta.Thread;
				aContext.Revisions.reset();
				aContext.Catalog.reset();
			}
			if (!aDelta.Findings.empty()) {
				aContext.Findings.reset();
			}
			return result{ };
		});
	}
	void private_review_workflow::restore(draft_completion aCompletion)
	{
		if (!Binding) {
			return;
		}
		const auto head       = Binding->Revision.HeadOid;
		const auto generation = Generation;
		const auto prior      = Current;
		post([head](context& aContext) {
			// Restore only drafts under the currently accepted head.
			result                                           output;
			application::review_composition_application_host host(*aContext.Store);
			output.Drafts  = accepted(host.restore(head));
			output.Refresh = false;
			return output; },
			 [this, generation, prior, complete = std::move(aCompletion)](auto aCommit) mutable {
				 // A comparison can retire the read while it waits on either
				 // lane. Restore current drafts once the new binding settles;
				 // an old conflict must not leave the caller unready forever.
				 // A store refusal, closed workflow or different PR context
				 // answers normally and never starts a retry loop.
				 if (!Closing && prior == Current && Binding && generation != Generation) {
					 restore(std::move(complete));
					 return;
				 }
				 if (complete) {
					 complete(std::move(aCommit));
				 }
			 });
	}
	void private_review_workflow::select_draft(core::source_line_ref aLine, draft_completion aCompletion)
	{
		if (!Binding || !Index) {
			return;
		}
		post([binding = *Binding, index = Index, aLine](context& aContext) {
			// [[BPR-1374]]. The host opens the line's latest new comment again,
			// as core::reopen_draft_at decides, or makes one.
			result                                           output;
			application::review_composition_application_host host(*aContext.Store);
			output.Drafts  = accepted(host.select_draft(binding, aLine, *index, { }, false));
			output.Refresh = false;
			return output;
		},
			 std::move(aCompletion));
	}
	void private_review_workflow::select_reply(core::source_line_ref aLine, std::string aThread, draft_completion aCompletion)
	{
		if (!Binding || !Index) {
			aCompletion(std::unexpected(application::private_run_error{ .Policy = core::private_review_refusal::invalid_target }));
			return;
		}
		post([binding = *Binding, index = Index, aLine, thread = std::move(aThread)](context& aContext) {
			// [[BPR-1374]]. The reply already written in this thread opens
			// again, whether it was written before a Refresh or a restart.
			result                                           output;
			application::review_composition_application_host host(*aContext.Store);
			output.Drafts  = accepted(host.select_draft(binding, aLine, *index, thread));
			output.Refresh = false;
			return output;
		},
			 std::move(aCompletion));
	}
	void private_review_workflow::materialize_draft(core::revisioned_review_draft aPlaceholder, core::revisioned_review_draft_command aCommand, draft_completion aCompletion)
	{
		if (!Binding || !Index) {
			if (aCompletion) {
				aCompletion(std::unexpected(application::private_run_error{ .Policy = core::private_review_refusal::invalid_target }));
			}
			return;
		}
		post([binding = *Binding, index = Index, placeholder = std::move(aPlaceholder), command = std::move(aCommand)](context& aContext) {
			result output;
			output.Drafts = accepted(application::review_composition_application_host(*aContext.Store).materialize_draft(binding, *index, placeholder, command));
			return output;
		},
			 std::move(aCompletion));
	}
	void private_review_workflow::change_draft(core::revisioned_review_draft_command aCommand, draft_completion aCompletion)
	{
		post([command = std::move(aCommand)](context& aContext) {
			// An undelivered publish admission is rolled back on generation retirement.
			result                                           output;
			application::review_composition_application_host host(*aContext.Store);
			output.Drafts                 = accepted(host.change_draft(command));
			output.UnsubmittedPublication = command.Command.Kind == core::review_draft_command_kind::begin_publish;
			output.Refresh                = false;
			return output;
		},
			 std::move(aCompletion));
	}
	void private_review_workflow::change_drafts(std::vector<core::revisioned_review_draft_command> aCommands, draft_completion aCompletion)
	{
		post([commands = std::move(aCommands)](context& aContext) {
			result                                           output;
			application::review_composition_application_host host(*aContext.Store);
			output.Drafts  = accepted(host.change_drafts(commands));
			output.Refresh = false;
			return output;
		},
			 std::move(aCompletion));
	}
	void private_review_workflow::earlier_suggestions(earlier_completion aCompletion)
	{
		if (Closing || !Current || !Binding) {
			if (aCompletion) {
				aCompletion({ });
			}
			return;
		}
		const auto generation = Generation;
		auto       context    = Current;
		const auto head       = Binding->Revision.HeadOid;
		++Pending;
		Effects.post([this, generation, context, head, completion = std::move(aCompletion)]() mutable {
			auto found = std::vector<earlier_suggestion>{ };
			{
				const auto lock = std::scoped_lock{ Gate };
				try {
					for (auto& record : take(context->Store->list_drafts())) {
						if (record.Binding.Revision.HeadOid != head && core::decide_suggestion_head_move(record.Draft.Draft, std::nullopt).has_value()) {
							found.push_back(earlier_suggestion{ .Draft = std::move(record.Draft), .Head = record.Binding.Revision.HeadOid });
						}
					}
				} catch (...) {
					found.clear();
				}
			}
			Decisions.post([this, generation, context, delivered = std::move(found), notify = std::move(completion)]() mutable {
				--Pending;
				if (Closing || generation != Generation || context != Current) {
					delivered.clear();
				}
				if (notify) {
					notify(std::move(delivered));
				}
			});
		});
	}
	void private_review_workflow::discard_earlier_suggestion(const core::review_draft_id aDraft, const std::uint64_t aRevision, std::function<void(bool)> aDone)
	{
		const auto head = Binding ? std::optional{ Binding->Revision.HeadOid } : std::nullopt;
		post([aDraft, aRevision, head](context& aContext) {
			result     output;
			const auto record = take(aContext.Store->load_draft(aDraft));
			require(record.Draft.Revision == aRevision && record.Draft.Draft.State == core::review_draft_state::drafting && std::optional{ record.Binding.Revision.HeadOid } != head
					&& core::decide_suggestion_head_move(record.Draft.Draft, std::nullopt).has_value());
			// Not the bound head's: the shared drafts never held it, so its
			// commit goes to nobody but the caller.
			output.Drafts  = accepted(application::review_composition_application_host(*aContext.Store).change_draft({ { aDraft, core::review_draft_command_kind::discard, { } }, aRevision, false, { }, core::review_draft_scope_id{ 1 } }));
			output.Refresh = false;
			return output; },
			 [done = std::move(aDone)](auto aCommit) {
				 if (done) {
					 done(aCommit.has_value());
				 }
			 });
	}
	void private_review_workflow::settle_review_comments(std::string aKey, bool aPublished, std::optional<core::review_submission_proposal> aProposal)
	{
		if (!Binding) {
			return;
		}
		post([key = std::move(aKey), aPublished, proposal = std::move(aProposal), head = Binding->Revision.HeadOid](context& aContext) {
			result                                           output;
			application::review_composition_application_host host(*aContext.Store);
			accepted(host.settle_review(key, aPublished, proposal));
			output.Drafts = accepted(host.restore(head));
			return output;
		});
	}
	void private_review_workflow::record_workshop_press(core::review_draft_id aDraft, press_completion aCompletion)
	{
		if (Closing || !Current || !Binding || !Index) {
			aCompletion(std::unexpected(application::private_run_error{ .Store = application::private_store_failure::unavailable }));
			return;
		}
		const auto generation = Generation;
		auto       context    = Current;
		++Pending;
		Effects.post([this, generation, context, binding = *Binding, index = Index, aDraft, completion = std::move(aCompletion)]() mutable {
			auto pressed = [&] {
				const auto lock = std::scoped_lock{ Gate };
				return application::private_review_application_host(*context->Store).record_workshop_press(binding, aDraft, *index);
			}();
			Decisions.post([this, generation, context, recorded = std::move(pressed), notify = std::move(completion)]() mutable {
				--Pending;
				if (Closing || generation != Generation || context != Current) {
					notify(std::unexpected(application::private_run_error{ .Policy = core::private_review_refusal::conflict }));
					return;
				}
				if (recorded) {
					changed({ .Scope = context->Scope, .Thread = recorded->Thread });
				} else {
					Panel.show_status(QString::fromStdString(message(recorded.error())));
				}
				notify(std::move(recorded));
			});
		});
	}
	auto private_review_workflow::find(const core::private_review_scope& aScope) -> std::shared_ptr<context>
	{
		auto found = std::ranges::find_if(Contexts, [&](const auto& aContext) { return aContext->Scope == aScope; });
		return found == Contexts.end() ? nullptr : *found;
	}
	auto private_review_workflow::admit(const application::private_run_intent& aIntent, const core::agent_profile& aProfile, const core::agent_session& aSession, const application::agent_context_observation& aContext, const core::agent_setting_facts& aSettings) -> application::private_run_result<application::private_run_admission>
	{
		const auto lock    = std::scoped_lock{ Gate };
		auto       context = find(aIntent.Binding.Scope);
		if (!context) {
			return std::unexpected(application::private_run_error{ .Store = application::private_store_failure::unavailable });
		}
		return application::private_review_application_host(*context->Store).admit(aIntent, aProfile, aSession, aContext, aSettings);
	}
	auto private_review_workflow::claim(const application::private_run_admission& aAdmission, std::stop_token aStop) -> application::private_run_result<std::shared_ptr<application::agent_input_claim>>
	{
		const auto lock    = std::scoped_lock{ Gate };
		auto       context = find(aAdmission.Delta.Scope);
		if (!context) {
			return std::unexpected(application::private_run_error{ .Store = application::private_store_failure::unavailable });
		}
		// The source guard lives through the process input write. Its transaction must
		// not share the presentation connection while that connection pages history.
		auto store = adapters::sqlite::make_private_review_store(context->Configuration, aStop);
		if (!store) {
			return std::unexpected(application::private_run_error{ .Store = store.error() });
		}
		auto inner = application::private_review_application_host(**store).claim(aAdmission, aStop);
		if (!inner) {
			return std::unexpected(inner.error());
		}
		struct isolated_claim final : application::agent_input_claim
		{
			std::unique_ptr<application::private_review_store_port> Store;
			std::shared_ptr<application::agent_input_claim>         Inner;
			auto                                                    permitted() const noexcept -> bool override { return Inner->permitted(); }
			void                                                    release() noexcept override { Inner->release(); }
		};
		auto claim   = std::make_shared<isolated_claim>();
		claim->Store = std::move(*store);
		claim->Inner = std::move(*inner);
		return claim;
	}
	auto private_review_workflow::finish(const application::private_run_admission& aAdmission, const application::agent_run_result& aResult, const core::agent_finding_snapshot& aSnapshot) -> application::private_run_result<application::private_review_delta>
	{
		const auto lock    = std::scoped_lock{ Gate };
		auto       context = find(aAdmission.Delta.Scope);
		if (!context) {
			return std::unexpected(application::private_run_error{ .Store = application::private_store_failure::unavailable });
		}
		return application::private_review_application_host(*context->Store).finish(aAdmission, aResult, aSnapshot);
	}
	auto private_review_workflow::publication_context(const application::review_mutation_request& aRequest) -> std::shared_ptr<context>
	{
		if (!aRequest.Proposal.Durable) {
			return { };
		}
		auto found = std::ranges::find_if(Contexts, [&](const auto& aContext) {const auto& s=aContext->Scope;return s.Incarnation.bytes()==aRequest.Proposal.Durable->Incarnation && s.Host==aRequest.Target.Host && s.Account==aRequest.ActingIdentity && s.Repository==aRequest.Target.Repository && s.PullRequest==aRequest.Target.PullRequest; });
		return found == Contexts.end() ? nullptr : *found;
	}
	auto private_review_workflow::prepare(const application::review_mutation_request& aRequest, std::string_view aLogical, std::string_view aWire) -> bool
	{
		if (!aRequest.Proposal.Durable) {
			return true;
		}
		const auto lock    = std::scoped_lock{ Gate };
		auto       context = publication_context(aRequest);
		return context && application::private_publication_application_host(*context->Store).prepare(aRequest, aLogical, aWire);
	}
	auto private_review_workflow::complete(const application::review_mutation_request& aRequest, std::string_view aLogical, std::string_view aWire, const application::review_mutation_receipt& aReceipt) -> bool
	{
		if (!aRequest.Proposal.Durable) {
			return true;
		}
		const auto lock    = std::scoped_lock{ Gate };
		auto       context = publication_context(aRequest);
		return context && application::private_publication_application_host(*context->Store).complete(aRequest, aLogical, aWire, aReceipt);
	}
	auto private_review_workflow::pending(const core::review_mutation_target& aTarget, core::github_account_id aActor) -> std::vector<application::private_publication_record>
	{
		const auto                                           lock = std::scoped_lock{ Gate };
		std::vector<application::private_publication_record> rows;
		for (const auto& context : Contexts) {
			const auto& scope = context->Scope;
			if (scope.Host != aTarget.Host || scope.Repository != aTarget.Repository
				|| scope.PullRequest != aTarget.PullRequest || scope.Account != aActor) {
				continue;
			}
			auto pending = application::private_publication_application_host(*context->Store).pending(aTarget, aActor);
			rows.insert(rows.end(), std::make_move_iterator(pending.begin()), std::make_move_iterator(pending.end()));
		}
		return rows;
	}
	auto private_review_workflow::complete_observed(const application::private_publication_record& aRecord, std::string_view aWire, const application::review_mutation_receipt& aReceipt) -> bool
	{
		const auto lock    = std::scoped_lock{ Gate };
		auto       context = find(aRecord.Target.Binding.Scope);
		return context && application::private_publication_application_host(*context->Store).complete_observed(aRecord, aWire, aReceipt);
	}
}
