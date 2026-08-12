#include "bpr/application/review_mutation_application_host.hpp"
#include "review_mutation_execution_authority.hpp"

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

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto hex_digest(const std::string_view aValue) -> std::string
	{
		constexpr auto digits = std::string_view{ "0123456789abcdef" };
		const auto     digest = core::sha256(std::span{
			reinterpret_cast<const std::uint8_t*>(aValue.data()), aValue.size() });
		auto           result = std::string{ };
		for (const auto byte : digest) {
			result.push_back(digits[(byte >> 4U) & 0x0fU]);
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
#else
			Root = std::filesystem::temp_directory_path()
				/ ("bpr-attended-bootstrap-" + std::to_string(::getpid()) + "-"
				   + std::to_string(Next.fetch_add(1U)));
			expect(std::filesystem::create_directory(Root),
				   "create owner-private bootstrap directory");
			expect(::chmod(Root.c_str(), 0700) == 0,
				   "set exact owner-private bootstrap directory permissions");
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
				&& aExpected.Number == core::pull_request_number{ 1U }
				&& aExpected.PullRequestDatabaseId == 4'246'446'933U
				&& aExpected.PullRequestNodeId == "PR_kwDOTyvof879G6NV"
				&& aExpected.BaseRepositoryDatabaseId == 1'328'277'631U
				&& core::canonical_hex(aExpected.BaseOid).view()
					== "e2a44aa8fef704de7373463222b1872e796918c6"
				&& core::canonical_hex(aExpected.Semantic.ExpectedHead).view()
					== "17990ba0c2fb50338141f35c0717f0f99639c7ba"
				&& aExpected.BaseRef == "main"
				&& aExpected.HeadRef == "codex/disposable-live-review-20260810"
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
		application::detail::designated_review_bootstrap_request Request;
		std::string                                              ApprovalId;
		std::string                                              Subject;

		fixture()
		{
			Request = {
				.ApprovalPath       = Directory.path() / "approval.txt",
				.ExecutablePath     = running_executable(),
				.SourceSha256       = std::string(64U, 'a'),
				.BuildConfiguration = "Release",
				.BuildTesting       = false,
			};
			ApprovalId         = std::string(64U, 'b');
			const auto appSha  = hex_digest(read_file(Request.ExecutablePath));
			auto       subject = application::detail::designated_review_approval_subject(
				Request, appSha, ApprovalId);
			expect(subject.has_value(), "render canonical approval subject");
			Subject = std::move(*subject);
			write_approval(Subject);
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

#if defined(__APPLE__)
	void test_nontrivial_macos_acl_authority_is_rejected()
	{
		auto              fixture = ::fixture{ };
		const auto* const owner   = ::getpwuid(::geteuid());
		expect(owner != nullptr && owner->pw_name != nullptr,
			   "resolve owner name for extended ACL fixture");
		auto entry     = std::string{ "user:" } + owner->pw_name + " allow read";
		auto path      = fixture.Request.ApprovalPath.string();
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
			   "attach nontrivial ACL to approval fixture");
		auto* const observedAcl = ::acl_get_file(
			fixture.Request.ApprovalPath.c_str(), ACL_TYPE_EXTENDED);
		auto observedEntry = acl_entry_t{ };
		expect(observedAcl != nullptr
				   && ::acl_get_entry(observedAcl, ACL_FIRST_ENTRY, &observedEntry) == 0,
			   "read the attached nontrivial ACL back from the fixture");
		static_cast<void>(::acl_free(observedAcl));
		auto observer = fake_observer{ };
		auto result   = application::detail::designated_review_authority_bootstrap::run(
			fixture.Request, observer);
		expect(!result.has_value(),
			   "macOS extended ACL authority mints no carrier");
		expect(result.error()
				   == application::detail::designated_review_bootstrap_failure::unsafe_artifact,
			   "macOS extended ACL returns unsafe-artifact failure");
		expect(observer.Calls == 0U,
			   "macOS extended ACL is rejected before observation");
	}
#endif

	void test_each_digest_bound_identity_field_mismatch_fails_before_observation()
	{
		const auto variants = std::array{
			std::pair{ "application_sha256=", "application_sha256=0" },
			std::pair{ "source_sha256=", "source_sha256=0" },
			std::pair{ "build_configuration=Release", "build_configuration=Debug" },
			std::pair{ "build_testing=off", "build_testing=on" },
			std::pair{ "hostname=github.com", "hostname=example.invalid" },
			std::pair{ "server_contract=github.com-2022-11-28", "server_contract=other" },
			std::pair{ "repository_owner=MikeMarcin", "repository_owner=Elsewhere" },
			std::pair{ "repository_name=BPR-Test", "repository_name=Other" },
			std::pair{ "repository_database_id=1328277631", "repository_database_id=2" },
			std::pair{ "repository_node_id=R_kgDOTyvofw", "repository_node_id=R_other" },
			std::pair{ "pull_request_number=1", "pull_request_number=2" },
			std::pair{ "pull_request_database_id=4246446933", "pull_request_database_id=2" },
			std::pair{ "pull_request_node_id=PR_kwDOTyvof879G6NV", "pull_request_node_id=PR_other" },
			std::pair{ "base_repository_database_id=1328277631", "base_repository_database_id=2" },
			std::pair{ "base_oid=e2a44aa8fef704de7373463222b1872e796918c6", "base_oid=0000000000000000000000000000000000000000" },
			std::pair{ "base_ref=main", "base_ref=other" },
			std::pair{ "head_oid=17990ba0c2fb50338141f35c0717f0f99639c7ba", "head_oid=0000000000000000000000000000000000000000" },
			std::pair{ "head_ref=codex/disposable-live-review-20260810", "head_ref=other" },
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

	void test_every_durability_sync_failure_is_typed_and_fail_closed()
	{
		for (const auto& [call, expected] : {
				 std::pair{ 1U, application::detail::designated_review_bootstrap_failure::directory_sync_failure },
				 std::pair{ 2U, application::detail::designated_review_bootstrap_failure::file_sync_failure },
				 std::pair{ 3U, application::detail::designated_review_bootstrap_failure::directory_sync_failure },
				 std::pair{ 4U, application::detail::designated_review_bootstrap_failure::directory_sync_failure },
			 }) {
			auto fixture  = ::fixture{ };
			auto observer = fake_observer{ };
			application::detail::fail_designated_review_fsync_after_for_testing(call);
			auto result = application::detail::designated_review_authority_bootstrap::run(
				fixture.Request, observer);
			application::detail::fail_designated_review_fsync_after_for_testing(0U);
			expect(!result.has_value() && result.error() == expected,
				   "each approval/spent-state fsync boundary returns its exact typed failure");
		}
	}
} // namespace

auto main() -> int
{
#if defined(_WIN32)
	std::cout << "designated-review-authority-bootstrap-tests:platform-deferred\n";
#else
	test_valid_approval_is_observed_consumed_and_replay_refused();
	test_absent_unsafe_malformed_and_oversized_approvals_fail_closed();
	#if defined(__APPLE__)
	test_nontrivial_macos_acl_authority_is_rejected();
	#endif
	test_each_digest_bound_identity_field_mismatch_fails_before_observation();
	test_public_failures_substitution_and_concurrent_consumption_fail_closed();
	test_every_durability_sync_failure_is_typed_and_fail_closed();
	std::cout << "designated-review-authority-bootstrap-tests:pass\n";
#endif
	return EXIT_SUCCESS;
}
