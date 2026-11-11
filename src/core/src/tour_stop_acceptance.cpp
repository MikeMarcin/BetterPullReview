#include "bpr/core/tour_stop_acceptance.hpp"

#include "bpr/core/agent_reference.hpp"
#include "bpr/core/review_tour_destination.hpp"

#include <algorithm>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto blank(const std::string_view aText) noexcept -> bool
		{
			return aText.find_first_not_of(" \t\r\n") == std::string_view::npos;
		}

		/// The stop's own lines at `aHead`: as authored there, as mapped
		/// there, or (for a stop an agent proposed) its destination there.
		[[nodiscard]] auto own_lines_at(const tour_stop& aStop, const git_commit_oid& aHead) -> std::optional<tour_code_destination>
		{
			if (aStop.Mapped && aStop.Mapped->Commit == aHead && valid(*aStop.Mapped)) {
				return destination_of(*aStop.Mapped);
			}
			if (const auto source = publication_source(aStop); source && source->Commit == aHead) {
				return source;
			}
			return std::nullopt;
		}

		/// [[BPR-2014]], critical review finding 3. The stop's own lines when
		/// they are not at `aHead`: what was accepted of them before, with what
		/// the comparison said, or (never accepted) its lines at a head the tour
		/// had before a push (its own head or a later one it observed; re-review
		/// finding 1), not compared yet. A historical stop, pinned at a commit
		/// the tour never had as its head, has none: no push can change its bytes.
		[[nodiscard]] auto own_lines_to_compare(const review_tour& aTour, const tour_stop& aStop, const git_commit_oid& aHead) -> std::optional<tour_material>
		{
			const auto source = publication_source(aStop);
			if (aStop.Acceptance && aStop.Acceptance->Destination == source) {
				const auto kept = std::ranges::find(aStop.Acceptance->Material, false, &tour_material::Reference);
				if (kept != aStop.Acceptance->Material.end()) {
					return *kept;
				}
			}
			if (source && source->Commit != aHead && (source->Commit == aTour.Head || std::ranges::find(aTour.EarlierHeads, source->Commit) != aTour.EarlierHeads.end())) {
				return tour_material{ .Range = *source };
			}
			return std::nullopt;
		}

		[[nodiscard]] auto reason_of(const tour_material& aMaterial, const bool aMissing) -> tour_acceptance_reason
		{
			if (aMaterial.Reference) {
				return aMissing ? tour_acceptance_reason::reference_missing : tour_acceptance_reason::reference_changed;
			}
			return aMissing ? tour_acceptance_reason::lines_missing : tour_acceptance_reason::lines_changed;
		}
	}

	auto stable_name(const tour_acceptance_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case tour_acceptance_state::accepted:     return "accepted";
		case tour_acceptance_state::not_accepted: return "not_accepted";
		case tour_acceptance_state::changed:      return "changed";
		}
		return "not_accepted";
	}

	auto stable_name(const tour_acceptance_reason aReason) noexcept -> std::string_view
	{
		switch (aReason) {
		case tour_acceptance_reason::none:              return "none";
		case tour_acceptance_reason::not_accepted:      return "not_accepted";
		case tour_acceptance_reason::text_changed:      return "text_changed";
		case tour_acceptance_reason::moved:             return "moved";
		case tour_acceptance_reason::lines_changed:     return "lines_changed";
		case tour_acceptance_reason::lines_missing:     return "lines_missing";
		case tour_acceptance_reason::reference_changed: return "reference_changed";
		case tour_acceptance_reason::reference_missing: return "reference_missing";
		case tour_acceptance_reason::unchecked:         return "unchecked";
		}
		return "not_accepted";
	}

	auto tour_current_head(const review_tour& aTour) -> git_commit_oid { return aTour.ObservedHead.value_or(aTour.Head); }

	auto decide_tour_stop_acceptance(const review_tour& aTour, const tour_stop& aStop) -> tour_acceptance_decision
	{
		using state  = tour_acceptance_state;
		using reason = tour_acceptance_reason;
		if (!aStop.Acceptance) {
			return { state::not_accepted, reason::not_accepted, { } };
		}
		const auto& accepted = *aStop.Acceptance;
		// Text the author did not accept (Accept all, a website edit, a merge
		// or split) is not accepted yet; an edit is accepted by its Save.
		if (accepted.Title != aStop.Title || accepted.Body != aStop.Body) {
			return { state::not_accepted, reason::text_changed, { } };
		}
		if (accepted.Destination != publication_source(aStop)) {
			return { state::changed, reason::moved, { } };
		}
		const auto head      = tour_current_head(aTour);
		auto       unchecked = false;
		for (const auto& material : accepted.Material) {
			if (material.Range.Commit == head) {
				continue;
			}
			if (material.CheckedAt != head || material.Verdict == tour_material_verdict::unchecked) {
				unchecked = true;
				continue;
			}
			if (material.Verdict != tour_material_verdict::unchanged) {
				return { state::changed, reason_of(material, material.Verdict == tour_material_verdict::missing), material.Written };
			}
		}
		if (unchecked) {
			return { state::changed, reason::unchecked, { } };
		}
		return { state::accepted, reason::none, { } };
	}

	auto tour_stop_accepted(const review_tour& aTour, const tour_stop& aStop) -> bool
	{
		return decide_tour_stop_acceptance(aTour, aStop).State == tour_acceptance_state::accepted;
	}

	auto tour_stop_accept_need(const tour_stop& aStop) noexcept -> tour_accept_need
	{
		if (blank(aStop.Title)) {
			return tour_accept_need::title;
		}
		if (blank(aStop.Body)) {
			return tour_accept_need::explanation;
		}
		if (!publication_source(aStop) && !(aStop.Mapped && valid(*aStop.Mapped))) {
			return tour_accept_need::lines;
		}
		return tour_accept_need::nothing;
	}

	auto tour_stop_accept_need(const review_tour& aTour, const tour_stop& aStop) -> tour_accept_need
	{
		if (const auto need = tour_stop_accept_need(aStop); need != tour_accept_need::nothing) {
			return need;
		}
		const auto head = tour_current_head(aTour);
		if (own_lines_at(aStop, head)) {
			return tour_accept_need::nothing;
		}
		const auto kept = own_lines_to_compare(aTour, aStop, head);
		return kept && kept->CheckedAt == head && (kept->Verdict == tour_material_verdict::changed || kept->Verdict == tour_material_verdict::missing) ? tour_accept_need::lines_at_head : tour_accept_need::nothing;
	}

	auto tour_stop_material(const tour_stop& aStop, const git_commit_oid& aHead, const std::span<const std::string> aFiles) -> std::vector<tour_material>
	{
		auto       result = std::vector<tour_material>{ };
		const auto own    = own_lines_at(aStop, aHead);
		if (own) {
			result.push_back({ .Range = *own });
		}
		const auto files   = agent_reference_files{ .Changed = { aFiles.begin(), aFiles.end() } };
		const auto context = own ? own->Path : (publication_source(aStop) ? publication_source(aStop)->Path : std::string{ });
		for (const auto& reference : find_agent_references(aStop.Body, true)) {
			if (result.size() >= maximum_tour_material) {
				break;
			}
			// Lines before the change are the base's, which a push does not move.
			if (reference.Before || reference.First == 0U || reference.Last < reference.First) {
				continue;
			}
			auto paths = reference.Path.empty() ? (context.empty() ? std::vector<std::string>{ } : std::vector{ context }) : resolve_agent_reference(reference.Path, files);
			if (paths.size() != 1U || paths.front().empty() || paths.front().size() > maximum_tour_path_bytes) {
				continue;
			}
			auto range = tour_code_destination{ .Commit = aHead, .Path = std::move(paths.front()), .Side = diff_side::right, .FirstLine = reference.First - 1U, .LastLine = reference.Last - 1U };
			if (!valid(range) || std::ranges::any_of(result, [&](const auto& aKnown) { return aKnown.Range == range; })) {
				continue;
			}
			auto written = aStop.Body.substr(reference.Begin, reference.Length);
			if (written.size() > maximum_tour_material_label_bytes) {
				written.clear();
			}
			result.push_back({ .Range = std::move(range), .Reference = true, .Written = std::move(written) });
		}
		return result;
	}

	auto tour_stop_names_files(const tour_stop& aStop) -> bool
	{
		return std::ranges::any_of(find_agent_references(aStop.Body, true), [](const auto& aReference) { return !aReference.Before && !aReference.Path.empty() && aReference.First != 0U && aReference.Last >= aReference.First; });
	}

	auto accept_tour_stop(const review_tour& aTour, const tour_stop& aStop, const std::span<const std::string> aFiles) -> tour_stop_acceptance
	{
		const auto head = tour_current_head(aTour);
		auto       made = tour_stop_acceptance{ .Title = aStop.Title, .Body = aStop.Body, .Destination = publication_source(aStop), .Head = head, .Material = tour_stop_material(aStop, head, aFiles) };
		for (auto& material : made.Material) {
			// The author accepted these bytes at this head.
			material.CheckedAt = head;
			material.Verdict   = tour_material_verdict::unchanged;
		}
		// Lines not at this head are not accepted by a press: they stay to be
		// compared, so the stop is never Accepted with nothing to compare.
		if (!own_lines_at(aStop, head)) {
			if (auto kept = own_lines_to_compare(aTour, aStop, head)) {
				made.Material.insert(made.Material.begin(), std::move(*kept));
				if (made.Material.size() > maximum_tour_material) {
					made.Material.pop_back();
				}
			}
		}
		return made;
	}

	void carry_tour_acceptance(review_tour& aTour)
	{
		for (auto& stop : aTour.Stops) {
			if (stop.Acceptance) {
				continue;
			}
			// Critical review finding 4: only the author's own Ready (seen in
			// the window or the walk) carries, never an agent's, a script's or
			// one of unknown origin, and never an outline-suggested stop. A
			// stop whose text names another file is not carried: the draft
			// does not know the changed files that reference resolves against.
			const bool reaffirmed = !aTour.ObservedHead || stop.ReaffirmedHead == aTour.ObservedHead;
			if (stop.Preparation == tour_preparation::ready && stop.CompletionOrigin == tour_completion_origin::observed && !stop.Suggestion && reaffirmed && tour_stop_accept_need(stop) == tour_accept_need::nothing && !tour_stop_names_files(stop)) {
				stop.Acceptance = accept_tour_stop(aTour, stop, { });
			}
		}
	}
} // namespace bpr::core
