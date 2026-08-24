/// \file
/// \brief Production-shaped retained supervisor identity continuity checks.

#include "designated_live_macos_capsule_identity.hpp"
#include "designated_live_macos_supervisor_identity.hpp"
#include "designated_live_macos_supervisor_launch.hpp"

#include <CommonCrypto/CommonDigest.h>
#include <mach-o/dyld.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <signal.h>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
	namespace capsule_identity    = bpr::app::designated_live_capsule_identity;
	namespace supervisor_identity = bpr::app::designated_live_supervisor_identity;
	namespace supervisor_launch   = bpr::app::designated_live_supervisor_launch;

	constexpr auto application_relative = supervisor_launch::application_relative;
	constexpr auto helper_relative      = std::string_view{
		"BetterPullReview.app/Contents/Helpers/"
		"better_pull_review_designated_live_supervisor"
	};
	constexpr auto verifier_relative = std::string_view{
		"BetterPullReview.app/Contents/Resources/"
		"designated_live_macos_capsule.py"
	};
	constexpr auto qt_conf_relative = std::string_view{
		"BetterPullReview.app/Contents/Resources/qt.conf"
	};
	constexpr auto nested_relative = std::string_view{
		"BetterPullReview.app/Contents/Resources/identity-marker.txt"
	};
	constexpr auto source_sha256 = std::string_view{
		"7777777777777777777777777777777777777777777777777777777777777777"
	};

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

	[[nodiscard]] auto running_executable()
		-> std::optional<std::filesystem::path>
	{
		auto required = std::uint32_t{ 0U };
		static_cast<void>(::_NSGetExecutablePath(nullptr, &required));
		if (required == 0U) {
			return std::nullopt;
		}
		auto buffer = std::vector<char>(required, '\0');
		if (::_NSGetExecutablePath(buffer.data(), &required) != 0) {
			return std::nullopt;
		}
		std::error_code error;
		auto            result = std::filesystem::canonical(buffer.data(), error);
		return error ? std::nullopt : std::optional{ std::move(result) };
	}

	[[nodiscard]] auto wait_success(const pid_t aProcess) -> bool
	{
		int status = 0;
		for (;;) {
			const auto waited = ::waitpid(aProcess, &status, 0);
			if (waited < 0 && errno == EINTR) {
				continue;
			}
			return waited == aProcess && WIFEXITED(status)
				&& WEXITSTATUS(status) == EXIT_SUCCESS;
		}
	}

	[[nodiscard]] auto run_command(
		const std::filesystem::path& aExecutable,
		std::vector<std::string>     aArguments) -> bool
	{
		auto pointers = std::vector<char*>{ };
		pointers.reserve(aArguments.size() + 2U);
		auto executable = aExecutable.string();
		pointers.push_back(executable.data());
		for (auto& argument : aArguments) {
			pointers.push_back(argument.data());
		}
		pointers.push_back(nullptr);
		auto pathValue = std::array{
			'P', 'A', 'T', 'H', '=', '/', 'u', 's', 'r', '/', 'b', 'i', 'n', ':',
			'/', 'b', 'i', 'n', ':', '/', 'u', 's', 'r', '/', 's', 'b', 'i', 'n',
			':', '/', 's', 'b', 'i', 'n', '\0'
		};
		auto  environment = std::array<char*, 2>{ pathValue.data(), nullptr };
		pid_t process     = -1;
		return posix_spawn(
				   &process,
				   executable.c_str(),
				   nullptr,
				   nullptr,
				   pointers.data(),
				   environment.data())
			== 0
			&& wait_success(process);
	}

	[[nodiscard]] auto sha256_hex(const std::string_view aBytes) -> std::string
	{
		auto digest = std::array<unsigned char, CC_SHA256_DIGEST_LENGTH>{ };
		CC_SHA256(
			aBytes.data(), static_cast<CC_LONG>(aBytes.size()), digest.data());
		static constexpr auto digits = std::string_view{ "0123456789abcdef" };
		auto                  result = std::string{ };
		result.reserve(digest.size() * 2U);
		for (const auto byte : digest) {
			result.push_back(digits[(byte >> 4U) & 0x0fU]);
			result.push_back(digits[byte & 0x0fU]);
		}
		return result;
	}

	[[nodiscard]] auto file_bytes(const std::filesystem::path& aPath) -> std::string
	{
		auto source = std::ifstream{ aPath, std::ios::binary };
		return {
			std::istreambuf_iterator<char>{ source },
			std::istreambuf_iterator<char>{ },
		};
	}

	[[nodiscard]] auto inventory_text(const int aRootDescriptor) -> std::string
	{
		const auto entries = capsule_identity::capture(aRootDescriptor);
		if (!entries.has_value()) {
			return { };
		}
		auto result = std::string{
			"schema_name=bpr.designated_live_macos_capsule\n"
			"schema_version=1\n"
			"source_sha256="
		};
		result.append(source_sha256);
		result.append("\napplication=").append(application_relative);
		result.append("\nsupervisor=").append(helper_relative);
		result.append("\nverifier=").append(verifier_relative).append("\n");
		for (const auto& entry : *entries) {
			const auto kind = S_ISDIR(entry.Mode) ? std::string_view{ "directory" }
				: S_ISREG(entry.Mode)             ? std::string_view{ "file" }
												  : std::string_view{ "symlink" };
			auto       mode = std::array<char, 5>{ };
			static_cast<void>(std::snprintf(
				mode.data(), mode.size(), "%04o", entry.Mode & 07777U));
			result.append(kind).push_back('\t');
			result.append(mode.data()).push_back('\t');
			if (entry.Sha256.has_value()) {
				result.append(*entry.Sha256);
			} else if (entry.SymlinkTarget.has_value()) {
				result.append(*entry.SymlinkTarget);
			} else {
				result.push_back('-');
			}
			result.push_back('\t');
			result.append(entry.Relative).push_back('\n');
		}
		return result;
	}

	struct fixture
	{
		std::filesystem::path Parent;
		std::filesystem::path Root;
		std::filesystem::path Inventory;
		std::string           InventoryBytes;
	};

	[[nodiscard]] auto make_fixture(
		const std::filesystem::path& aParent,
		const std::filesystem::path& aVerifier) -> std::optional<fixture>
	{
		const auto executable = running_executable();
		if (!executable.has_value()) {
			return std::nullopt;
		}
		std::filesystem::create_directories(aParent);
		std::filesystem::permissions(aParent, std::filesystem::perms::owner_all);
		const auto root = aParent / "capsule";
		std::filesystem::create_directories(root);
		std::filesystem::permissions(root, std::filesystem::perms::owner_all);
		const auto application = root / application_relative;
		const auto helper      = root / helper_relative;
		const auto verifier    = root / verifier_relative;
		std::filesystem::create_directories(application.parent_path());
		std::filesystem::create_directories(helper.parent_path());
		std::filesystem::create_directories(verifier.parent_path());
		std::filesystem::copy_file(
			*executable, application, std::filesystem::copy_options::overwrite_existing);
		std::filesystem::copy_file(
			*executable, helper, std::filesystem::copy_options::overwrite_existing);
		std::filesystem::copy_file(
			aVerifier, verifier, std::filesystem::copy_options::overwrite_existing);
		std::filesystem::permissions(
			application,
			std::filesystem::perms::owner_read
				| std::filesystem::perms::owner_write
				| std::filesystem::perms::owner_exec);
		std::filesystem::permissions(
			helper,
			std::filesystem::perms::owner_read
				| std::filesystem::perms::owner_write
				| std::filesystem::perms::owner_exec);
		std::filesystem::permissions(
			verifier, std::filesystem::perms::owner_read);
		write_file(root / qt_conf_relative, "[Paths]\nPlugins = PlugIns\n");
		write_file(root / nested_relative, "A\n");
		write_file(
			root / "BetterPullReview.app/Contents/Info.plist",
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
			"\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
			"<plist version=\"1.0\"><dict>"
			"<key>CFBundleExecutable</key><string>BetterPullReview</string>"
			"<key>CFBundleIdentifier</key>"
			"<string>io.github.BetterPullReview.identity-test</string>"
			"</dict></plist>\n");
		if (!run_command(
				"/usr/bin/codesign",
				{
					"--force",
					"--deep",
					"--sign",
					"-",
					"--timestamp=none",
					(root / "BetterPullReview.app").string(),
				})) {
			return std::nullopt;
		}
		const auto rootDescriptor = capsule_identity::open_root(root);
		if (!rootDescriptor.has_value()) {
			return std::nullopt;
		}
		auto inventoryBytes = inventory_text(*rootDescriptor);
		static_cast<void>(::close(*rootDescriptor));
		if (inventoryBytes.empty()) {
			return std::nullopt;
		}
		const auto inventory = aParent / "capsule.inventory";
		write_file(inventory, inventoryBytes);
		return fixture{
			.Parent         = aParent,
			.Root           = root,
			.Inventory      = inventory,
			.InventoryBytes = std::move(inventoryBytes),
		};
	}

	struct verifier_effects
	{
		int         SpawnAttempts = 0;
		std::string Executable;
	};

	struct verifier_course
	{
		supervisor_launch::verifier_outcome Outcome;
		verifier_effects                    Effects;
	};

	[[nodiscard]] auto run_verifier_controller(
		const supervisor_identity::session& aSession,
		std::function<void()>               aBeforeProductionSpawn = { })
		-> verifier_course
	{
		auto result          = verifier_course{ };
		auto operations      = supervisor_launch::verifier_operations::production();
		auto productionSpawn = std::move(operations.Spawn);
		operations.Spawn
			= [&, productionSpawn = std::move(productionSpawn),
			   aBeforeProductionSpawn = std::move(aBeforeProductionSpawn)](
				  pid_t*                            aProcess,
				  const char*                       aExecutable,
				  const posix_spawn_file_actions_t* aActions,
				  const posix_spawnattr_t*          aAttributes,
				  char* const                       aArguments[],
				  char* const                       aEnvironment[]) mutable {
				  ++result.Effects.SpawnAttempts;
				  result.Effects.Executable = aExecutable;
				  if (aBeforeProductionSpawn) {
					  aBeforeProductionSpawn();
				  }
				  return productionSpawn(
					  aProcess,
					  aExecutable,
					  aActions,
					  aAttributes,
					  aArguments,
					  aEnvironment);
			  };
		result.Outcome = supervisor_launch::verify_static(
			aSession, source_sha256, std::move(operations));
		return result;
	}

	[[nodiscard]] auto early_exit_is_safe(
		const supervisor_identity::session& aSession) -> bool
	{
		posix_spawn_file_actions_t actions{ };
		if (posix_spawn_file_actions_init(&actions) != 0) {
			return false;
		}
		auto transport  = aSession.configure_verifier_spawn(actions);
		auto executable = std::string{ "/usr/bin/false" };
		auto arguments  = std::array<char*, 2>{ executable.data(), nullptr };
		auto pathValue  = std::array{
			'P', 'A', 'T', 'H', '=', '/', 'u', 's', 'r', '/', 'b', 'i', 'n', ':',
			'/', 'b', 'i', 'n', '\0'
		};
		auto       environment = std::array<char*, 2>{ pathValue.data(), nullptr };
		pid_t      process     = -1;
		const auto spawned     = transport.has_value()
			? posix_spawn(
				  &process,
				  executable.c_str(),
				  &actions,
				  nullptr,
				  arguments.data(),
				  environment.data())
			: EINVAL;
		static_cast<void>(posix_spawn_file_actions_destroy(&actions));
		if (spawned != 0) {
			return false;
		}
		const auto childFailed = !wait_success(process);
		return childFailed
			&& !aSession.transmit_verifier_and_inventory(*transport);
	}

	struct launch_effects
	{
		int         SpawnAttempts  = 0;
		int         ResumeAttempts = 0;
		int         Refusals       = 0;
		std::string Executable;
	};

	struct launch_course
	{
		supervisor_launch::outcome Outcome;
		launch_effects             Effects;
		bool                       ChildSucceeded = false;
	};

	[[nodiscard]] auto run_launch_controller(
		const supervisor_identity::session&           aSession,
		const std::filesystem::path&                  aApplication,
		const std::filesystem::path&                  aMarker,
		std::function<void(supervisor_launch::phase)> aBarrier        = { },
		const bool                                    aPostSpawnCheck = true)
		-> std::optional<launch_course>
	{
		posix_spawn_file_actions_t actions{ };
		if (posix_spawn_file_actions_init(&actions) != 0) {
			return std::nullopt;
		}
		posix_spawnattr_t attributes{ };
		if (posix_spawnattr_init(&attributes) != 0) {
			static_cast<void>(posix_spawn_file_actions_destroy(&actions));
			return std::nullopt;
		}
		if (!aSession.configure_application_spawn(actions)) {
			static_cast<void>(posix_spawn_file_actions_destroy(&actions));
			static_cast<void>(posix_spawnattr_destroy(&attributes));
			return std::nullopt;
		}
		sigset_t emptySet{ };
		sigset_t defaultSet{ };
		static_cast<void>(sigemptyset(&emptySet));
		static_cast<void>(sigfillset(&defaultSet));
		const auto flags = static_cast<short>(
			POSIX_SPAWN_CLOEXEC_DEFAULT | POSIX_SPAWN_START_SUSPENDED
			| POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);
		if (posix_spawnattr_setflags(&attributes, flags) != 0
			|| posix_spawnattr_setsigmask(&attributes, &emptySet) != 0
			|| posix_spawnattr_setsigdefault(&attributes, &defaultSet) != 0) {
			static_cast<void>(posix_spawn_file_actions_destroy(&actions));
			static_cast<void>(posix_spawnattr_destroy(&attributes));
			return std::nullopt;
		}
		auto executable = std::string{ application_relative };
		auto childFlag  = std::string{ "--identity-child" };
		auto marker     = aMarker.string();
		auto arguments  = std::array<char*, 4>{
			executable.data(), childFlag.data(), marker.data(), nullptr
		};
		auto pathValue = std::array{
			'P', 'A', 'T', 'H', '=', '/', 'u', 's', 'r', '/', 'b', 'i', 'n', ':',
			'/', 'b', 'i', 'n', ':', '/', 'u', 's', 'r', '/', 's', 'b', 'i', 'n',
			':', '/', 's', 'b', 'i', 'n', '\0'
		};
		auto environment = std::array<char*, 2>{ pathValue.data(), nullptr };
		auto result      = launch_course{ };
		auto operations  = supervisor_launch::operations::production(
			[aPostSpawnCheck](const pid_t) { return aPostSpawnCheck; });
		operations.Barrier   = std::move(aBarrier);
		auto productionSpawn = std::move(operations.Spawn);
		operations.Spawn
			= [&, productionSpawn = std::move(productionSpawn)](
				  pid_t*                            aProcess,
				  const char*                       aExecutable,
				  const posix_spawn_file_actions_t* aActions,
				  const posix_spawnattr_t*          aAttributes,
				  char* const                       aArguments[],
				  char* const                       aEnvironment[]) mutable {
				  ++result.Effects.SpawnAttempts;
				  result.Effects.Executable = aExecutable;
				  return productionSpawn(
					  aProcess,
					  aExecutable,
					  aActions,
					  aAttributes,
					  aArguments,
					  aEnvironment);
			  };
		auto productionResume = std::move(operations.Resume);
		operations.Resume
			= [&, productionResume = std::move(productionResume)](
				  const pid_t aProcess) mutable {
				  ++result.Effects.ResumeAttempts;
				  return productionResume(aProcess);
			  };
		auto productionRefuse = std::move(operations.Refuse);
		operations.Refuse
			= [&, productionRefuse = std::move(productionRefuse)](
				  const pid_t aProcess) mutable {
				  ++result.Effects.Refusals;
				  productionRefuse(aProcess);
			  };
		result.Outcome = supervisor_launch::launch(
			aSession,
			aApplication,
			actions,
			attributes,
			arguments.data(),
			environment.data(),
			std::move(operations));
		static_cast<void>(posix_spawn_file_actions_destroy(&actions));
		static_cast<void>(posix_spawnattr_destroy(&attributes));
		if (result.Outcome.Resumed) {
			result.ChildSucceeded = wait_success(result.Outcome.Process);
		}
		return result;
	}

	void replace_inventory(const fixture& aFixture)
	{
		std::filesystem::rename(
			aFixture.Inventory, aFixture.Parent / "capsule.inventory.retained");
		write_file(
			aFixture.Inventory,
			aFixture.InventoryBytes + "substituted_inventory=true\n");
	}

	void replace_root(const fixture& aFixture)
	{
		const auto replacement = aFixture.Parent / "capsule-replacement";
		std::filesystem::copy(
			aFixture.Root,
			replacement,
			std::filesystem::copy_options::recursive
				| std::filesystem::copy_options::copy_symlinks);
		write_file(replacement / nested_relative, "B\n");
		std::filesystem::rename(
			aFixture.Root, aFixture.Parent / "capsule-retained");
		std::filesystem::rename(replacement, aFixture.Root);
	}

	void replace_verifier_with_fake_success(const fixture& aFixture)
	{
		const auto verifier = aFixture.Root / verifier_relative;
		std::filesystem::rename(
			verifier, verifier.parent_path() / "designated_live_macos_capsule.retained.py");
		write_file(
			verifier,
			"raise SystemExit(0)\n",
			std::filesystem::perms::owner_read);
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	if (argc == 3 && argv != nullptr && argv[1] != nullptr && argv[2] != nullptr
		&& std::string_view{ argv[1] } == "--identity-child") {
		write_file(argv[2], "resumed\n");
		return EXIT_SUCCESS;
	}
	if (argc != 2 || argv == nullptr || argv[1] == nullptr) {
		return EXIT_FAILURE;
	}
	const auto base = std::filesystem::temp_directory_path()
		/ ("bpr-supervisor-identity-" + std::to_string(::getpid()));
	std::filesystem::remove_all(base);
	std::filesystem::create_directories(base);
	std::filesystem::permissions(base, std::filesystem::perms::owner_all);
	const auto verifier       = std::filesystem::canonical(argv[1]);
	const auto verifierDigest = sha256_hex(file_bytes(verifier));

	{
		const auto current = make_fixture(base / "happy", verifier);
		expect(current.has_value(), "the exact signed fixture is created");
		if (current.has_value()) {
			auto retained = supervisor_identity::session::open(
				current->Root, current->Inventory, verifierDigest);
			expect(retained.has_value(), "root and inventory are retained before verification");
			if (retained.has_value()) {
				expect(
					early_exit_is_safe(*retained),
					"early verifier exit refuses without delivering SIGPIPE");
				const auto verifierCourse = run_verifier_controller(*retained);
				expect(
					verifierCourse.Outcome.accepted()
						&& verifierCourse.Effects.SpawnAttempts == 1
						&& verifierCourse.Effects.Executable
							== supervisor_launch::python_executable,
					"the real static verifier succeeds through retained objects");
				expect(retained->revalidate(), "the verified retained session revalidates");
				const auto marker = current->Parent / "child-resumed";
				const auto course = run_launch_controller(
					*retained,
					std::filesystem::path{ application_relative },
					marker);
				expect(
					course.has_value() && course->Effects.SpawnAttempts == 1
						&& course->Effects.Executable == application_relative,
					"the unchanged production controller permits exactly one spawn");
				expect(
					course.has_value() && course->Effects.ResumeAttempts == 1,
					"the unchanged production controller permits exactly one SIGCONT");
				expect(
					course.has_value() && course->ChildSucceeded,
					"the retained-root child exits successfully");
				expect(
					std::filesystem::is_regular_file(marker),
					"the resumed retained-root child executes");
			}
		}
	}

	{
		const auto current = make_fixture(base / "hostile-verifier", verifier);
		expect(current.has_value(), "the hostile-verifier fixture is created");
		if (current.has_value()) {
			replace_verifier_with_fake_success(*current);
			const auto retained = supervisor_identity::session::open(
				current->Root, current->Inventory, verifierDigest);
			expect(
				!retained.has_value(),
				"a fake-success verifier is refused by the compiled trusted digest");
		}
	}

	{
		const auto current = make_fixture(base / "verifier-substitution", verifier);
		expect(current.has_value(), "the verifier-substitution fixture is created");
		if (current.has_value()) {
			auto retained = supervisor_identity::session::open(
				current->Root, current->Inventory, verifierDigest);
			expect(retained.has_value(), "the trusted verifier is retained before substitution");
			if (retained.has_value()) {
				replace_verifier_with_fake_success(*current);
				const auto verifierCourse = run_verifier_controller(*retained);
				expect(
					verifierCourse.Effects.SpawnAttempts == 0
						&& !verifierCourse.Outcome.Spawned,
					"post-retention fake-success verifier substitution reaches no spawn");
			}
		}
	}

	{
		const auto current = make_fixture(base / "inventory-retained-writer", verifier);
		expect(current.has_value(), "the retained-writer inventory fixture is created");
		if (current.has_value()) {
			const auto writable = ::open(current->Inventory.c_str(), O_WRONLY | O_CLOEXEC);
			auto       retained = supervisor_identity::session::open(
				current->Root, current->Inventory, verifierDigest);
			expect(writable >= 0 && retained.has_value(), "a same-UID writable fd is retained before verification");
			if (writable >= 0 && retained.has_value()) {
				const auto replacement    = std::array<char, 1>{ 'X' };
				auto       changed        = false;
				const auto verifierCourse = run_verifier_controller(
					*retained,
					[&] {
						changed = ::pwrite(
									  writable,
									  replacement.data(),
									  replacement.size(),
									  0)
								== static_cast<ssize_t>(replacement.size())
							&& ::fsync(writable) == 0;
					});
				expect(changed, "the retained writable fd mutates same-size named bytes");
				expect(
					verifierCourse.Outcome.Succeeded
						&& verifierCourse.Outcome.Transmitted,
					"the real verifier receives anonymous retained inventory A bytes");
				expect(
					retained->inventory_sha256() == sha256_hex(current->InventoryBytes),
					"the bootstrap digest remains exact inventory A");
				expect(
					!retained->revalidate(),
					"same-inode inventory mutation refuses before application spawn");
			}
			if (writable >= 0) {
				static_cast<void>(::close(writable));
			}
		}
	}

	{
		const auto current = make_fixture(base / "root-during-verifier", verifier);
		expect(current.has_value(), "the during-verifier root fixture is created");
		if (current.has_value()) {
			auto retained = supervisor_identity::session::open(
				current->Root, current->Inventory, verifierDigest);
			expect(
				retained.has_value(),
				"the during-verifier root session opens before verification");
			if (retained.has_value()) {
				const auto verifierCourse = run_verifier_controller(
					*retained, [&] { replace_root(*current); });
				expect(
					verifierCourse.Effects.SpawnAttempts == 1
						&& verifierCourse.Outcome.Transmitted
						&& verifierCourse.Outcome.Succeeded,
					"whole-root replacement during verification still verifies retained A");
				expect(
					!verifierCourse.Outcome.Revalidated
						&& !verifierCourse.Outcome.accepted(),
					"whole-root replacement during verification breaks continuity");
				const auto marker = current->Parent / "unexpected-child";
				const auto course = run_launch_controller(
					*retained,
					std::filesystem::path{ application_relative },
					marker);
				expect(
					course.has_value() && course->Effects.SpawnAttempts == 0,
					"during-verifier root replacement refuses before spawn");
				expect(
					course.has_value() && course->Effects.ResumeAttempts == 0,
					"during-verifier root replacement reaches no SIGCONT");
				expect(
					!std::filesystem::exists(marker),
					"during-verifier root replacement executes no child code");
			}
		}
	}

	{
		const auto current = make_fixture(base / "root-substitution", verifier);
		expect(current.has_value(), "the root-substitution fixture is created");
		if (current.has_value()) {
			auto retained = supervisor_identity::session::open(
				current->Root, current->Inventory, verifierDigest);
			expect(
				retained.has_value(),
				"the root-substitution session opens before verification");
			if (retained.has_value()) {
				const auto verifierCourse = run_verifier_controller(*retained);
				expect(
					verifierCourse.Outcome.accepted(),
					"root A passes the real static verifier");
				replace_root(*current);
				const auto marker = current->Parent / "unexpected-child";
				const auto course = run_launch_controller(
					*retained,
					std::filesystem::path{ application_relative },
					marker);
				expect(
					course.has_value() && course->Effects.SpawnAttempts == 0,
					"whole-root A-to-B substitution refuses before spawn");
				expect(
					course.has_value() && course->Effects.ResumeAttempts == 0,
					"whole-root A-to-B substitution reaches no SIGCONT");
				expect(
					!std::filesystem::exists(marker),
					"the substituted root executes no child code");
			}
		}
	}

	for (const auto phase : { std::string_view{ "during" }, std::string_view{ "after" } }) {
		const auto current = make_fixture(base / ("inventory-" + std::string{ phase }), verifier);
		expect(current.has_value(), "the inventory-substitution fixture is created");
		if (!current.has_value()) {
			continue;
		}
		auto retained = supervisor_identity::session::open(
			current->Root, current->Inventory, verifierDigest);
		if (!retained.has_value()) {
			expect(false, "the inventory-substitution session opens");
			continue;
		}
		const auto digestA            = sha256_hex(current->InventoryBytes);
		const auto duringVerification = phase == "during";
		const auto verifierCourse     = run_verifier_controller(
			*retained,
			duringVerification
				? std::function<void()>{ [&] { replace_inventory(*current); } }
				: std::function<void()>{ });
		expect(
			verifierCourse.Outcome.Transmitted
				&& verifierCourse.Outcome.Succeeded,
			"the real verifier consumes retained inventory A bytes");
		if (phase == "after") {
			replace_inventory(*current);
		}
		expect(
			retained->inventory_sha256() == digestA,
			"the application-bound digest remains exact inventory A");
		const auto marker = current->Parent / "unexpected-child";
		const auto course = run_launch_controller(
			*retained,
			std::filesystem::path{ application_relative },
			marker);
		expect(
			course.has_value() && course->Effects.SpawnAttempts == 0,
			"inventory A-to-B substitution refuses before spawn");
		expect(
			course.has_value() && course->Effects.ResumeAttempts == 0,
			"inventory A-to-B substitution reaches no SIGCONT");
	}

	for (const auto mutation : {
			 std::string_view{ "whole-root" },
			 std::string_view{ "nested-resource" },
		 }) {
		for (const auto controlledPhase : {
				 supervisor_launch::phase::BeforeSpawn,
				 supervisor_launch::phase::BeforeResume,
			 }) {
			const auto phaseName = controlledPhase == supervisor_launch::phase::BeforeSpawn
				? std::string_view{ "before-spawn" }
				: std::string_view{ "before-resume" };
			const auto current   = make_fixture(
				base / (std::string{ "controller-" } + std::string{ mutation } + "-" + std::string{ phaseName }),
				verifier);
			expect(current.has_value(), "the controller hostile fixture is created");
			if (!current.has_value()) {
				continue;
			}
			auto retained = supervisor_identity::session::open(
				current->Root, current->Inventory, verifierDigest);
			expect(retained.has_value(), "the controller hostile session is retained");
			if (!retained.has_value()) {
				continue;
			}
			const auto marker = current->Parent / "unexpected-child";
			const auto course = run_launch_controller(
				*retained,
				std::filesystem::path{ application_relative },
				marker,
				[&](const supervisor_launch::phase aObserved) {
					if (aObserved != controlledPhase) {
						return;
					}
					if (mutation == "whole-root") {
						replace_root(*current);
					} else {
						write_file(current->Root / nested_relative, "B\n");
					}
				});
			const auto expectedSpawns
				= controlledPhase == supervisor_launch::phase::BeforeSpawn ? 0 : 1;
			const auto expectedRefusals
				= controlledPhase == supervisor_launch::phase::BeforeResume ? 1 : 0;
			expect(
				course.has_value()
					&& course->Effects.SpawnAttempts == expectedSpawns
					&& (expectedSpawns == 0
						|| course->Effects.Executable == application_relative),
				"the production controller has the exact hostile spawn count");
			expect(
				course.has_value() && course->Effects.ResumeAttempts == 0,
				"the production controller sends no hostile SIGCONT");
			expect(
				course.has_value()
					&& course->Effects.Refusals == expectedRefusals,
				"the production controller reaps only a spawned hostile child");
			expect(
				!std::filesystem::exists(marker),
				"the production controller executes no hostile child code");
		}
	}

	{
		const auto current = make_fixture(base / "absolute-application", verifier);
		expect(current.has_value(), "the absolute-application fixture is created");
		if (current.has_value()) {
			auto retained = supervisor_identity::session::open(
				current->Root, current->Inventory, verifierDigest);
			expect(retained.has_value(), "the absolute-application session is retained");
			if (retained.has_value()) {
				const auto marker = current->Parent / "unexpected-child";
				const auto course = run_launch_controller(
					*retained,
					current->Root / application_relative,
					marker);
				expect(
					course.has_value() && course->Effects.SpawnAttempts == 0,
					"an absolute application mutation reaches no production spawn");
				expect(
					course.has_value() && course->Effects.ResumeAttempts == 0,
					"an absolute application mutation reaches no SIGCONT");
			}
		}
	}

	{
		const auto current = make_fixture(base / "restore-failure", verifier);
		expect(current.has_value(), "the restore-failure fixture is created");
		if (current.has_value()) {
			auto retained = supervisor_identity::session::open(
				current->Root, current->Inventory, verifierDigest);
			expect(retained.has_value(), "the restore-failure session is retained");
			if (retained.has_value()) {
				const auto marker = current->Parent / "unexpected-child";
				const auto course = run_launch_controller(
					*retained,
					std::filesystem::path{ application_relative },
					marker,
					{ },
					false);
				auto alreadyReaped = false;
				if (course.has_value() && course->Outcome.Spawned) {
					int status    = 0;
					errno         = 0;
					alreadyReaped = ::waitpid(
										course->Outcome.Process, &status, WNOHANG)
							== -1
						&& errno == ECHILD;
				}
				expect(
					course.has_value() && course->Effects.SpawnAttempts == 1,
					"restore failure occurs after exactly one production spawn");
				expect(
					course.has_value() && course->Effects.Refusals == 1,
					"restore failure kills and reaps the suspended child");
				expect(
					alreadyReaped,
					"restore failure leaves no child for the supervisor to reap");
				expect(
					course.has_value() && course->Effects.ResumeAttempts == 0,
					"restore failure reaches no SIGCONT");
				expect(
					!std::filesystem::exists(marker),
					"restore failure executes no child code");
			}
		}
	}

	std::filesystem::remove_all(base);
	if (failures == 0) {
		std::cout << "macos-supervisor-identity-continuity-tests:pass\n";
	}
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
