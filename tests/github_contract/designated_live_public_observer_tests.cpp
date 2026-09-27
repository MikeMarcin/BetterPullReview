#include "designated_live_public_observer.hpp"

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stop_token>
#include <string_view>

namespace {
	using namespace bpr;

	[[nodiscard]] auto parse_port(const std::string_view aText)
		-> std::optional<std::uint16_t>
	{
		auto       value  = std::uint32_t{ 0U };
		const auto result = std::from_chars(
			aText.data(), aText.data() + aText.size(), value);
		return result.ec == std::errc{ }
				&& result.ptr == aText.data() + aText.size()
				&& value > 0U && value <= 65'535U
			? std::optional{ static_cast<std::uint16_t>(value) }
			: std::nullopt;
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	if (argc == 2 && std::string_view{ argv[1] } == "--cancelled") {
		auto cancellation = std::stop_source{ };
		cancellation.request_stop();
		const auto observed
			= adapters::github::detail::observe_designated_live_public_target(
				cancellation.get_token());
		const auto passed = !observed.has_value()
			&& observed.error()
				== adapters::github::detail::designated_live_public_observation_failure::cancelled;
		std::cout << (passed ? "DESIGNATED_PUBLIC_OBSERVER_OK\n"
							 : "DESIGNATED_PUBLIC_OBSERVER_FAILED:cancelled\n");
		return passed ? EXIT_SUCCESS : EXIT_FAILURE;
	}
	if (argc != 6 || std::string_view{ argv[1] } != "--port"
		|| std::string_view{ argv[3] } != "--ca") {
		return EXIT_FAILURE;
	}
	const auto port     = parse_port(argv[2]);
	const auto ca       = std::filesystem::path{ argv[4] };
	const auto expected = std::string_view{ argv[5] };
	if (!port.has_value() || !ca.is_absolute()
		|| (expected != "success" && expected != "identity"
			&& expected != "redirect" && expected != "response-limit")) {
		return EXIT_FAILURE;
	}
	const auto observed
		= adapters::github::detail::observe_designated_live_public_target_for_test({
			.Port                 = *port,
			.CertificateAuthority = ca,
		});
	const auto exactFailure = [&](const auto aFailure) {
		return !observed.has_value() && observed.error() == aFailure;
	};
	const auto passed = expected == "success"
		? observed.has_value()
			&& observed->Hostname.value() == "github.com"
			&& observed->Repository.Owner == "MikeMarcin"
			&& observed->Repository.Name == "BPR-Test"
			&& observed->Number.value() == 2U
			&& observed->RepositoryDatabaseId == 1'328'277'631U
			&& observed->RepositoryNodeId == "R_kgDOTyvofw"
			&& observed->PullRequestDatabaseId == 4'650'736'621U
			&& observed->PullRequestNodeId == "PR_kwDOTyvof88AAAABFTSb7Q"
			&& observed->BaseRepositoryDatabaseId == 1'328'277'631U
			&& observed->BaseRef == "codex/rfc0022-fixture-base-20260926"
			&& observed->HeadRef == "codex/rfc0022-live-comments-20260926"
			&& observed->Open
		: expected == "identity"
		? exactFailure(adapters::github::detail::
						   designated_live_public_observation_failure::identity_mismatch)
		: expected == "redirect"
		? exactFailure(adapters::github::detail::
						   designated_live_public_observation_failure::redirect)
		: exactFailure(adapters::github::detail::
						   designated_live_public_observation_failure::response_limit);
	if (passed) {
		std::cout << "DESIGNATED_PUBLIC_OBSERVER_OK\n";
	} else {
		std::cout << "DESIGNATED_PUBLIC_OBSERVER_FAILED:";
		if (observed.has_value()) {
			std::cout << "unexpected-success";
		} else {
			std::cout << static_cast<unsigned int>(observed.error());
		}
		std::cout << '\n';
	}
	return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
