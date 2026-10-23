#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_CAPABILITY_PROBE_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_CAPABILITY_PROBE_HPP

#include "github_host_profile.hpp"

#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/github_capability_diagnostic.hpp"
#include "bpr/core/github_model.hpp"
#include <span>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>

namespace bpr::adapters::github::detail {
	enum class capability_probe_unavailability : std::uint8_t {
		none,
		invalid_contract,
		invalid_limits,
		authentication,
		permission,
		network,
		api_version,
		rate_limit,
		certificate,
		redirect,
		timeout,
		cancelled,
		invalid_status,
		invalid_content_type,
		partial_response,
		malformed_response,
		response_limit,
		allocation_failure,
	};

	enum class capability_probe_http_method : std::uint8_t {
		post,
	};

	enum class capability_schema_fact : std::uint8_t {
		review_threads_read,
		viewed_file_state_read,
		mark_file_as_viewed,
		unmark_file_as_viewed,
		resolve_permission,
		resolve_review_thread,
		unresolve_permission,
		unresolve_review_thread,
		/// [[BPR-1746]]. `PullRequestReviewThread.viewerCanReply`.
		reply_permission,
		/// [[BPR-1746]]. `PullRequest.locked`.
		conversation_lock,
		count,
	};

	enum class capability_schema_fact_state : std::uint8_t {
		unknown,
		matched,
		missing,
		incompatible,
	};

	[[nodiscard]] constexpr auto valid(
		const capability_probe_unavailability aReason) noexcept -> bool
	{
		switch (aReason) {
		case capability_probe_unavailability::none:
		case capability_probe_unavailability::invalid_contract:
		case capability_probe_unavailability::invalid_limits:
		case capability_probe_unavailability::authentication:
		case capability_probe_unavailability::permission:
		case capability_probe_unavailability::network:
		case capability_probe_unavailability::api_version:
		case capability_probe_unavailability::rate_limit:
		case capability_probe_unavailability::certificate:
		case capability_probe_unavailability::redirect:
		case capability_probe_unavailability::timeout:
		case capability_probe_unavailability::cancelled:
		case capability_probe_unavailability::invalid_status:
		case capability_probe_unavailability::invalid_content_type:
		case capability_probe_unavailability::partial_response:
		case capability_probe_unavailability::malformed_response:
		case capability_probe_unavailability::response_limit:
		case capability_probe_unavailability::allocation_failure:
			return true;
		}
		return false;
	}

	[[nodiscard]] constexpr auto valid(
		const capability_probe_http_method aMethod) noexcept -> bool
	{
		return aMethod == capability_probe_http_method::post;
	}

	[[nodiscard]] constexpr auto valid(
		const capability_schema_fact_state aState) noexcept -> bool
	{
		switch (aState) {
		case capability_schema_fact_state::unknown:
		case capability_schema_fact_state::matched:
		case capability_schema_fact_state::missing:
		case capability_schema_fact_state::incompatible:
			return true;
		}
		return false;
	}

	inline constexpr std::size_t capability_schema_fact_count                = static_cast<std::size_t>(capability_schema_fact::count);
	inline constexpr std::size_t maximum_capability_probe_rate_limit_headers = 5U;

	struct capability_probe_limits
	{
		std::size_t MaximumRequestBytes      = 16U * 1024U;
		std::size_t MaximumResponseBytes     = 1U * 1024U * 1024U;
		std::size_t MaximumDepth             = 32U;
		std::size_t MaximumObjectMembers     = 64U;
		std::size_t MaximumArrayElements     = 1'024U;
		std::size_t MaximumQueryFields       = 1'024U;
		std::size_t MaximumMutationFields    = 1'024U;
		std::size_t MaximumRepositoryFields  = 512U;
		std::size_t MaximumPullRequestFields = 256U;
		std::size_t MaximumOtherTypeFields   = 64U;
		std::size_t MaximumNamedTypes        = 512U;
		std::size_t MaximumArguments         = 16U;
		std::size_t MaximumInputFields       = 16U;
		std::size_t MaximumEnumValues        = 32U;
		std::size_t MaximumTotalValues       = 50'000U;
		std::size_t MaximumStringBytes       = 4U * 1024U;

