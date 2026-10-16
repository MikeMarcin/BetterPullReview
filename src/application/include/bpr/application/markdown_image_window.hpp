#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MARKDOWN_IMAGE_WINDOW_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_MARKDOWN_IMAGE_WINDOW_HPP

#include "bpr/application/markdown_image_ports.hpp"
#include "bpr/application/markdown_image_service.hpp"
#include "bpr/application/port.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::application {
	/// \brief The accepted facts one window's image requests are judged by.
	struct markdown_image_window_facts
	{
		std::optional<core::markdown_context> Context;
		/// Changes whenever images of one scope must not reach another: the
		/// window, session, identity generation, account and pull request.
		std::string                       Scope;
		bool                              Available = false;
		core::markdown_image_fetch_policy Policy;
		std::uint64_t                     Account = 0U;
		/// The resolution request without its nodes and sequence.
		std::optional<markdown_image_resolution_request> Resolution;
	};

	/// \brief RFC-BPR-0084. One window's image requests: admission, resolution
	/// through GitHub's rendering, loads through the profile service and the
	/// projections they publish.
	///
	/// \details Every member function and completion runs on the decision lane.
	/// A body's new request replaces its earlier one; a changed scope cancels
	/// everything earlier. Results are published only while their scope is
	/// still the current one. At most `core::markdown_image_window_queue`
	/// references wait at once; later ones are `over_limit`.
	class markdown_image_window
	{
	public:
		using facts_source = std::function<markdown_image_window_facts()>;
		/// \brief A body identity's GraphQL node: empty for a current body
		/// without one, absent for a body that is not current.
		using node_source = std::function<std::optional<std::string>(std::string_view)>;
		using publication = std::function<void(std::vector<markdown_image_projection>)>;

		markdown_image_window(decision_executor& aDecision, markdown_image_service* aService,
							  markdown_image_resolution_port* aResolution, facts_source aFacts, node_source aNodes, publication aPublish);
		~markdown_image_window();
		markdown_image_window(const markdown_image_window&)                    = delete;
		auto operator=(const markdown_image_window&) -> markdown_image_window& = delete;

		/// \brief Admits a body's references and starts what they need.
		void request(const core::markdown_image_intent& aIntent);
		/// \brief Cancels every pending reference and resolution.
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
