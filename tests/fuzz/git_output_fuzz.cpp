// [[BPR-0970]]. Git output parsing: the smart protocol v2 replies a remote
// sends, what the installed Git prints, and the repository files a Review
// Cycle reads directly (index, HEAD, refs, packed-refs, config, ignore and
// attributes files, commit objects).
//
// Input: [selector] [parameter] then the payload. Multi-part cases split the
// payload at `fuzz::part_separator`.

#include "bpr/application/git_protocol_v2.hpp"
#include "bpr/core/git_config.hpp"
#include "bpr/core/git_objects.hpp"
#include "bpr/core/review_cycle_git.hpp"
#include "bpr/core/review_cycle_ignore.hpp"
#include "fuzz_support.hpp"
#include "git_process_output.hpp"

#include <cstddef>
#include <string>

namespace {
	using namespace bpr;

	constexpr std::size_t maximum_fuzz_wire_bytes = 65536U;

	auto core_algorithm(const std::uint8_t aParameter) -> core::git_hash_algorithm
	{
		return (aParameter & 1U) == 0U ? core::git_hash_algorithm::sha1 : core::git_hash_algorithm::sha256;
	}

	auto names(const std::string_view aText) -> void
	{
		if (const auto commit = core::parse_git_commit_oid(aText)) {
			fuzz::require(aText.size() == 40U || aText.size() == 64U);
		}
		static_cast<void>(core::parse_git_tree_oid(aText));
		static_cast<void>(core::parse_git_blob_oid(aText));
		if (const auto entry = core::parse_git_entry_oid(aText)) {
			const auto canonical = core::canonical_hex(*entry);
			fuzz::require(core::parse_git_entry_oid(canonical.view()) == entry);
		}
		if (const auto reference = application::parse_git_base_branch_ref(aText)) {
			fuzz::require(application::valid(*reference));
		}
		if (const auto entries = core::parse_git_config(aText)) {
			fuzz::require(entries->size() <= core::maximum_git_config_entries);
		}
		static_cast<void>(core::read_git_repository_format(aText));
	}

	auto index(const std::span<const std::uint8_t> aBytes, const std::uint8_t aParameter) -> void
	{
		const std::uint64_t maximum = 1U + (aParameter >> 1U);
		const auto          parsed  = core::parse_git_index(aBytes, core_algorithm(aParameter), maximum);
		if (parsed) {
			fuzz::require(parsed->Entries.size() <= maximum);
		}
	}

	auto references(const fuzz::fuzz_input& aInput, const std::uint8_t aParameter) -> void
	{
		const auto algorithm       = core_algorithm(aParameter);
		const auto [first, second] = aInput.split(fuzz::part_separator);
		const auto head            = core::parse_git_head(first, algorithm);
		fuzz::require(!(head.Reference && head.Commit));
		static_cast<void>(core::parse_git_reference(first, algorithm));
		// The second part is a packed-refs file, the first the name sought.
		static_cast<void>(core::find_packed_reference(second, first.substr(0U, 256U), algorithm));
	}

	auto ignore_rules(const fuzz::fuzz_input& aInput, const std::uint8_t aParameter) -> void
	{
		const auto [rules, paths]            = aInput.split(fuzz::part_separator);
		const auto                      fold = (aParameter & 4U) != 0U;
		core::workspace_ignore_rules    ignore{ fold };
		core::workspace_attribute_rules attributes{ fold };
		const auto                      base = (aParameter & 1U) == 0U ? std::string_view{ } : std::string_view{ "src/nested" };
		static_cast<void>(ignore.add(base, rules));
		static_cast<void>(attributes.add(base, rules));
		auto remaining = paths;
		for (int count = 0; count < 16 && !remaining.empty(); ++count) {
			const auto end  = remaining.find('\n');
			const auto path = remaining.substr(0U, end);
			static_cast<void>(ignore.ignored(path, (aParameter & 2U) != 0U));
			static_cast<void>(attributes.filter(path));
			// The first line of the rules is also tried as a single pattern.
			static_cast<void>(core::wildcard_match(rules.substr(0U, rules.find('\n')), path, fold));
			if (end == std::string_view::npos) {
				break;
			}
			remaining.remove_prefix(end + 1U);
		}
	}

