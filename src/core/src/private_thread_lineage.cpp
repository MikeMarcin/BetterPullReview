#include "bpr/core/private_thread_lineage.hpp"
#include "bpr/core/sha256.hpp"

namespace bpr::core {
	auto private_digest(std::string_view aBody) noexcept -> private_body_digest
	{
		return private_body_digest{ sha256({ reinterpret_cast<const std::uint8_t*>(aBody.data()), aBody.size() }) };
	}

	auto private_body_limit(const authored_provenance aProvenance) noexcept -> std::size_t
	{
		return aProvenance == authored_provenance::human ? maximum_private_reviewer_body_bytes : maximum_private_assistant_body_bytes;
	}

	auto valid_private_completion(const authored_provenance aProvenance, const private_revision_completion aCompletion) noexcept -> bool
	{
		return aCompletion <= private_revision_completion::cancelled
			&& (aProvenance == authored_provenance::adapter_observed || aCompletion == private_revision_completion::complete);
	}

	auto valid_private_lineage(const private_thread_lineage& aLineage) noexcept -> bool
	{
		return aLineage.Version > 0 && aLineage.Version <= maximum_private_identity && valid_private_id(aLineage.LastOrder)
			&& aLineage.Lifecycle <= private_thread_lifecycle::archived;
	}

	auto private_write_refusal(const private_thread_lineage& aLineage, const std::uint64_t aExpectedVersion) noexcept -> std::optional<private_review_refusal>
	{
		if (!valid_private_lineage(aLineage)) {
			return private_review_refusal::invalid_identity;
		}
		if (aExpectedVersion != aLineage.Version) {
			return private_review_refusal::conflict;
		}
		if (aLineage.Version == maximum_private_identity) {
			return private_review_refusal::bounds;
		}
		if (aLineage.Lifecycle == private_thread_lifecycle::archived) {
			return private_review_refusal::archived;
		}
		return std::nullopt;
	}

	auto advance_private_lineage(private_thread_lineage& aLineage, const private_vertex_order aOrder) noexcept -> std::optional<private_review_refusal>
	{
		if (aOrder <= aLineage.LastOrder) {
			return private_review_refusal::cycle;
		}
		if (aLineage.Version >= maximum_private_identity) {
			return private_review_refusal::bounds;
		}
		aLineage.LastOrder = aOrder;
		++aLineage.Version;
		return std::nullopt;
	}

	auto change_private_lifecycle(private_thread_lineage& aLineage, const std::uint64_t aExpectedVersion, const private_thread_lifecycle aLifecycle) noexcept
		-> std::optional<private_review_refusal>
	{
		if (!valid_private_lineage(aLineage)) {
			return private_review_refusal::invalid_identity;
		}
		if (aLineage.Version != aExpectedVersion) {
			return private_review_refusal::conflict;
		}
		if (aLifecycle > private_thread_lifecycle::archived) {
			return private_review_refusal::invalid_transition;
		}
		if (aLineage.Lifecycle == aLifecycle) {
			return std::nullopt;
		}
		if (aLineage.Version == maximum_private_identity) {
			return private_review_refusal::bounds;
		}
		aLineage.Lifecycle = aLifecycle;
		++aLineage.Version;
		return std::nullopt;
	}

	auto private_prior_precedes(const private_lineage_position& aNew, const private_lineage_position& aPrior) noexcept -> bool
	{
		return valid_private_id(aPrior.Turn) && valid_private_id(aPrior.Revision) && valid_private_id(aPrior.Order)
			&& aPrior.Order < aNew.Order && aPrior.Revision != aNew.Revision;
	}

	auto private_edit_derivation(const private_revision_completion aPrior) noexcept -> private_derivation_kind
	{
		return aPrior == private_revision_completion::complete ? private_derivation_kind::edited_from : private_derivation_kind::adopted_from_incomplete_run;
	}

	auto stable_name(const private_thread_lifecycle aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case private_thread_lifecycle::open:             return "open";
		case private_thread_lifecycle::resolved_locally: return "resolved_locally";
		case private_thread_lifecycle::archived:         return "archived";
		}
		return "invalid";
	}
	auto stable_name(const private_revision_completion aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case private_revision_completion::complete:   return "complete";
		case private_revision_completion::incomplete: return "incomplete";
		case private_revision_completion::failed:     return "failed";
		case private_revision_completion::cancelled:  return "cancelled";
		}
		return "invalid";
	}
	auto stable_name(const authored_provenance aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case authored_provenance::human:            return "human";
		case authored_provenance::adapter_observed: return "adapter_observed";
		case authored_provenance::self_reported:    return "self_reported";
		case authored_provenance::unknown:          return "unknown";
		}
		return "invalid";
	}
	auto stable_name(const private_review_refusal aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case private_review_refusal::invalid_identity:   return "invalid_identity";
		case private_review_refusal::invalid_binding:    return "invalid_binding";
		case private_review_refusal::invalid_subject:    return "invalid_subject";
		case private_review_refusal::invalid_actor:      return "invalid_actor";
		case private_review_refusal::invalid_completion: return "invalid_completion";
		case private_review_refusal::bounds:             return "bounds";
		case private_review_refusal::conflict:           return "conflict";
		case private_review_refusal::archived:           return "archived";
		case private_review_refusal::invalid_transition: return "invalid_transition";
		case private_review_refusal::missing_source:     return "missing_source";
		case private_review_refusal::wrong_scope:        return "wrong_scope";
		case private_review_refusal::digest_mismatch:    return "digest_mismatch";
		case private_review_refusal::incomplete_source:  return "incomplete_source";
		case private_review_refusal::duplicate_source:   return "duplicate_source";
		case private_review_refusal::cycle:              return "cycle";
		case private_review_refusal::ambiguous_target:   return "ambiguous_target";
		case private_review_refusal::invalid_target:     return "invalid_target";
		case private_review_refusal::source_changed:     return "source_changed";
		case private_review_refusal::invalid_provenance: return "invalid_provenance";
		case private_review_refusal::invalid_mode:       return "invalid_mode";
		}
		return "invalid";
	}
}
