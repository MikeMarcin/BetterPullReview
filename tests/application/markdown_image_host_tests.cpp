#include "bpr/application/identity_avatar_window.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/markdown_image_service.hpp"
#include "bpr/application/markdown_image_window.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::application;

	void expect(const bool aCondition, const std::string& aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	struct fake_fetch final : markdown_image_fetch_port
	{
		struct call
		{
			markdown_image_fetch_request                     Request;
			std::stop_token                                  Stop;
			std::function<void(markdown_image_fetch_result)> Complete;
		};
		std::vector<call> Calls;

		void fetch(markdown_image_fetch_request aRequest, std::stop_token aStop, std::function<void(markdown_image_fetch_result)> aCompletion) override
		{
			Calls.push_back({ std::move(aRequest), std::move(aStop), std::move(aCompletion) });
		}

		void ok(const std::size_t aIndex, std::string aBytes = "png", std::string aType = "image/png")
		{
			Calls[aIndex].Complete(markdown_image_fetched{ .Bytes = std::move(aBytes), .ContentType = std::move(aType) });
		}
	};

	struct fake_decoder final : markdown_image_decoder_port
	{
		mutable int Calls = 0;

		[[nodiscard]] auto decode(const std::string_view aBytes) const -> std::expected<markdown_decoded_image, core::markdown_image_state> override
		{
			++Calls;
			if (aBytes == "bad") {
				return std::unexpected{ core::markdown_image_state::unsupported };
			}
			auto image   = markdown_decoded_image{ };
			image.Width  = 10U;
			image.Height = 10U;
			image.Frames.push_back({ .Pixels = std::vector<std::uint8_t>(aBytes == "big" ? 4000U : 400U, 0xFFU), .DelayMs = 0U });
			return image;
		}
	};

	struct fake_resolution final : markdown_image_resolution_port
	{
		struct call
		{
			markdown_image_resolution_request                         Request;
			std::function<void(markdown_image_resolution_completion)> Complete;
		};
		std::vector<call>                              Calls;
		std::vector<markdown_image_resolution_request> Cancelled;

		void resolve(markdown_image_resolution_request aRequest, std::function<void(markdown_image_resolution_completion)> aCompletion) override
		{
			Calls.push_back({ std::move(aRequest), std::move(aCompletion) });
		}
		void cancel_resolution(const markdown_image_resolution_request& aRequest) override
		{
			Cancelled.push_back(aRequest);
		}
	};

	/// The profile's disk, kept across two services as across a restart.
	struct fake_disk final : markdown_image_disk_cache_port
	{
		std::map<std::string, std::string> Stored;
		int                                Loads = 0;

		[[nodiscard]] auto load(const std::string& aKey, std::stop_token) -> std::optional<std::string> override
		{
			++Loads;
			const auto found = Stored.find(aKey);
			return found == Stored.end() ? std::nullopt : std::optional<std::string>{ found->second };
		}
		auto store(const std::string& aKey, const std::string_view aBytes, std::stop_token) -> bool override
		{
			Stored[aKey] = std::string{ aBytes };
			return true;
		}
	};

	auto fetch_request(const std::string& aAddress) -> markdown_image_fetch_request
	{
		return { .First = { .Address = aAddress, .Credential = false, .Hop = 0U }, .Policy = { .Contract = core::markdown_image_host_contract::github_com, .Hostname = "github.com", .CredentialAvailable = false } };
	}

	struct outcome
	{
		int                                           Calls = 0;
		core::markdown_image_state                    State = core::markdown_image_state::loading;
		std::shared_ptr<const markdown_decoded_image> Image;
	};

	auto recorder(const std::shared_ptr<outcome>& aOutcome) -> markdown_image_service::subscriber
	{
		return [aOutcome](const core::markdown_image_state aState, std::shared_ptr<const markdown_decoded_image> aImage) {
			++aOutcome->Calls;
			aOutcome->State = aState;
			aOutcome->Image = std::move(aImage);
		};
	}

	void settle(manual_executor& aDecision, manual_executor& aDecode)
	{
		while (aDecision.pending() != 0U || aDecode.pending() != 0U) {
			aDecision.drain();
			aDecode.drain();
		}
	}

	void service_concurrency_and_sharing()
	{
		manual_executor                       decision;
		manual_executor                       decodeLane;
		fake_fetch                            fetch;
		fake_decoder                          decoder;
		markdown_image_service                service{ decision, decodeLane, &fetch, &decoder };
		std::vector<std::shared_ptr<outcome>> outcomes;
		for (int index = 0; index < 6; ++index) {
			outcomes.push_back(std::make_shared<outcome>());
			static_cast<void>(service.load({ .Key = "k" + std::to_string(index), .Fetch = fetch_request("https://camo.githubusercontent.com/" + std::to_string(index)) }, recorder(outcomes.back())));
		}
		expect(fetch.Calls.size() == core::markdown_image_fetch_concurrency && service.active_fetches() == 4U, "at most four fetches run at once");
		auto shared = std::make_shared<outcome>();
		static_cast<void>(service.load({ .Key = "k0", .Fetch = fetch_request("https://camo.githubusercontent.com/0") }, recorder(shared)));
		expect(fetch.Calls.size() == 4U, "the same key joins the running fetch");
		fetch.ok(0);
		decision.drain();
		expect(decoder.Calls == 0, "decoding waits for the decode lane, never the decision lane");
		expect(fetch.Calls.size() == 5U, "a finished fetch starts the next queued one");
		decodeLane.drain();
		decision.drain();
		expect(outcomes[0]->Calls == 1 && outcomes[0]->State == core::markdown_image_state::loaded && outcomes[0]->Image != nullptr, "decoded image delivered");
		expect(shared->Calls == 1 && shared->Image == outcomes[0]->Image, "joined subscriber gets the same pixels");
		expect(service.cached_bytes() == 400U, "decoded bytes cached");

		auto hit = std::make_shared<outcome>();
		static_cast<void>(service.load({ .Key = "k0", .Fetch = fetch_request("https://camo.githubusercontent.com/0") }, recorder(hit)));
		expect(hit->Calls == 0, "a cache hit never delivers inline");
		decision.drain();
		expect(hit->Calls == 1 && hit->State == core::markdown_image_state::loaded && fetch.Calls.size() == 5U, "a cache hit makes no request");

		for (std::size_t index = 1; index < fetch.Calls.size(); ++index) {
			fetch.ok(index);
		}
		settle(decision, decodeLane);
		expect(fetch.Calls.size() == 6U && fetch.Calls[5].Request.First.Address.ends_with("/5"), "queue drains in order");
		fetch.ok(5);
		settle(decision, decodeLane);
		expect(service.quiescent(), "quiescent after every fetch and decode returned");
	}

	void service_cancellation_and_failures()
	{
		manual_executor        decision;
		manual_executor        decodeLane;
		fake_fetch             fetch;
		fake_decoder           decoder;
		markdown_image_service service{ decision, decodeLane, &fetch, &decoder, 800U };
		auto                   first  = std::make_shared<outcome>();
		auto                   second = std::make_shared<outcome>();
		const auto             a      = service.load({ .Key = "a", .Fetch = fetch_request("https://camo.githubusercontent.com/a") }, recorder(first));
		const auto             b      = service.load({ .Key = "a", .Fetch = fetch_request("https://camo.githubusercontent.com/a") }, recorder(second));
		service.cancel(a);
		expect(!fetch.Calls[0].Stop.stop_requested(), "a remaining subscriber keeps the fetch");
		service.cancel(b);
		expect(fetch.Calls[0].Stop.stop_requested(), "the last cancellation stops the fetch");
		fetch.ok(0);
		settle(decision, decodeLane);
		expect(first->Calls == 0 && second->Calls == 0 && decoder.Calls == 0, "a cancelled load delivers and decodes nothing");
		expect(service.quiescent(), "a late completion still releases its slot");

		const auto failing = [&](const std::string& aKey, markdown_image_fetch_result aResult) {
			auto seen = std::make_shared<outcome>();
			static_cast<void>(service.load({ .Key = aKey, .Fetch = fetch_request("https://camo.githubusercontent.com/" + aKey) }, recorder(seen)));
			fetch.Calls.back().Complete(std::move(aResult));
			settle(decision, decodeLane);
			return seen->State;
		};
		expect(failing("svg", markdown_image_fetched{ .Bytes = "<svg/>", .ContentType = "image/svg+xml; charset=utf-8" }) == core::markdown_image_state::loaded, "an SVG type reaches the decoder (RFC-BPR-0084 Q1)");
		expect(failing("html", markdown_image_fetched{ .Bytes = "<html>", .ContentType = "text/html" }) == core::markdown_image_state::unsupported, "a non-image type is unsupported");
		expect(failing("huge", markdown_image_fetched{ .Bytes = std::string(core::markdown_image_encoded_bytes + 1U, 'x'), .ContentType = "image/png" }) == core::markdown_image_state::too_large, "over the byte bound is too large");
		expect(failing("blocked", std::unexpected{ core::markdown_image_state::blocked }) == core::markdown_image_state::blocked, "a refused hop is blocked");
		expect(failing("bad", markdown_image_fetched{ .Bytes = "bad", .ContentType = "IMAGE/PNG" }) == core::markdown_image_state::unsupported, "a decoder refusal ends the load");
		expect(decoder.Calls == 2, "only image types reach the decoder");

		for (const auto* key : { "x", "y", "z" }) {
			auto seen = std::make_shared<outcome>();
			static_cast<void>(service.load({ .Key = key, .Fetch = fetch_request(std::string{ "https://camo.githubusercontent.com/" } + key) }, recorder(seen)));
			fetch.ok(fetch.Calls.size() - 1U);
			settle(decision, decodeLane);
		}
		expect(service.cached_bytes() == 800U && service.cached("x") == nullptr && service.cached("z") != nullptr, "least recently used image evicted at the bound");
		expect(service.cached("y") != nullptr, "recently used image kept");

		auto late = std::make_shared<outcome>();
		static_cast<void>(service.load({ .Key = "late", .Fetch = fetch_request("https://camo.githubusercontent.com/late") }, recorder(late)));
		service.shutdown();
		fetch.ok(fetch.Calls.size() - 1U);
		settle(decision, decodeLane);
		expect(late->Calls == 0 && service.quiescent() && !service.available(), "shutdown delivers nothing and settles");
	}

	auto test_node_of(const std::string_view aBody) -> std::optional<std::string>
	{
		if (aBody.empty()) {
			return std::string{ "PR_node" };
		}
		if (aBody.starts_with("issue-comment:")) {
			return std::string{ aBody.substr(14) };
		}
		if (aBody == "public-preview-comment:1") {
			return std::string{ };
		}
		return std::nullopt;
	}

	struct window_fixture
	{
		manual_executor                        Decision;
		manual_executor                        DecodeLane;
		fake_fetch                             Fetch;
		fake_decoder                           Decoder;
		fake_resolution                        Resolution;
		markdown_image_disk_cache_port*        Disk = nullptr;
		markdown_image_service                 Service{ Decision, DecodeLane, &Fetch, &Decoder, core::markdown_image_memory_cache_bytes, Disk };
		core::markdown_context                 Context{ { core::default_bpr_profile_id, core::bpr_window_id{ 2 }, core::bpr_window_generation{ 3 } }, core::application_session_id{ 4 }, core::github_identity_generation{ 5 }, core::provider_snapshot_id{ 6 }, 7 };
		std::string                            Scope     = "scope-1";
		bool                                   Available = true;
		core::markdown_image_fetch_policy      Policy{ .Contract = core::markdown_image_host_contract::github_com, .Hostname = "github.com", .CredentialAvailable = true };
		std::vector<markdown_image_projection> Published;
		std::unique_ptr<markdown_image_window> Window;

		explicit window_fixture(markdown_image_disk_cache_port* aDisk = nullptr)
			: Disk{ aDisk }
		{
			Window = std::make_unique<markdown_image_window>(
				Decision, &Service, &Resolution,
				[this] {
					auto facts       = markdown_image_window_facts{ };
					facts.Context    = Context;
					facts.Scope      = Scope;
					facts.Available  = Available;
					facts.Policy     = Policy;
					facts.Account    = 99U;
					facts.Resolution = markdown_image_resolution_request{ };
					return facts;
				},
				&test_node_of,
				[this](std::vector<markdown_image_projection> aImages) {
					for (auto& image : aImages) {
						Published.push_back(std::move(image));
					}
				});
		}

		auto intent(std::string aBody, std::vector<std::string> aSources) const -> core::markdown_image_intent
		{
			auto result  = core::markdown_image_intent{ .Context = Context, .Body = std::move(aBody), .References = { } };
			auto ordinal = std::uint32_t{ 0 };
			for (auto& source : aSources) {
				result.References.push_back({ ordinal++, std::move(source) });
			}
			return result;
		}

		void settle()
		{
			::settle(Decision, DecodeLane);
		}

		void resolve(const std::size_t aIndex, std::vector<markdown_image_resolved_body> aBodies)
		{
			Resolution.Calls[aIndex].Complete(markdown_image_resolution_success{ .Request = Resolution.Calls[aIndex].Request, .Bodies = std::move(aBodies), .RateLimits = { } });
			settle();
		}
	};

	constexpr auto attachment_address = "https://github.com/user-attachments/assets/25addea2-357a-42c0-a1c8-fba0163382cc";
	constexpr auto signed_served      = "https://private-user-images.githubusercontent.com/1/2-25addea2-357a-42c0-a1c8-fba0163382cc.png?jwt=SIGNED";

	void window_flow()
	{
		window_fixture fixture;
		fixture.Window->request(fixture.intent("", { std::string{ attachment_address }, "data:image/png;base64,AA", "https://e.example/unserved.png" }));
		fixture.Window->request(fixture.intent("issue-comment:IC_1", { "https://e.example/a.png" }));
		expect(fixture.Published.size() == 1U && fixture.Published[0].Ordinal == 1U && fixture.Published[0].State == core::markdown_image_state::blocked, "a blocked source is published at once");
		expect(fixture.Resolution.Calls.empty(), "resolution waits for the end of the turn");
		fixture.settle();
		expect(fixture.Resolution.Calls.size() == 1U && fixture.Resolution.Calls[0].Request.Nodes == std::vector<std::string>{ "PR_node", "IC_1" }, "bodies of one turn share one resolution");
		fixture.resolve(0, { { .Node = "PR_node", .Served = { { .Source = std::string{ signed_served }, .Canonical = { } } } }, { .Node = "IC_1", .Served = { { .Source = "https://camo.githubusercontent.com/aa", .Canonical = "https://e.example/a.png" } } } });
		expect(fixture.Fetch.Calls.size() == 2U, "matched images are fetched");
		expect(fixture.Fetch.Calls[0].Request.First.Address == std::string{ signed_served } && !fixture.Fetch.Calls[0].Request.First.Credential, "GitHub.com attachment fetched at its signed address without credential");
		expect(fixture.Fetch.Calls[1].Request.First.Address == "https://camo.githubusercontent.com/aa", "external image fetched through camo");
		const auto unserved = std::ranges::find(fixture.Published, 2U, &markdown_image_projection::Ordinal);
		expect(unserved != fixture.Published.end() && unserved->State == core::markdown_image_state::blocked, "a source GitHub does not serve is blocked");
		fixture.Fetch.ok(0);
		fixture.Fetch.ok(1);
		fixture.settle();
		const auto loaded = std::ranges::count(fixture.Published, core::markdown_image_state::loaded, &markdown_image_projection::State);
		expect(loaded == 2, "both images loaded");
		for (const auto& image : fixture.Published) {
			expect(image.Source.find("jwt") == std::string::npos && image.Source.find("camo") == std::string::npos, "the projection carries only the source as written");
		}
		const auto calls = fixture.Resolution.Calls.size();
		fixture.Published.clear();
		fixture.Window->request(fixture.intent("", { std::string{ attachment_address } }));
		expect(fixture.Published.size() == 1U && fixture.Published[0].State == core::markdown_image_state::loaded && fixture.Published[0].Image != nullptr, "re-rendering a cached image publishes at once");
		fixture.settle();
		expect(fixture.Resolution.Calls.size() == calls && fixture.Fetch.Calls.size() == 2U, "a cached image makes no request");
		expect(fixture.Window->quiescent() && fixture.Window->pending() == 0U, "window settles");
	}

	void window_refusals()
	{
		window_fixture fixture;
		auto           stale = fixture.intent("", { std::string{ attachment_address } });
		++stale.Context.Revision;
		fixture.Window->request(stale);
		fixture.Window->request(fixture.intent("issue-comment-missing", { std::string{ attachment_address } }));
		fixture.settle();
		expect(fixture.Published.empty() && fixture.Resolution.Calls.empty(), "stale and absent bodies publish and request nothing");

		fixture.Available = false;
		fixture.Window->request(fixture.intent("", { std::string{ attachment_address }, "https://e.example/b.png" }));
		expect(fixture.Published.size() == 2U && std::ranges::all_of(fixture.Published, [](const auto& aImage) { return aImage.State == core::markdown_image_state::blocked; }), "no image capability keeps every placeholder");
		fixture.Available = true;
		fixture.Published.clear();
		fixture.Window->request(fixture.intent("public-preview-comment:1", { std::string{ attachment_address } }));
		expect(fixture.Published.size() == 1U && fixture.Published[0].State == core::markdown_image_state::blocked, "a body without a node keeps its placeholder");

		fixture.Published.clear();
		fixture.Window->request(fixture.intent("", { std::string{ attachment_address } }));
		fixture.settle();
		fixture.Resolution.Calls[0].Complete(markdown_image_resolution_error{ .Request = fixture.Resolution.Calls[0].Request, .Error = { } });
		fixture.settle();
		expect(fixture.Published.size() == 1U && fixture.Published[0].State == core::markdown_image_state::failed, "a failed resolution fails its images");
	}

	void window_supersession()
	{
		window_fixture fixture;
		fixture.Window->request(fixture.intent("", { std::string{ attachment_address } }));
		fixture.settle();
		fixture.resolve(0, { { .Node = "PR_node", .Served = { { .Source = std::string{ signed_served }, .Canonical = { } } } } });
		expect(fixture.Fetch.Calls.size() == 1U, "fetch started");
		fixture.Window->request(fixture.intent("", { std::string{ attachment_address } }));
		fixture.settle();
		expect(!fixture.Fetch.Calls[0].Stop.stop_requested() && fixture.Fetch.Calls.size() == 1U && fixture.Resolution.Calls.size() == 1U,
			   "asking again for the same image keeps its load and asks nothing new");
		fixture.Window->request(fixture.intent("", { "https://e.example/new.png" }));
		expect(fixture.Fetch.Calls[0].Stop.stop_requested(), "a body's new request cancels its earlier load");
		fixture.Fetch.ok(0);
		fixture.settle();
		expect(std::ranges::none_of(fixture.Published, [](const auto& aImage) { return aImage.State == core::markdown_image_state::loaded; }), "the superseded image is never published");

		fixture.resolve(1, { { .Node = "PR_node", .Served = { { .Source = "https://camo.githubusercontent.com/n", .Canonical = "https://e.example/new.png" } } } });
		expect(fixture.Fetch.Calls.size() == 2U, "new image fetching");
		fixture.Scope = "scope-2";
		fixture.Window->request(fixture.intent("", { "https://e.example/other.png" }));
		expect(fixture.Fetch.Calls[1].Stop.stop_requested(), "a changed scope cancels earlier loads");
		fixture.Fetch.ok(1);
		fixture.settle();
		expect(std::ranges::none_of(fixture.Published, [](const auto& aImage) { return aImage.State == core::markdown_image_state::loaded; }), "an earlier scope's image never reaches the new one");
		const auto inflight = fixture.Resolution.Calls.size();
		fixture.Scope       = "scope-3";
		fixture.Window->request(fixture.intent("issue-comment:IC_9", { "https://e.example/z.png" }));
		expect(!fixture.Resolution.Cancelled.empty(), "a changed scope cancels its resolutions");
		fixture.Resolution.Calls[inflight - 1U].Complete(markdown_image_resolution_cancelled{ .Request = fixture.Resolution.Calls[inflight - 1U].Request });
		fixture.settle();
		fixture.Window->shutdown();
		expect(!fixture.Window->quiescent(), "a resolution still out keeps the window from settling");
		for (std::size_t index = inflight; index < fixture.Resolution.Calls.size(); ++index) {
			fixture.Resolution.Calls[index].Complete(markdown_image_resolution_cancelled{ .Request = fixture.Resolution.Calls[index].Request });
		}
		fixture.settle();
		expect(fixture.Window->quiescent(), "shutdown settles once completions return");
	}

	void window_bounds()
	{
		window_fixture fixture;
		for (int body = 0; body < 26; ++body) {
			std::vector<std::string> sources;
			sources.reserve(32U);
			for (int index = 0; index < (body < 5 ? 32 : 1); ++index) {
				sources.push_back("https://e.example/" + std::to_string(body) + "/" + std::to_string(index) + ".png");
			}
			fixture.Window->request(fixture.intent("issue-comment:IC_" + std::to_string(body), std::move(sources)));
		}
		const auto over = std::ranges::count(fixture.Published, core::markdown_image_state::over_limit, &markdown_image_projection::State);
		expect(fixture.Window->pending() == core::markdown_image_window_queue, "the window queue is bounded");
		expect(over == 5 * 32 + 21 - static_cast<long>(core::markdown_image_window_queue), "references beyond the queue are over the limit");
		fixture.settle();
		expect(fixture.Resolution.Calls.size() == 1U && fixture.Resolution.Calls[0].Request.Nodes.size() == 4U, "only bodies with waiting references are resolved");

		window_fixture many;
		for (int body = 0; body < 26; ++body) {
			many.Window->request(many.intent("issue-comment:IC_" + std::to_string(body), { "https://e.example/a.png" }));
		}
		many.settle();
		expect(many.Resolution.Calls.size() == 2U && many.Resolution.Calls[0].Request.Nodes.size() == core::markdown_image_bodies_per_resolution, "resolution is batched by the per-query bound");

		window_fixture ghes;
		ghes.Policy = { .Contract = core::markdown_image_host_contract::ghes, .Hostname = "ghe.example", .CredentialAvailable = true };
		ghes.Window->request(ghes.intent("", { "https://ghe.example/user-attachments/assets/25addea2-357a-42c0-a1c8-fba0163382cc" }));
		ghes.settle();
		ghes.resolve(0, { { .Node = "PR_node", .Served = { { .Source = "https://ghe.example/user-attachments/assets/25addea2-357a-42c0-a1c8-fba0163382cc", .Canonical = { } } } } });
		expect(ghes.Fetch.Calls.size() == 1U && ghes.Fetch.Calls[0].Request.First.Credential && ghes.Fetch.Calls[0].Request.Policy.Hostname == "ghe.example", "a GHES attachment on its own host carries the credential");
	}
	void window_restart_from_disk()
	{
		fake_disk disk;
		{
			window_fixture first{ &disk };
			first.Window->request(first.intent("", { std::string{ attachment_address }, "https://e.example/bad.png" }));
			expect(first.Published.empty(), "a disk probe never answers inline");
			first.settle();
			expect(disk.Loads == 2 && first.Resolution.Calls.size() == 1U, "two misses go on to one resolution");
			first.resolve(0, { { .Node = "PR_node", .Served = { { .Source = std::string{ signed_served }, .Canonical = { } }, { .Source = "https://camo.githubusercontent.com/b", .Canonical = "https://e.example/bad.png" } } } });
			first.Fetch.ok(0);
			first.Fetch.ok(1, "bad");
			first.settle();
			expect(std::ranges::count(first.Published, core::markdown_image_state::loaded, &markdown_image_projection::State) == 1, "the good image loaded");
			expect(disk.Stored.size() == 1U, "only bytes that decoded are stored");
			for (const auto& [key, bytes] : disk.Stored) {
				expect(key.find("jwt") == std::string::npos && key.find("private-user-images") == std::string::npos, "the disk key never holds a signed served address");
			}
			first.Window->shutdown();
			first.Service.shutdown();
			first.settle();
		}
		window_fixture second{ &disk };
		second.Window->request(second.intent("", { std::string{ attachment_address } }));
		second.settle();
		expect(second.Published.size() == 1U && second.Published[0].State == core::markdown_image_state::loaded && second.Published[0].Image != nullptr, "a restarted profile shows the cached image");
		expect(second.Resolution.Calls.empty() && second.Fetch.Calls.empty(), "and makes no request for it");
		expect(second.Service.cached_bytes() == 400U, "the disk hit joins the memory cache");
		second.Published.clear();
		second.Window->request(second.intent("issue-comment:IC_1", { std::string{ attachment_address } }));
		expect(second.Published.size() == 1U && second.Published[0].State == core::markdown_image_state::loaded, "after that the memory cache answers at once");

		window_fixture other{ &disk };
		other.Window->request(other.intent("", { std::string{ attachment_address } }));
		other.Window->request(other.intent("", { "https://e.example/next.png" }));
		other.settle();
		expect(std::ranges::none_of(other.Published, [](const auto& aImage) { return aImage.Ordinal == 0U && aImage.Source == attachment_address && aImage.State == core::markdown_image_state::loaded; }),
			   "a superseded probe publishes nothing");
		expect(other.Resolution.Calls.size() == 1U && other.Resolution.Calls[0].Request.Nodes == std::vector<std::string>{ "PR_node" }, "the new reference goes on to resolution");
		other.Resolution.Calls[0].Complete(markdown_image_resolution_cancelled{ .Request = other.Resolution.Calls[0].Request });
		other.settle();
		expect(other.Window->quiescent() && other.Service.quiescent(), "probes settle");
	}

	/// [[BPR-1807]]. One window's people's pictures, through the profile's image lane.
	struct avatar_fixture
	{
		manual_executor                         Decision;
		manual_executor                         DecodeLane;
		fake_fetch                              Fetch;
		fake_decoder                            Decoder;
		markdown_image_disk_cache_port*         Disk = nullptr;
		markdown_image_service                  Service{ Decision, DecodeLane, &Fetch, &Decoder, core::markdown_image_memory_cache_bytes, Disk };
		std::string                             Scope     = "scope-1";
		bool                                    Available = true;
		core::markdown_image_fetch_policy       Policy{ .Contract = core::markdown_image_host_contract::github_com, .Hostname = "github.com", .CredentialAvailable = true };
		std::vector<identity_avatar_projection> Published;
		std::unique_ptr<identity_avatar_window> Window;

		explicit avatar_fixture(markdown_image_disk_cache_port* aDisk = nullptr)
			: Disk{ aDisk }
		{
			Window = std::make_unique<identity_avatar_window>(
				Decision, &Service,
				[this] {
					auto facts      = markdown_image_window_facts{ };
					facts.Scope     = Scope;
					facts.Available = Available;
					facts.Policy    = Policy;
					facts.Account   = 99U;
					return facts;
				},
				[this](std::vector<identity_avatar_projection> aAvatars) {
					for (auto& avatar : aAvatars) {
						Published.push_back(std::move(avatar));
					}
				});
		}

		void request(std::vector<core::identity_avatar_intent> aIntents)
		{
			Window->request(aIntents);
		}

		void settle()
		{
			::settle(Decision, DecodeLane);
		}

		[[nodiscard]] auto published(const std::string& aLogin, const std::uint32_t aPixels) const -> const identity_avatar_projection*
		{
			const auto found = std::ranges::find_if(Published, [&](const auto& aAvatar) { return aAvatar.Login == aLogin && aAvatar.Pixels == aPixels; });
			return found == Published.end() ? nullptr : &*found;
		}
	};

	void avatar_flow()
	{
		avatar_fixture fixture;
		fixture.request({ { "MikeMarcin", 40U }, { "mikemarcin", 44U }, { "../etc/passwd", 40U }, { "octocat", 120U } });
		expect(fixture.Published.size() == 1U && fixture.Published[0].State == core::markdown_image_state::blocked, "a login that is not an account's is blocked at once");
		fixture.settle();
		expect(fixture.Fetch.Calls.size() == 2U, "the same person at the same size is fetched once; another person is fetched too");
		const auto& first = fixture.Fetch.Calls[0].Request;
		expect(first.First.Address == "https://avatars.githubusercontent.com/mikemarcin?s=48" && !first.First.Credential, "the avatar host, by login, without credential");
		expect(first.Policy.AvatarOrigins && first.MaximumBytes == core::identity_avatar_encoded_bytes && first.DeadlineMs == core::identity_avatar_fetch_deadline_ms,
			   "held to the avatar origins and the picture bounds");
		expect(fixture.Fetch.Calls[1].Request.First.Address == "https://avatars.githubusercontent.com/octocat?s=128", "a larger face asks a larger copy");
		fixture.Fetch.ok(0);
		fixture.Fetch.ok(1);
		fixture.settle();
		const auto* const mine = fixture.published("mikemarcin", 48U);
		expect(mine != nullptr && mine->State == core::markdown_image_state::loaded && mine->Image != nullptr, "a loaded picture is published with its pixels");
		expect(fixture.published("octocat", 128U) != nullptr, "under its folded login and size");
		fixture.Published.clear();
		fixture.request({ { "MikeMarcin", 48U } });
		expect(fixture.Published.size() == 1U && fixture.Published[0].State == core::markdown_image_state::loaded, "a held picture answers at once");
		fixture.settle();
		expect(fixture.Fetch.Calls.size() == 2U && fixture.Window->pending() == 0U, "and asks the host nothing");
	}

	void avatar_refusals_and_scope()
	{
		{
			avatar_fixture fixture;
			fixture.Available = false;
			fixture.request({ { "octocat", 40U } });
			fixture.settle();
			expect(fixture.Published.size() == 1U && fixture.Published[0].State == core::markdown_image_state::failed && fixture.Fetch.Calls.empty(),
				   "a window that cannot fetch keeps the initials and asks nothing");
		}
		{
			avatar_fixture fixture;
			fixture.request({ { "octocat", 40U } });
			fixture.settle();
			fixture.Fetch.Calls[0].Complete(std::unexpected{ core::markdown_image_state::failed });
			fixture.settle();
			expect(fixture.Published.size() == 1U && fixture.Published[0].State == core::markdown_image_state::failed, "a failed fetch is published as failed");
		}
		{
			avatar_fixture fixture;
			fixture.request({ { "octocat", 40U } });
			fixture.settle();
			fixture.Scope = "scope-2";
			fixture.request({ { "hubot", 40U } });
			expect(fixture.Fetch.Calls[0].Stop.stop_requested(), "a changed scope cancels the earlier fetch");
			fixture.Fetch.ok(0);
			fixture.settle();
			expect(fixture.published("octocat", 48U) == nullptr, "and nothing of the earlier scope is published");
		}
		{
			avatar_fixture fixture;
			auto           many = std::vector<core::identity_avatar_intent>{ };
			for (std::size_t index = 0U; index <= core::identity_avatar_window_queue; ++index) {
				many.push_back({ "person" + std::to_string(index), 40U });
			}
			fixture.request(many);
			expect(std::ranges::count(fixture.Published, core::markdown_image_state::over_limit, &identity_avatar_projection::State) == 1,
				   "past the window's queue a picture is over the limit");
			fixture.Window->shutdown();
			fixture.Service.shutdown();
			fixture.settle();
		}
		{
			avatar_fixture fixture;
			fixture.Policy = { .Contract = core::markdown_image_host_contract::ghes, .Hostname = "ghe.example.com", .CredentialAvailable = true };
			fixture.request({ { "octo", 40U } });
			fixture.settle();
			expect(fixture.Fetch.Calls.size() == 1U && fixture.Fetch.Calls[0].Request.First.Address == "https://ghe.example.com/octo.png?size=48"
					   && fixture.Fetch.Calls[0].Request.First.Credential,
				   "a GHES picture is asked of the host itself, with its credential");
			fixture.Fetch.ok(0, "bad");
			fixture.settle();
			expect(fixture.Published.size() == 1U && fixture.Published[0].State == core::markdown_image_state::unsupported && fixture.Published[0].Image == nullptr,
				   "a picture that does not decode keeps the initials");
		}
	}

	void avatar_restart_from_disk()
	{
		fake_disk disk;
		{
			avatar_fixture first{ &disk };
			first.request({ { "octocat", 40U } });
			first.settle();
			first.Fetch.ok(0);
			first.settle();
			expect(disk.Stored.size() == 1U, "a decoded picture is kept on disk");
			first.Window->shutdown();
			first.Service.shutdown();
			first.settle();
		}
		avatar_fixture second{ &disk };
		second.request({ { "OctoCat", 40U } });
		expect(second.Published.empty(), "a disk probe never answers inline");
		second.settle();
		expect(second.Published.size() == 1U && second.Published[0].State == core::markdown_image_state::loaded && second.Fetch.Calls.empty(),
			   "a restarted profile shows the picture without asking the host");
	}
} // namespace

int main()
{
	service_concurrency_and_sharing();
	service_cancellation_and_failures();
	window_flow();
	window_refusals();
	window_supersession();
	window_bounds();
	window_restart_from_disk();
	avatar_flow();
	avatar_refusals_and_scope();
	avatar_restart_from_disk();
	return EXIT_SUCCESS;
}
