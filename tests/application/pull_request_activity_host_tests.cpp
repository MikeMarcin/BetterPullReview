// [[BPR-1850]]. The activity host over a fixture transport: one timer, the
// poll interval obeyed, 304s that change nothing, backoff, the baseline,
// relaunch without re-notifying, focus, sign-out and the setting.
#include "bpr/application/pull_request_activity_host.hpp"

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	class manual_executor final : public application::decision_executor, public application::effect_executor
	{
	public:
		void post(std::function<void()> aTask) override { Tasks.push_back(std::move(aTask)); }
		auto run_one() -> bool
		{
			if (Tasks.empty()) {
				return false;
			}
			auto task = std::move(Tasks.front());
			Tasks.pop_front();
			task();
			return true;
		}
		std::deque<std::function<void()>> Tasks;
	};

	constexpr auto date = std::string_view{ "Fri, 30 Oct 2026 04:02:16 GMT" };

	/// Answers each poll with the next scripted answer; holds the completion
	/// when the script is empty, as a slow network would.
	class fixture_transport final : public application::pull_request_activity_port
	{
	public:
		void poll_activity(application::pull_request_activity_request aRequest, application::pull_request_activity_completion aCompletion) override
		{
			Requests.push_back(aRequest);
			if (Script.empty()) {
				Held = std::move(aCompletion);
				return;
			}
			auto answer = std::move(Script.front());
			Script.pop_front();
			aCompletion(std::move(answer));
		}
		std::vector<application::pull_request_activity_request> Requests;
		std::deque<application::pull_request_activity_answer>   Script;
		application::pull_request_activity_completion           Held;
	};

	class memory_store final : public application::pull_request_activity_store_port
	{
	public:
		auto store(const core::activity_memory& aMemory) -> bool override
		{
			Row = core::stored_activity_rows(aMemory);
			++Writes;
			return true;
		}
		[[nodiscard]] auto                          load() const -> core::activity_memory_read { return core::read_activity_memory(Row); }
		std::optional<core::stored_activity_memory> Row;
		std::size_t                                 Writes = 0U;
	};

	class one_timer final : public application::pull_request_activity_timer_port
	{
	public:
		void                         schedule(const std::uint32_t aSeconds) override { Armed = aSeconds; }
		void                         cancel() override { Armed.reset(); }
		std::optional<std::uint32_t> Armed;
	};

	class posted final : public application::pull_request_activity_sink_port
	{
	public:
		auto post(const std::string& aTitle, const std::string& aBody) -> bool override
		{
			Posts.emplace_back(aTitle, aBody);
			return true;
		}
		std::vector<std::pair<std::string, std::string>> Posts;
	};

	auto session(const application::github_authentication_transport aTransport = application::github_authentication_transport::github_cli) -> application::personal_discovery_session
	{
		auto value                            = application::personal_discovery_session{ };
		value.Queue                           = { .Host = "github.com", .Account = "me", .Generation = 3U, .ViewerAccount = core::github_account_id{ 42U } };
		value.Context.ApplicationSession      = core::application_session_id{ 1U };
		value.Context.Profile                 = core::host_profile_id{ 2U };
		value.Context.Host                    = core::git_host_id{ 5U };
		value.Context.Account                 = core::github_account_id{ 42U };
		value.Context.IdentityGeneration      = core::github_identity_generation{ 3U };
		value.ProfileAuthority.Authentication = aTransport;
		return value;
	}

	auto thread(std::string aId, const std::int64_t aUpdated, const std::int64_t aNumber) -> core::activity_thread
	{
		return { .Id = std::move(aId), .UpdatedAt = aUpdated, .Unread = true, .LastReadAt = std::nullopt, .Reason = core::activity_reason::author, .Repository = "octo/widgets", .Number = aNumber, .Anchor = { .Kind = core::activity_anchor_kind::issue_comment, .Id = 9U } };
	}

	auto changed(std::vector<core::activity_thread> aThreads, const std::uint32_t aInterval = 60U) -> application::pull_request_activity_answer
	{
		auto answer              = application::pull_request_activity_answer{ };
		answer.Poll.Outcome      = core::activity_poll_outcome::changed;
		answer.Poll.PollInterval = aInterval;
		answer.LastModified      = std::string{ date };
		answer.Threads           = std::move(aThreads);
		answer.Complete          = true;
		answer.Requests          = 2U;
		return answer;
	}

	auto unchanged(const std::uint32_t aInterval = 60U) -> application::pull_request_activity_answer
	{
		auto answer              = application::pull_request_activity_answer{ };
		answer.Poll.Outcome      = core::activity_poll_outcome::unchanged;
		answer.Poll.PollInterval = aInterval;
		answer.Requests          = 2U;
		return answer;
	}

	/// The diagnostics log, as the summaries it was handed.
	class recording_log final : public application::diagnostic_log_port
	{
	public:
		auto record(const application::diagnostic_event& aEvent) noexcept -> application::diagnostic_log_status override
		{
			try {
				Lines.emplace_back(aEvent.Summary.text());
			} catch (...) { }
			return application::diagnostic_log_status::available;
		}
		auto                     status() const noexcept -> application::diagnostic_log_status override { return application::diagnostic_log_status::available; }
		std::vector<std::string> Lines;
	};

	struct harness
	{
		explicit harness(core::activity_memory_read aLoaded = { })
			: Log{ std::make_shared<recording_log>() }
			, Host{ Transport, Store, Effects, Decisions, Timer, Sink, [this] { return Now; }, [this] { return Focused; }, std::move(aLoaded), Log }
		{ }
		[[nodiscard]] auto logged(const std::string_view aAction) const -> std::size_t
		{
			return static_cast<std::size_t>(std::ranges::count_if(Log->Lines, [aAction](const std::string& aLine) { return aLine.starts_with("action=" + std::string{ aAction } + " "); }));
		}
		void drain()
		{
			while (Effects.run_one() || Decisions.run_one()) { }
		}
		fixture_transport                       Transport;
		memory_store                            Store;
		manual_executor                         Effects;
		manual_executor                         Decisions;
		one_timer                               Timer;
		posted                                  Sink;
		std::int64_t                            Now = 1793707000;
		std::vector<core::pull_request_ref>     Focused;
		std::shared_ptr<recording_log>          Log;
		application::pull_request_activity_host Host;
	};

	void test_polling(memory_store& aKept)
	{
		auto h = harness{ };
		expect(h.Host.state() == application::pull_request_activity_state::signed_out && h.Transport.Requests.empty(), "nothing is polled signed out");

		h.Transport.Script.push_back(changed({ thread("1", 1793706000, 7) }));
		h.Host.adopt(session());
		h.drain();
		expect(h.Transport.Requests.size() == 1U && h.Transport.Requests[0].IfModifiedSince.empty(), "a session polls at once, unconditionally the first time");
		expect(h.Transport.Requests[0].Context.Request.value() != 0U && h.Transport.Requests[0].Context.Profile == core::host_profile_id{ 2U }, "under the session's own identity");
		expect(h.Sink.Posts.empty(), "the first answer only takes a baseline");
		expect(h.Timer.Armed == 60U && h.Host.state() == application::pull_request_activity_state::watching, "the one timer waits the poll interval");
		expect(h.Store.Writes == 2U && h.Store.load().Memory.LastModified == date && h.Store.load().Memory.Baselined, "the account and the baseline are written");

		h.Now += 60;
		h.Transport.Script.push_back(unchanged(120U));
		h.Host.tick();
		h.drain();
		expect(h.Transport.Requests.back().IfModifiedSince == date, "the next poll is conditional on the last date");
		expect(h.Timer.Armed == 120U, "X-Poll-Interval is obeyed");
		expect(h.Store.Writes == 2U && h.Sink.Posts.empty(), "a 304 writes and posts nothing");

		h.Now += 120;
		h.Transport.Script.push_back(changed({ thread("1", 1793707100, 7) }));
		h.Host.tick();
		h.drain();
		expect(h.Sink.Posts.size() == 1U && h.Sink.Posts[0] == std::pair<std::string, std::string>{ "octo/widgets #7", "New comment." }, "new activity on my pull request is posted");
		const auto target = h.Host.activate();
		expect(target.PullRequest == core::pull_request_ref{ .Host = "github.com", .Account = "me", .Repository = "octo/widgets", .Number = 7 }, "its click opens that pull request");
		expect(target.Anchor == core::activity_anchor{ .Kind = core::activity_anchor_kind::issue_comment, .Id = 9U }, "at its comment");

		h.Now += 60;
		h.Transport.Script.push_back(changed({ thread("1", 1793707100, 7) }));
		h.Host.tick();
		h.drain();
		expect(h.Sink.Posts.size() == 1U, "the same thread again is not posted again");

		h.Now += 60;
		h.Focused = { { .Host = "github.com", .Account = "me", .Repository = "octo/widgets", .Number = 8 } };
		h.Transport.Script.push_back(changed({ thread("1", 1793707100, 7), thread("2", 1793707300, 8) }));
		h.Host.tick();
		h.drain();
		expect(h.Sink.Posts.size() == 1U && h.Host.statistics().Suppressed == 1U, "activity on the focused pull request is not posted");
		h.Focused.clear();

		auto failure         = application::pull_request_activity_answer{ };
		failure.Poll.Outcome = core::activity_poll_outcome::failed;
		failure.Requests     = 1U;
		h.Transport.Script.push_back(failure);
		h.Host.tick();
		h.drain();
		expect(h.Timer.Armed == 120U && h.Host.state() == application::pull_request_activity_state::waiting, "a failure backs off");

		auto limited            = application::pull_request_activity_answer{ };
		limited.Poll.Outcome    = core::activity_poll_outcome::rate_limited;
		limited.Poll.RetryAfter = 900U;
		h.Transport.Script.push_back(limited);
		h.Host.tick();
		h.drain();
		expect(h.Timer.Armed == 900U, "a rate limit's Retry-After is waited");

		const auto stats = h.Host.statistics();
		expect(stats.Polls == 7U && stats.Unchanged == 1U && stats.Changed == 4U && stats.Failed == 1U && stats.RateLimited == 1U && stats.Posted == 1U && stats.Requests == 11U,
			   "statistics count polls, answers and wire requests");
		expect(h.logged("activity_poll_changed") == 4U && h.logged("activity_poll_unchanged") == 1U && h.logged("activity_poll") == 2U && h.logged("activity_post") == 1U
				   && h.logged("activity_suppress") == 1U && h.logged("activity_activate") == 1U,
			   "the diagnostics log has one line per poll, post, suppression and click");

		// Signing out cancels the timer; a poll answered afterwards is stale.
		h.Host.tick();
		expect(h.Transport.Held != nullptr, "a poll is in flight");
		h.Host.adopt(std::nullopt);
		expect(!h.Timer.Armed && h.Host.state() == application::pull_request_activity_state::signed_out, "signing out stops polling");
		h.Transport.Held(changed({ thread("3", 1793708000, 9) }));
		h.drain();
		expect(h.Sink.Posts.size() == 1U && h.Host.statistics().Stale == 1U && !h.Timer.Armed, "and its late answer is ignored");
		h.Host.shutdown();
		h.drain();
		aKept = h.Store;
	}

	void test_relaunch(const memory_store& aKept)
	{
		auto h  = harness{ aKept.load() };
		h.Store = aKept;
		h.Transport.Script.push_back(changed({ thread("1", 1793707100, 7), thread("2", 1793707300, 8) }));
		h.Host.adopt(session());
		h.drain();
		expect(h.Transport.Requests.size() == 1U && h.Transport.Requests[0].IfModifiedSince == date, "a relaunch polls conditionally on the stored date");
		expect(h.Sink.Posts.empty(), "and does not re-notify what was seen before it");
	}

	void test_setting_and_sign_in()
	{
		auto h = harness{ };
		h.Transport.Script.push_back(changed({ }));
		h.Host.adopt(session());
		h.drain();
		h.Host.set_enabled(false);
		h.drain();
		expect(!h.Timer.Armed && h.Host.state() == application::pull_request_activity_state::off && !h.Store.load().Memory.Enabled, "off stops the timer and is stored");
		const auto before = h.Transport.Requests.size();
		h.Host.tick();
		expect(h.Transport.Requests.size() == before, "nothing is polled while off");
		h.Transport.Script.push_back(changed({ thread("5", 1793707000, 3) }));
		h.Host.set_enabled(true);
		h.drain();
		expect(h.Transport.Requests.size() == before + 1U && h.Transport.Requests.back().IfModifiedSince.empty() && h.Sink.Posts.empty(), "on polls at once and takes a new baseline");

		auto native = harness{ };
		native.Host.adopt(session(application::github_authentication_transport::native_https));
		native.drain();
		expect(native.Transport.Requests.empty() && !native.Timer.Armed && native.Host.state() == application::pull_request_activity_state::unsupported,
			   "a native sign-in, which GitHub refuses the list, is not polled");

		auto refused        = harness{ };
		auto answer         = application::pull_request_activity_answer{ };
		answer.Poll.Outcome = core::activity_poll_outcome::unsupported;
		refused.Transport.Script.push_back(answer);
		refused.Host.adopt(session());
		refused.drain();
		refused.Host.tick();
		expect(refused.Transport.Requests.size() == 1U && !refused.Timer.Armed && refused.Host.state() == application::pull_request_activity_state::unsupported,
			   "a sign-in GitHub refuses stops polling");
	}
} // namespace

auto main() -> int
{
	auto kept = memory_store{ };
	test_polling(kept);
	test_relaunch(kept);
	test_setting_and_sign_in();
	std::cout << "pull-request-activity-host:pass\n";
	return EXIT_SUCCESS;
}
