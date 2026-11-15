#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_LOCAL_DRAFT_GIT_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_LOCAL_DRAFT_GIT_PORT_HPP

#include "bpr/application/publication_candidate_port.hpp"

#include <cstdint>
#include <stop_token>
#include <string>
#include <vector>

/// \file
/// \brief Read-only Git object reads for a local PR draft (RFC-BPR-0108).
/// \details The same isolated catalog as publication: commands run in a
/// BPR-owned store with the checkout's objects, and the objects of each local
/// clone the remote chain followed, as read-only alternates. Nothing reads
/// the checkout's configuration, hooks, refs or index, runs a filter, writes
/// any clone or fetches; an object none of them holds is said to be missing.
/// Refs and configuration are read as data by the application, never here.
namespace bpr::application {
	struct local_draft_repository
	{
		/// The worktree's location.
		std::string Checkout;
		/// Clones whose objects may also be read: the chain's hops, in order.
		std::vector<std::string> Borrowed;
		core::git_hash_algorithm Algorithm = core::git_hash_algorithm::invalid;
	};

	/// \brief Most merge bases a comparison lists; more is refused as bounds.
	inline constexpr std::size_t maximum_local_draft_merge_bases = 16U;

	class local_draft_git_port
	{
	public:
		virtual ~local_draft_git_port() = default;

		/// \brief Every merge base of two commits (`merge-base --all`); empty
		/// when they share no history. A commit the objects lack is missing.
		virtual auto merge_bases(const local_draft_repository& aRepository, const core::git_commit_oid& aLeft, const core::git_commit_oid& aRight, std::stop_token aStop)
			-> publication_git_result<std::vector<core::git_commit_oid>> = 0;
		/// \brief One commit's complete tree, as publication lists it.
		virtual auto list_tree_of(const local_draft_repository& aRepository, const core::git_commit_oid& aCommit, std::stop_token aStop)
			-> publication_git_result<publication_commit_listing> = 0;
		/// \brief One blob's exact bytes, refused as bounds past `aMaximumBytes`.
		virtual auto read_blob(const local_draft_repository& aRepository, const core::git_entry_oid& aBlob, std::uint64_t aMaximumBytes, std::stop_token aStop)
			-> publication_git_result<std::vector<std::uint8_t>> = 0;
	};
}

#endif
