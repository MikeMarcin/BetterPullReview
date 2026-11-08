#include "github_application/credential_prompt.hpp"

#include "bpr/qt/sensitive_text_input.hpp"

#include <expected>
#include <utility>

namespace bpr::app {
	auto prompt_for_credential(
		QWidget* aParent) -> std::optional<application::github_credential_material>
	{
		auto input = qt::prompt_for_sensitive_text(
			application::maximum_github_credential_bytes,
			aParent);
		if (!input.has_value()) {
			return std::nullopt;
		}

		auto material = application::github_credential_material::create(input->bytes());
		input->reset();
		if (!material.has_value()) {
			return std::nullopt;
		}
		return std::move(*material);
	}
}
