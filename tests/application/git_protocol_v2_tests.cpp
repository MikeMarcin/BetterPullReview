#include "bpr/application/git_protocol_v2.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {
	using namespace bpr;

	static_assert(
		!std::is_copy_constructible_v<application::git_smart_transport_port>);
	static_assert(
		!std::is_move_constructible_v<application::git_smart_transport_port>);
	static_assert(
		!std::is_copy_constructible_v<
			application::git_smart_transport_factory_port>);
	static_assert(
		!std::is_move_constructible_v<
			application::git_smart_transport_factory_port>);

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32U> bytes{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xa5U);
		return Identity{ bytes };
	}

	[[nodiscard]] auto bytes(const std::string_view aValue)
		-> std::vector<std::byte>
	{
		return {
			reinterpret_cast<const std::byte*>(aValue.data()),
			reinterpret_cast<const std::byte*>(aValue.data() + aValue.size()),
		};
	}

	[[nodiscard]] auto text(const std::span<const std::byte> aValue) -> std::string
	{
		return {
			reinterpret_cast<const char*>(aValue.data()),
			aValue.size(),
		};
	}

	[[nodiscard]] auto packet(const std::string_view aPayload) -> std::string
	{
		constexpr std::string_view digits = "0123456789abcdef";
		const auto                 total  = aPayload.size() + 4U;
		std::array<char, 4U>       header{ };
		auto                       remaining = total;
		for (auto index = header.size(); index-- > 0U;) {
			header[index] = digits[remaining & 0x0fU];
			remaining >>= 4U;
		}
		return std::string{ header.data(), header.size() } + std::string{ aPayload };
	}

	[[nodiscard]] auto oid(
		const char                     aDigit,
		const core::git_hash_algorithm aFormat) -> core::git_commit_oid
	{
		const auto length = aFormat == core::git_hash_algorithm::sha1 ? 40U : 64U;
		const auto parsed = core::parse_git_commit_oid(std::string(length, aDigit));
		expect(parsed.has_value(), "test oid is canonical");
		return *parsed;
	}

	[[nodiscard]] auto authority() -> core::pull_request_authority
	{
		return {
			.Profile         = core::host_profile_id{ 11U },
			.ProfileRevision = core::github_profile_revision{ 12U },
			.Host            = core::git_host_id{ 13U },
			.HostAuthority   = opaque<core::host_authority_fingerprint>(14U),
			.Account         = core::github_account_id{ 15U },
			.Viewer          = core::github_viewer_actor_id{ 16U },
			.Repository      = core::repository_id{ 17U },
			.Cache           = opaque<core::repository_cache_scope_id>(18U),
			.PullRequest     = core::pull_request_id{ 19U },
			.Number          = core::pull_request_number{ 81U },
			.Scope           = opaque<core::pull_request_authority_scope_id>(20U),
		};
	}

	[[nodiscard]] auto context(
		const core::git_hash_algorithm aFormat = core::git_hash_algorithm::sha1)
		-> application::git_smart_request_context
	{
		const auto baseRef = application::parse_git_base_branch_ref("refs/heads/main");
		expect(baseRef.has_value(), "test base ref is valid");
		return {
			.ApplicationSession = core::application_session_id{ 1U },
			.Authority          = authority(),
			.Revision           = {
				.Source = {
					.Observation = opaque<
						core::epoch_source_observation_id>(22U),
					.Snapshot              = core::provider_snapshot_id{ 2U },
					.IdentityGeneration    = core::github_identity_generation{ 3U },
					.PullRequestGeneration = core::request_generation{ 4U },
				},
				.Origin     = core::pull_request_revision_origin::live,
				.BaseBranch = opaque<core::base_branch_id>(21U),
				.Base       = oid('1', aFormat),
				.Head       = oid('2', aFormat),
			},
			.Repository = { .Owner = "openai", .Name = "better-pull-review" },
			.BaseRef    = *baseRef,
		};
	}

	[[nodiscard]] auto capabilities(
		const core::git_hash_algorithm aFormat = core::git_hash_algorithm::sha1)
		-> application::detail::git_protocol_v2_capabilities
	{
		return {
			.ObjectFormat = aFormat,
			.LsRefs       = true,
			.Fetch        = true,
		};
	}

	[[nodiscard]] auto advertisement(
		const core::git_hash_algorithm aFormat) -> std::vector<std::byte>
	{
		const auto formatName = aFormat == core::git_hash_algorithm::sha1 ? "sha1" : "sha256";
		const auto body       = packet("version 2\n") + packet("ls-refs=unborn\n")
			+ packet("fetch=shallow wait-for-done\n")
			+ packet(std::string{ "object-format=" } + formatName + "\n")
			+ packet("server-option\n") + "0000";
		return bytes(body);
	}

	[[nodiscard]] auto refs_response(
		const application::git_smart_request_context& aRequestContext)
		-> std::vector<std::byte>
	{
		const auto base = std::string{ core::canonical_hex(aRequestContext.Revision.Base).view() };
		const auto head = std::string{ core::canonical_hex(aRequestContext.Revision.Head).view() };
		const auto body = packet(std::string{ base } + " refs/heads/main\n")
			+ packet(std::string{ head } + " refs/pull/81/head\n") + "0000";
		return bytes(body);
	}

	class recording_pack_sink final : public application::git_pack_stream_sink
	{
	public:
		explicit recording_pack_sink(const core::git_hash_algorithm aFormat)
			: Format{ aFormat }
		{
		}

		[[nodiscard]] auto append(
			const std::span<const std::byte> aInput,
			const std::stop_token            aStop)
			-> application::git_pack_stream_result<void> override
		{
			if (AppendFailure.has_value()) {
				return std::unexpected(*AppendFailure);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(
					application::git_pack_stream_failure::cancelled);
			}
			Received.insert(Received.end(), aInput.begin(), aInput.end());
			return { };
		}

		[[nodiscard]] auto finish(const std::stop_token aStop)
			-> application::git_pack_stream_result<
				application::completed_git_pack_receipt> override
		{
			if (FinishFailure.has_value()) {
				return std::unexpected(*FinishFailure);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(
					application::git_pack_stream_failure::cancelled);
			}
			Finished = true;
			application::git_pack_oid pack{
				.Algorithm = Format,
			};
			pack.Digest.front() = 0x42U;
			auto observed       = pack;
			if (MismatchedReceipt) {
				observed.Digest.front() = 0x43U;
			}
			auto token                  = std::array<std::uint8_t, 32U>{ };
			token.front()               = 0x71U;
			auto spoolIdentity          = application::git_artifact_identity{ };
			spoolIdentity.Bytes.front() = 1U;
			spoolIdentity.Bytes[1U]     = 1U;
			spoolIdentity.Bytes[2U]     = 0x7fU;
			spoolIdentity.Bytes.back()  = 0x51U;
			return application::completed_git_pack_receipt{
				.Operation    = opaque<core::epoch_operation_id>(41U),
				.ExpectedPack = pack,
				.ObservedPack = observed,
				.Spool        = {
					.Role = application::git_artifact_role::
						completed_spool,
					.Identity = spoolIdentity,
					.Bytes    = Received.size(),
				},
				.OpaqueToken = token,
			};
		}

		auto abort() noexcept -> void override
		{
			Aborted = true;
			++AbortCalls;
		}

		core::git_hash_algorithm                            Format;
		std::vector<std::byte>                              Received;
		std::optional<application::git_pack_stream_failure> AppendFailure;
		std::optional<application::git_pack_stream_failure> FinishFailure;
		bool                                                MismatchedReceipt = false;
		bool                                                Finished          = false;
		bool                                                Aborted           = false;
		std::size_t                                         AbortCalls        = 0U;
	};

	[[nodiscard]] auto fetch_response() -> std::vector<std::byte>
	{
		std::string progress(1U, static_cast<char>(2));
		progress += "counting\n";
		std::string packData(1U, static_cast<char>(1));
		packData += "PACKdata";
		return bytes(
			packet("packfile\n") + packet(progress) + packet(packData)
			+ "0000");
	}

	[[nodiscard]] constexpr auto fetch_limits()
		-> application::git_protocol_v2_fetch_limits
	{
		return {
			.MaximumWireBytes     = 4096U,
			.MaximumPackBytes     = 1024U,
			.MaximumProgressBytes = 128U,
			.MaximumPackets       = 16U,
		};
	}

	class scripted_transport final
		: public application::git_smart_transport_port
	{
	public:
		explicit scripted_transport(
			application::git_smart_request_context aBoundContext)
			: BoundContext{ std::move(aBoundContext) }
		{
			DiscoveryBody = advertisement(
				BoundContext.Revision.Base.algorithm());
			LsRefsBody = refs_response(BoundContext);
			FetchBody  = fetch_response();
		}

		auto revoke() noexcept -> void override
		{
			Revoked = true;
		}

		[[nodiscard]] auto revoked() const noexcept -> bool override
		{
			return Revoked;
		}

		[[nodiscard]] auto discover(
			const application::git_smart_discovery_request& aRequest,
			std::stop_token)
			-> application::git_smart_transport_result<
				application::git_smart_discovery_response> override
		{
			Calls.emplace_back("discover");
			if (Revoked || aRequest.Context != BoundContext) {
				return std::unexpected(application::git_smart_transport_error{
					.Failure = application::git_smart_transport_failure::invalid_request,
				});
			}
			if (DiscoveryFailure.has_value()) {
				return std::unexpected(application::git_smart_transport_error{
					.Failure = *DiscoveryFailure,
				});
			}
			return application::git_smart_discovery_response{
				.Body = DiscoveryBody,
			};
		}

		[[nodiscard]] auto ls_refs(
			const application::git_smart_ls_refs_request& aRequest,
			std::stop_token)
			-> application::git_smart_transport_result<
				application::git_smart_ls_refs_response> override
		{
			Calls.emplace_back("ls-refs");
			if (
				Revoked || aRequest.Context != BoundContext
				|| !application::valid(aRequest)) {
				return std::unexpected(application::git_smart_transport_error{
					.Failure = application::git_smart_transport_failure::invalid_request,
				});
			}
			CapturedLsRefsBody.assign(aRequest.Body.begin(), aRequest.Body.end());
			if (LsRefsFailure.has_value()) {
				return std::unexpected(application::git_smart_transport_error{
					.Failure = *LsRefsFailure,
				});
			}
			return application::git_smart_ls_refs_response{
				.Body = LsRefsBody,
			};
		}

		[[nodiscard]] auto fetch(
			const application::git_smart_fetch_request& aRequest,
			application::git_smart_wire_consumer&       aConsumer,
			std::stop_token)
			-> application::git_smart_transport_result<void> override
		{
			Calls.emplace_back("fetch");
			if (
				Revoked || aRequest.Context != BoundContext
				|| !application::valid(aRequest)) {
				return std::unexpected(application::git_smart_transport_error{
					.Failure = application::git_smart_transport_failure::invalid_request,
				});
			}
			CapturedFetchBody.assign(aRequest.Body.begin(), aRequest.Body.end());
			CapturedMaximumResponseBytes = aRequest.MaximumResponseBytes;
			if (FetchFailure.has_value()) {
				return std::unexpected(application::git_smart_transport_error{
					.Failure = *FetchFailure,
				});
			}

			const auto  chunkSize = std::max<std::size_t>(ChunkSize, 1U);
			std::size_t cursor    = 0U;
			while (cursor < FetchBody.size()) {
				const auto count = std::min(chunkSize, FetchBody.size() - cursor);
				if (!aConsumer.consume(
						std::span<const std::byte>{
							FetchBody.data() + cursor,
							count,
						})) {
					return std::unexpected(
						application::git_smart_transport_error{
							.Failure = application::git_smart_transport_failure::
								consumer_rejected,
						});
				}
				cursor += count;
			}
			return { };
		}

		application::git_smart_request_context BoundContext;
		std::vector<std::byte>                 DiscoveryBody;
		std::vector<std::byte>                 LsRefsBody;
		std::vector<std::byte>                 FetchBody;
		std::vector<std::byte>                 CapturedLsRefsBody;
		std::vector<std::byte>                 CapturedFetchBody;
		std::optional<application::git_smart_transport_failure>
																DiscoveryFailure;
		std::optional<application::git_smart_transport_failure> LsRefsFailure;
		std::optional<application::git_smart_transport_failure> FetchFailure;
		std::uint64_t                                           CapturedMaximumResponseBytes = 0U;
		std::size_t                                             ChunkSize                    = 1U;
		bool                                                    Revoked                      = false;
		std::vector<std::string_view>                           Calls;
	};

	auto test_base_ref_and_context_validation() -> void
	{
		expect(
			application::parse_git_base_branch_ref("refs/heads/topic/name")
				.has_value(),
			"a canonical full heads ref is accepted");
		for (const auto invalid : {
				 "main",
				 "refs/heads/",
				 "refs/heads/.hidden",
				 "refs/heads/topic..name",
				 "refs/heads/topic.lock",
				 "refs/heads/topic name",
				 "refs/heads/topic@{one",
			 }) {
			expect(
				!application::parse_git_base_branch_ref(invalid).has_value(),
				"noncanonical base ref is rejected");
		}

		auto validContext = context();
		expect(application::valid(validContext), "complete context is valid");
		validContext.ApplicationSession = { };
		expect(
			!application::valid(validContext),
			"zero application session invalidates context");
	}

	auto test_http_advertisement_preamble() -> void
	{
		const auto preamble = packet("# service=git-upload-pack\n") + "0000";
		for (const auto format : { core::git_hash_algorithm::sha1,
								   core::git_hash_algorithm::sha256 }) {
			const auto body    = advertisement(format);
			const auto wrapped = bytes(preamble + text(body));
			const auto direct  = application::detail::parse_git_protocol_v2_advertisement(body, format);
			const auto parsed  = application::detail::parse_git_protocol_v2_advertisement(wrapped, format);
			expect(direct.has_value() && parsed.has_value()
					   && parsed->ObjectFormat == direct->ObjectFormat
					   && parsed->LsRefs == direct->LsRefs && parsed->Fetch == direct->Fetch,
				   "HTTP preamble preserves required v2 capabilities and object format");
		}
		const auto body             = text(advertisement(core::git_hash_algorithm::sha1));
		auto       repeatedPreamble = preamble;
		repeatedPreamble += preamble;
		repeatedPreamble += body;
		for (const auto& invalid : {
				 packet("# service=git-receive-pack\n") + "0000" + body,
				 packet("# service=git-upload-pack\n") + body,
				 packet("# service=git-upload-pack\n") + "0001" + body,
				 packet("# service=git-upload-pack\r\n") + "0000" + body,
				 repeatedPreamble,
				 preamble,
				 preamble + packet("version 1\n") + "0000",
				 preamble + packet("version 2\n") + "0000",
				 preamble + body + "0000",
				 std::string{ "001f# service=git-upload-pack\n0000" } + body,
			 }) {
			expect(!application::detail::parse_git_protocol_v2_advertisement(
						bytes(invalid), core::git_hash_algorithm::sha1)
						.has_value(),
				   "invalid HTTP framing cannot bypass strict v2 admission");
		}
		const auto mismatched = bytes(preamble + text(advertisement(core::git_hash_algorithm::sha256)));
		expect(!application::detail::parse_git_protocol_v2_advertisement(
					mismatched, core::git_hash_algorithm::sha1)
					.has_value(),
			   "HTTP framing cannot bypass object-format matching");
	}

	auto test_advertisement_and_requests() -> void
	{
		for (const auto format : {
				 core::git_hash_algorithm::sha1,
				 core::git_hash_algorithm::sha256,
			 }) {
			const auto parsed = application::detail::parse_git_protocol_v2_advertisement(
				advertisement(format),
				format);
			expect(parsed.has_value(), "strict v2 advertisement is accepted");
			expect(
				parsed->ObjectFormat == format && parsed->LsRefs
					&& parsed->Fetch,
				"required capabilities and object format are retained");
		}

		const auto mismatched = application::detail::parse_git_protocol_v2_advertisement(
			advertisement(core::git_hash_algorithm::sha256),
			core::git_hash_algorithm::sha1);
		expect(
			!mismatched.has_value()
				&& mismatched.error().Failure
					== application::git_protocol_v2_failure::
						object_format_mismatch,
			"object-format mismatch is rejected");

		const auto missingFetch = bytes(
			packet("version 2\n") + packet("ls-refs\n")
			+ packet("object-format=sha1\n") + "0000");
		const auto missingFetchResult = application::detail::parse_git_protocol_v2_advertisement(
			missingFetch,
			core::git_hash_algorithm::sha1);
		expect(
			!missingFetchResult.has_value()
				&& missingFetchResult.error().Failure
					== application::git_protocol_v2_failure::
						missing_capability,
			"a missing required command capability is rejected");

		const auto missingFormat = bytes(
			packet("version 2\n") + packet("ls-refs\n")
			+ packet("fetch\n") + "0000");
		const auto missingFormatResult = application::detail::parse_git_protocol_v2_advertisement(
			missingFormat,
			core::git_hash_algorithm::sha1);
		expect(
			!missingFormatResult.has_value()
				&& missingFormatResult.error().Failure
					== application::git_protocol_v2_failure::
						missing_capability,
			"a missing object-format capability is rejected");

		const auto duplicate = bytes(
			packet("version 2\n") + packet("ls-refs\n")
			+ packet("fetch\n") + packet("fetch=shallow\n")
			+ packet("object-format=sha1\n") + "0000");
		const auto duplicateResult = application::detail::parse_git_protocol_v2_advertisement(
			duplicate,
			core::git_hash_algorithm::sha1);
		expect(
			!duplicateResult.has_value()
				&& duplicateResult.error().Failure
					== application::git_protocol_v2_failure::malformed_packet,
			"a duplicate capability key is rejected");

		const auto malformed = bytes(
			packet("version 2\n") + packet("ls-refs\n")
			+ packet("fetch\n") + packet("bad key=value\n")
			+ packet("object-format=sha1\n") + "0000");
		const auto malformedResult = application::detail::parse_git_protocol_v2_advertisement(
			malformed,
			core::git_hash_algorithm::sha1);
		expect(
			!malformedResult.has_value()
				&& malformedResult.error().Failure
					== application::git_protocol_v2_failure::malformed_packet,
			"a malformed capability line is rejected");

		const auto malformedValue = bytes(
			packet("version 2\n") + packet("ls-refs\n")
			+ packet("fetch\n") + packet("extension=bad|value\n")
			+ packet("object-format=sha1\n") + "0000");
		expect(
			!application::detail::parse_git_protocol_v2_advertisement(
				 malformedValue,
				 core::git_hash_algorithm::sha1)
				 .has_value(),
			"unknown capabilities with out-of-grammar values are rejected");

		const auto noLf = bytes(
			packet("version 2") + packet("ls-refs")
			+ packet("fetch=shallow wait-for-done")
			+ packet("object-format=sha1") + "0000");
		expect(
			application::detail::parse_git_protocol_v2_advertisement(
				noLf,
				core::git_hash_algorithm::sha1)
				.has_value(),
			"non-binary pkt-lines are accepted without their optional LF");

		const auto carriageReturn = bytes(
			packet("version 2\n") + packet("ls-refs\r\n")
			+ packet("fetch\n") + packet("object-format=sha1\n") + "0000");
		expect(
			!application::detail::parse_git_protocol_v2_advertisement(
				 carriageReturn,
				 core::git_hash_algorithm::sha1)
				 .has_value(),
			"carriage returns remain invalid when the optional LF is normalized");

		auto nulCapability = std::string{ "fetch" };
		nulCapability.push_back('\0');
		nulCapability += "ignored\n";
		const auto embeddedNul = bytes(
			packet("version 2\n") + packet("ls-refs\n")
			+ packet(nulCapability) + packet("object-format=sha1\n") + "0000");
		expect(
			!application::detail::parse_git_protocol_v2_advertisement(
				 embeddedNul,
				 core::git_hash_algorithm::sha1)
				 .has_value(),
			"embedded NUL remains invalid when optional LF is normalized");

		const auto requestContext = context();
		const auto caps           = capabilities();
		const auto lsRequest      = application::detail::make_git_protocol_v2_ls_refs_request(
			requestContext,
			caps);
		const auto expectedLs = packet("command=ls-refs\n") + packet("object-format=sha1\n")
			+ "0001" + packet("ref-prefix refs/heads/main\n")
			+ packet("ref-prefix refs/pull/81/head\n") + "0000";
		expect(
			lsRequest.has_value() && text(*lsRequest) == expectedLs,
			"ls-refs request has only the exact command, format, and two refs");

		const auto       haveOid = oid('3', core::git_hash_algorithm::sha1);
		const std::array haves{
			application::git_protocol_v2_have{
				.Cache          = requestContext.Authority.Cache,
				.Commit         = haveOid,
				.CatalogVisible = true,
				.ObjectVerified = true,
			},
			application::git_protocol_v2_have{
				.Cache          = requestContext.Authority.Cache,
				.Commit         = haveOid,
				.CatalogVisible = true,
				.ObjectVerified = true,
			},
		};
		const auto fetchRequest = application::detail::make_git_protocol_v2_fetch_request(
			requestContext,
			caps,
			haves);
		const auto base          = std::string{ core::canonical_hex(requestContext.Revision.Base).view() };
		const auto head          = std::string{ core::canonical_hex(requestContext.Revision.Head).view() };
		const auto have          = std::string{ core::canonical_hex(haveOid).view() };
		const auto expectedFetch = packet("command=fetch\n") + packet("object-format=sha1\n")
			+ "0001" + packet(std::string{ "want " } + std::string{ base } + "\n")
			+ packet(std::string{ "want " } + std::string{ head } + "\n")
			+ packet(std::string{ "have " } + std::string{ have } + "\n")
			+ packet("done\n") + "0000";
		expect(
			fetchRequest.has_value() && text(*fetchRequest) == expectedFetch,
			"fetch request deduplicates verified same-cache haves");
		expect(
			text(*fetchRequest).find("thin-pack") == std::string::npos
				&& text(*fetchRequest).find("sideband-all") == std::string::npos
				&& text(*fetchRequest).find("filter") == std::string::npos,
			"fetch request omits alternate response capabilities");

		auto wrongCache          = haves;
		wrongCache.front().Cache = opaque<core::repository_cache_scope_id>(99U);
		expect(
			!application::detail::make_git_protocol_v2_fetch_request(
				 requestContext,
				 caps,
				 wrongCache)
				 .has_value(),
			"a have from another cache is rejected");
		auto unverified                   = haves;
		unverified.front().ObjectVerified = false;
		expect(
			!application::detail::make_git_protocol_v2_fetch_request(
				 requestContext,
				 caps,
				 unverified)
				 .has_value(),
			"an unverified have is rejected");
		const std::vector excessiveHaves(
			application::maximum_git_protocol_v2_haves + 1U,
			haves.front());
		expect(
			!application::detail::make_git_protocol_v2_fetch_request(
				 requestContext,
				 caps,
				 excessiveHaves)
				 .has_value(),
			"the candidate have set is bounded before deduplication");

		const auto sha256Context = context(core::git_hash_algorithm::sha256);
		const auto sha256Request = application::detail::make_git_protocol_v2_fetch_request(
			sha256Context,
			capabilities(core::git_hash_algorithm::sha256),
			{ });
		expect(
			sha256Request.has_value()
				&& text(*sha256Request).find("object-format=sha256\n")
					!= std::string::npos
				&& text(*sha256Request).find(std::string{ core::canonical_hex(sha256Context.Revision.Base).view() })
					!= std::string::npos,
			"SHA-256 requests echo SHA-256 and render full 64-digit wants");

		auto identicalWants          = requestContext;
		identicalWants.Revision.Head = identicalWants.Revision.Base;
		const auto deduplicatedWants = application::detail::make_git_protocol_v2_fetch_request(
			identicalWants,
			caps,
			{ });
		expect(deduplicatedWants.has_value(), "equal accepted wants are valid");
		const auto wantsText = text(*deduplicatedWants);
		const auto firstWant = wantsText.find("want ");
		expect(
			firstWant != std::string::npos
				&& wantsText.find("want ", firstWant + 1U)
					== std::string::npos,
			"equal base and head wants are emitted once");
	}

	auto test_ls_refs_strict_identity() -> void
	{
		const auto requestContext = context();
		const auto parsed         = application::detail::parse_git_protocol_v2_ls_refs(
			refs_response(requestContext),
			requestContext);
		expect(
			parsed.has_value()
				&& parsed->Base == requestContext.Revision.Base
				&& parsed->Head == requestContext.Revision.Head,
			"ls-refs retains the exact accepted base and PR head");

		const auto base  = std::string{ core::canonical_hex(requestContext.Revision.Base).view() };
		const auto head  = std::string{ core::canonical_hex(requestContext.Revision.Head).view() };
		const auto extra = bytes(
			packet(std::string{ base } + " refs/heads/main\n")
			+ packet(std::string{ head } + " refs/pull/81/head\n")
			+ packet(std::string{ head } + " refs/heads/other\n") + "0000");
		const auto rejected = application::detail::parse_git_protocol_v2_ls_refs(
			extra,
			requestContext);
		expect(
			!rejected.has_value()
				&& rejected.error().Failure
					== application::git_protocol_v2_failure::unexpected_ref,
			"an unrequested advertised ref is rejected");

		const auto attributed = bytes(
			packet(std::string{ base } + " refs/heads/main peeled:"
				   + std::string{ base } + "\n")
			+ packet(std::string{ head } + " refs/pull/81/head\n") + "0000");
		expect(
			!application::detail::parse_git_protocol_v2_ls_refs(
				 attributed,
				 requestContext)
				 .has_value(),
			"unrequested ls-refs attributes are rejected");

		const auto duplicate = bytes(
			packet(std::string{ base } + " refs/heads/main\n")
			+ packet(std::string{ base } + " refs/heads/main\n")
			+ packet(std::string{ head } + " refs/pull/81/head\n") + "0000");
		const auto duplicateResult = application::detail::parse_git_protocol_v2_ls_refs(
			duplicate,
			requestContext);
		expect(
			!duplicateResult.has_value()
				&& duplicateResult.error().Failure
					== application::git_protocol_v2_failure::duplicate_ref,
			"a duplicate requested ref is rejected");

		const auto missing = bytes(
			packet(std::string{ base } + " refs/heads/main\n") + "0000");
		const auto missingResult = application::detail::parse_git_protocol_v2_ls_refs(
			missing,
			requestContext);
		expect(
			!missingResult.has_value()
				&& missingResult.error().Failure
					== application::git_protocol_v2_failure::
						incomplete_response,
			"a missing requested ref is rejected");

		const auto wrongOid = std::string{
			core::canonical_hex(
				oid('3', core::git_hash_algorithm::sha1))
				.view()
		};
		// [[BPR-0667]]. The base branch has moved on, as it does whenever
		// anything else merges; the advertisement says where it is now and the
		// pull request keeps the base it recorded. Refusing this made the
		// review history unavailable for nearly every real pull request.
		const auto movedBase = bytes(
			packet(std::string{ wrongOid } + " refs/heads/main\n")
			+ packet(std::string{ head } + " refs/pull/81/head\n") + "0000");
		const auto movedBaseResult = application::detail::parse_git_protocol_v2_ls_refs(
			movedBase,
			requestContext);
		expect(
			movedBaseResult.has_value()
				&& movedBaseResult->Base
					== oid('3', core::git_hash_algorithm::sha1)
				&& movedBaseResult->Head == requestContext.Revision.Head,
			"a base branch that has moved is reported where it is, not refused");

		// The pull request's own head never moves. Another oid there is a
		// different pull request, or a server substituting one.
		const auto substitutedHead = bytes(
			packet(std::string{ base } + " refs/heads/main\n")
			+ packet(std::string{ wrongOid } + " refs/pull/81/head\n") + "0000");
		const auto substitutedResult = application::detail::parse_git_protocol_v2_ls_refs(
			substitutedHead,
			requestContext);
		expect(
			!substitutedResult.has_value()
				&& substitutedResult.error().Failure
					== application::git_protocol_v2_failure::oid_mismatch,
			"a pull request head advertised at another oid is refused");

		const auto noLf = bytes(
			packet(std::string{ base } + " refs/heads/main")
			+ packet(std::string{ head } + " refs/pull/81/head") + "0000");
		expect(
			application::detail::parse_git_protocol_v2_ls_refs(
				noLf,
				requestContext)
				.has_value(),
			"ls-refs text packets are accepted without their optional LF");
	}

	auto test_incremental_fetch_parser() -> void
	{
		recording_pack_sink                               sink{ core::git_hash_algorithm::sha1 };
		application::detail::git_protocol_v2_fetch_parser parser{
			sink,
			fetch_limits(),
			core::git_hash_algorithm::sha1,
			{ },
		};
		const auto  response = fetch_response();
		std::size_t offset   = 0U;
		for (const auto byte : response) {
			if (!parser.consume(std::span<const std::byte>{ &byte, 1U })) {
				std::cerr << "fetch parser failed at byte " << offset
						  << " with error "
						  << static_cast<int>(parser.error()->Failure) << '\n';
				expect(false, "every one-byte raw transport chunk is consumed");
			}
			++offset;
		}
		const auto result = parser.finish();
		expect(result.has_value(), "complete sideband response finishes");
		expect(
			text(sink.Received) == "PACKdata",
			"only sideband channel one reaches the pack sink");
		expect(
			result->PackBytes == 8U && result->ProgressBytes == 9U
				&& result->WireBytes == response.size()
				&& result->Packets == 4U && sink.Finished && !sink.Aborted,
			"bounded parser reports exact retained counters");

		recording_pack_sink                               noLfSink{ core::git_hash_algorithm::sha1 };
		application::detail::git_protocol_v2_fetch_parser noLfParser{
			noLfSink,
			fetch_limits(),
			core::git_hash_algorithm::sha1,
			{ },
		};
		std::string packData(1U, static_cast<char>(1));
		packData += "PACKdata";
		const auto noLfResponse = bytes(packet("packfile") + packet(packData) + "0000");
		for (const auto byte : noLfResponse) {
			expect(
				noLfParser.consume(
					std::span<const std::byte>{ &byte, 1U }),
				"a no-LF section remains valid across one-byte fragments");
		}
		expect(
			noLfParser.finish().has_value()
				&& text(noLfSink.Received) == "PACKdata"
				&& noLfSink.AbortCalls == 0U,
			"the packfile section header accepts an omitted optional LF");
	}

	auto test_fetch_parser_rejections() -> void
	{
		{
			recording_pack_sink                               sink{ core::git_hash_algorithm::sha1 };
			application::detail::git_protocol_v2_fetch_parser parser{
				sink,
				fetch_limits(),
				core::git_hash_algorithm::sha1,
				{ },
			};
			const auto response = bytes(
				packet("acknowledgments\n") + packet("NAK\n") + "0000");
			expect(
				!parser.consume(response)
					&& parser.error()->Failure
						== application::git_protocol_v2_failure::
							unexpected_section
					&& sink.Aborted,
				"an acknowledgments section is rejected before pack input");
		}
		{
			recording_pack_sink                               sink{ core::git_hash_algorithm::sha1 };
			application::detail::git_protocol_v2_fetch_parser parser{
				sink,
				fetch_limits(),
				core::git_hash_algorithm::sha1,
				{ },
			};
			std::string fatal(1U, static_cast<char>(3));
			fatal += "server failure\n";
			const auto response = bytes(packet("packfile\n") + packet(fatal) + "0000");
			expect(
				!parser.consume(response)
					&& parser.error()->Failure
						== application::git_protocol_v2_failure::sideband_error
					&& sink.Aborted,
				"sideband fatal channel aborts the pack sink");
		}
		{
			recording_pack_sink                               sink{ core::git_hash_algorithm::sha1 };
			application::detail::git_protocol_v2_fetch_parser parser{
				sink,
				fetch_limits(),
				core::git_hash_algorithm::sha1,
				{ },
			};
			auto response = fetch_response();
			response.resize(response.size() - 4U);
			expect(parser.consume(response), "unterminated body can be streamed");
			const auto result = parser.finish();
			expect(
				!result.has_value()
					&& result.error().Failure
						== application::git_protocol_v2_failure::
							incomplete_response
					&& sink.Aborted,
				"missing terminal flush is rejected");
		}
	}

	auto test_fetch_limits_and_framing() -> void
	{
		const auto response = fetch_response();
		{
			auto limits             = fetch_limits();
			limits.MaximumPackBytes = 4U;
			recording_pack_sink                               sink{ core::git_hash_algorithm::sha1 };
			application::detail::git_protocol_v2_fetch_parser parser{
				sink,
				limits,
				core::git_hash_algorithm::sha1,
				{ },
			};
			expect(
				!parser.consume(response)
					&& parser.error()->Failure
						== application::git_protocol_v2_failure::pack_limit
					&& sink.Aborted,
				"pack bytes are bounded before the sink accepts excess data");
		}
		{
			auto limits                 = fetch_limits();
			limits.MaximumProgressBytes = 4U;
			recording_pack_sink                               sink{ core::git_hash_algorithm::sha1 };
			application::detail::git_protocol_v2_fetch_parser parser{
				sink,
				limits,
				core::git_hash_algorithm::sha1,
				{ },
			};
			expect(
				!parser.consume(response)
					&& parser.error()->Failure
						== application::git_protocol_v2_failure::progress_limit
					&& sink.Aborted,
				"progress bytes are independently bounded");
		}
		{
			auto limits             = fetch_limits();
			limits.MaximumWireBytes = response.size() - 1U;
			recording_pack_sink                               sink{ core::git_hash_algorithm::sha1 };
			application::detail::git_protocol_v2_fetch_parser parser{
				sink,
				limits,
				core::git_hash_algorithm::sha1,
				{ },
			};
			expect(
				!parser.consume(response)
					&& parser.error()->Failure
						== application::git_protocol_v2_failure::response_limit
					&& sink.Aborted,
				"raw response bytes are bounded before parsing");
		}
		{
			auto limits           = fetch_limits();
			limits.MaximumPackets = 2U;
			recording_pack_sink                               sink{ core::git_hash_algorithm::sha1 };
			application::detail::git_protocol_v2_fetch_parser parser{
				sink,
				limits,
				core::git_hash_algorithm::sha1,
				{ },
			};
			expect(
				!parser.consume(response)
					&& parser.error()->Failure
						== application::git_protocol_v2_failure::response_limit
					&& sink.Aborted,
				"pkt-line count is bounded");
		}
		{
			std::string invalidChannel(1U, static_cast<char>(4));
			invalidChannel += "data";
			const auto body = bytes(
				packet("packfile\n") + packet(invalidChannel) + "0000");
			recording_pack_sink                               sink{ core::git_hash_algorithm::sha1 };
			application::detail::git_protocol_v2_fetch_parser parser{
				sink,
				fetch_limits(),
				core::git_hash_algorithm::sha1,
				{ },
			};
			expect(
				!parser.consume(body)
					&& parser.error()->Failure
						== application::git_protocol_v2_failure::
							malformed_packet
					&& sink.Aborted,
				"an unknown sideband channel is rejected");
		}
		for (const auto framing : { "zzzz", "0003", "ffff" }) {
			const auto                                        body = bytes(framing);
			recording_pack_sink                               sink{ core::git_hash_algorithm::sha1 };
			application::detail::git_protocol_v2_fetch_parser parser{
				sink,
				fetch_limits(),
				core::git_hash_algorithm::sha1,
				{ },
			};
			expect(
				!parser.consume(body)
					&& parser.error()->Failure
						== application::git_protocol_v2_failure::
							malformed_packet
					&& sink.Aborted,
				"invalid pkt-line framing is rejected");
		}
	}

	auto test_pack_sink_failures_and_cancellation() -> void
	{
		const auto response = fetch_response();
		{
			recording_pack_sink sink{ core::git_hash_algorithm::sha1 };
			sink.AppendFailure = application::git_pack_stream_failure::storage;
			application::detail::git_protocol_v2_fetch_parser parser{
				sink,
				fetch_limits(),
				core::git_hash_algorithm::sha1,
				{ },
			};
			expect(
				!parser.consume(response) && parser.error()->PackFailure.has_value()
					&& *parser.error()->PackFailure
						== application::git_pack_stream_failure::storage
					&& sink.Aborted,
				"pack append failure is retained and aborts the sink");
		}
		{
			recording_pack_sink sink{ core::git_hash_algorithm::sha1 };
			sink.FinishFailure = application::git_pack_stream_failure::malformed_pack;
			application::detail::git_protocol_v2_fetch_parser parser{
				sink,
				fetch_limits(),
				core::git_hash_algorithm::sha1,
				{ },
			};
			expect(parser.consume(response), "pack reaches finish boundary");
			const auto result = parser.finish();
			expect(
				!result.has_value() && result.error().PackFailure.has_value()
					&& *result.error().PackFailure
						== application::git_pack_stream_failure::malformed_pack
					&& sink.Aborted,
				"pack finish failure is retained and aborts the sink");
		}
		{
			recording_pack_sink sink{ core::git_hash_algorithm::sha1 };
			sink.MismatchedReceipt = true;
			application::detail::git_protocol_v2_fetch_parser parser{
				sink,
				fetch_limits(),
				core::git_hash_algorithm::sha1,
				{ },
			};
			expect(parser.consume(response), "mismatched receipt reaches finish");
			const auto result = parser.finish();
			expect(
				!result.has_value() && result.error().PackFailure.has_value()
					&& *result.error().PackFailure
						== application::git_pack_stream_failure::invalid_state
					&& sink.Aborted,
				"mismatched expected and observed pack receipt is rejected");
		}
		{
			std::stop_source cancellation;
			cancellation.request_stop();
			recording_pack_sink                               sink{ core::git_hash_algorithm::sha1 };
			application::detail::git_protocol_v2_fetch_parser parser{
				sink,
				fetch_limits(),
				core::git_hash_algorithm::sha1,
				cancellation.get_token(),
			};
			expect(
				!parser.consume(response) && parser.error()->PackFailure.has_value()
					&& *parser.error()->PackFailure
						== application::git_pack_stream_failure::cancelled
					&& sink.Aborted,
				"stop cancellation is propagated by the pack sink and aborts");
		}
	}

	auto test_end_to_end_acquisition_and_error_mapping() -> void
	{
		const auto         requestContext = context(core::git_hash_algorithm::sha256);
		scripted_transport transport{ requestContext };
		transport.ChunkSize = 1U;

		recording_pack_sink sink{ core::git_hash_algorithm::sha256 };
		const auto          acquired = application::acquire_git_protocol_v2_pack(
			transport,
			requestContext,
			{ },
			sink,
			fetch_limits(),
			{ });
		expect(
			acquired.has_value()
				&& acquired->AdvertisedRefs.Base
					== requestContext.Revision.Base
				&& acquired->AdvertisedRefs.Head
					== requestContext.Revision.Head
				&& text(sink.Received) == "PACKdata"
				&& text(transport.CapturedLsRefsBody).find("object-format=sha256\n")
					!= std::string::npos
				&& transport.CapturedMaximumResponseBytes
					== fetch_limits().MaximumWireBytes
				&& text(transport.CapturedFetchBody).find("object-format=sha256\n")
					!= std::string::npos
				&& sink.AbortCalls == 0U,
			"one public acquisition validates refs before streaming SHA-256");
		expect(
			transport.Calls
				== std::vector<std::string_view>{
					"discover",
					"ls-refs",
					"fetch",
				},
			"the public operation cannot skip discovery or exact refs");

		{
			scripted_transport  historical{ requestContext };
			recording_pack_sink historicalSink{ core::git_hash_algorithm::sha256 };
			const auto          historicalHead = core::parse_git_commit_oid(std::string(64U, 'e'));
			expect(historicalHead.has_value(), "historical fixture OID is valid");
			const auto result = application::acquire_git_protocol_v2_historical_pack(
				historical, requestContext, *historicalHead, { }, historicalSink, fetch_limits(), { });
			expect(result && result->AdvertisedRefs.Head == requestContext.Revision.Head
					   && text(historical.CapturedFetchBody).find("want " + std::string(64U, 'e') + "\n") != std::string::npos
					   && historical.Calls == std::vector<std::string_view>{ "discover", "ls-refs", "fetch" }
					   && historicalSink.AbortCalls == 0U,
				   "historical want preserves live authority and validates current PR advertisement");
			scripted_transport unavailable{ requestContext };
			unavailable.FetchFailure = application::git_smart_transport_failure::network;
			recording_pack_sink unavailableSink{ core::git_hash_algorithm::sha256 };
			const auto          refused = application::acquire_git_protocol_v2_historical_pack(
				unavailable, requestContext, *historicalHead, { }, unavailableSink, fetch_limits(), { });
			expect(!refused && unavailableSink.AbortCalls == 1U,
				   "unavailable historical fetch aborts its owned sink exactly once");
		}

		{
			scripted_transport failed{ requestContext };
			failed.DiscoveryFailure = application::git_smart_transport_failure::network;
			recording_pack_sink failedSink{
				core::git_hash_algorithm::sha256
			};
			const auto result = application::acquire_git_protocol_v2_pack(
				failed,
				requestContext,
				{ },
				failedSink,
				fetch_limits(),
				{ });
			const auto* error = result.has_value()
				? nullptr
				: std::get_if<application::git_smart_transport_error>(
					  &result.error());
			expect(
				error != nullptr
					&& error->Failure
						== application::git_smart_transport_failure::network
					&& failedSink.AbortCalls == 1U,
				"discovery transport failure aborts once and retains taxonomy");
		}
		{
			scripted_transport rejected{ requestContext };
			std::string        badChannel(1U, static_cast<char>(4));
			badChannel += "data";
			rejected.FetchBody = bytes(
				packet("packfile\n") + packet(badChannel) + "0000");
			recording_pack_sink rejectedSink{
				core::git_hash_algorithm::sha256
			};
			const auto result = application::acquire_git_protocol_v2_pack(
				rejected,
				requestContext,
				{ },
				rejectedSink,
				fetch_limits(),
				{ });
			const auto* error = result.has_value()
				? nullptr
				: std::get_if<application::git_protocol_v2_error>(
					  &result.error());
			expect(
				error != nullptr
					&& error->Failure
						== application::git_protocol_v2_failure::
							malformed_packet
					&& rejectedSink.AbortCalls == 1U,
				"consumer rejection maps to protocol error and aborts once");
		}
		{
			scripted_transport timedOut{ requestContext };
			timedOut.FetchFailure = application::git_smart_transport_failure::timeout;
			recording_pack_sink timedOutSink{
				core::git_hash_algorithm::sha256
			};
			const auto result = application::acquire_git_protocol_v2_pack(
				timedOut,
				requestContext,
				{ },
				timedOutSink,
				fetch_limits(),
				{ });
			const auto* error = result.has_value()
				? nullptr
				: std::get_if<application::git_smart_transport_error>(
					  &result.error());
			expect(
				error != nullptr
					&& error->Failure
						== application::git_smart_transport_failure::timeout
					&& timedOutSink.AbortCalls == 1U,
				"fetch transport failure remains distinct and aborts once");
		}
		{
			scripted_transport bound{ requestContext };
			auto               staleContext = requestContext;
			staleContext.ApplicationSession = core::application_session_id{ 99U };
			recording_pack_sink staleSink{
				core::git_hash_algorithm::sha256
			};
			const auto result = application::acquire_git_protocol_v2_pack(
				bound,
				staleContext,
				{ },
				staleSink,
				fetch_limits(),
				{ });
			const auto* error = result.has_value()
				? nullptr
				: std::get_if<application::git_smart_transport_error>(
					  &result.error());
			expect(
				error != nullptr
					&& error->Failure
						== application::git_smart_transport_failure::
							invalid_request
					&& staleSink.AbortCalls == 1U,
				"a lease rejects a different otherwise-valid request context");
		}
		{
			scripted_transport revoked{ requestContext };
			revoked.revoke();
			recording_pack_sink revokedSink{
				core::git_hash_algorithm::sha256
			};
			const auto result = application::acquire_git_protocol_v2_pack(
				revoked,
				requestContext,
				{ },
				revokedSink,
				fetch_limits(),
				{ });
			expect(
				!result.has_value() && revoked.revoked()
					&& std::holds_alternative<
						application::git_smart_transport_error>(
						result.error())
					&& revokedSink.AbortCalls == 1U,
				"a revoked unique lease rejects later work");
		}
	}

	auto test_public_acquisition_rejects_bypass_inputs() -> void
	{
		const auto requestContext = context();
		{
			scripted_transport transport{ requestContext };
			auto               invalidContext = requestContext;
			invalidContext.ApplicationSession = { };
			recording_pack_sink sink{ core::git_hash_algorithm::sha1 };
			const auto          result = application::acquire_git_protocol_v2_pack(
				transport,
				invalidContext,
				{ },
				sink,
				fetch_limits(),
				{ });
			expect(
				!result.has_value() && transport.Calls.empty()
					&& sink.AbortCalls == 1U,
				"invalid context aborts once before a transport round");
		}
		{
			scripted_transport transport{ requestContext };
			transport.DiscoveryBody = bytes(
				packet("version 2\n") + packet("ls-refs\n")
				+ packet("object-format=sha1\n") + "0000");
			recording_pack_sink sink{ core::git_hash_algorithm::sha1 };
			const auto          result = application::acquire_git_protocol_v2_pack(
				transport,
				requestContext,
				{ },
				sink,
				fetch_limits(),
				{ });
			expect(
				!result.has_value()
					&& transport.Calls
						== std::vector<std::string_view>{ "discover" }
					&& sink.AbortCalls == 1U,
				"invalid capabilities cannot reach ls-refs or fetch");
		}
		{
			scripted_transport transport{ requestContext };
			const auto         base = std::string{
				core::canonical_hex(requestContext.Revision.Base).view()
			};
			transport.LsRefsBody = bytes(
				packet(base + " refs/heads/main\n") + "0000");
			recording_pack_sink sink{ core::git_hash_algorithm::sha1 };
			const auto          result = application::acquire_git_protocol_v2_pack(
				transport,
				requestContext,
				{ },
				sink,
				fetch_limits(),
				{ });
			expect(
				!result.has_value()
					&& transport.Calls
						== std::vector<std::string_view>{
							"discover",
							"ls-refs",
						}
					&& sink.AbortCalls == 1U,
				"incomplete exact refs abort once and cannot reach fetch");
		}
		{
			const auto assertRejectedBeforeTransport =
				[&](const std::span<const application::git_protocol_v2_have> aHaves,
					const std::string_view                                   aMessage) {
					scripted_transport  transport{ requestContext };
					recording_pack_sink sink{ core::git_hash_algorithm::sha1 };
					const auto          result = application::acquire_git_protocol_v2_pack(
						transport,
						requestContext,
						aHaves,
						sink,
						fetch_limits(),
						{ });
					expect(
						!result.has_value() && transport.Calls.empty()
							&& sink.AbortCalls == 1U,
						aMessage);
				};

			application::git_protocol_v2_have candidate{
				.Cache          = requestContext.Authority.Cache,
				.Commit         = oid('3', core::git_hash_algorithm::sha1),
				.CatalogVisible = true,
				.ObjectVerified = true,
			};
			candidate.Cache = opaque<core::repository_cache_scope_id>(99U);
			assertRejectedBeforeTransport(
				std::span<const application::git_protocol_v2_have>{ &candidate, 1U },
				"cross-cache have aborts before any transport round");

			candidate.Cache          = requestContext.Authority.Cache;
			candidate.CatalogVisible = false;
			assertRejectedBeforeTransport(
				std::span<const application::git_protocol_v2_have>{ &candidate, 1U },
				"catalog-invisible have aborts before any transport round");

			candidate.CatalogVisible = true;
			candidate.ObjectVerified = false;
			assertRejectedBeforeTransport(
				std::span<const application::git_protocol_v2_have>{ &candidate, 1U },
				"unverified have aborts before any transport round");

			candidate.ObjectVerified = true;
			candidate.Commit         = oid('3', core::git_hash_algorithm::sha256);
			assertRejectedBeforeTransport(
				std::span<const application::git_protocol_v2_have>{ &candidate, 1U },
				"mixed-format have aborts before any transport round");

			const std::vector excessiveHaves(
				application::maximum_git_protocol_v2_haves + 1U,
				application::git_protocol_v2_have{
					.Cache          = requestContext.Authority.Cache,
					.Commit         = oid('3', core::git_hash_algorithm::sha1),
					.CatalogVisible = true,
					.ObjectVerified = true,
				});
			assertRejectedBeforeTransport(
				excessiveHaves,
				"excessive have count aborts before any transport round");
		}
		{
			auto zeroWire                          = fetch_limits();
			zeroWire.MaximumWireBytes              = 0U;
			auto excessiveWire                     = fetch_limits();
			excessiveWire.MaximumWireBytes         = application::maximum_git_smart_fetch_response_bytes + 1U;
			auto zeroPack                          = fetch_limits();
			zeroPack.MaximumPackBytes              = 0U;
			auto excessivePack                     = fetch_limits();
			excessivePack.MaximumPackBytes         = application::maximum_git_protocol_v2_pack_bytes + 1U;
			auto excessiveProgress                 = fetch_limits();
			excessiveProgress.MaximumProgressBytes = application::maximum_git_protocol_v2_progress_bytes + 1U;
			auto zeroPackets                       = fetch_limits();
			zeroPackets.MaximumPackets             = 0U;
			auto excessivePackets                  = fetch_limits();
			excessivePackets.MaximumPackets        = application::maximum_git_protocol_v2_fetch_packets + 1U;

			for (const auto invalidLimits : {
					 zeroWire,
					 excessiveWire,
					 zeroPack,
					 excessivePack,
					 excessiveProgress,
					 zeroPackets,
					 excessivePackets,
				 }) {
				scripted_transport  transport{ requestContext };
				recording_pack_sink sink{ core::git_hash_algorithm::sha1 };
				const auto          result = application::acquire_git_protocol_v2_pack(
					transport,
					requestContext,
					{ },
					sink,
					invalidLimits,
					{ });
				expect(
					!result.has_value() && transport.Calls.empty()
						&& sink.AbortCalls == 1U,
					"invalid or overflowing limits abort before transport");
			}
		}
		{
			scripted_transport transport{ requestContext };
			std::stop_source   cancellation;
			cancellation.request_stop();
			recording_pack_sink sink{ core::git_hash_algorithm::sha1 };
			const auto          result = application::acquire_git_protocol_v2_pack(
				transport,
				requestContext,
				{ },
				sink,
				fetch_limits(),
				cancellation.get_token());
			expect(
				!result.has_value() && transport.Calls.empty()
					&& sink.AbortCalls == 1U,
				"pre-cancelled acquisition aborts once without network work");
		}
	}
} // namespace

int main()
{
	test_base_ref_and_context_validation();
	test_http_advertisement_preamble();
	test_advertisement_and_requests();
	test_ls_refs_strict_identity();
	test_incremental_fetch_parser();
	test_fetch_parser_rejections();
	test_fetch_limits_and_framing();
	test_pack_sink_failures_and_cancellation();
	test_end_to_end_acquisition_and_error_mapping();
	test_public_acquisition_rejects_bypass_inputs();
	return EXIT_SUCCESS;
}
