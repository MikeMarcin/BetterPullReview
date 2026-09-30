#ifndef BPR_TESTS_WORKSPACE_CONTRACT_SIMULATED_GITHUB_HPP
#define BPR_TESTS_WORKSPACE_CONTRACT_SIMULATED_GITHUB_HPP

/// \file
/// \brief An in-memory GitHub repository behind the publication catalog's
/// port, for the publication courses (RFC-BPR-0059). It computes every
/// identity the way Git does, never overwrites a reference, and can lose an
/// answer after taking effect, refuse, rewrite an identity or let another
/// actor move a branch. It is a fake transport, not GitHub: live behaviour
/// is qualified separately on a designated target.

#include "../support/test_expectation.hpp"
#include "bpr/application/github_publication_port.hpp"
#include "bpr/core/git_objects.hpp"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace bpr::tests::workspace_contract {
	enum class simulated_fault : std::uint8_t {
		none,
		/// Take effect, then lose the answer.
		lose_answer,
		/// Fail before anything leaves.
		not_sent,
		/// A definitive 403.
		forbid,
		/// Answer with another identity than the one computed.
		rewrite,
		/// Another actor creates the branch just before this request.
		race_branch,
		/// Another actor opens a pull request for the head just before this request.
		race_pull,
	};

	class simulated_github final : public application::github_publication_port
	{
	public:
		struct stored_commit
		{
			core::git_tree_oid                Tree;
			std::vector<core::git_commit_oid> Parents;
		};

		std::string                                               Owner      = "octo-org";
		std::string                                               Repository = "review-repository";
		bool                                                      CanPush    = true;
		bool                                                      Archived   = false;
		bool                                                      Hosted     = true;
		std::map<std::string, std::vector<std::uint8_t>>          Blobs;
		std::map<std::string, std::vector<core::git_tree_entry>>  Trees;
		std::map<std::string, stored_commit>                      Commits;
		std::map<std::string, core::git_commit_oid>               Branches;
		std::vector<application::github_pull_request_observation> Pulls;
		std::map<std::string, simulated_fault>                    Faults;
		std::map<std::string, int>                                Writes;
		std::uint64_t                                             NextPull = 17U;
		/// Where another actor points a branch it creates first.
		core::git_commit_oid Foreign;
		/// Runs after a pull request is stored, before the answer: another
		/// actor pushing to the head or moving the base.
		std::function<void(application::github_pull_request_observation&)> AfterPull;

		/// Seeds P as the head of `aBase`.
		void seed(const core::git_commit_oid& aParent, const core::git_tree_oid& aTree, std::vector<core::git_tree_entry> aEntries, const std::string& aBase)
		{
			Trees[hex(aTree)]     = std::move(aEntries);
			Commits[hex(aParent)] = { .Tree = aTree };
			Branches[aBase]       = aParent;
			Foreign               = aParent;
		}

		[[nodiscard]] auto writes() const -> int
		{
			auto total = 0;
			for (const auto& [name, count] : Writes) {
				total += count;
			}
			return total;
		}

		auto observe_repository(const application::github_publication_target& aTarget, std::stop_token) -> application::github_publication_result<application::github_publication_capability> override
		{
			if (!Hosted) {
				return failure(application::github_publication_failure::unsupported_host);
			}
			expect(aTarget.Owner == Owner && aTarget.Repository == Repository, "the plan's repository");
			return application::github_publication_capability{ .Owner = Owner, .Repository = Repository, .CanPush = CanPush, .Archived = Archived, .DefaultBranch = "main" };
		}
		auto observe_branch(const application::github_publication_target&, const std::string& aBranch, std::stop_token) -> application::github_publication_result<std::optional<core::git_commit_oid>> override
		{
			const auto found = Branches.find(aBranch);
			return found == Branches.end() ? std::optional<core::git_commit_oid>{ } : std::optional{ found->second };
		}
		auto observe_commit(const application::github_publication_target&, const core::git_commit_oid& aCommit, std::stop_token)
			-> application::github_publication_result<std::optional<application::github_publication_commit>> override
		{
			const auto found = Commits.find(hex(aCommit));
			if (found == Commits.end()) {
				return std::optional<application::github_publication_commit>{ };
			}
			return std::optional{ application::github_publication_commit{ .Commit = aCommit, .Tree = found->second.Tree, .Parents = found->second.Parents } };
		}
		auto observe_tree(const application::github_publication_target&, const core::git_tree_oid& aTree, std::stop_token) -> application::github_publication_result<bool> override
		{
			return Trees.contains(hex(aTree));
		}
		auto observe_blob(const application::github_publication_target&, const core::git_entry_oid& aBlob, std::stop_token) -> application::github_publication_result<bool> override
		{
			return Blobs.contains(hex(aBlob));
		}

		auto create_blob(const application::github_publication_target&, std::span<const std::uint8_t> aBytes, const core::git_entry_oid& aExpected, std::stop_token)
			-> application::github_publication_result<void> override
		{
			return write("blob", [&]() -> std::string {
				const auto id = core::git_blob_identity(aBytes, core::git_hash_algorithm::sha1);
				Blobs[std::string{ core::canonical_hex(id).view() }] = { aBytes.begin(), aBytes.end() };
				return std::string{ core::canonical_hex(id).view() }; }, hex(aExpected));
		}
		auto create_tree(const application::github_publication_target&, const core::git_tree_oid& aBase, const std::vector<core::git_tree_entry>& aChanges, const std::vector<std::string>& aDeletions,
						 const core::git_tree_oid& aExpected, std::stop_token) -> application::github_publication_result<void> override
		{
			return write("tree", [&]() -> std::string {
				const auto base = Trees.find(hex(aBase));
				expect(base != Trees.end(), "the base tree is on the server");
				auto entries = std::map<std::string, core::git_tree_entry>{ };
				for (const auto& entry : base->second) {
					entries[entry.Path] = entry;
				}
				for (const auto& change : aChanges) {
					expect(Blobs.contains(hex(change.Object)) || std::ranges::any_of(base->second, [&](const core::git_tree_entry& aEntry) { return aEntry.Object == change.Object; }),
						   "every blob a tree names is stored first");
					entries[change.Path] = change;
				}
				for (const auto& deletion : aDeletions) {
					entries.erase(deletion);
				}
				auto listing = std::vector<core::git_tree_entry>{ };
				for (auto& [path, entry] : entries) {
					listing.push_back(std::move(entry));
				}
				const auto built = core::build_git_trees(listing, core::git_hash_algorithm::sha1);
				expect(built.has_value(), "a valid tree");
				Trees[hex(built->Root)] = std::move(listing);
				return hex(built->Root); }, hex(aExpected));
		}
		auto create_commit(const application::github_publication_target&, const core::git_commit_content& aCommit, const core::git_commit_oid& aExpected, std::stop_token)
			-> application::github_publication_result<void> override
		{
			return write("commit", [&]() -> std::string {
				expect(Trees.contains(hex(aCommit.Tree)) && Commits.contains(hex(aCommit.Parents.front())), "the tree and parent exist first");
				const auto id = core::git_commit_identity(aCommit);
				expect(id.has_value(), "a valid commit");
				Commits[hex(*id)] = { .Tree = aCommit.Tree, .Parents = aCommit.Parents };
				return hex(*id); }, hex(aExpected));
		}
		auto create_branch(const application::github_publication_target&, const std::string& aBranch, const core::git_commit_oid& aCommit, std::stop_token)
			-> application::github_publication_result<application::github_branch_creation> override
		{
			const auto fault = take("branch");
			if (fault == simulated_fault::race_branch) {
				Branches[aBranch] = Foreign;
			}
			if (fault == simulated_fault::not_sent) {
				return failure(application::github_publication_failure::unavailable);
			}
			if (fault == simulated_fault::forbid) {
				return failure(application::github_publication_failure::refused, application::github_publication_transmission::indeterminate, 403U);
			}
			++Writes["branch"];
			if (Branches.contains(aBranch)) {
				return application::github_branch_creation::exists;
			}
			expect(Commits.contains(hex(aCommit)), "a branch names a stored commit");
			Branches[aBranch] = aCommit;
			if (fault == simulated_fault::lose_answer) {
				return failure(application::github_publication_failure::unavailable, application::github_publication_transmission::indeterminate, 502U);
			}
			return application::github_branch_creation::created;
		}
		auto create_pull_request(const application::github_publication_target&, const application::github_pull_request_draft& aDraft, std::stop_token)
			-> application::github_publication_result<application::github_pull_request_observation> override
		{
			const auto fault = take("pull");
			if (fault == simulated_fault::race_pull) {
				Pulls.push_back(pull(aDraft, Branches.at(aDraft.Head), "someone-else"));
			}
			if (fault == simulated_fault::not_sent) {
				return failure(application::github_publication_failure::unavailable);
			}
			if (fault == simulated_fault::forbid) {
				return failure(application::github_publication_failure::refused, application::github_publication_transmission::indeterminate, 403U);
			}
			++Writes["pull"];
			if (std::ranges::any_of(Pulls, [&](const auto& aPull) { return aPull.HeadRef == aDraft.Head && aPull.BaseRef == aDraft.Base && aPull.State == "open"; })) {
				return failure(application::github_publication_failure::refused, application::github_publication_transmission::indeterminate, 422U);
			}
			Pulls.push_back(pull(aDraft, Branches.at(aDraft.Head), "fixture-reviewer"));
			auto answer = Pulls.back();
			if (AfterPull) {
				AfterPull(Pulls.back());
			}
			if (fault == simulated_fault::lose_answer) {
				return failure(application::github_publication_failure::unavailable, application::github_publication_transmission::indeterminate, 502U);
			}
			return answer;
		}
		auto find_pull_requests(const application::github_publication_target&, const std::string& aHead, const std::string& aBase, std::stop_token)
			-> application::github_publication_result<std::vector<application::github_pull_request_observation>> override
		{
			auto found = std::vector<application::github_pull_request_observation>{ };
			std::ranges::copy_if(Pulls, std::back_inserter(found), [&](const auto& aPull) { return aPull.HeadRef == aHead && aPull.BaseRef == aBase; });
			return found;
		}
		auto observe_pull_request(const application::github_publication_target&, const std::uint64_t aNumber, std::stop_token)
			-> application::github_publication_result<std::optional<application::github_pull_request_observation>> override
		{
			const auto found = std::ranges::find(Pulls, aNumber, &application::github_pull_request_observation::Number);
			return found == Pulls.end() ? std::optional<application::github_pull_request_observation>{ } : std::optional{ *found };
		}

		[[nodiscard]] static auto hex(const auto& aOid) -> std::string
		{
			return std::string{ core::canonical_hex(aOid).view() };
		}

	private:
		[[nodiscard]] static auto failure(const application::github_publication_failure      aFailure,
										  const application::github_publication_transmission aTransmission = application::github_publication_transmission::definitely_not_sent, const std::uint16_t aStatus = 0U)
			-> std::unexpected<application::github_publication_error>
		{
			return std::unexpected{ application::github_publication_error{ .Failure = aFailure, .Transmission = aTransmission, .HttpStatus = aStatus } };
		}

		auto take(const std::string& aOperation) -> simulated_fault
		{
			const auto found = Faults.find(aOperation);
			if (found == Faults.end()) {
				return simulated_fault::none;
			}
			const auto fault = found->second;
			Faults.erase(found);
			return fault;
		}

		auto pull(const application::github_pull_request_draft& aDraft, const core::git_commit_oid& aHead, std::string aAuthor) -> application::github_pull_request_observation
		{
			const auto number = NextPull++;
			return { .Number         = number,
					 .NodeId         = "PR_" + std::to_string(number),
					 .Url            = "https://github.com/" + Owner + "/" + Repository + "/pull/" + std::to_string(number),
					 .State          = "open",
					 .Draft          = aDraft.Draft,
					 .Title          = aDraft.Title,
					 .HeadRef        = aDraft.Head,
					 .HeadOwner      = Owner,
					 .HeadRepository = Repository,
					 .HeadCommit     = aHead,
					 .BaseRef        = aDraft.Base,
					 .BaseCommit     = Branches.at(aDraft.Base),
					 .Author         = std::move(aAuthor) };
		}

		/// One object write: faults first, then the server's own identity,
		/// which must be the one the client computed.
		auto write(const std::string& aOperation, const std::function<std::string()>& aEffect, const std::string& aExpected) -> application::github_publication_result<void>
		{
			const auto fault = take(aOperation);
			if (fault == simulated_fault::not_sent) {
				return failure(application::github_publication_failure::unavailable);
			}
			if (fault == simulated_fault::forbid) {
				return failure(application::github_publication_failure::refused, application::github_publication_transmission::indeterminate, 403U);
			}
			++Writes[aOperation];
			const auto stored = aEffect();
			if (fault == simulated_fault::lose_answer) {
				return failure(application::github_publication_failure::unavailable, application::github_publication_transmission::indeterminate, 502U);
			}
			if (fault == simulated_fault::rewrite || stored != aExpected) {
				return failure(application::github_publication_failure::mismatch, application::github_publication_transmission::indeterminate, 201U);
			}
			return { };
		}
	};
}

#endif
