#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SETUP_DESTINATION_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SETUP_DESTINATION_PROJECTION_HPP

#include "bpr/core/setup_destination.hpp"
#include <functional>
#include <variant>

namespace bpr::application {
	/// \brief Parses supported HTTPS PR paths without selecting an authority.
	[[nodiscard]] auto parse_setup_destination(std::string_view aText)
		-> std::optional<core::setup_destination>;

	/// \brief Safe navigation presentation, containing no credential capability.
	///
	/// \details `Navigated` accompanies `Opened` when the receiving window
	/// already showed the pull request ([[BPR-0987]]): nothing is loading, and
	/// the window keeps the view it had.
	struct setup_destination_projection
	{
		std::optional<core::pending_setup_destination> Pending;
		std::string                                    Notice;
		bool                                           Opened             = false;
		bool                                           Navigated          = false;
		bool                                           RepositoryPrepared = false;
	};

	struct submit_setup_destination
	{
		std::string Text;
	};
	struct continue_setup_destination
	{
		core::setup_request_id Request;
	};
	struct cancel_setup_destination
	{ };
	/// \brief Explicit preparation for a repository draft, never a final Add gesture.
	struct submit_repository_setup
	{
		std::string Text;
	};
	using setup_destination_intent   = std::variant<submit_setup_destination, continue_setup_destination, cancel_setup_destination, submit_repository_setup>;
	using setup_destination_dispatch = std::function<void(setup_destination_intent)>;
}
#endif
