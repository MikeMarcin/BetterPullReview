#include "bpr/adapters/update/release_update_installation.hpp"
#include <QCoreApplication>

auto main(int argc, char** argv) -> int
{
	QCoreApplication application{ argc, argv };
	if (argc != 4) {
		return 1;
	}
	const auto operation = std::string_view{ argv[1] };
	if (operation != "--install" && operation != "--recover") {
		return 1;
	}
	const auto installed = std::filesystem::path{ argv[2] };
	if (!installed.is_absolute() || installed.string().size() > 4096U || std::string_view{ argv[3] }.size() != 36U) {
		return 1;
	}
	return bpr::adapters::update::run_release_update_helper(installed, argv[3], operation == "--recover");
}
