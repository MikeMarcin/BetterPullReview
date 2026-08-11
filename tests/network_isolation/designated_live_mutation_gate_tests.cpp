#include "designated_live_mutation_gate.hpp"

#include <array>
#include <atomic>
#include <filesystem>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if !defined(_WIN32)
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <unistd.h>
#endif

namespace bpr::tests::network_isolation {
	namespace {

		constexpr std::string_view ValidNonce               = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
		constexpr std::string_view ValidCanarySurfaceSha256 = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";

		class fake_approval final : public approval_artifact_port
		{
		public:
			std::string               Body{ approval_body_for(designated_identity(), ValidCanarySurfaceSha256, ValidNonce) };
			approval_read_status      ReadStatus{ approval_read_status::ready };
			approval_consume_status   ConsumeStatus{ approval_consume_status::consumed };
			std::atomic<unsigned>     InspectCount{ };
			std::atomic<unsigned>     ConsumeCount{ };
			std::vector<std::string>* Trace{ };

			[[nodiscard]] approval_read_result inspect() override
			{
				++InspectCount;
				if (Trace != nullptr) {
					Trace->emplace_back("inspect");
				}
				std::scoped_lock lock(Mutex);
				if (!Available) {
					return { .Status = approval_read_status::missing };
				}
				return {
					.Status   = ReadStatus,
					.Snapshot = approval_snapshot{
						.Body   = Body,
						.Path   = "/approval",
						.Device = 7,
						.Inode  = 11,
					},
				};
			}

			[[nodiscard]] approval_consume_status consume(
				const approval_snapshot& aSnapshot) override
			{
				++ConsumeCount;
				if (Trace != nullptr) {
					Trace->emplace_back("consume");
				}
				std::scoped_lock lock(Mutex);
				if (!Available) {
					return approval_consume_status::missing;
				}
				if (aSnapshot.Device != 7 || aSnapshot.Inode != 11) {
					return approval_consume_status::substituted;
				}
				if (ConsumeStatus == approval_consume_status::consumed) {
					Available = false;
				}
				return ConsumeStatus;
			}

		private:
			std::mutex Mutex;
			bool       Available{ true };
		};

		class fake_remote final : public remote_identity_port
		{
		public:
			remote_read_result Result{
				.Status   = remote_read_status::ready,
				.Identity = designated_identity(),
			};
			std::atomic<unsigned>     ReadCount{ };
			std::vector<std::string>* Trace{ };

			[[nodiscard]] remote_read_result read() override
			{
				++ReadCount;
				if (Trace != nullptr) {
					Trace->emplace_back("remote");
				}
				return Result;
			}
		};

		class fake_child final : public capability_child_port
		{
		public:
			child_start_result        Result{ .Status = child_run_status::succeeded };
			std::atomic<unsigned>     StartCount{ };
			std::string               Capability;
			std::string               ExpectedCanarySurfaceSha256;
			std::vector<std::string>* Trace{ };
			std::function<void()>     DuringStart;

			[[nodiscard]] child_start_result start(
				std::string_view aCapability,
				std::string_view aExpectedCanarySurfaceSha256) override
			{
				++StartCount;
				std::scoped_lock lock(Mutex);
				Capability                  = std::string(aCapability);
				ExpectedCanarySurfaceSha256 = std::string(aExpectedCanarySurfaceSha256);
				if (Trace != nullptr) {
					Trace->emplace_back("child");
				}
				if (DuringStart) {
					DuringStart();
				}
				return Result;
			}

		private:
			std::mutex Mutex;
		};

		unsigned Failures{ };

		void expect(bool aCondition, std::string_view aMessage)
		{
			if (!aCondition) {
				++Failures;
				std::cerr << "FAILED: " << aMessage << '\n';
			}
		}

		[[nodiscard]] live_gate_result run_gate(
			fake_approval& aApproval,
			fake_remote&   aRemote,
			fake_child&    aChild)
		{
			return designated_live_gate{ }.run(aApproval, aRemote, aChild);
		}

		void expect_no_child(
			live_gate_status aExpected,
			fake_approval&   aApproval,
			fake_remote&     aRemote,
			fake_child&      aChild,
			std::string_view aContext)
		{
			const auto result = run_gate(aApproval, aRemote, aChild);
			expect(result.Status == aExpected, aContext);
			expect(result.ChildLaunchCount == 0, "refusal reports zero child launches");
			expect(aChild.StartCount == 0, "refusal does not call child port");
		}

