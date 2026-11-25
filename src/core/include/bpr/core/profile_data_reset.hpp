#ifndef BPR_CORE_PROFILE_DATA_RESET_HPP
#define BPR_CORE_PROFILE_DATA_RESET_HPP

#include "bpr/core/release_maintenance.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1602]]. What each Troubleshooting reset in Settings ›
/// Storage & privacy removes from one profile, and what it never touches.
/// \details The policy is closed: a store or folder this file does not name
/// belongs to no reset, so a new store is kept until a change names it here.
/// Diagnostic logs, the shared registry and updater state and every other
/// profile are never removed by a reset, and the full purge stays under
/// Profile maintenance.
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
///
/// [[BPR-NNNN]], owner amendment 2026-11-24 ("the clear in settings menu
/// would be better as check boxes rather than separate actions since I often
/// want to clear several things at once but not everything"; "Clear/reset I
/// mean... Same thing"). One request carries a selection of categories, each
/// a checkbox in Settings › Storage & privacy, cleared together under one
/// confirmation; Reset everything is every category. The category Agent
/// sessions and workspaces clears BetterPullReview's own agent conversations
/// (transcripts, names, costs) and removes the retained agent workspaces they
/// own. The agents' own native session files (Claude Code's or Codex's
/// transcripts under their home folders) are not BetterPullReview's and no
/// category touches them. Settings holds the agent setup store those
/// conversations live in, so a selection with Settings always includes Agent
/// sessions and workspaces: the plan says so before anything runs, rather
/// than leaving the retained workspaces with no record.
namespace bpr::core {
	/// \brief The resets a person can ask for.
	/// \details Each value but `everything` is one category of the Clear
	/// list; `everything` names every category at once.
	enum class profile_data_reset : std::uint8_t {
		caches      = 1U,
		settings    = 2U,
		review_data = 3U,
		everything  = 4U,
		/// [[BPR-1858]] Signs out and forgets connections; keeps all other data.
		connections = 5U,
		/// [[BPR-NNNN]] BetterPullReview's agent conversations and the
		/// retained workspaces they own.
		agent_sessions = 6U,
		/// [[BPR-NNNN]] Saved repositories and servers, observer rules and
		/// Home's queue; before the Clear list only Reset everything removed them.
		saved_sources = 7U,
		/// [[BPR-NNNN]] Insights' lifetime statistics and history; before the
		/// Clear list only Reset everything removed them.
		statistics = 8U,
	};

	/// \brief [[BPR-NNNN]]. The categories one Clear asks for, as a set.
	/// \details Converts from one reset, so a single category, or
	/// `everything` (every category), is a selection. Unknown values select
	/// nothing.
	class profile_data_reset_selection
	{
	public:
		constexpr profile_data_reset_selection() noexcept = default;
		// NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions): one reset is a selection.
		constexpr profile_data_reset_selection(const profile_data_reset aReset) noexcept
			: Bits{ bit(aReset) }
		{ }
		/// \brief The selection whose bits are \p aBits; bits no category uses are dropped.
		[[nodiscard]] static constexpr auto from_bits(const std::uint16_t aBits) noexcept -> profile_data_reset_selection
		{
			auto selection = profile_data_reset_selection{ };
			selection.Bits = static_cast<std::uint16_t>(aBits & all_bits);
			return selection;
		}
		/// \brief Every category.
		[[nodiscard]] static constexpr auto all() noexcept -> profile_data_reset_selection { return from_bits(all_bits); }
		[[nodiscard]] constexpr auto        bits() const noexcept -> std::uint16_t { return Bits; }
		[[nodiscard]] constexpr auto        empty() const noexcept -> bool { return Bits == 0U; }
		/// \brief Whether the selection holds \p aReset; for `everything`, every category.
		[[nodiscard]] constexpr auto contains(const profile_data_reset aReset) const noexcept -> bool
		{
			const auto wanted = bit(aReset);
			return wanted != 0U && (Bits & wanted) == wanted;
		}
		[[nodiscard]] constexpr auto with(const profile_data_reset_selection aOther) const noexcept -> profile_data_reset_selection
		{
			return from_bits(static_cast<std::uint16_t>(Bits | aOther.Bits));
		}
		[[nodiscard]] constexpr auto without(const profile_data_reset_selection aOther) const noexcept -> profile_data_reset_selection
		{
			return from_bits(static_cast<std::uint16_t>(Bits & ~aOther.Bits));
		}
		/// \brief The one category it holds, or `everything` when it holds
		/// every one; none for an empty or mixed selection.
		[[nodiscard]] auto single() const noexcept -> std::optional<profile_data_reset>;
		constexpr auto     operator==(const profile_data_reset_selection&) const noexcept -> bool = default;

