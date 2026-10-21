#ifndef BPR_TESTS_CORE_PRIVATE_REVIEW_FIXTURE_HPP
#define BPR_TESTS_CORE_PRIVATE_REVIEW_FIXTURE_HPP
#include "bpr/core/private_review.hpp"
#include "product_diff.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>

namespace private_review_fixture {
	using namespace bpr::core;
	inline void expect(bool aValue, std::string_view aMessage)
	{
		if (!aValue) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	template <typename TIdentity>
	auto opaque(std::uint8_t aSeed = 1) -> TIdentity
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes[0] = aSeed;
		return TIdentity{ bytes };
	}
	inline auto binding() -> private_review_binding
	{
		const auto base = *parse_git_commit_oid("1111111111111111111111111111111111111111");
		const auto head = *parse_git_commit_oid("2222222222222222222222222222222222222222");
		return {
			.Scope         = { bpr_profile_id{ 1 }, opaque<private_ledger_incarnation>(), git_host_id{ 2 }, github_account_id{ 3 }, repository_id{ 4 }, pull_request_id{ 5 } },
			.Access        = github_access_mode::authenticated,
			.HostAuthority = opaque<host_authority_fingerprint>(),
			.Revision      = { git_host_id{ 2 }, repository_id{ 4 }, pull_request_id{ 5 }, base, head, epoch_ordinal{ 1 }, epoch_ordinal{ 2 } },
			.LeftAnchor    = { opaque<pull_request_authority_scope_id>(), opaque<epoch_catalog_incarnation_id>(), comparison_anchor_kind::epoch_base, epoch_ordinal{ 1 }, { } },
			.RightAnchor   = { opaque<pull_request_authority_scope_id>(), opaque<epoch_catalog_incarnation_id>(), comparison_anchor_kind::epoch_head, epoch_ordinal{ 2 }, { } },
			.Comparison    = { }
		};
	}
	inline auto input(std::uint64_t aId = 1) -> private_revision_input
	{
		return { private_review_turn_id{ aId }, private_review_turn_revision_id{ aId }, private_vertex_order{ aId }, private_reviewer_actor{ }, "reviewer text", private_revision_completion::complete };
	}
	inline auto created() -> private_revision_commit
	{
		auto result = create_private_thread(private_review_thread_id{ 1 }, { binding(), private_pr_subject{ } }, input());
		expect(result.has_value(), "public core create accepts exact PR binding");
		return *result;
	}
	inline auto index() -> diff_semantic_index
	{
		const auto bound     = binding();
		const auto authority = pull_request_authority{
			.Profile = host_profile_id{ 1 }, .ProfileRevision = github_profile_revision{ 1 }, .Host = bound.Scope.Host, .HostAuthority = bound.HostAuthority, .Account = bound.Scope.Account, .Viewer = github_viewer_actor_id{ 3 }, .Repository = bound.Scope.Repository, .Cache = opaque<repository_cache_scope_id>(), .PullRequest = bound.Scope.PullRequest, .Number = pull_request_number{ 5 }, .Scope = bound.LeftAnchor.Authority
		};
		const auto key      = file_comparison_key{ authority, bound.Scope.Repository, bound.Scope.PullRequest, bound.LeftAnchor, bound.RightAnchor,
												   parse_git_blob_oid("3333333333333333333333333333333333333333"), parse_git_blob_oid("4444444444444444444444444444444444444444"),
												   file_identity_of("file.cpp"), *options_digest({ }), diff_algorithm_revision{ 1 }, std::nullopt };
		const auto left     = std::string{ "first\nold\nlast\n" };
		const auto right    = std::string{ "first\nnew\nlast\n" };
		auto       computed = bpr::tests::product_file_diff({ .Key = key, .Options = { }, .LeftBytes = std::span{ reinterpret_cast<const std::uint8_t*>(left.data()), left.size() }, .RightBytes = std::span{ reinterpret_cast<const std::uint8_t*>(right.data()), right.size() } });
		expect(computed.Result.has_value(), "real core diff composition produces fixture");
		auto semantic = build_diff_semantic_index(std::make_shared<const computed_file_diff>(std::move(*computed.Result)));
		expect(semantic.has_value(), "semantic index validates actual computed result");
		return *semantic;
	}
}
#endif