		auto operator<=>(const capability_probe_limits&) const = default;
	};

	struct capability_probe_header
	{
		std::string Name;
		std::string Value;

		auto operator==(const capability_probe_header&) const -> bool = default;
	};

	inline constexpr std::uint8_t capability_probe_part_count = 9U;

	struct capability_probe_request
	{
		std::uint8_t                           Part = 0U;
		core::host_profile_id                  Profile;
		core::git_host_id                      Host;
		application::github_profile_revision   ProfileRevision;
		application::github_server_contract    Contract = application::github_server_contract::github_com_2022_11_28;
		capability_probe_http_method           Method   = capability_probe_http_method::post;
		std::string                            Authority;
		std::string                            Root;
		std::string                            Path;
		std::array<capability_probe_header, 3> Headers;
		std::string                            Body;

		auto operator==(const capability_probe_request&) const -> bool = default;
	};

	struct capability_probe_response
	{
		int         Status        = 0;
		bool        Authenticated = false;
		std::string ContentType;
		std::string Body;
		std::array<
			capability_probe_header,
			maximum_capability_probe_rate_limit_headers>
					RateLimitHeaders{ };
		std::size_t RateLimitHeaderCount = 0U;

		auto operator==(const capability_probe_response&) const -> bool = default;
	};

	using capability_probe_transport_result = std::expected<
		capability_probe_response,
		capability_probe_unavailability>;

	struct capability_probe_observation
	{
		core::github_capabilities Capabilities;
		std::array<
			capability_schema_fact_state,
			capability_schema_fact_count>
												  Facts{ };
		capability_probe_unavailability           Unavailability   = capability_probe_unavailability::partial_response;
		bool                                      Complete         = false;
		application::github_capability_diagnostic CapabilityDetail = application::github_capability_diagnostic::none;

		auto operator<=>(const capability_probe_observation&) const = default;
	};

	[[nodiscard]] auto valid(
		const capability_probe_observation& aObservation) noexcept -> bool;

	using capability_probe_request_result = std::expected<
		capability_probe_request,
		capability_probe_unavailability>;

	[[nodiscard]] auto capability_probe_graphql_document(std::uint8_t aPart = 0U) noexcept
		-> std::string_view;

	[[nodiscard]] auto capability_probe_part_aliases(std::uint8_t aPart) noexcept -> std::span<const std::string_view>;

	[[nodiscard]] auto capability_probe_document_is_query_only(
		std::string_view aDocument) noexcept -> bool;

	[[nodiscard]] auto compile_capability_probe_request(
		const validated_github_host_profile& aProfile,
		const capability_probe_limits& aLimits = { }, std::uint8_t aPart = 0U)
		-> capability_probe_request_result;

	/// \brief Request-local, bounded assembly; only the complete plan produces capabilities.
	class capability_probe_batch
	{
	public:
		capability_probe_batch();
		~capability_probe_batch();
		capability_probe_batch(const capability_probe_batch&)                    = delete;
		auto operator=(const capability_probe_batch&) -> capability_probe_batch& = delete;
		capability_probe_batch(capability_probe_batch&&) noexcept;
		auto operator=(capability_probe_batch&&) noexcept -> capability_probe_batch&;

		[[nodiscard]] auto append(const capability_probe_response& aResponse, std::uint8_t aPart) noexcept -> std::expected<void, capability_probe_observation>;
		[[nodiscard]] auto finish() const noexcept -> capability_probe_observation;

	private:
		struct assembly;
		std::unique_ptr<assembly> Assembly;
		capability_probe_response Response;
		std::size_t               Bytes  = 0U;
		std::size_t               Values = 0U;
		std::uint8_t              Parts  = 0U;
		bool                      Failed = false;
	};

	/// \brief Extracts only error prose while the caller owns the active credential.
	[[nodiscard]] auto sanitize_capability_error(std::string_view aBody, std::span<const std::byte> aCredential) noexcept -> application::diagnostic_summary;

	[[nodiscard]] auto normalize_capability_probe(
		const capability_probe_transport_result& aResponse,
		const capability_probe_limits&           aLimits = { }) noexcept
		-> capability_probe_observation;
} // namespace bpr::adapters::github::detail

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_CAPABILITY_PROBE_HPP
