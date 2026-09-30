#include "bpr/core/review_cycle_feedback.hpp"

#include "bpr/core/review_cycle_material.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <type_traits>

namespace bpr::core {
	namespace {
		auto text_of(const std::span<const std::uint8_t> aBytes) -> std::string_view
		{
			return { reinterpret_cast<const char*>(aBytes.data()), aBytes.size() };
		}

		auto allocated(const review_cycle_state& aCycle, const cycle_source_revision_id aRevision) -> bool
		{
			return valid_cycle_id(aRevision) && aRevision.value() < aCycle.Next.SourceRevision;
		}

		/// The structure alone: bounds, path and ranges.
		auto well_formed(const cycle_feedback_target& aTarget) -> bool
		{
			return std::visit([](const auto& aValue) -> bool {
				using value_type = std::decay_t<decltype(aValue)>;
				if constexpr (std::is_same_v<value_type, cycle_whole_target>) {
					return true;
				} else if constexpr (std::is_same_v<value_type, cycle_module_target>) {
					return valid_cycle_path(aValue.Path) && (!aValue.Revision || valid_cycle_id(*aValue.Revision));
				} else if constexpr (std::is_same_v<value_type, cycle_file_target>) {
					return valid_cycle_id(aValue.Revision) && valid_cycle_path(aValue.Path) && aValue.Blob.valid();
				} else {
					return valid_cycle_id(aValue.Revision) && valid_cycle_path(aValue.Path) && aValue.Blob.valid() && aValue.Start <= aValue.End
						&& aValue.End - aValue.Start == aValue.Original.size() && aValue.Original.size() <= maximum_cycle_anchor_bytes
						&& aValue.Before.size() <= maximum_cycle_anchor_context_bytes && aValue.Before.size() <= aValue.Start
						&& aValue.After.size() <= maximum_cycle_anchor_context_bytes
						&& (!aValue.Symbol || (!aValue.Symbol->empty() && aValue.Symbol->size() <= maximum_cycle_symbol_bytes));
				}
			},
							  aTarget);
		}

		/// A related reference may re-attach only the primary or an earlier one.
		auto well_formed(const cycle_related_reference& aReference, const std::size_t aIndex) -> bool
		{
			return well_formed(aReference.Target) && aReference.Disclosure <= cycle_reference_disclosure::private_only
				&& (!aReference.Rebinds || *aReference.Rebinds <= aIndex);
		}

		auto take_next(std::uint64_t& aCounter) -> std::optional<std::uint64_t>
		{
			if (aCounter == 0 || aCounter > maximum_cycle_identity) {
				return std::nullopt;
			}
			return aCounter++;
		}

		auto from_lineage(const private_review_refusal aRefusal) -> cycle_refusal
		{
			switch (aRefusal) {
			case private_review_refusal::conflict: return cycle_refusal::conflict;
			case private_review_refusal::bounds:   return cycle_refusal::bounds;
			case private_review_refusal::archived: return cycle_refusal::closed;
			default:                               return cycle_refusal::invalid_identity;
			}
		}

		auto valid_field(const std::optional<std::string>& aValue) -> bool
		{
			return !aValue
				|| (!aValue->empty() && aValue->size() <= maximum_cycle_provenance_field_bytes
					&& std::ranges::none_of(*aValue, [](const char aByte) { return static_cast<unsigned char>(aByte) < 0x20U || aByte == '\x7F'; }));
		}

		auto valid_reference(const cycle_feedback_revision_ref& aRef) -> bool
		{
			const auto& position = aRef.Position;
			return valid_cycle_id(aRef.Item.Cycle) && valid_cycle_id(aRef.Item.Id) && valid_private_id(position.Turn) && valid_private_id(position.Revision)
				&& valid_private_id(position.Order) && position.Order.value() == position.Revision.value() && aRef.Digest.valid();
		}

		auto valid_prior(const cycle_feedback_revision_ref& aNew, const cycle_feedback_revision_ref& aPrior) -> bool
		{
			return valid_reference(aPrior) && aPrior.Item == aNew.Item && private_prior_precedes(aNew.Position, aPrior.Position);
		}

