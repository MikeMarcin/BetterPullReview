#include "github_application/workbench_composition.hpp"

#include "bpr/core/github_model.hpp"
#include "bpr/core/model.hpp"

#include <algorithm>

namespace bpr::app {
	auto preserves_selected_diff(
		const core::epoch_state&                          aState,
		const application::accepted_pull_request_handoff& aHandoff) noexcept
		-> bool
	{
		if (
			!aState.Authority.has_value()
			|| !aState.AcceptedRevision.has_value()
			|| !aState.AcceptedComparison.has_value()) {
			return false;
		}
		const auto& authority = *aState.Authority;
		const auto& revision  = *aState.AcceptedRevision;
		return aState.ApplicationSession == aHandoff.ApplicationSession
			&& authority.Profile == aHandoff.Profile
			&& authority.ProfileRevision == aHandoff.ProfileRevision
			&& authority.Host == aHandoff.Host
			&& authority.HostAuthority == aHandoff.HostAuthority
			&& authority.Account == aHandoff.Account
			&& authority.Viewer == aHandoff.Viewer
			&& authority.Repository == aHandoff.Repository
			&& authority.PullRequest == aHandoff.PullRequest
			&& authority.Number == aHandoff.Number
			&& revision.Source.IdentityGeneration
			== aHandoff.IdentityGeneration
			&& revision.Origin == aHandoff.Origin
			&& revision.BaseBranch == aHandoff.BaseBranch
			&& revision.Base == aHandoff.Base
			&& revision.Head == aHandoff.Head
			&& revision.CurrentViewerReview == aHandoff.CurrentViewerReview
			&& revision.Base.algorithm() == aHandoff.ObjectFormat;
	}

	auto workbench_authority(
		const application::accepted_pull_request_handoff& aHandoff) noexcept
		-> application::workbench_pull_request_authority
	{
		return {
			.ApplicationSession    = aHandoff.ApplicationSession,
			.Profile               = aHandoff.Profile,
			.ProfileRevision       = aHandoff.ProfileRevision,
			.Host                  = aHandoff.Host,
			.HostAuthority         = aHandoff.HostAuthority,
			.Account               = aHandoff.Account,
			.Viewer                = aHandoff.Viewer,
			.Repository            = aHandoff.Repository,
			.PullRequest           = aHandoff.PullRequest,
			.Number                = aHandoff.Number,
			.Snapshot              = aHandoff.Snapshot,
			.IdentityGeneration    = aHandoff.IdentityGeneration,
			.PullRequestGeneration = aHandoff.PullRequestGeneration,
		};
	}

	void composition_appearance_projections::set_span_sink(span_sink aSink)
	{
		SpanSink = std::move(aSink);
	}

	void composition_appearance_projections::set_presentation_sink(presentation_sink aSink)
	{
		PresentationSink = std::move(aSink);
	}

	void composition_appearance_projections::set_theme_preview_sink(theme_preview_sink aSink)
	{
		ThemePreviewSink = std::move(aSink);
	}

	void composition_appearance_projections::set_refusal_sink(refusal_sink aSink)
	{
		RefusalSink = std::move(aSink);
	}

	void composition_appearance_projections::set_palette_adoption_sink(palette_adoption_sink aSink)
	{
		PaletteAdoptionSink = std::move(aSink);
	}

	void composition_appearance_projections::set_theme_export_sink(std::function<void()> aSink)
	{
		ThemeExportSink = std::move(aSink);
	}

	void composition_appearance_projections::publish(const application::appearance_projection& aProjection)
	{
		++Published;
		if (aProjection.ThemeExported && ThemeExportSink) {
			ThemeExportSink();
		}
		if (aProjection.Spans.has_value() && SpanSink) {
			SpanSink(*aProjection.Spans);
		}
		// The host publishes a bounded invalidation naming which domains
		// changed. What actually changed is read from the host's own
		// settings rather than reconstructed from the invalidation,
		// because the invalidation says a domain moved and not what it
		// moved to - and the record is the thing a surface has to show.
		if (aProjection.Presentation.has_value() && PresentationSink) {
			PresentationSink(*aProjection.Presentation);
		}
		if (aProjection.ThemePreview.has_value() && ThemePreviewSink) {
			ThemePreviewSink(*aProjection.ThemePreview);
		}
		if (aProjection.PaletteAdoption.has_value() && PaletteAdoptionSink) {
			PaletteAdoptionSink(*aProjection.PaletteAdoption);
		}
	}

