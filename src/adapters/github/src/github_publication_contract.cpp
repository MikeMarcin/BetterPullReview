#include "github_publication_contract.hpp"

#include "github_json_preflight.hpp"

#include "bpr/core/review_cycle_publication.hpp"

#include <glaze/json.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::adapters::github::detail {
	namespace {
		constexpr std::string_view rest_version = "2022-11-28";
		/// Reads of trees and pull request lists carry many entries.
		constexpr auto listing_limits = json_preflight_limits{ .Depth = 16U, .Elements = 100'000U, .Values = 2'000'000U };

		struct publication_json_options : glz::opts
		{
			// Glaze discovers these overrides by their exact library-defined names.
			// NOLINTNEXTLINE(readability-identifier-naming)
			bool validate_skipped = true;
			// NOLINTNEXTLINE(readability-identifier-naming)
			bool validate_trailing_whitespace = true;

			constexpr publication_json_options()
			{
				error_on_unknown_keys = false;
				error_on_missing_keys = true;
				partial_read          = false;
			}
		};

		struct write_json_options : glz::opts
		{
			// NOLINTNEXTLINE(readability-identifier-naming)
			bool escape_control_characters = true;

			constexpr write_json_options()
			{
				// A tree deletion is an explicit `"sha": null`.
				skip_null_members = false;
			}
		};

		auto failed(const publication_contract_failure aFailure) -> std::unexpected<publication_contract_failure>
		{
			return std::unexpected{ aFailure };
		}

		[[nodiscard]] auto percent_encode(const std::string_view aValue) -> std::string
		{
			constexpr std::string_view hexadecimal = "0123456789ABCDEF";
			auto                       encoded     = std::string{ };
			for (const auto raw : aValue) {
				const auto character = static_cast<unsigned char>(raw);
				if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') || character == '-' || character == '_'
					|| character == '.' || character == '~') {
					encoded.push_back(static_cast<char>(character));
					continue;
				}
				encoded.push_back('%');
				encoded.push_back(hexadecimal[static_cast<unsigned>(character) >> 4U]);
				encoded.push_back(hexadecimal[static_cast<unsigned>(character) & 0x0FU]);
			}
			return encoded;
		}

		/// A branch in a path keeps its separators; every segment is encoded.
		[[nodiscard]] auto encode_branch(const std::string_view aBranch) -> std::string
		{
			auto encoded = std::string{ };
			auto start   = std::size_t{ 0 };
			while (true) {
				const auto end = aBranch.find('/', start);
				encoded += percent_encode(aBranch.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
				if (end == std::string_view::npos) {
					return encoded;
				}
				encoded.push_back('/');
				start = end + 1U;
			}
		}

		[[nodiscard]] auto valid_target(const application::github_publication_target& aTarget) -> bool
		{
			return admitted_publication_server(aTarget.Server) && core::valid_publication_owner(aTarget.Owner) && core::valid_publication_repository(aTarget.Repository)
				&& core::valid_publication_owner(aTarget.Login) && aTarget.Account.value() != 0U && !aTarget.Host.empty();
		}

		[[nodiscard]] auto repository_path(const application::github_publication_target& aTarget) -> std::string
		{
			return "/repos/" + percent_encode(aTarget.Owner) + "/" + percent_encode(aTarget.Repository);
		}

		[[nodiscard]] auto headers(const bool aBody) -> std::vector<http_header>
		{
			auto out = std::vector<http_header>{
				{ .Name = "Accept", .Value = "application/vnd.github+json" },
				{ .Name = "User-Agent", .Value = "BetterPullReview/0.1" },
				{ .Name = "X-GitHub-Api-Version", .Value = std::string{ rest_version } },
			};
			if (aBody) {
				out.push_back({ .Name = "Content-Type", .Value = "application/json" });
			}
			return out;
		}

		[[nodiscard]] auto request(publication_semantic aSemantic, const application::github_publication_target& aTarget, const publication_operation aOperation, std::string aPath,
								   std::string aBody = { }, std::string aQuery = { }) -> publication_contract_result<publication_wire_request>
		{
			const auto exchange = publication_exchange_for(aOperation);
			if (!exchange || !valid_target(aTarget) || aBody.size() > native_publication_body_limit) {
				return failed(publication_contract_failure::invalid_request);
			}
			const auto body = !aBody.empty();
			return publication_wire_request{
				.Semantic  = std::move(aSemantic),
				.Target    = aTarget,
				.Operation = aOperation,
				.Method    = exchange->Method,
				.Path      = std::move(aPath),
				.Query     = std::move(aQuery),
				.Headers   = headers(body),
				.Body      = std::move(aBody),
			};
		}

		[[nodiscard]] auto hex(const auto& aOid) -> std::string
		{
			return std::string{ core::canonical_hex(aOid).view() };
		}

		[[nodiscard]] auto base64(const std::span<const std::uint8_t> aBytes) -> std::string
		{
			constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
			auto                       out      = std::string{ };
			out.reserve((aBytes.size() + 2U) / 3U * 4U);
			for (std::size_t index = 0; index < aBytes.size(); index += 3U) {
				const auto remaining = aBytes.size() - index;
				const auto value     = (static_cast<std::uint32_t>(aBytes[index]) << 16U) | (remaining > 1U ? static_cast<std::uint32_t>(aBytes[index + 1U]) << 8U : 0U)
					| (remaining > 2U ? static_cast<std::uint32_t>(aBytes[index + 2U]) : 0U);
				out.push_back(alphabet[(value >> 18U) & 63U]);
				out.push_back(alphabet[(value >> 12U) & 63U]);
				out.push_back(remaining > 1U ? alphabet[(value >> 6U) & 63U] : '=');
				out.push_back(remaining > 2U ? alphabet[value & 63U] : '=');
			}
			return out;
		}

		[[nodiscard]] auto octal(const std::uint32_t aMode) -> std::string
		{
			auto text  = std::string{ };
			auto value = aMode;
			do {
				text.insert(text.begin(), static_cast<char>('0' + (value & 7U)));
				value >>= 3U;
			} while (value != 0U);
			return text;
		}

		template <typename T>
		[[nodiscard]] auto document(const publication_wire_response& aResponse, const json_preflight_limits aLimits = { }) -> std::optional<T>
		{
			if (aResponse.Body.empty() || !json_preflight{ aResponse.Body, aLimits }.run()) {
				return std::nullopt;
			}
			auto parsed = T{ };
			if (glz::read<publication_json_options{ }>(parsed, aResponse.Body)) {
				return std::nullopt;
			}
			return parsed;
		}

		[[nodiscard]] auto ascii_equal_fold(const std::string_view aLeft, const std::string_view aRight) -> bool
		{
			return aLeft.size() == aRight.size() && std::ranges::equal(aLeft, aRight, [](const char aA, const char aB) {
					   const auto fold = [](const char aValue) { return aValue >= 'A' && aValue <= 'Z' ? static_cast<char>(aValue + ('a' - 'A')) : aValue; };
					   return fold(aA) == fold(aB);
				   });
		}

		template <typename TKind>
		[[nodiscard]] auto sha_of(const std::string_view aText, const core::git_hash_algorithm aFormat) -> std::optional<core::git_object_oid<TKind>>
		{
			const auto parsed = core::parse_git_blob_oid(aText);
			if (!parsed || parsed->algorithm() != aFormat) {
				return std::nullopt;
			}
			return core::retag<TKind>(*parsed);
		}

		struct sha_json
		{
			std::string Sha;
			struct glaze
			{
				static constexpr auto value = glz::object("sha", &sha_json::Sha);
			};
		};

		struct owner_json
		{
			std::string Login;
			struct glaze
			{
				static constexpr auto value = glz::object("login", &owner_json::Login);
			};
		};

		struct permissions_json
		{
			bool Push = false;
			struct glaze
			{
				static constexpr auto value = glz::object("push", &permissions_json::Push);
			};
		};

		struct repository_json
		{
			std::string                     Name;
			std::string                     FullName;
			owner_json                      Owner;
			bool                            Archived = false;
			std::string                     DefaultBranch;
			std::optional<permissions_json> Permissions;
			struct glaze
			{
				static constexpr auto value = glz::object("name", &repository_json::Name, "full_name", &repository_json::FullName, "owner", &repository_json::Owner, "archived",
														  &repository_json::Archived, "default_branch", &repository_json::DefaultBranch, "permissions", &repository_json::Permissions);
			};
		};

		struct ref_object_json
		{
			std::string Sha;
			std::string Type;
			struct glaze
			{
				static constexpr auto value = glz::object("sha", &ref_object_json::Sha, "type", &ref_object_json::Type);
			};
		};

		struct ref_json
		{
			std::string     Ref;
			ref_object_json Object;
			struct glaze
			{
				static constexpr auto value = glz::object("ref", &ref_json::Ref, "object", &ref_json::Object);
			};
		};

		struct commit_json
		{
			std::string           Sha;
			sha_json              Tree;
			std::vector<sha_json> Parents;
			struct glaze
			{
				static constexpr auto value = glz::object("sha", &commit_json::Sha, "tree", &commit_json::Tree, "parents", &commit_json::Parents);
			};
		};

		struct signature_write_json
		{
			std::string_view Name;
			std::string_view Email;
			std::string      Date;
			struct glaze
			{
				static constexpr auto value = glz::object("name", &signature_write_json::Name, "email", &signature_write_json::Email, "date", &signature_write_json::Date);
			};
		};

		struct commit_write_json
		{
			std::string_view         Message;
			std::string              Tree;
			std::vector<std::string> Parents;
			signature_write_json     Author;
			signature_write_json     Committer;
			struct glaze
			{
				static constexpr auto value = glz::object("message", &commit_write_json::Message, "tree", &commit_write_json::Tree, "parents", &commit_write_json::Parents, "author",
														  &commit_write_json::Author, "committer", &commit_write_json::Committer);
			};
		};

		struct blob_write_json
		{
			std::string      Content;
			std::string_view Encoding = "base64";
			struct glaze
			{
				static constexpr auto value = glz::object("content", &blob_write_json::Content, "encoding", &blob_write_json::Encoding);
			};
		};

		struct tree_entry_write_json
		{
			std::string_view           Path;
			std::string                Mode;
			std::string_view           Type = "blob";
			std::optional<std::string> Sha;
			struct glaze
			{
				static constexpr auto value
					= glz::object("path", &tree_entry_write_json::Path, "mode", &tree_entry_write_json::Mode, "type", &tree_entry_write_json::Type, "sha", &tree_entry_write_json::Sha);
			};
		};

		struct tree_write_json
		{
			std::string                        BaseTree;
			std::vector<tree_entry_write_json> Tree;
			struct glaze
			{
				static constexpr auto value = glz::object("base_tree", &tree_write_json::BaseTree, "tree", &tree_write_json::Tree);
			};
		};

		struct ref_write_json
		{
			std::string Ref;
			std::string Sha;
			struct glaze
			{
				static constexpr auto value = glz::object("ref", &ref_write_json::Ref, "sha", &ref_write_json::Sha);
			};
		};

		struct pull_write_json
		{
			std::string_view Title;
			std::string_view Body;
			std::string_view Head;
			std::string_view Base;
			bool             Draft = false;
			struct glaze
			{
				static constexpr auto value = glz::object("title", &pull_write_json::Title, "body", &pull_write_json::Body, "head", &pull_write_json::Head, "base", &pull_write_json::Base,
														  "draft", &pull_write_json::Draft);
			};
		};

		struct pull_repository_json
		{
			std::string Name;
			owner_json  Owner;
			struct glaze
			{
				static constexpr auto value = glz::object("name", &pull_repository_json::Name, "owner", &pull_repository_json::Owner);
			};
		};

		struct pull_head_json
		{
			std::string                         Ref;
			std::string                         Sha;
			std::optional<pull_repository_json> Repo;
			struct glaze
			{
				static constexpr auto value = glz::object("ref", &pull_head_json::Ref, "sha", &pull_head_json::Sha, "repo", &pull_head_json::Repo);
			};
		};

		struct pull_base_json
		{
			std::string Ref;
			std::string Sha;
			struct glaze
			{
				static constexpr auto value = glz::object("ref", &pull_base_json::Ref, "sha", &pull_base_json::Sha);
			};
		};

		struct pull_json
		{
			std::uint64_t             Number = 0;
			std::string               NodeId;
			std::string               HtmlUrl;
			std::string               State;
			std::optional<bool>       Draft;
			std::string               Title;
			std::optional<owner_json> User;
			pull_head_json            Head;
			pull_base_json            Base;
			struct glaze
			{
				static constexpr auto value = glz::object("number", &pull_json::Number, "node_id", &pull_json::NodeId, "html_url", &pull_json::HtmlUrl, "state", &pull_json::State, "draft",
														  &pull_json::Draft, "title", &pull_json::Title, "user", &pull_json::User, "head", &pull_json::Head, "base", &pull_json::Base);
			};
		};

		struct message_json
		{
			std::string Message;
			struct glaze
			{
				static constexpr auto value = glz::object("message", &message_json::Message);
			};
		};

		template <typename TWrite>
		[[nodiscard]] auto encode(const TWrite& aValue) -> std::optional<std::string>
		{
			auto out = std::string{ };
			if (glz::write<write_json_options{ }>(aValue, out)) {
				return std::nullopt;
			}
			return out;
		}

		[[nodiscard]] auto pull_observation(const pull_json& aPull, const core::git_hash_algorithm aFormat) -> std::optional<application::github_pull_request_observation>
		{
			const auto head = sha_of<core::git_commit_oid_kind>(aPull.Head.Sha, aFormat);
			const auto base = sha_of<core::git_commit_oid_kind>(aPull.Base.Sha, aFormat);
			if (aPull.Number == 0U || !head || !base || aPull.NodeId.empty() || aPull.NodeId.size() > 256U || aPull.HtmlUrl.size() > core::maximum_publication_remote_bytes
				|| !aPull.HtmlUrl.starts_with("https://") || (aPull.State != "open" && aPull.State != "closed")) {
				return std::nullopt;
			}
			return application::github_pull_request_observation{
				.Number         = aPull.Number,
				.NodeId         = aPull.NodeId,
				.Url            = aPull.HtmlUrl,
				.State          = aPull.State,
				.Draft          = aPull.Draft.value_or(false),
				.Title          = aPull.Title,
				.HeadRef        = aPull.Head.Ref,
				.HeadOwner      = aPull.Head.Repo ? aPull.Head.Repo->Owner.Login : std::string{ },
				.HeadRepository = aPull.Head.Repo ? aPull.Head.Repo->Name : std::string{ },
				.HeadCommit     = *head,
				.BaseRef        = aPull.Base.Ref,
				.BaseCommit     = *base,
				.Author         = aPull.User ? aPull.User->Login : std::string{ },
			};
		}

		/// Days since 1970-01-01 to a civil date (Howard Hinnant's algorithm).
		[[nodiscard]] auto civil(const std::int64_t aDays) -> std::array<std::int64_t, 3>
		{
			const auto z   = aDays + 719468;
			const auto era = (z >= 0 ? z : z - 146096) / 146097;
			const auto doe = z - era * 146097;
			const auto yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
			const auto doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
			const auto mp  = (5 * doy + 2) / 153;
			const auto d   = doy - (153 * mp + 2) / 5 + 1;
			const auto m   = mp < 10 ? mp + 3 : mp - 9;
			return { yoe + era * 400 + (m <= 2 ? 1 : 0), m, d };
		}

		[[nodiscard]] auto two(const std::int64_t aValue) -> std::string
		{
			return std::string{ static_cast<char>('0' + aValue / 10), static_cast<char>('0' + aValue % 10) };
		}
	}

	auto publication_exchange_for(const publication_operation aOperation) noexcept -> std::optional<publication_exchange>
	{
		switch (aOperation) {
		case publication_operation::observe_repository:
		case publication_operation::find_pull_requests:
			return publication_exchange{ .Method = http_method::get };
		case publication_operation::observe_branch:
		case publication_operation::observe_commit:
		case publication_operation::observe_tree:
		case publication_operation::observe_blob:
		case publication_operation::observe_pull_request:
			return publication_exchange{ .Method = http_method::get, .AdmittedStatus = 404 };
		case publication_operation::create_blob:
		case publication_operation::create_tree:
		case publication_operation::create_commit:
			return publication_exchange{ .Method = http_method::post, .SuccessfulStatus = 201, .Write = true };
		case publication_operation::create_branch:
		case publication_operation::create_pull_request:
			return publication_exchange{ .Method = http_method::post, .SuccessfulStatus = 201, .AdmittedStatus = 422, .Write = true };
		}
		return std::nullopt;
	}

	auto admitted_publication_server(const application::github_server_contract aServer) noexcept -> bool
	{
		return aServer == application::github_server_contract::github_com_2022_11_28;
	}

	auto publication_date(const core::git_signature& aSignature) -> std::string
	{
		const auto local   = aSignature.Seconds + static_cast<std::int64_t>(aSignature.OffsetMinutes) * 60;
		const auto days    = local >= 0 ? local / 86400 : (local - 86399) / 86400;
		const auto seconds = local - days * 86400;
		const auto date    = civil(days);
		auto       text    = std::to_string(date[0]) + "-" + two(date[1]) + "-" + two(date[2]) + "T" + two(seconds / 3600) + ":" + two((seconds / 60) % 60) + ":" + two(seconds % 60);
		if (aSignature.OffsetMinutes == 0) {
			return text + "Z";
		}
		const auto minutes = aSignature.OffsetMinutes < 0 ? -aSignature.OffsetMinutes : aSignature.OffsetMinutes;
		return text + (aSignature.OffsetMinutes < 0 ? "-" : "+") + two(minutes / 60) + ":" + two(minutes % 60);
	}

	auto compile_observe_repository(const application::github_publication_target& aTarget) -> publication_contract_result<publication_wire_request>
	{
		return request(publication_observe_repository{ }, aTarget, publication_operation::observe_repository, repository_path(aTarget));
	}

	auto compile_observe_branch(const application::github_publication_target& aTarget, const std::string& aBranch) -> publication_contract_result<publication_wire_request>
	{
		if (!core::valid_publication_branch(aBranch)) {
			return failed(publication_contract_failure::invalid_request);
		}
		return request(publication_observe_branch{ .Branch = aBranch }, aTarget, publication_operation::observe_branch, repository_path(aTarget) + "/git/ref/heads/" + encode_branch(aBranch));
	}

	auto compile_observe_commit(const application::github_publication_target& aTarget, const core::git_commit_oid& aCommit) -> publication_contract_result<publication_wire_request>
	{
		if (!aCommit.valid() || aCommit.algorithm() != core::git_hash_algorithm::sha1) {
			return failed(publication_contract_failure::invalid_request);
		}
		return request(publication_observe_commit{ .Commit = aCommit }, aTarget, publication_operation::observe_commit, repository_path(aTarget) + "/git/commits/" + hex(aCommit));
	}

	auto compile_observe_tree(const application::github_publication_target& aTarget, const core::git_tree_oid& aTree) -> publication_contract_result<publication_wire_request>
	{
		if (!aTree.valid() || aTree.algorithm() != core::git_hash_algorithm::sha1) {
			return failed(publication_contract_failure::invalid_request);
		}
		return request(publication_observe_tree{ .Tree = aTree }, aTarget, publication_operation::observe_tree, repository_path(aTarget) + "/git/trees/" + hex(aTree));
	}

	auto compile_observe_blob(const application::github_publication_target& aTarget, const core::git_entry_oid& aBlob) -> publication_contract_result<publication_wire_request>
	{
		if (!aBlob.valid() || aBlob.algorithm() != core::git_hash_algorithm::sha1) {
			return failed(publication_contract_failure::invalid_request);
		}
		return request(publication_observe_blob{ .Blob = aBlob }, aTarget, publication_operation::observe_blob, repository_path(aTarget) + "/git/blobs/" + hex(aBlob));
	}

	auto compile_create_blob(const application::github_publication_target& aTarget, const std::span<const std::uint8_t> aBytes) -> publication_contract_result<publication_wire_request>
	{
		if (aBytes.size() > application::maximum_github_publication_blob_bytes) {
			return failed(publication_contract_failure::invalid_request);
		}
		const auto body = encode(blob_write_json{ .Content = base64(aBytes) });
		if (!body) {
			return failed(publication_contract_failure::invalid_request);
		}
		return request(publication_create_blob{ .Bytes = std::vector<std::uint8_t>(aBytes.begin(), aBytes.end()) }, aTarget, publication_operation::create_blob, repository_path(aTarget) + "/git/blobs", *body);
	}

	auto compile_create_tree(const application::github_publication_target& aTarget, const core::git_tree_oid& aBase, const std::vector<core::git_tree_entry>& aChanges,
							 const std::vector<std::string>& aDeletions) -> publication_contract_result<publication_wire_request>
	{
		if (!aBase.valid() || aBase.algorithm() != core::git_hash_algorithm::sha1 || aChanges.size() + aDeletions.size() == 0U) {
			return failed(publication_contract_failure::invalid_request);
		}
		auto tree = tree_write_json{ .BaseTree = hex(aBase) };
		for (const auto& change : aChanges) {
			if (!change.Object.valid() || change.Object.algorithm() != core::git_hash_algorithm::sha1 || !core::valid_git_entry_mode(change.Mode) || change.Mode == core::git_mode_gitlink
				|| !core::valid_cycle_path(change.Path)) {
				return failed(publication_contract_failure::invalid_request);
			}
			tree.Tree.push_back({ .Path = change.Path, .Mode = octal(change.Mode), .Sha = hex(change.Object) });
		}
		for (const auto& deletion : aDeletions) {
			if (!core::valid_cycle_path(deletion)) {
				return failed(publication_contract_failure::invalid_request);
			}
			// A null identity removes the entry from the base tree.
			tree.Tree.push_back({ .Path = deletion, .Mode = octal(core::git_mode_regular), .Sha = std::nullopt });
		}
		const auto body = encode(tree);
		if (!body) {
			return failed(publication_contract_failure::invalid_request);
		}
		return request(publication_create_tree{ .Base = aBase, .Changes = aChanges, .Deletions = aDeletions }, aTarget, publication_operation::create_tree, repository_path(aTarget) + "/git/trees", *body);
	}

	auto compile_create_commit(const application::github_publication_target& aTarget, const core::git_commit_content& aCommit) -> publication_contract_result<publication_wire_request>
	{
		if (!core::encode_git_commit(aCommit) || aCommit.Tree.algorithm() != core::git_hash_algorithm::sha1 || aCommit.Parents.size() != 1U) {
			return failed(publication_contract_failure::invalid_request);
		}
		auto write = commit_write_json{
			.Message   = aCommit.Message,
			.Tree      = hex(aCommit.Tree),
			.Parents   = { hex(aCommit.Parents.front()) },
			.Author    = { .Name = aCommit.Author.Name, .Email = aCommit.Author.Email, .Date = publication_date(aCommit.Author) },
			.Committer = { .Name = aCommit.Committer.Name, .Email = aCommit.Committer.Email, .Date = publication_date(aCommit.Committer) },
		};
		const auto body = encode(write);
		if (!body) {
			return failed(publication_contract_failure::invalid_request);
		}
		return request(publication_create_commit{ .Commit = aCommit }, aTarget, publication_operation::create_commit, repository_path(aTarget) + "/git/commits", *body);
	}

	auto compile_create_branch(const application::github_publication_target& aTarget, const std::string& aBranch, const core::git_commit_oid& aCommit)
		-> publication_contract_result<publication_wire_request>
	{
		if (!core::valid_publication_branch(aBranch) || !aCommit.valid() || aCommit.algorithm() != core::git_hash_algorithm::sha1) {
			return failed(publication_contract_failure::invalid_request);
		}
		const auto body = encode(ref_write_json{ .Ref = "refs/heads/" + aBranch, .Sha = hex(aCommit) });
		if (!body) {
			return failed(publication_contract_failure::invalid_request);
		}
		return request(publication_create_branch{ .Branch = aBranch, .Commit = aCommit }, aTarget, publication_operation::create_branch, repository_path(aTarget) + "/git/refs", *body);
	}

	auto compile_create_pull_request(const application::github_publication_target& aTarget, const application::github_pull_request_draft& aDraft)
		-> publication_contract_result<publication_wire_request>
	{
		if (!core::valid_publication_branch(aDraft.Head) || !core::valid_publication_branch(aDraft.Base) || aDraft.Head == aDraft.Base || aDraft.Title.empty()
			|| aDraft.Title.size() > core::maximum_candidate_title_bytes || aDraft.Title.find_first_of("\r\n") != std::string::npos || aDraft.Body.size() > core::maximum_publication_body_bytes) {
			return failed(publication_contract_failure::invalid_request);
		}
		const auto body = encode(pull_write_json{ .Title = aDraft.Title, .Body = aDraft.Body, .Head = aDraft.Head, .Base = aDraft.Base, .Draft = aDraft.Draft });
		if (!body) {
			return failed(publication_contract_failure::invalid_request);
		}
		return request(publication_create_pull_request{ .Draft = aDraft }, aTarget, publication_operation::create_pull_request, repository_path(aTarget) + "/pulls", *body);
	}

	auto compile_find_pull_requests(const application::github_publication_target& aTarget, const std::string& aHead, const std::string& aBase)
		-> publication_contract_result<publication_wire_request>
	{
		if (!core::valid_publication_branch(aHead) || !core::valid_publication_branch(aBase)) {
			return failed(publication_contract_failure::invalid_request);
		}
		return request(publication_find_pull_requests{ .Head = aHead, .Base = aBase }, aTarget, publication_operation::find_pull_requests, repository_path(aTarget) + "/pulls", { },
					   "state=all&head=" + percent_encode(aTarget.Owner + ":" + aHead) + "&base=" + percent_encode(aBase) + "&per_page=100");
	}

	auto compile_observe_pull_request(const application::github_publication_target& aTarget, const std::uint64_t aNumber) -> publication_contract_result<publication_wire_request>
	{
		if (aNumber == 0U) {
			return failed(publication_contract_failure::invalid_request);
		}
		return request(publication_observe_pull_request{ .Number = aNumber }, aTarget, publication_operation::observe_pull_request, repository_path(aTarget) + "/pulls/" + std::to_string(aNumber));
	}

	auto compile_publication(const application::github_publication_target& aTarget, const publication_semantic& aSemantic) -> publication_contract_result<publication_wire_request>
	{
		return std::visit(
			[&aTarget](const auto& aRequest) -> publication_contract_result<publication_wire_request> {
				using request_type = std::decay_t<decltype(aRequest)>;
				if constexpr (std::is_same_v<request_type, publication_observe_repository>) {
					return compile_observe_repository(aTarget);
				} else if constexpr (std::is_same_v<request_type, publication_observe_branch>) {
					return compile_observe_branch(aTarget, aRequest.Branch);
				} else if constexpr (std::is_same_v<request_type, publication_observe_commit>) {
					return compile_observe_commit(aTarget, aRequest.Commit);
				} else if constexpr (std::is_same_v<request_type, publication_observe_tree>) {
					return compile_observe_tree(aTarget, aRequest.Tree);
				} else if constexpr (std::is_same_v<request_type, publication_observe_blob>) {
					return compile_observe_blob(aTarget, aRequest.Blob);
				} else if constexpr (std::is_same_v<request_type, publication_create_blob>) {
					return compile_create_blob(aTarget, aRequest.Bytes);
				} else if constexpr (std::is_same_v<request_type, publication_create_tree>) {
					return compile_create_tree(aTarget, aRequest.Base, aRequest.Changes, aRequest.Deletions);
				} else if constexpr (std::is_same_v<request_type, publication_create_commit>) {
					return compile_create_commit(aTarget, aRequest.Commit);
				} else if constexpr (std::is_same_v<request_type, publication_create_branch>) {
					return compile_create_branch(aTarget, aRequest.Branch, aRequest.Commit);
				} else if constexpr (std::is_same_v<request_type, publication_create_pull_request>) {
					return compile_create_pull_request(aTarget, aRequest.Draft);
				} else if constexpr (std::is_same_v<request_type, publication_find_pull_requests>) {
					return compile_find_pull_requests(aTarget, aRequest.Head, aRequest.Base);
				} else {
					return compile_observe_pull_request(aTarget, aRequest.Number);
				}
			},
			aSemantic);
	}

	auto validate_outbound_publication_request(const publication_wire_request& aRequest) -> bool
	{
		// The whole request must be what its own typed request compiles to.
		const auto compiled = compile_publication(aRequest.Target, aRequest.Semantic);
		return compiled.has_value() && *compiled == aRequest;
	}

	auto parse_repository(const publication_wire_response& aResponse, const application::github_publication_target& aTarget)
		-> publication_contract_result<application::github_publication_capability>
	{
		if (aResponse.Status != 200) {
			return failed(publication_contract_failure::unexpected_status);
		}
		const auto parsed = document<repository_json>(aResponse);
		if (!parsed) {
			return failed(publication_contract_failure::malformed_response);
		}
		// GitHub compares names without case; a redirect to a renamed or
		// transferred repository is another repository.
		if (!ascii_equal_fold(parsed->FullName, aTarget.Owner + "/" + aTarget.Repository) || !ascii_equal_fold(parsed->Owner.Login, aTarget.Owner)
			|| !ascii_equal_fold(parsed->Name, aTarget.Repository)) {
			return failed(publication_contract_failure::identity_mismatch);
		}
		return application::github_publication_capability{
			.Owner         = parsed->Owner.Login,
			.Repository    = parsed->Name,
			.CanPush       = parsed->Permissions && parsed->Permissions->Push,
			.Archived      = parsed->Archived,
			.DefaultBranch = parsed->DefaultBranch,
		};
	}

	auto parse_branch(const publication_wire_response& aResponse, const std::string& aBranch, const core::git_hash_algorithm aFormat)
		-> publication_contract_result<std::optional<core::git_commit_oid>>
	{
		if (aResponse.Status == 404) {
			return std::optional<core::git_commit_oid>{ };
		}
		if (aResponse.Status != 200) {
			return failed(publication_contract_failure::unexpected_status);
		}
		const auto parsed = document<ref_json>(aResponse);
		if (!parsed) {
			return failed(publication_contract_failure::malformed_response);
		}
		const auto commit = sha_of<core::git_commit_oid_kind>(parsed->Object.Sha, aFormat);
		if (parsed->Ref != "refs/heads/" + aBranch || parsed->Object.Type != "commit" || !commit) {
			return failed(publication_contract_failure::identity_mismatch);
		}
		return commit;
	}

	auto parse_commit(const publication_wire_response& aResponse, const core::git_commit_oid& aCommit) -> publication_contract_result<std::optional<application::github_publication_commit>>
	{
		if (aResponse.Status == 404) {
			return std::optional<application::github_publication_commit>{ };
		}
		if (aResponse.Status != 200) {
			return failed(publication_contract_failure::unexpected_status);
		}
		const auto parsed = document<commit_json>(aResponse);
		if (!parsed) {
			return failed(publication_contract_failure::malformed_response);
		}
		const auto sha  = sha_of<core::git_commit_oid_kind>(parsed->Sha, aCommit.algorithm());
		const auto tree = sha_of<core::git_tree_oid_kind>(parsed->Tree.Sha, aCommit.algorithm());
		if (!sha || *sha != aCommit || !tree || parsed->Parents.size() > 64U) {
			return failed(publication_contract_failure::identity_mismatch);
		}
		auto observed = application::github_publication_commit{ .Commit = *sha, .Tree = *tree };
		for (const auto& parent : parsed->Parents) {
			const auto value = sha_of<core::git_commit_oid_kind>(parent.Sha, aCommit.algorithm());
			if (!value) {
				return failed(publication_contract_failure::malformed_response);
			}
			observed.Parents.push_back(*value);
		}
		return std::optional{ std::move(observed) };
	}

	auto parse_tree_presence(const publication_wire_response& aResponse, const core::git_tree_oid& aTree) -> publication_contract_result<bool>
	{
		if (aResponse.Status == 404) {
			return false;
		}
		const auto parsed = aResponse.Status == 200 ? document<sha_json>(aResponse, listing_limits) : std::nullopt;
		if (!parsed) {
			return failed(aResponse.Status == 200 ? publication_contract_failure::malformed_response : publication_contract_failure::unexpected_status);
		}
		const auto sha = sha_of<core::git_tree_oid_kind>(parsed->Sha, aTree.algorithm());
		if (!sha || *sha != aTree) {
			return failed(publication_contract_failure::identity_mismatch);
		}
		return true;
	}

	auto parse_blob_presence(const publication_wire_response& aResponse, const core::git_entry_oid& aBlob) -> publication_contract_result<bool>
	{
		if (aResponse.Status == 404) {
			return false;
		}
		const auto parsed = aResponse.Status == 200 ? document<sha_json>(aResponse) : std::nullopt;
		if (!parsed) {
			return failed(aResponse.Status == 200 ? publication_contract_failure::malformed_response : publication_contract_failure::unexpected_status);
		}
		const auto sha = sha_of<core::git_entry_oid_kind>(parsed->Sha, aBlob.algorithm());
		if (!sha || *sha != aBlob) {
			return failed(publication_contract_failure::identity_mismatch);
		}
		return true;
	}

	auto parse_created_sha(const publication_wire_response& aResponse, const core::git_hash_algorithm aFormat) -> publication_contract_result<std::array<std::uint8_t, 32>>
	{
		if (aResponse.Status != 201) {
			return failed(publication_contract_failure::unexpected_status);
		}
		const auto parsed = document<sha_json>(aResponse, listing_limits);
		if (!parsed) {
			return failed(publication_contract_failure::malformed_response);
		}
		const auto sha = sha_of<core::git_entry_oid_kind>(parsed->Sha, aFormat);
		if (!sha) {
			return failed(publication_contract_failure::malformed_response);
		}
		return sha->bytes();
	}

	auto parse_created_commit(const publication_wire_response& aResponse, const core::git_commit_content& aCommit, const core::git_commit_oid& aExpected) -> publication_contract_result<void>
	{
		if (aResponse.Status != 201) {
			return failed(publication_contract_failure::unexpected_status);
		}
		const auto parsed = document<commit_json>(aResponse);
		if (!parsed) {
			return failed(publication_contract_failure::malformed_response);
		}
		const auto sha     = sha_of<core::git_commit_oid_kind>(parsed->Sha, aExpected.algorithm());
		const auto tree    = sha_of<core::git_tree_oid_kind>(parsed->Tree.Sha, aExpected.algorithm());
		auto       parents = std::vector<core::git_commit_oid>{ };
		for (const auto& parent : parsed->Parents) {
			if (const auto value = sha_of<core::git_commit_oid_kind>(parent.Sha, aExpected.algorithm())) {
				parents.push_back(*value);
			}
		}
		// The server's commit must be exactly the one computed locally.
		if (!sha || *sha != aExpected || !tree || *tree != aCommit.Tree || parents != aCommit.Parents) {
			return failed(publication_contract_failure::identity_mismatch);
		}
		return { };
	}

	auto parse_created_branch(const publication_wire_response& aResponse, const std::string& aBranch, const core::git_commit_oid& aCommit)
		-> publication_contract_result<application::github_branch_creation>
	{
		if (aResponse.Status == 422) {
			const auto parsed = document<message_json>(aResponse);
			if (!parsed) {
				return failed(publication_contract_failure::malformed_response);
			}
			if (parsed->Message != "Reference already exists") {
				return failed(publication_contract_failure::unexpected_status);
			}
			return application::github_branch_creation::exists;
		}
		if (aResponse.Status != 201) {
			return failed(publication_contract_failure::unexpected_status);
		}
		const auto parsed = document<ref_json>(aResponse);
		if (!parsed) {
			return failed(publication_contract_failure::malformed_response);
		}
		const auto commit = sha_of<core::git_commit_oid_kind>(parsed->Object.Sha, aCommit.algorithm());
		if (parsed->Ref != "refs/heads/" + aBranch || parsed->Object.Type != "commit" || !commit || *commit != aCommit) {
			return failed(publication_contract_failure::identity_mismatch);
		}
		return application::github_branch_creation::created;
	}

	auto parse_pull_request(const publication_wire_response& aResponse, const core::git_hash_algorithm aFormat)
		-> publication_contract_result<std::optional<application::github_pull_request_observation>>
	{
		if (aResponse.Status == 404) {
			return std::optional<application::github_pull_request_observation>{ };
		}
		if (aResponse.Status != 200 && aResponse.Status != 201) {
			return failed(publication_contract_failure::unexpected_status);
		}
		const auto parsed = document<pull_json>(aResponse);
		if (!parsed) {
			return failed(publication_contract_failure::malformed_response);
		}
		auto observed = pull_observation(*parsed, aFormat);
		if (!observed) {
			return failed(publication_contract_failure::malformed_response);
		}
		return observed;
	}

	auto parse_pull_requests(const publication_wire_response& aResponse, const core::git_hash_algorithm aFormat)
		-> publication_contract_result<std::vector<application::github_pull_request_observation>>
	{
		if (aResponse.Status != 200) {
			return failed(publication_contract_failure::unexpected_status);
		}
		// Every matching pull request, or none: a further page is refused.
		if (std::ranges::any_of(aResponse.Headers, [](const http_header& aHeader) { return ascii_equal_fold(aHeader.Name, "Link") && aHeader.Value.find("rel=\"next\"") != std::string::npos; })) {
			return failed(publication_contract_failure::malformed_response);
		}
		const auto parsed = document<std::vector<pull_json>>(aResponse, listing_limits);
		if (!parsed || parsed->size() > 100U) {
			return failed(publication_contract_failure::malformed_response);
		}
		auto out = std::vector<application::github_pull_request_observation>{ };
		for (const auto& pull : *parsed) {
			auto observed = pull_observation(pull, aFormat);
			if (!observed) {
				return failed(publication_contract_failure::malformed_response);
			}
			out.push_back(std::move(*observed));
		}
		return out;
	}
}
