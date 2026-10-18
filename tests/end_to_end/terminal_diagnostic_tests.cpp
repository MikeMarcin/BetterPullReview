#include "../../src/app/src/startup_diagnostic.hpp"
#include "../../src/app/src/terminal_diagnostics.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int main(int aArgc, char** aArgv)
{
	using namespace bpr::app::detail;
	{
		terminal_diagnostics diagnostics;
		if (aArgc == 2 && std::string_view{ aArgv[1] } == "--stress") {
			const auto                record = std::string{ "TERMINAL_RECORD:" } + std::string(1000U, 'x') + '\n';
			std::vector<std::jthread> writers;
			writers.reserve(8U);
			for (unsigned writer = 0U; writer < 8U; ++writer) {
				writers.emplace_back([&] { for (unsigned index = 0U; index < 10000U; ++index) { record_terminal_diagnostic(record); } });
			}
			writers.clear();
			// Let ordinary file/pipe delivery drain before the recovery record.
			std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });
			record_terminal_diagnostic("TERMINAL_RECORD:recovered\n");
		} else {
			qWarning("BPR_DIAGNOSTIC_PRIVATE_CANARY request body and source");
			terminal_text_stream{ } << "STARTUP_FAILURE_SOURCE:"
									<< QString::fromStdString(bounded_startup_diagnostic(std::string(1024U * 1024U, 'p').c_str())) << '\n';
			terminal_text_stream{ } << QString(4096, 'x');
			record_terminal_diagnostic("TERMINAL_RECORD:finished\n");
		}
	}
	std::cout << "terminal-diagnostics:complete\n";
}
