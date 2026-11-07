#include "github_application/device_authorization_composition.hpp"

#include "bpr/core/model.hpp"

#include <variant>

#if defined(BPR_BUILD_TESTING)
	#include <algorithm>
#endif

namespace bpr::app {
#if defined(BPR_BUILD_TESTING)
	counting_device_authorization_enrollment::counting_device_authorization_enrollment(
		application::device_authorization_enrollment_port& aInner) noexcept
		: Inner{ &aInner }
	{
	}

	void counting_device_authorization_enrollment::enroll(
		const core::host_profile_id             aProfile,
		application::github_credential_material aCredential)
	{
		++Enrollments;
		Inner->enroll(aProfile, std::move(aCredential));
	}

	void counting_device_authorization_enrollment::enroll_grant(core::host_profile_id aProfile, application::github_credential_material aCredential, application::device_binding_lifetime aLifetime, bool aSettled)
	{
		++Enrollments;
		Inner->enroll_grant(aProfile, std::move(aCredential), aLifetime, aSettled);
	}

	void counting_device_authorization_enrollment::settle_binding(application::github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, application::device_binding_lifetime aLifetime)
	{
		Inner->settle_binding(std::move(aAuthority), aGeneration, aLifetime);
	}

	void counting_device_authorization_enrollment::prepare_refresh(application::github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, std::function<void(bool)> aCompletion)
	{
		Inner->prepare_refresh(std::move(aAuthority), aGeneration, std::move(aCompletion));
	}

	void counting_device_authorization_enrollment::invalidate_grant(application::github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, application::device_authorization_refresh_failure aFailure) { Inner->invalidate_grant(std::move(aAuthority), aGeneration, aFailure); }

	auto counting_device_authorization_enrollment::enrollments() const noexcept -> std::size_t
	{
		return Enrollments;
	}
#endif
	setup_device_enrollment::setup_device_enrollment(application::device_authorization_enrollment_port& aInner, enrollment aEnroll)
		: Inner{ aInner }
		, Enroll{ std::move(aEnroll) }
	{ }

	void setup_device_enrollment::enroll(core::host_profile_id aProfile, application::github_credential_material aCredential) { Enroll(aProfile, std::move(aCredential), std::nullopt, false); }

	void setup_device_enrollment::enroll_grant(core::host_profile_id aProfile, application::github_credential_material aCredential, application::device_binding_lifetime aLifetime, bool aSettled) { Enroll(aProfile, std::move(aCredential), aLifetime, aSettled); }

	void setup_device_enrollment::settle_binding(application::github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, application::device_binding_lifetime aLifetime) { Inner.settle_binding(std::move(aAuthority), aGeneration, aLifetime); }

	void setup_device_enrollment::prepare_refresh(application::github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, std::function<void(bool)> aCompletion) { Inner.prepare_refresh(std::move(aAuthority), aGeneration, std::move(aCompletion)); }

	void setup_device_enrollment::invalidate_grant(application::github_credential_authority_key aAuthority, core::github_identity_generation aGeneration, application::device_authorization_refresh_failure aFailure) { Inner.invalidate_grant(std::move(aAuthority), aGeneration, aFailure); }

	void device_authorization_projection_sink::publish(
		const application::device_authorization_presentation& aPresentation)

	{
		Last = aPresentation;
		++Published;
		// The host clears the handoff on the next stage change, so a
		// reader that arrives after the flow moved on sees
		// `not_attempted` and cannot tell an opener that answered
		// from one that was never asked. This keeps the answer
		// instead of the moment it was true: what a self-test needs
		// at shutdown is which fate the last attempt met, and only a
		// new attempt or a new run replaces it.
		if (
			aPresentation.Handoff
			!= application::device_authorization_handoff::not_attempted) {
			LastAttempt = aPresentation.Handoff;
		}
	}

	void device_authorization_projection_sink::publish_refresh(
		const application::device_authorization_refresh_observation&
			aObservation)
	{
		LastRefresh = aObservation;
		++RefreshPublished;
	}

	auto device_authorization_projection_sink::last() const noexcept
		-> const application::device_authorization_presentation&
	{
		return Last;
	}

	auto device_authorization_projection_sink::last_refresh() const noexcept
		-> const application::device_authorization_refresh_observation&
	{
		return LastRefresh;
	}

	auto device_authorization_projection_sink::refresh_published() const noexcept -> std::size_t
	{
		return RefreshPublished;
	}

	auto device_authorization_projection_sink::last_attempt() const noexcept
		-> application::device_authorization_handoff
	{
		return LastAttempt;
	}

