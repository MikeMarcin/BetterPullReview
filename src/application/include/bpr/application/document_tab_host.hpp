#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DOCUMENT_TAB_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DOCUMENT_TAB_HOST_HPP

#include "bpr/application/document_tabs_projection.hpp"
#include "bpr/application/port.hpp"
#include "bpr/core/document_tabs.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace bpr::application {
	/// \brief [[BPR-1131]]. Presents one tab's exact view.
	///
	/// \details Called on the decision lane with each load the core asks for.
	/// The implementation presents the view through the hosts that already own
	/// comparisons and selected diffs, and answers with
	/// `document_tab_host::complete` for this request, once. It never chooses a
	/// substitute view: a missing file or a pruned anchor is `unavailable`.
	class document_tab_view_port
	{
	public:
		virtual ~document_tab_view_port() = default;

		virtual void load(const core::document_tab_view_request& aRequest) = 0;
	};

	/// \brief [[BPR-1131]]. Owns one window's document tabs for the pull
	/// request it shows.
	///
	/// \details The core decides every tab transition; this host performs
	/// them. It asks the view port to present the active tab, routes each
	/// completion to the load that asked for it, and refuses a completion or a
	/// reading for any other load, so late work cannot reach another tab. It
	/// publishes the tabs after every change and hands the persisted collection
	/// to the restoration owner, which keeps it with the window's reading.
	///
	/// Public operations and inspection are confined to the supplied decision
	/// lane, except `complete` and `observe_reading_position`, which may be
	/// called from anywhere and post their terminal onto that lane. The
	/// referenced ports and lane must stay live until `is_quiescent()`.
	class document_tab_host final
	{
	public:
		using persistence = std::function<void(const core::window_review_tabs&)>;

		document_tab_host(
			decision_executor&             aDecisionLane,
			document_tab_view_port&        aViews,
			document_tabs_projection_port& aProjections,
			persistence                    aPersist = { });
		~document_tab_host();

		document_tab_host(const document_tab_host&)                    = delete;
		auto operator=(const document_tab_host&) -> document_tab_host& = delete;
		document_tab_host(document_tab_host&&)                         = delete;
		auto operator=(document_tab_host&&) -> document_tab_host&      = delete;

		void apply(const core::document_tab_intent& aIntent);
		/// \brief Adopts a persisted collection, as Resume brings it back.
		void restore(const core::window_review_tabs& aTabs);
		/// \brief [[BPR-1478]]. The reviewer chose a file in Review, which the
		/// active tab takes once its comparison settles.
		void observe_file_choice(const core::file_identity& aFile);
		/// \brief The pull request refreshed: the active tab shows its exact
		/// view again, or the file the reviewer chose since ([[BPR-1478]]).
		void refresh();
		/// \brief The window leaves this pull request; identities continue.
		void reset();
		void complete(core::request_id aRequest, core::document_tab_view_outcome aOutcome);
		void observe_reading_position(core::request_id aRequest, std::optional<core::semantic_selection> aSelection);
		/// \brief Refuses later terminals; posted ones already queued become no-ops.
		void shutdown();

		[[nodiscard]] auto state() const noexcept -> const core::document_tabs_state&;
		[[nodiscard]] auto projection() const -> document_tabs_projection;
		[[nodiscard]] auto loads() const noexcept -> std::size_t;
		[[nodiscard]] auto stale_terminals() const noexcept -> std::size_t;
		[[nodiscard]] auto last_reason() const noexcept -> core::document_tab_reason;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DOCUMENT_TAB_HOST_HPP
