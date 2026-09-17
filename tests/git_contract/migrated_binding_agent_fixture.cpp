#include "bpr/adapters/mcp/mcp_context_helper.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#if defined(_WIN32)
	#include <fcntl.h>
	#include <io.h>
#endif

namespace {
	auto consume_binding() -> bool
	{
		std::ifstream bindingFile{ "binding" };
		std::string   frozen;
		if (!std::getline(bindingFile, frozen)) {
			return false;
		}
		const auto  binding = bpr::adapters::mcp::decode_revision_binding(frozen);
		std::string line;
		if (!binding || !std::getline(std::cin, line)
			|| line != "BPR-CONTEXT/2 EXTERNAL-TERMINAL") {
			return false;
		}
		std::map<std::string, std::string> fields;
		std::size_t                        bytes = 0;
		while (std::getline(std::cin, line)) {
			bytes += line.size() + 1U;
			const auto tab = line.find('\t');
			if (bytes > 65536U || tab == std::string::npos
				|| !fields.emplace(line.substr(0, tab), line.substr(tab + 1U)).second) {
				return false;
			}
		}
		const std::map<std::string, std::string> expected{
			{ "host", std::to_string(binding->Host.value()) },
			{ "repository", std::to_string(binding->Repository.value()) },
			{ "pull_request", std::to_string(binding->PullRequest.value()) },
			{ "base", std::string{ bpr::core::canonical_hex(binding->BaseOid).view() } },
			{ "head", std::string{ bpr::core::canonical_hex(binding->HeadOid).view() } },
			{ "left_epoch", std::to_string(binding->LeftEpoch.value()) },
			{ "right_epoch", std::to_string(binding->RightEpoch.value()) },
		};
		for (const auto& [name, value] : expected) {
			const auto found = fields.find(name);
			if (found == fields.end() || found->second != value) {
				return false;
			}
		}
		// Emit values consumed from stdin only after validating the frozen reference.
		std::cout << "consumed " << fields.at("host") << ' ' << fields.at("repository")
				  << ' ' << fields.at("pull_request") << ' ' << fields.at("left_epoch")
				  << ' ' << fields.at("right_epoch") << ' ' << fields.at("base")
				  << ' ' << fields.at("head") << '\n';
		return true;
	}
}

auto main() -> int
{
#if defined(_WIN32)
	// [[BPR-0853]]. Binary standard streams, as every raw-protocol fixture in
	// this tree selects (BPR-0240): the CRT's text mode would end the receipt
	// line with "\r\n", and the parent compares the bytes this fixture wrote.
	for (FILE* stream : { stdin, stdout, stderr }) {
		const auto descriptor = _fileno(stream);
		if (descriptor >= 0) {
			static_cast<void>(_setmode(descriptor, _O_BINARY));
		}
	}
#endif
	if (!consume_binding()) {
		std::cerr << "migration binding rejected\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
