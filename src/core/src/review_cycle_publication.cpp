#include "bpr/core/review_cycle_publication.hpp"

#include "bpr/core/review_cycle_material.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <utility>

namespace bpr::core {
	namespace {
		constexpr auto plan_magic    = std::string_view{ "BPRPUBP1" };
		constexpr auto journal_magic = std::string_view{ "BPRPUBJ1" };

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
			void u32(const std::uint32_t aValue)
			{
				for (auto shift = 24; shift >= 0; shift -= 8) {
					Bytes.push_back(static_cast<std::uint8_t>(aValue >> static_cast<unsigned>(shift)));
				}
			}
			void u64(const std::uint64_t aValue)
			{
				for (auto shift = 56; shift >= 0; shift -= 8) {
					Bytes.push_back(static_cast<std::uint8_t>(aValue >> static_cast<unsigned>(shift)));
				}
			}
			void i64(const std::int64_t aValue)
			{
				u64(static_cast<std::uint64_t>(aValue));
			}
			void text(const std::string_view aValue)
			{
				u32(static_cast<std::uint32_t>(aValue.size()));
				Bytes.insert(Bytes.end(), aValue.begin(), aValue.end());
			}
			void raw(const std::array<std::uint8_t, 32>& aValue)
			{
				Bytes.insert(Bytes.end(), aValue.begin(), aValue.end());
			}
			template <typename TKind>
			void oid(const git_object_oid<TKind>& aValue)
			{
				u8(static_cast<std::uint8_t>(aValue.algorithm()));
				raw(aValue.bytes());
			}
			void signature(const git_signature& aValue)
			{
				text(aValue.Name);
				text(aValue.Email);
				i64(aValue.Seconds);
				i64(aValue.OffsetMinutes);
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
			auto u32() -> std::uint32_t
			{
				auto value = std::uint32_t{ 0 };
				for (const auto byte : take(4U)) {
					value = (value << 8U) | byte;
				}
				return value;
			}
			auto u64() -> std::uint64_t
			{
				auto value = std::uint64_t{ 0 };
				for (const auto byte : take(8U)) {
					value = (value << 8U) | byte;
				}
				return value;
			}
			auto i64() -> std::int64_t
			{
				return static_cast<std::int64_t>(u64());
			}
			auto text(const std::size_t aLimit) -> std::string
			{
				const auto size = u32();
				if (size > aLimit) {
					Good = false;
					return { };
				}
				const auto bytes = take(size);
				return { bytes.begin(), bytes.end() };
			}
			auto raw() -> std::array<std::uint8_t, 32>
			{
				auto       value = std::array<std::uint8_t, 32>{ };
				const auto bytes = take(32U);
				if (!bytes.empty()) {
					std::ranges::copy(bytes, value.begin());
				}
				return value;
			}
			template <typename TOid>
			auto oid() -> TOid
			{
				const auto algorithm = u8();
				const auto bytes     = raw();
				if (algorithm > static_cast<std::uint8_t>(git_hash_algorithm::sha256)) {
					Good = false;
				}
				return TOid{ static_cast<git_hash_algorithm>(algorithm), bytes };
			}
			auto signature() -> git_signature
			{
				auto value          = git_signature{ };
				value.Name          = text(maximum_git_signature_bytes);
				value.Email         = text(maximum_git_signature_bytes);
				value.Seconds       = i64();
				const auto offset   = i64();
				Good                = Good && offset > -24 * 60 && offset < 24 * 60;
				value.OffsetMinutes = static_cast<std::int32_t>(offset);
				return value;
			}
			auto count(const std::size_t aLimit) -> std::size_t
			{
				const auto value = u32();
				if (value > aLimit) {
					Good = false;
					return 0U;
				}
				return value;
			}
			[[nodiscard]] auto finished() const -> bool
			{
				return Good && Offset == Bytes.size();
			}
		};

		[[nodiscard]] auto valid_login(const std::string_view aName, const std::size_t aLimit) -> bool
		{
			if (aName.empty() || aName.size() > aLimit || aName.front() == '-' || aName.back() == '-') {
				return false;
			}
			return std::ranges::all_of(aName, [](const char aCharacter) {
				return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= 'A' && aCharacter <= 'Z') || (aCharacter >= '0' && aCharacter <= '9') || aCharacter == '-';
			});
		}

