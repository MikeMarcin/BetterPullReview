#include "bpr/application/review_publication_capability_cache.hpp"
#include <cstdlib>
#include <iostream>

namespace {
	using namespace bpr;
	void expect(const bool aCondition, const char* aMessage)
	{
		if (!aCondition) {
			std::cerr << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	[[nodiscard]] auto key() -> application::review_publication_capability_key
	{
		return { .Profile = core::host_profile_id{ 1 }, .ProfileRevision = application::github_profile_revision{ 1 }, .Host = core::git_host_id{ 2 }, .Hostname = application::github_canonical_hostname{ "github.com" }, .Actor = { .Account = core::github_account_id{ 3 }, .DatabaseId = 4, .NodeId = "U_4", .Login = "fixture" }, .RepositoryDatabaseId = 5, .RepositoryNodeId = "R_5", .FreshnessGeneration = 1, .AnchorDigest = { 1 } };
	}
}
int main()
{
	using namespace bpr;
	using state  = core::off_hunk_creation_capability;
	using source = application::review_publication_evidence_source;
	{
		application::review_publication_capability_cache operations;
		const auto                                       exact = application::review_publication_capability_evidence{ .State = state::supported, .Source = source::recorded_contract, .ContractDigest = { 1 }, .ObservedAt = 100, .ExpiresAt = 200 };
		expect(operations.record(key(), exact, 100), "standalone exact source observed");
		for (auto operation : { application::review_publication_operation::create_pending_line_batch, application::review_publication_operation::create_pending_off_hunk_batch, application::review_publication_operation::add_pending_file_thread, application::review_publication_operation::create_standalone_file_comment, application::review_publication_operation::create_tour_conversation_comment }) {
			auto selected      = key();
			selected.Operation = operation;
			expect(!operations.lookup(selected, 100), "standalone evidence cannot qualify a different tour operation");
			expect(operations.record(selected, exact, 100) && operations.lookup(selected, 100)->State == state::supported, "only exact operation evidence qualifies");
			selected.AnchorDigest[0] = 2;
			expect(!operations.lookup(selected, 100), "another source range cannot borrow evidence");
		}
	}
	application::review_publication_capability_cache cache;
	auto                                             evidence = application::review_publication_capability_evidence{ .State = state::unsupported, .Source = source::observed_definitive_unsupported, .ContractDigest = { 1 }, .ObservedAt = 100, .ExpiresAt = 400 };
	expect(!cache.lookup(key(), 100), "empty is unknown");
	expect(cache.record(key(), evidence, 100) && cache.lookup(key(), 399)->State == state::unsupported, "exact current evidence retained");
	expect(!cache.lookup(key(), 400) && !cache.lookup(key(), 99), "expired and backward clock become unknown");
	for (int field = 0; field < 13; ++field) {
		auto changed = key();
		switch (field) {
		case 0:  changed.Profile = core::host_profile_id{ 2 }; break;
		case 1:  changed.ProfileRevision = application::github_profile_revision{ 2 }; break;
		case 2:  changed.Host = core::git_host_id{ 3 }; break;
		case 3:  changed.Hostname = application::github_canonical_hostname{ "reviews.example.test" }; break;
		case 4:  changed.ServerContract = application::github_server_contract::ghes_3_18_2022_11_28; break;
		case 5:  changed.Actor.DatabaseId = 8; break;
		case 6:  changed.RepositoryDatabaseId = 6; break;
		case 7:  changed.RepositoryNodeId = "R_6"; break;
		case 8:  changed.Operation = static_cast<application::review_publication_operation>(1); break;
		case 9:  changed.ApiRevision = 20221129; break;
		case 10: changed.ClassifierRevision = 2; break;
		case 11: changed.FreshnessGeneration = 2; break;
		case 12: changed.AnchorDigest[0] = 2; break;
		default: expect(false, "the fixture selects a known identity field"); break;
		}
		expect(!cache.lookup(changed, 100), "changed authority/operation/revision never borrows evidence");
	}
	evidence.Source = source::observed_native_success;
	expect(!cache.record(key(), evidence, 100), "success cannot record unsupported");
	evidence.State = state::supported;
	expect(cache.record(key(), evidence, 100), "native success records supported");
	evidence.State = state::unknown;
	expect(!cache.record(key(), evidence, 100), "ambiguous outcomes supply no evidence");
	evidence.State = state::supported;
	for (std::size_t index = 1; index < application::review_publication_capability_cache::maximum_entries; ++index) {
		auto changed = key();
		changed.RepositoryDatabaseId += index;
		expect(cache.record(changed, evidence, 100), "bounded entries admitted");
	}
	auto excess                 = key();
	excess.RepositoryDatabaseId = 999;
	expect(!cache.record(excess, evidence, 100), "capacity refuses new evidence");
	evidence.ObservedAt = 400;
	evidence.ExpiresAt  = 700;
	expect(cache.record(excess, evidence, 400), "expired entries reclaimed");
	evidence.ExpiresAt = 701;
	expect(!cache.record(key(), evidence, 400), "unbounded lifetime refused");
	return EXIT_SUCCESS;
}
