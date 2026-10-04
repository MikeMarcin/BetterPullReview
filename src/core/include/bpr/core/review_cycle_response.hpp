#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_RESPONSE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_RESPONSE_HPP

#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/review_cycle_round.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

/// \file
/// \brief What recipients say back about a frozen round (RFC-BPR-0057,
/// BPR-1164): an optional structured response (`bpr.cycle.response.v1`) or a
/// reply the reviewer enters by hand.
/// \details Both are untrusted statements appended to the cycle. A response
/// names one cycle, round and canonical round digest, and each of its entries
/// names an exact item revision of that round. Dispositions are the
/// recipient's claims; no field can verify, approve, publish, grant a
/// capability or name its own trust. Claimed authors, models and evidence stay
/// claims: evidence values are inert text that nothing opens, runs or
/// resolves. A partial response covers only the items it names; the others
/// have no response, which is not "not attempted". A manual reply needs no
/// JSON, identifier or protocol. Nothing here writes source, captures a
/// revision or records a reviewer decision.
namespace bpr::core {
	/// A response document is read within this bound.
	inline constexpr std::size_t maximum_response_document_bytes = 16U * 1024U * 1024U;
	inline constexpr std::size_t maximum_response_items          = 4096U;
	inline constexpr std::size_t maximum_response_evidence       = 256U;
	/// One entry's body, and one manual reply.
	inline constexpr std::size_t maximum_response_body_bytes = 256U * 1024U;
	/// A name, claimed model, evidence value or sender label.
	inline constexpr std::size_t maximum_response_text_bytes = 4096U;

	/// \brief What a recipient claims about one item. Never a verification.
	enum class cycle_response_disposition : std::uint8_t {
		addressed,
		partially_addressed,
		disputed,
		blocked,
		discussion_only,
	};
	[[nodiscard]] auto stable_name(cycle_response_disposition aValue) noexcept -> std::string_view;
	enum class cycle_claimed_author_kind : std::uint8_t {
		human,
		agent,
		unknown,
	};
	[[nodiscard]] auto stable_name(cycle_claimed_author_kind aValue) noexcept -> std::string_view;
	/// \brief Who the response says wrote it: a claim, never an observation.
	struct cycle_claimed_author
	{
		std::string                DisplayName;
		cycle_claimed_author_kind  Kind = cycle_claimed_author_kind::unknown;
		std::optional<std::string> Provider;
		std::optional<std::string> Model;
		std::optional<std::string> Effort;
		auto                       operator<=>(const cycle_claimed_author&) const = default;
	};
	enum class cycle_evidence_kind : std::uint8_t {
		text,
		path,
		commit,
		url,
		test_report,
	};
	[[nodiscard]] auto stable_name(cycle_evidence_kind aValue) noexcept -> std::string_view;
	/// \brief Reported evidence: inert text. A claimed test report is not a test result.
	struct cycle_response_evidence
	{
		cycle_evidence_kind Kind = cycle_evidence_kind::text;
		std::string         Value;
		auto                operator<=>(const cycle_response_evidence&) const = default;
	};
	struct cycle_response_entry
	{
		/// The round's item name (`I<n>`) and its exact revision.
		std::string                          Item;
		std::uint64_t                        ItemRevision = 0;
		cycle_response_disposition           Disposition  = cycle_response_disposition::discussion_only;
		std::string                          Body;
		std::vector<cycle_response_evidence> Evidence;
		auto                                 operator<=>(const cycle_response_entry&) const = default;
	};
	/// \brief A structured response, as sent.
	struct cycle_response_document
	{
		std::string          ResponseId;
		std::string          CycleId;
		std::string          RoundId;
		cycle_round_digest   RoundDigest;
		cycle_claimed_author ClaimedAuthor;
		/// A revision name the recipient says holds the result: only a
		/// reference until this profile captured that revision.
		std::optional<std::string>        ResultRevision;
		std::vector<cycle_response_entry> Entries;
		auto                              operator<=>(const cycle_response_document&) const = default;
	};

	enum class cycle_response_decode_problem : std::uint8_t {
		/// Not strict JSON; see `Json`.
		json,
		/// Another schema.
		version,
		/// A missing, extra or mistyped field, or a value outside its enum:
		/// including any attempt to verify, approve, publish or grant.
		shape,
		/// An identifier, revision or digest that breaks its pattern.
		identifier,
		/// Over a count or size bound, or text that is not UTF-8.
		bounds,
		/// The same item twice.
		duplicate,
	};
	[[nodiscard]] auto stable_name(cycle_response_decode_problem aValue) noexcept -> std::string_view;
	struct cycle_response_decode_error
	{
		cycle_response_decode_problem   Problem = cycle_response_decode_problem::shape;
		std::optional<cycle_json_error> Json;
		/// The entry at fault, when one is.
		std::optional<std::size_t> Entry;
		auto                       operator<=>(const cycle_response_decode_error&) const = default;
	};
	struct cycle_decoded_response
	{
		cycle_response_document Document;
		/// The canonical bytes; identical content has identical bytes.
		std::string           Canonical;
		cycle_material_digest Digest;
	};
	/// \brief Reads a response document as untrusted data: strict JSON, the
	/// exact field sets, patterns, enums and bounds, before anything is returned.
	[[nodiscard]] auto decode_response_document(std::string_view aDocument) -> std::expected<cycle_decoded_response, cycle_response_decode_error>;
	/// \brief The canonical bytes of a document; nothing when it has none.
	[[nodiscard]] auto encode_response_document(const cycle_response_document& aDocument) -> std::optional<std::string>;

