#include "bpr/core/review_cycle_verification.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <charconv>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>

namespace bpr::core {
	namespace {
		auto allocated(const review_cycle_state& aCycle, const cycle_source_revision_id aRevision) -> bool
		{
			return valid_cycle_id(aRevision) && aRevision.value() < aCycle.Next.SourceRevision;
		}

		/// The file a target names, if it names one.
		auto file_of(const cycle_feedback_target& aTarget) -> const std::string*
		{
			if (const auto* file = std::get_if<cycle_file_target>(&aTarget)) {
				return &file->Path;
			}
			if (const auto* anchor = std::get_if<cycle_source_anchor>(&aTarget)) {
				return &anchor->Path;
			}
			return nullptr;
		}

		/// A positive identity written in decimal without a leading zero.
		auto positive(const std::string_view aDigits) -> std::optional<std::uint64_t>
		{
			const auto digits = aDigits;
			if (digits.empty() || digits.size() > 19U || digits.front() == '0') {
				return std::nullopt;
			}
			auto       value  = std::uint64_t{ 0 };
			const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value);
			if (parsed.ec != std::errc{ } || parsed.ptr != digits.data() + digits.size() || value == 0U || value > maximum_cycle_identity) {
				return std::nullopt;
			}
			return value;
		}

		/// `<prefix><n>` with n a positive identity.
		auto numbered(const std::string_view aText, const std::string_view aPrefix) -> std::optional<std::uint64_t>
		{
			return aText.starts_with(aPrefix) ? positive(aText.substr(aPrefix.size())) : std::nullopt;
		}

