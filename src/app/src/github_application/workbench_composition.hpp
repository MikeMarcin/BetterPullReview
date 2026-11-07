#ifndef BPR_APP_GITHUB_APPLICATION_WORKBENCH_COMPOSITION_HPP
#define BPR_APP_GITHUB_APPLICATION_WORKBENCH_COMPOSITION_HPP
#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/application/commit_filter_model.hpp"
#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/application/github_epoch_handoff.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/application/workbench_snapshot_observation.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/epoch_model.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Appearance, workbench-review and diff projections the window composes,
// and the reviewer decision lane.
namespace bpr::app {
	[[nodiscard]] auto preserves_selected_diff(
		const core::epoch_state&                          aState,
		const application::accepted_pull_request_handoff& aHandoff) noexcept
		-> bool;

	[[nodiscard]] auto workbench_authority(
		const application::accepted_pull_request_handoff& aHandoff) noexcept
		-> application::workbench_pull_request_authority;

	/// \brief Routes appearance projections to the surfaces that consume them.
	///
	/// \details BPR-0182 composed the host, BPR-0183 made the appearance
	/// profile drive the composed window, and BPR-0185 gives the classified
	/// spans somewhere to go. `github_window` still does not implement
	/// `appearance_projection_port`, so the presentation invalidation half of
	/// a projection is counted rather than routed; the span half is handed to
	/// the sink the composition installs.
	class composition_appearance_projections final
		: public application::appearance_projection_port
	{
	public:
		using span_sink
			= std::function<void(const application::token_span_publication&)>;
		using presentation_sink     = std::function<void(
			const application::appearance_presentation_invalidation&)>;
		using theme_preview_sink    = std::function<void(
			const application::theme_preview_publication&)>;
		using palette_adoption_sink = std::function<void(
			const application::palette_adoption_publication&)>;
		using refusal_sink          = std::function<void(
			const application::appearance_failure_publication&)>;

		void set_span_sink(span_sink aSink);

		void set_presentation_sink(presentation_sink aSink);

		void set_theme_preview_sink(theme_preview_sink aSink);

		void set_refusal_sink(refusal_sink aSink);

		void set_palette_adoption_sink(palette_adoption_sink aSink);

		void set_theme_export_sink(std::function<void()> aSink);

		void publish(const application::appearance_projection& aProjection) override;

		void fail(const application::appearance_failure_publication& aFailure) override;

		[[nodiscard]] auto published() const noexcept -> std::size_t;

		[[nodiscard]] auto failed() const noexcept -> std::size_t;

	private:
		std::function<void()> ThemeExportSink;
		span_sink             SpanSink;
		presentation_sink     PresentationSink;
		theme_preview_sink    ThemePreviewSink;
		refusal_sink          RefusalSink;
		palette_adoption_sink PaletteAdoptionSink;
		std::size_t           Published = 0U;
		std::size_t           Failed    = 0U;
	};

	/// \brief Retains the exact accepted review observed by the workbench host.
	///
	/// \details The GitHub host owns the accepted snapshot, but the workbench
	/// observation runs on its own effect lane. Copying the already bounded file
	/// list here gives that worker immutable facts without reading mutable host
	/// state across lanes. The complete pull-request authority is compared on
	/// every read; a request for any other identity receives no files.
	class composition_workbench_review final
	{
	public:
		void adopt(
			const application::accepted_pull_request_handoff&               aHandoff,
			const std::optional<application::github_pull_request_snapshot>& aSnapshot) noexcept;

		[[nodiscard]] auto files(
			const application::workbench_pull_request_authority& aAuthority) const
			-> std::optional<std::vector<application::github_file_record>>;

		/// \brief RFC-BPR-0089. Adopts the commit filter's file set, or none.
		/// \returns Whether the files the workbench observes changed.
		auto filter(std::optional<std::vector<application::commit_filter_file>> aFiles) -> bool;

		/// \brief RFC-BPR-0089. The file whose filtered diff's receipt is kept
		/// under `aReceipt`, or `aReceipt` itself when it names a file.
		[[nodiscard]] auto receipt_owner(const core::file_identity aReceipt) const -> core::file_identity;

		/// \brief RFC-BPR-0089 M3. Whether `aReceipt` is the receipt of one of a
		/// split file's per-commit diffs.
		[[nodiscard]] auto split_receipt(const core::file_identity aReceipt) const -> bool;

		/// \brief RFC-BPR-0089. How the commit filter forms one file's diff.
		[[nodiscard]] auto file_filter(
			const application::workbench_pull_request_authority& aAuthority,
			const core::file_identity                            aFile) const
			-> std::optional<application::snapshot_workbench_observation::file_filter>;

		/// \brief Tombstones one exact authority and returns it for host invalidation.
		[[nodiscard]] auto invalidate(
			const application::pull_request_authority_invalidation& aInvalidation) noexcept
			-> std::optional<application::workbench_pull_request_authority>;

#if defined(BPR_BUILD_TESTING)
		[[nodiscard]] auto active() const noexcept -> bool;

		[[nodiscard]] auto exact(
			const application::accepted_pull_request_handoff& aHandoff) const noexcept
			-> bool;
		void make_unavailable(
			const application::workbench_pull_request_authority& aAuthority) noexcept;
#endif

	private:
		struct accepted_review
		{
			application::workbench_pull_request_authority Authority;
			std::vector<application::github_file_record>  Files;
			bool                                          Available = false;
			bool                                          Active    = false;
			/// RFC-BPR-0089. The commit filter's file set while it applies.
			std::optional<std::vector<application::commit_filter_file>> Filtered;
		};

		[[nodiscard]] auto matches(
			const application::pull_request_authority_invalidation& aInvalidation) const noexcept
			-> bool;

		mutable std::mutex             Mutex;
		std::optional<accepted_review> Review;
	};

	class composition_diff_projection final
		: public application::diff_projection_port
	{
	public:
		using publication_sink = std::function<void(
			const application::diff_publication&)>;
		using failure_sink     = std::function<void(const application::diff_failure_publication&)>;

		composition_diff_projection(
			application::diff_projection_port& aTarget,
			publication_sink                   aPublication,
			failure_sink                       aFailure = { });

		void publish(const application::diff_publication& aPublication) override;

		void fail(const application::diff_failure_publication& aFailure) override;

		void invalidate(
			const core::pull_request_authority& aAuthority) override;

	private:
		application::diff_projection_port* Target = nullptr;
		publication_sink                   Publication;
		failure_sink                       Failure;
	};

	/// \brief Forwards selected diff terminals and observes accepted availability.
	/// \brief [[BPR-1344]]. The decision lane of the reviewer's own hosts
	/// (epoch, diff, workbench): after each task they post, the application
	/// may start a preparation round it deferred while the reviewer had work.
	/// The lane is the profile's and outlives a window, so a task run after
	/// the window is gone finds its callback gone too.
	class reviewer_decision_lane final : public application::decision_executor
	{
	public:
		reviewer_decision_lane(application::decision_executor& aLane, std::function<void()> aAfter);

		void post(std::function<void()> aTask) override;

	private:
		application::decision_executor&        Lane;
		std::shared_ptr<std::function<void()>> After;
	};
}
#endif