		/// The next revision of a thread: a fresh or given turn at the next order.
		auto next_position(const cycle_feedback_thread& aThread, const std::optional<private_review_turn_id> aTurn)
			-> std::optional<private_lineage_position>
		{
			const auto order = aThread.Lineage.LastOrder.value() + 1U;
			if (order > maximum_private_identity || (!aTurn && aThread.NextTurn > maximum_private_identity)) {
				return std::nullopt;
			}
			return private_lineage_position{
				.Turn     = aTurn.value_or(private_review_turn_id{ aThread.NextTurn }),
				.Revision = private_review_turn_revision_id{ order },
				.Order    = private_vertex_order{ order },
			};
		}

		auto line_terminator(const std::span<const std::uint8_t> aFile, const std::size_t aIndex) -> bool
		{
			return aFile[aIndex] == '\n' || (aFile[aIndex] == '\r' && (aIndex + 1U == aFile.size() || aFile[aIndex + 1U] != '\n'));
		}

		/// Start of the line `aLines` lines above the one containing `aOffset`.
		auto context_start(const std::span<const std::uint8_t> aFile, const std::size_t aOffset) -> std::size_t
		{
			auto remaining = cycle_anchor_context_lines + 1U;
			auto index     = aOffset;
			while (index > 0) {
				if (line_terminator(aFile, index - 1U) && --remaining == 0U) {
					break;
				}
				--index;
			}
			return index;
		}

		/// End of the line `aLines` lines below the one containing `aOffset`.
		auto context_end(const std::span<const std::uint8_t> aFile, const std::size_t aOffset) -> std::size_t
		{
			auto remaining = cycle_anchor_context_lines + 1U;
			auto index     = aOffset;
			while (index < aFile.size()) {
				const auto terminator = line_terminator(aFile, index);
				++index;
				if (terminator && --remaining == 0U) {
					break;
				}
			}
			return index;
		}

		/// Every start of `aNeedle` in `aHaystack`, up to `aLimit` of them.
		auto occurrences(const std::string_view aHaystack, const std::string_view aNeedle, const std::size_t aLimit) -> std::vector<std::size_t>
		{
			auto found = std::vector<std::size_t>{ };
			if (aNeedle.empty() || aNeedle.size() > aHaystack.size()) {
				return found;
			}
			const auto searcher = std::boyer_moore_horspool_searcher{ aNeedle.begin(), aNeedle.end() };
			auto       from     = aHaystack.begin();
			while (found.size() < aLimit) {
				const auto hit = std::search(from, aHaystack.end(), searcher);
				if (hit == aHaystack.end()) {
					break;
				}
				found.push_back(static_cast<std::size_t>(hit - aHaystack.begin()));
				from = hit + 1;
			}
			return found;
		}

		auto correspond_anchor(const cycle_source_anchor& aAnchor, const std::string_view aBytes, const cycle_source_revision_id aAgainst) -> cycle_target_correspondence
		{
			const auto limit  = std::size_t{ maximum_cycle_correspondence_candidates } + 1U;
			auto       result = cycle_target_correspondence{ .Kind = cycle_correspondence::unmapped, .Against = aAgainst };
			const auto mapped = [&](const std::size_t aStart) {
				result.Kind  = cycle_correspondence::exact_mapped;
				result.Range = std::pair<std::uint64_t, std::uint64_t>{ aStart, aStart + aAnchor.Original.size() };
			};
			if (aAnchor.Original.empty()) {
				// An insertion point is found only through its surroundings.
				const auto around = aAnchor.Before + aAnchor.After;
				const auto hits   = occurrences(aBytes, around, limit);
				if (hits.size() == 1U) {
					mapped(hits.front() + aAnchor.Before.size());
				} else if (hits.size() > 1U) {
					result.Kind = cycle_correspondence::ambiguous;
				}
				result.Candidates = static_cast<std::uint32_t>(std::min(hits.size(), std::size_t{ maximum_cycle_correspondence_candidates }));
				return result;
			}
			const auto hits   = occurrences(aBytes, aAnchor.Original, limit);
			result.Candidates = static_cast<std::uint32_t>(std::min(hits.size(), std::size_t{ maximum_cycle_correspondence_candidates }));
			if (hits.size() == 1U) {
				mapped(hits.front());
				return result;
			}
			if (hits.empty()) {
				return result;
			}
			// Repeated text: only the complete original context may single one out.
			const auto inContext = [&](const std::size_t aStart) {
				const auto end = aStart + aAnchor.Original.size();
				return aStart >= aAnchor.Before.size() && aBytes.substr(aStart - aAnchor.Before.size(), aAnchor.Before.size()) == aAnchor.Before
					&& aBytes.substr(end, aAnchor.After.size()) == aAnchor.After;
			};
			const auto matching = std::ranges::count_if(hits, inContext);
			if (matching == 1 && hits.size() < limit) {
				mapped(*std::ranges::find_if(hits, inContext));
			} else {
				result.Kind = cycle_correspondence::ambiguous;
			}
			return result;
		}
	}

