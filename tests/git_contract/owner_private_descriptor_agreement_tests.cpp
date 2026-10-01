// [[BPR-0900]]. The product's half of the owner-private descriptor agreement
// row. The driver, `tests/scripts/owner_private_descriptor_agreement_tests.py`,
// has each author narrow its own directory, then asks this executable to judge
// them with the product's own rule.
//
//   narrow <directory>  narrows an existing directory the way the product
//                       narrows a storage root it owns
//   judge <directory>   asks the agent workspace ownership whether the
//                       directory may parent a workspace neighbourhood, which
//                       is `workspace_parent_trusted`, and prints the answer
#include "bpr/adapters/git/agent_workspace_ownership.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

auto main(const int aArgc, char** aArgv) -> int
{
	if (aArgc != 3) {
		std::cerr << "usage: owner_private_descriptor_agreement narrow|judge <directory>\n";
		return 2;
	}
	const auto mode      = std::string_view{ aArgv[1] };
	const auto directory = std::filesystem::path{ aArgv[2] };
	if (mode == "narrow") {
		if (!bpr::adapters::sqlite::make_storage_root_owner_private(directory)) {
			std::cout << "PRODUCT_NARROW_FAILED\n";
			return 1;
		}
		std::cout << "PRODUCT_NARROWED\n";
		return 0;
	}
	if (mode == "judge") {
		// The registry and the neighbourhood both sit directly in the judged
		// directory, so it is the neighbourhood's parent and the registry's.
		const auto result = bpr::adapters::git::recover_installed_agent_workspaces(directory / "registry", directory / "neighbourhood");
		if (result) {
			std::cout << "TRUSTED\n";
		} else {
			std::cout << "REFUSED " << static_cast<int>(result.error()) << '\n';
		}
		return 0;
	}
	std::cerr << "unknown mode\n";
	return 2;
}
