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
	/// \brief [[BPR-2140]], RFC-BPR-0107 (P02, owner 2026-11-12). The same
	/// prompt in two parts: the instructions (the contract, the hook's text,
	/// the answer's schema and style), which go in the system prompt, and the
	/// run's material (the untrusted evidence), which goes in the message.
	/// `build_writing_prompt` is the two joined.
	struct writing_prompt_parts
	{
		std::string Instructions;
		std::string Message;
	};
	[[nodiscard]] auto build_writing_prompt_parts(const core::writing_hook_settings& aSettings, const core::writing_request& aRequest, std::span<const writing_source> aSources)
		-> std::optional<writing_prompt_parts>;
	[[nodiscard]] auto decode_writing_result(std::string_view aJson, core::writing_hook aHook, std::span<const writing_source> aSources, bool aRequiresEvidence) -> std::optional<writing_result>;

	inline constexpr std::string_view writing_hook_version = "review-preparation-v1";
	/// \brief [[BPR-1996]], RFC-BPR-0100. Version of the shipped stop-comment
	/// guidance, `resources/review-preparation/tour-stop-comment.md`, which
	/// gained the citation and moved-code rules; the other tour hooks keep
	/// `writing_hook_version`.
	inline constexpr std::string_view tour_stop_comment_hook_version = "tour-stop-comment-v2";
	/// \brief [[BPR-1090]] Version of the shipped review-feedback guidance,
	/// `resources/review-preparation/review-feedback.md`. A change to that text
	/// changes this name; a profile override is kept whatever it says.
	inline constexpr std::string_view review_feedback_hook_version = "review-feedback-v2";
	/// \brief [[BPR-2051]] Versions of the shipped Enhance prompts,
	/// `resources/review-preparation/enhance-*.md`, one for each, so an
	/// override can say which default it was written against.
	inline constexpr std::string_view enhance_stop_hook_version = "enhance-tour-stop-v1";
	/// One prompt for comments, replies and notes since 2026-11-10; it
	/// replaced `enhance-comment-v1` and `enhance-note-v1`, which never shipped.
	inline constexpr std::string_view enhance_writing_hook_version = "enhance-writing-v1";
	/// \brief [[BPR-2059]] Version of the shipped suggested-comment format,
	/// `resources/review-preparation/suggested-comment.md`.
	/// [[BPR-2264]] v2: the problem, what goes wrong, then the fix.
	/// [[BPR-2270]] v3: a label as the Comment labels Feature asks, when the
	/// request carries it; otherwise none.
	inline constexpr std::string_view suggested_comment_hook_version = "suggested-comment-v3";
	/// \brief RFC-BPR-0110 ([[BPR-2026]]) Versions of the shipped
	/// local-authoring prompts, `resources/review-preparation/local-*.md`.
	inline constexpr std::string_view local_description_hook_version = "local-description-v1";
	inline constexpr std::string_view local_discussion_hook_version  = "local-discussion-v1";
	/// \brief RFC-BPR-0114 as amended 2026-11-12. Version of the shipped
	/// commit-message prompt, `resources/review-preparation/local-commit-message.md`.
	inline constexpr std::string_view local_commit_message_hook_version = "local-commit-message-v1";
	/// \brief RFC-BPR-0116 ([[BPR-2073]], [[BPR-2074]]). Versions of the
	/// returning questions' prompts, `resources/review-preparation/review-*.md`.
	inline constexpr std::string_view review_latest_upload_hook_version = "review-latest-upload-v2";
	inline constexpr std::string_view review_proposed_fix_hook_version  = "review-proposed-fix-v2";
	/// \brief RFC-BPR-0117 ([[BPR-2076]]). Version of Explain this file's
	/// prompt, `resources/review-preparation/review-explain-file.md`.
	inline constexpr std::string_view review_explain_file_hook_version = "review-explain-file-v2";
	/// \brief RFC-BPR-0118 ([[BPR-2078]]). Version of Suggest the change's
	/// prompt, `resources/review-preparation/review-suggest-change.md`.
	inline constexpr std::string_view review_suggest_change_hook_version = "review-suggest-change-v1";
	/// \brief RFC-BPR-0120 ([[BPR-2189]]). Version of a local draft's check
	/// prompt, `resources/review-preparation/local-run-check.md`.
	inline constexpr std::string_view local_run_check_hook_version = "local-draft-run-check-v1";
	/// \brief [[BPR-2264]]. Version of the Writing style Feature's shipped
	/// text, `resources/review-preparation/writing-style.md`.
	inline constexpr std::string_view writing_style_hook_version = "writing-style-v1";
	/// \brief [[BPR-2270]]. Version of the Comment labels Feature's shipped
	/// text, `resources/review-preparation/comment-labels.md`.
	inline constexpr std::string_view comment_labels_hook_version = "comment-labels-v1";
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
	/// \brief RFC-BPR-0128 (docs/design/report-templates-v1). The report shape an
	/// Agent tab request carries for the asks answered as reports outside the
	/// returning questions (What could this change break?, a review): the
	/// shared rules and those two templates,
	/// `resources/review-preparation/report-templates.md`.
	inline constexpr std::string_view report_templates_version = "report-templates-v2";
	[[nodiscard]] auto                report_templates_guidance() -> core::supplement_feedback_guidance;
	/// \brief [[BPR-2264]]. The Writing style Feature (`/bpr:writing-style`)
	/// as a request carries it: a fixed line that keeps every format the
	/// request's own instructions give in charge, then the profile's
	/// override, or the shipped text, as written. The text may name a skill
	/// installed for the agent; it is sent as it is.
	[[nodiscard]] auto writing_style_instructions(const core::writing_hook_settings& aSettings) -> std::string;
	/// \brief [[BPR-2264]]. The one helper every agent request goes through
	/// for its Writing style: the text above when a person reads the answer
	/// (\p aHumanAudience) or when the request's own text (\p aOwnTexts: its
	/// template, override or the person's typed words, never values
	/// substituted from pull request data) names `/bpr:writing-style`;
	/// otherwise nothing. Once, however often it is named. It is
	/// `prompt_features_for` with no posted comments, so own text that names
	/// `/bpr:comment-labels` brings Comment labels too.
	[[nodiscard]] auto writing_style_for(const core::writing_hook_settings& aSettings, bool aHumanAudience, std::span<const std::string_view> aOwnTexts = { }) -> std::string;
	/// \brief The first line of `writing_style_instructions`, which a
	/// request's style always starts with.
	[[nodiscard]] auto writing_style_preamble() noexcept -> std::string_view;
	/// \brief [[BPR-2270]]. The Comment labels Feature
	/// (`/bpr:comment-labels`) as a request carries it: a fixed line that
	/// confines it to the comments written for the reviewer to post, then the
	/// profile's override, or the shipped text. A blank override
	/// (`core::blank_feature_override`) turns it off: nothing.
	[[nodiscard]] auto comment_labels_instructions(const core::writing_hook_settings& aSettings) -> std::string;
	/// \brief The first line of `comment_labels_instructions`.
	[[nodiscard]] auto comment_labels_preamble() noexcept -> std::string_view;
	/// \brief [[BPR-2270]]. Who reads what a request writes, which decides the
	/// Features it carries.
	struct prompt_feature_audience
	{
		/// A person reads the answer: the Writing style.
		bool HumanAudience = false;
		/// The answer may hold a comment written for the reviewer to post (a
		/// suggested comment, a reply, a comment workshop's draft): Comment
		/// labels.
		bool PostedComments = false;
	};
	/// \brief [[BPR-2264]], [[BPR-2270]]. The one helper every agent request
	/// goes through for its Features: Comment labels when \p aAudience posts
	/// comments or the request's own text names `/bpr:comment-labels`, then
	/// the Writing style when a person reads the answer or the own text names
	/// `/bpr:writing-style`, each once however often it is named. The
	/// Writing style stays last.
	[[nodiscard]] auto prompt_features_for(const core::writing_hook_settings& aSettings, prompt_feature_audience aAudience, std::span<const std::string_view> aOwnTexts = { })
		-> std::string;
	[[nodiscard]] auto workshop_feedback_supplement(const core::writing_hook_settings& aSettings, review_label_helper aHelper) -> std::optional<core::supplement_feedback_guidance>;
}
#endif
