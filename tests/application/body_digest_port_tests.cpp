#include "../support/test_expectation.hpp"
#include "bpr/application/body_digest_port.hpp"
#include "bpr/application/feedback_counters.hpp"
#include "bpr/application/feedback_sources.hpp"
#include "bpr/core/sha256.hpp"

#if defined(BPR_HAS_PLATFORM_BODY_DIGEST)
	#include "platform_body_digest.hpp"
#endif

#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1447]]. The body digest port (RFC-BPR-0080 AC02): every
/// backend gives the core's SHA-256 for the known answers, every boundary
/// length and seeded inputs, and a snapshot carrying the port's digests is
/// counted exactly as one the core hashes itself, with no body hashed by the
/// core; a missing or mismatched digest falls back to the core.

namespace {
	using namespace bpr;
	using application::body_digest;
	using application::feedback_counter_model;
	using application::github_pull_request_snapshot;
	using tests::expect;

	auto core_digest(std::string_view aBody) -> body_digest
	{
		return core::sha256({ reinterpret_cast<const std::uint8_t*>(aBody.data()), aBody.size() });
	}

	auto hex(const body_digest& aDigest) -> std::string
	{
		constexpr auto digits = std::string_view{ "0123456789abcdef" };
		auto           text   = std::string{ };
		for (const auto byte : aDigest) {
			text.push_back(digits[byte >> 4U]);
			text.push_back(digits[byte & 0x0FU]);
		}
		return text;
	}

	struct backend
	{
		const char*                                    Name = "";
		std::unique_ptr<application::body_digest_port> Port;
	};

	auto backends() -> std::vector<backend>
	{
		auto result = std::vector<backend>{ };
		result.push_back({ .Name = "core", .Port = std::make_unique<application::core_body_digest>() });
#if defined(BPR_HAS_PLATFORM_BODY_DIGEST)
		result.push_back({ .Name = "platform", .Port = std::make_unique<adapters::github::platform_body_digest>() });
#endif
		return result;
	}

	auto digest_all(const application::body_digest_port& aPort, const std::vector<std::string>& aBodies) -> std::vector<body_digest>
	{
		auto views = std::vector<std::string_view>{ aBodies.begin(), aBodies.end() };
		auto out   = std::vector<body_digest>(views.size());
		aPort.digest(views, out);
		return out;
	}

