#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_VIEWER_NAMED_BLOB_READ_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_VIEWER_NAMED_BLOB_READ_HPP

#include "bpr/application/git_smart_transport_port.hpp"
#include "bpr/core/github_model.hpp"

#include <stop_token>
#include <string>

namespace bpr::adapters::github::detail {
	/// \brief The account a GitHub answer says performed its own request.
	struct github_answering_account
	{
		core::github_account_id Account;
		std::string             Login;

		auto operator<=>(const github_answering_account&) const = default;
	};

	/// \brief Text blobs together with the account that read them.
	struct viewer_named_blobs
	{
		application::git_smart_exact_blobs Blobs;
		github_answering_account           Viewer;
	};

	/// \brief A blob read whose answer names the account that made it.
	///
	/// \details [[BPR-1370]]. The configured `gh` has no per-invocation account
	/// selector, so the CLI lease observed the account with `gh api user`
	/// before and after every read. A GraphQL read that also selects
	/// `viewer { login databaseId }` is answered by the same request and the
	/// same token that read the blobs, so its answer names the account that
	/// performed this read, not one that held the slot a call earlier or later.
	/// The caller compares that account with the lease's bound account and
	/// refuses on a mismatch, as it does an observation. Only text blobs travel
	/// this way: binary, truncated, oversized, missing or mismatched objects are
	/// answered absent, and a single read of one of them takes the observed
	/// REST read. An account that differs from the request's own authority
	/// revokes the reader and every blob is answered absent; the viewer is
	/// still returned so the caller refuses its lease.
	class viewer_named_blob_reader
	{
	public:
		viewer_named_blob_reader()                                                   = default;
		viewer_named_blob_reader(const viewer_named_blob_reader&)                    = delete;
		auto operator=(const viewer_named_blob_reader&) -> viewer_named_blob_reader& = delete;
		viewer_named_blob_reader(viewer_named_blob_reader&&)                         = delete;
		auto operator=(viewer_named_blob_reader&&) -> viewer_named_blob_reader&      = delete;
		virtual ~viewer_named_blob_reader()                                          = default;

		[[nodiscard]] virtual auto read_exact_blobs_naming_viewer(
			const application::git_smart_exact_blobs_request& aRequest, std::stop_token aStop)
			-> application::git_smart_transport_result<viewer_named_blobs> = 0;
	};
}

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_VIEWER_NAMED_BLOB_READ_HPP
