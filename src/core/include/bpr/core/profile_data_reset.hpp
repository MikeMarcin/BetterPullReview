#ifndef BPR_CORE_PROFILE_DATA_RESET_HPP
#define BPR_CORE_PROFILE_DATA_RESET_HPP

#include "bpr/core/release_maintenance.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

/// \file
/// \brief [[BPR-1602]]. What each Troubleshooting reset in Settings ›
/// Storage & privacy removes from one profile, and what it never touches.
/// \details The policy is closed: a store or folder this file does not name
/// belongs to no reset, so a new store is kept until a change names it here.
/// Credentials, their device bindings and remembered connections, diagnostic
/// logs, the shared registry and updater state, agent workspaces and every
/// other profile are never part of a reset; signing out stays in Accounts &
/// hosts and the full purge stays under Profile maintenance.
namespace bpr::core {
	/// \brief The four resets a person can ask for.
	enum class profile_data_reset : std::uint8_t {
		caches      = 1U,
		settings    = 2U,
		review_data = 3U,
		everything  = 4U,
	};

	/// \brief Which part of a profile a store or folder holds.
	enum class profile_data_area : std::uint8_t {
		/// Downloaded or derived data the application rebuilds.
		caches,
		/// Choices made in Settings, and the window layout.
		settings,
		/// Review work that exists only on this computer.
		review_data,
		/// Saved repositories and servers, observer rules and Home's queue.
		/// Only Reset everything removes this store; Reset settings restores
		/// its queue display choices inside it instead.
		saved_sources,
		/// Never removed by a reset: sign-in, logs, shared and unknown data.
		kept,
	};

	/// \brief One requested reset, as confirmed in its dialog.
	struct profile_data_reset_request
	{
		profile_data_reset Reset = profile_data_reset::caches;
		/// The person ticked "I understand my private notes and unsent
		/// comments will be deleted".
		bool Acknowledged = false;
		/// Save a copy of the removed store files first.
		bool Copy                                                        = false;
		auto operator==(const profile_data_reset_request&) const -> bool = default;
	};

	/// \brief What a reset did, for its notice when the profile reopens.
	struct profile_data_reset_outcome
	{
		profile_data_reset Reset = profile_data_reset::caches;
		/// `none` when the reset completed. Anything else removed nothing,
		/// except `recovery_required`, which the next start finishes.
		release_maintenance_reason Reason = release_maintenance_reason::none;
		/// Exact files and folders removed.
		std::uint32_t Removed = 0U;
		/// The copy was saved before anything was removed.
		bool Copied = false;
		/// Reset settings restored Home's queue display choices too. False
		/// when that store could not be written; its other settings reset.
		bool QueueDisplayRestored                                        = true;
		auto operator==(const profile_data_reset_outcome&) const -> bool = default;
	};

	/// \brief The area of a store in `state/v1`, by its base name without
	/// `.sqlite3` ("review-ledger"); `kept` for any name it does not know.
	[[nodiscard]] auto profile_data_store_area(std::string_view aStore) noexcept -> profile_data_area;

	/// \brief The area of a top-level profile folder ("repositories"); `kept`
	/// for any folder it does not know.
	[[nodiscard]] auto profile_data_folder_area(std::string_view aFolder) noexcept -> profile_data_area;

	/// \brief Whether `aReset` removes what `aArea` holds. Never `kept`.
	[[nodiscard]] auto profile_data_reset_removes(profile_data_reset aReset, profile_data_area aArea) noexcept -> bool;

	/// \brief Whether the reset deletes work that exists only here, and so
	/// needs the explicit acknowledgement and offers a copy first.
	[[nodiscard]] auto profile_data_reset_destructive(profile_data_reset aReset) noexcept -> bool;

	/// \brief Whether the reset restores Home's queue display choices in the
	/// store it keeps (Reset settings), rather than removing that store.
	[[nodiscard]] auto profile_data_reset_restores_queue_display(profile_data_reset aReset) noexcept -> bool;

	/// \brief Admits a confirmed request: `none`, or `invalid_selection` for
	/// an unknown reset, a destructive one without its acknowledgement, or a
	/// copy asked of a reset that removes nothing worth copying.
	[[nodiscard]] auto admit_profile_data_reset(const profile_data_reset_request& aRequest) noexcept -> release_maintenance_reason;

	/// \brief The reset's name on the command line: "caches", "settings",
	/// "review-data" or "everything".
	[[nodiscard]] auto profile_data_reset_name(profile_data_reset aReset) noexcept -> std::string_view;

	/// \brief The reset a command-line name selects, or none.
	[[nodiscard]] auto profile_data_reset_from_name(std::string_view aName) noexcept -> std::optional<profile_data_reset>;
}

#endif