	auto authority() -> core::pull_request_authority
	{
		return {
			.Profile         = core::host_profile_id{ 1 },
			.ProfileRevision = core::github_profile_revision{ 2 },
			.Host            = core::git_host_id{ 3 },
			.HostAuthority   = fuzz::opaque<core::host_authority_fingerprint>(4),
			.Account         = core::github_account_id{ 5 },
			.Viewer          = core::github_viewer_actor_id{ 6 },
			.Repository      = core::repository_id{ 7 },
			.Cache           = fuzz::opaque<core::repository_cache_scope_id>(8),
			.PullRequest     = core::pull_request_id{ 9 },
			.Number          = core::pull_request_number{ 10 },
			.Scope           = fuzz::opaque<core::pull_request_authority_scope_id>(11),
		};
	}

	auto smart_context(const core::git_hash_algorithm aFormat) -> application::git_smart_request_context
	{
		const auto                             width = aFormat == core::git_hash_algorithm::sha1 ? 40U : 64U;
		application::git_smart_request_context context{
			.ApplicationSession = core::application_session_id{ 1 },
			.Authority          = authority(),
			.Revision           = {
				.Source              = { .Observation           = fuzz::opaque<core::epoch_source_observation_id>(22),
										 .Snapshot              = core::provider_snapshot_id{ 2 },
										 .IdentityGeneration    = core::github_identity_generation{ 3 },
										 .PullRequestGeneration = core::request_generation{ 4 } },
				.Origin              = core::pull_request_revision_origin::live,
				.BaseBranch          = fuzz::opaque<core::base_branch_id>(21),
				.Base                = *core::parse_git_commit_oid(std::string(width, '1')),
				.Head                = *core::parse_git_commit_oid(std::string(width, '2')),
				.CurrentViewerReview = std::nullopt },
			.Repository = { .Owner = "owner", .Name = "repository" },
			.BaseRef    = *application::parse_git_base_branch_ref("refs/heads/main"),
		};
		fuzz::require(application::valid(context));
		return context;
	}

	// Accepts pack bytes up to the parser's own bound and never completes, so
	// nothing is written and no receipt is minted.
	class bounded_sink final : public application::git_pack_stream_sink
	{
	public:
		std::size_t Bytes = 0U;

		auto append(const std::span<const std::byte> aBytes, std::stop_token) -> application::git_pack_stream_result<void> override
		{
			fuzz::require(Bytes + aBytes.size() <= maximum_fuzz_wire_bytes);
			Bytes += aBytes.size();
			return { };
		}

		auto finish(std::stop_token) -> application::git_pack_stream_result<application::completed_git_pack_receipt> override
		{
			return std::unexpected(application::git_pack_stream_failure::malformed_pack);
		}

		auto abort() noexcept -> void override { }
	};

	auto protocol_replies(const std::span<const std::uint8_t> aBytes, const std::uint8_t aParameter) -> void
	{
		const auto format = core_algorithm(aParameter);
		const auto bytes  = std::as_bytes(aBytes);
		if (const auto advertisement = application::detail::parse_git_protocol_v2_advertisement(bytes, format)) {
			fuzz::require(application::detail::valid(*advertisement) && advertisement->ObjectFormat == format);
		}
		const auto context = smart_context(format);
		if (const auto refs = application::detail::parse_git_protocol_v2_ls_refs(bytes, context)) {
			// A listing is only accepted for the head the request was bound to.
			fuzz::require(refs->Head == context.Revision.Head);
		}
	}