	void composition_appearance_projections::fail(const application::appearance_failure_publication& aFailure)
	{
		++Failed;
		if (RefusalSink) {
			RefusalSink(aFailure);
		}
	}

	auto composition_appearance_projections::published() const noexcept -> std::size_t
	{
		return Published;
	}

	auto composition_appearance_projections::failed() const noexcept -> std::size_t
	{
		return Failed;
	}

	void composition_workbench_review::adopt(
		const application::accepted_pull_request_handoff&               aHandoff,
		const std::optional<application::github_pull_request_snapshot>& aSnapshot) noexcept
	{
		auto files     = std::vector<application::github_file_record>{ };
		auto available = false;
		try {
			if (aSnapshot.has_value()
				&& aSnapshot->Profile == aHandoff.Profile
				&& aSnapshot->Host == aHandoff.Host
				&& aSnapshot->Account == aHandoff.Account
				&& aSnapshot->Snapshot == aHandoff.Snapshot
				&& aSnapshot->Repository.Repository == aHandoff.Repository
				&& aSnapshot->PullRequest.PullRequest == aHandoff.PullRequest
				&& aSnapshot->PullRequest.Number == aHandoff.Number) {
				files     = aSnapshot->Files;
				available = true;
			}
		} catch (...) {
			available = false;
			files.clear();
		}

		const auto lock = std::scoped_lock{ Mutex };
		Review          = accepted_review{
			.Authority = workbench_authority(aHandoff),
			.Files     = std::move(files),
			.Available = available,
			.Active    = true,
		};
	}

	auto composition_workbench_review::files(
		const application::workbench_pull_request_authority& aAuthority) const
		-> std::optional<std::vector<application::github_file_record>>
	{
		const auto lock = std::scoped_lock{ Mutex };
		if (!Review.has_value() || Review->Authority != aAuthority
			|| !Review->Available || !Review->Active) {
			return std::nullopt;
		}
		// RFC-BPR-0089. While a commit filter applies, its file set.
		if (Review->Filtered.has_value()) {
			auto records = std::vector<application::github_file_record>{ };
			records.reserve(Review->Filtered->size());
			for (const auto& file : *Review->Filtered) {
				records.push_back(file.Record);
			}
			return records;
		}
		return Review->Files;
	}

	auto composition_workbench_review::filter(std::optional<std::vector<application::commit_filter_file>> aFiles) -> bool
	{
		const auto lock = std::scoped_lock{ Mutex };
		if (!Review.has_value() || Review->Filtered == aFiles) {
			return false;
		}
		Review->Filtered = std::move(aFiles);
		return true;
	}

	auto composition_workbench_review::receipt_owner(const core::file_identity aReceipt) const -> core::file_identity
	{
		const auto lock = std::scoped_lock{ Mutex };
		if (Review.has_value() && Review->Filtered.has_value()) {
			for (const auto& file : *Review->Filtered) {
				if (file.Filter.has_value()) {
					const auto identity = core::file_identity_of(file.Record.Path);
					if (core::filtered_receipt_file(identity, *file.Filter) == aReceipt) {
						return identity;
					}
				}
			}
		}
		return aReceipt;
	}

	auto composition_workbench_review::split_receipt(const core::file_identity aReceipt) const -> bool
	{
		const auto lock = std::scoped_lock{ Mutex };
		if (!Review.has_value() || !Review->Filtered.has_value()) {
			return false;
		}
		return std::ranges::any_of(*Review->Filtered, [&](const application::commit_filter_file& aFile) {
			const auto identity = core::file_identity_of(aFile.Record.Path);
			return std::ranges::any_of(aFile.Segments, [&](const application::commit_filter_segment& aSegment) {
				return core::filtered_receipt_file(identity, aSegment.Endpoints) == aReceipt;
			});
		});
	}

