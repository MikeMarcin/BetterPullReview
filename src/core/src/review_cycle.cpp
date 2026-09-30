#include "bpr/core/review_cycle.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <ranges>

namespace bpr::core {
	namespace {
		/// Takes the next identity from a counter, refusing past the durable domain.
		auto take_next(std::uint64_t& aCounter) -> std::optional<std::uint64_t>
		{
			if (aCounter == 0 || aCounter > maximum_cycle_identity) {
				return std::nullopt;
			}
			return aCounter++;
		}

		auto valid_title(const std::string_view aTitle) -> bool
		{
			return !aTitle.empty() && aTitle.size() <= maximum_cycle_title_bytes
				&& std::ranges::none_of(aTitle, [](const char aValue) { return aValue == '\0'; });
		}

		auto valid_segment(const std::string_view aSegment) -> bool
		{
			return !aSegment.empty() && aSegment != "." && aSegment != "..";
		}

		auto valid_scope_path(const std::string_view aPath) -> bool
		{
			if (aPath.empty() || aPath.size() > maximum_cycle_scope_path_bytes
				|| std::ranges::any_of(aPath, [](const char aValue) { return aValue == '\0' || aValue == '\\'; })) {
				return false;
			}
			return std::ranges::all_of(aPath | std::views::split('/'), [](const auto& aSegment) {
				return valid_segment(std::string_view{ aSegment.begin(), aSegment.end() });
			});
		}

		/// True when `aPath` equals `aRoot` or lies beneath it.
		auto within(const std::string_view aRoot, const std::string_view aPath) -> bool
		{
			return aPath.starts_with(aRoot) && (aPath.size() == aRoot.size() || aPath[aRoot.size()] == '/');
		}

		auto consistent(const cycle_capture_observation& aCapture) -> bool
		{
			return aCapture.Material.valid() && aCapture.Entries <= maximum_cycle_capture_entries
				&& aCapture.ExplicitDeletions <= maximum_cycle_capture_entries
				&& aCapture.Unavailable <= maximum_cycle_capture_entries
				&& aCapture.Coverage <= cycle_capture_coverage::partial
				&& aCapture.Quality <= cycle_capture_quality::immutable_source;
		}

		/// A repeated capture of the same bytes under the same scope and coverage.
		auto repeats(const std::optional<cycle_source_revision>& aLatest, const cycle_capture_observation& aCapture) -> bool
		{
			return aLatest && aLatest->Capture.Material == aCapture.Material
				&& aLatest->Capture.ScopeVersion == aCapture.ScopeVersion
				&& aLatest->Capture.Coverage == aCapture.Coverage;
		}

		struct change_visitor
		{
			review_cycle_state&   Cycle;
			review_cycle_outcome& Outcome;

