#include "bpr/application/local_presentation.hpp"

#include "bpr/core/local_presentation.hpp"

#include <utility>

namespace bpr::application {
	auto present_local_comparison(const std::string_view aSubject, const std::string_view aPath, const std::optional<std::span<const std::uint8_t>>& aLeft,
								  const std::optional<std::span<const std::uint8_t>>& aRight, std::shared_ptr<diff_line_matcher_port> aMatcher, const std::uint64_t aSequence,
								  const std::stop_token aStop) -> std::optional<local_presentation>
	{
		const auto options = core::diff_options{ };
		const auto key     = core::local_presentation_key(aSubject, aPath, aLeft, aRight, options);
		if (!key || aSequence == 0U) {
			return std::nullopt;
		}
		const auto text = [](const std::optional<std::span<const std::uint8_t>>& aBytes) -> std::shared_ptr<const std::string> {
			return aBytes ? std::make_shared<const std::string>(aBytes->begin(), aBytes->end()) : nullptr;
		};
		auto presented = local_presentation{
			// One window session: the presentation's own sequence orders it.
			.Context = { .Request = core::request_id{ aSequence }, .ApplicationSession = core::application_session_id{ 1U }, .Key = *key, .Generation = core::request_generation{ aSequence } },
			.Path    = std::string{ aPath },
			.Left    = text(aLeft),
			.Right   = text(aRight),
		};
		if (!aMatcher) {
			presented.Reason = core::diff_reason_code::host_failure;
			return presented;
		}
		auto check    = stop_token_check{ aStop };
		auto matching = abandonable_diff_line_matching{ std::move(aMatcher), aStop };
		auto run      = core::compute_file_diff({ .Key = *key, .Options = options, .LeftBytes = aLeft, .RightBytes = aRight, .Limits = core::diff_compute_limits{ } },
												core::diff_compute_control{ .Stop = &check }, matching);
		if (!run.Outcome || aStop.stop_requested()) {
			return std::nullopt;
		}
		presented.Reason = run.Outcome->Reason;
		if (run.Outcome->Result) {
			auto result = std::make_shared<const core::computed_file_diff>(std::move(*run.Outcome->Result));
			if (auto index = core::build_diff_semantic_index(result)) {
				presented.Result = std::move(result);
				presented.Index  = std::make_shared<const core::diff_semantic_index>(std::move(*index));
			} else {
				presented.Reason = core::diff_reason_code::invalid_alignment;
			}
		}
		return presented;
	}
}
