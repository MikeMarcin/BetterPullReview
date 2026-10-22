#include "bpr/application/off_hunk_fact_port.hpp"
#include "bpr/application/review_mutation_recovery_port.hpp"

#include <array>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto bounded(const std::string& aText) noexcept -> bool
		{
			return !aText.empty() && aText.size() <= maximum_off_hunk_fact_text_bytes;
		}

		/// \brief The scope's kept entry for one operation, when it is current.
		[[nodiscard]] auto current_entry(off_hunk_fact_port& aFacts, const off_hunk_fact_scope& aScope, const core::off_hunk_fact_operation aOperation, const std::int64_t aNow)
			-> std::optional<off_hunk_fact_entry>
		{
			const auto loaded = aFacts.load_scope(aScope, aOperation);
			if (!loaded || !*loaded || !valid((**loaded).Key) || !valid((**loaded).Fact) || (**loaded).Key.Scope != aScope
				|| core::current_off_hunk_fact((**loaded).Key.Product, (**loaded).Fact.Observation, aNow) == core::off_hunk_creation_capability::unknown) {
				return std::nullopt;
			}
			return *loaded;
		}

		/// \brief The newer of the scope's current single-comment and review
		/// entries, as `core::newer_off_hunk_fact` orders them.
		[[nodiscard]] auto review_entry(off_hunk_fact_port& aFacts, const off_hunk_fact_scope& aScope, const std::int64_t aNow) -> std::optional<off_hunk_fact_entry>
		{
			const auto single = current_entry(aFacts, aScope, core::off_hunk_fact_operation::single_comment, aNow);
			const auto review = current_entry(aFacts, aScope, core::off_hunk_fact_operation::review, aNow);
			if (!single || !review) {
				return single ? single : review;
			}
			return core::newer_off_hunk_fact(single->Fact.Observation, review->Fact.Observation) == single->Fact.Observation ? single : review;
		}
	}

	auto valid(const off_hunk_fact_scope& aScope) noexcept -> bool
	{
		return aScope.Profile != 0U && !aScope.Host.empty() && aScope.Host.size() <= 253U && aScope.Account != 0U && aScope.RepositoryDatabaseId != 0U;
	}

	auto valid(const off_hunk_fact_key& aKey) noexcept -> bool
	{
		return valid(aKey.Scope) && aKey.Operation <= core::off_hunk_fact_operation::review && aKey.ProfileRevision != 0U
			&& (aKey.Product == core::github_server_product::github_com || aKey.Product == core::github_server_product::github_enterprise_server)
			&& core::valid_github_server_version(aKey.Product, aKey.ServerVersion) && aKey.ApiVersion != 0U && aKey.ClassifierRevision != 0U;
	}

	auto valid(const off_hunk_fact& aFact) noexcept -> bool
	{
		return core::valid(aFact.Observation) && bounded(aFact.Repository) && aFact.PullRequest != 0U && bounded(aFact.Path);
	}

	auto presented_off_hunk_fact(off_hunk_fact_port* const aFacts, const off_hunk_fact_scope& aScope, const std::int64_t aNowUtcMs) -> core::off_hunk_creation_capability
	{
		if (aFacts == nullptr || !valid(aScope)) {
			return core::off_hunk_creation_capability::unknown;
		}
		const auto entry = current_entry(*aFacts, aScope, core::off_hunk_fact_operation::single_comment, aNowUtcMs);
		return entry ? entry->Fact.Observation.State : core::off_hunk_creation_capability::unknown;
	}

	auto presented_review_off_hunk_fact(off_hunk_fact_port* const aFacts, const off_hunk_fact_scope& aScope, const std::int64_t aNowUtcMs) -> core::off_hunk_creation_capability
	{
		if (aFacts == nullptr || !valid(aScope)) {
			return core::off_hunk_creation_capability::unknown;
		}
		const auto entry = review_entry(*aFacts, aScope, aNowUtcMs);
		return entry ? entry->Fact.Observation.State : core::off_hunk_creation_capability::unknown;
	}

	auto demote_off_hunk_fact(off_hunk_fact_port* const aFacts, const off_hunk_fact_scope& aScope, off_hunk_fact aComment, const std::int64_t aNowUtcMs) -> bool
	{
		if (aFacts == nullptr || !valid(aScope)) {
			return false;
		}
		const auto entry  = review_entry(*aFacts, aScope, aNowUtcMs);
		const auto taught = core::learn_off_hunk_fact(core::off_hunk_answer::refused_naming_line, aNowUtcMs);
		if (!entry || entry->Fact.Observation.State != core::off_hunk_creation_capability::supported || !taught) {
			return false;
		}
		aComment.Observation = *taught;
		if (!valid(aComment)) {
			return false;
		}
		auto key = entry->Key;
		for (const auto operation : std::array{ core::off_hunk_fact_operation::single_comment, core::off_hunk_fact_operation::review }) {
			key.Operation = operation;
			if (!aFacts->record(key, aComment)) {
				return false;
			}
		}
		return true;
	}
}
