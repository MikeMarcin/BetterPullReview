#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_SHARE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_SHARE_HPP

#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_policy.hpp"
#include "bpr/core/review_cycle_round.hpp"
#include "bpr/core/review_cycle_round_markdown.hpp"
#include "bpr/core/review_cycle_verification.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/// \file
/// \brief Compose feedback and Share feedback (RFC-BPR-0056, RFC-BPR-0061,
/// BPR-1161): the edits that make one round's draft, the routes a frozen
/// round can leave by, and what was observed about each delivery.
/// \details A draft is edited one deliberate change at a time; the first
/// draft holds every open note. A route is offered or refused by the same
/// channel decision `authorize` makes, whichever channel it is, and a refused
/// route stays listed as refused: no other route is offered in its place.
/// A delivery observation records only what the host saw happen to these
/// bytes (copied, saved, retrieved, accepted by an adapter, acknowledged by a
/// recipient, failed or uncertain). No observation, alone or together, means
/// the round was received, read, acted on or accepted.
namespace bpr::core {
	/// \brief The ways a frozen round can leave the review.
	enum class cycle_share_route : std::uint8_t {
		/// Copy to Clipboard: the readable document.
		copy_document,
		/// Save to File: the readable document.
		save_document,
		/// Save as Bundle: the document, the exact round and its attachments.
		save_bundle,
		/// Use in external agent: the round is read through the local interface.
		external_agent,
		/// Send to Agent: an installed adapter delivers it.
		send_to_agent,
	};
	inline constexpr std::size_t cycle_share_route_count = 5U;
	[[nodiscard]] auto           stable_name(cycle_share_route aValue) noexcept -> std::string_view;
	/// \brief The disclosure channel a route uses.
	[[nodiscard]] auto share_channel(cycle_share_route aRoute) noexcept -> cycle_disclosure_channel;

	/// \brief What this installation can reach, as the host observed it.
	/// Not permission: policy decides that separately.
	struct cycle_share_capabilities
	{
		/// A local interface a permitted external harness can read a round through.
		bool ExternalInterface = false;
		/// An admitted agent adapter is installed.
		bool Adapter                                            = false;
		auto operator<=>(const cycle_share_capabilities&) const = default;
	};

	/// \brief One route as Share feedback lists it, with the channel decision.
	struct cycle_share_route_offer
	{
		cycle_share_route   Route    = cycle_share_route::copy_document;
		cycle_authorization Decision = cycle_authorization::channel_denied;
		[[nodiscard]] auto  offered() const noexcept -> bool
		{
			return Decision == cycle_authorization::allowed;
		}
		auto operator<=>(const cycle_share_route_offer&) const = default;
	};
	/// \brief The routes for a project of this origin, in menu order: Copy,
	/// Save and Save as Bundle always, each with its own channel's decision;
	/// Use in external agent and Send to Agent only when the capability is
	/// there. A refused route keeps its refusal.
	[[nodiscard]] auto share_routes(const cycle_disclosure_policy& aPolicy, const cycle_workspace_origin& aOrigin, const cycle_share_capabilities& aCapabilities)
		-> std::vector<cycle_share_route_offer>;
	/// \brief The origin a cycle's bound workspace shows: a folder or file of
	/// unknown origin, or a Git checkout, unclassified either way. Nothing
	/// here reads a remote; an unbound cycle is a plain folder.
	[[nodiscard]] auto cycle_origin(const review_cycle_state& aCycle) noexcept -> cycle_workspace_origin;

	/// \brief A change the reviewer makes on Compose feedback.
	struct include_round_item
	{
		cycle_feedback_item_id Item;
		bool                   Include                                      = true;
		auto                   operator<=>(const include_round_item&) const = default;
	};
	/// \brief Moves an included note one place earlier or later.
	struct move_round_item
	{
		cycle_feedback_item_id Item;
		bool                   Earlier                                   = true;
		auto                   operator<=>(const move_round_item&) const = default;
	};
	struct set_round_action
	{
		cycle_round_action Action                                     = cycle_round_action::implement;
		auto               operator<=>(const set_round_action&) const = default;
	};
	struct set_round_goal
	{
		std::string Goal;
		auto        operator<=>(const set_round_goal&) const = default;
	};
	/// \brief Includes source for the recipient to read: a whole file or a
	/// byte range of it, at a captured revision ([[BPR-1169]]).
	struct include_round_context
	{
		cycle_context_selection Context;
		auto                    operator<=>(const include_round_context&) const = default;
	};
	/// \brief Leaves out every included piece of one file.
	struct exclude_round_context
	{
		std::string Path;
		auto        operator<=>(const exclude_round_context&) const = default;
	};
	using cycle_compose_edit = std::variant<include_round_item, move_round_item, set_round_action, set_round_goal, include_round_context, exclude_round_context>;

