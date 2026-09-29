#ifndef BPR_ADAPTERS_SQLITE_TOUR_OWNER_CHANNEL_HPP
#define BPR_ADAPTERS_SQLITE_TOUR_OWNER_CHANNEL_HPP
#include "bpr/application/profile_runtime_lease.hpp"
#include "bpr/application/tour_owner_channel.hpp"
#include <filesystem>
namespace bpr::adapters::sqlite {
	/// \brief Separate versioned native tour endpoint; requires the existing exact-root lease.
	[[nodiscard]] auto make_tour_owner_server(const std::filesystem::path&, core::bpr_profile_id, std::shared_ptr<application::profile_runtime_lease>) -> std::expected<std::shared_ptr<application::tour_owner_server>, application::tour_channel_failure>;
	/// \brief Authenticates both peers and binds the root, profile and owner instance. Sends once.
	[[nodiscard]] auto send_tour_owner(const std::filesystem::path&, core::bpr_profile_id, std::span<const std::uint8_t>, std::stop_token = { }) -> std::expected<application::tour_channel_bytes, application::tour_channel_failure>;
	[[nodiscard]] auto encode_tour_request(application::tour_owner_request) -> application::tour_channel_bytes;
	[[nodiscard]] auto decode_tour_request(std::span<const std::uint8_t>) -> std::optional<application::tour_owner_request>;
	[[nodiscard]] auto encode_tour_response(application::tour_owner_response) -> application::tour_channel_bytes;
	[[nodiscard]] auto decode_tour_response(std::span<const std::uint8_t>) -> std::optional<application::tour_owner_response>;
	/// \brief Computes canonical payload identity; never trusts a caller-provided digest.
	[[nodiscard]] auto tour_request_digest(const application::tour_service_request&) -> std::array<std::uint8_t, 32>;
}
#endif
