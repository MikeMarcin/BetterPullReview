#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PROFILE_MANAGEMENT_PROJECTION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PROFILE_MANAGEMENT_PROJECTION_HPP

#include "bpr/core/profile_window.hpp"
#include "bpr/core/window_review_restoration.hpp"

#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace bpr::application {
	enum class profile_management_runtime : std::uint8_t { closed,
														   opening,
														   open,
														   closing };
	enum class profile_management_operation : std::uint8_t { reload,
															 create,
															 rename,
															 open,
															 new_window,
															 close,
															 quit };
	enum class profile_management_failure : std::uint8_t {
		registry_unavailable,
		invalid_registry,
		invalid_label,
		unknown_profile,
		profile_limit,
		window_limit,
		identity_exhausted,
		busy,
		closing,
		runtime_unavailable,
		owner_busy,
		cancelled,
		conflict
	};
	struct profile_management_item
	{
		core::bpr_profile_id       Profile;
		std::string                Label;
		profile_management_runtime Runtime = profile_management_runtime::closed;
	};
	struct profile_management_error
	{
		profile_management_operation        Operation;
		std::optional<core::bpr_profile_id> Profile;
		profile_management_failure          Reason;
	};
	struct profile_management_edit
	{
		profile_management_operation Operation;
		core::bpr_profile_id         Profile;
	};
	/// \brief Bounded accepted labels and runtime status; failed edits never carry replacement text.
	struct profile_management_projection
	{
		std::vector<profile_management_item>    Profiles;
		bool                                    Ready   = false;
		bool                                    Busy    = false;
		bool                                    Closing = false;
		std::optional<profile_management_error> Failure;
		std::optional<profile_management_edit>  AcceptedEdit;
	};
	struct reload_bpr_profiles_intent
	{ };
	struct create_bpr_profile_intent
	{
		std::string Label;
	};
	struct rename_bpr_profile_intent
	{
		core::bpr_profile_id Profile;
		std::string          Label;
	};
	struct open_bpr_profile_intent
	{
		core::bpr_profile_id Profile;
	};
	/// \brief Requests another live window in the named profile runtime.
	///
	/// \details [[BPR-1342]]. `Review`, when present, is the pull request a
	/// Home gesture asked the new window to open: one destination the sending
	/// window already admitted. It rides to the fresh window's start command
	/// only, never to a restored one.
	struct new_bpr_window_intent
	{
		core::bpr_profile_id                           Profile;
		std::optional<core::window_review_destination> Review{ };
	};

	using profile_management_intent   = std::variant<reload_bpr_profiles_intent, create_bpr_profile_intent, rename_bpr_profile_intent, open_bpr_profile_intent, new_bpr_window_intent>;
	using profile_management_dispatch = std::function<void(profile_management_intent)>;

	enum class profile_runtime_command_kind : std::uint8_t { start,
															 activate,
															 close,
															 quit };
	/// \brief Composition performs runtime work under the exact reserved window context.
	struct profile_runtime_command
	{
		profile_runtime_command_kind Kind;
		core::bpr_window_context     Context;
		std::string                  Label;
		bool                         Restore = false;
		/// [[BPR-1342]]. The pull request the fresh window opens after Home.
		std::optional<core::window_review_destination> Review{ };
	};
}
#endif
