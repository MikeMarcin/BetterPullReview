#include "bpr/application/review_mutation_application_host.hpp"
#include "review_mutation_execution_authority.hpp"

#include "../support/test_expectation.hpp"
#include "bpr/core/sha256.hpp"

#include <array>
#include <atomic>
#include <barrier>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#if !defined(_WIN32)
	#include <fcntl.h>
	#include <pwd.h>
	#include <spawn.h>
	#include <sys/stat.h>
	#include <sys/wait.h>
	#include <unistd.h>
	#if defined(__APPLE__)
		#include <mach-o/dyld.h>
		#include <membership.h>
		#include <sys/acl.h>
	#endif
#else
	#include <windows.h>
#endif

namespace {
	using namespace bpr;

	using bpr::tests::expect;

	[[nodiscard]] auto hex_digest(const std::string_view aValue) -> std::string
	{
		constexpr auto digits = std::string_view{ "0123456789abcdef" };
		const auto     digest = core::sha256(std::span{
			reinterpret_cast<const std::uint8_t*>(aValue.data()), aValue.size() });
		auto           result = std::string{ };
		for (const auto byte : digest) {
			result.push_back(digits[(static_cast<unsigned>(byte) >> 4U) & 0x0fU]);
			result.push_back(digits[byte & 0x0fU]);
		}
		return result;
	}

	[[nodiscard]] auto running_executable() -> std::filesystem::path
	{
#if defined(__APPLE__)
		auto required = std::uint32_t{ 0U };
		static_cast<void>(::_NSGetExecutablePath(nullptr, &required));
		auto path = std::vector<char>(required, '\0');
		expect(required != 0U && ::_NSGetExecutablePath(path.data(), &required) == 0,
			   "resolve running test executable");
		return std::filesystem::canonical(path.data());
#elif defined(__linux__)
		return std::filesystem::canonical("/proc/self/exe");
#else
		auto       path = std::vector<wchar_t>(32U * 1024U, L'\0');
		const auto size = ::GetModuleFileNameW(
			nullptr, path.data(), static_cast<DWORD>(path.size()));
		expect(size != 0U && size < path.size(), "resolve running test executable");
		return std::filesystem::path{ path.data(), path.data() + size };
#endif
	}

