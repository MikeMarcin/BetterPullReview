#include "bpr/application/review_cycle_runtime.hpp"

#include <deque>
#include <utility>

namespace bpr::application {
	namespace {
		auto cancelled() -> cycle_store_error
		{
			return cycle_store_error{ .Failure = cycle_store_failure::cancelled };
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
		effect_executor&                    Effects;
		decision_executor&                  Decisions;
		std::optional<core::review_subject> Selected;
		std::uint64_t                       Generation = 0;
		std::stop_source                    Stop;
		std::deque<std::function<void()>>   Queue;
		std::size_t                         Outstanding = 0;
		bool                                Running     = false;
		bool                                Closing     = false;

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
		const auto before = request.Revision.value() == 1U ? std::nullopt : std::optional{ core::cycle_source_revision_id{ request.Revision.value() - 1U } };
		const auto page   = Cycles->revisions(request.Subject, before, aStop);
		if (!page) {
			return { .Status = from_store(page.error().Failure) };
		}
		if (page->Revisions.empty() || page->Revisions.front().Revision.Id.Id != request.Revision) {
			return { .Status = review_material_status::missing };
		}
		if (page->Revisions.front().Manifest != request.Object) {
			return { .Status = review_material_status::wrong_authority };
		}
		auto bytes = Cycles->read_object(request.Object, std::move(aStop));
		if (!bytes) {
			return { .Status = from_store(bytes.error().Failure) };
		}
		return { .Status = review_material_status::available, .Bytes = std::move(*bytes) };
	}
}
