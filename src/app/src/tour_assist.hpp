#ifndef BPR_APP_TOUR_ASSIST_HPP
#define BPR_APP_TOUR_ASSIST_HPP
#include "bpr/application/review_tour_host.hpp"
#include "bpr/application/tour_assist.hpp"
#include "tour_cli_types.hpp"
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
namespace bpr::app {
	/// \brief The wall clock in Unix seconds, read only where a ticket is issued or answered.
	[[nodiscard]] inline auto tour_assist_now() -> std::int64_t
	{
		return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}
	/// \brief A managed host's cooperative statement about sending draft text to an agent it cannot verify.
	/// `manual-only` refuses every assist and outline command and leaves manual authoring untouched; an
	/// unrecognised value refuses too. It is a host setting, not a sandbox: BPR cannot verify who calls it.
	enum class tour_assist_export_policy : std::uint8_t { open,
														  manual_only,
														  invalid };
	[[nodiscard]] auto tour_assist_policy() -> tour_assist_export_policy;
	[[nodiscard]] auto tour_hook_name(core::writing_hook aHook) -> std::string_view;
	[[nodiscard]] auto parse_tour_hook(std::string_view aName) -> std::optional<core::writing_hook>;
	/// \brief What the author or their agent asked to be written. The agent never supplies any of it at completion.
	struct tour_assist_query
	{
		core::writing_hook                                     Hook = core::writing_hook::problem;
		core::tour_stop_id                                     Stop;
		std::optional<std::pair<std::uint64_t, std::uint64_t>> Range;
		std::optional<std::string>                             Quote;
		std::string                                            Session;
	};
	struct tour_assist_issued
	{
		application::tour_assist_ticket Ticket;
		std::string                     ContextJson;
	};
	/// \brief Resolve the exact target and range, assemble the minimum context from the draft, take the
	/// effective hook, and issue the draft's one live ticket. Reads nothing from a provider and launches no model.
	/// `aExtra` is caller-materialized source evidence with its destinations, e.g. a stop's pinned source.
	[[nodiscard]] auto tour_assist_issue(application::review_tour_host& aHost, const core::writing_hook_settings& aSettings, const tour_assist_query& aQuery, std::vector<application::writing_source> aExtra, std::int64_t aNow) -> tour_cli_result<tour_assist_issued>;
	/// \brief The JSON the caller prints for a completed request: what was applied, notes, and any proposal.
	[[nodiscard]] auto tour_assist_outcome_json(const application::tour_service_result& aResult, const application::review_tour_host& aHost) -> std::string;
	/// \brief Effective hooks, with the shipped version and each override's revision.
	[[nodiscard]] auto tour_hooks_json(const core::writing_hook_settings& aSettings) -> std::string;
	/// \brief Reads and rewrites the profile-owned hook overrides. Only the profile owner may call these.
	[[nodiscard]] auto load_tour_hook_settings(const std::filesystem::path& aRoot) -> tour_cli_result<core::writing_hook_settings>;
	[[nodiscard]] auto store_tour_hook_override(const std::filesystem::path& aRoot, core::writing_hook aHook, std::string aInstructions) -> tour_cli_result<core::writing_hook_settings>;
}
#endif
