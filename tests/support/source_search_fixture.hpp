#ifndef BPR_TESTS_SUPPORT_SOURCE_SEARCH_FIXTURE_HPP
#define BPR_TESTS_SUPPORT_SOURCE_SEARCH_FIXTURE_HPP

#include "bpr/core/source_search.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace bpr::tests {

	inline void search_expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(1);
		}
	}

	template <typename TIdentity>
	inline auto search_identity(const std::uint8_t aSeed) -> TIdentity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		return TIdentity{ bytes };
	}

	inline auto search_scope(const core::source_search_target aTarget = core::source_search_target::right) -> core::source_search_scope
	{
		using namespace core;
		auto scope                      = source_search_scope{ };
		scope.Session                   = application_session_id{ 1U };
		scope.Authority.Profile         = host_profile_id{ 1U };
		scope.Authority.ProfileRevision = github_profile_revision{ 1U };
		scope.Authority.Host            = git_host_id{ 1U };
		scope.Authority.HostAuthority   = search_identity<host_authority_fingerprint>(1U);
		scope.Authority.Account         = github_account_id{ 1U };
		scope.Authority.Viewer          = github_viewer_actor_id{ 1U };
		scope.Authority.Repository      = repository_id{ 1U };
		scope.Authority.Cache           = search_identity<repository_cache_scope_id>(2U);
		scope.Authority.PullRequest     = pull_request_id{ 1U };
		scope.Authority.Number          = pull_request_number{ 1U };
		scope.Authority.Scope           = search_identity<pull_request_authority_scope_id>(3U);
		const auto incarnation          = search_identity<epoch_catalog_incarnation_id>(4U);
		scope.Comparison.Left           = { { scope.Authority.Scope, incarnation, comparison_anchor_kind::epoch_base, epoch_ordinal{ 1 }, { } }, *parse_git_commit_oid(std::string(40U, 'a')), anchor_availability::available };
		scope.Comparison.Right          = { { scope.Authority.Scope, incarnation, comparison_anchor_kind::epoch_head, epoch_ordinal{ 2 }, { } }, *parse_git_commit_oid(std::string(40U, 'b')), anchor_availability::available };
		scope.CatalogueGeneration       = 1U;
		scope.Tab                       = document_tab_id{ 1U };
		scope.Target                    = aTarget;
		if (aTarget != source_search_target::files) {
			scope.Document = source_search_document{ search_identity<file_identity>(5U), search_identity<file_comparison_digest>(6U), parse_git_blob_oid(std::string(40U, 'c')), parse_git_blob_oid(std::string(40U, 'd')) };
		}
		search_expect(valid(scope), "fixture supplies a valid exact search scope");
		return scope;
	}

	inline auto search_request(core::source_search_state& aState) -> core::source_search_request
	{
		const auto output = core::edit_source_search_query(aState, core::source_search_input::ready, { });
		search_expect(output.Work.has_value(), "valid nonempty query emits work");
		return *output.Work;
	}

	inline auto search_terminal(const core::source_search_request& aRequest, const std::uint32_t aMatches = 3U) -> core::source_search_terminal
	{
		auto result                  = core::source_search_terminal{ };
		result.Request               = aRequest;
		result.Status                = core::source_search_status::complete;
		result.Summary.Matches       = aMatches;
		result.Summary.EligibleFiles = 2U;
		result.Summary.SearchedFiles = 2U;
		return result;
	}
}
#endif