	auto device_authorization_projection_sink::published() const noexcept -> std::size_t
	{
		return Published;
	}

	composition_device_authorization_projection::composition_device_authorization_projection(
		application::device_authorization_projection_port& aRecorded,
		presented_sink                                     aPresented,
		renewed_sink                                       aRenewed)
		: Recorded{ &aRecorded }
		, Presented{ std::move(aPresented) }
		, Renewed{ std::move(aRenewed) }
	{
	}

	void composition_device_authorization_projection::publish(
		const application::device_authorization_presentation& aPresentation)

	{
		Recorded->publish(aPresentation);
		if (Presented) {
			Presented(aPresentation);
		}
	}

	void composition_device_authorization_projection::publish_refresh(
		const application::device_authorization_refresh_observation&
			aObservation)
	{
		Recorded->publish_refresh(aObservation);
		if (Renewed) {
			Renewed(aObservation);
		}
	}
#if defined(BPR_BUILD_TESTING)
	auto recording_device_authorization_browser::open(const std::string_view aAddress)
		-> bool
	{
		LastAddress.assign(aAddress);
		++Opens;
		return true;
	}

	auto recording_device_authorization_browser::opens() const noexcept -> std::size_t
	{
		return Opens;
	}

	auto recording_device_authorization_browser::last_address() const noexcept
		-> const std::string&
	{
		return LastAddress;
	}
#endif
	void unregistered_device_authorization_transport::acquire_device_code(
		core::device_authorization_request_context  aContext,
		application::device_code_completion_handler aCompletion)
	{
		++Reached;
		aCompletion(application::device_authorization_transport_error{
			.Context = aContext,
			.Failure = application::device_authorization_transport_failure::
				network,
		});
	}

	void unregistered_device_authorization_transport::exchange_device_token(
		application::device_token_exchange           aExchange,
		application::device_token_completion_handler aCompletion)
	{
		refuse(aExchange, std::move(aCompletion));
	}

	void unregistered_device_authorization_transport::refresh_access_token(
		application::device_token_exchange           aExchange,
		application::device_token_completion_handler aCompletion)
	{
		refuse(aExchange, std::move(aCompletion));
	}

	void unregistered_device_authorization_transport::cancel(core::device_authorization_request_context)
	{
	}

	auto unregistered_device_authorization_transport::reached() const noexcept -> std::size_t
	{
		return Reached;
	}

	void unregistered_device_authorization_transport::refuse(
		const application::device_token_exchange&    aExchange,
		application::device_token_completion_handler aCompletion)
	{
		++Reached;
		aCompletion(application::device_authorization_transport_error{
			.Context = aExchange.Context,
			.Failure = application::device_authorization_transport_failure::
				network,
		});
	}

	auto initial_device_authorization_state()
		-> core::device_authorization_state
	{
		return {
			.ApplicationSession = core::application_session_id{ 1U },
			.NextRequest        = core::request_id{ 1U },
		};
	}
#if defined(BPR_BUILD_TESTING)
	composition_device_refresh_clock::composition_device_refresh_clock(
		std::unique_ptr<application::device_authorization_clock_port> aInner,
		std::function<std::size_t()>                                  aArmings,
		const std::chrono::milliseconds                               aOffset,
		const std::size_t                                             aSteps)
		: Inner{ std::move(aInner) }
		, Armings{ std::move(aArmings) }
		, Offset{ aOffset }
		, Steps{ aSteps }
	{
	}

	auto composition_device_refresh_clock::observe_wall_seconds() noexcept -> std::int64_t { return Inner->observe_wall_seconds(); }

	auto composition_device_refresh_clock::observe_monotonic() noexcept
		-> std::chrono::milliseconds
	{
		const auto observed = Inner->observe_monotonic();
		if (Armings) {
			// Never fewer steps than are already applied: the reading owes
			// its monotonicity to this clock, not to what it counts.
			Applied = std::min(std::max(Applied, Armings()), Steps);
		}
		return observed
			+ Offset * static_cast<std::chrono::milliseconds::rep>(Applied);
	}

	composition_device_wall_clock::composition_device_wall_clock(std::unique_ptr<application::device_authorization_clock_port> aInner, std::int64_t aOffset)
		: Inner{ std::move(aInner) }
		, Offset{ aOffset }
	{ }

	auto composition_device_wall_clock::observe_monotonic() noexcept -> std::chrono::milliseconds { return Inner->observe_monotonic(); }

	auto composition_device_wall_clock::observe_wall_seconds() noexcept -> std::int64_t { return Inner->observe_wall_seconds() + Offset; }
#endif
}
