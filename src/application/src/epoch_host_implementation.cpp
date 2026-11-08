#include "epoch_host_implementation.hpp"

#include "bpr/core/diff_transition.hpp"

namespace bpr::application {
	namespace {
		inline constexpr std::size_t maximum_pending_preparations = 100U;
	}

	epoch_application_host::implementation::authority_effect_actor::authority_effect_actor(std::shared_ptr<profile_epoch_actor> aShared)
		: Shared{ std::move(aShared) }
		, Cache{ Shared->cache() }
	{ }

	epoch_application_host::implementation::reviewer_work::reviewer_work(std::shared_ptr<authority_effect_actor> aActor)
		: Actor{ std::move(aActor) }
	{
		std::scoped_lock lock{ Actor->BlobAuthorityMutex };
		++Actor->ReviewerWork;
		Actor->SearchScanStop.request_stop();
		if (Actor->SearchRead) {
			Actor->SearchRead->Stop.request_stop();
		}
		if (Actor->PreparedRead) {
			static_cast<void>(Actor->PreparedRead->request_stop());
		}
	}

	epoch_application_host::implementation::reviewer_work::~reviewer_work()
	{
		std::scoped_lock lock{ Actor->BlobAuthorityMutex };
		--Actor->ReviewerWork;
		if (Actor->ReviewerWork == 0U) {
			Actor->SearchScanStop = std::stop_source{ };
		}
	}

	epoch_application_host::implementation::prepared_read_registration::prepared_read_registration(authority_effect_actor& aActor, const std::stop_source& aStop, const bool aHonorSuspension)
		: Actor{ aActor }
	{
		std::scoped_lock lock{ Actor.BlobAuthorityMutex };
		Suspended = aHonorSuspension && Actor.PreparedReadsSuspended;
		Admitted  = !Suspended && Actor.ReviewerWork == 0U;
		if (Admitted) {
			Actor.PreparedRead = aStop;
		}
	}

	epoch_application_host::implementation::prepared_read_registration::~prepared_read_registration()
	{
		if (Admitted) {
			std::scoped_lock lock{ Actor.BlobAuthorityMutex };
			Actor.PreparedRead.reset();
		}
	}

	auto epoch_application_host::implementation::prepared_read_registration::admitted() const noexcept -> bool
	{
		return Admitted;
	}

	auto epoch_application_host::implementation::prepared_read_registration::suspended() const noexcept -> bool
	{
		return Suspended;
	}

	epoch_application_host::implementation::actor_blob_read::actor_blob_read(const diff_blob_request& aRequest)
		: Request{ aRequest }
	{
		Observation.Context = aRequest.Context;
		Observation.Side    = aRequest.Side;
		Observation.Blob    = aRequest.Blob;
		Observation.Status  = diff_blob_status::failure;
	}

	epoch_application_host::implementation::authority_context::authority_context(
		accepted_pull_request_handoff                 aAccepted,
		core::pull_request_authority                  aAuthority,
		core::accepted_pull_request_revision          aRevision,
		std::unique_ptr<epoch_catalog_session>        aCatalog,
		std::shared_ptr<authority_effect_actor>       aActor,
		std::optional<core::epoch_occurrence_history> aOccurrenceHistory,
		std::optional<core::review_epoch_summary>     aCurrentEpoch)
		: Handoff{ std::move(aAccepted) }
		, Authority{ std::move(aAuthority) }
		, Revision{ std::move(aRevision) }
		, Catalog{ std::move(aCatalog) }
		, Actor{ std::move(aActor) }
		, OccurrenceHistory{ std::move(aOccurrenceHistory) }
		, CurrentEpoch{ std::move(aCurrentEpoch) }
	{
		Preparations.reserve(maximum_pending_preparations);
	}

	epoch_application_host::implementation::authority_context::~authority_context()
	{
		if (Handoff.SmartGit && !Handoff.SmartGit->revoked()) {
			Handoff.SmartGit->revoke();
		}
	}

	auto epoch_application_host::implementation::authority_context::store(prepared_operation aOperation) -> bool
	{
		if (
			Preparations.size() >= maximum_pending_preparations
			|| std::ranges::any_of(
				Preparations,
				[&](const auto& aExisting) {
					return aExisting.Operation == aOperation.Operation;
				})) {
			return false;
		}
		Preparations.push_back(std::move(aOperation));
		return true;
	}

	auto epoch_application_host::implementation::authority_context::take(const core::epoch_operation_id aOperation)
		-> std::optional<prepared_operation>
	{
		const auto found = std::ranges::find(
			Preparations,
			aOperation,
			&prepared_operation::Operation);
		if (found == Preparations.end()) {
			return std::nullopt;
		}
		auto result = std::move(*found);
		Preparations.erase(found);
		return result;
	}

	auto epoch_application_host::implementation::authority_context::requires_cache_cleanup(
		const std::optional<core::epoch_operation_id> aExpected)
		const noexcept -> bool
	{
		return std::ranges::any_of(
			Preparations,
			[&](const auto& aOperation) {
				return aOperation.Kind == prepared_kind::append
					&& (!aExpected || aOperation.Operation == *aExpected);
			});
	}
}
