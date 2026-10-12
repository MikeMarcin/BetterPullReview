#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_WINDOW_REVIEW_RESTORATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_WINDOW_REVIEW_RESTORATION_HPP

#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/diff_semantics.hpp"
#include "bpr/core/github_model.hpp"
#include "bpr/core/profile_window.hpp"
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>
#include <vector>

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
		/// Accepted viewer identity. Zero is anonymous or a legacy unknown owner;
		/// an authenticated destination with unknown ownership is never offered.
		github_account_id Account{ };
		auto              operator==(const window_review_destination&) const -> bool = default;
	};
	/// \brief Exact accepted reading identity; no alias or pixel coordinate is durable.
	struct window_review_position
	{
		comparison_pair                   Comparison;
		std::optional<file_identity>      File;
		std::optional<semantic_selection> Selection;
		auto                              operator==(const window_review_position&) const -> bool = default;
	};
	/// \brief [[BPR-1131]]. The most document tabs one window keeps for one
	/// pull request.
	///
	/// \details RFC-BPR-0025 asks for lightweight inactive tabs with bounded
	/// state. Sixteen views of one pull request is more than a reviewer
	/// compares at once, and it keeps the persisted collection of every
	/// window and every recently viewed entry within one bounded read.
	inline constexpr std::size_t maximum_document_tabs = 16U;
	/// \brief [[BPR-1131]]. A window's document tabs for one pull request, as
	/// the restoration store keeps them.
	///
	/// \details The window's comparison is the pair new tabs take. Each tab
	/// is its exact view: comparison, file and reading position, in tab
	/// order. Tab identities are not durable; a restored collection mints
	/// fresh ones in this order. `Active` names the active tab's index and is
	/// present exactly when there is a tab: closing the last tab leaves the
	/// pull request open with no tab.
	struct window_review_tabs
	{
		comparison_pair                     WindowComparison;
		std::vector<window_review_position> Tabs;
		std::optional<std::uint32_t>        Active;
		auto                                operator==(const window_review_tabs&) const -> bool = default;
	};
	struct window_review_record
	{
		bpr_window_id                         Window;
		window_review_destination             Destination;
		std::optional<window_review_position> Position                                              = std::nullopt;
		std::optional<window_review_tabs>     Tabs                                                  = std::nullopt;
		auto                                  operator==(const window_review_record&) const -> bool = default;
	};
	/// [[BPR-0733]]. Bounds of the per-profile recently viewed list.
	inline constexpr std::size_t maximum_recent_reviews      = 10U;
	inline constexpr std::size_t maximum_recent_review_title = 512U;
	/// \brief One recently viewed pull request: most recent first, at most one
	/// entry per destination. The host supplies the clock reading; zero means
	/// the time is unknown (a migrated row).
	struct window_recent_review
	{
		window_review_destination             Destination;
		std::string                           Title;
		std::int64_t                          ViewedAt = 0;
		std::optional<window_review_position> Position = std::nullopt;
		/// [[BPR-1131]]. The tabs Resume brings back, with the reading above as
		/// the active tab's.
		std::optional<window_review_tabs> Tabs                                                  = std::nullopt;
		std::optional<bool>               Draft                                                 = std::nullopt;
		auto                              operator==(const window_recent_review&) const -> bool = default;
	};
	[[nodiscard]] auto valid(const window_recent_review& aEntry) noexcept -> bool;
	[[nodiscard]] auto valid_recent_reviews(const std::vector<window_recent_review>& aEntries) noexcept -> bool;
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
		bpr_window_context                    Context;
		request_id                            Request;
		window_review_destination             Destination;
		std::optional<window_review_position> Position = std::nullopt;
		std::optional<window_review_tabs>     Tabs     = std::nullopt;
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
	/// \brief Exact session admission for saved metadata and reading. Unknown
	/// authenticated ownership cannot borrow the current account on migration.
	[[nodiscard]] auto matches_review_authority(const window_review_destination& aDestination, const window_review_authority& aAuthority, github_account_id aAccount) noexcept -> bool;
	[[nodiscard]] auto valid(const window_review_position& aPosition) noexcept -> bool;
	/// \brief Whether an exact pair can be a durable comparison: both anchors
	/// available, in one authority, one catalog incarnation and one object format.
	[[nodiscard]] auto valid_exact_pair(const comparison_pair& aPair) noexcept -> bool;
	/// \brief [[BPR-1131]]. A bounded collection whose tabs each name a file and
	/// an exact pair in the window comparison's pull request, with `Active`
	/// present exactly when a tab is.
	[[nodiscard]] auto valid(const window_review_tabs& aTabs) noexcept -> bool;
	enum class window_reading_restore_action : std::uint8_t { wait,
															  select_comparison,
															  select_file,
															  select_location,
															  complete,
															  unavailable };
	/// \brief Bounded facts from the exact window's accepted epoch, file and diff hosts.
	struct window_reading_observation
	{
		bool                       FilesReady    = false;
		bool                       FileAvailable = false;
		bool                       Failed        = false;
		bool                       DiffReady     = false;
		const diff_semantic_index* SemanticIndex = nullptr;
	};
	/// \brief Orders exact restoration and refuses substitutions before presentation.
	class window_reading_restoration final
	{
	public:
		explicit window_reading_restoration(window_review_position aPosition);
		[[nodiscard]] auto advance(const epoch_state& aEpoch, const window_reading_observation& aObservation) -> window_reading_restore_action;
		[[nodiscard]] auto position() const noexcept -> const window_review_position& { return Position; }

	private:
		window_review_position        Position;
		window_reading_restore_action Stage = window_reading_restore_action::select_comparison;
	};
	[[nodiscard]] auto valid_window_review_records(const std::vector<window_review_record>& aRecords) noexcept -> bool;

	/// Pure exact-context restoration policy. Input contexts are least to most recently focused.
	/// [[BPR-0733]]. Loading selects the startup authority only; nothing opens
	/// until a window asks for a recently viewed entry through open_recent.
	class window_review_restoration final
	{
	public:
		window_review_restoration(bpr_profile_id aProfile, std::vector<bpr_window_context> aContexts, std::vector<std::string> aConfiguredHosts);
		[[nodiscard]] auto load(const std::vector<window_review_record>& aRecords, const std::vector<window_recent_review>& aRecent = { }) -> window_review_actions;
		/// \brief A window's gesture on the recently viewed list; the open is
		/// issued once the window is ready and startup has settled.
		[[nodiscard]] auto open_recent(bpr_window_context aContext, std::size_t aIndex) -> window_review_actions;
		/// \brief Records the title once a snapshot of a listed destination was seen.
		[[nodiscard]] auto accepted_title(const window_review_destination& aDestination, std::string_view aTitle) -> bool;
		[[nodiscard]] auto accepted_draft(const window_review_destination& aDestination, std::optional<bool> aDraft) -> bool;
		[[nodiscard]] auto recent() const noexcept -> const std::vector<window_recent_review>& { return Recent; }
		[[nodiscard]] auto attach(bpr_window_context aContext) -> bool;
		[[nodiscard]] auto current(bpr_window_context aContext) const noexcept -> bool;
		[[nodiscard]] auto mark_window_ready(bpr_window_context aContext) -> window_review_actions;
		void               cancel_restore(bpr_window_context aContext);
		void               cancel_startup();
		[[nodiscard]] auto current_startup(request_id aRequest) const noexcept -> bool;
		[[nodiscard]] auto complete_startup(request_id aRequest, const std::expected<window_review_authority, window_review_failure>& aResult) -> window_review_actions;
		[[nodiscard]] auto current_restore(bpr_window_context aContext, request_id aRequest) const noexcept -> bool;
		[[nodiscard]] auto complete_restore(bpr_window_context aContext, request_id aRequest, const std::expected<void, window_review_failure>& aResult) -> window_review_actions;
		[[nodiscard]] auto accepted_read(bpr_window_context aContext, const window_review_destination& aDestination, std::int64_t aViewedAt = 0) -> bool;
		[[nodiscard]] auto accepted_position(bpr_window_context aContext, const window_review_destination& aDestination, const window_review_position& aPosition) -> bool;
		/// \brief [[BPR-1131]]. Retains the window's accepted document tabs for its
		/// accepted destination, and for that destination's recently viewed entry.
		[[nodiscard]] auto accepted_tabs(bpr_window_context aContext, const window_review_destination& aDestination, const window_review_tabs& aTabs) -> bool;
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
			std::optional<window_review_position>    Position = std::nullopt;
			std::optional<window_review_tabs>        Tabs     = std::nullopt;
		};
		bpr_profile_id                         Profile;
		std::vector<slot>                      Slots;
		std::vector<std::string>               ConfiguredHosts;
		std::vector<window_review_record>      Records;
		std::vector<window_recent_review>      Recent;
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