	[[nodiscard]] auto read_file(const std::filesystem::path& aPath) -> std::string
	{
		auto input = std::ifstream{ aPath, std::ios::binary };
		expect(input.is_open(), "open running executable for fixture digest");
		return { std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
	}

	class temporary_directory final
	{
	public:
		temporary_directory()
		{
#if defined(_WIN32)
			Root = std::filesystem::temp_directory_path()
				/ ("bpr-attended-bootstrap-" + std::to_string(::GetCurrentProcessId())
				   + "-" + std::to_string(Next.fetch_add(1U)));
			expect(std::filesystem::create_directory(Root),
				   "create owner-private bootstrap directory");
			// [[BPR-1121]]. Removed again if the rest of construction fails.
			auto made = bpr::tests::scratch_guard{ Root };

			Root = std::filesystem::canonical(Root);
			made.release();
#else
			Root = std::filesystem::temp_directory_path()
				/ ("bpr-attended-bootstrap-" + std::to_string(::getpid()) + "-"
				   + std::to_string(Next.fetch_add(1U)));
			expect(std::filesystem::create_directory(Root),
				   "create owner-private bootstrap directory");
			// [[BPR-1121]]. Removed again if the rest of construction fails.
			auto made = bpr::tests::scratch_guard{ Root };

			Root = std::filesystem::canonical(Root);
			expect(::chmod(Root.c_str(), 0700) == 0,
				   "set exact owner-private bootstrap directory permissions");
			made.release();
#endif
		}

		~temporary_directory()
		{
			std::error_code error;
			std::filesystem::remove_all(Root, error);
		}

		[[nodiscard]] auto path() const -> const std::filesystem::path& { return Root; }

	private:
		inline static std::atomic<std::uint64_t> Next = 1U;
		std::filesystem::path                    Root;
	};

	class fake_observer final
		: public application::detail::designated_review_public_observation_port
	{
	public:
		auto observe(const application::review_mutation_target_binding& aExpected)
			-> std::expected<
				void,
				application::detail::designated_review_public_observation_failure> override
		{
			++Calls;
			ObservedExact = aExpected.Hostname.value() == "github.com"
				&& aExpected.Repository.Owner == "MikeMarcin"
				&& aExpected.Repository.Name == "BPR-Test"
				&& aExpected.RepositoryDatabaseId == 1'328'277'631U
				&& aExpected.RepositoryNodeId == "R_kgDOTyvofw"
				&& aExpected.Number == core::pull_request_number{ 2U }
				&& aExpected.PullRequestDatabaseId == 4'650'736'621U
				&& aExpected.PullRequestNodeId == "PR_kwDOTyvof88AAAABFTSb7Q"
				&& aExpected.BaseRepositoryDatabaseId == 1'328'277'631U
				&& core::canonical_hex(aExpected.BaseOid).view()
					== "a939a85a5525994fe9e9cb4c973ad9362ed427c7"
				&& core::canonical_hex(aExpected.Semantic.ExpectedHead).view()
					== "61e8dbcd1c3940f6df0fa720bec5485dc25d4a3a"
				&& aExpected.BaseRef == "codex/rfc0022-fixture-base-20260926"
				&& aExpected.HeadRef == "codex/rfc0022-live-comments-20260926"
				&& aExpected.Open;
			if (BeforeReturn) {
				BeforeReturn();
			}
			return Failure.has_value()
				? std::expected<
					  void,
					  application::detail::designated_review_public_observation_failure>{
					  std::unexpected(*Failure)
				  }
				: std::expected<void, application::detail::designated_review_public_observation_failure>{ };
		}

		std::atomic<std::size_t> Calls         = 0U;
		bool                     ObservedExact = false;
		std::optional<application::detail::designated_review_public_observation_failure>
							  Failure;
		std::function<void()> BeforeReturn;
	};

	struct fixture
	{
		temporary_directory                                      Directory;
		temporary_directory                                      ReplayDirectory;
		application::detail::designated_review_bootstrap_request Request;
		std::string                                              ApprovalId;
		std::string                                              Subject;

		fixture()
		{
			auto replay = application::detail::designated_review_replay_initializer::
				initialize_or_reset(ReplayDirectory.path() / "replay");
			expect(replay.has_value(), "initialize explicit owner-private replay namespace");
			Request = {
				.ApprovalPath       = Directory.path() / "approval.txt",
				.ExecutablePath     = running_executable(),
				.SourceSha256       = std::string(64U, 'a'),
				.BuildConfiguration = "Release",
				.SupervisorSha256   = std::string(64U, 'c'),
				.InventorySha256    = std::string(64U, 'd'),
				.Replay             = std::move(*replay),
				.BuildTesting       = false,
			};
			ApprovalId = std::string(64U, 'b');
			refresh_subject();
			write_approval(Subject);
		}

		void refresh_subject()
		{
			const auto appSha  = hex_digest(read_file(Request.ExecutablePath));
			auto       subject = application::detail::designated_review_approval_subject(
				Request, appSha, ApprovalId);
			expect(subject.has_value(), "render canonical approval subject");
			Subject = std::move(*subject);
		}

		void write_approval(const std::string_view aSubject)
		{
			write(
				Request.ApprovalPath,
				std::string{ aSubject } + "subject_sha256=" + hex_digest(aSubject) + "\n",
				0600);
		}

		static void write(
			const std::filesystem::path& aPath,
			const std::string_view       aBytes,
			const int                    aMode)
		{
			auto output = std::ofstream{ aPath, std::ios::binary | std::ios::trunc };
			expect(output.is_open(), "open bootstrap fixture file");
			output.write(aBytes.data(), static_cast<std::streamsize>(aBytes.size()));
			output.close();
			expect(output.good(), "write bootstrap fixture file");
#if !defined(_WIN32)
			expect(::chmod(aPath.c_str(), static_cast<mode_t>(aMode)) == 0,
				   "set exact bootstrap fixture permissions");
#else
			static_cast<void>(aMode);
#endif
		}
	};

	[[nodiscard]] auto replace_once(
		std::string aText, const std::string_view aExpected, const std::string_view aReplacement)
		-> std::string
	{
		const auto offset = aText.find(aExpected);
		expect(offset != std::string::npos, "approval mismatch field exists");
		aText.replace(offset, aExpected.size(), aReplacement);
		return aText;
	}

	void create_private_directory(const std::filesystem::path& aPath)
	{
		expect(std::filesystem::create_directory(aPath), "create private fixture directory");
#if !defined(_WIN32)
		expect(::chmod(aPath.c_str(), 0700) == 0,
			   "set exact private fixture directory permissions");
#endif
	}

	[[nodiscard]] auto marker_id(const std::size_t aIndex) -> std::string
	{
		return hex_digest("replay-marker-" + std::to_string(aIndex));
	}

	void write_marker(
		const std::filesystem::path& aNamespace,
		const std::string_view       aApprovalId)
	{
		fixture::write(
			aNamespace / std::string{ aApprovalId },
			std::string{ "approval_id=" } + std::string{ aApprovalId } + "\n",
			0600);
	}

	void test_valid_approval_is_observed_consumed_and_replay_refused()
	{
		auto fixture  = ::fixture{ };
		auto observer = fake_observer{ };
		auto granted  = application::detail::designated_review_authority_bootstrap::run(
			fixture.Request, observer);
		expect(granted.has_value() && *granted != nullptr,
			   "one exact owner-private approval mints one opaque carrier");
		expect(observer.Calls == 1U && observer.ObservedExact,
			   "bootstrap observes every compiled public target field");
		expect(!std::filesystem::exists(fixture.Request.ApprovalPath),
			   "successful bootstrap atomically consumes the approval path");
		fixture.write_approval(fixture.Subject);
		auto replay = application::detail::designated_review_authority_bootstrap::run(
			fixture.Request, observer);
		expect(!replay.has_value()
				   && replay.error()
					   == application::detail::designated_review_bootstrap_failure::artifact_replayed
				   && observer.Calls == 1U,
			   "recreating the consumed pathname cannot bypass durable spent state");
	}

	void test_initializer_is_explicit_fresh_and_attended_load_never_creates()
	{
		auto       directory = temporary_directory{ };
		const auto missing   = directory.path() / "fresh-product-data" / "replay";
		auto       loaded    = application::detail::designated_review_replay_initializer::load(
			missing);
		expect(!loaded.has_value()
				   && loaded.error()
					   == application::detail::designated_review_bootstrap_failure::replay_state_absent,
			   "load refuses absent replay state");
		expect(!std::filesystem::exists(missing.parent_path()),
			   "load does not create a missing product-data parent");
		{
			auto attended           = ::fixture{ };
			attended.Request.Replay = {
				.NamespacePath = missing,
				.NamespaceId   = std::string(64U, 'c'),
			};
			attended.refresh_subject();
			attended.write_approval(attended.Subject);
			auto observer = fake_observer{ };
			auto result   = application::detail::designated_review_authority_bootstrap::run(
				attended.Request, observer);
			expect(!result.has_value()
					   && result.error()
						   == application::detail::designated_review_bootstrap_failure::replay_state_absent
					   && observer.Calls == 0U
					   && !std::filesystem::exists(missing.parent_path()),
				   "attended bootstrap refuses missing replay state without creating it");
		}
		auto initialized
			= application::detail::designated_review_replay_initializer::initialize_or_reset(
				missing);
		expect(initialized.has_value() && initialized->NamespacePath == missing
				   && initialized->NamespaceId.size() == 64U,
			   "explicit initialization descriptor-safely creates fresh product data and replay state");
		auto firstLoad = application::detail::designated_review_replay_initializer::load(
			missing);
		expect(firstLoad.has_value() && *firstLoad == *initialized,
			   "load returns the exact schema-bound namespace identity");
		auto reset
			= application::detail::designated_review_replay_initializer::initialize_or_reset(
				missing);
		expect(reset.has_value() && reset->NamespacePath == missing
				   && reset->NamespaceId != initialized->NamespaceId,
			   "explicit reset selects a fresh random namespace identity");
		auto secondLoad = application::detail::designated_review_replay_initializer::load(
			missing);
		expect(secondLoad.has_value() && *secondLoad == *reset,
			   "load observes the reset namespace identity without repairing it");
	}

	void test_global_replay_survives_distinct_roots_and_parent_recreation()
	{
		{
			auto fixture        = ::fixture{ };
			auto secondRoot     = temporary_directory{ };
			auto second         = fixture.Request;
			second.ApprovalPath = secondRoot.path() / "approval-copy.txt";
			fixture::write(
				second.ApprovalPath,
				fixture.Subject + "subject_sha256=" + hex_digest(fixture.Subject) + "\n",
				0600);
			auto firstObserver = fake_observer{ };
			auto first         = application::detail::designated_review_authority_bootstrap::run(
				fixture.Request, firstObserver);
			expect(first.has_value(), "first private approval root spends the global identity");
			auto secondObserver = fake_observer{ };
			auto replay         = application::detail::designated_review_authority_bootstrap::run(
				second, secondObserver);
			expect(!replay.has_value()
					   && replay.error()
						   == application::detail::designated_review_bootstrap_failure::artifact_replayed
					   && secondObserver.Calls == 0U,
				   "identical bytes in a distinct private root cannot mint a second grant");
		}

		{
			auto fixture  = ::fixture{ };
			auto observer = fake_observer{ };
			expect(application::detail::designated_review_authority_bootstrap::run(
					   fixture.Request, observer)
					   .has_value(),
				   "initial artifact parent spend succeeds");
			const auto approvalParent = fixture.Directory.path();
			expect(std::filesystem::remove_all(approvalParent) != 0U,
				   "remove the original artifact parent");
			create_private_directory(approvalParent);
			fixture.write_approval(fixture.Subject);
			auto replay = application::detail::designated_review_authority_bootstrap::run(
				fixture.Request, observer);
			expect(!replay.has_value()
					   && replay.error()
						   == application::detail::designated_review_bootstrap_failure::artifact_replayed
					   && observer.Calls == 1U,
				   "deleting and recreating the approval parent does not erase global replay state");
		}
	}

	void test_reset_invalidates_prior_approval_subjects()
	{
		auto       fixture = ::fixture{ };
		const auto prior   = fixture.Request.Replay;
		auto       reset   = application::detail::designated_review_replay_initializer::
			initialize_or_reset(prior.NamespacePath);
		expect(reset.has_value() && reset->NamespaceId != prior.NamespaceId,
			   "reset advances the namespace identity");
		auto observer = fake_observer{ };
		auto stale    = application::detail::designated_review_authority_bootstrap::run(
			fixture.Request, observer);
		expect(!stale.has_value()
				   && stale.error()
					   == application::detail::designated_review_bootstrap_failure::replay_state_mismatch
				   && observer.Calls == 0U,
			   "approval bound to the prior namespace identity is invalid after reset");
		fixture.Request.Replay = std::move(*reset);
		fixture.refresh_subject();
		fixture.write_approval(fixture.Subject);
		auto fresh = application::detail::designated_review_authority_bootstrap::run(
			fixture.Request, observer);
		expect(fresh.has_value() && observer.Calls == 1U,
			   "a newly bound approval is required after reset");
	}

	void test_absent_unsafe_malformed_and_oversized_approvals_fail_closed()
	{
		{
			auto fixture = ::fixture{ };
			std::filesystem::remove(fixture.Request.ApprovalPath);
			auto observer = fake_observer{ };
			expect(
				!application::detail::designated_review_authority_bootstrap::run(
					 fixture.Request, observer)
						.has_value()
					&& observer.Calls == 0U,
				"an absent approval fails before public observation");
		}
		{
			auto fixture                   = ::fixture{ };
			fixture.Request.ExecutablePath = fixture.Directory.path() / "substitute-app";
			fixture::write(fixture.Request.ExecutablePath, "not-the-running-app\n", 0700);
			auto subject = application::detail::designated_review_approval_subject(
				fixture.Request,
				hex_digest("not-the-running-app\n"),
				fixture.ApprovalId);
			expect(subject.has_value(), "render substitute executable approval");
			fixture.write_approval(*subject);
			auto observer = fake_observer{ };
			auto result   = application::detail::designated_review_authority_bootstrap::run(
				fixture.Request, observer);
			expect(
				!result.has_value()
					&& result.error()
						== application::detail::designated_review_bootstrap_failure::executable_unavailable
					&& observer.Calls == 0U,
				"an approved alternate executable cannot stand in for the running process");
		}
#if !defined(_WIN32)
		{
			auto fixture = ::fixture{ };
			expect(::chmod(fixture.Directory.path().c_str(), 0755) == 0,
				   "make parent deliberately unsafe");
			auto observer = fake_observer{ };
			expect(
				!application::detail::designated_review_authority_bootstrap::run(
					 fixture.Request, observer)
						.has_value()
					&& observer.Calls == 0U,
				"a non-private approval parent fails closed");
		}
		{
			auto fixture = ::fixture{ };
			expect(::chmod(fixture.Request.ApprovalPath.c_str(), 0644) == 0,
				   "make approval deliberately unsafe");
			auto observer = fake_observer{ };
			expect(
				!application::detail::designated_review_authority_bootstrap::run(
					 fixture.Request, observer)
						.has_value()
					&& observer.Calls == 0U,
				"a non-private approval file fails closed");
		}
#endif
		for (const auto& malformed : { std::string{ "not-an-approval\n" },
									   std::string{ "schema_name=x\r\n" },
									   std::string{ "schema_name=x" } }) {
			auto fixture = ::fixture{ };
			fixture::write(fixture.Request.ApprovalPath, malformed, 0600);
			auto observer = fake_observer{ };
			expect(
				!application::detail::designated_review_authority_bootstrap::run(
					 fixture.Request, observer)
						.has_value()
					&& observer.Calls == 0U,
				"malformed approval bytes fail before observation");
		}
		for (const auto& malformedId : {
				 replace_once(fixture{ }.Subject, std::string(64U, 'b'), "UPPERCASE"),
				 fixture{ }.Subject + "approval_id=" + std::string(64U, 'c') + "\n",
			 }) {
			auto fixture = ::fixture{ };
			fixture.write_approval(malformedId);
			auto observer = fake_observer{ };
			expect(
				!application::detail::designated_review_authority_bootstrap::run(
					 fixture.Request, observer)
						.has_value()
					&& observer.Calls == 0U,
				"a malformed or duplicate artifact-owned approval id fails before observation");
		}
		{
			auto fixture = ::fixture{ };
			fixture::write(
				fixture.Request.ApprovalPath,
				fixture.Subject + "subject_sha256=" + std::string(64U, '0') + "\n",
				0600);
			auto observer = fake_observer{ };
			auto result   = application::detail::designated_review_authority_bootstrap::run(
				fixture.Request, observer);
			expect(
				!result.has_value()
					&& result.error()
						== application::detail::designated_review_bootstrap_failure::approval_digest_mismatch
					&& observer.Calls == 0U,
				"a distinct subject digest mismatch fails before observation");
		}
		{
			auto fixture = ::fixture{ };
			fixture::write(fixture.Request.ApprovalPath, std::string(8U * 1024U + 1U, 'x'), 0600);
			auto observer = fake_observer{ };
			expect(
				!application::detail::designated_review_authority_bootstrap::run(
					 fixture.Request, observer)
						.has_value()
					&& observer.Calls == 0U,
				"oversized approval bytes fail before observation");
		}
	}

	void test_every_controlled_approval_ancestor_and_leaf_symlink_is_rejected()
	{
#if !defined(_WIN32)
		for (auto position = std::size_t{ 0U }; position != 4U; ++position) {
			auto       fixture = ::fixture{ };
			auto       paths   = temporary_directory{ };
			const auto real    = paths.path() / "real";
			const auto first   = real / "first";
			const auto second  = first / "second";
			create_private_directory(real);
			create_private_directory(first);
			create_private_directory(second);
			const auto actual = second / "approval.txt";
			fixture::write(
				actual,
				fixture.Subject + "subject_sha256=" + hex_digest(fixture.Subject) + "\n",
				0600);
			auto request = fixture.Request;
			switch (position) {
			case 0U:
				std::filesystem::create_directory_symlink(real, paths.path() / "real-link");
				request.ApprovalPath
					= paths.path() / "real-link" / "first" / "second" / "approval.txt";
				break;
			case 1U:
				std::filesystem::create_directory_symlink(first, real / "first-link");
				request.ApprovalPath
					= real / "first-link" / "second" / "approval.txt";
				break;
			case 2U:
				std::filesystem::create_directory_symlink(second, first / "second-link");
				request.ApprovalPath = first / "second-link" / "approval.txt";
				break;
			case 3U:
				std::filesystem::create_symlink(actual, second / "approval-link.txt");
				request.ApprovalPath = second / "approval-link.txt";
				break;
			default:
				std::abort();
			}
			auto observer = fake_observer{ };
			auto result   = application::detail::designated_review_authority_bootstrap::run(
				request, observer);
			expect(!result.has_value() && observer.Calls == 0U,
				   "each controlled approval ancestor position and leaf refuses symlinks");
		}
#endif
	}

	void test_every_controlled_replay_ancestor_symlink_is_rejected()
	{
#if !defined(_WIN32)
		for (auto position = std::size_t{ 0U }; position != 4U; ++position) {
			auto       fixture = ::fixture{ };
			auto       paths   = temporary_directory{ };
			const auto real    = paths.path() / "real";
			const auto first   = real / "first";
			const auto second  = first / "second";
			create_private_directory(real);
			create_private_directory(first);
			create_private_directory(second);
			const auto actual      = second / "replay";
			auto       initialized = application::detail::designated_review_replay_initializer::
				initialize_or_reset(actual);
			expect(initialized.has_value(), "initialize replay symlink fixture");
			fixture.Request.Replay = *initialized;
			switch (position) {
			case 0U:
				std::filesystem::create_directory_symlink(real, paths.path() / "real-link");
				fixture.Request.Replay.NamespacePath
					= paths.path() / "real-link" / "first" / "second" / "replay";
				break;
			case 1U:
				std::filesystem::create_directory_symlink(first, real / "first-link");
				fixture.Request.Replay.NamespacePath
					= real / "first-link" / "second" / "replay";
				break;
			case 2U:
				std::filesystem::create_directory_symlink(second, first / "second-link");
				fixture.Request.Replay.NamespacePath
					= first / "second-link" / "replay";
				break;
			case 3U:
				std::filesystem::create_directory_symlink(actual, second / "replay-link");
				fixture.Request.Replay.NamespacePath = second / "replay-link";
				break;
			default:
				std::abort();
			}
			fixture.refresh_subject();
			fixture.write_approval(fixture.Subject);
			auto observer = fake_observer{ };
			auto result   = application::detail::designated_review_authority_bootstrap::run(
				fixture.Request, observer);
			expect(!result.has_value()
					   && result.error()
						   == application::detail::designated_review_bootstrap_failure::unsafe_replay_state
					   && observer.Calls == 0U,
				   "each controlled replay ancestor position refuses symlinks");
		}
#endif
	}

	void test_unsafe_replay_permissions_links_metadata_and_substitution_fail_closed()
	{
#if !defined(_WIN32)
		{
			auto fixture = ::fixture{ };
			expect(::chmod(fixture.Request.Replay.NamespacePath.c_str(), 0755) == 0,
				   "make replay namespace permissions unsafe");
			auto observer = fake_observer{ };
			auto result   = application::detail::designated_review_authority_bootstrap::run(
				fixture.Request, observer);
			expect(!result.has_value()
					   && result.error()
						   == application::detail::designated_review_bootstrap_failure::unsafe_replay_state
					   && observer.Calls == 0U,
				   "non-private replay namespace permissions fail before observation");
		}
		{
			auto       fixture = ::fixture{ };
			const auto metadata
				= fixture.Request.Replay.NamespacePath / ".bpr-replay-namespace";
			expect(::chmod(metadata.c_str(), 0644) == 0,
				   "make replay metadata permissions unsafe");
			auto observer = fake_observer{ };
			auto result   = application::detail::designated_review_authority_bootstrap::run(
				fixture.Request, observer);
			expect(!result.has_value()
					   && result.error()
						   == application::detail::designated_review_bootstrap_failure::unsafe_replay_state
					   && observer.Calls == 0U,
				   "unsafe replay metadata permissions fail before observation");
		}
		{
			auto       fixture = ::fixture{ };
			const auto metadata
				= fixture.Request.Replay.NamespacePath / ".bpr-replay-namespace";
			std::filesystem::create_hard_link(
				metadata, fixture.ReplayDirectory.path() / "metadata-hard-link");
			auto observer = fake_observer{ };
			auto result   = application::detail::designated_review_authority_bootstrap::run(
				fixture.Request, observer);
			expect(!result.has_value()
					   && result.error()
						   == application::detail::designated_review_bootstrap_failure::unsafe_replay_state
					   && observer.Calls == 0U,
				   "linked replay metadata fails before observation");
		}
		{
			auto       fixture = ::fixture{ };
			const auto metadata
				= fixture.Request.Replay.NamespacePath / ".bpr-replay-namespace";
			const auto backing = fixture.ReplayDirectory.path() / "metadata-backing";
			std::filesystem::rename(metadata, backing);
			std::filesystem::create_symlink(backing, metadata);
			auto observer = fake_observer{ };
			auto result   = application::detail::designated_review_authority_bootstrap::run(
				fixture.Request, observer);
			expect(!result.has_value()
					   && result.error()
						   == application::detail::designated_review_bootstrap_failure::unsafe_replay_state
					   && observer.Calls == 0U,
				   "symlinked replay metadata fails before observation");
		}
		{
			auto       fixture    = ::fixture{ };
			auto       observer   = fake_observer{ };
			const auto original   = fixture.Request.Replay.NamespacePath;
			const auto moved      = fixture.ReplayDirectory.path() / "replay-retained";
			observer.BeforeReturn = [&] {
				std::filesystem::rename(original, moved);
				create_private_directory(original);
			};
			auto result = application::detail::designated_review_authority_bootstrap::run(
				fixture.Request, observer);
			expect(!result.has_value()
					   && result.error()
						   == application::detail::designated_review_bootstrap_failure::replay_state_substituted
					   && observer.Calls == 1U,
				   "replay-root name-to-inode substitution at the spend barrier fails closed");
		}
#endif
	}

#if defined(__APPLE__)
	void attach_nontrivial_acl(const std::filesystem::path& aPath)
	{
		const auto* const owner = ::getpwuid(::geteuid());
		expect(owner != nullptr && owner->pw_name != nullptr,
			   "resolve owner name for extended ACL fixture");
		auto entry     = std::string{ "user:" } + owner->pw_name + " allow read";
		auto path      = aPath.string();
		auto tool      = std::string{ "/bin/chmod" };
		auto flag      = std::string{ "+a" };
		auto arguments = std::array<char*, 5U>{
			tool.data(), flag.data(), entry.data(), path.data(), nullptr
		};
		auto environment = std::array<char*, 1U>{ nullptr };
		auto process     = pid_t{ };
		expect(::posix_spawn(
				   &process,
				   tool.c_str(),
				   nullptr,
				   nullptr,
				   arguments.data(),
				   environment.data())
				   == 0,
			   "launch fixed native chmod ACL fixture helper");
		auto status = int{ };
		expect(::waitpid(process, &status, 0) == process
				   && WIFEXITED(status) && WEXITSTATUS(status) == 0,
			   "attach nontrivial ACL to fixture");
		auto* const observedAcl   = ::acl_get_file(aPath.c_str(), ACL_TYPE_EXTENDED);
		auto        observedEntry = acl_entry_t{ };
		expect(observedAcl != nullptr
				   && ::acl_get_entry(observedAcl, ACL_FIRST_ENTRY, &observedEntry) == 0,
			   "read the attached nontrivial ACL back from the fixture");
		static_cast<void>(::acl_free(observedAcl));
	}

	void test_nontrivial_macos_acl_authority_is_rejected()
	{
		{
			auto fixture = ::fixture{ };
			attach_nontrivial_acl(fixture.Request.ApprovalPath);
			auto observer = fake_observer{ };
			auto result   = application::detail::designated_review_authority_bootstrap::run(
				fixture.Request, observer);
			expect(!result.has_value()
					   && result.error()
						   == application::detail::designated_review_bootstrap_failure::unsafe_artifact
					   && observer.Calls == 0U,
				   "macOS approval ACL authority mints no carrier");
		}
		{
			auto fixture = ::fixture{ };
			attach_nontrivial_acl(fixture.Request.Replay.NamespacePath);
			auto observer = fake_observer{ };
			auto result   = application::detail::designated_review_authority_bootstrap::run(
				fixture.Request, observer);
			expect(!result.has_value()
					   && result.error()
						   == application::detail::designated_review_bootstrap_failure::unsafe_replay_state
					   && observer.Calls == 0U,
				   "macOS replay-root ACL authority mints no carrier");
		}
	}
#endif

	void test_each_digest_bound_identity_field_mismatch_fails_before_observation()
	{
		const auto variants = std::array{
			// BPR-0352 advanced this to 3. An approval bound to the older
			// subject describes a question that said nothing about the
			// supervisor or the inventory, and must be refused rather than
			// read as though it had answered this one.
			std::pair{ "schema_version=3", "schema_version=2" },
			std::pair{ "application_sha256=", "application_sha256=0" },
			std::pair{ "source_sha256=", "source_sha256=0" },
			std::pair{ "supervisor_sha256=", "supervisor_sha256=0" },
			std::pair{ "inventory_sha256=", "inventory_sha256=0" },
			std::pair{ "build_configuration=Release", "build_configuration=Debug" },
			std::pair{ "build_testing=off", "build_testing=on" },
			std::pair{ "replay_schema=bpr.designated_live_review_replay.v1", "replay_schema=other" },
			std::pair{ "replay_namespace_path=", "replay_namespace_path=/other/" },
			std::pair{ "replay_namespace_id=", "replay_namespace_id=0" },
			std::pair{ "hostname=github.com", "hostname=example.invalid" },
			std::pair{ "server_contract=github.com-2022-11-28", "server_contract=other" },
			std::pair{ "repository_owner=MikeMarcin", "repository_owner=Elsewhere" },
			std::pair{ "repository_name=BPR-Test", "repository_name=Other" },
			std::pair{ "repository_database_id=1328277631", "repository_database_id=2" },
			std::pair{ "repository_node_id=R_kgDOTyvofw", "repository_node_id=R_other" },
			std::pair{ "pull_request_number=2", "pull_request_number=1" },
			std::pair{ "pull_request_database_id=4650736621", "pull_request_database_id=2" },
			std::pair{ "pull_request_node_id=PR_kwDOTyvof88AAAABFTSb7Q", "pull_request_node_id=PR_other" },
			std::pair{ "base_repository_database_id=1328277631", "base_repository_database_id=2" },
			std::pair{ "base_oid=a939a85a5525994fe9e9cb4c973ad9362ed427c7", "base_oid=0000000000000000000000000000000000000000" },
			std::pair{ "base_ref=codex/rfc0022-fixture-base-20260926", "base_ref=other" },
			std::pair{ "head_oid=61e8dbcd1c3940f6df0fa720bec5485dc25d4a3a", "head_oid=0000000000000000000000000000000000000000" },
			std::pair{ "head_ref=codex/rfc0022-live-comments-20260926", "head_ref=other" },
			std::pair{ "state=open", "state=closed" },
			std::pair{ "effect=one-source-line-review-comment", "effect=anything" },
		};
		for (const auto& [expected, replacement] : variants) {
			auto fixture = ::fixture{ };
			fixture.write_approval(replace_once(fixture.Subject, expected, replacement));
			auto observer = fake_observer{ };
			expect(
				!application::detail::designated_review_authority_bootstrap::run(
					 fixture.Request, observer)
						.has_value()
					&& observer.Calls == 0U,
				"a re-digested identity-field mismatch mints no authority");
		}
	}

	void test_public_failures_substitution_and_concurrent_consumption_fail_closed()
	{
		for (const auto failure : {
				 application::detail::designated_review_public_observation_failure::network,
				 application::detail::designated_review_public_observation_failure::timeout,
				 application::detail::designated_review_public_observation_failure::http,
				 application::detail::designated_review_public_observation_failure::redirect,
				 application::detail::designated_review_public_observation_failure::malformed,
				 application::detail::designated_review_public_observation_failure::response_limit,
				 application::detail::designated_review_public_observation_failure::identity_mismatch,
				 application::detail::designated_review_public_observation_failure::unavailable,
			 }) {
			auto fixture     = ::fixture{ };
			auto observer    = fake_observer{ };
			observer.Failure = failure;
			expect(
				!application::detail::designated_review_authority_bootstrap::run(
					 fixture.Request, observer)
						.has_value()
					&& std::filesystem::exists(fixture.Request.ApprovalPath),
				"every bounded public-observation failure preserves no grant");
		}

		{
			auto       fixture    = ::fixture{ };
			auto       observer   = fake_observer{ };
			const auto original   = fixture.Request.ApprovalPath.string() + ".original";
			observer.BeforeReturn = [&fixture, &original] {
				std::filesystem::rename(fixture.Request.ApprovalPath, original);
				fixture.write_approval(fixture.Subject);
			};
			auto result = application::detail::designated_review_authority_bootstrap::run(
				fixture.Request, observer);
			expect(
				!result.has_value()
					&& result.error()
						== application::detail::designated_review_bootstrap_failure::artifact_substituted,
				"path substitution after retained read mints no carrier");
		}

		{
			auto fixture = ::fixture{ };
			class synchronized_observer final
				: public application::detail::designated_review_public_observation_port
			{
			public:
				auto observe(const application::review_mutation_target_binding&)
					-> std::expected<
						void,
						application::detail::designated_review_public_observation_failure> override
				{
					Arrive.arrive_and_wait();
					return { };
				}
				std::barrier<> Arrive{ 2 };
			} observer;
			std::array<bool, 2U> granted{ false, false };
			auto                 first = std::jthread{ [&] {
				granted[0] = application::detail::designated_review_authority_bootstrap::run(
								 fixture.Request, observer)
								 .has_value();
			} };
			auto second = std::jthread{ [&] {
				granted[1] = application::detail::designated_review_authority_bootstrap::run(
								 fixture.Request, observer)
								 .has_value();
			} };
			first.join();
			second.join();
			expect(granted[0] != granted[1],
				   "two concurrent retained readers can atomically consume only once");
		}
	}

	void test_cross_process_spend_is_globally_exclusive()
	{
#if !defined(_WIN32)
		auto fixture        = ::fixture{ };
		auto secondRoot     = temporary_directory{ };
		auto second         = fixture.Request;
		second.ApprovalPath = secondRoot.path() / "approval-copy.txt";
		fixture::write(
			second.ApprovalPath,
			fixture.Subject + "subject_sha256=" + hex_digest(fixture.Subject) + "\n",
			0600);
		auto startPipe  = std::array<int, 2U>{ -1, -1 };
		auto resultPipe = std::array<int, 2U>{ -1, -1 };
		expect(::pipe(startPipe.data()) == 0 && ::pipe(resultPipe.data()) == 0,
			   "create cross-process spend barriers");
		auto children = std::array<pid_t, 2U>{ };
		for (auto index = std::size_t{ 0U }; index != children.size(); ++index) {
			children[index] = ::fork();
			expect(children[index] >= 0, "fork cross-process replay spender");
			if (children[index] == 0) {
				// [[BPR-1121]]. The child never unwinds into this process's frames.
				bpr::tests::end_forked_child([&] {
					static_cast<void>(::close(startPipe[1]));
					static_cast<void>(::close(resultPipe[0]));
					auto signal = char{ };
					if (::read(startPipe[0], &signal, 1U) != 1) {
						return 3;
					}
					auto        observer = fake_observer{ };
					const auto& request  = index == 0U ? fixture.Request : second;
					const auto  granted
						= application::detail::designated_review_authority_bootstrap::run(
							  request, observer)
							  .has_value();
					const auto result = granted ? '1' : '0';
					if (::write(resultPipe[1], &result, 1U) != 1) {
						return 4;
					}
					return 0;
				});
			}
		}
		static_cast<void>(::close(startPipe[0]));
		static_cast<void>(::close(resultPipe[1]));
		const auto release = std::array<char, 2U>{ 'x', 'x' };
		expect(::write(startPipe[1], release.data(), release.size())
				   == static_cast<ssize_t>(release.size()),
			   "release both cross-process spenders");
		static_cast<void>(::close(startPipe[1]));
		auto results   = std::array<char, 2U>{ };
		auto readCount = std::size_t{ 0U };
		while (readCount != results.size()) {
			const auto count = ::read(
				resultPipe[0], results.data() + readCount, results.size() - readCount);
			expect(count > 0, "read cross-process spend result");
			readCount += static_cast<std::size_t>(count);
		}
		static_cast<void>(::close(resultPipe[0]));
		for (const auto child : children) {
			auto status = int{ };
			expect(::waitpid(child, &status, 0) == child
					   && WIFEXITED(status) && WEXITSTATUS(status) == 0,
				   "cross-process spender exits cleanly");
		}
		const auto grants = static_cast<std::size_t>(results[0] == '1')
			+ static_cast<std::size_t>(results[1] == '1');
		expect(grants == 1U,
			   "two honest processes spending identical approval bytes produce exactly one grant");
#endif
	}

	void test_marker_unlink_substitution_and_link_races_fail_closed()
	{
		for (const auto mutation : {
				 application::detail::designated_review_marker_mutation_for_testing::unlink,
				 application::detail::designated_review_marker_mutation_for_testing::substitute,
				 application::detail::designated_review_marker_mutation_for_testing::hard_link,
			 }) {
			auto fixture  = ::fixture{ };
			auto observer = fake_observer{ };
			application::detail::mutate_designated_review_marker_after_sync_for_testing(
				mutation);
			auto result = application::detail::designated_review_authority_bootstrap::run(
				fixture.Request, observer);
			expect(!result.has_value()
					   && result.error()
						   == application::detail::designated_review_bootstrap_failure::replay_state_substituted
					   && observer.Calls == 1U
					   && !std::filesystem::exists(fixture.Request.ApprovalPath),
				   "post-sync marker mutation is detected and consumes the reusable approval");
			fixture.write_approval(fixture.Subject);
			auto retry = application::detail::designated_review_authority_bootstrap::run(
				fixture.Request, observer);
			expect(!retry.has_value() && observer.Calls == 1U,
				   "a detected marker race leaves no reusable grant");
		}
	}

	void test_fixed_marker_capacity_fails_closed_and_explicit_reset_recovers()
	{
		auto fixture = ::fixture{ };
		for (auto index = std::size_t{ 0U };
			 index != application::detail::designated_review_replay_marker_capacity;
			 ++index) {
			write_marker(fixture.Request.Replay.NamespacePath, marker_id(index));
		}
		auto observer = fake_observer{ };
		auto full     = application::detail::designated_review_authority_bootstrap::run(
			fixture.Request, observer);
		expect(!full.has_value()
				   && full.error()
					   == application::detail::designated_review_bootstrap_failure::replay_state_capacity
				   && observer.Calls == 0U,
			   "the fixed 128-marker namespace refuses a new spend before observation");
		auto reset = application::detail::designated_review_replay_initializer::
			initialize_or_reset(fixture.Request.Replay.NamespacePath);
		expect(reset.has_value()
				   && reset->NamespaceId != fixture.Request.Replay.NamespaceId,
			   "explicit reset clears a full bounded namespace with a fresh identity");
		fixture.Request.Replay = std::move(*reset);
		fixture.refresh_subject();
		fixture.write_approval(fixture.Subject);
		auto granted = application::detail::designated_review_authority_bootstrap::run(
			fixture.Request, observer);
		expect(granted.has_value() && observer.Calls == 1U,
			   "fresh approval after explicit capacity reset can spend once");
	}

	void test_failed_consumption_keeps_the_durable_marker_authoritative()
	{
		auto       fixture    = ::fixture{ };
		const auto quarantine = fixture.Directory.path()
			/ ".approval.txt.bpr-consumed";
		fixture::write(quarantine, "occupied\n", 0600);
		auto observer = fake_observer{ };
		auto failed   = application::detail::designated_review_authority_bootstrap::run(
			fixture.Request, observer);
		expect(!failed.has_value()
				   && failed.error()
					   == application::detail::designated_review_bootstrap_failure::consume_failure
				   && observer.Calls == 1U
				   && std::filesystem::exists(fixture.Request.ApprovalPath),
			   "a post-marker consumption failure grants nothing and leaves the artifact visible");
		auto replay = application::detail::designated_review_authority_bootstrap::run(
			fixture.Request, observer);
		expect(!replay.has_value()
				   && replay.error()
					   == application::detail::designated_review_bootstrap_failure::artifact_replayed
				   && observer.Calls == 1U,
			   "the durable marker blocks reuse after consumption failure");
	}

	void test_every_durability_sync_failure_is_typed_and_fail_closed()
	{
		for (const auto& [call, expected] : {
				 std::pair{ 1U, application::detail::designated_review_bootstrap_failure::file_sync_failure },
				 std::pair{ 2U, application::detail::designated_review_bootstrap_failure::directory_sync_failure },
				 std::pair{ 3U, application::detail::designated_review_bootstrap_failure::directory_sync_failure },
			 }) {
			auto fixture  = ::fixture{ };
			auto observer = fake_observer{ };
			application::detail::fail_designated_review_fsync_after_for_testing(call);
			auto result = application::detail::designated_review_authority_bootstrap::run(
				fixture.Request, observer);
			application::detail::fail_designated_review_fsync_after_for_testing(0U);
			expect(!result.has_value() && result.error() == expected,
				   "each marker/consumption fsync boundary returns its exact typed failure");
			expect(!std::filesystem::exists(fixture.Request.ApprovalPath),
				   "a detected post-marker durability failure consumes the reusable artifact");
			fixture.write_approval(fixture.Subject);
			auto retry = application::detail::designated_review_authority_bootstrap::run(
				fixture.Request, observer);
			expect(!retry.has_value()
					   && retry.error()
						   == application::detail::designated_review_bootstrap_failure::artifact_replayed
					   && observer.Calls == 1U,
				   "the surviving failure marker refuses immediate approval reuse");
		}
	}

	void test_initializer_durability_failures_are_typed_and_load_never_repairs()
	{
		for (const auto& [call, expected] : {
				 std::pair{ 1U, application::detail::designated_review_bootstrap_failure::directory_sync_failure },
				 std::pair{ 2U, application::detail::designated_review_bootstrap_failure::file_sync_failure },
				 std::pair{ 3U, application::detail::designated_review_bootstrap_failure::directory_sync_failure },
			 }) {
			auto       directory = temporary_directory{ };
			const auto path      = directory.path() / "replay";
			application::detail::fail_designated_review_fsync_after_for_testing(call);
			auto initialized = application::detail::designated_review_replay_initializer::
				initialize_or_reset(path);
			application::detail::fail_designated_review_fsync_after_for_testing(0U);
			expect(!initialized.has_value() && initialized.error() == expected,
				   "each initializer durability barrier returns its exact typed failure");
			static_cast<void>(application::detail::designated_review_replay_initializer::load(
				path));
			auto retry = application::detail::designated_review_replay_initializer::
				initialize_or_reset(path);
			expect(retry.has_value(),
				   "only another explicit initializer invocation repairs interrupted initialization");
		}
	}
} // namespace

auto main() -> int
try {
#if defined(_WIN32)
	std::cout << "designated-review-authority-bootstrap-tests:platform-deferred\n";
#else
	test_valid_approval_is_observed_consumed_and_replay_refused();
	test_initializer_is_explicit_fresh_and_attended_load_never_creates();
	test_global_replay_survives_distinct_roots_and_parent_recreation();
	test_reset_invalidates_prior_approval_subjects();
	test_absent_unsafe_malformed_and_oversized_approvals_fail_closed();
	test_every_controlled_approval_ancestor_and_leaf_symlink_is_rejected();
	test_every_controlled_replay_ancestor_symlink_is_rejected();
	test_unsafe_replay_permissions_links_metadata_and_substitution_fail_closed();
	#if defined(__APPLE__)
	test_nontrivial_macos_acl_authority_is_rejected();
	#endif
	test_each_digest_bound_identity_field_mismatch_fails_before_observation();
	test_public_failures_substitution_and_concurrent_consumption_fail_closed();
	test_cross_process_spend_is_globally_exclusive();
	test_marker_unlink_substitution_and_link_races_fail_closed();
	test_fixed_marker_capacity_fails_closed_and_explicit_reset_recovers();
	test_failed_consumption_keeps_the_durable_marker_authoritative();
	test_every_durability_sync_failure_is_typed_and_fail_closed();
	test_initializer_durability_failures_are_typed_and_load_never_repairs();
	std::cout << "designated-review-authority-bootstrap-tests:pass\n";
#endif
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