		void test_exact_positive()
		{
			fake_approval            approval;
			fake_remote              remote;
			fake_child               child;
			std::vector<std::string> trace;
			approval.Trace    = &trace;
			remote.Trace      = &trace;
			child.Trace       = &trace;
			const auto result = run_gate(approval, remote, child);
			expect(result.Status == live_gate_status::launched, "exact identity launches");
			expect(result.ChildLaunchCount == 1, "exact identity reports one child");
			expect(approval.InspectCount == 1, "exact identity reads approval once");
			expect(remote.ReadCount == 1, "exact identity reads remote once");
			expect(approval.ConsumeCount == 1, "exact identity consumes approval once");
			expect(child.StartCount == 1, "exact identity starts child once");
			expect(child.Capability == capability_for(ValidNonce), "child receives only exact versioned nonce capability");
			expect(child.ExpectedCanarySurfaceSha256 == ValidCanarySurfaceSha256, "child is bound to exact approval surface identity");
			expect(trace == std::vector<std::string>{ "inspect", "remote", "consume", "child" }, "positive trace is inspect -> remote -> consume -> child");
		}

		void test_exact_runner_arguments()
		{
			expect(exact_runner_arguments({ "runner", "--approval", "/private/approval" }), "fixed runner command is accepted");
			expect(!exact_runner_arguments({ "runner", "--approval", "/private/approval", "--", "/arbitrary/child" }), "caller-selected child argv is rejected");
			expect(!exact_runner_arguments({ "runner", "--approval" }), "missing approval argv is rejected");
		}

		void test_missing_and_malformed_approval()
		{
			{
				fake_approval approval;
				fake_remote   remote;
				fake_child    child;
				approval.ReadStatus = approval_read_status::missing;
				expect_no_child(live_gate_status::approval_missing, approval, remote, child, "missing approval refuses");
				expect(remote.ReadCount == 0, "missing approval refuses before network");
			}
			for (const std::string& body : {
					 std::string{ },
					 std::string("bpr.designated-live-approval/v0\n"),
					 approval_body_for(designated_identity(), ValidCanarySurfaceSha256, "not-a-nonce"),
					 approval_body_for(designated_identity(), "NOT-A-LOWERCASE-DIGEST", ValidNonce),
					 approval_body_for(designated_identity(), ValidCanarySurfaceSha256, ValidNonce) + "extra=true\n",
				 }) {
				fake_approval approval;
				fake_remote   remote;
				fake_child    child;
				approval.Body = body;
				expect_no_child(live_gate_status::approval_malformed, approval, remote, child, "malformed approval refuses");
				expect(remote.ReadCount == 0, "malformed approval refuses before network");
			}
		}

		using identity_mutation = std::pair<identity_field, std::function<void(designated_live_identity&)>>;

		[[nodiscard]] std::vector<identity_mutation> every_identity_mutation()
		{
			return {
				{ identity_field::host, [](auto& aValue) { aValue.Host             = "ghe.example.test"; } },
				{ identity_field::owner, [](auto& aValue) { aValue.Owner           = "OtherOwner"; } },
				{ identity_field::repository, [](auto& aValue) { aValue.Repository = "OtherRepository"; } },
				{ identity_field::repository_database_id, [](auto& aValue) { ++aValue.RepositoryDatabaseId; } },
				{ identity_field::repository_node_id, [](auto& aValue) { aValue.RepositoryNodeId += "x"; } },
				{ identity_field::pull_request_number, [](auto& aValue) { ++aValue.PullRequestNumber; } },
				{ identity_field::pull_request_database_id, [](auto& aValue) { ++aValue.PullRequestDatabaseId; } },
				{ identity_field::pull_request_node_id, [](auto& aValue) { aValue.PullRequestNodeId += "x"; } },
				{ identity_field::base_repository_database_id, [](auto& aValue) { ++aValue.BaseRepositoryDatabaseId; } },
				{ identity_field::state, [](auto& aValue) { aValue.State         = "closed"; } },
				{ identity_field::head_sha, [](auto& aValue) { aValue.HeadSha[0] = 'f'; } },
				{ identity_field::base_sha, [](auto& aValue) { aValue.BaseSha[0] = 'f'; } },
				{ identity_field::head_branch, [](auto& aValue) { aValue.HeadBranch += "-moved"; } },
			};
		}

