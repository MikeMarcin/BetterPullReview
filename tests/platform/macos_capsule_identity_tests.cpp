/// \file
/// \brief Proves a nested capsule substitution is refused at the barrier.
///
/// BPR-0262 fact_2. The recursive check that already existed runs in a Python
/// verifier which has fully exited before the child is resumed, and the
/// supervisor's own post-verification checks reach only the top-level
/// executable. This exercises the component that closes that interval.
///
/// The barrier here is an ordinary function call, not a race: capture, mutate,
/// revalidate. BPR-0354 asks for exactly that — "the barrier is deterministic
/// and hermetic; a timing-dependent reproduction is not evidence" — so nothing
/// in this file sleeps, spawns, or retries.

#include "designated_live_macos_capsule_identity.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {
	namespace identity = bpr::app::designated_live_capsule_identity;

	auto failures = 0;

	void expect(const bool aCondition, const std::string_view aClaim)
	{
		if (!aCondition) {
			++failures;
			std::cerr << "FAIL: " << aClaim << '\n';
		}
	}

	void write_file(
		const std::filesystem::path& aPath,
		const std::string_view       aContents,
		const std::filesystem::perms aPermissions
		= std::filesystem::perms::owner_read | std::filesystem::perms::owner_write)
	{
		std::filesystem::create_directories(aPath.parent_path());
		auto sink = std::ofstream{ aPath, std::ios::binary | std::ios::trunc };
		sink.write(aContents.data(), static_cast<std::streamsize>(aContents.size()));
		sink.close();
		std::filesystem::permissions(aPath, aPermissions);
	}

	/// A capsule shaped like the real one: an executable, a nested framework
	/// reached through a symlink, and a plugin. The nested entries are the
	/// point — the top-level executable was never the gap.
	auto make_capsule(const std::filesystem::path& aRoot) -> void
	{
		write_file(
			aRoot / "BetterPullReview.app/Contents/MacOS/better_pull_review",
			"application");
		write_file(
			aRoot / "BetterPullReview.app/Contents/Helpers/supervisor",
			"supervisor");
		write_file(
			aRoot / "BetterPullReview.app/Contents/Resources/qt.conf",
			"[Paths]\nPlugins = PlugIns\n");
		write_file(
			aRoot / "BetterPullReview.app/Contents/PlugIns/platforms/libqcocoa.dylib",
			"platform plugin");
		write_file(
			aRoot
				/ "BetterPullReview.app/Contents/Frameworks/QtCore.framework/Versions/A/QtCore",
			"framework");
		std::filesystem::create_directory_symlink(
			"A",
			aRoot
				/ "BetterPullReview.app/Contents/Frameworks/QtCore.framework/Versions/Current");
	}

	class retained_root
	{
	public:
		explicit retained_root(const std::filesystem::path& aRoot)
			: Descriptor{ identity::open_root(aRoot) }
		{
		}

		retained_root(const retained_root&)                    = delete;
		auto operator=(const retained_root&) -> retained_root& = delete;

		~retained_root()
		{
			if (Descriptor.has_value()) {
				static_cast<void>(::close(*Descriptor));
			}
		}

		[[nodiscard]] auto get() const -> int
		{
			return Descriptor.value_or(-1);
		}

		[[nodiscard]] auto valid() const -> bool
		{
			return Descriptor.has_value();
		}

	private:
		std::optional<int> Descriptor;
	};
} // namespace

auto main() -> int
{
	const auto base = std::filesystem::temp_directory_path()
		/ ("bpr-capsule-identity-" + std::to_string(::getpid()));
	std::filesystem::remove_all(base);
	std::filesystem::create_directories(base);

	const auto root = base / "capsule";
	std::filesystem::create_directory(root);
	make_capsule(root);

	const auto retained = retained_root{ root };
	expect(retained.valid(), "a capsule root opens for retention");

	const auto captured = identity::capture(retained.get());
	expect(captured.has_value(), "the capsule captures completely");
	if (!captured.has_value()) {
		std::filesystem::remove_all(base);
		return EXIT_FAILURE;
	}
	expect(
		captured->size() >= 6U,
		"the capture reaches nested entries rather than the top level only");
	expect(
		identity::unchanged(retained.get(), *captured),
		"an untouched capsule revalidates");

	// The barrier. Each case substitutes exactly one nested entry after the
	// capture and requires the revalidation to refuse.
	{
		const auto plugin = root
			/ "BetterPullReview.app/Contents/PlugIns/platforms/libqcocoa.dylib";
		const auto original = std::string{ "platform plugin" };
		write_file(plugin, "substituted plugin");
		expect(
			!identity::unchanged(retained.get(), *captured),
			"a substituted nested plugin is refused");
		write_file(plugin, original);
	}

	{
		const auto framework = root
			/ "BetterPullReview.app/Contents/Frameworks/QtCore.framework/Versions/A/QtCore";
		write_file(framework, "framework");
		// Same bytes, new inode and timestamp: content equality alone would
		// have missed this, which is why the capture records identity too.
		expect(
			!identity::unchanged(retained.get(), *captured),
			"a nested file replaced with identical bytes is still refused");
	}

	{
		const auto added = root
			/ "BetterPullReview.app/Contents/PlugIns/platforms/extra.dylib";
		write_file(added, "added");
		expect(
			!identity::unchanged(retained.get(), *captured),
			"an entry added after the capture is refused");
		std::filesystem::remove(added);
	}

	{
		const auto qtConf
			= root / "BetterPullReview.app/Contents/Resources/qt.conf";
		std::filesystem::remove(qtConf);
		expect(
			!identity::unchanged(retained.get(), *captured),
			"an entry removed after the capture is refused");
	}

	std::filesystem::remove_all(base);
	if (failures == 0) {
		std::cout << "macos-capsule-identity-tests:pass\n";
	}
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
