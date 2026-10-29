#include "../../src/adapters/update/src/macos_release_update_mounts.hpp"
#include "bpr/adapters/update/release_update_staging.hpp"
#include "bpr/application/manual_executor.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>

#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>

namespace {
	using namespace bpr;
	using reason = core::release_update_reason;
	void expect(bool aValue, const char* aMessage)
	{
		if (!aValue) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
}
auto main(int aArgc, char** aArgv) -> int
{
	QCoreApplication application{ aArgc, aArgv };
	const auto       inventory = R"(<plist version="1.0"><dict><key>images</key><array><dict><key>system-entities</key><array><dict><key>dev-entry</key><string>/dev/disk99s1</string><key>mount-point</key><string>/Volumes/Fixture</string></dict></array></dict></array></dict></plist>)";
	expect(adapters::update::detail::release_update_volume_is_image(inventory, "/dev/disk99s1") == true, "writable image recognized by exact device");
	expect(adapters::update::detail::release_update_volume_is_image(inventory, "/dev/disk99s10") == false, "unrelated writable volume is not an image");
	expect(adapters::update::detail::release_update_volume_is_image("<plist version=\"1.0\"><dict><key>images</key><array/></dict></plist>", "/dev/disk1s1") == false, "empty mounted-image inventory accepted");
	for (const auto invalid : { "", "invalid", "<plist version=\"1.0\"><array/></plist>", "<plist version=\"1.0\"><dict><key>images</key><string>bad</string></dict></plist>", "<plist version=\"1.0\"><dict><key>images</key><array><dict/></array></dict></plist>" }) {
		expect(!adapters::update::detail::release_update_volume_is_image(invalid, "/dev/disk1s1"), "malformed mounted-image observation refuses eligibility");
	}
	expect(!adapters::update::detail::release_update_volume_is_image(std::string(65537U, 'x'), "/dev/disk1s1"), "mount inventory is bounded");
	expect(aArgc == 6, "staging fixture arguments");
	const std::filesystem::path  root{ aArgv[1] }, installed{ aArgv[2] }, zip{ aArgv[3] };
	const std::string            scenario{ aArgv[4] }, version{ aArgv[5] };
	application::manual_executor cleanup;
	auto                         adapter   = adapters::update::make_release_update_staging(root, installed, cleanup);
	auto                         candidate = core::release_update_candidate{ core::release_update_release_id{ 10U }, core::release_update_asset_id{ 20U }, *core::parse_release_update_version(version), "v" + version, "BetterPullReview-" + version + "-macos-universal.zip", std::filesystem::file_size(zip), { } };
	QFile                        input{ QString::fromStdString(zip.string()) };
	expect(input.open(QIODevice::ReadOnly), "fixture archive opens");
	QCryptographicHash digest{ QCryptographicHash::Sha256 };
	expect(digest.addData(&input), "fixture digest");
	input.seek(0);
	candidate.Sha256.emplace();
	const auto expected = digest.result();
	for (std::size_t index = 0; index < 32U; ++index) {
		(*candidate.Sha256)[index] = static_cast<std::uint8_t>(expected[static_cast<qsizetype>(index)]);
	}
	if (scenario == "digest") {
		candidate.Sha256->fill(0);
	}
	auto writer = adapter->begin(candidate);
	expect(writer != nullptr, "owner-private writer opens");
	expect(!adapter->begin(candidate), "second transaction cannot take staging lock");
	expect(!writer->finish(), "short download is not an archive");
	while (!input.atEnd()) {
		const auto block = input.read(65536);
		expect(writer->append(std::as_bytes(std::span{ block.constData(), static_cast<std::size_t>(block.size()) })), "stream written within declared bound");
	}
	auto archive = writer->finish();
	expect(archive != nullptr, "exact download finishes");
	expect(!writer->append(std::array{ std::byte{ 0 } }), "finished writer refuses excess bytes");
	writer.reset();
	std::filesystem::path transaction;
	for (const auto& entry : std::filesystem::directory_iterator{ root / "updates" }) {
		if (entry.path().filename().string().starts_with("transaction-")) {
			transaction = entry.path();
		}
	}
	expect(!transaction.empty(), "exact transaction recorded");
	if (scenario == "changed-archive") {
		std::ofstream changed{ transaction / "download.zip", std::ios::app | std::ios::binary };
		changed.put('x');
	}
	std::stop_source stop;
	if (scenario == "cancel") {
		stop.request_stop();
	}
	auto                                result = adapter->verify(archive, stop.get_token());
	const std::map<std::string, reason> refusals{
		{ "numbered", reason::wrong_signer }, { "numbered-zero", reason::invalid_bundle }, { "numbered-mismatch", reason::invalid_bundle }, { "numbered-missing-seal", reason::invalid_bundle }, { "expanded-size", reason::size_limit }, { "entry-limit", reason::size_limit }, { "digest", reason::digest_mismatch }, { "changed-archive", reason::changed_candidate }, { "cancel", reason::cancelled }, { "traversal", reason::unsafe_archive }, { "absolute", reason::unsafe_archive }, { "symlink-escape", reason::unsafe_archive }, { "symlink-write", reason::unsafe_archive }, { "duplicate", reason::unsafe_archive }, { "device", reason::unsafe_archive }, { "wrong-top", reason::invalid_bundle }, { "wrong-id", reason::invalid_bundle }, { "wrong-version", reason::invalid_bundle }, { "future-os", reason::invalid_bundle }, { "wrong-architecture", reason::wrong_architecture }, { "unsigned", reason::wrong_signer }
	};
	const auto failure = scenario == "signed" ? reason::none : refusals.at(scenario);
	std::cout << "staging:" << scenario << ":reason=" << static_cast<int>(result.Failure) << ":expected=" << static_cast<int>(failure) << '\n';
	expect(result.Failure == failure, "native validation terminal matches criterion");
	if (scenario == "signed") {
		expect(adapters::update::release_update_installation_capability(installed, { }) == core::release_update_capability::eligible, "native signed installed copy is eligible after bounded mount inventory");
		expect(result.Bundle && adapter->revalidate(result.Bundle, { }) == reason::none, "exact signed candidate revalidates");
		std::ofstream changed{ transaction / "BetterPullReview.app/Contents/Resources/rfc0035-fixture-change" };
		changed << "fixture";
		changed.close();
		expect(adapter->revalidate(result.Bundle, { }) == reason::changed_candidate, "changed staged tree rejected before replacement");
		std::cout << "signed-source-sha256:" << expected.toHex().toStdString() << '\n';
	}
	result.Bundle.reset();
	archive.reset();
	cleanup.drain();
	expect(!std::filesystem::exists(transaction), "owned transaction cleaned on cleanup lane");
	std::cout << "release-update-staging:pass\n";
}
