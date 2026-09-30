#include "bpr/application/review_cycle_views.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace bpr::application {
	namespace {
		auto failed(const cycle_store_failure aFailure) -> std::unexpected<cycle_store_error>
		{
			return std::unexpected(cycle_store_error{ .Failure = aFailure });
		}

		/// Sender revisions that rounds attached to this cycle mapped onto it.
		auto imported_revisions(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const std::stop_token& aStop)
			-> cycle_store_result<std::vector<core::cycle_imported_revision>>
		{
			auto found = std::vector<core::cycle_imported_revision>{ };
			auto after = std::optional<core::cycle_import_id>{ };
			do {
				auto page = aStore.imports(after, aStop);
				if (!page) {
					return std::unexpected(page.error());
				}
				for (const auto& import : page->Imports) {
					if (!import.Association || import.Association->Cycle != aSubject) {
						continue;
					}
					for (const auto& mapping : import.Association->Revisions) {
						found.push_back({ .ForeignCycle    = import.Summary.ForeignCycle,
										  .ForeignRound    = import.Summary.ForeignRound,
										  .ForeignRevision = mapping.Foreign,
										  .Local           = mapping.Local });
					}
				}
				after = page->Next;
			} while (after);
			return found;
		}
	}

	auto load_view_facts(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, std::stop_token aStop) -> cycle_store_result<core::cycle_view_facts>
	{
		auto facts = core::cycle_view_facts{ };
		auto last  = aStore.last_review(aSubject, aStop);
		if (!last) {
			return std::unexpected(last.error());
		}
		facts.LastReview = *last;
		auto after       = std::optional<core::cycle_round_id>{ };
		do {
			auto page = aStore.rounds(aSubject, after, aStop);
			if (!page) {
				return std::unexpected(page.error());
			}
			for (const auto& round : page->Rounds) {
				auto bytes = aStore.read_object(round.Payload, aStop);
				if (!bytes) {
					return std::unexpected(bytes.error());
				}
				const auto decoded = core::decode_round_payload({ reinterpret_cast<const char*>(bytes->data()), bytes->size() });
				if (!decoded || decoded->Digest != round.Round.Digest) {
					return failed(cycle_store_failure::corrupt);
				}
				if (const auto start = core::round_start(decoded->Payload.SourceRevision)) {
					facts.Rounds.push_back({ .Round = round.Round.Id.Id, .Start = *start });
				}
			}
			after = page->Next;
		} while (after);
		return facts;
	}

	auto load_cycle_history(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject,
							const std::optional<core::cycle_source_revision_id> aAfter, const std::size_t aLimit, std::stop_token aStop)
		-> cycle_store_result<cycle_history_page>
	{
		const auto limit = std::clamp<std::size_t>(aLimit, 1U, maximum_cycle_history_page);
		// The revision `aAfter` names is read too, as the context the first
		// listed capture is described against.
		auto cursor  = aAfter && aAfter->value() > 1U ? std::optional{ core::cycle_source_revision_id{ aAfter->value() - 1U } } : std::nullopt;
		auto listed  = std::vector<stored_cycle_revision>{ };
		auto more    = false;
		auto wanted  = limit + (aAfter ? 1U : 0U);
		auto reading = true;
		while (reading) {
			auto page = aStore.revisions(aSubject, cursor, aStop);
			if (!page) {
				return std::unexpected(page.error());
			}
			for (auto& revision : page->Revisions) {
				if (listed.size() == wanted) {
					more = true;
					break;
				}
				listed.push_back(std::move(revision));
			}
			cursor  = page->Next;
			more    = more || (listed.size() == wanted && page->Next.has_value());
			reading = cursor.has_value() && listed.size() < wanted;
		}
		auto sources = std::vector<core::cycle_revision_source>{ };
		sources.reserve(listed.size());
		for (const auto& revision : listed) {
			const auto manifest = aHost.manifest(aSubject, revision.Revision.Id.Id, aStop);
			if (!manifest) {
				return failed(manifest.error() == review_material_status::cancelled     ? cycle_store_failure::cancelled
								  : manifest.error() == review_material_status::missing ? cycle_store_failure::object_unavailable
																						: cycle_store_failure::corrupt);
			}
			const auto& facts = **manifest;
			sources.push_back({ .Revision      = revision.Revision,
								.HeadReference = facts.HeadReference,
								.HeadCommit    = facts.HeadCommit,
								.Repository    = facts.ObjectFormat != core::git_hash_algorithm::invalid });
		}
		auto imported = imported_revisions(aStore, aSubject, aStop);
		if (!imported) {
			return std::unexpected(imported.error());
		}
		auto history = core::describe_history(sources, *imported);
		if (aAfter && !history.Entries.empty() && history.Entries.front().Revision == *aAfter) {
			history.Entries.erase(history.Entries.begin());
		}
		auto page = cycle_history_page{ .History = std::move(history) };
		if (more && !page.History.Entries.empty()) {
			page.Next = page.History.Entries.back().Revision;
		}
		return page;
	}
}