	auto fetch(const std::span<const std::uint8_t> aBytes, const std::uint8_t aParameter) -> void
	{
		const auto                                      format = core_algorithm(aParameter);
		bounded_sink                                    sink;
		const application::git_protocol_v2_fetch_limits limits{
			.MaximumWireBytes     = maximum_fuzz_wire_bytes,
			.MaximumPackBytes     = maximum_fuzz_wire_bytes,
			.MaximumProgressBytes = 8192U,
			.MaximumPackets       = 256U,
		};
		application::detail::git_protocol_v2_fetch_parser parser{ sink, limits, format, { } };
		// The reply arrives in two reads split where the parameter says.
		const auto bytes = std::as_bytes(aBytes);
		const auto split = (bytes.size() * (aParameter >> 1U)) / 127U;
		if (parser.consume(bytes.first(std::min(split, bytes.size())))) {
			static_cast<void>(parser.consume(bytes.subspan(std::min(split, bytes.size()))));
		}
		static_cast<void>(parser.finish());
		fuzz::require(parser.wire_bytes() <= limits.MaximumWireBytes);
		fuzz::require(parser.pack_bytes() <= limits.MaximumPackBytes);
		fuzz::require(parser.progress_bytes() <= limits.MaximumProgressBytes);
		fuzz::require(parser.packets() <= limits.MaximumPackets);
	}

	auto process_output(const fuzz::fuzz_input& aInput, const std::uint8_t aParameter) -> void
	{
		const auto algorithm       = (aParameter & 1U) == 0U ? application::git_hash_algorithm::sha1 : application::git_hash_algorithm::sha256;
		const auto [output, extra] = aInput.split(fuzz::part_separator);
		switch ((aParameter >> 1U) % 7U) {
		case 0U:
			if (const auto bases = adapters::git::detail::parse_comparison_bases((aParameter & 0x80U) != 0U ? 1 : 0, output, algorithm)) {
				fuzz::require(bases->Status != application::git_comparison_base_status::unique || bases->Base.has_value());
			}
			break;
		case 1U:
			if (const auto blob = adapters::git::detail::parse_ls_tree_blob(output, extra, algorithm); blob && *blob) {
				fuzz::require(output.ends_with(std::string{ extra } + '\0'));
			}
			break;
		case 2U: {
			const auto pairs = adapters::git::detail::parse_rename_pairs(output, extra.substr(0U, extra.find('\n')), std::nullopt);
			if (pairs) {
				fuzz::require(pairs->Count <= application::maximum_git_rename_pair_records);
				for (std::size_t record = 0U; record < pairs->Count; ++record) {
					fuzz::require(adapters::git::detail::valid_tree_entry_path(pairs->Records[record].LeftPath));
					fuzz::require(adapters::git::detail::valid_tree_entry_path(pairs->Records[record].RightPath));
				}
			}
			break;
		}
		case 3U:
			if (const auto size = adapters::git::detail::parse_blob_size(output, extra.substr(0U, 64U), 4096U)) {
				fuzz::require(*size <= 4096U);
			}
			break;
		case 4U:
			static_cast<void>(adapters::git::detail::parse_committer_seconds(output));
			break;
		case 5U:
			static_cast<void>(adapters::git::detail::parse_ref_record(output, extra.substr(0U, 256U), algorithm));
			break;
		default:
			if (const auto version = adapters::git::detail::parse_version(output)) {
				fuzz::require(output.starts_with("git version ") && output.ends_with('\n'));
			}
			break;
		}
	}

	auto objects(const std::span<const std::uint8_t> aBytes, const std::uint8_t aParameter) -> void
	{
		const auto commit = *core::parse_git_commit_oid(std::string((aParameter & 1U) == 0U ? 40U : 64U, 'a'));
		static_cast<void>(core::verified_commit_tree(aBytes, commit));
		if (const auto normalised = core::line_feed_form(aBytes)) {
			fuzz::require(normalised->size() <= aBytes.size());
		}
	}
}

// libFuzzer calls the entry point by this exact name.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* aData, const std::size_t aSize) -> int
{
	fuzz::fuzz_input input{ aData, aSize };
	const auto       selector  = input.byte();
	const auto       parameter = input.byte();
	if (input.rest().size() > maximum_fuzz_wire_bytes) {
		return 0;
	}
	switch (selector % 8U) {
	case 0U:
		names(input.text());
		break;
	case 1U:
		index(input.rest(), parameter);
		break;
	case 2U:
		references(input, parameter);
		break;
	case 3U:
		ignore_rules(input, parameter);
		break;
	case 4U:
		protocol_replies(input.rest(), parameter);
		break;
	case 5U:
		fetch(input.rest(), parameter);
		break;
	case 6U:
		process_output(input, parameter);
		break;
	default:
		objects(input.rest(), parameter);
		break;
	}
	return 0;
}
