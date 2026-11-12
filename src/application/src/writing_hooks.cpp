#include "bpr/application/writing_hooks.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "writing_hooks_data.hpp"
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
		case core::writing_hook::feedback:          return review_feedback_hook_version;
		case core::writing_hook::enhance_stop:      return enhance_stop_hook_version;
		case core::writing_hook::enhance_writing:   return enhance_writing_hook_version;
		case core::writing_hook::suggested_comment: return suggested_comment_hook_version;
		case core::writing_hook::local_description: return local_description_hook_version;
		case core::writing_hook::local_discussion:  return local_discussion_hook_version;
		case core::writing_hook::local_commit_message: return local_commit_message_hook_version;
		case core::writing_hook::comment:           return tour_stop_comment_hook_version;
		case core::writing_hook::problem:
		case core::writing_hook::solution:
		case core::writing_hook::outline:           return writing_hook_version;
		}
		return writing_hook_version;
	}
	namespace {
		/// [[BPR-2051]] Indexed by `core::writing_hook`. Written out, never
		/// derived from names or order: an ID once shipped is kept.
		constexpr auto Purposes = std::array<std::string_view, core::writing_hook_count>{
			"tour.problem",
			"tour.solution",
			"tour.outline",
			"tour.stop-comment",
			"review.feedback",
			"enhance.tour-stop",
			"enhance.writing",
			"review.suggested-comment",
			"local.description",
			"local.discussion",
			"local.commit-message",
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
