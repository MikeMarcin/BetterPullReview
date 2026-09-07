#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GIT_PROTOCOL_V2_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GIT_PROTOCOL_V2_HPP

#include "bpr/application/git_pack_stream.hpp"
#include "bpr/application/git_smart_transport_port.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <variant>
#include <vector>

namespace bpr::application {
	inline constexpr std::size_t   maximum_git_protocol_v2_packet_bytes    = 65'520U;
	inline constexpr std::size_t   maximum_git_protocol_v2_control_packets = 256U;
	inline constexpr std::size_t   maximum_git_protocol_v2_haves           = 64U;
	inline constexpr std::uint64_t maximum_git_protocol_v2_pack_bytes      = 4ULL * 1024ULL * 1024ULL * 1024ULL;
	inline constexpr std::uint64_t maximum_git_protocol_v2_progress_bytes  = 1024ULL * 1024ULL;
	inline constexpr std::uint64_t maximum_git_protocol_v2_fetch_packets   = 131'072U;

	enum class git_protocol_v2_failure : std::uint8_t {
		invalid_request,
		request_limit,
		response_limit,
		malformed_packet,
		unsupported_version,
		missing_capability,
		object_format_mismatch,
		unexpected_ref,
		duplicate_ref,
		oid_mismatch,
		unexpected_section,
		sideband_error,
		progress_limit,
		pack_limit,
		incomplete_response,
		pack_sink,
		allocation,
	};

	/// \brief Describes the git protocol v2 error record.
	struct git_protocol_v2_error
	{
		git_protocol_v2_failure                Failure = git_protocol_v2_failure::invalid_request;
		std::optional<git_pack_stream_failure> PackFailure;

		auto operator<=>(const git_protocol_v2_error&) const = default;
	};

	template <typename Value>
	using git_protocol_v2_result = std::expected<Value, git_protocol_v2_error>;

	/// \brief Describes the git protocol v2 refs record.
	struct git_protocol_v2_refs
	{
		/// [[BPR-0667]]. Where the base branch points now, which is ordinarily
		/// ahead of the base the pull request recorded.
		core::git_commit_oid Base;
		/// The pull request's own head, which never moves; an advertisement of
		/// any other oid there is refused as `oid_mismatch`.
		core::git_commit_oid Head;

		auto operator<=>(const git_protocol_v2_refs&) const = default;
	};

	/// \brief Describes the git protocol v2 have record.
	struct git_protocol_v2_have
	{
		core::repository_cache_scope_id Cache;
		core::git_commit_oid            Commit;
		bool                            CatalogVisible = false;
		bool                            ObjectVerified = false;

		auto operator<=>(const git_protocol_v2_have&) const = default;
	};

	/// \brief Describes the git protocol v2 fetch limits record.
	struct git_protocol_v2_fetch_limits
	{
		std::uint64_t MaximumWireBytes     = 0U;
		std::uint64_t MaximumPackBytes     = 0U;
		std::uint64_t MaximumProgressBytes = 0U;
		std::uint64_t MaximumPackets       = 0U;