	auto source_revision_of(const cycle_feedback_target& aTarget) noexcept -> std::optional<cycle_source_revision_id>
	{
		if (const auto* module = std::get_if<cycle_module_target>(&aTarget)) {
			return module->Revision;
		}
		if (const auto* file = std::get_if<cycle_file_target>(&aTarget)) {
			return file->Revision;
		}
		if (const auto* anchor = std::get_if<cycle_source_anchor>(&aTarget)) {
			return anchor->Revision;
		}
		return std::nullopt;
	}

	auto valid_target(const review_cycle_state& aCycle, const cycle_feedback_target& aTarget) -> bool
	{
		const auto revision = source_revision_of(aTarget);
		return well_formed(aTarget) && (!revision || allocated(aCycle, *revision));
	}

	auto verify_target(const cycle_feedback_target& aTarget, const std::span<const std::uint8_t> aFile) -> bool
	{
		if (const auto* file = std::get_if<cycle_file_target>(&aTarget)) {
			return cycle_material_digest{ sha256(aFile) } == file->Blob;
		}
		const auto* anchor = std::get_if<cycle_source_anchor>(&aTarget);
		if (anchor == nullptr) {
			return true;
		}
		if (!well_formed(aTarget) || cycle_material_digest{ sha256(aFile) } != anchor->Blob || anchor->End > aFile.size()) {
			return false;
		}
		const auto bytes = text_of(aFile);
		const auto start = static_cast<std::size_t>(anchor->Start);
		const auto end   = static_cast<std::size_t>(anchor->End);
		return classify_material(aFile).text() && utf8_boundary(aFile, start) && utf8_boundary(aFile, end)
			&& bytes.substr(start, end - start) == anchor->Original
			&& bytes.substr(start - anchor->Before.size(), anchor->Before.size()) == anchor->Before
			&& bytes.substr(end, anchor->After.size()) == anchor->After;
	}

	auto make_anchor(const cycle_source_revision_id aRevision, std::string aPath, const std::span<const std::uint8_t> aFile, const std::uint64_t aStart,
					 const std::uint64_t aEnd) -> std::optional<cycle_source_anchor>
	{
		if (aStart > aEnd || aEnd > aFile.size() || aEnd - aStart > maximum_cycle_anchor_bytes || !classify_material(aFile).text()
			|| !utf8_boundary(aFile, aStart) || !utf8_boundary(aFile, aEnd)) {
			return std::nullopt;
		}
		const auto start = static_cast<std::size_t>(aStart);
		const auto end   = static_cast<std::size_t>(aEnd);
		// Keep whole lines of context where the bound allows, trimmed to scalars.
		auto before = std::max(context_start(aFile, start), start - std::min(start, maximum_cycle_anchor_context_bytes));
		while (!utf8_boundary(aFile, before)) {
			++before;
		}
		auto after = std::min(context_end(aFile, end), end + maximum_cycle_anchor_context_bytes);
		while (!utf8_boundary(aFile, after)) {
			--after;
		}
		const auto bytes  = text_of(aFile);
		auto       anchor = cycle_source_anchor{
			.Revision = aRevision,
			.Path     = std::move(aPath),
			.Blob     = cycle_material_digest{ sha256(aFile) },
			.Start    = aStart,
			.End      = aEnd,
			.Original = std::string{ bytes.substr(start, end - start) },
			.Before   = std::string{ bytes.substr(before, start - before) },
			.After    = std::string{ bytes.substr(end, after - end) },
			.Symbol   = std::nullopt,
		};
		if (!well_formed(cycle_feedback_target{ anchor })) {
			return std::nullopt;
		}
		return anchor;
	}

