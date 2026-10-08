#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REPOSITORY_ADDITION_INPUT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REPOSITORY_ADDITION_INPUT_HPP

#include "bpr/core/github_model.hpp"

#include <compare>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace bpr::application {
	/// \brief A syntactic repository root, without account/configuration/save authority.
	struct repository_addition_target
	{
		std::string              Hostname;
		core::repository_locator Repository;
		auto                     operator<=>(const repository_addition_target&) const = default;
	};

	/// \brief Concrete input correction categories; presentation owns their wording.
	enum class repository_addition_input_problem : std::uint8_t {
		empty,
		too_long,
		invalid_character,
		repository_root_required,
		invalid_host,
		invalid_repository,
	};

	/// \brief Admits owner/name or an HTTPS repository root with an optional trailing slash.
	/// \details Surrounding ASCII whitespace is trimmed. Hostnames are canonicalized;
	/// repository spelling is preserved under the existing locator grammar. A URL host
	/// is returned explicitly, never substituted by aDefaultHostname or configured here.
	[[nodiscard]] auto parse_repository_addition(std::string_view aText, std::string_view aDefaultHostname)
		-> std::expected<repository_addition_target, repository_addition_input_problem>;
}
#endif