	private:
		static constexpr std::uint16_t      all_bits = 0b1111'0111U;
		[[nodiscard]] static constexpr auto bit(const profile_data_reset aReset) noexcept -> std::uint16_t
		{
			if (aReset == profile_data_reset::everything) {
				return all_bits;
			}
			const auto value = static_cast<unsigned>(aReset);
			return value >= 1U && value <= 8U ? static_cast<std::uint16_t>(1U << (value - 1U)) : std::uint16_t{ 0U };
		}
		std::uint16_t Bits = 0U;
	};

	/// \brief [[BPR-NNNN]]. The categories of the Clear list, in its order.
	inline constexpr std::array<profile_data_reset, 7> profile_data_reset_categories{
		profile_data_reset::caches,
		profile_data_reset::settings,
		profile_data_reset::agent_sessions,
		profile_data_reset::review_data,
		profile_data_reset::saved_sources,
		profile_data_reset::statistics,
		profile_data_reset::connections,
	};

	/// \brief [[BPR-NNNN]]. One step of a Clear, in the order the host runs them.
	enum class profile_data_reset_step : std::uint8_t {
		/// Remove the agent conversations' retained workspaces, then forget
		/// the conversations. First, while the catalog that names the
		/// workspaces is still there.
		agent_sessions,
		/// Remove the selected stores and folders, and the stored sign-ins,
		/// under one journal.
		files,
		/// Restore Home's queue display choices in the store it keeps.
		queue_display,
		/// Forget every remembered connection in the store it keeps.
		forget_connections,
	};

	/// \brief [[BPR-NNNN]]. What one confirmed selection does, decided before
	/// anything runs.
	struct profile_data_reset_plan
	{
		/// What runs: the chosen categories and those they imply.
		profile_data_reset_selection Selection;
		/// The categories added because a chosen one needs them.
		profile_data_reset_selection Implied;
		/// In order; a step none of the selection needs is absent.
		std::vector<profile_data_reset_step> Steps;
		auto                                 operator==(const profile_data_reset_plan&) const -> bool = default;
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
		/// Only Saved repositories removes this store; Settings restores its
		/// queue display choices inside it instead.
		saved_sources,
		/// Lifetime Insights statistics and history.
		statistics,
		/// [[BPR-1858]] The device bindings that name this profile's vault
		/// entries, removed with those entries by Connections.
		connections,
		/// Never removed by a reset: remembered connections (edited instead),
		/// logs, shared and unknown data.
		kept,
	};

	/// \brief One requested reset, as confirmed in its dialog.
	struct profile_data_reset_request
	{
		/// [[BPR-NNNN]] Every checked category.
		profile_data_reset_selection Reset = profile_data_reset::caches;
		/// The person ticked the acknowledgement that work existing only
		/// here will be deleted.
		bool Acknowledged = false;
		/// Save a copy of the removed store files first.
		bool Copy                                                        = false;
		auto operator==(const profile_data_reset_request&) const -> bool = default;
	};

	/// \brief What a reset did, for its notice when the profile reopens.
	struct profile_data_reset_outcome
	{
		/// What ran: the plan's selection.
		profile_data_reset_selection Reset = profile_data_reset::caches;
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
		bool ConnectionsRetained = false;
		/// [[BPR-NNNN]] The categories that did not clear while the others
		/// did; `Reason` stays `none` for such a partial clear.
		profile_data_reset_selection Failed = { };
		/// [[BPR-NNNN]] Retained agent workspaces removed, and those left
		/// because another process held one or its ownership did not verify.
		std::uint32_t WorkspacesRemoved                                           = 0U;
		std::uint32_t WorkspacesLeft                                              = 0U;
		auto          operator==(const profile_data_reset_outcome&) const -> bool = default;
	};

