#ifndef BPR_APP_GITHUB_APPLICATION_CREDENTIAL_PROMPT_HPP
#define BPR_APP_GITHUB_APPLICATION_CREDENTIAL_PROMPT_HPP
#include "bpr/application/github_credential_vault.hpp"

#include <QWidget>

#include <optional>

// Asks the reviewer for a GitHub credential through the sensitive-text prompt.
namespace bpr::app {
	[[nodiscard]] auto prompt_for_credential(
		QWidget* aParent) -> std::optional<application::github_credential_material>;
}
#endif