	enum class cycle_response_problem : std::uint8_t {
		/// It names another cycle.
		wrong_cycle,
		/// It names another round than the one it is attached to.
		wrong_round,
		/// It names the round with other content than the frozen round's.
		wrong_digest,
		/// An entry names an item the round does not hold.
		unknown_item,
		/// An entry names another revision of the item than the round holds.
		stale_item,
		/// A manual reply with no text.
		empty,
		/// The response identifier was already used for other content.
		reused_identifier,
	};
	[[nodiscard]] auto stable_name(cycle_response_problem aValue) noexcept -> std::string_view;
	struct cycle_response_refusal
	{
		cycle_response_problem     Problem = cycle_response_problem::wrong_round;
		std::optional<std::size_t> Entry;
		auto                       operator<=>(const cycle_response_refusal&) const = default;
	};
	/// \brief Checks a response against the frozen round it is attached to.
	[[nodiscard]] auto check_response(const cycle_response_document& aResponse, const cycle_round_payload& aRound, const cycle_round_digest& aDigest)
		-> std::optional<cycle_response_refusal>;

	/// \brief A reply the reviewer enters by hand: pasted prose, an email, a
	/// chat message. The sender label is what the reviewer typed.
	struct cycle_manual_reply
	{
		std::string RoundId;
		std::string From;
		/// Round items the reply is about; empty for the round as a whole.
		std::vector<std::string> Items;
		std::string              Body;
		auto                     operator<=>(const cycle_manual_reply&) const = default;
	};
	/// \brief Checks a manual reply against its round: some text, items the
	/// round holds, each once.
	[[nodiscard]] auto check_manual_reply(const cycle_manual_reply& aReply, const cycle_round_payload& aRound) -> std::optional<cycle_response_refusal>;
	/// \brief The stored form (`bpr.cycle.manual-reply.v1`, canonical JSON), and back.
	[[nodiscard]] auto encode_manual_reply(const cycle_manual_reply& aReply) -> std::optional<std::string>;
	[[nodiscard]] auto decode_manual_reply(std::string_view aCanonical) -> std::optional<cycle_manual_reply>;

	/// \brief How this profile received a statement: the observed channel,
	/// separate from anything the statement claims about itself.
	enum class cycle_response_channel : std::uint8_t {
		/// A structured response file the reviewer chose.
		response_file,
		/// Text the reviewer entered or pasted.
		manual_entry,
		/// A structured response a harness holding a response grant appended
		/// through the local interface ([[BPR-1169]]).
		local_interface,
		/// A structured response the recipient returned to the agent adapter
		/// that sent it the round, when it names exactly that round
		/// ([[BPR-1180]]).
		agent_adapter,
	};
	[[nodiscard]] auto stable_name(cycle_response_channel aValue) noexcept -> std::string_view;
	/// \brief Whether the statement is a structured response document, as
	/// opposed to a manual reply.
	[[nodiscard]] auto structured_response(cycle_response_channel aValue) noexcept -> bool;
	/// \brief One appended statement as the store records it. Its content is
	/// an immutable object: the canonical response or manual reply.
	struct cycle_response_record
	{
		cycle_response_ref     Id;
		cycle_round_id         Round;
		cycle_round_digest     RoundDigest;
		cycle_response_channel Channel = cycle_response_channel::manual_entry;
		/// The response's own identifier; empty for a manual reply.
		std::string           ForeignId;
		cycle_material_digest Content;
		std::uint64_t         Size                                            = 0;
		auto                  operator<=>(const cycle_response_record&) const = default;
	};

	/// \brief What one statement says about one item, for display beside the note.
	struct cycle_item_statement
	{
		cycle_response_id                         Response;
		cycle_round_id                            Round;
		cycle_response_channel                    Channel = cycle_response_channel::manual_entry;
		std::string                               From;
		std::optional<cycle_claimed_author>       ClaimedAuthor;
		std::optional<cycle_response_disposition> Disposition;
		std::string                               Body;
		std::vector<cycle_response_evidence>      Evidence;
		auto                                      operator<=>(const cycle_item_statement&) const = default;
	};
	using cycle_response_content = std::variant<cycle_response_document, cycle_manual_reply>;
	/// \brief Every statement about `aItem` (`I<n>`), in append order. A
	/// manual reply about the round as a whole is not an item statement.
	[[nodiscard]] auto item_statements(const std::vector<std::pair<cycle_response_record, cycle_response_content>>& aResponses, std::string_view aItem)
		-> std::vector<cycle_item_statement>;
	/// \brief The claimed result revision, when this cycle captured it; a
	/// name that is not a captured local revision resolves to nothing.
	[[nodiscard]] auto resolve_claimed_result(const review_cycle_state& aCycle, const std::optional<std::string>& aClaimed) -> std::optional<cycle_source_revision_id>;
}

#endif
