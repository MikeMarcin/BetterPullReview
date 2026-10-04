#include "bpr/core/review_cycle_association.hpp"

#include "bpr/core/review_tour_markdown.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace bpr::core {
	namespace {
		constexpr auto association_magic = std::string_view{ "BPRLINK1" };

		auto lower(std::string_view aText) -> std::string
		{
			auto result = std::string{ aText };
			std::ranges::transform(result, result.begin(), [](const char aCharacter) { return aCharacter >= 'A' && aCharacter <= 'Z' ? static_cast<char>(aCharacter - 'A' + 'a') : aCharacter; });
			return result;
		}

		/// A host name as GitHub and GHES spell one: letters, digits, dots and
		/// hyphens, no empty label.
		auto valid_host(const std::string_view aHost) -> bool
		{
			if (aHost.empty() || aHost.size() > maximum_publication_host_bytes || aHost.front() == '.' || aHost.back() == '.' || aHost.find("..") != std::string_view::npos) {
				return false;
			}
			return std::ranges::all_of(aHost, [](const char aCharacter) {
				return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= '0' && aCharacter <= '9') || aCharacter == '.' || aCharacter == '-';
			});
		}

		struct writer
		{
			std::vector<std::uint8_t> Bytes;

			void u8(const std::uint8_t aValue)
			{
				Bytes.push_back(aValue);
			}
			void flag(const bool aValue)
			{
				u8(aValue ? 1U : 0U);
			}
			void u64(const std::uint64_t aValue)
			{
				for (auto shift = 56; shift >= 0; shift -= 8) {
					Bytes.push_back(static_cast<std::uint8_t>(aValue >> static_cast<unsigned>(shift)));
				}
			}
			void text(const std::string_view aValue)
			{
				u64(aValue.size());
				Bytes.insert(Bytes.end(), aValue.begin(), aValue.end());
			}
			void oid(const git_commit_oid& aValue)
			{
				u8(static_cast<std::uint8_t>(aValue.algorithm()));
				Bytes.insert(Bytes.end(), aValue.bytes().begin(), aValue.bytes().end());
			}
		};

		struct reader
		{
			std::span<const std::uint8_t> Bytes;
			std::size_t                   Offset = 0;
			bool                          Good   = true;

			auto take(const std::size_t aCount) -> std::span<const std::uint8_t>
			{
				if (!Good || Bytes.size() - Offset < aCount) {
					Good = false;
					return { };
				}
				const auto out = Bytes.subspan(Offset, aCount);
				Offset += aCount;
				return out;
			}
			auto u8() -> std::uint8_t
			{
				const auto value = take(1U);
				return value.empty() ? 0U : value[0];
			}
			auto flag() -> bool
			{
				const auto value = u8();
				Good             = Good && value <= 1U;
				return value == 1U;
			}
			auto u64() -> std::uint64_t
			{
				auto value = std::uint64_t{ 0 };
				for (const auto byte : take(8U)) {
					value = (value << 8U) | byte;
				}
				return value;
			}
			auto text(const std::size_t aLimit) -> std::string
			{
				const auto size = u64();
				if (size > aLimit) {
					Good = false;
					return { };
				}
				const auto bytes = take(static_cast<std::size_t>(size));
				return { bytes.begin(), bytes.end() };
			}
			auto oid() -> git_commit_oid
			{
				const auto algorithm = u8();
				const auto bytes     = take(32U);
				auto       value     = std::array<std::uint8_t, 32>{ };
				if (!bytes.empty()) {
					std::ranges::copy(bytes, value.begin());
				}
				if (algorithm == static_cast<std::uint8_t>(git_hash_algorithm::sha1)) {
					return git_commit_oid{ git_hash_algorithm::sha1, value };
				}
				if (algorithm == static_cast<std::uint8_t>(git_hash_algorithm::sha256)) {
					return git_commit_oid{ git_hash_algorithm::sha256, value };
				}
				Good = false;
				return { };
			}
			[[nodiscard]] auto finished() const noexcept -> bool
			{
				return Good && Offset == Bytes.size();
			}
		};

		/// The fields every admitted association must carry.
		auto well_formed(const cycle_pull_association& aAssociation) -> bool
		{
			return valid_host(aAssociation.Address.Host) && valid_publication_owner(aAssociation.Address.Owner) && valid_publication_repository(aAssociation.Address.Repository)
				&& aAssociation.Address.Number > 0U && !aAssociation.NodeId.empty() && aAssociation.NodeId.size() <= maximum_association_ref_bytes && !aAssociation.Url.empty()
				&& aAssociation.Url.size() <= maximum_pull_address_bytes && aAssociation.Head.valid() && aAssociation.HeadRef.size() <= maximum_association_ref_bytes
				&& aAssociation.BaseRef.size() <= maximum_association_ref_bytes && aAssociation.State.size() <= maximum_association_ref_bytes
				&& aAssociation.Login.size() <= maximum_publication_login_bytes && (aAssociation.Origin == cycle_pull_origin::published) == aAssociation.Publication.has_value()
				&& (!aAssociation.Publication || valid_cycle_id(*aAssociation.Publication));
		}
	}

	auto name(const cycle_pull_origin aOrigin) noexcept -> std::string_view
	{
		switch (aOrigin) {
		case cycle_pull_origin::published: return "published";
		case cycle_pull_origin::external:  return "external";
		}
		return "unknown";
	}

	auto name(const cycle_link_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case cycle_link_refusal::address:        return "address";
		case cycle_link_refusal::host:           return "host";
		case cycle_link_refusal::mismatch:       return "mismatch";
		case cycle_link_refusal::not_found:      return "not_found";
		case cycle_link_refusal::already_linked: return "already_linked";
		case cycle_link_refusal::bounds:         return "bounds";
		}
		return "unknown";
	}

	auto same_pull_request(const cycle_pull_address& aLeft, const cycle_pull_address& aRight) noexcept -> bool
	{
		return aLeft.Number == aRight.Number && lower(aLeft.Host) == lower(aRight.Host) && lower(aLeft.Owner) == lower(aRight.Owner) && lower(aLeft.Repository) == lower(aRight.Repository);
	}

	auto decide_external_link(const cycle_pull_address& aAsked, const cycle_pull_observation& aObserved, const std::span<const cycle_pull_association> aExisting,
							  const std::optional<cycle_source_revision_id> aRevision) -> std::expected<cycle_pull_association, cycle_link_refusal>
	{
		if (aAsked.Number == 0U || !valid_host(aAsked.Host) || !valid_publication_owner(aAsked.Owner) || !valid_publication_repository(aAsked.Repository)) {
			return std::unexpected(cycle_link_refusal::address);
		}
		if (std::ranges::any_of(aExisting, [&](const cycle_pull_association& aKnown) { return same_pull_request(aKnown.Address, aAsked); })) {
			return std::unexpected(cycle_link_refusal::already_linked);
		}
		if (aExisting.size() >= maximum_cycle_associations) {
			return std::unexpected(cycle_link_refusal::bounds);
		}
		// The read must be of exactly that pull request, and must say so itself.
		if (!same_pull_request(aObserved.Address, aAsked) || !aObserved.UrlAddress || !same_pull_request(*aObserved.UrlAddress, aAsked)) {
			return std::unexpected(cycle_link_refusal::mismatch);
		}
		auto association = cycle_pull_association{
			.Origin   = cycle_pull_origin::external,
			.Address  = aObserved.Address,
			.NodeId   = aObserved.NodeId,
			.Url      = aObserved.Url,
			.State    = aObserved.State,
			.Draft    = aObserved.Draft,
			.HeadRef  = aObserved.HeadRef,
			.Head     = aObserved.Head,
			.BaseRef  = aObserved.BaseRef,
			.BaseHead = aObserved.BaseHead,
			.Account  = aObserved.Account,
			.Login    = aObserved.Login,
			.Revision = aRevision,
		};
		if (!well_formed(association) || association.Account.value() == 0U) {
			return std::unexpected(cycle_link_refusal::mismatch);
		}
		return association;
	}

	auto published_association(const cycle_publication_id aPublication, const publication_plan& aPlan, const publication_journal& aJournal) -> std::optional<cycle_pull_association>
	{
		if (aJournal.Phase != publication_phase::linked || !aJournal.Link) {
			return std::nullopt;
		}
		const auto& link        = *aJournal.Link;
		auto        association = cycle_pull_association{
			.Origin      = cycle_pull_origin::published,
			.Publication = aPublication,
			.Address     = { .Host = aPlan.Destination.Host, .Owner = aPlan.Destination.Owner, .Repository = aPlan.Destination.Repository, .Number = link.Number },
			.NodeId      = link.NodeId,
			.Url         = link.Url,
			.Draft       = aPlan.Draft,
			.HeadRef     = aPlan.Destination.NewBranch,
			.Head        = link.Head,
			.BaseRef     = link.BaseBranch,
			.BaseHead    = aPlan.TargetHead,
			.Account     = aPlan.Destination.Account,
			.Login       = aPlan.Destination.Login,
			.Revision    = aPlan.Final,
			.Drifted     = link.Drifted,
		};
		return well_formed(association) ? std::optional{ std::move(association) } : std::nullopt;
	}

	auto encode_association(const cycle_pull_association& aAssociation) -> std::vector<std::uint8_t>
	{
		auto out = writer{ };
		out.Bytes.insert(out.Bytes.end(), association_magic.begin(), association_magic.end());
		out.u8(static_cast<std::uint8_t>(aAssociation.Origin));
		out.flag(aAssociation.Publication.has_value());
		if (aAssociation.Publication) {
			out.u64(aAssociation.Publication->value());
		}
		out.text(aAssociation.Address.Host);
		out.text(aAssociation.Address.Owner);
		out.text(aAssociation.Address.Repository);
		out.u64(aAssociation.Address.Number);
		out.text(aAssociation.NodeId);
		out.text(aAssociation.Url);
		out.text(aAssociation.State);
		out.flag(aAssociation.Draft);
		out.text(aAssociation.HeadRef);
		out.oid(aAssociation.Head);
		out.text(aAssociation.BaseRef);
		out.flag(aAssociation.BaseHead.has_value());
		if (aAssociation.BaseHead) {
			out.oid(*aAssociation.BaseHead);
		}
		out.u64(aAssociation.Account.value());
		out.text(aAssociation.Login);
		out.flag(aAssociation.Revision.has_value());
		if (aAssociation.Revision) {
			out.u64(aAssociation.Revision->value());
		}
		out.flag(aAssociation.Drifted);
		return std::move(out.Bytes);
	}

	auto decode_association(const std::span<const std::uint8_t> aBytes) -> std::optional<cycle_pull_association>
	{
		auto       in    = reader{ .Bytes = aBytes };
		const auto magic = in.take(association_magic.size());
		if (!std::ranges::equal(magic, association_magic, [](const std::uint8_t aLeft, const char aRight) { return aLeft == static_cast<std::uint8_t>(aRight); })) {
			return std::nullopt;
		}
		auto       association = cycle_pull_association{ };
		const auto origin      = in.u8();
		association.Origin     = static_cast<cycle_pull_origin>(origin);
		if (in.flag()) {
			association.Publication = cycle_publication_id{ in.u64() };
		}
		association.Address.Host       = in.text(maximum_publication_host_bytes);
		association.Address.Owner      = in.text(maximum_publication_owner_bytes);
		association.Address.Repository = in.text(maximum_publication_name_bytes);
		association.Address.Number     = in.u64();
		association.NodeId             = in.text(maximum_association_ref_bytes);
		association.Url                = in.text(maximum_pull_address_bytes);
		association.State              = in.text(maximum_association_ref_bytes);
		association.Draft              = in.flag();
		association.HeadRef            = in.text(maximum_association_ref_bytes);
		association.Head               = in.oid();
		association.BaseRef            = in.text(maximum_association_ref_bytes);
		if (in.flag()) {
			association.BaseHead = in.oid();
		}
		association.Account = github_account_id{ in.u64() };
		association.Login   = in.text(maximum_publication_login_bytes);
		if (in.flag()) {
			association.Revision = cycle_source_revision_id{ in.u64() };
		}
		association.Drifted = in.flag();
		if (!in.finished() || origin > static_cast<std::uint8_t>(cycle_pull_origin::external) || !well_formed(association)) {
			return std::nullopt;
		}
		return association;
	}

	auto publication_body(const std::string_view aProblem, const std::string_view aSolution) -> std::optional<std::string>
	{
		auto body = insert_tour_brief({ }, 0U, aProblem, aSolution);
		if (!body || body->size() > maximum_publication_body_bytes) {
			return std::nullopt;
		}
		return body;
	}

	auto suggested_publication_branch(const review_cycle_id aCycle, const cycle_source_revision_id aFinal) -> std::string
	{
		return "bpr/cycle-" + std::to_string(aCycle.value()) + "-r" + std::to_string(aFinal.value());
	}
}