			auto operator()(const rename_review_cycle& aChange) const -> cycle_refusal
			{
				if (!valid_title(aChange.Title)) {
					return cycle_refusal::bounds;
				}
				Outcome.Changed = Cycle.Title != aChange.Title;
				Cycle.Title     = aChange.Title;
				return cycle_refusal::none;
			}
			auto operator()(const archive_review_cycle&) const -> cycle_refusal
			{
				Outcome.Changed = Cycle.Lifecycle != cycle_lifecycle::archived;
				Cycle.Lifecycle = cycle_lifecycle::archived;
				return cycle_refusal::none;
			}
			auto operator()(const unarchive_review_cycle&) const -> cycle_refusal
			{
				Outcome.Changed = Cycle.Lifecycle != cycle_lifecycle::active;
				Cycle.Lifecycle = cycle_lifecycle::active;
				return cycle_refusal::none;
			}
			auto operator()(const change_cycle_scope& aChange) const -> cycle_refusal
			{
				if (const auto refusal = validate_scope_paths(aChange.Paths); refusal != cycle_refusal::none) {
					return refusal;
				}
				if (file_bound(Cycle) && !aChange.Paths.empty()) {
					return cycle_refusal::invalid_scope;
				}
				if (aChange.Paths == Cycle.Scope.Paths) {
					return cycle_refusal::none;
				}
				if (Cycle.Scope.Version >= maximum_cycle_identity) {
					return cycle_refusal::exhausted;
				}
				Cycle.Scope     = { .Version = Cycle.Scope.Version + 1, .Paths = aChange.Paths };
				Outcome.Changed = true;
				return cycle_refusal::none;
			}
			auto operator()(const record_cycle_capture& aChange) const -> cycle_refusal
			{
				if (aChange.Capture.ScopeVersion != Cycle.Scope.Version || !consistent(aChange.Capture)) {
					return cycle_refusal::invalid_capture;
				}
				if (aChange.Capture.WorkspaceVersion != (Cycle.Workspace ? Cycle.Workspace->Version : 0)) {
					return cycle_refusal::stale_workspace;
				}
				if (!aChange.Deliberate && repeats(Cycle.Latest, aChange.Capture)) {
					return cycle_refusal::none;
				}
				const auto id = take_next(Cycle.Next.SourceRevision);
				if (!id) {
					return cycle_refusal::exhausted;
				}
				auto revision = cycle_source_revision{
					.Id      = { .Cycle = Cycle.Subject.Cycle, .Id = cycle_source_revision_id{ *id } },
					.Parent  = Cycle.Latest ? std::optional{ Cycle.Latest->Id.Id } : std::nullopt,
					.Capture = aChange.Capture,
				};
				if (!Cycle.Initial) {
					Cycle.Initial = revision;
				}
				Outcome.Captured = revision.Id;
				Cycle.Latest     = std::move(revision);
				Outcome.Changed  = true;
				return cycle_refusal::none;
			}
			auto operator()(const bind_cycle_workspace& aChange) const -> cycle_refusal
			{
				if (!valid(aChange.Root) || (aChange.Root.Kind == workspace_root_kind::file && !Cycle.Scope.Paths.empty())) {
					return cycle_refusal::invalid_workspace;
				}
				const auto& current = Cycle.Workspace;
				if (current && directory_root(current->Root.Kind) != directory_root(aChange.Root.Kind)) {
					return cycle_refusal::invalid_workspace;
				}
				if (current && current->Root.Location == aChange.Root.Location && verify_workspace(*current, aChange.Root) == workspace_verification::matches) {
					return cycle_refusal::none;
				}
				if ((current || Cycle.Latest) && !aChange.Confirmed) {
					return cycle_refusal::unconfirmed;
				}
				if (Cycle.Latest && !aChange.Check) {
					return cycle_refusal::unconfirmed;
				}
				const auto version = current ? current->Version : 0;
				if (version >= maximum_cycle_identity) {
					return cycle_refusal::exhausted;
				}
				Cycle.Workspace = cycle_workspace_binding{
					.Project   = current ? current->Project : derive_workspace_project(Cycle.Subject),
					.Version   = version + 1,
					.Root      = aChange.Root,
					.Admission = Cycle.Latest ? aChange.Check : std::nullopt,
				};
				Outcome.Changed = true;
				return cycle_refusal::none;
			}

			[[nodiscard]] static auto file_bound(const review_cycle_state& aCycle) -> bool
			{
				return aCycle.Workspace && aCycle.Workspace->Root.Kind == workspace_root_kind::file;
			}
		};

		auto admits(const review_cycle_state& aCycle, const review_cycle_change& aChange) -> cycle_refusal
		{
			if (aCycle.Lifecycle == cycle_lifecycle::archived
				&& !std::holds_alternative<unarchive_review_cycle>(aChange)
				&& !std::holds_alternative<archive_review_cycle>(aChange)) {
				return cycle_refusal::archived;
			}
			return cycle_refusal::none;
		}
	}

	auto valid(const local_cycle_authority& aAuthority) noexcept -> bool
	{
		return aAuthority.Profile.value() != 0 && aAuthority.Incarnation.valid();
	}

	auto valid(const local_cycle_subject& aSubject) noexcept -> bool
	{
		return valid(aSubject.Authority) && valid_cycle_id(aSubject.Cycle) && aSubject.Generation > 0
			&& aSubject.Generation <= maximum_cycle_identity;
	}

