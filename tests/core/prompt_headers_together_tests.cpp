/// \file
/// \brief RFC-BPR-0107's `prompt_kind` (agent_configuration.hpp) and
/// RFC-BPR-0125's `prompt_ask` (prompt_macro.hpp) live in one namespace: both
/// headers compile together and each enum keeps its own name.

#include "bpr/core/agent_configuration.hpp"
#include "bpr/core/prompt_macro.hpp"

#include <iostream>
#include <type_traits>

static_assert(!std::is_same_v<bpr::core::prompt_kind, bpr::core::prompt_ask>, "the two enums are distinct types");

auto main() -> int
{
	const auto ask = bpr::core::prompt_ask::review;
	std::cout << "prompt-headers-together:pass " << bpr::core::stable_name(ask) << '\n';
	return 0;
}
