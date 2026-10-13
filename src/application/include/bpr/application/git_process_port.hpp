#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GIT_PROCESS_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GIT_PROCESS_PORT_HPP

#include "bpr/application/agent_workspace.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <utility>
#include <variant>

namespace bpr::application {
	inline constexpr std::uint64_t maximum_git_pack_input_bytes           = 4ULL * 1024ULL * 1024ULL * 1024ULL;
	inline constexpr std::uint64_t maximum_git_blob_materialization_bytes = 16ULL * 1024ULL * 1024ULL;
	inline constexpr std::size_t   maximum_git_tree_entry_path_bytes      = 4U * 1024U;
	inline constexpr std::size_t   maximum_git_rename_pair_records        = 4U;
	inline constexpr std::size_t   maximum_git_rename_scan_records        = 256U;
	inline constexpr std::size_t   maximum_git_ref_transaction_updates    = 4U;
	enum class git_hash_algorithm : std::uint8_t {
		sha1,
		sha256,
	};
	/// \brief Describes the git object identity record.

	template <typename Kind>
	struct git_object_identity
	{
		git_hash_algorithm            Algorithm = git_hash_algorithm::sha1;
		std::array<std::uint8_t, 32U> Digest{ };

		auto operator<=>(const git_object_identity&) const = default;
	};

	struct git_commit_identity_kind;
	struct git_tree_identity_kind;
	struct git_pack_identity_kind;
	struct git_blob_identity_kind;

	using git_commit_id = git_object_identity<git_commit_identity_kind>;
	using git_tree_id   = git_object_identity<git_tree_identity_kind>;
	using git_pack_id   = git_object_identity<git_pack_identity_kind>;
	using git_blob_id   = git_object_identity<git_blob_identity_kind>;
	/// \brief Checks whether the supplied value satisfies its contract.

	template <typename Kind>
	[[nodiscard]] constexpr auto valid(
		const git_object_identity<Kind>& aObject) noexcept -> bool
	{
		if (
			aObject.Algorithm != git_hash_algorithm::sha1
			&& aObject.Algorithm != git_hash_algorithm::sha256) {
			return false;
		}
		const auto digestSize = aObject.Algorithm == git_hash_algorithm::sha1 ? 20U : 32U;
		bool       nonzero    = false;
		for (std::size_t index = 0; index < digestSize; ++index) {
			nonzero = nonzero || aObject.Digest[index] != 0U;
		}
		if (!nonzero) {
			return false;
		}
		for (std::size_t index = digestSize; index < aObject.Digest.size(); ++index) {
			if (aObject.Digest[index] != 0U) {
				return false;
			}
		}
		return true;
	}

	/// \brief Describes the git operation identity record.
	struct git_operation_identity
	{
		std::array<std::uint8_t, 32U> Bytes{ };

		auto operator<=>(const git_operation_identity&) const = default;
	};

	/// \brief Describes the git pull request scope identity record.
	struct git_pull_request_scope_identity
	{
		std::array<std::uint8_t, 32U> Bytes{ };

		auto operator<=>(const git_pull_request_scope_identity&) const = default;
	};
	/// \brief Computes the valid identity result.

	template <typename Identity>
	[[nodiscard]] constexpr auto valid_identity(const Identity& aIdentity) noexcept
		-> bool
	{
		for (const auto byte : aIdentity.Bytes) {
			if (byte != 0U) {
				return true;
			}
		}
		return false;
	}

	enum class git_private_ref_endpoint : std::uint8_t {
		base,
		head,
	};

	/// \brief Describes the git staging ref record.
	struct git_staging_ref
	{
		git_operation_identity   Operation;
		git_private_ref_endpoint Endpoint = git_private_ref_endpoint::base;

		auto operator<=>(const git_staging_ref&) const = default;
	};

	/// \brief Describes the git epoch ref record.
	struct git_epoch_ref
	{
		git_pull_request_scope_identity Scope;
		std::uint64_t                   Ordinal  = 0U;
		git_private_ref_endpoint        Endpoint = git_private_ref_endpoint::base;

		auto operator<=>(const git_epoch_ref&) const = default;
	};

	using git_private_ref = std::variant<git_staging_ref, git_epoch_ref>;

	/// \brief Describes the git ref expected absent record.
	struct git_ref_expected_absent
	{
		auto operator<=>(const git_ref_expected_absent&) const = default;
	};

	/// \brief Describes the git ref expected object record.
	struct git_ref_expected_object
	{
		git_commit_id Object;

		auto operator<=>(const git_ref_expected_object&) const = default;
	};

	using git_ref_expected_state = std::variant<git_ref_expected_absent, git_ref_expected_object>;

	/// \brief Describes the git ref write record.
	struct git_ref_write
	{
		git_private_ref        Ref;
		git_commit_id          DesiredObject;
		git_ref_expected_state ExpectedOld;

