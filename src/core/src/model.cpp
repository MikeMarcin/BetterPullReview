#include "bpr/core/model.hpp"

namespace bpr::core {
	auto name(const authority_domain aAuthority) noexcept -> std::string_view
	{
		switch (aAuthority) {
		case authority_domain::github_shared:
			return "GitHub shared state";
		case authority_domain::local_private:
			return "Local private state";
		case authority_domain::derived_projection:
			return "Derived projection";
		}

		return "Unknown authority";
	}
}
