#include "bpr/application/review_cycle_runtime.hpp"

#include <algorithm>
#include <array>
#include <deque>
#include <utility>

namespace bpr::application {
	namespace {
		auto cancelled() -> cycle_store_error
		{
			return cycle_store_error{ .Failure = cycle_store_failure::cancelled };
		}
		/// The cycle store never answers for a pull-request selection.
		auto refused_selection() -> cycle_store_error
		{
			return cycle_store_error{ .Failure = cycle_store_failure::refused, .Refusal = core::cycle_refusal::invalid_identity };
		}
	}

	struct review_cycle_runtime::implementation : std::enable_shared_from_this<implementation>
	{
		implementation(std::unique_ptr<review_cycle_store> aStore, effect_executor& aEffects, decision_executor& aDecisions)
			: Store{ std::move(aStore) }
			, Effects{ aEffects }
			, Decisions{ aDecisions }
		{
		}

		std::unique_ptr<review_cycle_store> Store;
		/// Created on first use; used only by work on the effect lane, which
		/// runs one call at a time.
		std::shared_ptr<review_cycle_comparison_host> Comparisons;
		effect_executor&                              Effects;
		decision_executor&                            Decisions;
		std::optional<core::review_subject>           Selected;
		std::uint64_t                                 Generation = 0;
		std::stop_source                              Stop;
		std::deque<std::function<void()>>             Queue;
		std::size_t                                   Outstanding = 0;
		bool                                          Running     = false;
		bool                                          Closing     = false;

		/// Starts the next queued store call when none is running.
		void pump()
		{
			if (Running || Queue.empty()) {
				return;
			}
			Running   = true;
			auto next = std::move(Queue.front());
			Queue.pop_front();
			next();
		}

		/// Queues one store call; its result returns to the decision lane.
		template <typename T, typename TWork>
		auto submit(TWork aWork, completion<T> aDone) -> bool
		{
			if (Closing) {
				return false;
			}
			++Outstanding;
			Queue.push_back([self = shared_from_this(), work = std::move(aWork), done = std::move(aDone), generation = Generation, token = Stop.get_token()]() mutable {
				self->dispatch<T>(std::move(work), std::move(done), generation, token);
			});
			pump();
			return true;
		}

		/// Runs the call on the effect lane.
		template <typename T, typename TWork>
		void dispatch(TWork aWork, completion<T> aDone, const std::uint64_t aGeneration, std::stop_token aToken)
		{
			Effects.post([self = shared_from_this(), work = std::move(aWork), done = std::move(aDone), aGeneration, aToken]() mutable {
				self->complete<T>(work(*self->Store, aToken), std::move(done), aGeneration);
			});
		}

		/// Returns the result to the decision lane.
		template <typename T>
		void complete(cycle_store_result<T> aResult, completion<T> aDone, const std::uint64_t aGeneration)
		{
			Decisions.post([self = shared_from_this(), result = std::move(aResult), done = std::move(aDone), aGeneration]() mutable {
				self->finish<T>(std::move(result), std::move(done), aGeneration);
			});
		}

		/// Delivers one terminal; a superseded selection's result is cancelled.
		template <typename T>
		void finish(cycle_store_result<T> aResult, completion<T> aDone, const std::uint64_t aGeneration)
		{
			if (aGeneration != Generation) {
				aResult = std::unexpected(cancelled());
			}
			--Outstanding;
			Running = false;
			aDone(std::move(aResult));
			pump();
		}

		auto comparisons(const core::local_cycle_authority& aAuthority) -> std::shared_ptr<review_cycle_comparison_host>
		{
			if (!Comparisons) {
				Comparisons = std::make_shared<review_cycle_comparison_host>(*Store, aAuthority);
			}
			return Comparisons;
		}

		/// The cycle subject currently selected, if the selection is one.
		[[nodiscard]] auto selected_cycle() const -> std::optional<core::local_cycle_subject>
		{
			if (!Selected || !std::holds_alternative<core::local_cycle_subject>(*Selected)) {
				return std::nullopt;
			}
			return std::get<core::local_cycle_subject>(*Selected);
		}
	};

	review_cycle_runtime::review_cycle_runtime(std::unique_ptr<review_cycle_store> aStore, effect_executor& aEffects, decision_executor& aDecisions)
		: Implementation{ std::make_shared<implementation>(std::move(aStore), aEffects, aDecisions) }
	{
	}

