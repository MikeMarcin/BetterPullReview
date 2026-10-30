/// \file
/// \brief [[BPR-1831]]. The clone scan, image and avatar fetches and queue
/// notifications leave diagnostic records: each records its end (and a scan
/// its start) with a typed failure, a duration and a count; high-frequency
/// fetches are aggregated per window; answers pass through unchanged; and no
/// folder, remote, address, login, title or body reaches a record.

#include "bpr/application/clone_link_host.hpp"
#include "bpr/application/host_operation_diagnostics.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/notification_delivery.hpp"
#include "bpr/core/operation_diagnosis.hpp"

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using application::diagnostic_operation;
	using application::diagnostic_outcome;
	using core::operation_action;
	using core::operation_failure;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	constexpr std::string_view canary = "PRIVATE_CANARY";

	class recording_log final : public application::diagnostic_log_port
	{
	public:
		[[nodiscard]] auto record(const application::diagnostic_event& aEvent) noexcept -> application::diagnostic_log_status override
		{
			Events.push_back(aEvent);
			return application::diagnostic_log_status::available;
		}
		[[nodiscard]] auto status() const noexcept -> application::diagnostic_log_status override { return application::diagnostic_log_status::available; }

		[[nodiscard]] auto line(const std::size_t aIndex) const -> std::string { return std::string{ Events.at(aIndex).Summary.text() }; }
		[[nodiscard]] auto last() const -> core::operation_diagnosis { return *core::parse_operation_diagnosis(Events.back().Summary.text()); }

		/// Every record is exactly an operation line on its family's
		/// operation, and holds none of the private words.
		void expect_private(const diagnostic_operation aOperation, const std::vector<std::string_view>& aPrivate) const
		{
			for (const auto& event : Events) {
				const auto text = event.Summary.text();
				expect(core::parse_operation_diagnosis(text).has_value() && event.Operation == aOperation, "every record is exactly an operation line on its family's operation");
				expect(event.Request == 0U && event.WindowSession == 0U, "with no identity");
				for (const auto word : aPrivate) {
					expect(text.find(word) == std::string_view::npos, "no private word reaches a record");
				}
			}
		}

		std::vector<application::diagnostic_event> Events;
	};

	void test_aggregation()
	{
		using clock    = application::aggregated_operation_log::clock;
		auto       log = std::make_shared<recording_log>();
		const auto t0  = clock::time_point{ } + std::chrono::hours{ 1 };
		{
			auto aggregate = application::aggregated_operation_log{ log, std::chrono::minutes{ 1 } };
			aggregate.record({ .Action = operation_action::image_fetch, .DurationMilliseconds = 40U, .Count = 99U }, t0);
			expect(log->Events.size() == 1U && log->line(0U) == "action=image_fetch failure=none process=none exit=none duration_ms=40 count=1 written=0 read_back=0",
				   "a group's first end records at once, counting itself");
			for (auto index = 1; index <= 3; ++index) {
				aggregate.record({ .Action = operation_action::image_fetch, .DurationMilliseconds = static_cast<std::uint32_t>(index * 10) }, t0 + std::chrono::seconds{ index });
			}
			expect(log->Events.size() == 1U, "ends inside the window are held back");
			aggregate.record({ .Action = operation_action::image_fetch, .Failure = operation_failure::blocked }, t0 + std::chrono::seconds{ 5 });
			expect(log->Events.size() == 2U && log->Events[1].Outcome == diagnostic_outcome::failed && log->last().Failure == operation_failure::blocked, "another failure is its own group and records at once");
			aggregate.record({ .Action = operation_action::image_fetch, .DurationMilliseconds = 5U }, t0 + std::chrono::seconds{ 61 });
			expect(log->Events.size() == 3U && log->last().Count == 4U && log->last().DurationMilliseconds == 30U, "after the window one line stands for every held end, with the longest duration");
			aggregate.record({ .Action = operation_action::image_fetch, .DurationMilliseconds = 7U }, t0 + std::chrono::seconds{ 62 });
			expect(log->Events.size() == 3U, "and the next end is held again");
		}
		expect(log->Events.size() == 4U && log->last().Count == 1U && log->last().DurationMilliseconds == 7U, "the held end is recorded when the log ends");
		expect(log->Events[0].Operation == diagnostic_operation::image_fetch, "on the image operation");

		auto       empty   = application::aggregated_operation_log{ nullptr };
		const auto invalid = std::make_shared<recording_log>();
		empty.record({ .Action = operation_action::image_fetch });
		auto refused = application::aggregated_operation_log{ invalid };
		refused.record({ .Action = operation_action::image_fetch, .Failure = operation_failure::refused_by_server });
		expect(invalid->Events.empty(), "no log records nothing, and an invalid line records nothing");
	}

	/// Answers each fetch when the test says.
	class scripted_fetch final : public application::markdown_image_fetch_port
	{
	public:
		void fetch(application::markdown_image_fetch_request aRequest, std::stop_token, std::function<void(application::markdown_image_fetch_result)> aCompletion) override
		{
			Requests.push_back(std::move(aRequest));
			Completions.push_back(std::move(aCompletion));
		}

		std::vector<application::markdown_image_fetch_request>                     Requests;
		std::vector<std::function<void(application::markdown_image_fetch_result)>> Completions;
	};

	void test_image_fetches()
	{
		auto log       = std::make_shared<recording_log>();
		auto aggregate = std::make_shared<application::aggregated_operation_log>(log);
		auto inner     = scripted_fetch{ };
		auto port      = application::diagnosed_image_fetch{ inner, aggregate };

		auto avatar                 = application::markdown_image_fetch_request{ };
		avatar.First.Address        = "https://avatars.githubusercontent.com/u/1?login=" + std::string{ canary };
		avatar.Policy.Hostname      = std::string{ canary };
		avatar.Policy.AvatarOrigins = true;
		auto answered               = std::optional<application::markdown_image_fetch_result>{ };
		port.fetch(avatar, { }, [&answered](application::markdown_image_fetch_result aResult) { answered = std::move(aResult); });
		expect(inner.Requests.size() == 1U && inner.Requests[0].First.Address == avatar.First.Address && inner.Requests[0].Policy.AvatarOrigins, "the request passes through unchanged");
		expect(log->Events.empty(), "a fetch records nothing until it ends");
		inner.Completions[0](application::markdown_image_fetched{ .Bytes = std::string{ canary }, .ContentType = "image/png" });
		expect(answered && *answered && (*answered)->Bytes == canary, "the bytes pass through unchanged");
		expect(log->Events.size() == 1U && log->Events[0].Outcome == diagnostic_outcome::succeeded && log->last().Action == operation_action::avatar_fetch, "an avatar fetch records its success");

		auto image          = application::markdown_image_fetch_request{ };
		image.First.Address = "https://example.invalid/" + std::string{ canary } + ".png";
		auto tooLarge       = std::optional<application::markdown_image_fetch_result>{ };
		port.fetch(image, { }, [&tooLarge](application::markdown_image_fetch_result aResult) { tooLarge = std::move(aResult); });
		inner.Completions[1](std::unexpected(core::markdown_image_state::too_large));
		expect(tooLarge && !*tooLarge && tooLarge->error() == core::markdown_image_state::too_large, "a failure passes through unchanged");
		expect(log->Events.size() == 2U && log->Events[1].Outcome == diagnostic_outcome::failed && log->last().Action == operation_action::image_fetch && log->last().Failure == operation_failure::too_large,
			   "a body image's failure records its state");

		auto stop = std::stop_source{ };
		port.fetch(image, stop.get_token(), [](application::markdown_image_fetch_result) { });
		stop.request_stop();
		inner.Completions[2](std::unexpected(core::markdown_image_state::failed));
		expect(log->Events.size() == 3U && log->Events[2].Outcome == diagnostic_outcome::cancelled, "a stopped fetch is a cancellation");

		for (auto index = 0; index < 50; ++index) {
			port.fetch(avatar, { }, [](application::markdown_image_fetch_result) { });
			inner.Completions.back()(application::markdown_image_fetched{ });
		}
		expect(log->Events.size() == 3U, "fifty more avatars inside the window record nothing yet");
		aggregate->flush();
		expect(log->Events.size() == 4U && log->last().Count == 50U && log->last().Action == operation_action::avatar_fetch, "and then one line counts them");
		log->expect_private(diagnostic_operation::image_fetch, { canary, "avatars.githubusercontent", "example.invalid", "image/png" });

		expect(application::operation_failure_of(core::markdown_image_state::blocked) == operation_failure::blocked && application::operation_failure_of(core::markdown_image_state::unsupported) == operation_failure::unsupported
				   && application::operation_failure_of(core::markdown_image_state::failed) == operation_failure::fetch_failed && application::operation_failure_of(core::markdown_image_state::over_limit) == operation_failure::over_limit,
			   "each image state names its failure");
	}

	class scripted_platform final : public application::notification_sink_port
	{
	public:
		[[nodiscard]] auto permission() -> application::notification_permission override { return Current; }
		[[nodiscard]] auto request_permission() -> application::notification_permission override { return Current = Answer; }
		[[nodiscard]] auto deliver(const application::delivered_notification& aPayload) -> application::notification_submission_result override
		{
			Payloads.push_back(aPayload);
			return Result;
		}

		application::notification_permission             Current = application::notification_permission::not_determined;
		application::notification_permission             Answer  = application::notification_permission::granted;
		application::notification_submission_result      Result  = application::notification_submission_result::submitted;
		std::vector<application::delivered_notification> Payloads;
	};

	void test_notifications()
	{
		auto log      = std::make_shared<recording_log>();
		auto platform = scripted_platform{ };
		auto chosen   = std::optional<application::queue_notification_target>{ };
		auto notifier = application::review_queue_notifier{ platform, [&chosen](application::queue_notification_target aTarget) { chosen = std::move(aTarget); }, log };
		auto request  = application::queue_notification_request{
			.Ref            = { .Host = std::string{ canary }, .Account = "octo-PRIVATE", .Repository = "repo-PRIVATE", .Number = 336 },
			.Bucket         = core::attention_bucket::new_comments_or_author_replies,
			.UnreadComments = 3U,
		};

		expect(notifier.deliver(request) && platform.Payloads.size() == 1U, "a granted notification is delivered as before");
		expect(log->Events.size() == 2U && log->line(0U).starts_with("action=notification_permission failure=none ") && log->line(1U).starts_with("action=notification_deliver failure=none "),
			   "the permission answer and the submission each record their end");

		platform.Result = application::notification_submission_result::suppressed_unattributable_activation;
		expect(!notifier.deliver(request), "an unattributable activation still suppresses");
		expect(log->Events.back().Outcome == diagnostic_outcome::failed && log->last().Failure == operation_failure::unattributable_activation, "and records why");

		platform.Current = application::notification_permission::refused;
		expect(!notifier.deliver(request), "a refusal still delivers nothing");
		expect(log->last().Action == operation_action::notification_deliver && log->last().Failure == operation_failure::permission_refused, "and records the refusal");

		const auto before = log->Events.size();
		auto       quiet  = request;
		quiet.Bucket      = core::attention_bucket::quiet;
		expect(!notifier.deliver(quiet) && log->Events.size() == before, "a quiet bucket records nothing");

		notifier.activate(application::notification_target(request));
		expect(chosen && log->last().Action == operation_action::notification_activate && log->Events.back().Outcome == diagnostic_outcome::succeeded, "an activation routes and records");
		log->expect_private(diagnostic_operation::notification, { canary, "PRIVATE", platform.Payloads.front().Title, platform.Payloads.front().Body });

		auto unlogged = application::review_queue_notifier{ platform };
		static_cast<void>(unlogged.deliver(request));
	}

	struct clone_fixture
	{
		struct store final : application::clone_link_store_port
		{
			[[nodiscard]] auto load_clone_folders() -> std::optional<std::vector<std::string>> override { return Unreadable ? std::nullopt : std::optional{ Folders }; }
			[[nodiscard]] auto store_clone_folders(const std::span<const std::string> aFolders) -> bool override
			{
				Folders.assign(aFolders.begin(), aFolders.end());
				return true;
			}
			[[nodiscard]] auto load_links() -> std::optional<std::vector<application::repository_link>> override { return Links; }
			[[nodiscard]] auto store_link(const application::repository_link& aLink) -> bool override
			{
				Links.push_back(aLink);
				return true;
			}
			[[nodiscard]] auto forget_link(const std::string&, std::uint64_t, const std::string&, const std::string&) -> bool override { return true; }

			std::vector<std::string>                  Folders;
			std::vector<application::repository_link> Links;
			bool                                      Unreadable = false;
		};

		application::manual_executor                  Effects;
		application::manual_executor                  Decision;
		store                                         Store;
		std::shared_ptr<recording_log>                Log = std::make_shared<recording_log>();
		std::vector<core::clone_candidate>            Found;
		std::optional<core::clone_candidate>          Read;
		bool                                          ScanThrows = false;
		std::unique_ptr<application::clone_link_host> Host;

		clone_fixture()
		{
			auto dependencies       = application::clone_link_dependencies{ };
			dependencies.Store      = &Store;
			dependencies.Reads.Scan = [this](std::span<const std::string>, std::stop_token) {
				if (ScanThrows) {
					throw std::runtime_error{ std::string{ canary } };
				}
				return Found;
			};
			dependencies.Reads.Read = [this](const std::string&) { return Read; };
			dependencies.Effects    = &Effects;
			dependencies.Decision   = &Decision;
			dependencies.Log        = Log;
			Host                    = std::make_unique<application::clone_link_host>(std::move(dependencies));
		}
		~clone_fixture() { Host->shutdown(); }

		void settle()
		{
			for (auto turns = 0; turns < 32 && (Effects.pending() != 0U || Decision.pending() != 0U); ++turns) {
				Effects.drain();
				Decision.drain();
			}
		}

		[[nodiscard]] auto lines(const operation_action aAction) const -> std::vector<core::operation_diagnosis>
		{
			auto found = std::vector<core::operation_diagnosis>{ };
			for (const auto& event : Log->Events) {
				const auto line = core::parse_operation_diagnosis(event.Summary.text());
				if (line && line->Action == aAction) {
					found.push_back(*line);
				}
			}
			return found;
		}
	};

	[[nodiscard]] auto candidate(const std::string& aPath, const std::string& aUrl) -> core::clone_candidate
	{
		auto value    = core::clone_candidate{ };
		value.Path    = aPath;
		value.Folder  = "/Users/PRIVATE/src";
		value.Remotes = { { .Name = "origin", .Url = aUrl } };
		value.Branch  = "main";
		return value;
	}

	[[nodiscard]] auto repository() -> application::clone_repository
	{
		return { .Host = "github.com", .Repository = 42U, .Owner = "owner-PRIVATE", .Name = "engine-PRIVATE" };
	}

	void test_clone_scan()
	{
		auto f  = clone_fixture{ };
		f.Found = { candidate("/Users/PRIVATE/src/engine", "https://token-PRIVATE@github.com/owner-PRIVATE/engine-PRIVATE"), candidate("/Users/PRIVATE/src/other", "https://github.com/x/y") };
		f.Host->load();
		f.Host->observe_repository(repository());
		f.settle();
		expect(f.lines(operation_action::clone_load).size() == 1U && f.lines(operation_action::clone_load)[0].Failure == operation_failure::none, "reading the folders and links records its end");
		f.Host->add_clone_folder("/Users/PRIVATE/src");
		f.settle();
		const auto scans = f.lines(operation_action::clone_scan);
		expect(scans.size() == 2U && f.Log->Events[f.Log->Events.size() - 2U].Outcome == diagnostic_outcome::started && scans[1].Count == 2U && f.Log->Events.back().Outcome == diagnostic_outcome::succeeded,
			   "a scan records its start and its end, counting the clones it found");

		f.ScanThrows = true;
		f.Host->look_again();
		f.settle();
		expect(f.lines(operation_action::clone_scan).back().Failure == operation_failure::scan_failed && f.Log->Events.back().Outcome == diagnostic_outcome::failed, "a scan that throws is a failure");

		auto moved = clone_fixture{ };
		moved.Store.Links.push_back({ .Host = "github.com", .Repository = 42U, .Owner = "owner-PRIVATE", .Name = "engine-PRIVATE", .Path = "/Users/PRIVATE/src/engine", .Remote = "origin" });
		moved.Host->load();
		moved.Host->observe_repository(repository());
		moved.settle();
		expect(moved.lines(operation_action::clone_verify).size() == 1U && moved.lines(operation_action::clone_verify)[0].Failure == operation_failure::not_a_clone, "a moved clone's check records not_a_clone");

		auto repointed        = clone_fixture{ };
		repointed.Store.Links = moved.Store.Links;
		repointed.Read        = candidate("/Users/PRIVATE/src/engine", "https://github.com/someone/else");
		repointed.Host->load();
		repointed.Host->observe_repository(repository());
		repointed.settle();
		expect(repointed.lines(operation_action::clone_verify).back().Failure == operation_failure::no_matching_remote, "a repointed clone's check records no_matching_remote");

		auto serving        = clone_fixture{ };
		serving.Store.Links = moved.Store.Links;
		serving.Read        = candidate("/Users/PRIVATE/src/engine", "https://github.com/owner-PRIVATE/engine-PRIVATE");
		serving.Host->load();
		serving.Host->observe_repository(repository());
		serving.settle();
		expect(serving.Host->repository_chosen() && serving.lines(operation_action::clone_verify).back().Failure == operation_failure::none, "a link that serves records its success");

		auto unreadable             = clone_fixture{ };
		unreadable.Store.Unreadable = true;
		unreadable.Host->load();
		unreadable.settle();
		expect(unreadable.lines(operation_action::clone_load).back().Failure == operation_failure::store_unreadable, "an unreadable store is a failure");

		for (const auto* fixture : { &f, &moved, &repointed, &serving, &unreadable }) {
			fixture->Log->expect_private(diagnostic_operation::clone_scan, { canary, "PRIVATE", "token", "github.com", "origin" });
		}
	}

	void test_search_status_names()
	{
		using status = core::source_search_status;
		expect(application::operation_failure_of(status::complete) == operation_failure::none && application::operation_failure_of(status::result_limit) == operation_failure::none
				   && application::operation_failure_of(status::cancelled) == operation_failure::cancelled && application::operation_failure_of(status::unavailable) == operation_failure::unavailable
				   && application::operation_failure_of(status::resource_limit) == operation_failure::resource_limit && application::operation_failure_of(status::failure) == operation_failure::search_failed,
			   "each search terminal names its outcome");
		expect(application::diagnostic_category(application::diagnostic_operation_names[static_cast<std::size_t>(diagnostic_operation::storage)]) == "storage"
				   && application::diagnostic_category(application::diagnostic_operation_names[static_cast<std::size_t>(diagnostic_operation::notification)]) == "notification",
			   "the new operations have their own categories");
	}
} // namespace

auto main() -> int
{
	test_aggregation();
	test_image_fetches();
	test_notifications();
	test_clone_scan();
	test_search_status_names();
	std::cout << "host-operation-diagnostics-tests:pass\n";
	return EXIT_SUCCESS;
}
