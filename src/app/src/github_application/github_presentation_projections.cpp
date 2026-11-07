#include "github_application/github_presentation_projections.hpp"

#include "bpr/core/model.hpp"

#include <optional>

namespace bpr::app {
	composition_github_projection::composition_github_projection(
		application::github_projection_port& aPresented,
		accepted_sink                        aAccepted)
		: Presented{ &aPresented }
		, Accepted{ std::move(aAccepted) }
	{
	}

	void composition_github_projection::publish(
		const application::github_presentation_update& aUpdate)
	{
		Presented->publish(aUpdate);
		if (Accepted) {
			Accepted(aUpdate);
		}
	}

	void public_preview_projection_sink::publish(
		const application::public_preview_presentation& aPresentation)

	{
		Last = aPresentation;
		++Published;
	}

	auto public_preview_projection_sink::last() const noexcept
		-> const application::public_preview_presentation&
	{
		return Last;
	}

	auto public_preview_projection_sink::published() const noexcept -> std::size_t
	{
		return Published;
	}

	composition_public_preview_projection::composition_public_preview_projection(
		application::public_preview_projection_port& aRecorded,
		presented_sink                               aPresented)
		: Recorded{ &aRecorded }
		, Presented{ std::move(aPresented) }
	{
	}

	void composition_public_preview_projection::publish(
		const application::public_preview_presentation& aPresentation)

	{
		Recorded->publish(aPresentation);
		if (Presented) {
			Presented(aPresentation);
		}
	}

	auto initial_github_state() -> core::github_state
	{
		return {
			.ApplicationSession    = core::application_session_id{ 1U },
			.NextRequest           = core::request_id{ 1U },
			.ActiveProfile         = std::nullopt,
			.IdentityGeneration    = { },
			.PendingSession        = std::nullopt,
			.Host                  = std::nullopt,
			.Account               = std::nullopt,
			.Capabilities          = { },
			.CapabilityRevision    = { },
			.PullRequestGeneration = { },
			.PendingPullRequest    = std::nullopt,
			.ActiveLocator         = std::nullopt,
			.ActiveNumber          = std::nullopt,
			.Repository            = std::nullopt,
			.PullRequest           = std::nullopt,
			.AcceptedSnapshot      = std::nullopt,
		};
	}
}