	review_cycle_runtime::~review_cycle_runtime()
	{
		shutdown();
	}

	auto review_cycle_runtime::select(core::review_subject aSubject) -> std::uint64_t
	{
		auto& self = *Implementation;
		self.Stop.request_stop();
		self.Stop     = std::stop_source{ };
		self.Selected = std::move(aSubject);
		return ++self.Generation;
	}

	auto review_cycle_runtime::selected() const noexcept -> const std::optional<core::review_subject>&
	{
		return Implementation->Selected;
	}

	auto review_cycle_runtime::open(completion<core::review_cycle_catalog> aDone) -> bool
	{
		return Implementation->submit<core::review_cycle_catalog>([](review_cycle_store& aStore, std::stop_token aStop) { return aStore.catalog(aStop); }, std::move(aDone));
	}

	auto review_cycle_runtime::create(cycle_operation_id aOperation, core::create_review_cycle aCommand, completion<cycle_write_receipt> aDone) -> bool
	{
		return Implementation->submit<cycle_write_receipt>([aOperation, command = std::move(aCommand)](review_cycle_store& aStore, std::stop_token aStop) {
			return aStore.create(aOperation, command, aStop);
		},
														   std::move(aDone));
	}

	auto review_cycle_runtime::load_selected(completion<std::optional<core::review_cycle_state>> aDone) -> bool
	{
		const auto subject = Implementation->selected_cycle();
		return Implementation->submit<std::optional<core::review_cycle_state>>([subject](review_cycle_store& aStore, std::stop_token aStop) -> cycle_store_result<std::optional<core::review_cycle_state>> {
			if (!subject) {
				return std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::refused, .Refusal = core::cycle_refusal::invalid_identity });
			}
			return aStore.load(*subject, aStop);
		},
																			   std::move(aDone));
	}

	auto review_cycle_runtime::capture_selected(cycle_operation_id aOperation, std::uint64_t aExpectedRevision, core::cycle_capture_observation aCapture,
												std::vector<std::uint8_t> aManifest, completion<cycle_write_receipt> aDone) -> bool
	{
		const auto subject = Implementation->selected_cycle();
		return Implementation->submit<cycle_write_receipt>([subject, aOperation, aExpectedRevision, capture = aCapture, manifest = std::move(aManifest)](review_cycle_store& aStore, std::stop_token aStop) mutable -> cycle_store_result<cycle_write_receipt> {
			if (!subject) {
				return std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::refused, .Refusal = core::cycle_refusal::invalid_identity });
			}
			const auto object = aStore.install(manifest, aStop);
			if (!object) {
				return std::unexpected(object.error());
			}
			capture.Material = object->Digest;
			return aStore.record_capture(aOperation, { .Subject = *subject, .ExpectedRevision = aExpectedRevision, .Capture = { .Capture = capture }, .Manifest = *object }, aStop);
		},
														   std::move(aDone));
	}

	auto review_cycle_runtime::capture_workspace(workspace_source_port& aSource, cycle_capture_request aRequest, completion<cycle_capture_outcome> aDone) -> bool
	{
		const auto subject = Implementation->selected_cycle();
		return Implementation->submit<cycle_capture_outcome>([subject, source = &aSource, request = std::move(aRequest)](review_cycle_store& aStore, std::stop_token aStop) -> cycle_store_result<cycle_capture_outcome> {
			if (!subject) {
				return std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::refused, .Refusal = core::cycle_refusal::invalid_identity });
			}
			const auto loaded = aStore.load(*subject, aStop);
			if (!loaded) {
				return std::unexpected(loaded.error());
			}
			if (!*loaded) {
				return std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::not_found });
			}
			auto host = review_cycle_capture_host{ *source, aStore };
			return host.capture(**loaded, request, std::move(aStop));
		},
															 std::move(aDone));
	}

	auto review_cycle_runtime::compare_selected(core::cycle_comparison_pair aPair, std::optional<std::string> aAfter, const std::size_t aLimit, const bool aChangedOnly,
												completion<cycle_comparison_page_observation> aDone) -> bool
	{
		const auto subject = Implementation->selected_cycle();
		auto       host    = subject ? Implementation->comparisons(subject->Authority) : nullptr;
		return Implementation->submit<cycle_comparison_page_observation>([subject, host, aPair, after = std::move(aAfter), aLimit, aChangedOnly](review_cycle_store&, std::stop_token aStop) -> cycle_store_result<cycle_comparison_page_observation> {
			if (!subject) {
				return std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::refused, .Refusal = core::cycle_refusal::invalid_identity });
			}
			return host->page(*subject, aPair, after, aLimit, aChangedOnly, std::move(aStop));
		},
																		 std::move(aDone));
	}

	auto review_cycle_runtime::compare_file_selected(core::cycle_comparison_pair aPair, std::string aPath, core::diff_options aOptions, completion<cycle_file_observation> aDone) -> bool
	{
		const auto subject = Implementation->selected_cycle();
		auto       host    = subject ? Implementation->comparisons(subject->Authority) : nullptr;
		return Implementation->submit<cycle_file_observation>([subject, host, aPair, path = std::move(aPath), aOptions](review_cycle_store&, std::stop_token aStop) -> cycle_store_result<cycle_file_observation> {
			if (!subject) {
				return std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::refused, .Refusal = core::cycle_refusal::invalid_identity });
			}
			return host->file(*subject, aPair, path, aOptions, std::move(aStop));
		},
															  std::move(aDone));
	}

	auto review_cycle_runtime::begin_feedback_selected(cycle_operation_id aOperation, const std::uint64_t aExpectedRevision, core::begin_cycle_feedback aCommand,
													   completion<cycle_write_receipt> aDone) -> bool
	{
		const auto subject = Implementation->selected_cycle();
		auto       host    = subject ? Implementation->comparisons(subject->Authority) : nullptr;
		return Implementation->submit<cycle_write_receipt>([subject, host, aOperation, aExpectedRevision, command = std::move(aCommand)](review_cycle_store& aStore, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> {
			if (!subject) {
				return std::unexpected(refused_selection());
			}
			auto targets = std::vector<const core::cycle_feedback_target*>{ &command.Primary };
			for (const auto& reference : command.Related) {
				targets.push_back(&reference.Target);
			}
			if (auto verified = verify_feedback_targets(*host, *subject, targets, aStop); !verified) {
				return std::unexpected(verified.error());
			}
			return aStore.begin_feedback(aOperation, *subject, aExpectedRevision, command, aStop);
		},
														   std::move(aDone));
	}

	auto review_cycle_runtime::change_feedback_selected(cycle_operation_id aOperation, core::cycle_feedback_item_ref aItem, const std::uint64_t aExpectedVersion,
														core::cycle_feedback_change aChange, completion<cycle_write_receipt> aDone) -> bool
	{
		const auto subject = Implementation->selected_cycle();
		auto       host    = subject ? Implementation->comparisons(subject->Authority) : nullptr;
		return Implementation->submit<cycle_write_receipt>([subject, host, aOperation, aItem, aExpectedVersion, change = std::move(aChange)](review_cycle_store& aStore, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> {
			if (!subject) {
				return std::unexpected(refused_selection());
			}
			if (const auto* reference = std::get_if<core::add_feedback_reference>(&change)) {
				const auto targets = std::array{ &reference->Reference.Target };
				if (auto verified = verify_feedback_targets(*host, *subject, targets, aStop); !verified) {
					return std::unexpected(verified.error());
				}
			}
			return aStore.change_feedback(aOperation, *subject, aItem, aExpectedVersion, change, aStop);
		},
														   std::move(aDone));
	}

	auto review_cycle_runtime::feedback_selected(std::optional<core::cycle_feedback_item_id> aAfter, completion<cycle_feedback_page> aDone) -> bool
	{
		const auto subject = Implementation->selected_cycle();
		return Implementation->submit<cycle_feedback_page>([subject, aAfter](review_cycle_store& aStore, std::stop_token aStop) -> cycle_store_result<cycle_feedback_page> {
			if (!subject) {
				return std::unexpected(refused_selection());
			}
			return aStore.feedback(*subject, aAfter, aStop);
		},
														   std::move(aDone));
	}

	auto review_cycle_runtime::feedback_revisions_selected(core::cycle_feedback_item_ref aItem, std::optional<core::private_review_turn_revision_id> aAfter,
														   completion<cycle_feedback_revision_page> aDone) -> bool
	{
		const auto subject = Implementation->selected_cycle();
		return Implementation->submit<cycle_feedback_revision_page>([subject, aItem, aAfter](review_cycle_store& aStore, std::stop_token aStop) -> cycle_store_result<cycle_feedback_revision_page> {
			if (!subject) {
				return std::unexpected(refused_selection());
			}
			return aStore.feedback_revisions(*subject, aItem, aAfter, aStop);
		},
																	std::move(aDone));
	}

	auto review_cycle_runtime::observe_feedback_selected(core::cycle_feedback_target aTarget, const core::cycle_source_revision_id aAgainst, completion<cycle_correspondence_observation> aDone) -> bool
	{
		const auto subject = Implementation->selected_cycle();
		auto       host    = subject ? Implementation->comparisons(subject->Authority) : nullptr;
		return Implementation->submit<cycle_correspondence_observation>([subject, host, target = std::move(aTarget), aAgainst](review_cycle_store&, std::stop_token aStop) -> cycle_store_result<cycle_correspondence_observation> {
			if (!subject) {
				return std::unexpected(refused_selection());
			}
			return observe_feedback_target(*host, *subject, target, aAgainst, std::move(aStop));
		},
																		std::move(aDone));
	}

	auto review_cycle_runtime::add_suggestion_selected(cycle_operation_id aOperation, core::cycle_feedback_item_ref aItem, const std::uint64_t aExpectedVersion, core::suggest_change aChange,
													   completion<cycle_write_receipt> aDone) -> bool
	{
		const auto subject = Implementation->selected_cycle();
		auto       host    = subject ? Implementation->comparisons(subject->Authority) : nullptr;
		return Implementation->submit<cycle_write_receipt>([subject, host, aOperation, aItem, aExpectedVersion, change = std::move(aChange)](review_cycle_store& aStore, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> {
			if (!subject) {
				return std::unexpected(refused_selection());
			}
			const auto target  = core::cycle_feedback_target{ change.Selection };
			const auto targets = std::array{ &target };
			if (auto verified = verify_feedback_targets(*host, *subject, targets, aStop); !verified) {
				return std::unexpected(verified.error());
			}
			return aStore.add_suggestion(aOperation, *subject, aItem, aExpectedVersion, change, aStop);
		},
														   std::move(aDone));
	}

	auto review_cycle_runtime::change_suggestion_selected(cycle_operation_id aOperation, core::cycle_suggestion_ref aSuggestion, const std::uint64_t aExpectedRevision,
														  core::cycle_suggestion_change aChange, completion<cycle_write_receipt> aDone) -> bool
	{
		const auto subject = Implementation->selected_cycle();
		auto       host    = subject ? Implementation->comparisons(subject->Authority) : nullptr;
		return Implementation->submit<cycle_write_receipt>([subject, host, aOperation, aSuggestion, aExpectedRevision, change = std::move(aChange)](review_cycle_store& aStore, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> {
			if (!subject) {
				return std::unexpected(refused_selection());
			}
			if (const auto* revalidate = std::get_if<core::revalidate_suggestion>(&change)) {
				const auto target  = core::cycle_feedback_target{ revalidate->Candidate };
				const auto targets = std::array{ &target };
				if (auto verified = verify_feedback_targets(*host, *subject, targets, aStop); !verified) {
					return std::unexpected(verified.error());
				}
			}
			if (const auto* applied = std::get_if<core::observe_suggestion_applied>(&change)) {
				const auto current = aStore.suggestions(*subject, aSuggestion.Item, aStop);
				if (!current) {
					return std::unexpected(current.error());
				}
				const auto found = std::ranges::find(*current, aSuggestion, &core::cycle_suggestion::Id);
				if (found == current->end()) {
					return std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::not_found });
				}
				if (auto confirmed = confirm_suggestion_applied(*host, *subject, *found, applied->Result, aStop); !confirmed) {
					return std::unexpected(confirmed.error());
				}
			}
			return aStore.change_suggestion(aOperation, *subject, aSuggestion, aExpectedRevision, change, aStop);
		},
														   std::move(aDone));
	}

	auto review_cycle_runtime::suggestions_selected(core::cycle_feedback_item_ref aItem, completion<std::vector<core::cycle_suggestion>> aDone) -> bool
	{
		const auto subject = Implementation->selected_cycle();
		return Implementation->submit<std::vector<core::cycle_suggestion>>([subject, aItem](review_cycle_store& aStore, std::stop_token aStop) -> cycle_store_result<std::vector<core::cycle_suggestion>> {
			if (!subject) {
				return std::unexpected(refused_selection());
			}
			return aStore.suggestions(*subject, aItem, aStop);
		},
																		   std::move(aDone));
	}

	auto review_cycle_runtime::preview_suggestion_selected(core::cycle_suggestion aSuggestion, completion<cycle_suggestion_preview_observation> aDone) -> bool
	{
		const auto subject = Implementation->selected_cycle();
		auto       host    = subject ? Implementation->comparisons(subject->Authority) : nullptr;
		return Implementation->submit<cycle_suggestion_preview_observation>([subject, host, suggestion = std::move(aSuggestion)](review_cycle_store&, std::stop_token aStop) -> cycle_store_result<cycle_suggestion_preview_observation> {
			if (!subject) {
				return std::unexpected(refused_selection());
			}
			return preview_cycle_suggestion(*host, *subject, suggestion, std::move(aStop));
		},
																			std::move(aDone));
	}

	auto review_cycle_runtime::preflight_suggestions_selected(std::vector<core::cycle_suggestion> aSuggestions, std::optional<core::cycle_source_revision_id> aAgainst,
															  completion<cycle_suggestion_preflight_observation> aDone) -> bool
	{
		const auto subject = Implementation->selected_cycle();
		auto       host    = subject ? Implementation->comparisons(subject->Authority) : nullptr;
		return Implementation->submit<cycle_suggestion_preflight_observation>([subject, host, suggestions = std::move(aSuggestions), aAgainst](review_cycle_store&, std::stop_token aStop) -> cycle_store_result<cycle_suggestion_preflight_observation> {
			if (!subject) {
				return std::unexpected(refused_selection());
			}
			return preflight_cycle_suggestions(*host, *subject, suggestions, aAgainst, std::move(aStop));
		},
																			  std::move(aDone));
	}

	auto review_cycle_runtime::observe_workspace(workspace_source_port& aSource, std::string aLocation, completion<workspace_root_observation_result> aDone) -> bool
	{
		return Implementation->submit<workspace_root_observation_result>([source = &aSource, location = std::move(aLocation)](review_cycle_store& aStore, std::stop_token aStop) -> cycle_store_result<workspace_root_observation_result> {
			auto       host     = review_cycle_capture_host{ *source, aStore };
			const auto observed = host.observe(location, std::move(aStop));
			if (!observed) {
				return workspace_root_observation_result{ .Failure = observed.error() };
			}
			return workspace_root_observation_result{ .Root = *observed };
		},
																		 std::move(aDone));
	}

	auto review_cycle_runtime::list(std::optional<core::review_cycle_id> aAfter, completion<cycle_page> aDone) -> bool
	{
		return Implementation->submit<cycle_page>([aAfter](review_cycle_store& aStore, std::stop_token aStop) { return aStore.list(aAfter, aStop); }, std::move(aDone));
	}

	auto review_cycle_runtime::load(core::local_cycle_subject aSubject, completion<std::optional<core::review_cycle_state>> aDone) -> bool
	{
		return Implementation->submit<std::optional<core::review_cycle_state>>([aSubject](review_cycle_store& aStore, std::stop_token aStop) { return aStore.load(aSubject, aStop); },
																			   std::move(aDone));
	}

	auto review_cycle_runtime::apply_selected(cycle_operation_id aOperation, const std::uint64_t aExpectedRevision, core::review_cycle_change aChange, completion<cycle_write_receipt> aDone)
		-> bool
	{
		const auto subject = Implementation->selected_cycle();
		return Implementation->submit<cycle_write_receipt>([subject, aOperation, aExpectedRevision, change = std::move(aChange)](review_cycle_store& aStore, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> {
			if (!subject) {
				return std::unexpected(refused_selection());
			}
			return aStore.apply(aOperation, { .Subject = *subject, .ExpectedRevision = aExpectedRevision, .Change = change }, aStop);
		},
														   std::move(aDone));
	}

	auto review_cycle_runtime::revisions_selected(std::optional<core::cycle_source_revision_id> aAfter, completion<cycle_revision_page> aDone) -> bool
	{
		const auto subject = Implementation->selected_cycle();
		return Implementation->submit<cycle_revision_page>([subject, aAfter](review_cycle_store& aStore, std::stop_token aStop) -> cycle_store_result<cycle_revision_page> {
			if (!subject) {
				return std::unexpected(refused_selection());
			}
			return aStore.revisions(*subject, aAfter, aStop);
		},
														   std::move(aDone));
	}

	auto review_cycle_runtime::save_tabs_selected(core::cycle_tabs_record aRecord, completion<void> aDone) -> bool
	{
		const auto subject = Implementation->selected_cycle();
		return Implementation->submit<void>([subject, record = std::move(aRecord)](review_cycle_store& aStore, std::stop_token aStop) -> cycle_store_result<void> {
			if (!subject) {
				return std::unexpected(refused_selection());
			}
			return aStore.save_tabs(*subject, record, aStop);
		},
											std::move(aDone));
	}

	auto review_cycle_runtime::load_tabs_selected(completion<std::optional<core::cycle_tabs_record>> aDone) -> bool
	{
		const auto subject = Implementation->selected_cycle();
		return Implementation->submit<std::optional<core::cycle_tabs_record>>([subject](review_cycle_store& aStore, std::stop_token aStop) -> cycle_store_result<std::optional<core::cycle_tabs_record>> {
			if (!subject) {
				return std::unexpected(refused_selection());
			}
			return aStore.load_tabs(*subject, aStop);
		},
																			  std::move(aDone));
	}

	void review_cycle_runtime::shutdown()
	{
		auto& self   = *Implementation;
		self.Closing = true;
		self.Stop.request_stop();
	}

	auto review_cycle_runtime::is_quiescent() const noexcept -> bool
	{
		return Implementation->Outstanding == 0;
	}

	review_material_bridge::review_material_bridge(diff_blob_port* aPullRequests, review_cycle_store* aCycles, core::local_cycle_authority aCycleAuthority) noexcept
		: PullRequests{ aPullRequests }
		, Cycles{ aCycles }
		, CycleAuthority{ aCycleAuthority }
	{
	}

	namespace {
		auto from_blob(const diff_blob_status aStatus) -> review_material_status
		{
			switch (aStatus) {
			case diff_blob_status::available:
			case diff_blob_status::binary:
				return review_material_status::available;
			case diff_blob_status::missing:
				return review_material_status::missing;
			case diff_blob_status::materialization_limit:
				return review_material_status::too_large;
			case diff_blob_status::cancelled:
				return review_material_status::cancelled;
			case diff_blob_status::decode_failure:
			case diff_blob_status::generated_file_unsupported:
			case diff_blob_status::failure:
				return review_material_status::failure;
			}
			return review_material_status::failure;
		}

		auto from_store(const cycle_store_failure aFailure) -> review_material_status
		{
			switch (aFailure) {
			case cycle_store_failure::not_found:
			case cycle_store_failure::object_unavailable:
				return review_material_status::missing;
			case cycle_store_failure::refused:
			case cycle_store_failure::wrong_profile:
				return review_material_status::wrong_authority;
			case cycle_store_failure::corrupt:
				return review_material_status::corrupt;
			case cycle_store_failure::cancelled:
				return review_material_status::cancelled;
			case cycle_store_failure::bounds:
				return review_material_status::too_large;
			default:
				return review_material_status::failure;
			}
		}
	}

	auto review_material_bridge::materialize(const review_material_request& aRequest, std::stop_token aStop) -> review_material_observation
	{
		if (const auto* blob = std::get_if<diff_blob_request>(&aRequest)) {
			if (PullRequests == nullptr) {
				return { .Status = review_material_status::wrong_authority };
			}
			auto       observation = PullRequests->materialize(*blob, std::move(aStop));
			const auto status      = from_blob(observation.Status);
			return { .Status = status, .Bytes = status == review_material_status::available ? std::move(observation.Bytes) : std::vector<std::uint8_t>{ } };
		}
		const auto& request = std::get<cycle_material_request>(aRequest);
		if (Cycles == nullptr || request.Subject.Authority != CycleAuthority || !core::valid(request.Subject) || !core::valid_cycle_id(request.Revision)) {
			return { .Status = review_material_status::wrong_authority };
		}
		if (request.Object.Size > request.MaximumBytes) {
			return { .Status = review_material_status::too_large };
		}
		// Only an object the named revision pinned: its manifest or content the
		// manifest references. Never a live file, never another revision's.
		const auto holds = Cycles->revision_holds(request.Subject, request.Revision, request.Object, aStop);
		if (!holds) {
			return { .Status = from_store(holds.error().Failure) };
		}
		if (!*holds) {
			return { .Status = review_material_status::wrong_authority };
		}
		auto bytes = Cycles->read_object(request.Object, std::move(aStop));
		if (!bytes) {
			return { .Status = from_store(bytes.error().Failure) };
		}
		return { .Status = review_material_status::available, .Bytes = std::move(*bytes) };
	}
}
