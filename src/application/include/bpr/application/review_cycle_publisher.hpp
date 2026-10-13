#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_PUBLISHER_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_PUBLISHER_HPP

#include "bpr/application/github_publication_port.hpp"
#include "bpr/application/review_cycle_publication.hpp"
#include "bpr/application/review_cycle_store.hpp"

#include <cstdint>
#include <optional>
#include <stop_token>
#include <string_view>

/// \file
/// \brief Carrying out an authorized publication through the GitHub
/// publication catalog (RFC-BPR-0059, BPR-1175 and BPR-1176).
/// \details Capability is read first: the repository admits pushes, the
/// object format is one the provider takes, no changed path needs a scope
/// this build cannot verify, P exists there, the target is where the plan
/// saw it and the new branch does not exist. Then each remaining step is
/// journaled before it is attempted and completed with what was observed;
/// the first step that is not confirmed stops the run. Recovery only reads.
namespace bpr::application {
	/// \brief Why nothing was attempted.
	enum class cycle_publish_limit : std::uint8_t {
		/// The account's host or server contract is not admitted for native publication.
		unsupported_host,
		/// The provider takes SHA-1 repositories only.
		object_format,
		no_push_permission,
		archived,
		/// Workflow files need a scope this build does not verify.
		protected_path,
		/// P is not in the destination repository: push it first, or prepare externally.
		parent_not_on_remote,
		/// The base branch is missing, or not where the plan saw it.
		target_moved,
		/// A branch of that name exists and this plan never created it.
		branch_exists,
		/// A blob over the catalog's upload bound.
		blob_too_large,
		/// The retained bytes are missing or not the planned object.
		material,
		/// A pull request for the new branch already exists and this plan
		/// never asked for one: associating it is the reviewer's choice.
		pull_request_exists,
		/// SHA-1 collision detection refused the retained bytes of a planned
		/// blob; nothing is uploaded ([[BPR-1450]]).
		collision_detected,
	};
	[[nodiscard]] auto name(cycle_publish_limit aLimit) noexcept -> std::string_view;

	enum class cycle_publish_status : std::uint8_t {
		/// Every step through the new branch is confirmed.
		branch_created,
		/// The pull request exists and is linked.
		linked,
		/// Nothing was attempted; see `Limit`.
		limited,
		/// A step was not confirmed; the journal says how (uncertain,
		/// refused, not sent or foreign).
		stopped,
		/// No current authorization, or a closed publication.
		unauthorized,
		/// Recovery read what it could; see the journal.
		recovered,
		/// A provider read failed; see `Provider`.
		provider,
		/// The store refused; see `Store`.
		store,
		cancelled,
	};

	struct cycle_publish_outcome
	{
		cycle_publish_status                    Status = cycle_publish_status::cancelled;
		std::optional<cycle_publish_limit>      Limit;
		std::optional<github_publication_error> Provider;
		std::optional<cycle_store_error>        Store;
		std::optional<stored_cycle_publication> Publication;
	};

	/// \brief How one server pull request relates to a plan: its head is the
	/// plan's new branch in the destination repository and its base the plan's
	/// base; `Ours` when it is also the plan's commit under the plan's login.
	struct cycle_pull_request_match
	{
		bool Candidate = false;
		bool Ours      = false;
	};
	[[nodiscard]] auto match_pull_request(const core::publication_plan& aPlan, const github_pull_request_observation& aPull) -> cycle_pull_request_match;

	/// \brief Runs on the effect lane; the journal host, store and provider
	/// must outlive it. The provider is bound to one account, which is the
	/// account every journaled write names.
	class review_cycle_publisher
	{
	public:
		review_cycle_publisher(review_cycle_publication_host& aJournal, review_cycle_store& aStore, github_publication_port& aProvider, github_server_contract aServer,
							   core::github_account_id aAccount) noexcept;

		/// \brief Attempts every remaining step under the current authorization.
		auto publish(const core::local_cycle_subject& aSubject, core::cycle_publication_id aPublication, std::stop_token aStop) -> cycle_publish_outcome;
		/// \brief Reads the provider for every uncertain effect and records
		/// what it found. Nothing remote is written.
		auto recover(const core::local_cycle_subject& aSubject, core::cycle_publication_id aPublication, std::stop_token aStop) -> cycle_publish_outcome;

	private:
		auto pull_request(const core::local_cycle_subject& aSubject, core::cycle_publication_id aPublication, stored_cycle_publication aCurrent, const github_publication_target& aTarget,
						  std::stop_token aStop) -> cycle_publish_outcome;

		review_cycle_publication_host& Journal;
		review_cycle_store&            Store;
		github_publication_port&       Provider;
		github_server_contract         Server = github_server_contract::github_com_2022_11_28;
		core::github_account_id        Account;
	};
}

#endif
