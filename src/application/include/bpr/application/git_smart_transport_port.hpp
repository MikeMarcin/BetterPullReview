#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GIT_SMART_TRANSPORT_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GIT_SMART_TRANSPORT_PORT_HPP

#include "bpr/core/epoch_model.hpp"

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::application {
	inline constexpr std::size_t   maximum_git_base_branch_ref_bytes        = 1024U;
	inline constexpr std::size_t   maximum_git_smart_request_bytes          = 64U * 1024U;
	inline constexpr std::size_t   maximum_git_smart_control_response_bytes = 64U * 1024U;
	inline constexpr std::uint64_t maximum_git_smart_fetch_response_bytes   = 4ULL * 1024ULL * 1024ULL * 1024ULL + 64ULL * 1024ULL * 1024ULL;

	/// \brief Provides the git base branch ref contract.
	class git_base_branch_ref
	{
	public:
		constexpr git_base_branch_ref() = default;

		[[nodiscard]] constexpr auto view() const noexcept -> std::string_view
		{
			return { Characters.data(), Length };
		}

		auto operator<=>(const git_base_branch_ref&) const = default;

	private:
		friend auto parse_git_base_branch_ref(std::string_view) noexcept
			-> std::optional<git_base_branch_ref>;

		std::array<char, maximum_git_base_branch_ref_bytes> Characters{ };
		std::uint16_t                                       Length = 0U;
	};

	/// \brief Parses the supplied git base branch ref.
	[[nodiscard]] auto parse_git_base_branch_ref(std::string_view aValue) noexcept
		-> std::optional<git_base_branch_ref>;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const git_base_branch_ref& aRef) noexcept -> bool
	{
		return !aRef.view().empty();
	}

	enum class git_smart_acquisition_mode : std::uint8_t { complete_closure,
														   sparse_commit_trees };

	/// \brief Describes the git smart request context record.
	struct git_smart_request_context
	{
		core::application_session_id         ApplicationSession;
		core::pull_request_authority         Authority;
		core::accepted_pull_request_revision Revision;
		core::repository_locator             Repository;
		git_base_branch_ref                  BaseRef;
		git_smart_acquisition_mode           AcquisitionMode = git_smart_acquisition_mode::complete_closure;

		auto operator<=>(const git_smart_request_context&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const git_smart_request_context& aContext) noexcept -> bool;

	enum class git_smart_transport_failure : std::uint8_t {
		invalid_request,
		unavailable,
		authentication,
		permission,
		network,
		certificate,
		proxy,
		redirect,
		timeout,
		cancelled,
		request_limit,
		response_limit,
		malformed_response,
		consumer_rejected,
		/// SHA-1 collision detection refused a fetched object's bytes; they
		/// are not returned ([[BPR-1450]]).
		collision_detected,
	};

	/// \brief Closed diagnostic location within object acquisition; never grants authority or changes failure policy.
	enum class git_smart_transport_step : std::uint8_t {
		none,
		admission,
		temporary_store,
		process_runner,
		identity_before,
		cli_version,
		git_version,
		cli_qualification,
		git_qualification,
		clone,
		object_check,
		identity_after,
		local_store,
		local_pack,
		pack_read,
		git_capability_probe,
		cli_capability_probe,
	};

	/// \brief Describes the git smart transport error record.
	struct git_smart_transport_error
	{
		git_smart_transport_failure Failure = git_smart_transport_failure::unavailable;
		git_smart_transport_step    Step    = git_smart_transport_step::none;

		auto operator<=>(const git_smart_transport_error&) const = default;
	};

	template <typename Value>
	using git_smart_transport_result = std::expected<Value, git_smart_transport_error>;

	/// \brief Describes the git smart discovery request record.
	struct git_smart_discovery_request
	{
		git_smart_request_context Context;
	};

	/// \brief Describes the git smart discovery response record.
	struct git_smart_discovery_response
	{
		std::vector<std::byte> Body;
	};

	/// \brief Describes the git smart ls refs request record.
	struct git_smart_ls_refs_request
	{
		git_smart_request_context  Context;
		std::span<const std::byte> Body;
	};

	/// \brief Describes the git smart ls refs response record.
	struct git_smart_ls_refs_response
	{
		std::vector<std::byte> Body;
	};

	/// \brief Describes the git smart fetch request record.
	struct git_smart_fetch_request
	{
		git_smart_request_context  Context;
		std::span<const std::byte> Body;
		std::uint64_t              MaximumResponseBytes = 0U;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const git_smart_discovery_request& aRequest) noexcept -> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const git_smart_ls_refs_request& aRequest) noexcept -> bool;
	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const git_smart_fetch_request& aRequest) noexcept -> bool;

	/// \brief Requests one content-verified blob within an exact accepted authority.
	struct git_smart_exact_blob_request
	{
		git_smart_request_context Context;
		core::git_blob_oid        ExpectedBlob;
		std::size_t               MaximumBytes = 0U;
	};

	inline constexpr std::size_t maximum_git_exact_blob_bytes = 16U * 1024U * 1024U;

	/// \brief Requests several content-verified blobs in one round trip.
	///
	/// \details [[BPR-1327]]. Preparation reads the blobs of the files the
	/// reviewer is about to open together, rather than one exact read (and its
	/// account observations) per blob. Every blob is resolved by the host from
	/// its retained exact tree, exactly as for a single read.
	struct git_smart_exact_blobs_request
	{
		git_smart_request_context       Context;
		std::vector<core::git_blob_oid> ExpectedBlobs;
		/// Per blob. A larger blob is answered absent, not truncated.
		std::size_t MaximumBytes = 0U;
	};

	/// Blobs per batch, and the largest blob a batch carries. A larger blob is
	/// left to the exact single read when the reviewer opens its file.
	inline constexpr std::size_t maximum_git_exact_blob_batch       = 16U;
	inline constexpr std::size_t maximum_git_exact_blob_batch_bytes = 512U * 1024U;

	/// \brief Checks a batch request's bounds; it does not check authority.
	[[nodiscard]] inline auto valid(const git_smart_exact_blobs_request& aRequest) noexcept -> bool
	{
		return !aRequest.ExpectedBlobs.empty() && aRequest.ExpectedBlobs.size() <= maximum_git_exact_blob_batch
			&& aRequest.MaximumBytes != 0U && aRequest.MaximumBytes <= maximum_git_exact_blob_batch_bytes
			&& std::ranges::all_of(aRequest.ExpectedBlobs, [](const core::git_blob_oid& aBlob) { return aBlob.valid(); });
	}

	/// \brief One answer per requested blob, in request order. An absent
	/// answer means the transport could not return verified bytes for that
	/// blob in the batch; it may still be read on its own.
	using git_smart_exact_blobs = std::vector<std::optional<std::vector<std::byte>>>;

	/// \brief RFC-BPR-0089. Asks for the head's first-parent chain, trees and
	/// no blobs, to read each commit's own change.
	///
	/// \details `Depth` generations from `Head` cover the chain and the commit
	/// it grew from; `Commits` are every commit the host will read there, and
	/// the store must hold each one. Only a sparse acquisition asks: a complete
	/// one already holds the whole history.
	struct git_smart_chain_request
	{
		git_smart_request_context         Context;
		core::git_commit_oid              Head;
		std::uint32_t                     Depth = 0U;
		std::vector<core::git_commit_oid> Commits;
	};

	inline constexpr std::uint32_t maximum_git_smart_chain_depth = 251U;

	/// \brief Checks the request's own shape; it does not check authority.
	[[nodiscard]] inline auto valid(const git_smart_chain_request& aRequest) noexcept -> bool
	{
		return aRequest.Head.valid() && aRequest.Head == aRequest.Context.Revision.Head && aRequest.Depth >= 2U
			&& aRequest.Depth <= maximum_git_smart_chain_depth && !aRequest.Commits.empty()
			&& aRequest.Commits.size() <= 2U * maximum_git_smart_chain_depth
			&& aRequest.Context.AcquisitionMode == git_smart_acquisition_mode::sparse_commit_trees
			&& std::ranges::all_of(aRequest.Commits, [&aRequest](const core::git_commit_oid& aCommit) {
				   return aCommit.valid() && aCommit.algorithm() == aRequest.Head.algorithm();
			   });
	}

	/// \brief One side of a path a chain commit's tree diff names.
	struct git_chain_changed_side
	{
		std::string        Path;
		core::git_blob_oid Object;
		std::uint32_t      Mode = 0U;

		auto operator<=>(const git_chain_changed_side&) const = default;
	};

	/// \brief One path a chain commit's tree diff names, as Git's raw letter.
	struct git_chain_changed_entry
	{
		std::optional<git_chain_changed_side> Left;
		std::optional<git_chain_changed_side> Right;
		char                                  Change = 'M';

		auto operator<=>(const git_chain_changed_entry&) const = default;
	};

	/// \brief RFC-BPR-0089. A transient, quarantined copy of the chain's
	/// commits and trees, owned by the host for one review and removed with it.
	///
	/// \details Never fetches: an object it does not hold answers absent.
	/// Its tree diff is the cache's own bounded one, with exact renames.
	class git_chain_tree_store
	{
	public:
		virtual ~git_chain_tree_store() = default;

		git_chain_tree_store()                                               = default;
		git_chain_tree_store(const git_chain_tree_store&)                    = delete;
		auto operator=(const git_chain_tree_store&) -> git_chain_tree_store& = delete;
		git_chain_tree_store(git_chain_tree_store&&)                         = delete;
		auto operator=(git_chain_tree_store&&) -> git_chain_tree_store&      = delete;

		/// \brief Whether the store holds this commit and its trees.
		[[nodiscard]] virtual auto holds(const core::git_commit_oid& aCommit) const noexcept -> bool = 0;
		/// \brief The tree diff from `aLeft` to `aRight`, both held.
		[[nodiscard]] virtual auto changed_entries(const core::git_commit_oid& aLeft, const core::git_commit_oid& aRight, std::stop_token aStop)
			-> git_smart_transport_result<std::vector<git_chain_changed_entry>> = 0;
		/// \brief The blob at one path of a held commit, or none.
		[[nodiscard]] virtual auto find_blob_at_path(const core::git_commit_oid& aCommit, const std::string& aPath, std::stop_token aStop)
			-> git_smart_transport_result<std::optional<core::git_blob_oid>> = 0;
	};

	/// \brief Provides the git smart wire consumer contract.
	class git_smart_wire_consumer
	{
	public:
		virtual ~git_smart_wire_consumer() = default;

		[[nodiscard]] virtual auto consume(
			std::span<const std::byte> aBytes) noexcept -> bool = 0;
	};

	/// \brief Provides the git smart transport port contract.
	class git_smart_transport_port
	{
	public:
		virtual ~git_smart_transport_port() = default;

		git_smart_transport_port()                                = default;
		git_smart_transport_port(const git_smart_transport_port&) = delete;
		auto operator=(const git_smart_transport_port&)
			-> git_smart_transport_port&                     = delete;
		git_smart_transport_port(git_smart_transport_port&&) = delete;
		auto operator=(git_smart_transport_port&&)
			-> git_smart_transport_port& = delete;

		virtual auto               revoke() noexcept -> void        = 0;
		[[nodiscard]] virtual auto revoked() const noexcept -> bool = 0;

		/// \brief Returns exact verified blob bytes; transports without this capability refuse.
		/// \details The host supplies the OID resolved from its retained exact tree.
		[[nodiscard]] virtual auto read_exact_blob(
			const git_smart_exact_blob_request&, std::stop_token)
			-> git_smart_transport_result<std::vector<std::byte>>
		{
			return std::unexpected{ git_smart_transport_error{ git_smart_transport_failure::unavailable } };
		}

		/// \brief Returns several verified blobs in one round trip; transports
		/// without this capability refuse, and the caller reads none ahead.
		[[nodiscard]] virtual auto read_exact_blobs(
			const git_smart_exact_blobs_request&, std::stop_token)
			-> git_smart_transport_result<git_smart_exact_blobs>
		{
			return std::unexpected{ git_smart_transport_error{ git_smart_transport_failure::unavailable } };
		}

		/// \brief RFC-BPR-0089. Acquires the head's first-parent chain trees
		/// into a transient store; transports without this capability refuse.
		[[nodiscard]] virtual auto acquire_chain_trees(const git_smart_chain_request&, std::stop_token)
			-> git_smart_transport_result<std::shared_ptr<git_chain_tree_store>>
		{
			return std::unexpected{ git_smart_transport_error{ git_smart_transport_failure::unavailable } };
		}

		[[nodiscard]] virtual auto discover(
			const git_smart_discovery_request& aRequest,
			std::stop_token                    aStop)
			-> git_smart_transport_result<git_smart_discovery_response> = 0;
		[[nodiscard]] virtual auto ls_refs(
			const git_smart_ls_refs_request& aRequest,
			std::stop_token                  aStop)
			-> git_smart_transport_result<git_smart_ls_refs_response> = 0;
		[[nodiscard]] virtual auto fetch(
			const git_smart_fetch_request& aRequest,
			git_smart_wire_consumer&       aConsumer,
			std::stop_token                aStop) -> git_smart_transport_result<void> = 0;
	};

	// The authority retains only weak ownership so it can revoke every issued
	// transport without forwarding protocol rounds or extending a job lifetime.
	using git_smart_transport = std::shared_ptr<git_smart_transport_port>;

	/// \brief Provides the git smart transport factory port contract.
	class git_smart_transport_factory_port
	{
	public:
		virtual ~git_smart_transport_factory_port() = default;

		git_smart_transport_factory_port() = default;
		git_smart_transport_factory_port(
			const git_smart_transport_factory_port&) = delete;
		auto operator=(const git_smart_transport_factory_port&)
			-> git_smart_transport_factory_port& = delete;
		git_smart_transport_factory_port(
			git_smart_transport_factory_port&&) = delete;
		auto operator=(git_smart_transport_factory_port&&)
			-> git_smart_transport_factory_port& = delete;

		/// \brief Reports support for explicitly sparse commit/tree acquisition.
		[[nodiscard]] virtual auto supports_sparse_commit_trees() const noexcept -> bool { return false; }

		[[nodiscard]] virtual auto acquire(
			const git_smart_request_context& aContext)
			-> git_smart_transport_result<git_smart_transport> = 0;
	};
}

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GIT_SMART_TRANSPORT_PORT_HPP
