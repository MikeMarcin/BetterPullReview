#include "bpr/application/writing_hooks.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/prompt_features.hpp"
#include "writing_hooks_data.hpp"
#include <algorithm>
namespace bpr::application {
	static_assert(detail::WritingDefaults.size() == core::writing_hook_count, "every hook ships a default");
	auto default_writing_hook(core::writing_hook aHook) -> std::string_view
	{
		const auto index = static_cast<std::size_t>(aHook);
		return index < detail::WritingDefaults.size() ? detail::WritingDefaults[index] : std::string_view{ };
	}
	auto writing_hook_version_of(core::writing_hook aHook) noexcept -> std::string_view
	{
		switch (aHook) {
		case core::writing_hook::feedback:              return review_feedback_hook_version;
		case core::writing_hook::enhance_stop:          return enhance_stop_hook_version;
		case core::writing_hook::enhance_writing:       return enhance_writing_hook_version;
		case core::writing_hook::suggested_comment:     return suggested_comment_hook_version;
		case core::writing_hook::local_description:     return local_description_hook_version;
		case core::writing_hook::local_discussion:      return local_discussion_hook_version;
		case core::writing_hook::local_commit_message:  return local_commit_message_hook_version;
		case core::writing_hook::review_latest_upload:  return review_latest_upload_hook_version;
		case core::writing_hook::review_proposed_fix:   return review_proposed_fix_hook_version;
		case core::writing_hook::review_explain_file:   return review_explain_file_hook_version;
		case core::writing_hook::review_suggest_change: return review_suggest_change_hook_version;
		case core::writing_hook::local_run_check:       return local_run_check_hook_version;
		case core::writing_hook::writing_style:         return writing_style_hook_version;
		case core::writing_hook::comment_labels:        return comment_labels_hook_version;
		case core::writing_hook::comment:               return tour_stop_comment_hook_version;
		case core::writing_hook::outline:               return writing_hook_version;
		}
		return writing_hook_version;
	}
	namespace {
		/// [[BPR-2051]] Indexed by `core::writing_hook`. Written out, never
		/// derived from names or order: an ID once shipped is kept.
		constexpr auto Purposes = std::array<std::string_view, core::writing_hook_count>{
			// [[BPR-2274]] "tour.problem" and "tour.solution" are retired.
			"tour.outline",
			"tour.stop-comment",
			"review.feedback",
			"enhance.tour-stop",
			"enhance.writing",
			"review.suggested-comment",
			"local.description",
			"local.discussion",
			"local.commit-message",
			"review.latest-upload",
			"review.proposed-fix",
			"review.explain-file",
			"review.suggest-change",
			"local-draft.run-check",
			// [[BPR-2264]] A Feature's text, by the Feature's name.
			"feature.writing-style",
			// [[BPR-2270]]
			"feature.comment-labels",
		};
	}
	auto writing_hook_purpose(const core::writing_hook aHook) noexcept -> std::string_view
	{
		const auto index = static_cast<std::size_t>(aHook);
		return index < Purposes.size() ? Purposes[index] : std::string_view{ };
	}
	auto writing_hook_of_purpose(const std::string_view aPurpose) noexcept -> std::optional<core::writing_hook>
	{
		for (std::size_t index = 0; index < Purposes.size(); ++index) {
			if (Purposes[index] == aPurpose) {
				return static_cast<core::writing_hook>(index);
			}
		}
		// Owner look, 2026-11-10: one Enhance writing prompt took the place
		// of the comment's and the note's, so their IDs name it.
		if (aPurpose == "enhance.comment" || aPurpose == "enhance.note") {
			return core::writing_hook::enhance_writing;
		}
		return std::nullopt;
	}
	auto default_writing_hook_digest(const core::writing_hook aHook) -> std::array<std::uint8_t, 32> { return core::writing_digest(default_writing_hook(aHook)); }
	auto writing_application_contract() -> std::string_view { return detail::WritingContract; }
	auto effective_writing_hook(const core::writing_hook_settings& aSettings, core::writing_hook aHook) -> std::string_view
	{
		const auto index = static_cast<std::size_t>(aHook);
		if (index >= aSettings.Overrides.size() || !core::valid(aSettings)) {
			return { };
		}
		return aSettings.Overrides[index].empty() ? default_writing_hook(aHook) : aSettings.Overrides[index];
	}
	auto workshop_feedback_guidance(const core::writing_hook_settings& aSettings, review_label_helper aHelper) -> std::string_view
	{
		return aHelper == review_label_helper::on ? effective_writing_hook(aSettings, core::writing_hook::feedback) : std::string_view{ };
	}
	static_assert(core::maximum_writing_instruction_bytes <= core::maximum_feedback_guidance_bytes, "every valid feedback override fits a workshop request");
	static_assert(review_feedback_hook_version.size() <= core::maximum_feedback_guidance_version_bytes);
	auto report_templates_guidance() -> core::supplement_feedback_guidance
	{
		return core::supplement_feedback_guidance{ .Source = core::feedback_guidance_source::shipped, .Version = std::string{ report_templates_version }, .Text = std::string{ detail::ReportTemplates } };
	}
	auto suggested_comment_format(const core::writing_hook_settings& aSettings) -> std::optional<core::supplement_feedback_guidance>
	{
		const auto text = effective_writing_hook(aSettings, core::writing_hook::suggested_comment);
		if (text.empty()) {
			return std::nullopt;
		}
		return core::supplement_feedback_guidance{
			.Source  = aSettings.Overrides[static_cast<std::size_t>(core::writing_hook::suggested_comment)].empty() ? core::feedback_guidance_source::shipped : core::feedback_guidance_source::profile_override,
			.Version = std::string{ suggested_comment_hook_version },
			.Text    = std::string{ text },
		};
	}
	auto writing_style_preamble() noexcept -> std::string_view
	{
		return "Writing style (the /bpr:writing-style Feature). It governs the wording of the prose a person reads in your answer. The request's own instructions, "
			   "and any output format, schema, template or block they give, win over it on any conflict. In a JSON answer it governs only the text inside string values, "
			   "never keys, structure or other fields.\n\n";
	}
	auto writing_style_instructions(const core::writing_hook_settings& aSettings) -> std::string
	{
		auto text = effective_writing_hook(aSettings, core::writing_hook::writing_style);
		if (text.empty()) {
			text = default_writing_hook(core::writing_hook::writing_style);
		}
		return std::string{ writing_style_preamble() } + std::string{ text };
	}
	auto writing_style_for(const core::writing_hook_settings& aSettings, const bool aHumanAudience, const std::span<const std::string_view> aOwnTexts) -> std::string
	{
		return prompt_features_for(aSettings, { .HumanAudience = aHumanAudience }, aOwnTexts);
	}
	auto comment_labels_preamble() noexcept -> std::string_view
	{
		return "Comment labels (the /bpr:comment-labels Feature). It governs how each comment you write for the reviewer to post begins: a finding's suggested comment, "
			   "a reply written to be posted, a comment you draft or edit. It does not apply to the rest of your answer.\n\n";
	}
	auto comment_labels_instructions(const core::writing_hook_settings& aSettings) -> std::string
	{
		const auto index = static_cast<std::size_t>(core::writing_hook::comment_labels);
		// [[BPR-2270]], owner 2026-11-18: a blanked convention is plain comments.
		if (!core::valid(aSettings) || (!aSettings.Overrides[index].empty() && core::writing_blank(aSettings.Overrides[index]))) {
			return { };
		}
		return std::string{ comment_labels_preamble() } + std::string{ effective_writing_hook(aSettings, core::writing_hook::comment_labels) };
	}
	auto prompt_features_for(const core::writing_hook_settings& aSettings, const prompt_feature_audience aAudience, const std::span<const std::string_view> aOwnTexts) -> std::string
	{
		const auto named  = core::referenced_features(aOwnTexts);
		const auto names  = [&named](const std::string_view aFeature) { return std::ranges::find(named, aFeature) != named.end(); };
		auto       text   = std::string{ };
		const auto labels = aAudience.PostedComments || names(core::comment_labels_feature) ? comment_labels_instructions(aSettings) : std::string{ };
		const auto style  = aAudience.HumanAudience || names(core::writing_style_feature) ? writing_style_instructions(aSettings) : std::string{ };
		for (const auto* const part : { &labels, &style }) {
			if (!part->empty()) {
				text += text.empty() ? *part : "\n\n" + *part;
			}
		}
		return text;
	}
	auto workshop_feedback_supplement(const core::writing_hook_settings& aSettings, review_label_helper aHelper) -> std::optional<core::supplement_feedback_guidance>
	{
		const auto text = workshop_feedback_guidance(aSettings, aHelper);
		if (text.empty()) {
			return std::nullopt;
		}
		auto guidance    = core::supplement_feedback_guidance{ };
		guidance.Source  = aSettings.Overrides[static_cast<std::size_t>(core::writing_hook::feedback)].empty() ? core::feedback_guidance_source::shipped : core::feedback_guidance_source::profile_override;
		guidance.Version = std::string{ review_feedback_hook_version };
		guidance.Text    = std::string{ text };
		return guidance;
	}
}