		auto effective(const std::span<const cycle_verification_record> aDecisions) -> const cycle_verification_record*
		{
			return aDecisions.empty() ? nullptr : &aDecisions.back();
		}
	}

	auto stable_name(const cycle_reviewer_decision aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_reviewer_decision::verified:   return "verified";
		case cycle_reviewer_decision::needs_work: return "needs_work";
		case cycle_reviewer_decision::withdrawn:  return "withdrawn";
		case cycle_reviewer_decision::deferred:   return "deferred";
		}
		return "unknown";
	}

	auto stable_name(const cycle_verification_currency aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_verification_currency::current:          return "current";
		case cycle_verification_currency::material_changed: return "material_changed";
		case cycle_verification_currency::item_amended:     return "item_amended";
		}
		return "unknown";
	}

	auto stable_name(const cycle_item_status aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_item_status::open:               return "open";
		case cycle_item_status::reported:           return "reported";
		case cycle_item_status::needs_verification: return "needs_verification";
		case cycle_item_status::verified:           return "verified";
		case cycle_item_status::recheck_needed:     return "recheck_needed";
		case cycle_item_status::needs_work:         return "needs_work";
		case cycle_item_status::withdrawn:          return "withdrawn";
		case cycle_item_status::deferred:           return "deferred";
		}
		return "unknown";
	}

	auto verification_material(const cycle_feedback_thread& aThread, const cycle_manifest& aManifest, const cycle_material_digest& aManifestDigest) -> cycle_material_digest
	{
		auto paths  = std::set<std::string_view>{ };
		auto broad  = false;
		auto gather = [&](const cycle_feedback_target& aTarget) {
			if (const auto* path = file_of(aTarget)) {
				paths.insert(*path);
			} else {
				broad = true;
			}
		};
		gather(aThread.Primary);
		for (const auto& related : aThread.Related) {
			gather(related.Target);
		}
		// A note on a module or the whole cycle is about everything captured.
		if (broad) {
			return aManifestDigest;
		}
		constexpr auto domain = std::string_view{ "BPR-cycle-verification-material-v1" };
		auto           bytes  = std::vector<std::uint8_t>(domain.begin(), domain.end());
		bytes.push_back(0);
		for (const auto path : paths) {
			bytes.insert(bytes.end(), path.begin(), path.end());
			bytes.push_back(0);
			const auto* entry = find_entry(aManifest, path);
			if (entry != nullptr && entry->Content) {
				bytes.push_back(1);
				bytes.insert(bytes.end(), entry->Content->bytes().begin(), entry->Content->bytes().end());
			} else {
				// Absent, unreadable or not captured: named as such, never as some other bytes.
				bytes.push_back(0);
			}
		}
		return cycle_material_digest{ sha256(bytes) };
	}

	auto check_verification(const review_cycle_state& aCycle, const cycle_feedback_thread& aThread, const private_review_turn_revision_id aCurrentWording,
							const cycle_verification_record& aRecord) -> std::optional<cycle_refusal>
	{
		if (aRecord.Item != aThread.Id.Id || aThread.Id.Cycle != aCycle.Subject.Cycle || !valid_cycle_id(aRecord.Item)) {
			return cycle_refusal::invalid_identity;
		}
		if (!aCycle.Latest || !allocated(aCycle, aRecord.Checked)) {
			return cycle_refusal::invalid_target;
		}
		if (aRecord.Wording != aCurrentWording) {
			return cycle_refusal::conflict;
		}
		return std::nullopt;
	}

	auto verification_currency(const cycle_verification_record& aRecord, const private_review_turn_revision_id aCurrentWording, const cycle_material_digest& aCurrentMaterial)
		-> cycle_verification_currency
	{
		if (aRecord.Wording != aCurrentWording) {
			return cycle_verification_currency::item_amended;
		}
		return aRecord.Material == aCurrentMaterial ? cycle_verification_currency::current : cycle_verification_currency::material_changed;
	}

	auto reconcile_item(const cycle_reconciliation_inputs& aInputs) -> cycle_item_reconciliation
	{
		auto result = cycle_item_reconciliation{ .Against = aInputs.Against, .Primary = aInputs.Primary };
		if (aInputs.Thread != nullptr) {
			result.Item = aInputs.Thread->Id.Id;
		}
		result.Suggestions.assign(aInputs.Suggestions.begin(), aInputs.Suggestions.end());
		for (const auto& statement : aInputs.Statements) {
			if (statement.Disposition) {
				result.Claim = statement.Disposition;
			}
		}
		const auto* decision = effective(aInputs.Decisions);
		if (decision != nullptr) {
			result.Decision = *decision;
			result.Currency = verification_currency(*decision, aInputs.Wording, aInputs.Material);
		}
		if (decision != nullptr && decision->Decision) {
			switch (*decision->Decision) {
			case cycle_reviewer_decision::verified:
				result.Status = result.Currency == cycle_verification_currency::current ? cycle_item_status::verified : cycle_item_status::recheck_needed;
				return result;
			case cycle_reviewer_decision::needs_work: result.Status = cycle_item_status::needs_work; return result;
			case cycle_reviewer_decision::withdrawn:  result.Status = cycle_item_status::withdrawn; return result;
			case cycle_reviewer_decision::deferred:   result.Status = cycle_item_status::deferred; return result;
			}
		}
		// No current decision: only what recipients said. Correspondence and
		// suggestion observations are shown beside it, and decide nothing.
		if (result.Claim == cycle_response_disposition::addressed || result.Claim == cycle_response_disposition::partially_addressed) {
			result.Status = cycle_item_status::needs_verification;
		} else if (!aInputs.Statements.empty()) {
			result.Status = cycle_item_status::reported;
		}
		return result;
	}

	auto resolved(const cycle_item_reconciliation& aItem) noexcept -> bool
	{
		return aItem.Status == cycle_item_status::verified || aItem.Status == cycle_item_status::withdrawn;
	}

	auto round_assignment(const cycle_round_payload& aRound) -> std::optional<cycle_round_selection>
	{
		const auto source = numbered(aRound.SourceRevision, "R");
		if (!source) {
			return std::nullopt;
		}
		auto selection = cycle_round_selection{
			.Source      = cycle_source_revision_id{ *source },
			.Action      = aRound.Assignment.Action,
			.Goal        = aRound.Assignment.Goal,
			.Constraints = aRound.Assignment.Constraints,
			.WriteScope  = aRound.Assignment.WriteScope,
		};
		for (const auto& wire : aRound.Items) {
			const auto item = numbered(wire.Id, "I");
			if (!item || !valid_cycle_id(private_review_turn_revision_id{ wire.Revision })) {
				return std::nullopt;
			}
			auto selected = cycle_round_item_selection{ .Item = cycle_feedback_item_id{ *item }, .Wording = private_review_turn_revision_id{ wire.Revision }, .Action = wire.ActionOverride };
			for (const auto& related : wire.Related) {
				const auto other = numbered(related, "I");
				if (!other) {
					return std::nullopt;
				}
				selected.Related.emplace_back(*other);
			}
			for (const auto& suggestion : wire.Suggestions) {
				// `SG<item>.<n>`: the note's own suggestion n.
				const auto dot = suggestion.Id.find('.');
				const auto of  = dot == std::string::npos ? std::nullopt : numbered(std::string_view{ suggestion.Id }.substr(0, dot), "SG");
				const auto nth = dot == std::string::npos ? std::nullopt : positive(std::string_view{ suggestion.Id }.substr(dot + 1U));
				if (!of || !nth || *of != *item) {
					return std::nullopt;
				}
				selected.Suggestions.push_back({ .Id = cycle_suggestion_id{ *nth }, .Revision = suggestion.Revision });
			}
			selection.Items.push_back(std::move(selected));
		}
		return selection;
	}

	auto plan_next_round(const cycle_round_selection& aPrevious, const std::span<const cycle_next_round_item> aItems, const cycle_source_revision_id aSource) -> cycle_next_round_plan
	{
		auto  plan       = cycle_next_round_plan{ };
		auto& next       = plan.Selection;
		next.Source      = aSource;
		next.Action      = aPrevious.Action;
		next.Goal        = aPrevious.Goal;
		next.Constraints = aPrevious.Constraints;
		next.WriteScope  = aPrevious.WriteScope;
		const auto find  = [&](const cycle_feedback_item_id aItem) -> const cycle_next_round_item* {
			const auto found = std::ranges::find(aItems, aItem, &cycle_next_round_item::Item);
			return found == aItems.end() ? nullptr : &*found;
		};
		for (const auto& previous : aPrevious.Items) {
			const auto* now = find(previous.Item);
			if (now == nullptr || now->Lifecycle == private_thread_lifecycle::archived) {
				plan.Archived.push_back(previous.Item);
				continue;
			}
			if (now->Resolved) {
				plan.Resolved.push_back(previous.Item);
				continue;
			}
			// The same assignment item, at its wording now, with the suggestions
			// it carried at their revisions now (one since removed is dropped).
			auto pins = std::vector<cycle_round_suggestion_pin>{ };
			for (const auto& pin : previous.Suggestions) {
				const auto current = std::ranges::find(now->Suggestions, pin.Id, &cycle_round_suggestion_pin::Id);
				if (current != now->Suggestions.end()) {
					pins.push_back(*current);
				}
			}
			next.Items.push_back({ .Item = previous.Item, .Wording = now->Wording, .Action = previous.Action, .Suggestions = std::move(pins), .Related = previous.Related });
			plan.Carried.push_back(previous.Item);
		}
		for (const auto& item : aItems) {
			const auto carried = std::ranges::find(next.Items, item.Item, &cycle_round_item_selection::Item) != next.Items.end();
			if (carried || item.InRound || item.Resolved || item.Lifecycle != private_thread_lifecycle::open) {
				continue;
			}
			next.Items.push_back({ .Item = item.Item, .Wording = item.Wording, .Suggestions = item.Suggestions });
			plan.Added.push_back(item.Item);
		}
		// A relation only to an item still in the round.
		for (auto& selected : next.Items) {
			std::erase_if(selected.Related, [&](const cycle_feedback_item_id aOther) {
				return std::ranges::find(next.Items, aOther, &cycle_round_item_selection::Item) == next.Items.end();
			});
		}
		return plan;
	}
}
