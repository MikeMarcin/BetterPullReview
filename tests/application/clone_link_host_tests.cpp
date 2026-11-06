/// \file
/// \brief [[BPR-1750]], RFC-BPR-0092 M3 (owner decision D2). The
/// clone link host on manual lanes with scripted reads: discovery in the
/// listed folders, choosing a folder, the private copy, a link checked again
/// and found moved, folders and suggestions, and forgetting.

#include "bpr/application/clone_link_host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <expected>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
	using namespace bpr;
	using application::clone_choice_answer;
	using application::clone_link_problem;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	class memory_store final : public application::clone_link_store_port
	{
	public:
		[[nodiscard]] auto load_clone_folders() -> std::optional<std::vector<std::string>> override { return Folders; }
		[[nodiscard]] auto store_clone_folders(const std::span<const std::string> aFolders) -> bool override
		{
			Folders.assign(aFolders.begin(), aFolders.end());
			return true;
		}
		[[nodiscard]] auto load_links() -> std::optional<std::vector<application::repository_link>> override { return Links; }
		[[nodiscard]] auto store_link(const application::repository_link& aLink) -> bool override
		{
			std::erase_if(Links, [&aLink](const application::repository_link& aOther) { return aOther.Host == aLink.Host && aOther.Repository == aLink.Repository; });
			Links.push_back(aLink);
			return true;
		}
		[[nodiscard]] auto forget_link(const std::string& aHost, const std::uint64_t aRepository, const std::string&, const std::string&) -> bool override
		{
			std::erase_if(Links, [&](const application::repository_link& aLink) { return aLink.Host == aHost && aLink.Repository == aRepository; });
			return true;
		}

		std::vector<std::string>                  Folders;
		std::vector<application::repository_link> Links;
	};

	[[nodiscard]] auto clone(std::string aPath, std::string aFolder, std::string aUrl) -> core::clone_candidate
	{
		auto value    = core::clone_candidate{ };
		value.Path    = std::move(aPath);
		value.Folder  = std::move(aFolder);
		value.Remotes = { { .Name = "origin", .Url = std::move(aUrl) } };
		value.Branch  = "main";
		return value;
	}

	struct fixture
	{
		application::manual_executor                    Effects;
		application::manual_executor                    Decision;
		memory_store                                    Store;
		std::map<std::string, core::clone_candidate>    Disk;
		std::vector<std::vector<std::string>>           Scans;
		std::vector<application::clone_link_projection> Published;
		std::unique_ptr<application::clone_link_host>   Host;

		fixture()
		{
			auto dependencies       = application::clone_link_dependencies{ };
			dependencies.Store      = &Store;
			dependencies.Reads.Scan = [this](const std::span<const std::string> aFolders, std::stop_token) {
				Scans.emplace_back(aFolders.begin(), aFolders.end());
				auto found = std::vector<core::clone_candidate>{ };
				for (const auto& [path, candidate] : Disk) {
					if (std::ranges::contains(aFolders, candidate.Folder)) {
						found.push_back(candidate);
					}
				}
				return found;
			};
			dependencies.Reads.Read = [this](const std::string& aPath) -> std::optional<core::clone_candidate> {
				std::scoped_lock lock{ DiskMutex };
				const auto       found = Disk.find(aPath);
				if (found == Disk.end()) {
					return std::nullopt;
				}
				auto read   = found->second;
				read.Folder = { };
				return read;
			};
			dependencies.Reads.Existing = [](const std::span<const std::string> aFolders) {
				auto existing = std::vector<std::string>{ };
				for (const auto& folder : aFolders) {
					if (folder.ends_with("Documents/GitHub") || folder.ends_with("/src")) {
						existing.push_back(folder);
					}
				}
				return existing;
			};
			dependencies.Home     = "/Users/me";
			dependencies.Effects  = &Effects;
			dependencies.Decision = &Decision;
			dependencies.Publish  = [this](const application::clone_link_projection& aProjection) { Published.push_back(aProjection); };
			// [[BPR-1879]]. The profile's folder of kept clones, and a scripted
			// download that the test drives.
			dependencies.ManagedRoot = "/profile/agent-clones-v1";
			dependencies.Keep        = [this](const application::clone_repository&, const std::string& aDestination) -> application::clone_link_dependencies::keep_job {
				++Prepared;
				if (!CanPrepare) {
					return { };
				}
				return [this, aDestination](const application::git_smart_clone_progress& aProgress, const std::stop_token aStop) -> std::expected<void, application::managed_clone_error> {
					++Runs;
					aProgress(1000U);
					while (!Release && !aStop.stop_requested()) {
						std::this_thread::sleep_for(std::chrono::milliseconds{ 2 });
					}
					if (aStop.stop_requested()) {
						return std::unexpected{ application::managed_clone_error{ application::managed_clone_failure::cancelled, { } } };
					}
					if (Outcome) {
						return std::unexpected{ application::managed_clone_error{ *Outcome, { } } };
					}
					std::scoped_lock lock{ DiskMutex };
					Disk[aDestination] = clone(aDestination, { }, "https://github.com/bpr-fixtures/engine.git");
					return { };
				};
			};
			Host = std::make_unique<application::clone_link_host>(std::move(dependencies));
		}

		~fixture()
		{
			Host->shutdown();
			static_cast<void>(wait([this] { return Host->is_quiescent(); }));
		}

		/// [[BPR-1879]]. Drains the lanes until \p aDone, while a kept clone's
		/// worker runs on its own thread; at most five seconds.
		template <typename Done>
		auto wait(Done aDone) -> bool
		{
			for (auto turns = 0; turns < 2500; ++turns) {
				settle();
				if (aDone()) {
					return true;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds{ 2 });
			}
			return false;
		}

		std::mutex                                        DiskMutex;
		std::atomic_bool                                  Release{ false };
		std::atomic_bool                                  CanPrepare{ true };
		std::optional<application::managed_clone_failure> Outcome;
		std::atomic_int                                   Prepared{ 0 };
		std::atomic_int                                   Runs{ 0 };

		void settle()
		{
			for (auto turns = 0; turns < 32 && (Effects.pending() != 0U || Decision.pending() != 0U); ++turns) {
				Effects.drain();
				Decision.drain();
			}
		}

		[[nodiscard]] auto view() const -> application::clone_link_projection { return Host->projection(); }
	};

	[[nodiscard]] auto engine() -> application::clone_repository
	{
		auto repository       = application::clone_repository{ };
		repository.Host       = "github.com";
		repository.Repository = 42U;
		repository.Owner      = "bpr-fixtures";
		repository.Name       = "engine";
		return repository;
	}

	[[nodiscard]] auto keepable() -> application::clone_repository
	{
		auto repository             = engine();
		repository.CanKeep          = true;
		repository.ApproximateBytes = 4096U;
		return repository;
	}

	/// \brief [[BPR-1879]]. Let BetterPullReview keep one: the location and
	/// size are known before it starts, the download runs on its own worker
	/// with progress, and once in place it is linked as the repository's clone,
	/// marked as the one BetterPullReview keeps. Cancel, a failure, a review
	/// that cannot keep one, one already in place, another repository.
	void test_kept_clone()
	{
		{
			auto f = fixture{ };
			f.Host->load();
			f.Host->observe_repository(engine());
			f.settle();
			expect(f.view().ManagedLocation.empty(), "a review that cannot keep a clone is offered none");
			f.Host->keep_clone();
			expect(f.Prepared == 0 && f.view().Keeping.State == application::managed_clone_state::none, "and pressing it does nothing");
		}
		auto f = fixture{ };
		f.Host->load();
		f.Host->observe_repository(keepable());
		f.settle();
		expect(f.view().ManagedLocation == "/profile/agent-clones-v1/github.com-42" && f.view().Repository->ApproximateBytes == 4096U,
			   "AC1: where it would live and GitHub's size are known before it starts");
		f.Host->keep_clone();
		expect(f.view().Keeping.State == application::managed_clone_state::downloading, "it starts downloading at once");
		f.Host->keep_clone();
		expect(f.wait([&f] { return f.view().Keeping.Received == 1000U; }), "bytes received reach the projection");
		expect(f.Prepared == 1 && !f.Host->repository_chosen(), "a second press starts no other; the step is not done while it runs");
		f.Release = true;
		expect(f.wait([&f] { return f.Host->repository_chosen(); }), "AC1: once it is in place the question can continue");
		expect(f.view().Linked && f.view().Linked->Managed && f.view().Linked->Path == "/profile/agent-clones-v1/github.com-42" && f.view().Linked->Remote == "origin"
				   && f.view().Keeping.State == application::managed_clone_state::none,
			   "it is linked as the clone BetterPullReview keeps");
		expect(f.Store.Links.size() == 1U && f.Store.Links.front().Path == "/profile/agent-clones-v1/github.com-42" && !f.Store.Links.front().PrivateCopy, "and remembered");

		// AC2: a new window reuses it without a download.
		auto again  = fixture{ };
		again.Store = f.Store;
		again.Disk  = f.Disk;
		again.Host->load();
		again.Host->observe_repository(keepable());
		again.settle();
		expect(again.Host->repository_chosen() && again.view().Linked->Managed && again.Runs == 0, "AC2: a later question reuses the kept clone and downloads nothing");

		auto found = fixture{ };
		found.Disk = f.Disk;
		found.Host->load();
		found.Host->observe_repository(keepable());
		found.settle();
		found.Host->keep_clone();
		expect(found.wait([&found] { return found.Host->repository_chosen(); }) && found.Runs == 0, "a kept clone already in place is linked without a download");

		auto cancel = fixture{ };
		cancel.Host->load();
		cancel.Host->observe_repository(keepable());
		cancel.settle();
		cancel.Host->keep_clone();
		expect(cancel.wait([&cancel] { return cancel.Runs == 1; }), "the download runs");
		cancel.Host->cancel_keeping();
		expect(cancel.wait([&cancel] { return cancel.view().Keeping.State == application::managed_clone_state::none; }) && !cancel.Host->repository_chosen() && cancel.Store.Links.empty(),
			   "AC1: Cancel stops it and links nothing");

		auto failed    = fixture{ };
		failed.Outcome = application::managed_clone_failure::connection_failed;
		failed.Release = true;
		failed.Host->load();
		failed.Host->observe_repository(keepable());
		failed.settle();
		failed.Host->keep_clone();
		expect(failed.wait([&failed] { return failed.view().Keeping.State == application::managed_clone_state::failed; })
				   && failed.view().Keeping.Failure == application::managed_clone_failure::connection_failed && !failed.Host->repository_chosen(),
			   "a failed download says why and links nothing");

		auto refused       = fixture{ };
		refused.CanPrepare = false;
		refused.Host->load();
		refused.Host->observe_repository(keepable());
		refused.settle();
		refused.Host->keep_clone();
		expect(refused.view().Keeping.State == application::managed_clone_state::failed && refused.view().Keeping.Failure == application::managed_clone_failure::unavailable && refused.Runs == 0,
			   "a review that cannot make one says so without starting");

		auto moved = fixture{ };
		moved.Host->load();
		moved.Host->observe_repository(keepable());
		moved.settle();
		moved.Host->keep_clone();
		expect(moved.wait([&moved] { return moved.Runs == 1; }), "the download runs again");
		auto other       = keepable();
		other.Repository = 43U;
		other.Name       = "other";
		moved.Host->observe_repository(other);
		expect(moved.view().Keeping.State == application::managed_clone_state::none && moved.view().ManagedLocation == "/profile/agent-clones-v1/github.com-43",
			   "another repository stops the download and has its own location");
		expect(moved.wait([&moved] { return moved.Host->is_quiescent(); }) && moved.Store.Links.empty(), "and the stopped download links nothing");
	}

	void test_folders_and_discovery()
	{
		auto f                                      = fixture{ };
		f.Disk["/Users/me/Documents/GitHub/engine"] = clone("/Users/me/Documents/GitHub/engine", "/Users/me/Documents/GitHub", "https://token@github.com/bpr-fixtures/engine");
		f.Disk["/Users/me/src/engine-old"]          = clone("/Users/me/src/engine-old", "/Users/me/src", "git@github.com:bpr-fixtures/engine.git");
		f.Host->load();
		f.Host->observe_repository(engine());
		f.settle();
		expect(f.view().Folders.empty(), "no clone folder is listed at first");
		expect(f.view().Suggestions == std::vector<std::string>{ "/Users/me/Documents/GitHub", "/Users/me/src" }, "usual folders that exist are suggested");
		expect(f.Scans.empty(), "nothing is scanned before a folder is listed");
		expect(!f.Host->repository_chosen(), "no link and no private copy: not chosen");

		f.Host->add_clone_folder("/Users/me/Documents/GitHub");
		f.settle();
		expect(f.Store.Folders == std::vector<std::string>{ "/Users/me/Documents/GitHub" }, "an added folder is saved");
		expect(f.view().Suggestions == std::vector<std::string>{ "/Users/me/src" }, "a listed folder is no longer suggested");
		expect(f.view().Matches.size() == 1U && f.view().Matches.front().Path == "/Users/me/Documents/GitHub/engine", "discovery runs in the listed folder");
		f.Host->add_clone_folder("/Users/me/src");
		f.Host->add_clone_folder("relative/folder");
		f.Host->add_clone_folder("/Users/me/src");
		f.settle();
		expect(f.view().Folders == std::vector<std::string>{ "/Users/me/Documents/GitHub", "/Users/me/src" }, "relative and repeated folders are not listed");
		expect(f.view().Matches.size() == 2U && f.view().Matches.back().Remote == "origin", "both clones match, in folder order");
		expect(f.view().Matches.front().Location == "github.com/bpr-fixtures/engine", "no user information in a match");

		f.Host->remove_clone_folder("/Users/me/Documents/GitHub");
		f.settle();
		expect(f.view().Matches.size() == 1U && f.view().Matches.front().Path == "/Users/me/src/engine-old", "removing a folder narrows the search");
	}

	void test_choice_private_copy_and_checks()
	{
		auto f                         = fixture{ };
		f.Disk["/Users/me/src/engine"] = clone("/Users/me/src/engine", "/Users/me/src", "https://github.com/bpr-fixtures/engine");
		f.Disk["/Users/me/src/other"]  = clone("/Users/me/src/other", "/Users/me/src", "https://github.com/bpr-fixtures/other");
		f.Host->load();
		f.Host->observe_repository(engine());
		f.settle();
		auto answer = std::optional<clone_choice_answer>{ };
		f.Host->choose_clone("/Users/me/src/other", [&answer](const clone_choice_answer aAnswer) { answer = aAnswer; });
		f.settle();
		expect(answer == clone_choice_answer::no_matching_remote, "a clone of another repository is refused and says why");
		f.Host->choose_clone("/Users/me/nothing", [&answer](const clone_choice_answer aAnswer) { answer = aAnswer; });
		f.settle();
		expect(answer == clone_choice_answer::not_a_clone, "a folder that is not a clone is refused");
		f.Host->choose_clone("/Users/me/src/engine", [&answer](const clone_choice_answer aAnswer) { answer = aAnswer; });
		f.settle();
		expect(answer == clone_choice_answer::linked && f.Host->repository_chosen(), "a clone that names the repository is linked");
		expect(f.Store.Links.size() == 1U && f.Store.Links.front().Path == "/Users/me/src/engine" && f.Store.Links.front().Remote == "origin", "and remembered");

		// A new window over the same store: the link is checked again.
		auto again  = fixture{ };
		again.Store = f.Store;
		again.Disk  = f.Disk;
		again.Host->load();
		again.Host->observe_repository(engine());
		again.settle();
		expect(again.Host->repository_chosen() && again.view().Linked->Path == "/Users/me/src/engine", "a remembered link that still serves is used");

		auto renamed = engine();
		renamed.Name = "engine-renamed";
		again.Host->observe_repository(renamed);
		again.settle();
		expect(again.Host->repository_chosen(), "a renamed repository keeps its link by identity");

		auto moved  = fixture{ };
		moved.Store = f.Store;
		moved.Host->load();
		moved.Host->observe_repository(engine());
		moved.settle();
		expect(!moved.Host->repository_chosen() && moved.view().Problem == clone_link_problem::not_a_clone, "a moved clone reopens the step with its reason");

		auto repointed                         = fixture{ };
		repointed.Store                        = f.Store;
		repointed.Disk["/Users/me/src/engine"] = clone("/Users/me/src/engine", "/Users/me/src", "https://github.com/someone/else");
		repointed.Host->load();
		repointed.Host->observe_repository(engine());
		repointed.settle();
		expect(repointed.view().Problem == clone_link_problem::no_matching_remote, "a clone that no longer names the repository says so");

		repointed.Host->use_private_copy();
		repointed.settle();
		expect(repointed.Host->repository_chosen() && repointed.view().Linked->PrivateCopy, "the private copy is a recorded choice");
		expect(repointed.Store.Links.size() == 1U && repointed.Store.Links.front().PrivateCopy && repointed.Store.Links.front().Path.empty(), "and replaces the link");

		repointed.Host->forget(*repointed.view().Linked);
		repointed.settle();
		expect(!repointed.Host->repository_chosen() && repointed.Store.Links.empty(), "Forget removes the choice");
	}
}

auto main() -> int
{
	test_folders_and_discovery();
	test_choice_private_copy_and_checks();
	test_kept_clone();
	std::cout << "clone-link-host-tests:pass\n";
	return EXIT_SUCCESS;
}
