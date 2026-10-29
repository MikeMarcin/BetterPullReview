#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_IDENTITY_AVATAR_WINDOW_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_IDENTITY_AVATAR_WINDOW_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/application/markdown_image_service.hpp"
#include "bpr/application/markdown_image_window.hpp"
#include "bpr/application/port.hpp"
#include "bpr/core/identity_avatar_picture.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace bpr::application {
	/// \brief [[BPR-1807]]. One window's people's pictures: admission against
	/// the accepted profile, loads through the profile's image lane, and the
	/// projections they publish.
	///
	/// \details Every member function and completion runs on the decision lane.
	/// The facts are the window's body-image facts: the same profile, account,
	/// credential and scope. A picture is admitted by `core::admit_identity_avatar`
	/// and fetched, decoded, cached in memory and on disk by the profile's
	/// `markdown_image_service`, so it shares that lane's bounds, its
	/// concurrency and its caches. A changed scope cancels everything earlier;
	/// results are published only while their scope is still the current one.
	/// A request asked again while it waits joins it. Without an authenticated
	/// window every request is `failed` at once, so the initials stay.
	class identity_avatar_window
	{
	public:
		using facts_source = std::function<markdown_image_window_facts()>;
		using publication  = std::function<void(std::vector<identity_avatar_projection>)>;

		identity_avatar_window(decision_executor& aDecision, markdown_image_service* aService, facts_source aFacts, publication aPublish);
		~identity_avatar_window();
		identity_avatar_window(const identity_avatar_window&)                    = delete;
		auto operator=(const identity_avatar_window&) -> identity_avatar_window& = delete;

		/// \brief Admits the pictures a presentation asked for and starts what they need.
		void request(std::span<const core::identity_avatar_intent> aIntents);
		/// \brief Cancels every waiting picture.
		void reset();
		/// \brief Resets and refuses further requests.
		void               shutdown();
		[[nodiscard]] auto quiescent() const noexcept -> bool;
		[[nodiscard]] auto pending() const noexcept -> std::size_t;

	private:
		struct state;
		std::shared_ptr<state> State;
	};
} // namespace bpr::application

#endif