		auto operator<=>(const git_protocol_v2_fetch_limits&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(
		const git_protocol_v2_fetch_limits& aLimits) noexcept -> bool;

	/// \brief Describes the git protocol v2 fetch result record.
	struct git_protocol_v2_fetch_result
	{
		completed_git_pack_receipt Pack;
		std::uint64_t              WireBytes     = 0U;
		std::uint64_t              PackBytes     = 0U;
		std::uint64_t              ProgressBytes = 0U;
		std::uint64_t              Packets       = 0U;

		auto operator<=>(const git_protocol_v2_fetch_result&) const = default;
	};

	namespace detail {
		/// \brief Describes the git protocol v2 capabilities record.
		struct git_protocol_v2_capabilities
		{
			core::git_hash_algorithm ObjectFormat = core::git_hash_algorithm::invalid;
			bool                     LsRefs       = false;
			bool                     Fetch        = false;

			auto operator<=>(const git_protocol_v2_capabilities&) const = default;
		};

		[[nodiscard]] auto valid(
			const git_protocol_v2_capabilities& aCapabilities) noexcept -> bool;

		[[nodiscard]] auto parse_git_protocol_v2_advertisement(
			std::span<const std::byte> aResponse,
			core::git_hash_algorithm   aExpectedFormat)
			-> git_protocol_v2_result<git_protocol_v2_capabilities>;

		[[nodiscard]] auto make_git_protocol_v2_ls_refs_request(
			const git_smart_request_context&    aContext,
			const git_protocol_v2_capabilities& aCapabilities)
			-> git_protocol_v2_result<std::vector<std::byte>>;

		[[nodiscard]] auto parse_git_protocol_v2_ls_refs(
			std::span<const std::byte>       aResponse,
			const git_smart_request_context& aContext)
			-> git_protocol_v2_result<git_protocol_v2_refs>;

		[[nodiscard]] auto make_git_protocol_v2_fetch_request(
			const git_smart_request_context&      aContext,
			const git_protocol_v2_capabilities&   aCapabilities,
			std::span<const git_protocol_v2_have> aHaves)
			-> git_protocol_v2_result<std::vector<std::byte>>;

		/// \brief Provides the git protocol v2 fetch parser contract.
		/// \brief Provides the git protocol v2 fetch parser contract.
		class git_protocol_v2_fetch_parser final : public git_smart_wire_consumer
		{
		public:
			git_protocol_v2_fetch_parser(
				git_pack_stream_sink&        aSink,
				git_protocol_v2_fetch_limits aLimits,
				core::git_hash_algorithm     aExpectedFormat,
				std::stop_token              aStop) noexcept;
			~git_protocol_v2_fetch_parser() override;

			git_protocol_v2_fetch_parser(
				const git_protocol_v2_fetch_parser&) = delete;
			auto operator=(const git_protocol_v2_fetch_parser&)
				-> git_protocol_v2_fetch_parser&                         = delete;
			git_protocol_v2_fetch_parser(git_protocol_v2_fetch_parser&&) = delete;
			auto operator=(git_protocol_v2_fetch_parser&&)
				-> git_protocol_v2_fetch_parser& = delete;

			[[nodiscard]] auto consume(
				std::span<const std::byte> aBytes) noexcept -> bool override;
			[[nodiscard]] auto finish()
				-> git_protocol_v2_result<git_protocol_v2_fetch_result>;
			auto abort() noexcept -> void;

			[[nodiscard]] auto error() const noexcept
				-> std::optional<git_protocol_v2_error>;
			[[nodiscard]] auto wire_bytes() const noexcept -> std::uint64_t;
			[[nodiscard]] auto pack_bytes() const noexcept -> std::uint64_t;
			[[nodiscard]] auto progress_bytes() const noexcept -> std::uint64_t;
			[[nodiscard]] auto packets() const noexcept -> std::uint64_t;

		private:
			enum class response_state : std::uint8_t {
				packfile_header,
				sideband,
				terminal,
				failed,
				finished,
			};

			auto               fail(git_protocol_v2_error aError) noexcept -> bool;
			[[nodiscard]] auto begin_packet() noexcept -> bool;
			[[nodiscard]] auto consume_payload(
				std::span<const std::byte> aBytes) noexcept -> bool;
			auto reset_packet() noexcept -> void;
			auto abort_sink() noexcept -> void;

			git_pack_stream_sink*                Sink = nullptr;
			git_protocol_v2_fetch_limits         Limits;
			core::git_hash_algorithm             ExpectedFormat = core::git_hash_algorithm::invalid;
			std::stop_token                      Stop;
			response_state                       State = response_state::packfile_header;
			std::array<char, 4U>                 Header{ };
			std::size_t                          HeaderBytes     = 0U;
			std::uint32_t                        PayloadBytes    = 0U;
			std::uint32_t                        PayloadConsumed = 0U;
			std::array<char, 9U>                 SectionHeader{ };
			std::size_t                          SectionHeaderBytes = 0U;
			std::uint8_t                         Channel            = 0U;
			bool                                 ChannelRead        = false;
			bool                                 SawPackData        = false;
			bool                                 SinkAborted        = false;
			std::optional<git_protocol_v2_error> Error;
			std::uint64_t                        WireBytes     = 0U;
			std::uint64_t                        PackBytes     = 0U;
			std::uint64_t                        ProgressBytes = 0U;
			std::uint64_t                        Packets       = 0U;
		};
	} // namespace detail

	using git_protocol_v2_client_error = std::variant<git_smart_transport_error, git_protocol_v2_error>;

	template <typename Value>
	using git_protocol_v2_client_result = std::expected<Value, git_protocol_v2_client_error>;

	/// \brief Describes the git protocol v2 acquisition result record.
	struct git_protocol_v2_acquisition_result
	{
		git_protocol_v2_refs         AdvertisedRefs;
		git_protocol_v2_fetch_result Fetch;

		auto operator<=>(const git_protocol_v2_acquisition_result&) const = default;
	};

	/// \brief Computes the acquire git protocol v2 pack result.
	[[nodiscard]] auto acquire_git_protocol_v2_pack(
		git_smart_transport_port&             aTransport,
		const git_smart_request_context&      aContext,
		std::span<const git_protocol_v2_have> aHaves,
		git_pack_stream_sink&                 aSink,
		git_protocol_v2_fetch_limits          aLimits,
		std::stop_token                       aStop)
		-> git_protocol_v2_client_result<git_protocol_v2_acquisition_result>;
}

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GIT_PROTOCOL_V2_HPP
