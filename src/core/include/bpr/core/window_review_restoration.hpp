#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_WINDOW_REVIEW_RESTORATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_WINDOW_REVIEW_RESTORATION_HPP

#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/github_model.hpp"
#include "bpr/core/profile_window.hpp"
#include <expected>
#include <optional>

namespace bpr::core {
	struct window_review_authority
	{
		github_access_mode Mode = github_access_mode::authenticated;
		std::string        Hostname;
		auto               operator==(const window_review_authority&) const -> bool = default;
	};
	struct window_review_destination
	{
		github_access_mode  Mode = github_access_mode::authenticated;
		std::string         Hostname;
		repository_locator  Repository;
		pull_request_number Number;
		auto                operator==(const window_review_destination&) const -> bool = default;
	};
	struct window_review_record
	{
		bpr_window_id             Window;
		window_review_destination Destination;
		auto                      operator==(const window_review_record&) const -> bool = default;
	};
	enum class window_review_failure : std::uint8_t { invalid_state,
													  invalid_destination,
													  missing_server,
													  conflicting_authority,
													  unavailable,
													  cancelled,
													  identity_exhausted,
													  corrupt,
													  unsupported_schema,
													  conflict };
	struct window_review_startup_request
	{
		request_id                             Request;
		std::optional<window_review_authority> Authority;
	};
	struct window_review_open_request
	{
		bpr_window_context        Context;
		request_id                Request;
		window_review_destination Destination;
	};
	struct window_review_error
	{
		std::optional<bpr_window_context> Context;
		window_review_failure             Reason;
	};
	struct window_review_actions
	{
		std::optional<window_review_startup_request> Startup;
		std::vector<window_review_open_request>      Opens;
		std::vector<window_review_error>             Failures;
	};
	[[nodiscard]] auto valid(const window_review_authority& aAuthority) noexcept -> bool;
	[[nodiscard]] auto valid(const window_review_destination& aDestination) noexcept -> bool;
	[[nodiscard]] auto valid_window_review_records(const std::vector<window_review_record>& aRecords) noexcept -> bool;

	/// Pure exact-context restoration policy. Input contexts are least to most recently focused.
	class window_review_restoration final
	{
	public:
		window_review_restoration(bpr_profile_id aProfile, std::vector<bpr_window_context> aContexts, std::vector<std::string> aConfiguredHosts);
		[[nodiscard]] auto load(const std::vector<window_review_record>& aRecords) -> window_review_actions;
		[[nodiscard]] auto attach(bpr_window_context aContext) -> bool;
		[[nodiscard]] auto current(bpr_window_context aContext) const noexcept -> bool;
		[[nodiscard]] auto mark_window_ready(bpr_window_context aContext) -> window_review_actions;
		void               cancel_restore(bpr_window_context aContext);
		void               cancel_startup();
		[[nodiscard]] auto current_startup(request_id aRequest) const noexcept -> bool;
		[[nodiscard]] auto complete_startup(request_id aRequest, const std::expected<window_review_authority, window_review_failure>& aResult) -> window_review_actions;
		[[nodiscard]] auto current_restore(bpr_window_context aContext, request_id aRequest) const noexcept -> bool;
		[[nodiscard]] auto complete_restore(bpr_window_context aContext, request_id aRequest, const std::expected<void, window_review_failure>& aResult) -> window_review_actions;
		[[nodiscard]] auto accepted_read(bpr_window_context aContext, const window_review_destination& aDestination) -> bool;
		[[nodiscard]] auto close(bpr_window_context aContext) -> bool;
		[[nodiscard]] auto records() const noexcept -> const std::vector<window_review_record>&;

	private:
		struct slot
		{
			bpr_window_context                       Context;
			bool                                     Cancelled = false;
			bool                                     Ready     = false;
			std::optional<request_id>                Request;
			std::optional<window_review_destination> Pending;
		};
		bpr_profile_id                         Profile;
		std::vector<slot>                      Slots;
		std::vector<std::string>               ConfiguredHosts;
		std::vector<window_review_record>      Records;
		std::optional<request_id>              Startup;
		std::optional<window_review_authority> Chosen;
		std::uint64_t                          NextRequest       = 1;
		std::uint64_t                          LastWindowId      = 0;
		bool                                   Initializing      = true;
		bool                                   Loaded            = false;
		bool                                   StartupSuppressed = false;
		[[nodiscard]] auto                     open_ready() -> window_review_actions;
	};
}
#endif