	auto derive_workspace_project(const local_cycle_subject& aSubject) -> workspace_project_id
	{
		constexpr auto domain = std::string_view{ "BPR-workspace-project-v1" };
		auto           bytes  = std::vector<std::uint8_t>(domain.begin(), domain.end());
		bytes.push_back(0);
		const auto& incarnation = aSubject.Authority.Incarnation.bytes();
		bytes.insert(bytes.end(), incarnation.begin(), incarnation.end());
		for (const auto value : { aSubject.Authority.Profile.value(), aSubject.Cycle.value(), aSubject.Generation }) {
			for (auto shift = 56; shift >= 0; shift -= 8) {
				bytes.push_back(static_cast<std::uint8_t>(static_cast<std::uint64_t>(value) >> static_cast<unsigned>(shift)));
			}
		}
		return workspace_project_id{ sha256(bytes) };
	}

	auto valid_cycle_path(const std::string_view aPath) -> bool
	{
		return valid_scope_path(aPath);
	}

	auto validate_scope_paths(const std::vector<std::string>& aPaths) -> cycle_refusal
	{
		const auto bytes = std::ranges::fold_left(aPaths, std::size_t{ 0 }, [](const std::size_t aTotal, const std::string& aPath) {
			return aTotal + std::min(aPath.size(), maximum_cycle_scope_path_bytes + 1);
		});
		if (aPaths.size() > maximum_cycle_scope_paths || bytes > maximum_cycle_scope_bytes) {
			return cycle_refusal::bounds;
		}
		if (!std::ranges::all_of(aPaths, [](const std::string& aPath) { return valid_scope_path(aPath); })) {
			return cycle_refusal::invalid_scope;
		}
		// Sorted, unique and with no path nested in another: one spelling per scope.
		const auto misplaced = std::ranges::adjacent_find(aPaths, [](const std::string& aLeft, const std::string& aRight) {
			return !(aLeft < aRight) || within(aLeft, aRight);
		});
		return misplaced == aPaths.end() ? cycle_refusal::none : cycle_refusal::invalid_scope;
	}

	auto scope_contains(const cycle_scope_specification& aScope, const std::string_view aPath) -> bool
	{
		return aScope.Paths.empty()
			|| std::ranges::any_of(aScope.Paths, [aPath](const std::string& aRoot) { return within(aRoot, aPath); });
	}

	auto create(const review_cycle_catalog& aCatalog, const create_review_cycle& aCommand)
		-> std::expected<created_review_cycle, cycle_refusal>
	{
		if (!valid(aCatalog.Authority) || aCommand.Authority != aCatalog.Authority) {
			return std::unexpected{ cycle_refusal::invalid_identity };
		}
		if (!valid_title(aCommand.Title)) {
			return std::unexpected{ cycle_refusal::bounds };
		}
		if (const auto refusal = validate_scope_paths(aCommand.Scope.Paths); refusal != cycle_refusal::none) {
			return std::unexpected{ refusal };
		}
		auto       catalog = aCatalog;
		const auto id      = take_next(catalog.NextCycle);
		if (!id) {
			return std::unexpected{ cycle_refusal::exhausted };
		}
		auto cycle = review_cycle_state{
			.Subject = { .Authority = aCatalog.Authority, .Cycle = review_cycle_id{ *id }, .Generation = 1 },
			.Title   = aCommand.Title,
			.Scope   = { .Version = 1, .Paths = aCommand.Scope.Paths },
		};
		return created_review_cycle{ .Catalog = std::move(catalog), .Cycle = std::move(cycle) };
	}

	auto apply(const review_cycle_state& aCycle, const review_cycle_command& aCommand) -> review_cycle_outcome
	{
		auto result = review_cycle_outcome{ .Accepted = aCycle };
		if (!valid(aCycle.Subject) || aCommand.Subject != aCycle.Subject) {
			result.Refusal = cycle_refusal::invalid_identity;
			return result;
		}
		if (aCommand.ExpectedRevision != aCycle.Revision) {
			result.Refusal = cycle_refusal::conflict;
			return result;
		}
		if (aCycle.Revision >= maximum_cycle_identity) {
			result.Refusal = cycle_refusal::exhausted;
			return result;
		}
		if (const auto refusal = admits(aCycle, aCommand.Change); refusal != cycle_refusal::none) {
			result.Refusal = refusal;
			return result;
		}
		auto next    = aCycle;
		auto outcome = review_cycle_outcome{ };
		if (const auto refusal = std::visit(change_visitor{ next, outcome }, aCommand.Change); refusal != cycle_refusal::none) {
			result.Refusal = refusal;
			return result;
		}
		if (!outcome.Changed) {
			return result;
		}
		++next.Revision;
		outcome.Accepted = std::move(next);
		return outcome;
	}