		[[nodiscard]] auto valid_repository_name(const std::string_view aName) -> bool
		{
			if (aName.empty() || aName.size() > maximum_publication_name_bytes || aName == "." || aName == ".." || aName.ends_with(".git")) {
				return false;
			}
			return std::ranges::all_of(aName, [](const char aCharacter) {
				return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= 'A' && aCharacter <= 'Z') || (aCharacter >= '0' && aCharacter <= '9') || aCharacter == '-' || aCharacter == '_'
					|| aCharacter == '.';
			});
		}

		[[nodiscard]] auto valid_host(const std::string_view aHost) -> bool
		{
			if (aHost.empty() || aHost.size() > maximum_publication_host_bytes || aHost.front() == '.' || aHost.back() == '.' || aHost.find("..") != std::string_view::npos) {
				return false;
			}
			return std::ranges::all_of(aHost, [](const char aCharacter) { return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= '0' && aCharacter <= '9') || aCharacter == '-' || aCharacter == '.'; });
		}

		/// A branch name Git and GitHub both accept as a new reference.
		[[nodiscard]] auto valid_branch(const std::string_view aName) -> bool
		{
			if (aName.empty() || aName.size() > maximum_publication_branch_bytes || aName == "@" || aName.front() == '-' || aName.front() == '/' || aName.back() == '/'
				|| aName.back() == '.' || aName.ends_with(".lock") || aName.find("..") != std::string_view::npos || aName.find("@{") != std::string_view::npos
				|| aName.find("//") != std::string_view::npos || aName.starts_with("refs/")) {
				return false;
			}
			for (const auto character : aName) {
				const auto value = static_cast<unsigned char>(character);
				if (value < 0x21U || value == 0x7FU || std::string_view{ "~^:?*[\\" }.find(character) != std::string_view::npos) {
					return false;
				}
			}
			for (auto start = std::size_t{ 0 }; start < aName.size();) {
				const auto end = aName.find('/', start);
				if (aName[start] == '.' || aName.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start).ends_with(".lock")) {
					return false;
				}
				if (end == std::string_view::npos) {
					break;
				}
				start = end + 1U;
			}
			return true;
		}

		[[nodiscard]] auto valid_title(const std::string_view aTitle) -> bool
		{
			return !aTitle.empty() && aTitle.size() <= maximum_candidate_title_bytes && aTitle.find_first_of("\r\n") == std::string_view::npos && aTitle.front() != ' ' && aTitle.back() != ' '
				&& classify_material({ reinterpret_cast<const std::uint8_t*>(aTitle.data()), aTitle.size() }).Kind == material_text_kind::utf8;
		}

		[[nodiscard]] auto valid_body(const std::string_view aBody) -> bool
		{
			const auto kind = classify_material({ reinterpret_cast<const std::uint8_t*>(aBody.data()), aBody.size() }).Kind;
			return aBody.size() <= maximum_publication_body_bytes && (kind == material_text_kind::utf8 || kind == material_text_kind::empty);
		}

		[[nodiscard]] auto hex(const git_entry_oid& aOid) -> std::string
		{
			return std::string{ canonical_hex(aOid).view() };
		}

		/// Every write the plan needs, in order: distinct blobs, the tree, the
		/// commit, the branch, the pull request.
		[[nodiscard]] auto steps(const publication_plan& aPlan) -> std::vector<publication_step>
		{
			auto blobs = std::set<std::string>{ };
			for (const auto& blob : aPlan.Blobs) {
				blobs.insert(hex(blob.Object));
			}
			auto out = std::vector<publication_step>{ };
			for (const auto& blob : blobs) {
				out.push_back({ .Kind = publication_effect_kind::blob, .Subject = blob });
			}
			out.push_back({ .Kind = publication_effect_kind::tree, .Subject = std::string{ canonical_hex(aPlan.Tree).view() } });
			out.push_back({ .Kind = publication_effect_kind::commit, .Subject = std::string{ canonical_hex(aPlan.Commit).view() } });
			out.push_back({ .Kind = publication_effect_kind::branch, .Subject = aPlan.Destination.NewBranch });
			out.push_back({ .Kind = publication_effect_kind::pull_request, .Subject = "pull" });
			return out;
		}

		[[nodiscard]] auto confirmed(const publication_journal& aJournal, const publication_effect_kind aKind, const std::string_view aSubject) -> bool
		{
			return std::ranges::any_of(aJournal.Effects, [&](const publication_effect& aEffect) {
				return aEffect.Kind == aKind && aEffect.Subject == aSubject && aEffect.State == publication_effect_state::confirmed;
			});
		}

		[[nodiscard]] auto unresolved(const publication_journal& aJournal) -> bool
		{
			return std::ranges::any_of(aJournal.Effects, [](const publication_effect& aEffect) {
				return aEffect.State == publication_effect_state::intended || aEffect.State == publication_effect_state::uncertain;
			});
		}

		/// The phase confirmed effects establish.
		[[nodiscard]] auto progress(const publication_journal& aJournal) -> publication_phase
		{
			const auto any = [&aJournal](const auto aSelect) { return std::ranges::any_of(aJournal.Effects, aSelect); };
			const auto is  = [](const publication_effect_kind aKind) {
				return [aKind](const publication_effect& aEffect) { return aEffect.Kind == aKind && aEffect.State == publication_effect_state::confirmed; };
			};
			if (any(is(publication_effect_kind::pull_request))) {
				return publication_phase::linked;
			}
			if (any(is(publication_effect_kind::branch))) {
				return publication_phase::branch_created;
			}
			if (any(is(publication_effect_kind::blob)) || any(is(publication_effect_kind::tree)) || any(is(publication_effect_kind::commit))) {
				return publication_phase::uploading_objects;
			}
			return aJournal.Generation != 0U ? publication_phase::authorized : publication_phase::prepared;
		}

		[[nodiscard]] auto closed(const publication_phase aPhase) noexcept -> bool
		{
			return aPhase == publication_phase::linked || aPhase == publication_phase::failed || aPhase == publication_phase::target_drift || aPhase == publication_phase::superseded;
		}

		[[nodiscard]] auto valid_link(const publication_link& aLink, const git_hash_algorithm aFormat) -> bool
		{
			return aLink.Number != 0U && aLink.NodeId.size() <= 256U && aLink.Url.size() <= maximum_publication_remote_bytes && aLink.Head.valid() && aLink.Head.algorithm() == aFormat
				&& valid_branch(aLink.BaseBranch);
		}

		void write_link(writer& aOut, const publication_link& aLink)
		{
			aOut.u64(aLink.Number);
			aOut.text(aLink.NodeId);
			aOut.text(aLink.Url);
			aOut.oid(aLink.Head);
			aOut.text(aLink.BaseBranch);
			aOut.flag(aLink.Drifted);
		}

		auto read_link(reader& aIn) -> publication_link
		{
			auto link       = publication_link{ };
			link.Number     = aIn.u64();
			link.NodeId     = aIn.text(256U);
			link.Url        = aIn.text(maximum_publication_remote_bytes);
			link.Head       = aIn.oid<git_commit_oid>();
			link.BaseBranch = aIn.text(maximum_publication_branch_bytes);
			link.Drifted    = aIn.flag();
			return link;
		}

		[[nodiscard]] auto lower_hex(const std::array<std::uint8_t, 32>& aBytes) -> std::string
		{
			constexpr auto digits = std::string_view{ "0123456789abcdef" };
			auto           text   = std::string{ };
			for (const auto byte : aBytes) {
				text += digits[byte >> 4U];
				text += digits[byte & 15U];
			}
			return text;
		}

		[[nodiscard]] auto refuse(const publication_refusal aRefusal) -> std::unexpected<publication_refusal>
		{
			return std::unexpected{ aRefusal };
		}
	}

	auto name(const publication_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case publication_refusal::invalid_plan:
			return "invalid_plan";
		case publication_refusal::stale_plan:
			return "stale_plan";
		case publication_refusal::not_reviewer:
			return "not_reviewer";
		case publication_refusal::authority_changed:
			return "authority_changed";
		case publication_refusal::unauthorized:
			return "unauthorized";
		case publication_refusal::out_of_order:
			return "out_of_order";
		case publication_refusal::duplicate:
			return "duplicate";
		case publication_refusal::unresolved:
			return "unresolved";
		case publication_refusal::closed:
			return "closed";
		case publication_refusal::unknown_effect:
			return "unknown_effect";
		}
		return "unknown";
	}

	auto name(const publication_phase aPhase) noexcept -> std::string_view
	{
		switch (aPhase) {
		case publication_phase::prepared:
			return "prepared";
		case publication_phase::authorized:
			return "authorized";
		case publication_phase::uploading_objects:
			return "uploading_objects";
		case publication_phase::branch_created:
			return "branch_created";
		case publication_phase::pr_creation_pending:
			return "pr_creation_pending";
		case publication_phase::linked:
			return "linked";
		case publication_phase::failed:
			return "failed";
		case publication_phase::uncertain:
			return "uncertain";
		case publication_phase::target_drift:
			return "target_drift";
		case publication_phase::cancelled:
			return "cancelled";
		case publication_phase::superseded:
			return "superseded";
		}
		return "unknown";
	}

	auto name(const publication_effect_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case publication_effect_kind::blob:
			return "blob";
		case publication_effect_kind::tree:
			return "tree";
		case publication_effect_kind::commit:
			return "commit";
		case publication_effect_kind::branch:
			return "branch";
		case publication_effect_kind::pull_request:
			return "pull_request";
		}
		return "unknown";
	}

	auto name(const publication_effect_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case publication_effect_state::intended:
			return "intended";
		case publication_effect_state::confirmed:
			return "confirmed";
		case publication_effect_state::refused:
			return "refused";
		case publication_effect_state::not_sent:
			return "not_sent";
		case publication_effect_state::uncertain:
			return "uncertain";
		case publication_effect_state::absent:
			return "absent";
		case publication_effect_state::foreign:
			return "foreign";
		}
		return "unknown";
	}

	auto valid(const publication_destination& aDestination) -> bool
	{
		return valid_host(aDestination.Host) && aDestination.Account.value() != 0U && valid_login(aDestination.Login, maximum_publication_login_bytes)
			&& valid_login(aDestination.Owner, maximum_publication_owner_bytes) && valid_repository_name(aDestination.Repository) && valid_branch(aDestination.BaseBranch)
			&& valid_branch(aDestination.NewBranch) && aDestination.NewBranch != aDestination.BaseBranch;
	}

	auto make_publication_plan(const outgoing_plan& aPlan, const publication_candidate& aCandidate, const publication_destination& aDestination, std::string aPullTitle,
							   std::string aPullBody, const bool aDraft) -> std::expected<publication_plan, publication_refusal>
	{
		if (!valid(aDestination) || !valid_title(aPullTitle) || !valid_body(aPullBody) || aCandidate.Parent != aPlan.BaseCommit || !aCandidate.Commit.valid()
			|| aCandidate.Format != aPlan.BaseCommit.algorithm() || git_commit_identity(aCandidate.Content) != aCandidate.Commit || aCandidate.Blobs.size() > maximum_publication_effects) {
			return refuse(publication_refusal::invalid_plan);
		}
		return publication_plan{
			.Start           = aPlan.StartRevision,
			.Final           = aPlan.FinalRevision,
			.Mode            = aPlan.Mode,
			.Format          = aCandidate.Format,
			.Parent          = aCandidate.Parent,
			.Tree            = aCandidate.Tree,
			.Commit          = aCandidate.Commit,
			.Content         = aCandidate.Content,
			.Blobs           = aCandidate.Blobs,
			.Deleted         = aCandidate.Deleted,
			.Transformations = aCandidate.Transformations,
			.TargetHead      = aPlan.TargetHead,
			.Destination     = aDestination,
			.PullTitle       = std::move(aPullTitle),
			.PullBody        = std::move(aPullBody),
			.Draft           = aDraft,
		};
	}

	auto encode_publication_plan(const publication_plan& aPlan) -> std::vector<std::uint8_t>
	{
		auto out = writer{ };
		out.Bytes.insert(out.Bytes.end(), plan_magic.begin(), plan_magic.end());
		out.u64(aPlan.Start.value());
		out.u64(aPlan.Final.value());
		out.u8(static_cast<std::uint8_t>(aPlan.Mode));
		out.u8(static_cast<std::uint8_t>(aPlan.Format));
		out.oid(aPlan.Parent);
		out.oid(aPlan.Tree);
		out.oid(aPlan.Commit);
		out.signature(aPlan.Content.Author);
		out.signature(aPlan.Content.Committer);
		out.text(aPlan.Content.Message);
		out.u32(static_cast<std::uint32_t>(aPlan.Blobs.size()));
		for (const auto& blob : aPlan.Blobs) {
			out.text(blob.Path);
			out.u32(blob.Mode);
			out.oid(blob.Object);
			out.raw(blob.Content.bytes());
			out.u64(blob.Size);
			out.flag(blob.LineFeed);
		}
		out.u32(static_cast<std::uint32_t>(aPlan.Deleted.size()));
		for (const auto& path : aPlan.Deleted) {
			out.text(path);
		}
		out.u32(static_cast<std::uint32_t>(aPlan.Transformations.size()));
		for (const auto& transformation : aPlan.Transformations) {
			out.text(transformation.Path);
			out.raw(transformation.Before.bytes());
			out.oid(transformation.BeforeObject);
			out.oid(transformation.After);
		}
		out.flag(aPlan.TargetHead.has_value());
		if (aPlan.TargetHead) {
			out.oid(*aPlan.TargetHead);
		}
		const auto& destination = aPlan.Destination;
		out.text(destination.Host);
		out.u64(destination.Account.value());
		out.text(destination.Login);
		out.text(destination.Owner);
		out.text(destination.Repository);
		out.text(destination.BaseBranch);
		out.text(destination.NewBranch);
		out.text(aPlan.PullTitle);
		out.text(aPlan.PullBody);
		out.flag(aPlan.Draft);
		return std::move(out.Bytes);
	}

	auto decode_publication_plan(const std::span<const std::uint8_t> aBytes) -> std::optional<publication_plan>
	{
		auto       in    = reader{ .Bytes = aBytes };
		const auto magic = in.take(plan_magic.size());
		if (!std::ranges::equal(magic, plan_magic, [](const std::uint8_t aLeft, const char aRight) { return aLeft == static_cast<std::uint8_t>(aRight); })) {
			return std::nullopt;
		}
		auto plan              = publication_plan{ };
		plan.Start             = cycle_source_revision_id{ in.u64() };
		plan.Final             = cycle_source_revision_id{ in.u64() };
		const auto mode        = in.u8();
		plan.Mode              = static_cast<outgoing_selection_mode>(mode);
		plan.Format            = static_cast<git_hash_algorithm>(in.u8());
		plan.Parent            = in.oid<git_commit_oid>();
		plan.Tree              = in.oid<git_tree_oid>();
		plan.Commit            = in.oid<git_commit_oid>();
		plan.Content.Tree      = plan.Tree;
		plan.Content.Parents   = { plan.Parent };
		plan.Content.Author    = in.signature();
		plan.Content.Committer = in.signature();
		plan.Content.Message   = in.text(maximum_git_message_bytes);
		for (auto count = in.count(maximum_publication_effects); in.Good && count != 0U; --count) {
			auto blob     = candidate_blob{ };
			blob.Path     = in.text(maximum_workspace_path_bytes);
			blob.Mode     = in.u32();
			blob.Object   = in.oid<git_entry_oid>();
			blob.Content  = cycle_material_digest{ in.raw() };
			blob.Size     = in.u64();
			blob.LineFeed = in.flag();
			plan.Blobs.push_back(std::move(blob));
		}
		for (auto count = in.count(maximum_git_tree_entries); in.Good && count != 0U; --count) {
			plan.Deleted.push_back(in.text(maximum_workspace_path_bytes));
		}
		for (auto count = in.count(maximum_publication_effects); in.Good && count != 0U; --count) {
			auto transformation         = candidate_transformation{ };
			transformation.Path         = in.text(maximum_workspace_path_bytes);
			transformation.Before       = cycle_material_digest{ in.raw() };
			transformation.BeforeObject = in.oid<git_entry_oid>();
			transformation.After        = in.oid<git_entry_oid>();
			plan.Transformations.push_back(std::move(transformation));
		}
		if (in.flag()) {
			plan.TargetHead = in.oid<git_commit_oid>();
		}
		plan.Destination.Host       = in.text(maximum_publication_host_bytes);
		plan.Destination.Account    = github_account_id{ in.u64() };
		plan.Destination.Login      = in.text(maximum_publication_login_bytes);
		plan.Destination.Owner      = in.text(maximum_publication_owner_bytes);
		plan.Destination.Repository = in.text(maximum_publication_name_bytes);
		plan.Destination.BaseBranch = in.text(maximum_publication_branch_bytes);
		plan.Destination.NewBranch  = in.text(maximum_publication_branch_bytes);
		plan.PullTitle              = in.text(maximum_candidate_title_bytes);
		plan.PullBody               = in.text(maximum_publication_body_bytes);
		plan.Draft                  = in.flag();
		if (!in.finished() || mode > static_cast<std::uint8_t>(outgoing_selection_mode::improvements_only) || !valid(plan.Destination) || !valid_title(plan.PullTitle)
			|| !valid_body(plan.PullBody) || git_commit_identity(plan.Content) != plan.Commit
			|| encode_publication_plan(plan) != std::vector<std::uint8_t>(aBytes.begin(), aBytes.end())) {
			return std::nullopt;
		}
		return plan;
	}

	auto publication_plan_digest(const publication_plan& aPlan) -> cycle_material_digest
	{
		return cycle_material_digest{ sha256(encode_publication_plan(aPlan)) };
	}

	auto encode_publication_journal(const publication_journal& aJournal) -> std::vector<std::uint8_t>
	{
		auto out = writer{ };
		out.Bytes.insert(out.Bytes.end(), journal_magic.begin(), journal_magic.end());
		out.raw(aJournal.PlanDigest.bytes());
		out.u8(static_cast<std::uint8_t>(aJournal.Phase));
		out.u64(aJournal.Generation);
		out.u64(aJournal.NextGeneration);
		out.u32(static_cast<std::uint32_t>(aJournal.Effects.size()));
		for (const auto& effect : aJournal.Effects) {
			out.u64(effect.Sequence);
			out.u8(static_cast<std::uint8_t>(effect.Kind));
			out.text(effect.Subject);
			out.u8(static_cast<std::uint8_t>(effect.State));
			out.u64(effect.Generation);
			out.text(effect.Remote);
		}
		out.flag(aJournal.Link.has_value());
		if (aJournal.Link) {
			write_link(out, *aJournal.Link);
		}
		return std::move(out.Bytes);
	}

	auto decode_publication_journal(const std::span<const std::uint8_t> aBytes) -> std::optional<publication_journal>
	{
		auto       in    = reader{ .Bytes = aBytes };
		const auto magic = in.take(journal_magic.size());
		if (!std::ranges::equal(magic, journal_magic, [](const std::uint8_t aLeft, const char aRight) { return aLeft == static_cast<std::uint8_t>(aRight); })) {
			return std::nullopt;
		}
		auto journal           = publication_journal{ };
		journal.PlanDigest     = cycle_material_digest{ in.raw() };
		const auto phase       = in.u8();
		journal.Phase          = static_cast<publication_phase>(phase);
		journal.Generation     = in.u64();
		journal.NextGeneration = in.u64();
		for (auto count = in.count(maximum_publication_effects); in.Good && count != 0U; --count) {
			auto effect       = publication_effect{ };
			effect.Sequence   = in.u64();
			const auto kind   = in.u8();
			effect.Kind       = static_cast<publication_effect_kind>(kind);
			effect.Subject    = in.text(maximum_publication_branch_bytes);
			const auto state  = in.u8();
			effect.State      = static_cast<publication_effect_state>(state);
			effect.Generation = in.u64();
			effect.Remote     = in.text(maximum_publication_remote_bytes);
			in.Good           = in.Good && kind <= static_cast<std::uint8_t>(publication_effect_kind::pull_request) && state <= static_cast<std::uint8_t>(publication_effect_state::foreign)
				&& effect.Sequence == journal.Effects.size() + 1U;
			journal.Effects.push_back(std::move(effect));
		}
		if (in.flag()) {
			journal.Link = read_link(in);
		}
		if (!in.finished() || phase > static_cast<std::uint8_t>(publication_phase::superseded) || journal.Generation >= journal.NextGeneration) {
			return std::nullopt;
		}
		return journal;
	}

	auto start_publication(const publication_plan& aPlan) -> publication_journal
	{
		return { .PlanDigest = publication_plan_digest(aPlan) };
	}

	auto next_publication_step(const publication_plan& aPlan, const publication_journal& aJournal) -> std::optional<publication_step>
	{
		for (auto& step : steps(aPlan)) {
			if (!confirmed(aJournal, step.Kind, step.Subject)) {
				return std::move(step);
			}
		}
		return std::nullopt;
	}

	auto apply_publication(const publication_plan& aPlan, const publication_journal& aJournal, const publication_command& aCommand)
		-> std::expected<publication_outcome, publication_refusal>
	{
		if (aJournal.PlanDigest != publication_plan_digest(aPlan)) {
			return refuse(publication_refusal::stale_plan);
		}
		auto next = aJournal;
		return std::visit(
			[&](const auto& aChange) -> std::expected<publication_outcome, publication_refusal> {
				using change_type = std::decay_t<decltype(aChange)>;
				if constexpr (std::is_same_v<change_type, authorize_publication>) {
					// The one decision point for the publish capability.
					const auto decision = authorize(cycle_disclosure_policy{ },
													{ .Actor = aChange.Actor, .Capability = cycle_capability::publish, .Subject = aChange.Subject, .PublicationConfirmed = aChange.PlanDigest == aJournal.PlanDigest });
					if (decision == cycle_authorization::publication_unconfirmed) {
						return refuse(publication_refusal::stale_plan);
					}
					if (decision != cycle_authorization::allowed) {
						return refuse(publication_refusal::not_reviewer);
					}
					if (aChange.Account != aPlan.Destination.Account) {
						return refuse(publication_refusal::authority_changed);
					}
					if (closed(aJournal.Phase)) {
						return refuse(publication_refusal::closed);
					}
					if (unresolved(aJournal)) {
						return refuse(publication_refusal::unresolved);
					}
					next.Generation = aJournal.NextGeneration;
					++next.NextGeneration;
					next.Phase = progress(next);
					return publication_outcome{ .Journal = std::move(next) };
				} else if constexpr (std::is_same_v<change_type, begin_publication_effect>) {
					if (closed(aJournal.Phase) || aJournal.Phase == publication_phase::cancelled) {
						return refuse(publication_refusal::closed);
					}
					if (unresolved(aJournal)) {
						return refuse(publication_refusal::unresolved);
					}
					if (aJournal.Generation == 0U || aChange.Generation != aJournal.Generation) {
						return refuse(publication_refusal::unauthorized);
					}
					if (aChange.Account != aPlan.Destination.Account) {
						return refuse(publication_refusal::authority_changed);
					}
					if (confirmed(aJournal, aChange.Kind, aChange.Subject)) {
						return refuse(publication_refusal::duplicate);
					}
					const auto step = next_publication_step(aPlan, aJournal);
					if (!step || step->Kind != aChange.Kind || step->Subject != aChange.Subject || aJournal.Effects.size() >= maximum_publication_effects) {
						return refuse(publication_refusal::out_of_order);
					}
					const auto sequence = aJournal.Effects.size() + 1U;
					next.Effects.push_back({ .Sequence = sequence, .Kind = aChange.Kind, .Subject = aChange.Subject, .State = publication_effect_state::intended, .Generation = aJournal.Generation });
					next.Phase = aChange.Kind == publication_effect_kind::pull_request ? publication_phase::pr_creation_pending : publication_phase::uploading_objects;
					return publication_outcome{ .Journal = std::move(next), .Sequence = sequence };
				} else if constexpr (std::is_same_v<change_type, complete_publication_effect> || std::is_same_v<change_type, observe_publication_effect>) {
					constexpr auto observing = std::is_same_v<change_type, observe_publication_effect>;
					if (aChange.Sequence == 0U || aChange.Sequence > aJournal.Effects.size()) {
						return refuse(publication_refusal::unknown_effect);
					}
					auto&      effect  = next.Effects[aChange.Sequence - 1U];
					const auto outcome = [&aChange] {
						if constexpr (observing) {
							return aChange.Observed;
						} else {
							return aChange.Outcome;
						}
					}();
					const auto expected = observing ? publication_effect_state::uncertain : publication_effect_state::intended;
					const auto allowed  = observing ? (outcome == publication_effect_state::confirmed || outcome == publication_effect_state::absent || outcome == publication_effect_state::foreign
													   || outcome == publication_effect_state::uncertain)
													: (outcome == publication_effect_state::confirmed || outcome == publication_effect_state::refused || outcome == publication_effect_state::not_sent
													   || outcome == publication_effect_state::uncertain || outcome == publication_effect_state::foreign);
					if (effect.State != expected || !allowed || aChange.Remote.size() > maximum_publication_remote_bytes) {
						return refuse(publication_refusal::unknown_effect);
					}
					const auto pull = effect.Kind == publication_effect_kind::pull_request && outcome == publication_effect_state::confirmed;
					if (pull != aChange.Link.has_value() || (aChange.Link && !valid_link(*aChange.Link, aPlan.Format))) {
						return refuse(publication_refusal::unknown_effect);
					}
					effect.State  = outcome;
					effect.Remote = aChange.Remote;
					if (aChange.Link) {
						next.Link = *aChange.Link;
					}
					if (outcome == publication_effect_state::refused) {
						next.Phase      = publication_phase::failed;
						next.Generation = 0U;
					} else if (outcome == publication_effect_state::foreign) {
						next.Phase      = publication_phase::target_drift;
						next.Generation = 0U;
					} else if (outcome == publication_effect_state::uncertain) {
						next.Phase      = publication_phase::uncertain;
						next.Generation = 0U;
					} else if (!unresolved(next)) {
						next.Phase = progress(next);
					}
					return publication_outcome{ .Journal = std::move(next) };
				} else if constexpr (std::is_same_v<change_type, restart_publication>) {
					for (auto& effect : next.Effects) {
						if (effect.State == publication_effect_state::intended) {
							effect.State = publication_effect_state::uncertain;
						}
					}
					next.Generation = 0U;
					if (!closed(aJournal.Phase) && aJournal.Phase != publication_phase::cancelled) {
						next.Phase = unresolved(next) ? publication_phase::uncertain : progress(next);
					}
					return publication_outcome{ .Journal = std::move(next) };
				} else if constexpr (std::is_same_v<change_type, cancel_publication>) {
					if (closed(aJournal.Phase)) {
						return refuse(publication_refusal::closed);
					}
					if (std::ranges::any_of(aJournal.Effects, [](const publication_effect& aEffect) { return aEffect.State == publication_effect_state::intended; })) {
						return refuse(publication_refusal::unresolved);
					}
					next.Phase      = publication_phase::cancelled;
					next.Generation = 0U;
					return publication_outcome{ .Journal = std::move(next) };
				} else if constexpr (std::is_same_v<change_type, supersede_publication>) {
					if (!aJournal.Effects.empty() || (aJournal.Phase != publication_phase::prepared && aJournal.Phase != publication_phase::authorized)) {
						return refuse(publication_refusal::unresolved);
					}
					next.Phase      = publication_phase::superseded;
					next.Generation = 0U;
					return publication_outcome{ .Journal = std::move(next) };
				} else {
					if (aJournal.Phase != publication_phase::linked || !aJournal.Link || aChange.Link.Number != aJournal.Link->Number || !valid_link(aChange.Link, aPlan.Format)) {
						return refuse(publication_refusal::unknown_effect);
					}
					next.Link = aChange.Link;
					return publication_outcome{ .Journal = std::move(next) };
				}
			},
			aCommand);
	}
	auto render_publication_preview(const publication_plan& aPlan) -> std::string
	{
		const auto  signature   = [](const git_signature& aSignature) { return aSignature.Name + " <" + aSignature.Email + ">"; };
		const auto& destination = aPlan.Destination;
		auto        out         = std::string{ "Publish as a new pull request: nothing has been sent.\n" };
		out += "From cycle revisions R" + std::to_string(aPlan.Start.value()) + " to R" + std::to_string(aPlan.Final.value()) + (aPlan.Mode == outgoing_selection_mode::improvements_only ? ", only the cycle's improvements.\n" : ", starting work and improvements.\n");
		out += "On " + destination.Host + " as " + destination.Login + " (account " + std::to_string(destination.Account.value()) + "): " + destination.Owner + "/" + destination.Repository + ".\n";
		out += "New branch " + destination.NewBranch + " at commit " + std::string{ canonical_hex(aPlan.Commit).view() } + ", whose only parent is P " + std::string{ canonical_hex(aPlan.Parent).view() } + ".\n";
		out += "Pull request into " + destination.BaseBranch + (aPlan.Draft ? ", as a draft.\n" : ", ready for review.\n");
		if (aPlan.TargetHead) {
			out += aPlan.TargetHead == aPlan.Parent ? std::string{ "The target was at P when planned.\n" }
													: "The target was at " + std::string{ canonical_hex(*aPlan.TargetHead).view() } + " when planned, not P: the pull request will show that difference.\n";
		}
		out += "Tree " + std::string{ canonical_hex(aPlan.Tree).view() } + ": " + std::to_string(aPlan.Blobs.size()) + " added or changed, " + std::to_string(aPlan.Deleted.size()) + " removed";
		out += aPlan.Transformations.empty() ? std::string{ ", every byte as captured.\n" } : ", " + std::to_string(aPlan.Transformations.size()) + " normalized from CRLF to LF.\n";
		for (const auto& blob : aPlan.Blobs) {
			out += "  " + workspace_path_label(blob.Path) + (blob.LineFeed ? " (CRLF -> LF)\n" : "\n");
		}
		for (const auto& path : aPlan.Deleted) {
			out += "  " + workspace_path_label(path) + " (removed)\n";
		}
		out += "Author: " + signature(aPlan.Content.Author) + "; committer: " + signature(aPlan.Content.Committer) + "; unsigned.\n";
		out += "Commit message:\n";
		auto message = std::string_view{ aPlan.Content.Message };
		while (!message.empty()) {
			const auto end = message.find('\n');
			out += "  ";
			out += message.substr(0U, end);
			out += '\n';
			message.remove_prefix(end == std::string_view::npos ? message.size() : end + 1U);
		}
		out += "Pull request title: " + aPlan.PullTitle + "\n";
		out += aPlan.PullBody.empty() ? std::string{ "Pull request body: (empty)\n" } : "Pull request body:\n" + aPlan.PullBody + (aPlan.PullBody.ends_with('\n') ? "" : "\n");
		out += "Private cycle history, notes and rounds are not uploaded.\n";
		out += "Confirming authorizes exactly this plan: " + std::string{ lower_hex(publication_plan_digest(aPlan).bytes()) } + ".\n";
		return out;
	}
}