	void test_known_answers()
	{
		// FIPS 180-4 examples.
		const auto vectors = std::vector<std::pair<std::string, std::string_view>>{
			{ "", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
			{ "abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
			{ "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" },
			{ std::string(1'000'000U, 'a'), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0" },
		};
		auto bodies = std::vector<std::string>{ };
		for (const auto& vector : vectors) {
			bodies.push_back(vector.first);
		}
		for (const auto& [name, port] : backends()) {
			const auto digests = digest_all(*port, bodies);
			for (std::size_t index = 0U; index < vectors.size(); ++index) {
				expect(hex(digests[index]) == vectors[index].second, std::string{ name } + ": known answer " + std::to_string(index));
			}
		}
	}

	void test_boundaries_and_seeded_inputs()
	{
		auto bodies = std::vector<std::string>{ };
		// Every length across the first three blocks and their padding edges.
		for (std::size_t length = 0U; length <= 200U; ++length) {
			bodies.emplace_back(length, static_cast<char>('a' + (length % 26U)));
		}
		// A fixed seed: the same inputs on every run and station.
		auto random = std::mt19937_64{ 1447U }; // NOLINT(bugprone-random-generator-seed)
		for (const auto length : { 1023U, 1024U, 1025U, 65'535U, 65'536U, 262'144U, 1'048'576U, 1'048'577U }) {
			auto body = std::string(length, '\0');
			for (auto& byte : body) {
				byte = static_cast<char>(random() & 0xFFU);
			}
			bodies.push_back(std::move(body));
		}
		for (std::size_t count = 0U; count < 64U; ++count) {
			auto body = std::string(random() % 4096U, '\0');
			for (auto& byte : body) {
				byte = static_cast<char>(random() & 0xFFU);
			}
			bodies.push_back(std::move(body));
		}
		for (const auto& [name, port] : backends()) {
			const auto digests = digest_all(*port, bodies);
			for (std::size_t index = 0U; index < bodies.size(); ++index) {
				expect(digests[index] == core_digest(bodies[index]), std::string{ name } + ": the core's digest for body " + std::to_string(index));
			}
			auto refused = false;
			try {
				auto one = std::vector<body_digest>(1U);
				port->digest({ }, one);
			} catch (const std::invalid_argument&) {
				refused = true;
			}
			expect(refused, std::string{ name } + ": a digest list of another length is refused");
		}
	}

	auto actor(std::string aLogin) -> application::github_actor_record
	{
		return { .Actor = application::github_actor_id{ aLogin }, .Login = aLogin };
	}

	auto snapshot_of(std::size_t aThreads) -> github_pull_request_snapshot
	{
		static constexpr auto labels   = std::array<std::string_view, 6U>{ "issue (blocking): ", "nitpick: ", "question: ", "praise: ", "", "suggestion (non-blocking): " };
		auto                  shot     = github_pull_request_snapshot{ };
		shot.Viewer                    = actor("reviewer");
		shot.ReviewThreadsAvailability = application::github_collection_availability::available;
		for (std::size_t index = 0U; index < aThreads; ++index) {
			const auto id     = "t" + std::to_string(index);
			const auto path   = "src/m" + std::to_string(index % 7U) + "/f" + std::to_string(index % 13U) + ".cpp";
			auto       record = application::github_review_thread_record{ .Thread = application::github_thread_id{ id }, .Path = path, .Line = 3U, .Resolved = index % 3U == 0U };
			record.Comments.push_back({ .Comment = application::github_comment_id{ id + "-root" }, .Author = actor("reviewer"), .Body = std::string{ labels[index % labels.size()] } + "body " + std::to_string(index), .Path = path });
			record.Comments.push_back({ .Comment = application::github_comment_id{ id + "-reply" },
										.ReplyTo = application::github_comment_id{ id + "-root" },
										.Author  = actor("author"),
										.Body    = index % 2U == 0U ? "Fixed." : "praise: thanks",
										.Path    = path });
			shot.ReviewThreads.push_back(std::move(record));
		}
		shot.IssueComments.push_back({ .Comment = application::github_comment_id{ "i1" }, .Author = actor("author"), .Body = "todo: the changelog" });
		shot.Reviews.push_back({ .Review = application::github_review_id{ "v1" }, .Author = actor("reviewer"), .State = application::github_review_state::commented, .Body = "thought: the shape is right" });
		return shot;
	}

	auto key() -> core::cached_review_key
	{
		core::cached_review_key result;
		result.Authority[0] = 1U;
		result.Profile      = core::host_profile_id{ 1U };
		result.Host         = core::git_host_id{ 1U };
		result.Account      = core::github_account_id{ 1U };
		result.Locator      = { "octo", "repo" };
		result.Number       = core::pull_request_number{ 1447U };
		return result;
	}

	void expect_same_model(const feedback_counter_model& aSupplied, const feedback_counter_model& aHashed, const std::string& aWhat)
	{
		const auto& left  = aSupplied.population().Units;
		const auto& right = aHashed.population().Units;
		expect(left.size() == right.size(), aWhat + ": the same units");
		for (std::size_t index = 0U; index < left.size() && index < right.size(); ++index) {
			expect(left[index].Id == right[index].Id && left[index].Root == right[index].Root && left[index].Label == right[index].Label
					   && left[index].Resolution == right[index].Resolution && left[index].Anchor == right[index].Anchor,
				   aWhat + ": the same unit " + right[index].Id);
		}
		const auto scopes = aHashed.scopes();
		expect(aSupplied.scopes().size() == scopes.size(), aWhat + ": the same scopes");
		for (const auto& scope : scopes) {
			const auto l = aSupplied.tally_of(scope);
			const auto r = aHashed.tally_of(scope);
			expect(l.Valid == r.Valid && l.Collection == r.Collection && l.Labelled == r.Labelled && l.Plain == r.Plain && l.Custom == r.Custom
					   && l.Unclassified == r.Unclassified && l.Unattributed == r.Unattributed && l.AttributionUnknown == r.AttributionUnknown,
				   aWhat + ": the same counts for '" + scope.Path + "'");
		}
	}

	void test_supplied_digests_count_as_core_hashed()
	{
		for (const auto& [name, port] : backends()) {
			const auto what           = std::string{ name };
			auto       plain          = snapshot_of(40U);
			auto       given          = plain;
			given.FeedbackBodyDigests = application::digest_feedback_bodies(given, *port);
			expect(given == plain, what + ": the digests take no part in comparing snapshots");

			// Each post gets the core's digest of exactly its body.
			const auto sources = application::observe_feedback_sources(given);
			expect(!sources.Posts.empty(), what + ": posts");
			for (const auto& post : sources.Posts) {
				expect(post.BodyDigest && *post.BodyDigest == core_digest(post.Body), what + ": the core's digest for " + post.Id);
			}
			for (const auto& post : application::observe_feedback_sources(plain).Posts) {
				expect(!post.BodyDigest, what + ": no digest without the port");
			}

			auto       supplied = feedback_counter_model{ };
			auto       hashed   = feedback_counter_model{ };
			const auto a        = supplied.observe(key(), given);
			const auto b        = hashed.observe(key(), plain);
			expect(a.Admitted && b.Admitted && a.Reparsed == b.Reparsed && a.Changed == b.Changed, what + ": the same first observation");
			expect(supplied.work().BodiesHashed == 0U && supplied.work().BytesHashed == 0U && supplied.work().BodiesDigestSupplied == sources.Posts.size(),
				   what + ": the core hashed no body the snapshot carried a digest for");
			expect(hashed.work().BodiesHashed == sources.Posts.size() && hashed.work().BodiesDigestSupplied == 0U && hashed.work().BytesHashed > 0U,
				   what + ": without digests the core hashes every body");
			expect_same_model(supplied, hashed, what);

			// A body edited after digesting, to another length, is hashed by
			// the core; every other body is reused.
			given.ReviewThreads[0].Comments[0].Body = "question: a different length now";
			plain.ReviewThreads[0].Comments[0].Body = given.ReviewThreads[0].Comments[0].Body;
			const auto before                       = supplied.work();
			static_cast<void>(supplied.observe(key(), given));
			static_cast<void>(hashed.observe(key(), plain));
			expect(supplied.work().BodiesHashed - before.BodiesHashed == 1U && supplied.work().BodiesDigestSupplied == before.BodiesDigestSupplied,
				   what + ": a body whose length no longer matches its digest is hashed by the core");
			expect_same_model(supplied, hashed, what + " after an edit");

			// A digest list that does not line up with the posts is ignored.
			auto misaligned = snapshot_of(40U);
			auto entries    = std::vector<application::github_feedback_body_digest>{ *application::digest_feedback_bodies(misaligned, *port).Entries };
			entries.pop_back();
			misaligned.FeedbackBodyDigests.Entries = std::make_shared<const std::vector<application::github_feedback_body_digest>>(std::move(entries));
			auto fresh                             = feedback_counter_model{ };
			static_cast<void>(fresh.observe(key(), misaligned));
			expect(fresh.work().BodiesDigestSupplied == 0U && fresh.work().BodiesHashed == fresh.work().BodiesParsed, what + ": a short digest list is not used");

			// An entry named for another post is not used for this one.
			auto renamed = snapshot_of(3U);
			auto list    = std::vector<application::github_feedback_body_digest>{ *application::digest_feedback_bodies(renamed, *port).Entries };
			list[0].Post = "review-comment:other";
			list[1].Digest.fill(0U);
			renamed.FeedbackBodyDigests.Entries = std::make_shared<const std::vector<application::github_feedback_body_digest>>(std::move(list));
			const auto posts                    = application::observe_feedback_sources(renamed).Posts;
			expect(!posts[0].BodyDigest && posts[1].BodyDigest && posts[2].BodyDigest, what + ": an entry for another post is not used");
		}
	}

	auto same_annotation(const core::feedback_annotation& aLeft, const core::feedback_annotation& aRight) -> bool
	{
		return aLeft.BodyDigest == aRight.BodyDigest && aLeft.Status == aRight.Status && aLeft.Label == aRight.Label && aLeft.Guidance == aRight.Guidance
			&& aLeft.Diagnostics == aRight.Diagnostics && aLeft.InspectedBytes == aRight.InspectedBytes && aLeft.ParserRevision == aRight.ParserRevision
			&& aLeft.Decorations.size() == aRight.Decorations.size();
	}

	void test_annotations_carry_the_same_digest()
	{
		for (const auto* body : { "issue (blocking): leak", "plain words", "", "nitpick: a\nlonger\nbody" }) {
			const auto segment = core::feedback_body_segment{ };
			const auto hashed  = core::recognize_feedback_body(body, segment);
			const auto given   = core::recognize_feedback_body(body, segment, core_digest(body));
			expect(same_annotation(hashed, given), std::string{ "the same annotation for '" } + body + "'");
			const auto managed = core::feedback_body_segment{ .Managed = true };
			const auto hashedM = core::recognize_feedback_body(body, managed);
			const auto givenM  = core::recognize_feedback_body(body, managed, core_digest(body));
			expect(same_annotation(hashedM, givenM), std::string{ "the same unavailable annotation for '" } + body + "'");
		}
	}
} // namespace

auto main() -> int
try {
	test_known_answers();
	test_boundaries_and_seeded_inputs();
	test_annotations_carry_the_same_digest();
	test_supplied_digests_count_as_core_hashed();
#if defined(BPR_HAS_PLATFORM_BODY_DIGEST)
	std::cout << "body-digest-port backend=" << adapters::github::platform_body_digest_backend() << '\n';
#endif
	std::cout << "body-digest-port-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "body digest port failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
