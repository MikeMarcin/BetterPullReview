#include "bpr/core/review_tour_reconciliation.hpp"
#include "bpr/core/review_tour_revision.hpp"
#include "bpr/core/review_tour_update.hpp"
#include <algorithm>
namespace bpr::core {
	namespace {
		auto difference(std::string_view aBase, std::string_view aValue) -> tour_text_edit
		{
			std::size_t prefix = 0, suffix = 0;
			while (prefix < std::min(aBase.size(), aValue.size()) && aBase[prefix] == aValue[prefix]) {
				++prefix;
			}
			while (suffix < std::min(aBase.size(), aValue.size()) - prefix && aBase[aBase.size() - suffix - 1] == aValue[aValue.size() - suffix - 1]) {
				++suffix;
			}
			// Never cut a multi-byte code point at the common prefix/suffix.
			while (prefix && prefix < aBase.size() && (static_cast<unsigned char>(aBase[prefix]) & 0xc0U) == 0x80U) {
				--prefix;
			}
			while (suffix && (static_cast<unsigned char>(aBase[aBase.size() - suffix]) & 0xc0U) == 0x80U) {
				--suffix;
			}
			return { { prefix, aBase.size() - suffix }, std::string{ aBase.substr(prefix, aBase.size() - prefix - suffix) }, std::string{ aValue.substr(prefix, aValue.size() - prefix - suffix) } };
		}
		auto value_of(const tour_field_baseline& aField) -> std::string
		{
			if (aField.DraftText) {
				return *aField.DraftText;
			}
			return aField.SourceSpan ? aField.Raw.substr(aField.SourceSpan->first, aField.SourceSpan->second - aField.SourceSpan->first) : aField.Raw;
		}
		auto field_baseline(std::string_view aRaw, std::string_view aName, std::string_view aEntity) -> std::optional<tour_field_baseline>
		{
			const auto mapping = map_tour_markdown(aRaw);
			if (!mapping.Bounded || std::ranges::count(mapping.Sections, aName, &tour_markdown_section::Name) != 1) {
				return { };
			}
			const auto span = std::ranges::find(mapping.Sections, aName, &tour_markdown_section::Name)->Content;
			return tour_field_baseline{ .Raw = std::string{ aRaw }, .RemoteEntity = std::string{ aEntity }, .SourceSpan = std::pair{ span.Begin, span.End } };
		}
		auto remote_field_value(const tour_field_baseline& aBase, const tour_field_baseline& aRemote) -> std::string
		{
			auto result = value_of(aRemote);
			if (aBase.DraftText && aBase.SourceSpan) {
				const auto raw = aBase.Raw.substr(aBase.SourceSpan->first, aBase.SourceSpan->second - aBase.SourceSpan->first);
				const auto at  = raw.find(*aBase.DraftText);
				if (at != std::string::npos) {
					const auto prefix = raw.substr(0, at), suffix = raw.substr(at + aBase.DraftText->size());
					if (result.size() >= prefix.size() + suffix.size() && result.starts_with(prefix) && result.ends_with(suffix)) {
						result = result.substr(prefix.size(), result.size() - prefix.size() - suffix.size());
					}
				}
			}
			return result;
		}
		void reconcile_brief(review_tour& aResult, const review_tour& aRemote, bool aProblem)
		{
			const auto name     = aProblem ? "Problem" : "Solution";
			auto&      local    = aProblem ? aResult.Problem : aResult.Solution;
			auto&      baseline = aProblem ? aResult.ProblemBaseline : aResult.SolutionBaseline;
			auto       observed = aProblem ? aRemote.ProblemBaseline : aRemote.SolutionBaseline;
			if (baseline && aResult.Remote && baseline->RemoteEntity == aResult.Remote->Entity && aRemote.IndexBaseline) {
				observed = field_baseline(aRemote.IndexBaseline->Raw, name, aRemote.Remote->Entity);
			}
			// [[BPR-1787]]. A brief is compared only with the place it was
			// published: the description is never matched against the comment.
			if (observed && baseline && baseline->RemoteEntity != observed->RemoteEntity) {
				observed.reset();
			}
			if (!observed) {
				aResult.RemoteStale = true;
				aResult.ImportNotice += "Website brief requires source mapping. ";
				return;
			}
			if (!baseline && aResult.IndexBaseline) {
				baseline = field_baseline(aResult.IndexBaseline->Raw, name, aResult.Remote->Entity);
			}
			if (!baseline) {
				aResult.RemoteStale = true;
				return;
			}
			if (baseline->Raw == observed->Raw) {
				return;
			}
			const auto before = value_of(*baseline), remote = remote_field_value(*baseline, *observed);
			observed->DraftText = remote;
			const auto merged   = merge_tour_text(before, local, remote);
			if (merged) {
				local    = *merged;
				baseline = std::move(observed);
			} else {
				aResult.Conflicts.push_back({ aProblem ? tour_conflict_field::problem : tour_conflict_field::solution, { }, before, local, remote, *observed });
			}
		}
		auto route(const review_tour& aTour) -> std::vector<std::string>
		{
			std::vector<std::string> result;
			result.reserve(aTour.Stops.size());
			for (const auto& stop : aTour.Stops) {
				result.push_back(stop.Remote ? stop.Remote->WebUrl : "local stop " + std::to_string(stop.Id.value()));
			}
			return result;
		}
		auto baseline_route(const tour_field_baseline& aBaseline) -> std::vector<std::string>
		{
			const auto               map = map_tour_markdown(aBaseline.Raw);
			std::vector<std::string> result;
			for (const auto& list : map.Lists) {
				if ((aBaseline.SourceSpan && list.Source == tour_text_span{ aBaseline.SourceSpan->first, aBaseline.SourceSpan->second }) || (!aBaseline.SourceSpan && map.Lists.size() == 1)) {
					for (const auto& entry : list.Entries) {
						result.push_back(tour_link_identity(entry.Destination));
					}
					return result;
				}
			}
			return { };
		}
		auto route_text(const review_tour& aTour) -> std::string
		{
			std::string result;
			for (const auto& stop : aTour.Stops) {
				result += stop.Title + " — " + (stop.Remote ? stop.Remote->WebUrl : "unpublished stop") + "\n";
			}
			return result;
		}
		void accept_stop_body(tour_stop& aStop, const std::string& aBody, const tour_field_baseline& aObservation)
		{
			const auto previousBody = aStop.Body, previousTitle = aStop.Title;
			const auto mapping = map_tour_stop(aBody);
			aStop.Body         = mapping.Narrative ? aBody.substr(mapping.Narrative->Begin, mapping.Narrative->End - mapping.Narrative->Begin) : aBody;
			if (mapping.Heading && mapping.Title != tour_markdown_label(aStop.Title)) {
				aStop.Title = mapping.Title;
			}
			if (aStop.Suggestion && (aStop.Body != previousBody || aStop.Title != previousTitle)) {
				aStop.Preparation = tour_preparation::editing;
			}
			aStop.Baseline            = aObservation;
			const auto observed       = map_tour_stop(aObservation.Raw);
			aStop.Baseline->DraftText = observed.Narrative ? aObservation.Raw.substr(observed.Narrative->Begin, observed.Narrative->End - observed.Narrative->Begin) : aObservation.Raw;
		}
		void reconcile_stop(review_tour& aResult, tour_stop& aLocal, const tour_stop& aRemote)
		{
			if (!aLocal.Baseline || !aRemote.Baseline || !aLocal.Remote || !aRemote.Remote || aLocal.Remote->Entity != aRemote.Remote->Entity) {
				aResult.RemoteStale = true;
				return;
			}
			if (aLocal.Baseline->Raw == aRemote.Baseline->Raw) {
				aLocal.Remote->Resolved = aRemote.Remote->Resolved;
				aLocal.Remote->Outdated = aRemote.Remote->Outdated;
				return;
			}
			const auto proposed = propose_tour_stop_text(aLocal);
			if (!proposed) {
				aResult.RemoteStale = true;
				return;
			}
			const auto merged = merge_tour_text(aLocal.Baseline->Raw, *proposed, aRemote.Baseline->Raw);
			if (merged) {
				accept_stop_body(aLocal, *merged, *aRemote.Baseline);
			} else {
				aResult.Conflicts.push_back({ tour_conflict_field::stop, aLocal.Id, aLocal.Baseline->Raw, *proposed, aRemote.Baseline->Raw, *aRemote.Baseline });
			}
			aLocal.Remote->Resolved = aRemote.Remote->Resolved;
			aLocal.Remote->Outdated = aRemote.Remote->Outdated;
		}
	}
	auto merge_tour_text(std::string_view aBase, std::string_view aLocal, std::string_view aRemote) -> std::optional<std::string>
	{
		if (aBase.size() > maximum_tour_field_bytes || aLocal.size() > maximum_tour_field_bytes || aRemote.size() > maximum_tour_field_bytes) {
			return { };
		}
		if (aLocal == aBase || aLocal == aRemote) {
			return std::string{ aRemote };
		}
		if (aRemote == aBase) {
			return std::string{ aLocal };
		}
		const auto local = difference(aBase, aLocal), remote = difference(aBase, aRemote);
		if (local.Span.Begin == remote.Span.Begin || !(local.Span.End <= remote.Span.Begin || remote.Span.End <= local.Span.Begin)) {
			return { };
		}
		return edit_tour_markdown(aBase, std::vector{ local, remote });
	}
	auto reconcile_tour(const review_tour& aLocal, const review_tour& aRemote) -> review_tour
	{
		auto result = aLocal;
		if (validate(aLocal) != tour_refusal::none || validate(aRemote) != tour_refusal::none || !aLocal.Remote || !aRemote.Remote || aLocal.Owner != aRemote.Owner || aLocal.Remote->Entity != aRemote.Remote->Entity || !aLocal.IndexBaseline || !aRemote.IndexBaseline) {
			return result;
		}
		if (aRemote.ReadOnly || !aLocal.Conflicts.empty()) {
			result.RemoteStale  = true;
			result.ImportNotice = "Partial or unresolved website observation. Retained text is stale; resolve the saved versions and refresh.";
			return result;
		}
		const auto changes = observed_tour_changes(aLocal, aRemote);
		if (!changes.empty()) {
			result.HasPriorObservation = true;
			result.ObservedChanges     = changes;
		}
		result.RemoteStale  = false;
		result.ImportNotice = aRemote.ImportNotice;
		reconcile_brief(result, aRemote, true);
		reconcile_brief(result, aRemote, false);
		for (auto& stop : result.Stops) {
			if (!stop.Remote) {
				continue;
			}
			const auto remote = std::ranges::find_if(aRemote.Stops, [&](const auto& aValue) { return aValue.Remote && aValue.Remote->Entity == stop.Remote->Entity; });
			if (remote != aRemote.Stops.end()) {
				reconcile_stop(result, stop, *remote);
			}
		}
		const auto before = baseline_route(*aLocal.IndexBaseline), local = route(aLocal), remote = route(aRemote);
		if (remote != before && remote != local && local != before) {
			result.Conflicts.push_back({ tour_conflict_field::route, { }, aLocal.IndexBaseline->Raw, route_text(aLocal), aRemote.IndexBaseline->Raw, *aRemote.IndexBaseline });
			result.IncomingRoute = aRemote.Stops;
		} else {
			if (local == before && remote != before) {
				std::vector<tour_stop> stops;
				for (auto stop : aRemote.Stops) {
					const auto existing = std::ranges::find_if(result.Stops, [&](const auto& aValue) { return aValue.Remote && stop.Remote && aValue.Remote->Entity == stop.Remote->Entity; });
					if (existing != result.Stops.end()) {
						stop = *existing;
					} else {
						stop.Id = tour_stop_id{ ++result.LastStopId };
					}
					stops.push_back(std::move(stop));
				}
				const bool lostEdit = std::ranges::any_of(result.Stops, [&](const auto& aStop) { return aStop.Remote && aStop.Baseline && std::ranges::find(remote, aStop.Remote->WebUrl) == remote.end() && propose_tour_stop_text(aStop) != std::optional{ aStop.Baseline->Raw }; });
				if (lostEdit) {
					result.Conflicts.push_back({ tour_conflict_field::route, { }, aLocal.IndexBaseline->Raw, route_text(aLocal), aRemote.IndexBaseline->Raw, *aRemote.IndexBaseline });
					result.IncomingRoute = aRemote.Stops;
				} else {
					result.Stops = std::move(stops);
				}
			}
			if (std::ranges::none_of(result.Conflicts, [](const auto& aConflict) { return aConflict.Field == tour_conflict_field::route; })) {
				if (result.IndexBaseline->Raw != aRemote.IndexBaseline->Raw) {
					result.IndexBaseline = aRemote.IndexBaseline;
				}
			}
		}
		if (validate(result) != tour_refusal::none) {
			result              = aLocal;
			result.RemoteStale  = true;
			result.ImportNotice = "Website reconciliation exceeds the retained text limit; local work is preserved.";
		}
		return result;
	}
	auto resolve_tour_conflict(const review_tour& aTour, std::size_t aIndex, bool aUseRemote) -> std::optional<review_tour>
	{
		if (aIndex >= aTour.Conflicts.size()) {
			return { };
		}
		auto       result   = aTour;
		const auto conflict = result.Conflicts[aIndex];

		switch (conflict.Field) {
		case tour_conflict_field::problem:
			if (aUseRemote) {
				result.Problem = conflict.Remote;
			}
			result.ProblemBaseline = conflict.Observation;
			break;
		case tour_conflict_field::solution:
			if (aUseRemote) {
				result.Solution = conflict.Remote;
			}
			result.SolutionBaseline = conflict.Observation;
			break;
		case tour_conflict_field::stop: {
			const auto stop = std::ranges::find(result.Stops, conflict.Stop, &tour_stop::Id);
			if (stop == result.Stops.end()) {
				return { };
			}
			const auto chosen = aUseRemote ? std::optional{ conflict.Remote } : propose_tour_stop_text(*stop);
			if (!chosen) {
				return { };
			}
			accept_stop_body(*stop, *chosen, conflict.Observation);
			break;
		}
		case tour_conflict_field::route:
			if (aUseRemote) {
				for (auto& stop : result.IncomingRoute) {
					const auto old = std::ranges::find_if(result.Stops, [&](const auto& aValue) { return aValue.Remote && stop.Remote && aValue.Remote->Entity == stop.Remote->Entity; });
					if (old == result.Stops.end()) {
						stop.Id = tour_stop_id{ ++result.LastStopId };
					} else {
						stop = *old;
					}
				}
				result.Stops = std::move(result.IncomingRoute);
			}
			result.IncomingRoute.clear();
			result.IndexBaseline = conflict.Observation;
			break;
		}
		result.Conflicts.erase(result.Conflicts.begin() + static_cast<std::ptrdiff_t>(aIndex));
		if (validate(result) != tour_refusal::none) {
			return { };
		}
		return result;
	}
	void retain_tour_brief_baselines(review_tour& aTour, std::string_view aRaw, std::string_view aEntity)
	{
		for (const auto problem : { true, false }) {
			auto field = field_baseline(aRaw, problem ? "Problem" : "Solution", aEntity);
			if (field) {
				field->DraftText                                           = problem ? aTour.Problem : aTour.Solution;
				(problem ? aTour.ProblemBaseline : aTour.SolutionBaseline) = std::move(field);
			}
		}
	}
	auto propose_tour_description(const review_tour& aTour, std::string_view aRaw) -> std::optional<std::string>
	{
		if (!aTour.Remote) {
			const auto mapped   = map_tour_markdown(aRaw);
			const bool hasBrief = std::ranges::any_of(mapped.Sections, [](const auto& aSection) { return aSection.Name == "Problem" || aSection.Name == "Solution"; });
			if (mapped.Bounded && !hasBrief) {
				const auto newline = aRaw.find("\r\n") == std::string_view::npos ? "\n" : "\r\n";
				return insert_tour_brief(aRaw, 0, aTour.Problem, aTour.Solution, newline);
			}
		}
		std::vector<tour_text_edit> edits;
		for (const auto problem : { true, false }) {
			const auto field = field_baseline(aRaw, problem ? "Problem" : "Solution", { });
			if (!field) {
				return { };
			}
			const auto& baseline = problem ? aTour.ProblemBaseline : aTour.SolutionBaseline;
			const auto& desired  = problem ? aTour.Problem : aTour.Solution;
			auto        before = value_of(*field), replacement = desired;
			if (baseline && baseline->DraftText && baseline->SourceSpan) {
				const auto content  = baseline->Raw.substr(baseline->SourceSpan->first, baseline->SourceSpan->second - baseline->SourceSpan->first);
				const auto position = content.find(*baseline->DraftText);
				if (position == std::string::npos) {
					return { };
				}
				const auto prefix = content.substr(0, position), suffix = content.substr(position + baseline->DraftText->size());
				replacement = prefix;
				replacement += desired;
				replacement += suffix;
			} else if (baseline && baseline->SourceSpan) {
				// [[BPR-1787]]. A cold import holds the exact section text: an
				// unchanged field writes nothing, and a revision keeps the
				// section's own blank lines around it.
				const auto content = baseline->Raw.substr(baseline->SourceSpan->first, baseline->SourceSpan->second - baseline->SourceSpan->first);
				const auto first   = content.find_first_not_of(" \t\r\n");
				const auto begin   = desired.find_first_not_of(" \t\r\n");
				if (desired != content && first != std::string::npos && begin != std::string::npos) {
					replacement = content.substr(0, first) + desired.substr(begin, desired.find_last_not_of(" \t\r\n") - begin + 1) + content.substr(content.find_last_not_of(" \t\r\n") + 1);
				} else if (desired != content) {
					const auto nl = aRaw.find("\r\n") == std::string_view::npos ? "\n" : "\r\n";
					replacement   = std::string{ nl } + desired + nl + nl;
				}
			} else if (!desired.starts_with('\n') && !desired.starts_with("\r\n")) {
				const auto nl = aRaw.find("\r\n") == std::string_view::npos ? "\n" : "\r\n";
				replacement   = std::string{ nl } + desired + nl + nl;
			}
			if (before != replacement) {
				edits.push_back({ { field->SourceSpan->first, field->SourceSpan->second }, before, replacement });
			}
		}
		return edit_tour_markdown(aRaw, edits);
	}
	auto tour_comment_lead(const review_tour& aTour, bool aBriefInDescription) -> std::string
	{
		if (aBriefInDescription) {
			return std::string{ tour_brief_in_description_note };
		}
		return "## Problem\n\n" + aTour.Problem + "\n\n## Solution\n\n" + aTour.Solution;
	}
	auto tour_brief_in_comment(const review_tour& aTour) -> bool
	{
		if (!aTour.IndexBaseline) {
			return false;
		}
		const auto mapped = map_tour_markdown(aTour.IndexBaseline->Raw);
		return std::ranges::any_of(mapped.Sections, [](const auto& aSection) { return aSection.Name == "Problem" || aSection.Name == "Solution"; });
	}
	auto tour_brief_in_description(const review_tour& aTour) -> bool
	{
		const auto elsewhere = [&](const std::optional<tour_field_baseline>& aBaseline) { return aBaseline && aTour.Remote && aBaseline->RemoteEntity != aTour.Remote->Entity; };
		return elsewhere(aTour.ProblemBaseline) || elsewhere(aTour.SolutionBaseline);
	}
}