	auto project_lines(const std::span<const std::uint8_t> aFile, const std::uint64_t aStart, const std::uint64_t aEnd) -> std::optional<cycle_line_span>
	{
		if (aStart > aEnd || aEnd > aFile.size()) {
			return std::nullopt;
		}
		const auto lineAt = [&](const std::size_t aOffset) {
			auto line = std::uint64_t{ 1 };
			for (std::size_t index = 0; index < aOffset; ++index) {
				line += line_terminator(aFile, index) ? 1U : 0U;
			}
			return line;
		};
		const auto first = lineAt(static_cast<std::size_t>(aStart));
		// The last byte of the range, so a range ending at a line break stays on its line.
		const auto last = aStart == aEnd ? first : lineAt(static_cast<std::size_t>(aEnd - 1U));
		return cycle_line_span{ .First = first, .Last = last };
	}

	auto valid(const cycle_feedback_author& aAuthor) noexcept -> bool
	{
		const auto fields = std::array{ &aAuthor.Adapter, &aAuthor.Provider, &aAuthor.Model, &aAuthor.Effort };
		if (!std::ranges::all_of(fields, [](const auto* aField) { return valid_field(*aField); })) {
			return false;
		}
		switch (aAuthor.Provenance) {
		case authored_provenance::human:
		case authored_provenance::unknown:
			return std::ranges::none_of(fields, [](const auto* aField) { return aField->has_value(); });
		case authored_provenance::adapter_observed:
			return aAuthor.Adapter.has_value();
		case authored_provenance::self_reported:
			// A self-report describes itself; no adapter observed it.
			return !aAuthor.Adapter.has_value();
		}
		return false;
	}

	auto valid(const cycle_feedback_thread& aThread) -> bool
	{
		if (!valid_cycle_id(aThread.Id.Cycle) || !valid_cycle_id(aThread.Id.Id) || !well_formed(aThread.Primary)
			|| aThread.Related.size() > maximum_cycle_related_references || !valid_private_lineage(aThread.Lineage) || aThread.NextTurn < 2U
			|| aThread.NextTurn > maximum_private_identity + 1U || aThread.NextSuggestion == 0U || aThread.NextSuggestion > maximum_cycle_identity + 1U) {
			return false;
		}
		for (std::size_t index = 0; index < aThread.Related.size(); ++index) {
			if (!well_formed(aThread.Related[index], index)) {
				return false;
			}
		}
		return true;
	}

	auto valid(const cycle_feedback_revision& aRevision) -> bool
	{
		const auto& id         = aRevision.Identity;
		const auto  provenance = aRevision.Author.Provenance;
		if (!valid_reference(id) || !valid(aRevision.Author) || aRevision.Body.size() > private_body_limit(provenance)
			|| !valid_private_completion(provenance, aRevision.Completion) || id.Digest != private_digest(aRevision.Body)
			|| (aRevision.ReplyTo && !valid_prior(id, *aRevision.ReplyTo)) || aRevision.Predecessor.has_value() != aRevision.EditDerivation.has_value()) {
			return false;
		}
		if (!aRevision.Predecessor) {
			return true;
		}
		const auto derivation = *aRevision.EditDerivation;
		return valid_prior(id, *aRevision.Predecessor) && aRevision.Predecessor->Position.Turn == id.Position.Turn && provenance == authored_provenance::human
			&& (derivation == private_derivation_kind::edited_from || derivation == private_derivation_kind::adopted_from_incomplete_run);
	}