	/// \brief The first draft of a cycle: every open note in item order, at
	/// its current wording with the suggestions it still offers, against
	/// `aSource`, to implement.
	[[nodiscard]] auto first_composition(std::span<const cycle_next_round_item> aNotes, cycle_source_revision_id aSource) -> cycle_round_selection;
	/// \brief Applies one edit. An included note joins at the end with its
	/// current wording and offered suggestions; a note left out takes the
	/// relations to it along. Nothing when the edit changes nothing or names
	/// a note that is not open.
	[[nodiscard]] auto edit_composition(const cycle_round_selection& aSelection, std::span<const cycle_next_round_item> aNotes, const cycle_compose_edit& aEdit)
		-> std::optional<cycle_round_selection>;
	/// \brief Brings each included note to its current wording and each
	/// selected suggestion to its current revision, after the reviewer edited
	/// them. Nothing when everything is current. A note no longer open stays,
	/// so freezing names it rather than dropping it.
	[[nodiscard]] auto refresh_composition(const cycle_round_selection& aSelection, std::span<const cycle_next_round_item> aNotes) -> std::optional<cycle_round_selection>;

	/// \brief Exactly what left: its format, digest and size.
	struct cycle_delivery_output
	{
		cycle_round_file_kind Format = cycle_round_file_kind::markdown;
		cycle_material_digest Digest;
		std::uint64_t         Bytes                                           = 0;
		auto                  operator<=>(const cycle_delivery_output&) const = default;
	};
	/// \brief One observation about a frozen round leaving by one channel.
	struct cycle_delivery_observation
	{
		cycle_round_ref          Round;
		cycle_round_digest       Digest;
		cycle_delivery_state     State   = cycle_delivery_state::uncertain;
		cycle_disclosure_channel Channel = cycle_disclosure_channel::clipboard;
		/// Required for copied, saved, retrieved and accepted.
		std::optional<cycle_delivery_output> Output;
		/// The saved file's name, never its folder; empty for any other state.
		std::string Destination;
		auto        operator<=>(const cycle_delivery_observation&) const = default;
	};
	struct cycle_delivery_record
	{
		/// Order of observation within the cycle, from 1.
		std::uint64_t              Ordinal = 0;
		cycle_delivery_observation Observation;
		auto                       operator<=>(const cycle_delivery_record&) const = default;
	};
	/// The most observations one cycle keeps.
	inline constexpr std::uint64_t maximum_cycle_deliveries    = 65'536U;
	inline constexpr std::size_t   maximum_delivery_name_bytes = 255U;

	enum class cycle_delivery_problem : std::uint8_t {
		/// Not this cycle's round, or not the digest it was frozen with.
		wrong_round,
		/// The state needs evidence this channel cannot give.
		wrong_channel,
		/// The state names bytes that left, and none are given.
		output_required,
		/// A saved file needs its name: one path segment, no folder.
		destination_not_a_name,
		/// Only a saved file has a destination.
		unexpected_destination,
	};
	[[nodiscard]] auto stable_name(cycle_delivery_problem aValue) noexcept -> std::string_view;
	/// \brief Admits one observation of `aRound`: copied only from the
	/// clipboard, saved only to a local file, retrieved only by the local
	/// interface or a terminal, accepted only by an adapter, acknowledged only
	/// through an adapter or the local interface. Failed and uncertain fit
	/// any channel. Nothing when it may be recorded.
	[[nodiscard]] auto admit_delivery(const local_cycle_subject& aSubject, const cycle_round& aRound, const cycle_delivery_observation& aObservation) -> std::optional<cycle_delivery_problem>;
	/// \brief True for a file name with no folder: not empty, `.` or `..`,
	/// no separator, colon or control character, within its bound.
	[[nodiscard]] auto delivery_destination_name(std::string_view aName) noexcept -> bool;
}

#endif