	auto composition_workbench_review::file_filter(
		const application::workbench_pull_request_authority& aAuthority,
		const core::file_identity                            aFile) const
		-> std::optional<application::snapshot_workbench_observation::file_filter>
	{
		const auto lock = std::scoped_lock{ Mutex };
		if (!Review.has_value() || Review->Authority != aAuthority || !Review->Filtered.has_value()) {
			return std::nullopt;
		}
		for (const auto& file : *Review->Filtered) {
			if ((file.Filter.has_value() || file.Split) && core::file_identity_of(file.Record.Path) == aFile) {
				return application::snapshot_workbench_observation::file_filter{ .Endpoints = file.Filter, .Split = file.Split, .Segments = application::workbench_segments(file.Segments) };
			}
		}
		return std::nullopt;
	}

	auto composition_workbench_review::invalidate(
		const application::pull_request_authority_invalidation& aInvalidation) noexcept
		-> std::optional<application::workbench_pull_request_authority>
	{
		const auto lock = std::scoped_lock{ Mutex };
		if (!matches(aInvalidation)) {
			return std::nullopt;
		}
		Review->Files.clear();
		Review->Filtered.reset();
		Review->Available = false;
		Review->Active    = false;
		return Review->Authority;
	}
#if defined(BPR_BUILD_TESTING)
	auto composition_workbench_review::active() const noexcept -> bool
	{
		const auto lock = std::scoped_lock{ Mutex };
		return Review.has_value() && Review->Active;
	}

	auto composition_workbench_review::exact(
		const application::accepted_pull_request_handoff& aHandoff) const noexcept
		-> bool
	{
		const auto lock = std::scoped_lock{ Mutex };
		return Review.has_value() && Review->Active
			&& Review->Authority == workbench_authority(aHandoff)
			&& Review->Available;
	}

	void composition_workbench_review::make_unavailable(
		const application::workbench_pull_request_authority& aAuthority) noexcept
	{
		const auto lock = std::scoped_lock{ Mutex };
		if (Review.has_value() && Review->Authority == aAuthority) {
			Review->Available = false;
			Review->Files.clear();
		}
	}
#endif
	auto composition_workbench_review::matches(
		const application::pull_request_authority_invalidation& aInvalidation) const noexcept
		-> bool
	{
		if (!Review.has_value()) {
			return false;
		}
		const auto& authority = Review->Authority;
		return authority.ApplicationSession == aInvalidation.ApplicationSession
			&& authority.IdentityGeneration == aInvalidation.IdentityGeneration
			&& authority.Profile == aInvalidation.Profile
			&& authority.ProfileRevision == aInvalidation.ProfileRevision
			&& authority.Host == aInvalidation.Host
			&& authority.HostAuthority == aInvalidation.HostAuthority
			&& authority.Account == aInvalidation.Account
			&& authority.Viewer == aInvalidation.Viewer
			&& authority.Repository == aInvalidation.Repository
			&& authority.PullRequest == aInvalidation.PullRequest;
	}

	composition_diff_projection::composition_diff_projection(
		application::diff_projection_port& aTarget,
		publication_sink                   aPublication,
		failure_sink                       aFailure)
		: Target{ &aTarget }
		, Publication{ std::move(aPublication) }
		, Failure{ std::move(aFailure) }
	{
	}

	void composition_diff_projection::publish(const application::diff_publication& aPublication)
	{
		Target->publish(aPublication);
		if (Publication) {
			Publication(aPublication);
		}
	}

	void composition_diff_projection::fail(const application::diff_failure_publication& aFailure)
	{
		Target->fail(aFailure);
		if (Failure) {
			Failure(aFailure);
		}
	}

	void composition_diff_projection::invalidate(
		const core::pull_request_authority& aAuthority)
	{
		Target->invalidate(aAuthority);
	}

	reviewer_decision_lane::reviewer_decision_lane(application::decision_executor& aLane, std::function<void()> aAfter)
		: Lane{ aLane }
		, After{ std::make_shared<std::function<void()>>(std::move(aAfter)) }
	{
	}

	void reviewer_decision_lane::post(std::function<void()> aTask)
	{
		Lane.post([task = std::move(aTask), after = std::weak_ptr{ After }] {
			task();
			if (const auto live = after.lock()) {
				(*live)();
			}
		});
	}
}
