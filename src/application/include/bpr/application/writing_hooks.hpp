#ifndef BPR_APPLICATION_WRITING_HOOKS_HPP
#define BPR_APPLICATION_WRITING_HOOKS_HPP
#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/review_preparation.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>
namespace bpr::application {
	struct writing_source
	{
		std::string                            Id;
		std::string                            Text;
		std::optional<core::tour_source_range> Source                                          = std::nullopt;
		auto                                   operator==(const writing_source&) const -> bool = default;
	};
	struct writing_note
	{
		std::string              Kind;
		std::string              Text;
		std::vector<std::string> SourceRefs;
	};
	struct writing_outline_stop
	{
		std::string              Title;
		std::string              Anchor;
		std::vector<std::string> Related;
		std::string              Reason;
		std::string              Question;
	};
	struct writing_result
	{
		bool                              Insufficient = false;
		std::string                       Replacement;
		std::string                       Reason;
		std::vector<writing_note>         Notes;
		std::vector<writing_outline_stop> Stops;
	};
	[[nodiscard]] auto build_writing_prompt(const core::writing_hook_settings& aSettings, const core::writing_request& aRequest, std::span<const writing_source> aSources) -> std::optional<std::string>;
	[[nodiscard]] auto decode_writing_result(std::string_view aJson, core::writing_hook aHook, std::span<const writing_source> aSources, bool aRequiresEvidence) -> std::optional<writing_result>;

	inline constexpr std::string_view writing_hook_version = "review-preparation-v1";
	/// \brief [[BPR-1090]] Version of the shipped review-feedback guidance,
	/// `resources/review-preparation/review-feedback.md`. A change to that text
	/// changes this name; a profile override is kept whatever it says.
	inline constexpr std::string_view review_feedback_hook_version = "review-feedback-v1";
	/// \brief [[BPR-2051]] Versions of the shipped Enhance prompts,
	/// `resources/review-preparation/enhance-*.md`, one for each, so an
	/// override can say which default it was written against.
	inline constexpr std::string_view enhance_stop_hook_version = "enhance-tour-stop-v1";
	/// One prompt for comments, replies and notes since 2026-11-10; it
	/// replaced `enhance-comment-v1` and `enhance-note-v1`, which never shipped.
	inline constexpr std::string_view enhance_writing_hook_version = "enhance-writing-v1";
	/// \brief [[BPR-2059]] Version of the shipped suggested-comment format,
	/// `resources/review-preparation/suggested-comment.md`.
	inline constexpr std::string_view suggested_comment_hook_version = "suggested-comment-v1";
	/// \brief [[BPR-2051]] The stable purpose ID of one prompt, as
	/// RFC-BPR-0107's exchange names it: it never changes with the prompt's
	/// text, version, position in this enumeration or display name.
	[[nodiscard]] auto writing_hook_purpose(core::writing_hook aHook) noexcept -> std::string_view;
	/// \brief The prompt a purpose ID names, or none for an ID this version
	/// does not recognize. The retired `enhance.comment` and `enhance.note`
	/// name `enhance.writing`, which replaced them.
	[[nodiscard]] auto writing_hook_of_purpose(std::string_view aPurpose) noexcept -> std::optional<core::writing_hook>;
	/// \brief [[BPR-2051]] SHA-256 of one hook's shipped default text: with
	/// `writing_hook_version_of`, the built-in provenance an exported
	/// override carries.
	[[nodiscard]] auto default_writing_hook_digest(core::writing_hook aHook) -> std::array<std::uint8_t, 32>;
	/// \brief The version of one hook's shipped default.
	[[nodiscard]] auto writing_hook_version_of(core::writing_hook aHook) noexcept -> std::string_view;
	/// \brief Shipped instruction resource; no runtime file or repository lookup.
	[[nodiscard]] auto default_writing_hook(core::writing_hook aHook) -> std::string_view;
	[[nodiscard]] auto writing_application_contract() -> std::string_view;
	/// \brief The profile's override, or the shipped default when it has none.
	[[nodiscard]] auto effective_writing_hook(const core::writing_hook_settings& aSettings, core::writing_hook aHook) -> std::string_view;

	enum class review_label_helper : std::uint8_t;
	/// \brief [[BPR-1090]] The review-feedback guidance a comment workshop
	/// request carries: the effective `feedback` hook while the label helper is
	/// on, and nothing while it is off. Sending it is the workshop's
	/// (BPR-1091); no other request carries it.
	[[nodiscard]] auto workshop_feedback_guidance(const core::writing_hook_settings& aSettings, review_label_helper aHelper) -> std::string_view;
	/// \brief [[BPR-1091]] The guidance one comment-workshop press sends, as
	/// the supplement carries it: `workshop_feedback_guidance`'s text, the
	/// shipped version it was taken from, and whether it is the profile's
	/// override. None while the label helper is off, which leaves the press's
	/// request byte for byte what it was before the hook existed.
	/// \brief [[BPR-2059]] The suggested-comment format every Agent tab
	/// request's brief carries: the profile's override, or the shipped
	/// default, with its version and which of the two it is.
	[[nodiscard]] auto suggested_comment_format(const core::writing_hook_settings& aSettings) -> std::optional<core::supplement_feedback_guidance>;
	[[nodiscard]] auto workshop_feedback_supplement(const core::writing_hook_settings& aSettings, review_label_helper aHelper) -> std::optional<core::supplement_feedback_guidance>;
}
#endif