		void test_every_approval_identity_mismatch()
		{
			for (const auto& [field, mutate] : every_identity_mutation()) {
				fake_approval approval;
				fake_remote   remote;
				fake_child    child;
				auto          identity = designated_identity();
				mutate(identity);
				approval.Body     = approval_body_for(identity, ValidCanarySurfaceSha256, ValidNonce);
				const auto result = run_gate(approval, remote, child);
				expect(result.Status == live_gate_status::approval_identity_mismatch, "approval tuple mismatch refuses");
				expect(result.Mismatch == field, "approval mismatch names exact field");
				expect(remote.ReadCount == 0, "approval tuple mismatch refuses before network");
				expect(child.StartCount == 0, "approval tuple mismatch creates no child");
			}
		}

		void test_every_remote_identity_mismatch()
		{
			for (const auto& [field, mutate] : every_identity_mutation()) {
				fake_approval approval;
				fake_remote   remote;
				fake_child    child;
				mutate(remote.Result.Identity);
				const auto result = run_gate(approval, remote, child);
				expect(result.Status == live_gate_status::remote_identity_mismatch, "remote tuple mismatch refuses");
				expect(result.Mismatch == field, "remote mismatch names exact field");
				expect(approval.ConsumeCount == 0, "remote tuple mismatch does not consume approval");
				expect(child.StartCount == 0, "remote tuple mismatch creates no child");
			}
		}

		void test_remote_failures()
		{
			constexpr std::array cases{
				std::pair{ remote_read_status::network_failure, live_gate_status::remote_network_failure },
				std::pair{ remote_read_status::timeout, live_gate_status::remote_timeout },
				std::pair{ remote_read_status::http_failure, live_gate_status::remote_http_failure },
				std::pair{ remote_read_status::malformed, live_gate_status::remote_malformed },
				std::pair{ remote_read_status::oversize, live_gate_status::remote_oversize },
			};
			for (const auto& [remoteStatus, gateStatus] : cases) {
				fake_approval approval;
				fake_remote   remote;
				fake_child    child;
				remote.Result.Status = remoteStatus;
				expect_no_child(gateStatus, approval, remote, child, "typed remote failure refuses");
				expect(approval.ConsumeCount == 0, "remote failure does not consume approval");
			}
		}

		void test_consume_failures()
		{
			constexpr std::array cases{
				std::pair{ approval_consume_status::missing, live_gate_status::approval_consume_missing },
				std::pair{ approval_consume_status::substituted, live_gate_status::approval_substituted },
				std::pair{ approval_consume_status::failed, live_gate_status::approval_consume_failed },
			};
			for (const auto& [consumeStatus, gateStatus] : cases) {
				fake_approval approval;
				fake_remote   remote;
				fake_child    child;
				approval.ConsumeStatus = consumeStatus;
				expect_no_child(gateStatus, approval, remote, child, "consume failure refuses");
			}
		}

		void test_concurrent_and_repeated_use()
		{
			fake_approval                   approval;
			fake_remote                     remote;
			fake_child                      child;
			std::array<live_gate_result, 2> results;
			std::array<std::thread, 2>      threads{
				std::thread([&]() { results[0] = run_gate(approval, remote, child); }),
				std::thread([&]() { results[1] = run_gate(approval, remote, child); }),
			};
			for (auto& thread : threads) {
				thread.join();
			}
			expect(child.StartCount == 1, "concurrent approval use starts at most one child");
			expect((results[0].Status == live_gate_status::launched) != (results[1].Status == live_gate_status::launched), "exactly one concurrent gate launches");

			const auto repeated = run_gate(approval, remote, child);
			expect(repeated.Status == live_gate_status::approval_missing, "repeated consumed approval refuses");
			expect(child.StartCount == 1, "repeat starts no second child");
		}