	auto classify_path(const cycle_scope_specification& aScope, const cycle_capture_coverage aCoverage,
					   const std::string_view aPath, const cycle_path_observation& aObservation) -> cycle_path_state
	{
		if (!scope_contains(aScope, aPath)) {
			return cycle_path_state::outside_scope;
		}
		if (aObservation.ExplicitlyDeleted) {
			return cycle_path_state::deleted;
		}
		if (aObservation.Unavailable) {
			return cycle_path_state::unavailable;
		}
		if (aObservation.Listed) {
			return cycle_path_state::present;
		}
		return aCoverage == cycle_capture_coverage::complete ? cycle_path_state::absent : cycle_path_state::unknown;
	}

	auto compare_path(const cycle_path_state aBefore, const std::optional<cycle_material_digest>& aBeforeBytes,
					  const cycle_path_state aAfter, const std::optional<cycle_material_digest>& aAfterBytes) -> cycle_path_change
	{
		const auto gone = [](const cycle_path_state aState) {
			return aState == cycle_path_state::deleted || aState == cycle_path_state::absent;
		};
		const auto beforePresent = aBefore == cycle_path_state::present;
		const auto afterPresent  = aAfter == cycle_path_state::present;
		if (beforePresent && afterPresent) {
			if (!aBeforeBytes || !aAfterBytes) {
				return cycle_path_change::unknown;
			}
			return *aBeforeBytes == *aAfterBytes ? cycle_path_change::unchanged : cycle_path_change::modified;
		}
		if (beforePresent && gone(aAfter)) {
			return cycle_path_change::deleted;
		}
		if (gone(aBefore) && afterPresent) {
			return cycle_path_change::added;
		}
		if (gone(aBefore) && gone(aAfter)) {
			return cycle_path_change::unchanged;
		}
		return cycle_path_change::unknown;
	}

	auto explain_capture(const cycle_capture_observation& aCapture, const std::uint32_t aAttempts) noexcept -> cycle_capture_explanation
	{
		auto explanation = cycle_capture_explanation{
			.Quality           = aCapture.Quality,
			.AtomicClaim       = aCapture.Quality == cycle_capture_quality::immutable_source,
			.Complete          = aCapture.Coverage == cycle_capture_coverage::complete,
			.Unavailable       = aCapture.Unavailable,
			.ExplicitDeletions = aCapture.ExplicitDeletions,
			.Retries           = aAttempts > 0 ? aAttempts - 1 : 0,
		};
		explanation.Attention = !explanation.Complete || explanation.Unavailable != 0 || explanation.Retries != 0;
		return explanation;
	}

	auto same_material(const cycle_source_revision& aLeft, const cycle_source_revision& aRight) noexcept -> bool
	{
		return aLeft.Capture.Material == aRight.Capture.Material;
	}

	auto begin_round_draft(const review_cycle_state& aCycle, const std::uint64_t aExpectedRevision)
		-> std::expected<std::pair<review_cycle_state, cycle_round_draft>, cycle_refusal>
	{
		if (!valid(aCycle.Subject)) {
			return std::unexpected{ cycle_refusal::invalid_identity };
		}
		if (aExpectedRevision != aCycle.Revision) {
			return std::unexpected{ cycle_refusal::conflict };
		}
		if (aCycle.Lifecycle == cycle_lifecycle::archived) {
			return std::unexpected{ cycle_refusal::archived };
		}
		auto       next = aCycle;
		const auto id   = take_next(next.Next.RoundDraft);
		if (!id || next.Revision >= maximum_cycle_identity) {
			return std::unexpected{ cycle_refusal::exhausted };
		}
		++next.Revision;
		auto draft = cycle_round_draft{ .Id = { .Cycle = aCycle.Subject.Cycle, .Id = cycle_round_draft_id{ *id } } };
		return std::pair{ std::move(next), draft };
	}

