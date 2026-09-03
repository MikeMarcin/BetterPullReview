/// \file
/// \brief The one address shape the composed opener forwards, and nothing else.
///
/// \details BPR-0537. `system_browser_opener::open` re-parses the verification
/// address strictly and forwards only an absolute `https` address that has a
/// host and no userinfo; before this its evidence was three owner observations
/// of a browser that did open, which say nothing about the addresses it must
/// refuse.
///
/// Nothing here launches anything. The accept/refuse decision is asked of
/// `validated_browser_address` directly, and `open` is driven with the
/// platform launch substituted for a stand-in that counts, so "a refused
/// address reaches no launch" is a number this test reads rather than the
/// absence of a window somebody would have to be watching for.

#include "../../src/app/src/system_browser_opener.hpp"

#include <QString>
#include <QUrl>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>

namespace {
	using bpr::app::validated_browser_address;
	using bpr::app::detail::substitute_browser_launch;

	int Failures = 0;

	void expect(const bool aCondition, const std::string_view aWhat)
	{
		if (!aCondition) {
			std::cerr << "system-browser-opener-tests:failed:" << aWhat << '\n';
			++Failures;
		}
	}

	/// The addresses a caller could hand this opener and must not reach a
	/// launch: the wrong scheme, no scheme at all, embedded credentials, no
	/// host, nothing, a scheme a launch service would happily run as code, a
	/// scheme that reads the local disk, and a string strict parsing refuses.
	constexpr std::string_view refused_addresses[] = {
		"http://github.com/login/device",
		"github.com/login/device",
		"https://user@github.com/login/device",
		"https://user:secret@github.com/login/device",
		"https:///login/device",
		"",
		"javascript:alert(1)",
		"file:///etc/passwd",
		"https://git hub.com/login/device",
	};

	constexpr std::string_view accepted_address = "https://github.com/login/device";

	/// The stand-in for the platform launch, and the only thing in this file
	/// that a forwarded address can reach.
	struct launch_record final
	{
		int     Calls = 0;
		QString Address;
	};

	auto launches() -> launch_record&
	{
		static auto record = launch_record{ };
		return record;
	}

	auto record_launch(const QUrl& aAddress) -> bool
	{
		auto& seen = launches();
		++seen.Calls;
		seen.Address = aAddress.toString();
		return true;
	}

	auto as_qstring(const std::string_view aText) -> QString
	{
		return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size()));
	}

	void the_verification_address_is_accepted()
	{
		const auto accepted = validated_browser_address(accepted_address);
		expect(accepted.has_value(), "an absolute https address must be accepted");
		if (accepted.has_value()) {
			expect(
				accepted->toString() == as_qstring(accepted_address),
				"the accepted address must be the one that was asked about");
		}
	}

	void every_other_shape_is_refused()
	{
		for (const auto address : refused_addresses) {
			const auto refused = validated_browser_address(address);
			if (refused.has_value()) {
				std::cerr << "system-browser-opener-tests:forwarded:" << address
						  << '\n';
				++Failures;
			}
		}
	}

	/// The half `validated_browser_address` cannot state on its own.
	void a_refused_address_reaches_no_launch()
	{
		const auto replaced = substitute_browser_launch(&record_launch);
		launches()          = launch_record{ };
		for (const auto address : refused_addresses) {
			auto opener = bpr::app::system_browser_opener{ };
			expect(!opener.open(address), "open must refuse the address");
		}
		expect(launches().Calls == 0, "a refused address must reach no launch");

		auto opener = bpr::app::system_browser_opener{ };
		expect(opener.open(accepted_address), "open must forward the accepted address");
		expect(launches().Calls == 1, "the accepted address must launch exactly once");
		expect(
			launches().Address == as_qstring(accepted_address),
			"the launched address must be the one that was opened");

		// Restored, and the seam proves it swapped: what comes back is the
		// stand-in this test installed, not whatever it replaced.
		const auto restored = substitute_browser_launch(nullptr);
		expect(restored == &record_launch, "the seam must answer with the launch it replaced");
		expect(replaced != &record_launch, "the platform launch must have been in place first");
	}
} // namespace

auto main() -> int
{
	the_verification_address_is_accepted();
	every_other_shape_is_refused();
	a_refused_address_reaches_no_launch();

	if (Failures != 0) {
		std::cerr << "system-browser-opener-tests:failures=" << Failures << '\n';
		return EXIT_FAILURE;
	}
	std::cout << "system-browser-opener-tests:pass\n";
	return EXIT_SUCCESS;
}
