#include "bpr/core/local_presentation.hpp"

#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/git_objects.hpp"
#include "bpr/core/sha256.hpp"

#include <array>
#include <string>
#include <vector>

namespace bpr::core {
	namespace {
		constexpr auto presentation_domain = std::string_view{ "BPR-local-presentation-v1" };

		/// SHA-256 of the domain, a role name and length-prefixed parts.
		[[nodiscard]] auto digest(const std::string_view aRole, std::initializer_list<std::string_view> aParts) -> std::array<std::uint8_t, 32>
		{
			auto       bytes  = std::vector<std::uint8_t>(presentation_domain.begin(), presentation_domain.end());
			const auto append = [&bytes](const std::string_view aText) {
				for (std::size_t shift = 0U; shift < 64U; shift += 8U) {
					bytes.push_back(static_cast<std::uint8_t>((static_cast<std::uint64_t>(aText.size()) >> shift) & 0xFFU));
				}
				bytes.insert(bytes.end(), aText.begin(), aText.end());
			};
			bytes.push_back(0U);
			append(aRole);
			for (const auto part : aParts) {
				append(part);
			}
			return sha256(bytes);
		}

		/// A nonzero 63-bit value from a digest, for the integral identities.
		[[nodiscard]] auto nonzero(const std::array<std::uint8_t, 32>& aDigest, const std::size_t aOffset) -> std::uint64_t
		{
			auto value = std::uint64_t{ 0U };
			for (std::size_t index = 0U; index < 8U; ++index) {
				value = (value << 8U) | aDigest[(aOffset + index) % aDigest.size()];
			}
			value &= 0x7FFF'FFFF'FFFF'FFFFULL;
			return value == 0U ? 1U : value;
		}
	}

	auto local_presentation_file(const std::string_view aSubject, const std::string_view aPath) -> file_identity
	{
		return file_identity{ digest("file", { aSubject, aPath }) };
	}

	auto local_presentation_identity_of(const std::string_view aSubject) -> local_presentation_identity
	{
		const auto subject   = digest("subject", { aSubject });
		const auto scope     = pull_request_authority_scope_id{ digest("scope", { aSubject }) };
		const auto authority = pull_request_authority{
			.Profile         = host_profile_id{ nonzero(subject, 0U) },
			.ProfileRevision = github_profile_revision{ 1U },
			.Host            = git_host_id{ nonzero(subject, 8U) },
			.HostAuthority   = host_authority_fingerprint{ digest("host", { aSubject }) },
			.Account         = github_account_id{ nonzero(subject, 16U) },
			.Viewer          = github_viewer_actor_id{ nonzero(subject, 24U) },
			.Repository      = repository_id{ nonzero(subject, 4U) },
			.Cache           = repository_cache_scope_id{ digest("cache", { aSubject }) },
			.PullRequest     = pull_request_id{ nonzero(subject, 12U) },
			.Number          = pull_request_number{ 1U },
			.Scope           = scope,
			.Mode            = github_access_mode::authenticated,
		};
		const auto incarnation = epoch_catalog_incarnation_id{ digest("incarnation", { aSubject }) };
		return {
			.Authority = authority,
			.Left      = { .Authority = scope, .Incarnation = incarnation, .Kind = comparison_anchor_kind::epoch_base, .Epoch = epoch_ordinal{ 1 } },
			.Right     = { .Authority = scope, .Incarnation = incarnation, .Kind = comparison_anchor_kind::epoch_head, .Epoch = epoch_ordinal{ 2 } },
		};
	}

	auto local_presentation_key(const std::string_view aSubject, const std::string_view aPath, const std::optional<std::span<const std::uint8_t>> aLeft,
								const std::optional<std::span<const std::uint8_t>> aRight, const diff_options& aOptions) -> std::optional<file_comparison_key>
	{
		const auto options = options_digest(aOptions);
		if ((!aLeft && !aRight) || !options) {
			return std::nullopt;
		}
		const auto blob = [](const std::optional<std::span<const std::uint8_t>>& aBytes) -> std::optional<std::optional<git_blob_oid>> {
			if (!aBytes) {
				return std::optional<git_blob_oid>{ };
			}
			auto identity = git_blob_identity(*aBytes, git_hash_algorithm::sha1);
			if (!identity) {
				return std::nullopt;
			}
			return std::optional{ *identity };
		};
		const auto left  = blob(aLeft);
		const auto right = blob(aRight);
		if (!left || !right) {
			return std::nullopt;
		}
		const auto identity = local_presentation_identity_of(aSubject);
		return file_comparison_key{
			.Authority   = identity.Authority,
			.Repository  = identity.Authority.Repository,
			.PullRequest = identity.Authority.PullRequest,
			.LeftAnchor  = identity.Left,
			.RightAnchor = identity.Right,
			.LeftBlob    = *left,
			.RightBlob   = *right,
			.File        = local_presentation_file(aSubject, aPath),
			.Options     = *options,
			.Algorithm   = supported_diff_algorithm_revision,
		};
	}
}
