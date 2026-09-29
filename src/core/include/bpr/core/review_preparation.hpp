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
	enum class writing_hook : std::uint8_t { problem,
											 solution,
											 outline,
											 comment };
	inline constexpr std::size_t writing_hook_count                = 4;
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