		auto operator<=>(const git_ref_write&) const = default;
	};

	/// \brief Describes the git ref remove record.
	struct git_ref_remove
	{
		git_private_ref Ref;
		git_commit_id   ExpectedOld;

		auto operator<=>(const git_ref_remove&) const = default;
	};

	using git_ref_mutation = std::variant<git_ref_write, git_ref_remove>;

	/// \brief Describes the git initialize repository request record.
	struct git_initialize_repository_request
	{
		std::filesystem::path Repository;
		git_hash_algorithm    Algorithm = git_hash_algorithm::sha1;
	};

	/// \brief Describes the git index pack request record.
	struct git_index_pack_request
	{
		std::filesystem::path  Repository;
		git_operation_identity Operation;
		std::uint64_t          SpoolBytes        = 0U;
		std::uint64_t          MaximumInputBytes = 0U;
		git_pack_id            ExpectedPack;
	};

	/// \brief Describes the git commit request record.
	struct git_commit_request
	{
		std::filesystem::path Repository;
		git_commit_id         Commit;
	};

	/// \brief Materializes one exact commit from an owned bare cache repository.
	struct git_agent_workspace_request
	{
		std::filesystem::path                   Repository;
		agent_workspace_materialization_request Materialization;

		auto operator<=>(const git_agent_workspace_request&) const = default;
	};

	/// \brief Describes one exact, bounded blob materialization request.
	struct git_blob_request
	{
		std::filesystem::path Repository;
		git_blob_id           Blob;
		std::uint64_t         MaximumBytes = 0U;
	};

	/// \brief Describes one exact repository tree-entry lookup.
	struct git_tree_entry_request
	{
		std::filesystem::path Repository;
		git_commit_id         Commit;
		std::filesystem::path Path;

		auto operator<=>(const git_tree_entry_request&) const = default;
	};

	inline constexpr std::size_t maximum_git_changed_entries         = 10000U;
	inline constexpr std::size_t maximum_git_changed_catalogue_bytes = 8U * 1024U * 1024U;

	/// \brief One exact changed tree entry; unsupported modes remain explicit.
	struct git_changed_side
	{
		std::filesystem::path Path;
		git_blob_id           Object;
		std::uint32_t         Mode = 0U;
	};
	struct git_changed_entry
	{
		std::optional<git_changed_side> Left;
		std::optional<git_changed_side> Right;
		char                            Change = 'M';
	};
	/// \brief Receives bounded domain records, with no process arguments or provider facts.
	class git_changed_entry_sink
	{
	public:
		virtual ~git_changed_entry_sink()                                          = default;
		[[nodiscard]] virtual auto accept(const git_changed_entry& aEntry) -> bool = 0;
	};
	/// \brief Enumerates only this exact pair, with hard record/output bounds.
	struct git_changed_entries_request
	{
		std::filesystem::path Repository;
		git_commit_id         Left;
		git_commit_id         Right;
	};

	/// \brief Describes one exact selected-commit rename query.
	struct git_rename_pair_request
	{
		std::filesystem::path                Repository;
		git_commit_id                        Left;
		git_commit_id                        Right;
		std::filesystem::path                CurrentPath;
		std::optional<std::filesystem::path> PreviousPath;

		auto operator<=>(const git_rename_pair_request&) const = default;
	};

	/// \brief Describes one exact rename record reported by installed Git.
	struct git_rename_pair
	{
		std::filesystem::path LeftPath;
		std::filesystem::path RightPath;

		auto operator<=>(const git_rename_pair&) const = default;
	};

	/// \brief Retains a bounded set of selected-commit rename records.
	struct git_rename_pair_observation
	{
		std::array<git_rename_pair, maximum_git_rename_pair_records> Records;
		std::size_t                                                  Count = 0U;

		auto operator<=>(const git_rename_pair_observation&) const = default;
	};

	/// \brief Describes the git ancestry request record.
	struct git_ancestry_request
	{
		std::filesystem::path Repository;
		git_commit_id         Ancestor;
		git_commit_id         Descendant;
	};

	enum class git_comparison_base_status : std::uint8_t {
		unique,
		no_common_ancestor,
		multiple_merge_bases,
		objects_unavailable,
	};

	/// \brief Queries a derived comparison base between two exact cached commits.
	struct git_comparison_base_request
	{
		std::filesystem::path Repository;
		git_commit_id         Head;
		git_commit_id         SelectedBase;
	};

	/// \brief Reports a unique derived base, never historical base provenance.
	struct git_comparison_base_observation
	{
		git_comparison_base_status   Status = git_comparison_base_status::objects_unavailable;
		std::optional<git_commit_id> Base;
	};

