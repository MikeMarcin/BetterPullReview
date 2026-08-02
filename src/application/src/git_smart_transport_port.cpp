#include "bpr/application/git_smart_transport_port.hpp"

#include "bpr/core/epoch_transition.hpp"

#include <algorithm>
#include <string_view>

namespace bpr::application {
	namespace {
		[[nodiscard]] constexpr auto forbidden_ref_character(
			const char aCharacter) noexcept -> bool
		{
			const auto byte = static_cast<unsigned char>(aCharacter);
			return byte <= 0x20U || byte == 0x7fU || aCharacter == '~'
				|| aCharacter == '^' || aCharacter == ':' || aCharacter == '?'
				|| aCharacter == '*' || aCharacter == '[' || aCharacter == '\\';
		}

		[[nodiscard]] auto valid_ref_component(
			const std::string_view aComponent) noexcept -> bool
		{
			return !aComponent.empty() && aComponent != "." && aComponent != ".."
				&& aComponent.front() != '.' && aComponent.back() != '.'
				&& !aComponent.ends_with(".lock");
		}
	}

	auto parse_git_base_branch_ref(const std::string_view aValue) noexcept
		-> std::optional<git_base_branch_ref>
	{
		constexpr std::string_view prefix = "refs/heads/";
		if (
			aValue.size() <= prefix.size()
			|| aValue.size() > maximum_git_base_branch_ref_bytes
			|| !aValue.starts_with(prefix) || aValue.ends_with('/')
			|| aValue.ends_with('.') || aValue.find("..") != std::string_view::npos
			|| aValue.find("@{") != std::string_view::npos
			|| std::any_of(
				aValue.begin(),
				aValue.end(),
				forbidden_ref_character)) {
			return std::nullopt;
		}

		auto componentStart = prefix.size();
		while (componentStart < aValue.size()) {
			const auto separator    = aValue.find('/', componentStart);
			const auto componentEnd = separator == std::string_view::npos ? aValue.size() : separator;
			if (!valid_ref_component(
					aValue.substr(
						componentStart,
						componentEnd - componentStart))) {
				return std::nullopt;
			}
			if (separator == std::string_view::npos) {
				break;
			}
			componentStart = separator + 1U;
		}

		git_base_branch_ref result;
		std::copy(aValue.begin(), aValue.end(), result.Characters.begin());
		result.Length = static_cast<std::uint16_t>(aValue.size());
		return result;
	}

	auto valid(const git_smart_request_context& aContext) noexcept -> bool
	{
		return aContext.ApplicationSession.value() != 0U
			&& core::valid(aContext.Authority)
			&& core::valid(aContext.Revision)
			&& core::valid(aContext.Repository) && valid(aContext.BaseRef);
	}

	auto valid(const git_smart_discovery_request& aRequest) noexcept -> bool
	{
		return valid(aRequest.Context);
	}

	auto valid(const git_smart_ls_refs_request& aRequest) noexcept -> bool
	{
		return valid(aRequest.Context) && !aRequest.Body.empty()
			&& aRequest.Body.size() <= maximum_git_smart_request_bytes;
	}

	auto valid(const git_smart_fetch_request& aRequest) noexcept -> bool
	{
		return valid(aRequest.Context) && !aRequest.Body.empty()
			&& aRequest.Body.size() <= maximum_git_smart_request_bytes
			&& aRequest.MaximumResponseBytes != 0U
			&& aRequest.MaximumResponseBytes
			<= maximum_git_smart_fetch_response_bytes;
	}
}