	auto edit_round_draft(const cycle_round_draft& aDraft, const std::uint64_t aExpectedRevision)
		-> std::expected<cycle_round_draft, cycle_refusal>
	{
		if (!valid_cycle_id(aDraft.Id.Cycle) || !valid_cycle_id(aDraft.Id.Id) || aDraft.Revision == 0) {
			return std::unexpected{ cycle_refusal::invalid_identity };
		}
		if (aExpectedRevision != aDraft.Revision) {
			return std::unexpected{ cycle_refusal::conflict };
		}
		if (aDraft.Revision >= maximum_cycle_identity) {
			return std::unexpected{ cycle_refusal::exhausted };
		}
		auto next = aDraft;
		++next.Revision;
		return next;
	}

	auto freeze_round(const review_cycle_state& aCycle, const cycle_round_draft& aDraft,
					  const std::uint64_t aExpectedDraftRevision, const cycle_round_digest& aDigest,
					  const std::optional<cycle_round>& aPrior) -> std::expected<frozen_cycle_round, cycle_refusal>
	{
		if (!valid(aCycle.Subject) || !belongs_to(aDraft.Id, aCycle.Subject) || !aDigest.valid()
			|| (aPrior && (aPrior->Draft != aDraft.Id || !belongs_to(aPrior->Id, aCycle.Subject)))) {
			return std::unexpected{ cycle_refusal::invalid_identity };
		}
		if (aExpectedDraftRevision != aDraft.Revision) {
			return std::unexpected{ cycle_refusal::conflict };
		}
		if (aPrior && aPrior->DraftRevision == aDraft.Revision) {
			if (aPrior->Digest != aDigest) {
				return std::unexpected{ cycle_refusal::conflict };
			}
			return frozen_cycle_round{ .Cycle = aCycle, .Round = *aPrior, .Reused = true };
		}
		if (aCycle.Lifecycle == cycle_lifecycle::archived) {
			return std::unexpected{ cycle_refusal::archived };
		}
		auto       next = aCycle;
		const auto id   = take_next(next.Next.Round);
		if (!id || next.Revision >= maximum_cycle_identity) {
			return std::unexpected{ cycle_refusal::exhausted };
		}
		++next.Revision;
		auto round = cycle_round{
			.Id            = { .Cycle = aCycle.Subject.Cycle, .Id = cycle_round_id{ *id } },
			.Draft         = aDraft.Id,
			.DraftRevision = aDraft.Revision,
			.Digest        = aDigest,
		};
		return frozen_cycle_round{ .Cycle = std::move(next), .Round = round };
	}

	auto record_claim(const cycle_item_judgment& aJudgment, const cycle_response_claim aClaim) -> cycle_item_judgment
	{
		auto next  = aJudgment;
		next.Claim = aClaim;
		return next;
	}

	auto record_decision(const cycle_item_judgment& aJudgment, const std::optional<cycle_reviewer_decision> aDecision) -> cycle_item_judgment
	{
		auto next     = aJudgment;
		next.Decision = aDecision;
		return next;
	}

	auto project(const review_cycle_state& aCycle) -> review_cycle_projection
	{
		const auto& initial = aCycle.Initial;
		return review_cycle_projection{
			.Subject          = aCycle.Subject,
			.Revision         = aCycle.Revision,
			.Title            = aCycle.Title.substr(0, maximum_cycle_title_bytes),
			.Lifecycle        = aCycle.Lifecycle,
			.ScopeVersion     = aCycle.Scope.Version,
			.ScopePaths       = aCycle.Scope.Paths.size(),
			.Captured         = initial.has_value(),
			.InitialEmpty     = initial && initial->Capture.Coverage == cycle_capture_coverage::complete && initial->Capture.Entries == 0,
			.LatestCoverage   = aCycle.Latest ? std::optional{ aCycle.Latest->Capture.Coverage } : std::nullopt,
			.Latest           = aCycle.Latest ? std::optional{ aCycle.Latest->Id } : std::nullopt,
			.Revisions        = aCycle.Next.SourceRevision - 1,
			.WorkspaceKind    = aCycle.Workspace ? std::optional{ aCycle.Workspace->Root.Kind } : std::nullopt,
			.WorkspaceVersion = aCycle.Workspace ? aCycle.Workspace->Version : 0,
		};
	}
}