	/// \brief Describes the git connectivity request record.
	struct git_connectivity_request
	{
		std::filesystem::path Repository;
		git_commit_id         Base;
		git_commit_id         Head;
	};

	/// \brief Describes the git update refs request record.
	struct git_update_refs_request
	{
		std::filesystem::path             Repository;
		std::span<const git_ref_mutation> Mutations;
	};

	/// \brief Describes the git inspect ref request record.
	struct git_inspect_ref_request
	{
		std::filesystem::path Repository;
		git_private_ref       Ref;
	};

	/// \brief Describes the git installation record.
	struct git_installation
	{
		std::uint32_t VersionMajor = 0U;
		std::uint32_t VersionMinor = 0U;
		std::uint32_t VersionPatch = 0U;

		auto operator<=>(const git_installation&) const = default;
	};

	/// \brief Describes the git index pack success record.
	struct git_index_pack_success
	{
		git_pack_id Pack;

		auto operator<=>(const git_index_pack_success&) const = default;
	};

	/// \brief Describes the git commit inspection record.
	struct git_commit_inspection
	{
		git_tree_id Tree;

		auto operator<=>(const git_commit_inspection&) const = default;
	};

	/// \brief Receives one exact blob after bounded process validation.
	class git_blob_materialization_sink
	{
	public:
		git_blob_materialization_sink()                                     = default;
		git_blob_materialization_sink(const git_blob_materialization_sink&) = delete;
		auto operator=(const git_blob_materialization_sink&)
			-> git_blob_materialization_sink&                          = delete;
		git_blob_materialization_sink(git_blob_materialization_sink&&) = delete;
		auto operator=(git_blob_materialization_sink&&)
			-> git_blob_materialization_sink&    = delete;
		virtual ~git_blob_materialization_sink() = default;

		virtual auto accept(
			const git_blob_id&         aBlob,
			std::span<const std::byte> aBytes) -> void = 0;
	};

	enum class git_ancestry : std::uint8_t {
		ancestor,
		not_ancestor,
	};

	enum class git_process_failure : std::uint8_t {
		invalid_configuration,
		unsupported_git,
		invalid_request,
		invalid_repository,
		process_start,
		standard_input,
		standard_output_limit,
		standard_error_limit,
		timeout,
		cancelled,
		process_crashed,
		nonzero_exit,
		object_unavailable,
		malformed_output,
		reap_failure,
		/// \brief Git refused an object its own fsck rules call an error.
		///
		/// \details Distinct from `nonzero_exit`, which says only that some Git
		/// command failed. `Detail` carries the fsck message identifier, one of
		/// the adapter's own fixed names, so the refusal can say which object
		/// class Git would not accept (BPR-0880).
		unsupported_object,
	};

	/// \brief Carries one Git process failure with the subprocess explanation.
	/// \details `Detail` is empty unless installed Git itself reported the failure
	/// on standard error. It is bounded by the adapter's standard-error limit and
	/// has remote tokens removed before storage. A bare failure code remains a
	/// complete error and converts implicitly for compatibility.
	struct git_process_error
	{
		git_process_failure Code = git_process_failure::invalid_configuration;
		std::string         Detail;

		git_process_error() = default;

		// Implicit on purpose: a bare failure code is still a complete error.
		git_process_error(const git_process_failure aCode)
			: Code{ aCode }
		{
		}

		git_process_error(
			const git_process_failure aCode,
			decltype(Detail)          aDetail)
			: Code{ aCode }
			, Detail{ std::move(aDetail) }
		{
		}

		auto operator<=>(const git_process_error&) const = default;
	};

	/// \brief Compares a Git process error with its primary failure code.
	[[nodiscard]] inline auto operator==(
		const git_process_error&  aError,
		const git_process_failure aCode) noexcept -> bool
	{
		return aError.Code == aCode;
	}

	template <typename Value>
	using git_process_result = std::expected<Value, git_process_error>;

	/// \brief Provides the git process port contract.
	class git_process_port
	{
	public:
		virtual ~git_process_port() = default;

		[[nodiscard]] virtual auto installation() const noexcept
			-> const git_installation& = 0;
		[[nodiscard]] virtual auto pack_spool_path(
			const git_operation_identity& aOperation) const
			-> git_process_result<std::filesystem::path> = 0;
		[[nodiscard]] virtual auto initialize_bare_repository(
			const git_initialize_repository_request& aRequest,
			std::stop_token                          aStop) -> git_process_result<void> = 0;

		// Calls are synchronous so the application host can run them on its
		// effect executor. The caller retains the stop_source for the supplied
		// token and can therefore cancel a blocked child from another thread.

