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
/// Diagnostic logs, the shared registry and updater state, agent workspaces
/// and every other profile are never removed by a reset, and the full purge
/// stays under Profile maintenance.
///
/// [[BPR-1858]] amends [[BPR-1602]]'s rule that no reset reads or deletes
/// credentials, by the owner's decision 04ebe7a6 in attended-review session
/// bbad2aaf (2026-11-04: "Reset everything, add a separate reset connections
/// option on that page too."). Reset connections and Reset everything sign
/// the profile out: this profile's own vault entries named by its device
/// bindings are deleted through the purge's credential cleanup, its
/// device-bindings store goes, and every remembered connection, native or
/// GitHub CLI, is forgotten while the identity high-water marks stay. The
/// next launch opens at Sign in to GitHub. gh's own sign-in belongs to gh and
/// the other tools that use it, and no reset touches it.
namespace bpr::core {
	/// \brief The resets a person can ask for.
	enum class profile_data_reset : std::uint8_t {
		caches      = 1U,
		settings    = 2U,
		review_data = 3U,
		everything  = 4U,
		/// [[BPR-1858]] Signs out and forgets connections; keeps all other data.
		connections = 5U,
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
		/// Lifetime Insights statistics and history, removed only by Reset everything.
		statistics,
		/// [[BPR-1858]] The device bindings that name this profile's vault
		/// entries, removed with those entries by Reset connections and
		/// Reset everything.
		connections,
		/// Never removed by a reset: remembered connections (edited instead),
		/// logs, shared and unknown data.
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
		bool QueueDisplayRestored = true;
		/// [[BPR-1858]] The reset signed out: it forgot a remembered
		/// connection (native or GitHub CLI) or deleted a stored sign-in.
		bool SignedOut = false;
		/// [[BPR-1858]] The reset could not forget the remembered connections;
		/// its stored sign-ins and other data were removed.
		bool ConnectionsRetained                                         = false;
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

	/// \brief [[BPR-1858]]. Whether the reset signs the profile out (Reset
	/// connections and Reset everything): it deletes this profile's stored
	/// sign-ins through the credential cleanup and forgets every remembered
	/// connection in the store it keeps.
	[[nodiscard]] auto profile_data_reset_signs_out(profile_data_reset aReset) noexcept -> bool;

	/// \brief Admits a confirmed request: `none`, or `invalid_selection` for
	/// an unknown reset, a destructive one without its acknowledgement, or a
	/// copy asked of a reset that removes nothing worth copying.
	[[nodiscard]] auto admit_profile_data_reset(const profile_data_reset_request& aRequest) noexcept -> release_maintenance_reason;

	/// \brief The reset's name on the command line: "caches", "settings",
	/// "review-data", "connections" or "everything".
	[[nodiscard]] auto profile_data_reset_name(profile_data_reset aReset) noexcept -> std::string_view;

	/// \brief The reset a command-line name selects, or none.
	[[nodiscard]] auto profile_data_reset_from_name(std::string_view aName) noexcept -> std::optional<profile_data_reset>;
}

#endif
