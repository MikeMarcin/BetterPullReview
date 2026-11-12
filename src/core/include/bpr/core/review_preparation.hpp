#ifndef BPR_CORE_REVIEW_PREPARATION_HPP
#define BPR_CORE_REVIEW_PREPARATION_HPP
#include "bpr/core/agent_profile.hpp"
#include "bpr/core/review_tour.hpp"
#include <array>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
namespace bpr::core {
	/// \brief The profile's writing hooks. The first four are the tour's
	/// review-preparation hooks, each writing one tour field. [[BPR-1090]]
	/// `feedback` is the review-feedback guidance: it writes no tour field, so
	/// no `writing_target` names it, and a comment workshop request carries it
	/// only while the label helper is on.
	///
	/// [[BPR-2051]] The Enhance prompts came later: one for a tour stop and,
	/// since the owner's look of 2026-11-10 ("Change it from \"Enhance a
	/// note\" to \"Enhance writing\""), one, `enhance_writing`, for every
	/// draft comment, reply and private note. `enhance_stop` is never a
	/// target; it is the instructions a stop's Enhance sends
	/// (`writing_instructions_for`). `enhance_writing` is both: its surfaces
	/// are not tour fields.
	///
	/// [[BPR-2059]] `suggested_comment` came last: how an agent writes the
	/// comments it suggests for the reviewer to post. It writes no field and
	/// is never a target; every Agent tab request carries it in its brief.
	///
	/// RFC-BPR-0110 ([[BPR-2026]]) The two local-authoring prompts came
	/// next: `local_description`, how an agent writes a local draft's full
	/// description, and `local_discussion`, what a local draft's
	/// conversations tell the agent. Neither is a tour field.
	///
	/// RFC-BPR-0114 as amended 2026-11-12: `local_commit_message`, how an
	/// agent writes a commit's subject and description from the staged
	/// change. It writes no tour field either.
	enum class writing_hook : std::uint8_t { problem,
											 solution,
											 outline,
											 comment,
											 feedback,
											 enhance_stop,
											 enhance_writing,
											 suggested_comment,
											 local_description,
											 local_discussion,
											 local_commit_message };
	inline constexpr std::size_t writing_hook_count = 11;
	/// \brief [[BPR-2051]] Whether the hook is one of the Enhance prompts.
	[[nodiscard]] constexpr auto enhance_hook(const writing_hook aHook) noexcept -> bool { return aHook >= writing_hook::enhance_stop && aHook <= writing_hook::enhance_writing; }
	/// \brief The review-preparation hooks, `problem` to `comment`.
	inline constexpr std::size_t review_preparation_hook_count     = 4;
	inline constexpr std::size_t maximum_writing_instruction_bytes = 8192;
	/// \brief Profile-owned text overrides; empty selects the shipped version.
	struct writing_hook_settings
	{
		std::array<std::string, writing_hook_count> Overrides;
		std::uint64_t                               Revision                                        = 1;
		auto                                        operator<=>(const writing_hook_settings&) const = default;
	};
	[[nodiscard]] auto valid_writing_utf8(std::string_view aText) noexcept -> bool;
	[[nodiscard]] auto valid(const writing_hook_settings& aSettings) noexcept -> bool;

	enum class writing_scope : std::uint8_t { selection,
											  field,
											  empty_field };
	enum class writing_refusal : std::uint8_t { invalid_capture,
												stale,
												duplicate,
												invalid_result,
												unchanged,
												ambiguous,
												not_found };
	/// \brief Application-owned identities. Model output never supplies this record.
	struct writing_target
	{
		tour_owner                   Owner;
		tour_draft_id                Draft;
		tour_stop_id                 Stop;
		writing_hook                 Hook = writing_hook::problem;
		bpr_window_context           Window;
		application_session_id       Session;
		agent_profile_id             Agent;
		std::uint64_t                HookRevision     = 0;
		std::uint64_t                TextGeneration   = 0;
		std::uint64_t                EditorGeneration = 0;
		std::array<std::uint8_t, 32> ContextDigest{ };
		auto                         operator==(const writing_target&) const -> bool = default;
	};
	/// \brief One real Qt selection, with half-open canonical UTF-8 boundaries.
	struct writing_selection
	{
		std::size_t   Begin = 0, End = 0;
		int           Anchor = 0, Cursor = 0;
		std::uint64_t FocusRevision                                      = 0;
		unsigned      Spans                                              = 0;
		auto          operator==(const writing_selection&) const -> bool = default;
	};
	struct writing_request
	{
		std::uint64_t                Id = 0;
		writing_target               Target;
		writing_scope                Scope = writing_scope::field;
		writing_selection            Selection;
		std::array<std::uint8_t, 32> RawDigest{ };
		std::string                  Raw;
	};
	/// \brief Accepted splice, applied once by the native editor and persisted as a local delta.
	struct writing_edit
	{
		writing_request Request;
		std::string     Replacement;
		std::string     Text;
	};
	/// \brief [[BPR-2051]] Whose instructions a request on \p aTarget with
	/// \p aScope sends. A tour stop with text (the whole comment or a
	/// selection) is Enhance and sends the stop's Enhance prompt; an empty stop
	/// is Generate comment and sends the stop-comment prompt. Every other
	/// target sends its own.
	[[nodiscard]] constexpr auto writing_instructions_for(const writing_hook aTarget, const writing_scope aScope) noexcept -> writing_hook
	{
		return aTarget == writing_hook::comment && aScope != writing_scope::empty_field ? writing_hook::enhance_stop : aTarget;
	}
	[[nodiscard]] auto writing_digest(std::string_view aText) noexcept -> std::array<std::uint8_t, 32>;
	[[nodiscard]] auto writing_blank(std::string_view aText) noexcept -> bool;
	[[nodiscard]] auto writing_boundary(std::string_view aText, std::size_t aOffset) noexcept -> bool;
	/// \brief Exactly one current match of a quoted passage, as half-open byte offsets of the raw field.
	/// Zero matches and repeated matches are errors; the scope is never widened to make one match.
	[[nodiscard]] auto writing_quote_range(std::string_view aRaw, std::string_view aQuote) -> std::expected<std::pair<std::size_t, std::size_t>, writing_refusal>;
	/// \brief The field text after replacing [aBegin, aEnd) with a result, or why it cannot be applied.
	/// Shared by every presentation: same bounds, same no-op refusal, same UTF-8 and size limits.
	[[nodiscard]] auto splice_writing_result(std::string_view aRaw, std::size_t aBegin, std::size_t aEnd, std::string_view aReplacement) -> std::expected<std::string, writing_refusal>;
	/// \brief One bounded pending replacement. Supersession and consumption retire its ID.
	class scoped_writing_state final
	{
	public:
		[[nodiscard]] auto begin(writing_target aTarget, std::string aRaw, writing_selection aSelection) -> std::expected<writing_request, writing_refusal>;
		[[nodiscard]] auto complete(std::uint64_t aRequest, const writing_target& aCurrent, std::string_view aRaw, std::string aReplacement) -> std::expected<writing_edit, writing_refusal>;
		[[nodiscard]] auto complete_outline(std::uint64_t aRequest, const writing_target& aCurrent) -> bool;
		void               cancel() noexcept { Pending.reset(); }
		[[nodiscard]] auto pending() const noexcept -> const std::optional<writing_request>& { return Pending; }

	private:
		std::uint64_t                  Sequence = 0;
		std::optional<writing_request> Pending;
	};
}
#endif