		void test_child_failures()
		{
			{
				fake_approval approval;
				fake_remote   remote;
				fake_child    child;
				child.Result      = { };
				const auto result = run_gate(approval, remote, child);
				expect(result.Status == live_gate_status::child_start_failed, "child start failure is typed");
				expect(result.ChildLaunchCount == 0, "failed start reports zero launched children");
			}
			{
				fake_approval approval;
				fake_remote   remote;
				fake_child    child;
				child.Result      = { .Status = child_run_status::failed };
				const auto result = run_gate(approval, remote, child);
				expect(result.Status == live_gate_status::child_failed, "child nonzero exit is typed");
				expect(result.ChildLaunchCount == 1, "started failing child is counted");
			}
			{
				fake_approval approval;
				fake_remote   remote;
				fake_child    child;
				child.Result      = { .Status = child_run_status::timed_out };
				const auto result = run_gate(approval, remote, child);
				expect(result.Status == live_gate_status::child_timed_out, "child timeout is typed");
				expect(result.ChildLaunchCount == 1, "timed-out child is counted");
			}
		}

#if !defined(_WIN32)
		[[nodiscard]] std::filesystem::path make_temporary_directory()
		{
			std::array<char, 128>      pattern{ };
			constexpr std::string_view prefix = "/tmp/bpr-designated-live-gate.XXXXXX";
			std::copy(prefix.begin(), prefix.end(), pattern.begin());
			char* created = mkdtemp(pattern.data());
			return created == nullptr ? std::filesystem::path{ } : std::filesystem::path(created);
		}

		void write_private_file(const std::filesystem::path& aPath, std::string_view aBody)
		{
			const int descriptor = open(aPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IWUSR);
			expect(descriptor >= 0, "private approval file opens");
			if (descriptor < 0) {
				return;
			}
			const auto count = write(descriptor, aBody.data(), aBody.size());
			expect(count == static_cast<ssize_t>(aBody.size()), "private approval file writes completely");
			close(descriptor);
		}

		void unlink_if_present(const std::filesystem::path& aPath)
		{
			if (unlink(aPath.c_str()) != 0 && errno != ENOENT) {
				expect(false, "exact temporary file cleanup succeeds");
			}
		}

		class replace_after_identity_check final : public approval_consume_hook
		{
		public:
			replace_after_identity_check(
				std::filesystem::path aApproval,
				std::filesystem::path aDisplaced)
				: Approval(std::move(aApproval))
				, Displaced(std::move(aDisplaced))
			{
			}

			void after_current_identity_check() override
			{
				expect(rename(Approval.c_str(), Displaced.c_str()) == 0, "race hook displaces approved inode");
				write_private_file(Approval, approval_body_for(designated_identity(), ValidCanarySurfaceSha256, ValidNonce));
			}

		private:
			std::filesystem::path Approval;
			std::filesystem::path Displaced;
		};

