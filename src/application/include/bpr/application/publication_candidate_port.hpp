#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PUBLICATION_CANDIDATE_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_PUBLICATION_CANDIDATE_PORT_HPP

#include "bpr/core/git_objects.hpp"
#include "bpr/core/review_cycle_outgoing.hpp"

#include <cstdint>
#include <expected>
#include <stop_token>
#include <string>
#include <vector>

/// \file
/// \brief Local Git plumbing for publication candidates (RFC-BPR-0059).
/// \details A separate, finite catalog: it reads exact commits and trees of
/// the bound checkout's repository and writes only into a BPR-owned private
/// object store with a private index. It never writes the checkout, its
/// index, refs or configuration, never reads that repository's configuration
/// or hooks, runs no filter, hook, signing program or repository script, and
/// has no network or credential capability. The existing epoch cache ports
/// gain nothing from it.
namespace bpr::application {
	/// \brief The repository a publication reads, named by the bound
	/// checkout's location, in its own object format.
	struct publication_repository
	{
		/// The binding's recorded location of the checkout.
		std::string              Checkout;
		core::git_hash_algorithm Algorithm = core::git_hash_algorithm::invalid;
	};

	enum class publication_git_failure : std::uint8_t {
		/// Installed Git or the private store could not be prepared.
		unavailable,
		/// The location holds no usable Git repository, or it uses an object
		/// or reference format this catalog does not read.
		invalid_repository,
		/// The object is not in the repository; nothing is fetched.
		object_unavailable,
		/// Git's answer did not parse, or did not match what was asked.
		malformed,
		/// More entries or bytes than a candidate admits.
		bounds,
		cancelled,
	};

	struct publication_git_error
	{
		publication_git_failure Failure = publication_git_failure::unavailable;
		/// Installed Git's own short explanation, when it gave one.
		std::string Detail;
		auto        operator<=>(const publication_git_error&) const = default;
	};

	template <typename T>
	using publication_git_result = std::expected<T, publication_git_error>;

	/// \brief One exact commit as the repository holds it.
	struct publication_commit_listing
	{
		/// The commit object's content, so the caller can check it hashes to
		/// the commit asked for and names the tree listed.
		std::vector<std::uint8_t> CommitObject;
		core::git_tree_oid        Tree;
		/// Every non-tree entry, sorted by path bytes.
		std::vector<core::git_tree_entry> Entries;
	};

	class publication_candidate_port
	{
	public:
		virtual ~publication_candidate_port() = default;

		/// \brief Lists one commit's complete tree. Nothing is written to the
		/// checkout's repository.
		virtual auto list_commit(const publication_repository& aRepository, const core::git_commit_oid& aCommit, std::stop_token aStop)
			-> publication_git_result<publication_commit_listing> = 0;
	};
}

#endif