		/// \brief Installs a complete pack whose objects are valid and whose references resolve.
		/// \details References may resolve to objects already in the repository, as in a pack a
		/// server trimmed with `have` lines (BPR-0846). Success therefore does not prove reachable
		/// closure: the caller must verify_connectivity before publishing or journaling a revision.
		[[nodiscard]] virtual auto index_pack(
			const git_index_pack_request& aRequest,
			std::stop_token               aStop) -> git_process_result<git_index_pack_success> = 0;
		/// \brief Validates a self-contained pack's object integrity, allowing absent references.
		/// \details Sparse SHA-1 import only: does not prove reachable closure and cannot
		/// resolve thin-pack deltas from external objects. Existing byte/time limits apply.
		[[nodiscard]] virtual auto index_sparse_pack(
			const git_index_pack_request& aRequest, std::stop_token aStop)
			-> git_process_result<git_index_pack_success>
		{
			static_cast<void>(aRequest);
			static_cast<void>(aStop);
			return std::unexpected{ git_process_failure::invalid_request };
		}
		[[nodiscard]] virtual auto inspect_commit(
			const git_commit_request& aRequest,
			std::stop_token           aStop) -> git_process_result<git_commit_inspection> = 0;
		[[nodiscard]] virtual auto materialize_agent_workspace(
			const git_agent_workspace_request& aRequest,
			std::stop_token                    aStop)
			-> git_process_result<owned_agent_workspace> = 0;
		[[nodiscard]] virtual auto cleanup_agent_workspace(
			const owned_agent_workspace& aWorkspace,
			std::stop_token              aStop) -> git_process_result<void> = 0;
		[[nodiscard]] virtual auto materialize_blob(
			const git_blob_request&        aRequest,
			git_blob_materialization_sink& aSink,
			std::stop_token                aStop) -> git_process_result<void> = 0;
		[[nodiscard]] virtual auto find_blob_at_path(
			const git_tree_entry_request& aRequest,
			std::stop_token               aStop)
			-> git_process_result<std::optional<git_blob_id>> = 0;
		/// \brief Complete exact-tree catalogue, or an explicit refusal; no partial membership.
		[[nodiscard]] virtual auto changed_entries(const git_changed_entries_request& aRequest, git_changed_entry_sink& aSink, std::stop_token aStop)
			-> git_process_result<void>
		{
			static_cast<void>(aRequest);
			static_cast<void>(aSink);
			static_cast<void>(aStop);
			return std::unexpected{ git_process_failure::invalid_request };
		}
		[[nodiscard]] virtual auto find_rename_pairs(
			const git_rename_pair_request& aRequest,
			std::stop_token                aStop)
			-> git_process_result<git_rename_pair_observation> = 0;
		[[nodiscard]] virtual auto is_ancestor(
			const git_ancestry_request& aRequest,
			std::stop_token             aStop) -> git_process_result<git_ancestry> = 0;

		/// \brief Computes all merge bases with bounded output; never fetches.
		/// \details Requires complete reachable closure. Multiple best ancestors
		/// are unavailable rather than selecting one arbitrarily. Existing process
		/// time, output and cancellation limits apply; errors carry no candidate.
		[[nodiscard]] virtual auto derive_comparison_base(
			const git_comparison_base_request& aRequest,
			std::stop_token                    aStop) -> git_process_result<git_comparison_base_observation>
		{
			static_cast<void>(aRequest);
			static_cast<void>(aStop);
			return std::unexpected{ git_process_failure::invalid_request };
		}

		/// \brief Reads exact commit committer time as UTC Unix seconds, not push time.
		/// \details Missing commits return nullopt; malformed dates remain errors.
		[[nodiscard]] virtual auto read_committer_time_seconds(
			const git_commit_request& aRequest,
			std::stop_token           aStop) -> git_process_result<std::optional<std::int64_t>>
		{
			static_cast<void>(aRequest);
			static_cast<void>(aStop);
			return std::unexpected{ git_process_failure::invalid_request };
		}
		[[nodiscard]] virtual auto verify_connectivity(
			const git_connectivity_request& aRequest,
			std::stop_token                 aStop) -> git_process_result<void> = 0;
		[[nodiscard]] virtual auto update_refs(
			const git_update_refs_request& aRequest,
			std::stop_token                aStop) -> git_process_result<void> = 0;
		[[nodiscard]] virtual auto inspect_ref(
			const git_inspect_ref_request& aRequest,
			std::stop_token                aStop)
			-> git_process_result<std::optional<git_commit_id>> = 0;

		// Reads only the exact direct ref target. Recovery uses this separately
		// from object inspection so a ref whose object is missing or corrupt is
		// still reported as physical ref evidence rather than as an absent ref.
		[[nodiscard]] virtual auto inspect_ref_target(
			const git_inspect_ref_request& aRequest,
			std::stop_token                aStop)
			-> git_process_result<std::optional<git_commit_id>> = 0;
	};
}

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GIT_PROCESS_PORT_HPP
