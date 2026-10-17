#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_WINDOW_READ_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_WINDOW_READ_HOST_HPP

#include "bpr/application/body_digest_port.hpp"
#include "bpr/application/cached_review_overview_port.hpp"
#include "bpr/application/cached_review_source_port.hpp"
#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/github_epoch_handoff.hpp"
#include "bpr/application/github_projection.hpp"
#include "bpr/application/github_provider_port.hpp"
#include "bpr/application/markdown_image_window.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/public_preview_projection.hpp"
#include "bpr/application/review_mutation_port.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/github_window_read.hpp"

#include <functional>
#include <memory>
#include <span>

namespace bpr::application {
	class device_authorization_browser_port;
	class github_read_admission;
	enum class github_public_preview_refusal_cause : std::uint8_t {
		/// No anonymous read authority is composed, so the mode cannot be
		/// entered at all. This is a composition fact the host reports
		/// rather than a policy the core decided.
		public_preview_unavailable,
		/// The core policy refused. `Policy` carries its typed refusal.
		policy_refused,
	};

	/// \brief Describes a refused public-preview activation or read.
	///
	/// Sign-in is offered from every refusal, because `core::anonymous_refusal`
	/// makes that a property of the type rather than a flag a caller sets.
	struct github_public_preview_refusal
	{
		github_public_preview_refusal_cause Cause
			= github_public_preview_refusal_cause::public_preview_unavailable;
		std::optional<core::anonymous_refusal> Policy;

		auto operator<=>(const github_public_preview_refusal&) const = default;
	};

	/// Trusted observation published only after the profile owner accepts identity.
	struct github_profile_session
	{
		core::bpr_profile_id             BprProfile;
		core::application_session_id     ApplicationSession;
		github_profile_authority         ProfileAuthority;
		github_session_read_model        Session;
		core::github_identity_generation Generation;
		github_capability_cache_entry    CapabilityEvidence;
		github_account_transport         AccountTransport;
		auto                             operator==(const github_profile_session&) const -> bool = default;
	};

	struct github_window_read_update
	{
		core::bpr_window_context                     Window;
		core::github_transition_output               Transition;
		std::optional<github_provider_error>         ProviderError;
		std::optional<github_public_preview_refusal> PublicPreviewRefusal;
		/// Provider-free presentation mapped inside the window host publication boundary.
		github_presentation_update Presentation;
	};

	/// \brief [[BPR-1338]]. The canonical address of an admitted pull request page:
	/// HTTPS on the admitted hostname, then owner, repository, "pull" and number,
	/// exactly what the provider contract requires a pull request's `html_url` to be.
	[[nodiscard]] auto pull_request_page_address(const core::pull_request_page& aPage) -> std::string;

	/// Owns one PR request, accepted snapshot and its revocable epoch authorities.
	/// [[BPR-1447]]. With \p aBodyDigests, a fetched snapshot's feedback bodies
	/// are digested on the provider's completion thread, before the snapshot
	/// is posted to the decision lane; the port must outlive the host.
	/// All commands and publications run on the decision lane. Provider terminals
	/// post to that lane; ports and lane remain alive until quiescent.
	class github_window_read_host
	{
	public:
		using publication = std::function<void(const github_window_read_update&)>;
		github_window_read_host(core::bpr_window_context     aWindow,
								core::application_session_id aSession, github_pull_request_port& aReads,
								decision_executor& aDecision, publication aPublication,
								github_epoch_handoff_port*             aEpochs          = nullptr,
								github_pull_request_port*              aAnonymousReads  = nullptr,
								git_smart_transport_factory_port*      aAnonymousGit    = nullptr,
								diagnostic_log_port*                   aLog             = nullptr,
								cached_review_overview_port*           aOverviews       = nullptr,
								effect_executor*                       aCacheEffects    = nullptr,
								cached_review_source_port*             aSources         = nullptr,
								std::shared_ptr<github_read_admission> aAdmission       = nullptr,
								const body_digest_port*                aBodyDigests     = nullptr,
								markdown_image_service*                aImages          = nullptr,
								markdown_image_resolution_port*        aImageResolution = nullptr);
		~github_window_read_host();
		github_window_read_host(const github_window_read_host&)                                  = delete;
		auto               operator=(const github_window_read_host&) -> github_window_read_host& = delete;
		[[nodiscard]] auto adopt(const github_profile_session& aSession) -> bool;
		[[nodiscard]] auto retire(const github_profile_session& aSession) -> bool;
		/// \brief The adopted account session, as this window would have presented its acceptance.
		/// \details [[BPR-1017]]. An account session's acceptance is published when it
		/// happens, so a window attached afterwards never presented it. This is that
		/// presentation: the adoption's transition and the accepted session, with no pull
		/// request, read or credential behind it. Empty before adoption, once this window
		/// has moved on from it, and for a public-preview session, whose presentation is
		/// the profile host's.
		[[nodiscard]] auto adopted_presentation() const -> std::optional<github_presentation_update>;
		void               advance_request_floor(core::request_id aNext) noexcept;
		/// \brief Opens, rereads or closes this window's pull request.
		/// \details [[BPR-1573]]. A close the core accepts stops any read in
		/// flight and invalidates the epoch authority, the release opening
		/// another pull request makes; a refused close changes nothing.
		void dispatch(const core::github_window_read_event& aEvent);
		/// Share one immutable accepted server observation with bounded sibling
		/// readers. Does not fetch, move comparisons, or hand off another epoch.
		void share_snapshot(std::span<const std::shared_ptr<github_window_read_host>> aWindows, std::uint64_t aSequence);
		/// \brief Revalidates a displayed body gesture on the decision lane before the browser handoff.
		[[nodiscard]] auto open_markdown_link(const core::markdown_link_intent& aIntent, device_authorization_browser_port* aBrowser) -> core::markdown_link_result;
		/// \brief [[BPR-1338]]. Opens the accepted pull request's own page for a gesture against
		/// this window's current publication. The address is built from the accepted profile
		/// authority's hostname and the accepted snapshot, never from remote or typed text.
		[[nodiscard]] auto open_pull_request_page(const core::markdown_context& aContext, device_authorization_browser_port* aBrowser) -> core::markdown_link_result;
		void               read_cached_source(std::string aPath);
		/// \brief RFC-BPR-0084. Admits a rendered body's image references against the
		/// current publication and starts their resolution and loads; results arrive
		/// as `github_presentation_update::Images`.
		void               request_markdown_images(const core::markdown_image_intent& aIntent);
		void               cancel();
		void               shutdown();
		[[nodiscard]] auto state() const noexcept -> const core::github_state&;
		[[nodiscard]] auto window() const noexcept -> core::bpr_window_context;
		[[nodiscard]] auto accepted_snapshot() const noexcept -> const std::optional<github_pull_request_snapshot>&;
		[[nodiscard]] auto mutation_transport() const noexcept -> review_mutation_transport;
		[[nodiscard]] auto rate_limits() const noexcept -> const github_rate_limit_state&;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;
		[[nodiscard]] auto epoch_handoff_delivery_failed() const noexcept -> bool;
		[[nodiscard]] auto epoch_handoff_unavailable() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
}
#endif