	/// \brief The area of a store in `state/v1`, by its base name without
	/// `.sqlite3` ("review-ledger"); `kept` for any name it does not know.
	[[nodiscard]] auto profile_data_store_area(std::string_view aStore) noexcept -> profile_data_area;

	/// \brief The area of a top-level profile folder ("repositories"); `kept`
	/// for any folder it does not know.
	[[nodiscard]] auto profile_data_folder_area(std::string_view aFolder) noexcept -> profile_data_area;

	/// \brief Whether `aReset` removes what `aArea` holds. Never `kept`.
	[[nodiscard]] auto profile_data_reset_removes(profile_data_reset_selection aReset, profile_data_area aArea) noexcept -> bool;

	/// \brief Whether the reset deletes work that exists only here, and so
	/// needs the explicit acknowledgement: review data, agent sessions, saved
	/// repositories or statistics.
	[[nodiscard]] auto profile_data_reset_destructive(profile_data_reset_selection aReset) noexcept -> bool;

	/// \brief Whether the reset restores Home's queue display choices in the
	/// store it keeps (Settings without Saved repositories), rather than
	/// removing that store.
	[[nodiscard]] auto profile_data_reset_restores_queue_display(profile_data_reset_selection aReset) noexcept -> bool;

	/// \brief [[BPR-1858]]. Whether the reset signs the profile out (it holds
	/// Connections): it deletes this profile's stored sign-ins through the
	/// credential cleanup and forgets every remembered connection in the
	/// store it keeps.
	[[nodiscard]] auto profile_data_reset_signs_out(profile_data_reset_selection aReset) noexcept -> bool;

	/// \brief [[BPR-NNNN]]. Whether the selection removes any store or folder
	/// of the profile, or a stored sign-in (every category but Agent sessions
	/// and workspaces does).
	[[nodiscard]] auto profile_data_reset_removes_files(profile_data_reset_selection aReset) noexcept -> bool;

	/// \brief [[BPR-NNNN]]. Whether "Save a copy" can be offered: the
	/// selection is destructive and removes store files to copy.
	[[nodiscard]] auto profile_data_reset_copyable(profile_data_reset_selection aReset) noexcept -> bool;

	/// \brief [[BPR-NNNN]]. The categories \p aChosen implies and does not
	/// hold: Agent sessions and workspaces with Settings, since the
	/// conversations live in the agent setup store Settings removes.
	[[nodiscard]] auto profile_data_reset_implied(profile_data_reset_selection aChosen) noexcept -> profile_data_reset_selection;

	/// \brief [[BPR-NNNN]]. The plan for \p aChosen: its closed selection and
	/// its steps in order.
	[[nodiscard]] auto plan_profile_data_reset(profile_data_reset_selection aChosen) -> profile_data_reset_plan;

	/// \brief [[BPR-NNNN]]. The categories that must not run after the agent
	/// sessions step left \p aWorkspacesLeft retained workspaces on disk:
	/// Settings, since removing the agent setup store would lose their only
	/// record. A catalog that could not be read at all holds nothing back: a
	/// damaged store must still be removable.
	[[nodiscard]] auto profile_data_reset_held_back(std::uint32_t aWorkspacesLeft) noexcept -> profile_data_reset_selection;

	/// \brief Admits a confirmed request: `none`, or `invalid_selection` for
	/// an empty selection, a destructive one without its acknowledgement, or
	/// a copy asked of one with nothing worth copying. The request's
	/// selection is judged with what it implies.
	[[nodiscard]] auto admit_profile_data_reset(const profile_data_reset_request& aRequest) noexcept -> release_maintenance_reason;

	/// \brief The reset's name on the command line: "caches", "settings",
	/// "review-data", "connections", "everything", "agent-sessions",
	/// "saved-sources" or "statistics".
	[[nodiscard]] auto profile_data_reset_name(profile_data_reset aReset) noexcept -> std::string_view;

	/// \brief The reset a command-line name selects, or none.
	[[nodiscard]] auto profile_data_reset_from_name(std::string_view aName) noexcept -> std::optional<profile_data_reset>;

	/// \brief [[BPR-NNNN]]. The selection a comma-separated list of names
	/// selects ("caches,agent-sessions"), or none when any name is unknown or
	/// empty.
	[[nodiscard]] auto profile_data_reset_selection_from_names(std::string_view aNames) noexcept -> std::optional<profile_data_reset_selection>;
}

#endif