		void test_native_approval_constraints()
		{
			const auto directory = make_temporary_directory();
			expect(!directory.empty(), "native approval temporary directory exists");
			if (directory.empty()) {
				return;
			}
			const auto path = directory / "approval";
			write_private_file(path, approval_body_for(designated_identity(), ValidCanarySurfaceSha256, ValidNonce));
			native_approval_artifact valid(path);
			{
				const auto inspected = valid.inspect();
				expect(inspected.Status == approval_read_status::ready, "owner-private regular artifact is accepted");
				expect(valid.consume(inspected.Snapshot) == approval_consume_status::consumed, "exact artifact is atomically consumed");
			}
			expect(valid.inspect().Status == approval_read_status::missing, "consumed artifact cannot be reused");

			write_private_file(path, approval_body_for(designated_identity(), ValidCanarySurfaceSha256, ValidNonce));
			chmod(path.c_str(), S_IRUSR | S_IWUSR | S_IRGRP);
			expect(valid.inspect().Status == approval_read_status::unsafe, "group-readable artifact is refused");
			unlink_if_present(path);

			write_private_file(path, approval_body_for(designated_identity(), ValidCanarySurfaceSha256, ValidNonce));
			expect(chmod(directory.c_str(), S_IRWXU | S_IRGRP | S_IXGRP) == 0, "test makes approval parent non-private");
			expect(valid.inspect().Status == approval_read_status::unsafe, "non-private approval parent is refused");
			expect(chmod(directory.c_str(), S_IRWXU) == 0, "test restores private approval parent");
			unlink_if_present(path);

			write_private_file(path, approval_body_for(designated_identity(), ValidCanarySurfaceSha256, ValidNonce));
			const auto beforeSubstitution = valid.inspect();
			const auto displaced          = directory / "displaced";
			std::filesystem::rename(path, displaced);
			write_private_file(path, approval_body_for(designated_identity(), ValidCanarySurfaceSha256, ValidNonce));
			expect(valid.consume(beforeSubstitution.Snapshot) == approval_consume_status::substituted, "artifact substitution is refused before child creation");
			expect(std::filesystem::exists(path), "substituted artifact is not mistaken for the approved inode");
			unlink_if_present(path);
			unlink_if_present(displaced);

			write_private_file(path, approval_body_for(designated_identity(), ValidCanarySurfaceSha256, ValidNonce));
			const auto                   preciseDisplaced = directory / "precise-displaced";
			replace_after_identity_check hook(path, preciseDisplaced);
			native_approval_artifact     raced(path, &hook);
			fake_remote                  remote;
			fake_child                   child;
			const auto                   raceResult = designated_live_gate{ }.run(raced, remote, child);
			expect(raceResult.Status == live_gate_status::approval_substituted, "substitution between identity check and namespace removal is typed");
			expect(raceResult.ChildLaunchCount == 0 && child.StartCount == 0, "precise substitution creates no child");
			expect(std::filesystem::exists(preciseDisplaced), "precise substitution does not consume approved inode");
			unlink_if_present(path);
			unlink_if_present(preciseDisplaced);

			write_private_file(path, approval_body_for(designated_identity(), ValidCanarySurfaceSha256, ValidNonce));
			native_approval_artifact        concurrent(path);
			fake_remote                     concurrentRemote;
			fake_child                      concurrentChild;
			std::array<live_gate_result, 2> concurrentResults;
			std::array<std::thread, 2>      nativeThreads{
				std::thread([&]() { concurrentResults[0] = designated_live_gate{ }.run(concurrent, concurrentRemote, concurrentChild); }),
				std::thread([&]() { concurrentResults[1] = designated_live_gate{ }.run(concurrent, concurrentRemote, concurrentChild); }),
			};
			for (auto& thread : nativeThreads) {
				thread.join();
			}
			expect(concurrentChild.StartCount == 1, "native concurrent approval use starts exactly one child");
			expect((concurrentResults[0].Status == live_gate_status::launched) != (concurrentResults[1].Status == live_gate_status::launched), "native concurrent approval has exactly one launch result");
			expect(designated_live_gate{ }.run(concurrent, concurrentRemote, concurrentChild).Status == live_gate_status::approval_missing, "native consumed approval refuses repeat");

			write_private_file(path, approval_body_for(designated_identity(), ValidCanarySurfaceSha256, ValidNonce));
			native_approval_artifact retained(path);
			fake_remote              retainedRemote;
			fake_child               retainedChild;
			const auto               claimDirectory = directory / ".bpr-designated-live-approval-claim-v1";
			retainedChild.DuringStart               = [&]() {
				expect(std::filesystem::is_directory(claimDirectory), "approval claim directory remains retained during child execution");
				expect(!std::filesystem::exists(path), "consumed approval namespace is absent during child execution");
			};
			expect(designated_live_gate{ }.run(retained, retainedRemote, retainedChild).Status == live_gate_status::launched, "native retained-authority gate launches once");
			expect(!std::filesystem::exists(claimDirectory), "approval claim authority releases after child termination");

			const auto target = directory / "target";
			write_private_file(target, approval_body_for(designated_identity(), ValidCanarySurfaceSha256, ValidNonce));
			const auto link = directory / "approval-link";
			std::filesystem::create_symlink(target, link);
			native_approval_artifact symlink(link);
			expect(symlink.inspect().Status == approval_read_status::unsafe, "symlink artifact is refused");

			const auto large = directory / "large";
			write_private_file(large, std::string(4097, 'a'));
			native_approval_artifact oversized(large);
			expect(oversized.inspect().Status == approval_read_status::oversize, "oversized artifact is refused");

			unlink_if_present(link);
			unlink_if_present(target);
			unlink_if_present(large);
			expect(rmdir(directory.c_str()) == 0, "exact temporary directory cleanup succeeds");
		}
#else
		void test_native_approval_constraints()
		{
			native_approval_artifact unsupported("C:/approval");
			expect(unsupported.inspect().Status == approval_read_status::unsupported_platform, "Windows approval validation remains fail closed pending target-native work");
		}
#endif

	} // namespace
} // namespace bpr::tests::network_isolation

int main()
{
	using namespace bpr::tests::network_isolation;
	test_exact_positive();
	test_exact_runner_arguments();
	test_missing_and_malformed_approval();
	test_every_approval_identity_mismatch();
	test_every_remote_identity_mismatch();
	test_remote_failures();
	test_consume_failures();
	test_concurrent_and_repeated_use();
	test_child_failures();
	test_native_approval_constraints();
	if (Failures != 0) {
		return 1;
	}
	std::cout << "DESIGNATED_LIVE_GATE_TESTS_OK\n";
	return 0;
}