	auto begin_feedback(const review_cycle_state& aCycle, const std::uint64_t aExpectedRevision, const begin_cycle_feedback& aCommand)
		-> std::expected<begun_cycle_feedback, cycle_refusal>
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
		if (aCommand.Related.size() > maximum_cycle_related_references || aCommand.Body.size() > private_body_limit(authored_provenance::human)) {
			return std::unexpected{ cycle_refusal::bounds };
		}
		if (!valid_target(aCycle, aCommand.Primary)) {
			return std::unexpected{ cycle_refusal::invalid_target };
		}
		for (std::size_t index = 0; index < aCommand.Related.size(); ++index) {
			if (!well_formed(aCommand.Related[index], index) || !valid_target(aCycle, aCommand.Related[index].Target)) {
				return std::unexpected{ cycle_refusal::invalid_target };
			}
		}
		auto       cycle = aCycle;
		const auto item  = take_next(cycle.Next.FeedbackItem);
		if (!item || cycle.Revision >= maximum_cycle_identity) {
			return std::unexpected{ cycle_refusal::exhausted };
		}
		++cycle.Revision;
		const auto id     = cycle_feedback_item_ref{ .Cycle = aCycle.Subject.Cycle, .Id = cycle_feedback_item_id{ *item } };
		auto       thread = cycle_feedback_thread{
			.Id       = id,
			.Primary  = aCommand.Primary,
			.Related  = aCommand.Related,
			.Lineage  = { .Version = 1, .LastOrder = private_vertex_order{ 1 }, .Lifecycle = private_thread_lifecycle::open },
			.NextTurn = 2,
		};
		auto revision = cycle_feedback_revision{
			.Identity = { .Item = id, .Position = { private_review_turn_id{ 1 }, private_review_turn_revision_id{ 1 }, private_vertex_order{ 1 } }, .Digest = private_digest(aCommand.Body) },
			.Author   = { },
			.Body     = aCommand.Body,
		};
		return begun_cycle_feedback{ .Cycle = std::move(cycle), .Thread = std::move(thread), .Revision = std::move(revision) };
	}

	auto apply_feedback(const review_cycle_state& aCycle, const cycle_feedback_thread& aThread, const std::uint64_t aExpectedVersion,
						const cycle_feedback_change& aChange, const cycle_feedback_revision* aNamed) -> std::expected<cycle_feedback_outcome, cycle_refusal>
	{
		if (!valid(aCycle.Subject) || aThread.Id.Cycle != aCycle.Subject.Cycle || !valid(aThread)) {
			return std::unexpected{ cycle_refusal::invalid_identity };
		}
		if (aCycle.Lifecycle == cycle_lifecycle::archived) {
			return std::unexpected{ cycle_refusal::archived };
		}
		auto outcome = cycle_feedback_outcome{ .Thread = aThread };
		if (const auto* lifecycle = std::get_if<set_feedback_lifecycle>(&aChange)) {
			if (auto refusal = change_private_lifecycle(outcome.Thread.Lineage, aExpectedVersion, lifecycle->Lifecycle)) {
				return std::unexpected{ from_lineage(*refusal) };
			}
			outcome.Changed = outcome.Thread.Lineage != aThread.Lineage;
			return outcome;
		}
		if (auto refusal = private_write_refusal(aThread.Lineage, aExpectedVersion)) {
			return std::unexpected{ from_lineage(*refusal) };
		}
		const auto named = [&](const private_review_turn_revision_id aId) {
			return aNamed != nullptr && valid(*aNamed) && aNamed->Identity.Item == aThread.Id && aNamed->Identity.Position.Revision == aId
				&& aNamed->Identity.Position.Order <= aThread.Lineage.LastOrder;
		};
		if (const auto* reference = std::get_if<add_feedback_reference>(&aChange)) {
			if (aThread.Related.size() >= maximum_cycle_related_references) {
				return std::unexpected{ cycle_refusal::bounds };
			}
			if (!well_formed(reference->Reference, aThread.Related.size()) || !valid_target(aCycle, reference->Reference.Target)) {
				return std::unexpected{ cycle_refusal::invalid_target };
			}
			outcome.Thread.Related.push_back(reference->Reference);
			++outcome.Thread.Lineage.Version;
			outcome.Changed = true;
			return outcome;
		}
		auto revision = cycle_feedback_revision{ .Identity = { .Item = aThread.Id } };
		auto turn     = std::optional<private_review_turn_id>{ };
		if (const auto* append = std::get_if<append_feedback_turn>(&aChange)) {
			if (!valid(append->Author)) {
				return std::unexpected{ cycle_refusal::invalid_author };
			}
			if (!valid_private_completion(append->Author.Provenance, append->Completion)) {
				return std::unexpected{ cycle_refusal::invalid_completion };
			}
			if (append->Body.size() > private_body_limit(append->Author.Provenance)) {
				return std::unexpected{ cycle_refusal::bounds };
			}
			if (append->ReplyTo) {
				if (!named(*append->ReplyTo)) {
					return std::unexpected{ cycle_refusal::missing_source };
				}
				revision.ReplyTo = aNamed->Identity;
			}
			revision.Author     = append->Author;
			revision.Body       = append->Body;
			revision.Completion = append->Completion;
		} else {
			const auto& edit = std::get<edit_feedback_turn>(aChange);
			if (aNamed == nullptr || !valid(*aNamed) || aNamed->Identity.Item != aThread.Id) {
				return std::unexpected{ cycle_refusal::missing_source };
			}
			// `aNamed` is the latest revision of the edited turn; naming an older one is stale.
			if (!named(edit.Latest)) {
				return std::unexpected{ cycle_refusal::conflict };
			}
			if (aNamed->Completion != private_revision_completion::complete && !edit.AdoptIncomplete) {
				return std::unexpected{ cycle_refusal::incomplete_source };
			}
			if (edit.Body.size() > private_body_limit(authored_provenance::human)) {
				return std::unexpected{ cycle_refusal::bounds };
			}
			turn                    = aNamed->Identity.Position.Turn;
			revision.Body           = edit.Body;
			revision.Predecessor    = aNamed->Identity;
			revision.ReplyTo        = aNamed->ReplyTo;
			revision.EditDerivation = private_edit_derivation(aNamed->Completion);
		}
		const auto position = next_position(aThread, turn);
		if (!position) {
			return std::unexpected{ cycle_refusal::exhausted };
		}
		if (auto refusal = advance_private_lineage(outcome.Thread.Lineage, position->Order)) {
			return std::unexpected{ from_lineage(*refusal) };
		}
		if (!turn) {
			++outcome.Thread.NextTurn;
		}
		revision.Identity.Position = *position;
		revision.Identity.Digest   = private_digest(revision.Body);
		outcome.Revision           = std::move(revision);
		outcome.Changed            = true;
		return outcome;
	}

	auto stable_name(const cycle_correspondence aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_correspondence::exact_material: return "exact_material";
		case cycle_correspondence::exact_mapped:   return "exact_mapped";
		case cycle_correspondence::ambiguous:      return "ambiguous";
		case cycle_correspondence::deleted:        return "deleted";
		case cycle_correspondence::unavailable:    return "unavailable";
		case cycle_correspondence::unmapped:       return "unmapped";
		}
		return "invalid";
	}

	auto observe_correspondence(const cycle_feedback_target& aTarget, const cycle_current_source& aCurrent) -> std::optional<cycle_target_correspondence>
	{
		const auto* file   = std::get_if<cycle_file_target>(&aTarget);
		const auto* anchor = std::get_if<cycle_source_anchor>(&aTarget);
		if (file == nullptr && anchor == nullptr) {
			return std::nullopt;
		}
		auto result = cycle_target_correspondence{ .Kind = cycle_correspondence::unavailable, .Against = aCurrent.Revision };
		if (aCurrent.State == cycle_path_state::deleted || aCurrent.State == cycle_path_state::absent) {
			result.Kind = cycle_correspondence::deleted;
			return result;
		}
		if (aCurrent.State != cycle_path_state::present || !aCurrent.Blob) {
			return result;
		}
		const auto& original = file != nullptr ? file->Blob : anchor->Blob;
		if (*aCurrent.Blob == original) {
			result.Kind = cycle_correspondence::exact_material;
			if (anchor != nullptr) {
				result.Range = std::pair{ anchor->Start, anchor->End };
			}
			return result;
		}
		if (file != nullptr) {
			result.Kind = cycle_correspondence::exact_mapped;
			return result;
		}
		if (!aCurrent.Bytes || !classify_material(*aCurrent.Bytes).text()) {
			return result;
		}
		return correspond_anchor(*anchor, text_of(*aCurrent.Bytes), aCurrent.Revision);
	}
}
