#include "github_mutation_contract.hpp"

#include "bpr/application/review_mutation_coordinator.hpp"
#include "bpr/core/sha256.hpp"

#include <QDateTime>
#include <QString>

#include <glaze/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <concepts>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace bpr::adapters::github::detail {
	namespace {
		constexpr std::string_view rest_version = "2022-11-28";

		[[nodiscard]] auto failed(const review_mutation_contract_failure aFailure)
			-> std::unexpected<review_mutation_contract_failure>
		{
			return std::unexpected{ aFailure };
		}

		[[nodiscard]] auto bounded_text(
			const std::string_view aText,
			const std::size_t      aMaximum) noexcept -> bool
		{
			return !aText.empty() && aText.size() <= aMaximum
				&& std::ranges::all_of(aText, [](const unsigned char aCharacter) {
					   return aCharacter >= 0x20U && aCharacter != 0x7FU;
				   });
		}

		[[nodiscard]] auto percent_encode(const std::string_view aValue) -> std::string
		{
			constexpr std::string_view hexadecimal = "0123456789ABCDEF";
			std::string                encoded;
			encoded.reserve(aValue.size());
			for (const auto raw : aValue) {
				const auto character  = static_cast<unsigned char>(raw);
				const auto unreserved = std::isalnum(character) != 0 || character == '-'
					|| character == '_' || character == '.' || character == '~';
				if (unreserved) {
					encoded.push_back(static_cast<char>(character));
					continue;
				}
				encoded.push_back('%');
				encoded.push_back(hexadecimal[(static_cast<unsigned>(character) >> 4U) & 0x0FU]);
				encoded.push_back(hexadecimal[character & 0x0FU]);
			}
			return encoded;
		}

		[[nodiscard]] constexpr auto valid_server(
			const application::github_server_contract aServer) noexcept -> bool
		{
			return aServer == application::github_server_contract::github_com_2022_11_28
				|| aServer == application::github_server_contract::ghes_3_18_2022_11_28;
		}

		[[nodiscard]] constexpr auto rest_prefix(
			const application::github_server_contract aServer) noexcept -> std::string_view
		{
			return aServer == application::github_server_contract::ghes_3_18_2022_11_28
				? std::string_view{ "/api/v3" }
				: std::string_view{ };
		}

		/// [[BPR-0657]]. GitHub's GraphQL root: beside the REST root on
		/// github.com, under `/api` on GitHub Enterprise Server.
		[[nodiscard]] constexpr auto graphql_path(
			const application::github_server_contract aServer) noexcept -> std::string_view
		{
			return aServer == application::github_server_contract::ghes_3_18_2022_11_28
				? std::string_view{ "/api/graphql" }
				: std::string_view{ "/graphql" };
		}

		/// The operation names are the ones the read contract reserves as
		/// mutations, so a document that reaches the read transport is refused
		/// by name.
		constexpr auto reply_document = std::string_view{
			"mutation ReplyToThread($thread:ID!,$body:String!){"
			"addPullRequestReviewThreadReply(input:{pullRequestReviewThreadId:$thread,body:$body}){"
			"comment{id databaseId body path line commit{oid} author{login} viewerDidAuthor}}}"
		};
		constexpr auto resolve_document = std::string_view{
			"mutation ResolveThread($thread:ID!){resolveReviewThread(input:{threadId:$thread}){"
			"thread{id isResolved}}}"
		};
		constexpr auto unresolve_document = std::string_view{
			"mutation UnresolveThread($thread:ID!){unresolveReviewThread(input:{threadId:$thread}){"
			"thread{id isResolved}}}"
		};

		[[nodiscard]] auto graphql_token(const std::string_view aValue) noexcept -> bool
		{
			return !aValue.empty() && aValue.size() <= 256U
				&& std::ranges::all_of(aValue, [](const char aCharacter) {
					   const auto byte = static_cast<unsigned char>(aCharacter);
					   return byte >= 0x21U && byte <= 0x7eU && aCharacter != '"' && aCharacter != '\\';
				   });
		}

		[[nodiscard]] auto utf8_round_trip(const std::string_view aText) -> bool
		{
			const auto decoded = QString::fromUtf8(
				aText.data(), static_cast<qsizetype>(aText.size()));
			const auto encoded = decoded.toUtf8();
			return static_cast<std::size_t>(encoded.size()) == aText.size()
				&& std::equal(encoded.begin(), encoded.end(), aText.begin());
		}

		[[nodiscard]] auto common_headers() -> std::vector<http_header>
		{
			return {
				{ .Name = "Accept", .Value = "application/vnd.github+json" },
				{ .Name = "User-Agent", .Value = "BetterPullReview/0.1" },
				{ .Name = "X-GitHub-Api-Version", .Value = std::string{ rest_version } },
			};
		}

		[[nodiscard]] auto pull_request_path(
			const application::review_mutation_target_binding& aTarget,
			const application::github_server_contract          aServer)
			-> std::string
		{
			return std::string{ rest_prefix(aServer) } + "/repos/"
				+ percent_encode(aTarget.Repository.Owner) + "/"
				+ percent_encode(aTarget.Repository.Name) + "/pulls/"
				+ std::to_string(aTarget.Number.value());
		}

		struct review_submission_names
		{
			std::string_view Event;
			std::string_view State;
		};

		struct review_submission_json
		{
			std::string_view Body;
			std::string_view CommitId;
			std::string_view Event;

			struct glaze
			{
				static constexpr auto value = glz::object(
					"body", &review_submission_json::Body,
					"commit_id", &review_submission_json::CommitId,
					"event", &review_submission_json::Event);
			};
		};

		struct review_author_json
		{
			std::uint64_t Id = 0U;
			std::string   NodeId;
			std::string   Login;

			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &review_author_json::Id,
					"node_id", &review_author_json::NodeId,
					"login", &review_author_json::Login);
			};
		};

		struct review_response_json
		{
			std::uint64_t      Id = 0U;
			std::string        NodeId;
			std::string        CommitId;
			std::string        Body;
			std::string        State;
			std::string        SubmittedAt;
			std::string        PullRequestUrl;
			review_author_json User;

			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &review_response_json::Id,
					"node_id", &review_response_json::NodeId,
					"commit_id", &review_response_json::CommitId,
					"body", &review_response_json::Body,
					"state", &review_response_json::State,
					"submitted_at", &review_response_json::SubmittedAt,
					"pull_request_url", &review_response_json::PullRequestUrl,
					"user", &review_response_json::User);
			};
		};

		struct review_list_json
		{
			std::uint64_t                     Id = 0U;
			std::string                       NodeId;
			std::string                       State;
			std::string                       PullRequestUrl;
			std::optional<std::string>        Body;
			std::optional<std::string>        CommitId;
			std::optional<std::string>        SubmittedAt;
			std::optional<review_author_json> User;
			struct glaze
			{
				static constexpr auto value = glz::object("id", &review_list_json::Id, "node_id", &review_list_json::NodeId, "state", &review_list_json::State, "pull_request_url", &review_list_json::PullRequestUrl, "body", &review_list_json::Body, "commit_id", &review_list_json::CommitId, "submitted_at", &review_list_json::SubmittedAt, "user", &review_list_json::User);
			};
		};
		struct recovery_repository_json
		{
			std::uint64_t Id = 0U;
			std::string   NodeId;
			std::string   FullName;
			struct glaze
			{
				static constexpr auto value = glz::object("id", &recovery_repository_json::Id, "node_id", &recovery_repository_json::NodeId, "full_name", &recovery_repository_json::FullName);
			};
		};
		struct recovery_base_json
		{
			recovery_repository_json Repository;
			struct glaze
			{
				static constexpr auto value = glz::object("repo", &recovery_base_json::Repository);
			};
		};
		struct recovery_target_json
		{
			std::uint64_t      Id = 0U;
			std::string        NodeId;
			std::uint64_t      Number = 0U;
			recovery_base_json Base;
			struct glaze
			{
				static constexpr auto value = glz::object("id", &recovery_target_json::Id, "node_id", &recovery_target_json::NodeId, "number", &recovery_target_json::Number, "base", &recovery_target_json::Base);
			};
		};
		struct comment_submission_json
		{
			std::string_view Body;
			std::string_view CommitId;
			std::uint64_t    Line = 0U;
			std::string_view Path;
			std::string_view Side;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"body", &comment_submission_json::Body,
					"commit_id", &comment_submission_json::CommitId,
					"line", &comment_submission_json::Line,
					"path", &comment_submission_json::Path,
					"side", &comment_submission_json::Side);
			};
		};

		struct comment_response_json
		{
			std::uint64_t      Id = 0U;
			std::string        NodeId;
			std::string        Path;
			std::string        CommitId;
			std::string        Side;
			std::uint64_t      Line = 0U;
			std::string        Body;
			review_author_json User;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &comment_response_json::Id,
					"node_id", &comment_response_json::NodeId,
					"path", &comment_response_json::Path,
					"commit_id", &comment_response_json::CommitId,
					"side", &comment_response_json::Side,
					"line", &comment_response_json::Line,
					"body", &comment_response_json::Body,
					"user", &comment_response_json::User);
			};
		};

		// [[BPR-0657]]. The GraphQL envelopes, outbound and inbound.
		struct reply_variables_json
		{
			std::string_view Thread;
			std::string_view Body;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"thread", &reply_variables_json::Thread,
					"body", &reply_variables_json::Body);
			};
		};
		struct reply_query_json
		{
			std::string_view     Query;
			reply_variables_json Variables;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"query", &reply_query_json::Query,
					"variables", &reply_query_json::Variables);
			};
		};
		struct resolution_variables_json
		{
			std::string_view Thread;
			struct glaze
			{
				static constexpr auto value = glz::object("thread", &resolution_variables_json::Thread);
			};
		};
		struct resolution_query_json
		{
			std::string_view          Query;
			resolution_variables_json Variables;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"query", &resolution_query_json::Query,
					"variables", &resolution_query_json::Variables);
			};
		};
		struct reply_commit_json
		{
			std::string Oid;
			struct glaze
			{
				static constexpr auto value = glz::object("oid", &reply_commit_json::Oid);
			};
		};
		struct reply_author_json
		{
			std::string Login;
			struct glaze
			{
				static constexpr auto value = glz::object("login", &reply_author_json::Login);
			};
		};
		struct reply_comment_json
		{
			std::string       Id;
			std::uint64_t     DatabaseId = 0U;
			std::string       Body;
			std::string       Path;
			std::uint64_t     Line = 0U;
			reply_commit_json Commit;
			reply_author_json Author;
			bool              ViewerDidAuthor = false;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &reply_comment_json::Id,
					"databaseId", &reply_comment_json::DatabaseId,
					"body", &reply_comment_json::Body,
					"path", &reply_comment_json::Path,
					"line", &reply_comment_json::Line,
					"commit", &reply_comment_json::Commit,
					"author", &reply_comment_json::Author,
					"viewerDidAuthor", &reply_comment_json::ViewerDidAuthor);
			};
		};
		struct reply_payload_json
		{
			reply_comment_json Comment;
			struct glaze
			{
				static constexpr auto value = glz::object("comment", &reply_payload_json::Comment);
			};
		};
		struct reply_data_json
		{
			reply_payload_json Reply;
			struct glaze
			{
				static constexpr auto value = glz::object("addPullRequestReviewThreadReply", &reply_data_json::Reply);
			};
		};
		struct reply_response_json
		{
			reply_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &reply_response_json::Data);
			};
		};
		struct resolution_thread_json
		{
			std::string Id;
			bool        IsResolved = false;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &resolution_thread_json::Id,
					"isResolved", &resolution_thread_json::IsResolved);
			};
		};
		struct resolution_payload_json
		{
			resolution_thread_json Thread;
			struct glaze
			{
				static constexpr auto value = glz::object("thread", &resolution_payload_json::Thread);
			};
		};
		struct resolve_data_json
		{
			resolution_payload_json Resolve;
			struct glaze
			{
				static constexpr auto value = glz::object("resolveReviewThread", &resolve_data_json::Resolve);
			};
		};
		struct unresolve_data_json
		{
			resolution_payload_json Unresolve;
			struct glaze
			{
				static constexpr auto value = glz::object("unresolveReviewThread", &unresolve_data_json::Unresolve);
			};
		};
		struct resolve_response_json
		{
			resolve_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &resolve_response_json::Data);
			};
		};
		struct unresolve_response_json
		{
			unresolve_data_json Data;
			struct glaze
			{
				static constexpr auto value = glz::object("data", &unresolve_response_json::Data);
			};
		};
		struct target_head_json
		{
			std::string Sha;
			std::string Ref;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"sha", &target_head_json::Sha,
					"ref", &target_head_json::Ref);
			};
		};

		struct target_base_json
		{
			std::string              Sha;
			std::string              Ref;
			recovery_repository_json Repository;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"sha", &target_base_json::Sha,
					"ref", &target_base_json::Ref,
					"repo", &target_base_json::Repository);
			};
		};

		struct target_response_json
		{
			std::uint64_t    Id = 0U;
			std::string      NodeId;
			std::uint64_t    Number = 0U;
			std::string      State;
			target_head_json Head;
			target_base_json Base;
			struct glaze
			{
				static constexpr auto value = glz::object(
					"id", &target_response_json::Id,
					"node_id", &target_response_json::NodeId,
					"number", &target_response_json::Number,
					"state", &target_response_json::State,
					"head", &target_response_json::Head,
					"base", &target_response_json::Base);
			};
		};

		struct review_json_options : glz::opts
		{
			bool escape_control_characters    = true;
			bool validate_skipped             = true;
			bool validate_trailing_whitespace = true;

			constexpr review_json_options()
			{
				error_on_unknown_keys = false;
				error_on_missing_keys = true;
				partial_read          = false;
			}
		};

		[[nodiscard]] auto mutation_body(const application::review_mutation_request& aRequest) -> std::string
		{
			const auto& binding = *aRequest.LiveBinding;
			const auto  commit  = core::canonical_hex(binding.Anchor.Commit);
			const auto  object  = comment_submission_json{
				.Body     = aRequest.Proposal.Body,
				.CommitId = commit.view(),
				.Line     = static_cast<std::uint64_t>(binding.Anchor.Semantic.Line) + 1U,
				.Path     = binding.Anchor.Path,
				.Side     = binding.Anchor.Semantic.Side == core::diff_side::left ? "LEFT" : "RIGHT",
			};
			auto bytes = glz::write<review_json_options{ }>(object);
			if (!bytes) {
				return { };
			}
			// Preserve Qt compact JSON spelling in durable line-comment fingerprints.
			// Skip escaped backslashes so literal user text such as \u000B stays literal.
			for (std::size_t index = 0U; index + 1U < bytes->size(); ++index) {
				if ((*bytes)[index] != '\\') {
					continue;
				}
				++index;
				if ((*bytes)[index] == 'u' && index + 4U < bytes->size()) {
					for (auto digit = 0U; digit < 4U; ++digit) {
						auto& character = (*bytes)[++index];
						if (character >= 'A' && character <= 'F') {
							character = static_cast<char>(character + ('a' - 'A'));
						}
					}
				}
			}
			return std::move(*bytes);
		}

		// [[BPR-0657]]. The reply's GraphQL envelope: the reserved document
		// and the two variables, written by the same JSON rules as the REST
		// body so the durable fingerprint is byte-stable.
		[[nodiscard]] auto reply_body(const application::review_mutation_request& aRequest) -> std::string
		{
			const auto object = reply_query_json{
				.Query     = reply_document,
				.Variables = {
					.Thread = *aRequest.Proposal.ReplyToThread,
					.Body   = aRequest.Proposal.Body,
				},
			};
			auto bytes = glz::write<review_json_options{ }>(object);
			return bytes ? std::move(*bytes) : std::string{ };
		}

		[[nodiscard]] auto resolution_body(const application::review_thread_resolution_request& aRequest) -> std::string
		{
			const auto object = resolution_query_json{
				.Query     = aRequest.Resolve ? resolve_document : unresolve_document,
				.Variables = { .Thread = aRequest.Thread },
			};
			auto bytes = glz::write<review_json_options{ }>(object);
			return bytes ? std::move(*bytes) : std::string{ };
		}

		[[nodiscard]] auto submission_names(const core::review_disposition aDisposition)
			-> std::optional<review_submission_names>
		{
			switch (aDisposition) {
			case core::review_disposition::comment:
				return review_submission_names{ "COMMENT", "COMMENTED" };
			case core::review_disposition::approve:
				return review_submission_names{ "APPROVE", "APPROVED" };
			case core::review_disposition::request_changes:
				return review_submission_names{ "REQUEST_CHANGES", "CHANGES_REQUESTED" };
			}
			return std::nullopt;
		}

		[[nodiscard]] auto valid_submission(const review_submission_request& aRequest)
			-> bool
		{
			if (!application::valid(aRequest.Target)
				|| !application::valid(aRequest.ActingIdentity)
				|| !submission_names(aRequest.Disposition).has_value()
				|| aRequest.Body.size() > maximum_review_mutation_body_bytes) {
				return false;
			}
			const auto texts = std::array<std::string_view, 7>{
				aRequest.Body, aRequest.Target.RepositoryNodeId,
				aRequest.Target.PullRequestNodeId, aRequest.Target.BaseRef,
				aRequest.Target.HeadRef, aRequest.ActingIdentity.NodeId,
				aRequest.ActingIdentity.Login
			};
			return std::ranges::all_of(texts, utf8_round_trip)
				&& (aRequest.Disposition == core::review_disposition::approve
					|| !QString::fromUtf8(aRequest.Body.data(), static_cast<qsizetype>(aRequest.Body.size())).trimmed().isEmpty());
		}

		[[nodiscard]] auto submitted_timestamp(const std::string_view aText) -> bool
		{
			if (aText.size() != 20U) {
				return false;
			}
			const auto value     = QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size()));
			const auto timestamp = QDateTime::fromString(value, Qt::ISODate);
			return timestamp.isValid() && timestamp.toString(Qt::ISODate) == value
				&& aText.back() == 'Z';
		}

		[[nodiscard]] auto review_observation(review_response_json aResponse, const review_submission_request& aExpected) -> review_mutation_parse_result<review_submission_observation>
		{
			const auto commit = core::parse_git_commit_oid(aResponse.CommitId);
			if (aResponse.Id == 0U || aResponse.User.Id == 0U
				|| !bounded_text(aResponse.NodeId, 256U) || !commit.has_value()
				|| !submitted_timestamp(aResponse.SubmittedAt)) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			const auto& target      = aExpected.Target;
			const auto  authority   = target.ServerContract == application::github_server_contract::github_com_2022_11_28
				? std::string{ "api.github.com" }
				: std::string{ target.Hostname.value() };
			const auto  expectedUrl = "https://" + authority + pull_request_path(target, target.ServerContract);
			if (aResponse.PullRequestUrl != expectedUrl || *commit != target.Semantic.ExpectedHead
				|| aResponse.Body != aExpected.Body || aResponse.State != submission_names(aExpected.Disposition)->State
				|| aResponse.User.Id != aExpected.ActingIdentity.DatabaseId
				|| aResponse.User.NodeId != aExpected.ActingIdentity.NodeId
				|| aResponse.User.Login != aExpected.ActingIdentity.Login) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			return review_submission_observation{
				.ReviewDatabaseId = aResponse.Id,
				.ReviewNodeId     = std::move(aResponse.NodeId),
				.Target           = target,
				.Author           = aExpected.ActingIdentity,
				.Disposition      = aExpected.Disposition,
				.Body             = std::move(aResponse.Body),
				.SubmittedAt      = std::move(aResponse.SubmittedAt),
			};
		}

		void append_fingerprint_integer(std::string& aMaterial, const std::uint64_t aValue)
		{
			for (auto shift = 0U; shift < 64U; shift += 8U) {
				aMaterial.push_back(static_cast<char>((aValue >> shift) & 0xFFU));
			}
		}

		void append_fingerprint_text(
			std::string& aMaterial, const std::string_view aValue)
		{
			append_fingerprint_integer(aMaterial, aValue.size());
			aMaterial.append(aValue);
		}

		[[nodiscard]] auto canonical_write_fingerprint(
			const review_mutation_wire_request& aRequest)
			-> std::array<std::uint8_t, 32>
		{
			auto material = std::string{ };
			append_fingerprint_integer(
				material, static_cast<std::uint64_t>(aRequest.Server));
			append_fingerprint_integer(
				material, static_cast<std::uint64_t>(aRequest.Operation));
			append_fingerprint_integer(
				material, static_cast<std::uint64_t>(aRequest.Method));
			append_fingerprint_text(material, aRequest.Path);
			append_fingerprint_integer(material, aRequest.Headers.size());
			for (const auto& header : aRequest.Headers) {
				append_fingerprint_text(material, header.Name);
				append_fingerprint_text(material, header.Value);
			}
			append_fingerprint_text(material, aRequest.Body);
			return core::sha256(std::span{
				reinterpret_cast<const std::uint8_t*>(material.data()),
				material.size() });
		}

		void append_fingerprint_oid(
			std::string& aMaterial, const core::git_commit_oid& aOid)
		{
			append_fingerprint_integer(
				aMaterial, static_cast<std::uint64_t>(aOid.algorithm()));
			aMaterial.append(
				reinterpret_cast<const char*>(aOid.bytes().data()),
				aOid.bytes().size());
		}

		[[nodiscard]] auto canonical_authority_fingerprint(
			const application::review_mutation_live_binding& aBinding)
			-> std::array<std::uint8_t, 32>
		{
			const auto& target   = aBinding.Target;
			const auto& actor    = aBinding.ActingIdentity;
			auto        material = std::string{ };
			append_fingerprint_integer(material, target.Semantic.Host.value());
			append_fingerprint_integer(material, target.Semantic.Repository.value());
			append_fingerprint_integer(material, target.Semantic.PullRequest.value());
			append_fingerprint_oid(material, target.Semantic.ExpectedHead);
			append_fingerprint_text(material, target.Hostname.value());
			append_fingerprint_integer(
				material, static_cast<std::uint64_t>(target.ServerContract));
			append_fingerprint_text(material, target.Repository.Owner);
			append_fingerprint_text(material, target.Repository.Name);
			append_fingerprint_integer(material, target.Number.value());
			append_fingerprint_integer(material, target.RepositoryDatabaseId);
			append_fingerprint_text(material, target.RepositoryNodeId);
			append_fingerprint_integer(material, target.PullRequestDatabaseId);
			append_fingerprint_text(material, target.PullRequestNodeId);
			append_fingerprint_integer(material, target.BaseRepositoryDatabaseId);
			append_fingerprint_oid(material, target.BaseOid);
			append_fingerprint_text(material, target.BaseRef);
			append_fingerprint_text(material, target.HeadRef);
			append_fingerprint_integer(material, target.Open ? 1U : 0U);
			append_fingerprint_integer(material, actor.Account.value());
			append_fingerprint_integer(material, actor.DatabaseId);
			append_fingerprint_text(material, actor.NodeId);
			append_fingerprint_text(material, actor.Login);
			return core::sha256(std::span{
				reinterpret_cast<const std::uint8_t*>(material.data()),
				material.size() });
		}

		class json_preflight final
		{
		public:
			explicit json_preflight(const std::string_view aInput) noexcept
				: Input{ aInput }
			{
			}

			[[nodiscard]] auto run() noexcept -> bool
			{
				whitespace();
				const auto accepted = value(0U);
				whitespace();
				return accepted && Position == Input.size();
			}

		private:
			static constexpr std::size_t maximum_depth          = 24U;
			static constexpr std::size_t maximum_object_members = 128U;
			static constexpr std::size_t maximum_array_elements = 128U;
			static constexpr std::size_t maximum_total_values   = 4'096U;

			[[nodiscard]] auto value(const std::size_t aDepth) noexcept -> bool
			{
				if (Values == maximum_total_values) {
					return false;
				}
				++Values;
				whitespace();
				if (Position >= Input.size()) {
					return false;
				}
				switch (Input[Position]) {
				case '{':
					return object(aDepth);
				case '[':
					return array(aDepth);
				case '"':
					return string(nullptr, false);
				case 't':
					return literal("true");
				case 'f':
					return literal("false");
				case 'n':
					return literal("null");
				default:
					return number();
				}
			}

			[[nodiscard]] auto object(const std::size_t aDepth) noexcept -> bool
			{
				if (aDepth >= maximum_depth || !consume('{')) {
					return false;
				}
				whitespace();
				if (consume('}')) {
					return true;
				}
				std::array<std::string_view, maximum_object_members> keys;
				std::size_t                                          count = 0U;
				while (count < keys.size()) {
					std::string_view key;
					if (!string(&key, true)
						|| std::ranges::find(keys.begin(), keys.begin() + static_cast<std::ptrdiff_t>(count), key)
							!= keys.begin() + static_cast<std::ptrdiff_t>(count)) {
						return false;
					}
					keys[count++] = key;
					whitespace();
					if (!consume(':') || !value(aDepth + 1U)) {
						return false;
					}
					whitespace();
					if (consume('}')) {
						return true;
					}
					if (!consume(',')) {
						return false;
					}
					whitespace();
				}
				return false;
			}

			[[nodiscard]] auto array(const std::size_t aDepth) noexcept -> bool
			{
				if (aDepth >= maximum_depth || !consume('[')) {
					return false;
				}
				whitespace();
				if (consume(']')) {
					return true;
				}
				for (std::size_t count = 0U; count < maximum_array_elements; ++count) {
					if (!value(aDepth + 1U)) {
						return false;
					}
					whitespace();
					if (consume(']')) {
						return true;
					}
					if (!consume(',')) {
						return false;
					}
					whitespace();
				}
				return false;
			}

			[[nodiscard]] auto string(
				std::string_view* aOutput,
				const bool        aKey) noexcept -> bool
			{
				if (!consume('"')) {
					return false;
				}
				const auto start = Position;
				while (Position < Input.size()) {
					const auto character = static_cast<unsigned char>(Input[Position++]);
					if (character == '"') {
						if (aOutput != nullptr) {
							*aOutput = Input.substr(start, Position - start - 1U);
						}
						return true;
					}
					if (character < 0x20U || (aKey && character == '\\')) {
						return false;
					}
					if (character != '\\') {
						continue;
					}
					if (Position >= Input.size()) {
						return false;
					}
					const auto escaped = Input[Position++];
					if (std::string_view{ "\"\\/bfnrt" }.find(escaped) != std::string_view::npos) {
						continue;
					}
					if (escaped != 'u' || Input.size() - Position < 4U) {
						return false;
					}
					for (std::size_t digit = 0U; digit < 4U; ++digit) {
						if (std::isxdigit(static_cast<unsigned char>(Input[Position++])) == 0) {
							return false;
						}
					}
				}
				return false;
			}

			[[nodiscard]] auto number() noexcept -> bool
			{
				const auto start = Position;
				static_cast<void>(consume('-'));
				if (consume('0')) {
					if (Position < Input.size() && std::isdigit(static_cast<unsigned char>(Input[Position])) != 0) {
						return false;
					}
				} else {
					const auto digits = Position;
					while (Position < Input.size() && std::isdigit(static_cast<unsigned char>(Input[Position])) != 0) {
						++Position;
					}
					if (Position == digits) {
						return false;
					}
				}
				if (consume('.')) {
					const auto digits = Position;
					while (Position < Input.size() && std::isdigit(static_cast<unsigned char>(Input[Position])) != 0) {
						++Position;
					}
					if (Position == digits) {
						return false;
					}
				}
				if (Position < Input.size() && (Input[Position] == 'e' || Input[Position] == 'E')) {
					++Position;
					static_cast<void>(consume('+') || consume('-'));
					const auto digits = Position;
					while (Position < Input.size() && std::isdigit(static_cast<unsigned char>(Input[Position])) != 0) {
						++Position;
					}
					if (Position == digits) {
						return false;
					}
				}
				return Position != start;
			}

			[[nodiscard]] auto literal(const std::string_view aExpected) noexcept -> bool
			{
				if (Input.substr(Position, aExpected.size()) != aExpected) {
					return false;
				}
				Position += aExpected.size();
				return true;
			}

			void whitespace() noexcept
			{
				while (Position < Input.size()
					   && (Input[Position] == ' ' || Input[Position] == '\t'
						   || Input[Position] == '\r' || Input[Position] == '\n')) {
					++Position;
				}
			}

			[[nodiscard]] auto consume(const char aExpected) noexcept -> bool
			{
				if (Position >= Input.size() || Input[Position] != aExpected) {
					return false;
				}
				++Position;
				return true;
			}

			std::string_view Input;
			std::size_t      Position = 0U;
			std::size_t      Values   = 0U;
		};

		template <typename T>
		[[nodiscard]] auto document(const review_mutation_wire_response& aResponse) -> std::optional<T>
		{
			if (aResponse.Body.empty() || aResponse.Body.size() > maximum_review_mutation_response_bytes
				|| !json_preflight{ aResponse.Body }.run()) {
				return std::nullopt;
			}
			auto parsed = T{ };
			if (glz::read<review_json_options{ }>(parsed, aResponse.Body)) {
				return std::nullopt;
			}
			return parsed;
		}

		[[nodiscard]] auto comment_observation(const comment_response_json&                aResponse,
											   const application::review_mutation_request& aExpected)
			-> review_mutation_parse_result<review_mutation_comment_observation>
		{
			const auto     commit            = core::parse_git_commit_oid(aResponse.CommitId);
			constexpr auto maximumGithubLine = static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1U;
			if (aResponse.Id == 0U || aResponse.User.Id == 0U || !commit
				|| aResponse.Line == 0U || aResponse.Line > maximumGithubLine) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			const auto& binding      = *aExpected.LiveBinding;
			const auto  expectedSide = binding.Anchor.Semantic.Side == core::diff_side::left ? "LEFT" : "RIGHT";
			const auto  semanticLine = static_cast<std::uint32_t>(aResponse.Line - 1U);
			if (aResponse.Path != binding.Anchor.Path || *commit != binding.Anchor.Commit
				|| aResponse.Side != expectedSide || semanticLine != binding.Anchor.Semantic.Line
				|| aResponse.Body != aExpected.Proposal.Body
				|| aResponse.User.Id != binding.ActingIdentity.DatabaseId
				|| aResponse.User.NodeId != binding.ActingIdentity.NodeId
				|| aResponse.User.Login != binding.ActingIdentity.Login) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			return review_mutation_comment_observation{
				.Receipt    = { .ServerCommentId = aResponse.NodeId, .Duplicate = false },
				.DatabaseId = aResponse.Id,
				.NodeId     = aResponse.NodeId,
				.Anchor     = binding.Anchor,
				.Body       = aResponse.Body,
				.Author     = binding.ActingIdentity,
			};
		}

		// [[BPR-0657]]. A reply answered by GraphQL: the same identity checks a
		// REST comment gets, over the fields the mutation returns.
		[[nodiscard]] auto reply_observation(const reply_comment_json&                   aComment,
											 const application::review_mutation_request& aExpected)
			-> review_mutation_parse_result<review_mutation_comment_observation>
		{
			const auto     commit            = core::parse_git_commit_oid(aComment.Commit.Oid);
			constexpr auto maximumGithubLine = static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1U;
			if (aComment.Id.empty() || aComment.DatabaseId == 0U || !commit
				|| aComment.Line == 0U || aComment.Line > maximumGithubLine) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			const auto& binding      = *aExpected.LiveBinding;
			const auto  semanticLine = static_cast<std::uint32_t>(aComment.Line - 1U);
			if (aComment.Path != binding.Anchor.Path || *commit != binding.Anchor.Commit
				|| semanticLine != binding.Anchor.Semantic.Line
				|| aComment.Body != aExpected.Proposal.Body || !aComment.ViewerDidAuthor
				|| aComment.Author.Login != binding.ActingIdentity.Login) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			return review_mutation_comment_observation{
				.Receipt    = { .ServerCommentId = aComment.Id, .Duplicate = false },
				.DatabaseId = aComment.DatabaseId,
				.NodeId     = aComment.Id,
				.Anchor     = binding.Anchor,
				.Body       = aComment.Body,
				.Author     = binding.ActingIdentity,
			};
		}

		[[nodiscard]] auto response_headers_valid(
			const review_mutation_wire_response& aResponse) noexcept -> bool
		{
			return aResponse.Headers.size() <= 64U
				&& std::ranges::all_of(aResponse.Headers, [](const http_header& aHeader) {
					   return bounded_text(aHeader.Name, 256U)
						   && bounded_text(aHeader.Value, 8U * 1024U)
						   && aHeader.Name != "Location" && aHeader.Name != "location";
				   });
		}

		[[nodiscard]] auto has_header(
			const review_mutation_wire_response& aResponse,
			const std::string_view               aName) noexcept -> bool
		{
			return std::ranges::any_of(aResponse.Headers, [aName](const http_header& aHeader) {
				return aHeader.Name.size() == aName.size()
					&& std::ranges::equal(
						   aHeader.Name,
						   aName,
						   [](const unsigned char aLeft, const unsigned char aRight) {
							   return std::tolower(aLeft) == std::tolower(aRight);
						   });
			});
		}

		[[nodiscard]] auto same_attempt(
			const application::review_mutation_recovery_record& aLeft,
			const application::review_mutation_recovery_record& aRight) noexcept
			-> bool
		{
			return aLeft.IdempotencyKey == aRight.IdempotencyKey
				&& aLeft.RequestFingerprint == aRight.RequestFingerprint
				&& aLeft.AuthorityFingerprint == aRight.AuthorityFingerprint
				&& aLeft.Body == aRight.Body && aLeft.Anchor == aRight.Anchor;
		}

		[[nodiscard]] auto exact_comment_ids(
			const review_mutation_server_reconciliation& aObservation)
			-> std::vector<std::string>
		{
			auto result = std::vector<std::string>{ };
			result.reserve(aObservation.ExactReceipts.size());
			std::ranges::transform(
				aObservation.ExactReceipts,
				std::back_inserter(result),
				&application::review_mutation_receipt::ServerCommentId);
			return result;
		}

		[[nodiscard]] auto new_exact_receipt(
			const review_mutation_server_reconciliation& aObservation,
			const std::vector<std::string>&              aBaseline)
			-> std::optional<application::review_mutation_receipt>
		{
			auto result = std::optional<application::review_mutation_receipt>{ };
			for (const auto& receipt : aObservation.ExactReceipts) {
				if (std::ranges::binary_search(aBaseline, receipt.ServerCommentId)) {
					continue;
				}
				if (result.has_value()) {
					return std::nullopt;
				}
				result = receipt;
			}
			return result;
		}

		[[nodiscard]] auto map_contract_failure(
			const review_mutation_contract_failure aFailure) noexcept
			-> application::review_mutation_failure
		{
			return aFailure == review_mutation_contract_failure::identity_mismatch
					|| aFailure == review_mutation_contract_failure::invalid_request
				? application::review_mutation_failure::undesignated_target
				: application::review_mutation_failure::unavailable;
		}
	} // namespace

	auto compile_target_observation_request(
		const application::review_mutation_target_binding& aTarget,
		const application::github_server_contract          aServer)
		-> review_mutation_request_result
	{
		if (!application::valid(aTarget) || !valid_server(aServer)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		auto request = review_mutation_wire_request{
			.Semantic  = aTarget,
			.Server    = aServer,
			.Operation = review_mutation_operation::observe_pull_request,
			.Method    = http_method::get,
			.Path      = pull_request_path(aTarget, aServer),
			.Headers   = common_headers(),
			.Body      = { },
		};
		return request;
	}

	auto compile_viewer_observation_request(
		const application::review_mutation_acting_identity& aIdentity,
		const application::github_server_contract           aServer)
		-> review_mutation_request_result
	{
		if (!application::valid(aIdentity) || !valid_server(aServer)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		return review_mutation_wire_request{
			.Semantic  = aIdentity,
			.Server    = aServer,
			.Operation = review_mutation_operation::observe_viewer,
			.Method    = http_method::get,
			.Path      = std::string{ rest_prefix(aServer) } + "/user",
			.Headers   = common_headers(),
			.Body      = { },
		};
	}

	auto compile_review_comment_request(
		const application::review_mutation_request& aRequest,
		const application::github_server_contract   aServer)
		-> review_mutation_request_result
	{
		if (!valid_server(aServer) || !aRequest.LiveBinding.has_value()
			|| !application::valid(*aRequest.LiveBinding)
			|| aRequest.Proposal.Anchor != aRequest.LiveBinding->Anchor.Semantic
			|| aRequest.Target != aRequest.LiveBinding->Target.Semantic
			|| aRequest.ActingIdentity != aRequest.LiveBinding->ActingIdentity.Account
			|| aRequest.Proposal.Disposition != core::review_disposition::comment
			|| aRequest.Proposal.Body.empty()
			|| aRequest.Proposal.Body.size() > maximum_review_mutation_body_bytes
			|| !utf8_round_trip(aRequest.Proposal.Body)
			|| !utf8_round_trip(aRequest.LiveBinding->Anchor.Path)
			|| aRequest.IdempotencyKey
				!= application::idempotency_key_for(aRequest.Proposal, aRequest.Target)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		auto headers = common_headers();
		headers.push_back({ .Name = "Content-Type", .Value = "application/json" });
		// [[BPR-0657]]. A reply names its thread and goes by GraphQL, the only
		// GitHub API that addresses a thread by the identity the read side
		// reported; a new root comment keeps the REST form.
		if (aRequest.Proposal.ReplyToThread.has_value()) {
			if (!graphql_token(*aRequest.Proposal.ReplyToThread)) {
				return failed(review_mutation_contract_failure::invalid_request);
			}
			auto body = reply_body(aRequest);
			if (body.empty() || body.size() > maximum_review_mutation_body_bytes) {
				return failed(review_mutation_contract_failure::invalid_request);
			}
			return review_mutation_wire_request{
				.Semantic  = aRequest,
				.Server    = aServer,
				.Operation = review_mutation_operation::reply_review_thread,
				.Method    = http_method::post,
				.Path      = std::string{ graphql_path(aServer) },
				.Headers   = std::move(headers),
				.Body      = std::move(body),
			};
		}
		auto body = mutation_body(aRequest);
		if (body.empty() || body.size() > maximum_review_mutation_body_bytes) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		return review_mutation_wire_request{
			.Semantic  = aRequest,
			.Server    = aServer,
			.Operation = review_mutation_operation::create_review_comment,
			.Method    = http_method::post,
			.Path      = pull_request_path(aRequest.LiveBinding->Target, aServer) + "/comments",
			.Headers   = std::move(headers),
			.Body      = std::move(body),
		};
	}

	auto compile_thread_resolution_request(
		const application::review_thread_resolution_request& aRequest,
		const application::github_server_contract            aServer)
		-> review_mutation_request_result
	{
		// A resolution binds the target and the actor; it has no source line.
		if (!valid_server(aServer) || !aRequest.LiveBinding.has_value()
			|| !application::valid(aRequest.LiveBinding->Target)
			|| !application::valid(aRequest.LiveBinding->ActingIdentity)
			|| aRequest.Target != aRequest.LiveBinding->Target.Semantic
			|| aRequest.ActingIdentity != aRequest.LiveBinding->ActingIdentity.Account
			|| !graphql_token(aRequest.Thread)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		auto headers = common_headers();
		headers.push_back({ .Name = "Content-Type", .Value = "application/json" });
		auto body = resolution_body(aRequest);
		if (body.empty() || body.size() > maximum_review_mutation_body_bytes) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		return review_mutation_wire_request{
			.Semantic  = aRequest,
			.Server    = aServer,
			.Operation = aRequest.Resolve ? review_mutation_operation::resolve_review_thread
										  : review_mutation_operation::unresolve_review_thread,
			.Method    = http_method::post,
			.Path      = std::string{ graphql_path(aServer) },
			.Headers   = std::move(headers),
			.Body      = std::move(body),
		};
	}

	auto compile_review_submission_request(
		const review_submission_request&          aRequest,
		const application::github_server_contract aServer)
		-> review_mutation_request_result
	{
		if (!valid_server(aServer) || aServer != aRequest.Target.ServerContract
			|| !valid_submission(aRequest)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		const auto commit = core::canonical_hex(aRequest.Target.Semantic.ExpectedHead);
		const auto object = review_submission_json{
			.Body     = aRequest.Body,
			.CommitId = commit.view(),
			.Event    = submission_names(aRequest.Disposition)->Event,
		};
		auto bytes = glz::write<review_json_options{ }>(object);
		if (!bytes.has_value() || bytes->size() > maximum_review_mutation_body_bytes) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		auto headers = common_headers();
		headers.push_back({ .Name = "Content-Type", .Value = "application/json" });
		return review_mutation_wire_request{
			.Semantic  = aRequest,
			.Server    = aServer,
			.Operation = review_mutation_operation::create_review,
			.Method    = http_method::post,
			.Path      = pull_request_path(aRequest.Target, aServer) + "/reviews",
			.Headers   = std::move(headers),
			.Body      = std::move(*bytes),
		};
	}

	auto compile_review_submission_observation_request(const review_submission_request& aRequest, const application::github_server_contract aServer) -> review_mutation_request_result
	{
		auto result = compile_review_submission_request(aRequest, aServer);
		if (!result) {
			return result;
		}
		result->Operation = review_mutation_operation::observe_reviews;
		result->Method    = http_method::get;
		result->Path += "?per_page=100";
		result->Headers = common_headers();
		result->Body.clear();
		return result;
	}

	auto compile_review_comment_observation_request(
		const application::review_mutation_request& aRequest,
		const application::github_server_contract   aServer)
		-> review_mutation_request_result
	{
		const auto mutation = compile_review_comment_request(aRequest, aServer);
		if (!mutation.has_value()) {
			return std::unexpected(mutation.error());
		}
		return review_mutation_wire_request{
			.Semantic  = aRequest,
			.Server    = aServer,
			.Operation = review_mutation_operation::observe_review_comments,
			.Method    = http_method::get,
			.Path      = mutation->Path + "?per_page=100",
			.Headers   = common_headers(),
			.Body      = { },
		};
	}

	auto validate_outbound_review_mutation_request(
		const review_mutation_wire_request& aRequest)
		-> std::expected<void, review_mutation_contract_failure>
	{
		try {
			auto canonical = review_mutation_request_result{ failed(
				review_mutation_contract_failure::invalid_request) };
			std::visit(
				[&canonical, &aRequest](const auto& aSemantic) {
					using semantic_type = std::remove_cvref_t<decltype(aSemantic)>;
					if constexpr (std::same_as<semantic_type, application::review_mutation_target_binding>) {
						canonical = compile_target_observation_request(aSemantic, aRequest.Server);
					} else if constexpr (std::same_as<semantic_type, application::review_mutation_acting_identity>) {
						canonical = compile_viewer_observation_request(aSemantic, aRequest.Server);
					} else if constexpr (std::same_as<semantic_type, review_submission_request>) {
						canonical = aRequest.Operation == review_mutation_operation::observe_reviews
							? compile_review_submission_observation_request(aSemantic, aRequest.Server)
							: compile_review_submission_request(aSemantic, aRequest.Server);
					} else if constexpr (std::same_as<semantic_type, application::review_thread_resolution_request>) {
						canonical = compile_thread_resolution_request(aSemantic, aRequest.Server);
					} else if (aRequest.Operation
								   == review_mutation_operation::create_review_comment
							   || aRequest.Operation
								   == review_mutation_operation::reply_review_thread) {
						canonical = compile_review_comment_request(aSemantic, aRequest.Server);
					} else if (aRequest.Operation
							   == review_mutation_operation::observe_review_comments) {
						canonical = compile_review_comment_observation_request(
							aSemantic, aRequest.Server);
					}
				},
				aRequest.Semantic);
			if (!canonical.has_value() || *canonical != aRequest) {
				return failed(review_mutation_contract_failure::invalid_request);
			}
			return { };
		} catch (...) {
			return failed(review_mutation_contract_failure::response_limit);
		}
	}

	auto parse_review_submission_recovery_target(const review_mutation_wire_response& aResponse, const application::review_mutation_target_binding& aExpected) -> review_mutation_parse_result<void>
	{
		if (!application::valid(aExpected)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse) || has_header(aResponse, "location")) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		if (aResponse.Body.size() > maximum_review_mutation_response_bytes) {
			return failed(review_mutation_contract_failure::response_limit);
		}
		if (!json_preflight{ aResponse.Body }.run()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		recovery_target_json target;
		if (glz::read<review_json_options{ }>(target, aResponse.Body)) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		const auto& repository = target.Base.Repository;
		if (target.Id != aExpected.PullRequestDatabaseId || target.NodeId != aExpected.PullRequestNodeId || target.Number != aExpected.Number.value() || repository.Id != aExpected.RepositoryDatabaseId || repository.Id != aExpected.BaseRepositoryDatabaseId || repository.NodeId != aExpected.RepositoryNodeId || repository.FullName != aExpected.Repository.Owner + "/" + aExpected.Repository.Name) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		return { };
	}

	auto parse_target_observation(
		const review_mutation_wire_response&               aResponse,
		const application::review_mutation_target_binding& aExpected)
		-> review_mutation_parse_result<review_mutation_target_observation>
	{
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto response = document<target_response_json>(aResponse);
		if (!response || response->Id == 0U || response->Number == 0U || response->Base.Repository.Id == 0U) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		const auto head = core::parse_git_commit_oid(response->Head.Sha);
		const auto base = core::parse_git_commit_oid(response->Base.Sha);
		if (!head || !base) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		const auto& repository = response->Base.Repository;
		if (response->Id != aExpected.PullRequestDatabaseId || response->NodeId != aExpected.PullRequestNodeId
			|| response->Number != aExpected.Number.value() || response->State != "open"
			|| repository.Id != aExpected.RepositoryDatabaseId || repository.Id != aExpected.BaseRepositoryDatabaseId
			|| repository.NodeId != aExpected.RepositoryNodeId
			|| repository.FullName != aExpected.Repository.Owner + "/" + aExpected.Repository.Name
			|| *base != aExpected.BaseOid || response->Base.Ref != aExpected.BaseRef || response->Head.Ref != aExpected.HeadRef) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		auto observed                  = aExpected;
		observed.Semantic.ExpectedHead = *head;
		return review_mutation_target_observation{ .Target = std::move(observed) };
	}

	auto parse_viewer_observation(
		const review_mutation_wire_response&                aResponse,
		const application::review_mutation_acting_identity& aExpected)
		-> review_mutation_parse_result<review_mutation_viewer_observation>
	{
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto response = document<review_author_json>(aResponse);
		if (!response || response->Id == 0U) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		if (response->Id != aExpected.DatabaseId || response->NodeId != aExpected.NodeId || response->Login != aExpected.Login) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		return review_mutation_viewer_observation{ .Identity = aExpected };
	}

	auto parse_review_comment_response(
		const review_mutation_wire_response&        aResponse,
		const application::review_mutation_request& aExpected)
		-> review_mutation_parse_result<review_mutation_comment_observation>
	{
		// [[BPR-0657]]. A reply comes back from GraphQL as 200 with an
		// envelope; a new root comment from REST as 201 with the comment.
		if (aExpected.Proposal.ReplyToThread.has_value()) {
			if (aResponse.Status != 200 || !response_headers_valid(aResponse) || !aExpected.LiveBinding) {
				return failed(review_mutation_contract_failure::invalid_status);
			}
			const auto response = document<reply_response_json>(aResponse);
			if (!response) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			return reply_observation(response->Data.Reply.Comment, aExpected);
		}
		if (aResponse.Status != 201 || !response_headers_valid(aResponse) || !aExpected.LiveBinding) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto response = document<comment_response_json>(aResponse);
		if (!response) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		return comment_observation(*response, aExpected);
	}

	auto parse_thread_resolution_response(
		const review_mutation_wire_response&                 aResponse,
		const application::review_thread_resolution_request& aExpected)
		-> review_mutation_parse_result<review_thread_resolution_observation>
	{
		if (!compile_thread_resolution_request(aExpected, aExpected.LiveBinding ? aExpected.LiveBinding->Target.ServerContract : application::github_server_contract::github_com_2022_11_28).has_value()) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		auto thread = std::optional<resolution_thread_json>{ };
		if (aExpected.Resolve) {
			if (const auto response = document<resolve_response_json>(aResponse); response) {
				thread = response->Data.Resolve.Thread;
			}
		} else if (const auto response = document<unresolve_response_json>(aResponse); response) {
			thread = response->Data.Unresolve.Thread;
		}
		if (!thread.has_value()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		if (thread->Id != aExpected.Thread || thread->IsResolved != aExpected.Resolve) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		return review_thread_resolution_observation{
			.Receipt = { .Thread = thread->Id, .Resolved = thread->IsResolved },
		};
	}

	auto parse_review_submission_response(
		const review_mutation_wire_response& aResponse,
		const review_submission_request&     aExpected)
		-> review_mutation_parse_result<review_submission_observation>
	{
		if (!compile_review_submission_request(aExpected, aExpected.Target.ServerContract).has_value()) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (has_header(aResponse, "location") || (aResponse.Status >= 300 && aResponse.Status < 400)) {
			return failed(review_mutation_contract_failure::redirect);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		if (aResponse.Body.size() > maximum_review_mutation_response_bytes) {
			return failed(review_mutation_contract_failure::response_limit);
		}
		if (!json_preflight{ aResponse.Body }.run()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		auto response = review_response_json{ };
		if (glz::read<review_json_options{ }>(response, aResponse.Body)) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		return review_observation(std::move(response), aExpected);
	}

	auto parse_review_submission_observations(const review_mutation_wire_response& aResponse, const review_submission_request& aExpected) -> review_mutation_parse_result<review_submission_reconciliation>
	{
		if (!compile_review_submission_observation_request(aExpected, aExpected.Target.ServerContract)) {
			return failed(review_mutation_contract_failure::invalid_request);
		}
		if (aResponse.Status != 200 || !response_headers_valid(aResponse) || has_header(aResponse, "location") || has_header(aResponse, "link")) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		if (aResponse.Body.size() > maximum_review_mutation_response_bytes) {
			return failed(review_mutation_contract_failure::response_limit);
		}
		if (!json_preflight{ aResponse.Body }.run()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		std::vector<review_list_json> reviews;
		if (glz::read<review_json_options{ }>(reviews, aResponse.Body) || reviews.size() > 100U) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		review_submission_reconciliation result;
		const auto                       authority   = aExpected.Target.ServerContract == application::github_server_contract::github_com_2022_11_28 ? std::string{ "api.github.com" } : std::string{ aExpected.Target.Hostname.value() };
		const auto                       expectedUrl = "https://" + authority + pull_request_path(aExpected.Target, aExpected.Target.ServerContract);
		for (auto& review : reviews) {
			if (review.Id == 0U || !bounded_text(review.NodeId, 256U) || review.PullRequestUrl != expectedUrl) {
				return failed(review_mutation_contract_failure::identity_mismatch);
			}
			result.ReviewIds.emplace_back(review.Id);
			if (review.State == "PENDING") {
				if (review.SubmittedAt) {
					return failed(review_mutation_contract_failure::malformed_response);
				}
				continue;
			}
			if (review.State != "DISMISSED" && review.State != "COMMENTED" && review.State != "APPROVED" && review.State != "CHANGES_REQUESTED") {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			if (!review.SubmittedAt || !submitted_timestamp(*review.SubmittedAt)) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			if (review.State == "DISMISSED" || !review.User || !review.Body || !review.CommitId) {
				continue;
			}
			const auto exact = review_observation(review_response_json{
													  .Id = review.Id, .NodeId = std::move(review.NodeId), .CommitId = std::move(*review.CommitId), .Body = std::move(*review.Body), .State = std::move(review.State), .SubmittedAt = std::move(*review.SubmittedAt), .PullRequestUrl = std::move(review.PullRequestUrl), .User = std::move(*review.User) },
												  aExpected);

			if (exact) {
				result.ExactReviews.push_back(*exact);
			} else if (exact.error() != review_mutation_contract_failure::identity_mismatch) {
				return failed(exact.error());
			}
		}
		std::ranges::sort(result.ReviewIds);
		if (std::ranges::adjacent_find(result.ReviewIds) != result.ReviewIds.end()) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		return result;
	}

	auto parse_review_comment_observations(
		const review_mutation_wire_response&        aResponse,
		const application::review_mutation_request& aExpected)
		-> review_mutation_parse_result<review_mutation_server_reconciliation>
	{
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)
			|| has_header(aResponse, "Link") || !aExpected.LiveBinding.has_value()) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto comments = document<std::vector<comment_response_json>>(aResponse);
		if (!comments || comments->size() > 100U) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		auto result = review_mutation_server_reconciliation{ };
		result.Projection.reserve(comments->size());
		for (const auto& comment : *comments) {
			if (!bounded_text(comment.NodeId, 256U) || comment.Body.size() > maximum_review_mutation_response_bytes || !utf8_round_trip(comment.Body)) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			result.Projection.push_back(application::review_comment_projection{
				.ServerId = comment.NodeId,
				.Body     = comment.Body,
			});
			const auto exact = comment_observation(comment, aExpected);
			if (exact) {
				result.ExactReceipts.push_back(exact->Receipt);
			} else if (exact.error() != review_mutation_contract_failure::identity_mismatch) {
				return failed(exact.error());
			}
		}

		std::ranges::sort(result.Projection, { }, &application::review_comment_projection::ServerId);
		if (std::ranges::adjacent_find(
				result.Projection,
				{ },
				&application::review_comment_projection::ServerId)
			!= result.Projection.end()) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		std::ranges::sort(
			result.ExactReceipts,
			{ },
			&application::review_mutation_receipt::ServerCommentId);
		const auto duplicates = std::ranges::unique(
			result.ExactReceipts,
			{ },
			&application::review_mutation_receipt::ServerCommentId);
		result.ExactReceipts.erase(duplicates.begin(), duplicates.end());
		return result;
	}

	auto valid(const live_review_mutation_configuration& aConfiguration) noexcept -> bool
	{
		return application::valid(aConfiguration.CredentialContext)
			&& application::valid(aConfiguration.Target)
			&& application::valid(aConfiguration.ActingIdentity)
			&& aConfiguration.CredentialContext.Authority.Host
			== aConfiguration.Target.Semantic.Host
			&& aConfiguration.CredentialContext.Authority.Hostname
			== aConfiguration.Target.Hostname
			&& aConfiguration.CredentialContext.Authority.ServerContract
			== aConfiguration.Target.ServerContract
			&& aConfiguration.CredentialContext.Authority.Account
			== aConfiguration.ActingIdentity.Account;
	}

	review_mutation_session::review_mutation_session(
		github_mutation_wire_transport&        aTransport,
		live_review_mutation_configuration     aConfiguration,
		application::review_mutation_transport aLease,
		std::unique_ptr<application::review_mutation_recovery_port>
			aRecovery) noexcept
		: Transport{ &aTransport }
		, Configuration{ std::move(aConfiguration) }
		, Lease{ std::move(aLease) }
		, Recovery{ std::move(aRecovery) }
	{
	}

	auto review_mutation_session::perform(
		review_mutation_wire_request         aRequest,
		const std::stop_token                aStop,
		github_mutation_transmission_witness aWitness)
		-> std::expected<review_mutation_wire_response, application::review_mutation_failure>
	{
		if (aStop.stop_requested()) {
			return std::unexpected(application::review_mutation_failure::cancelled);
		}
		if (Transport == nullptr || Lease == nullptr || !Lease->active()
			|| !valid(Configuration)) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
		auto completion = Transport->perform_review_mutation(
			github_mutation_wire_request{
				.Context = Configuration.CredentialContext,
				.Wire    = std::move(aRequest),
			},
			aStop,
			std::move(aWitness));
		if (auto* success = std::get_if<github_mutation_wire_success>(&completion)) {
			return std::move(success->Response);
		}
		if (auto* error = std::get_if<github_mutation_wire_error>(&completion)) {
			return std::unexpected(error->Failure);
		}
		return std::unexpected(application::review_mutation_failure::cancelled);
	}

	auto review_mutation_session::observe_exact(const std::stop_token aStop)
		-> std::expected<void, application::review_mutation_failure>
	{
		const auto viewerRequest = compile_viewer_observation_request(
			Configuration.ActingIdentity,
			Configuration.CredentialContext.Authority.ServerContract);
		if (!viewerRequest.has_value()) {
			return std::unexpected(map_contract_failure(viewerRequest.error()));
		}
		auto viewerResponse = perform(*viewerRequest, aStop);
		if (!viewerResponse.has_value()) {
			return std::unexpected(viewerResponse.error());
		}
		const auto viewer = parse_viewer_observation(*viewerResponse, Configuration.ActingIdentity);
		if (!viewer.has_value()) {
			return std::unexpected(map_contract_failure(viewer.error()));
		}

		// Target/head is deliberately the last observation before POST. Viewer
		// identity is checked first so no other network operation widens the
		// target-revalidation window.
		const auto targetRequest = compile_target_observation_request(
			Configuration.Target,
			Configuration.CredentialContext.Authority.ServerContract);
		if (!targetRequest.has_value()) {
			return std::unexpected(map_contract_failure(targetRequest.error()));
		}
		auto targetResponse = perform(*targetRequest, aStop);
		if (!targetResponse.has_value()) {
			return std::unexpected(targetResponse.error());
		}
		const auto target = parse_target_observation(*targetResponse, Configuration.Target);
		if (!target.has_value()) {
			return std::unexpected(map_contract_failure(target.error()));
		}
		if (target->Target.Semantic.ExpectedHead
			!= Configuration.Target.Semantic.ExpectedHead) {
			return std::unexpected(application::review_mutation_failure::stale_head);
		}
		return { };
	}

	auto review_mutation_session::reconcile(
		const application::review_mutation_request& aRequest,
		const std::stop_token                       aStop)
		-> std::expected<
			review_mutation_server_reconciliation,
			application::review_mutation_failure>
	{
		const auto wire = compile_review_comment_observation_request(
			aRequest,
			Configuration.CredentialContext.Authority.ServerContract);
		if (!wire.has_value()) {
			return std::unexpected(map_contract_failure(wire.error()));
		}
		auto response = perform(*wire, aStop);
		if (!response.has_value()) {
			return std::unexpected(response.error());
		}
		auto parsed = parse_review_comment_observations(*response, aRequest);
		if (!parsed.has_value()) {
			return std::unexpected(map_contract_failure(parsed.error()));
		}
		return std::move(*parsed);
	}

	auto review_mutation_session::observe_target(const std::stop_token aStop)
		-> std::expected<core::review_mutation_target, application::review_mutation_failure>
	{
		try {
			std::scoped_lock lock{ Mutex };
			const auto       targetRequest = compile_target_observation_request(
				Configuration.Target,
				Configuration.CredentialContext.Authority.ServerContract);
			if (!targetRequest.has_value()) {
				return std::unexpected(map_contract_failure(targetRequest.error()));
			}
			auto targetResponse = perform(*targetRequest, aStop);
			if (!targetResponse.has_value()) {
				return std::unexpected(targetResponse.error());
			}
			const auto target = parse_target_observation(*targetResponse, Configuration.Target);
			if (!target.has_value()) {
				return std::unexpected(map_contract_failure(target.error()));
			}
			const auto viewerRequest = compile_viewer_observation_request(
				Configuration.ActingIdentity,
				Configuration.CredentialContext.Authority.ServerContract);
			if (!viewerRequest.has_value()) {
				return std::unexpected(map_contract_failure(viewerRequest.error()));
			}
			auto viewerResponse = perform(*viewerRequest, aStop);
			if (!viewerResponse.has_value()) {
				return std::unexpected(viewerResponse.error());
			}
			const auto viewer = parse_viewer_observation(*viewerResponse, Configuration.ActingIdentity);
			if (!viewer.has_value()) {
				return std::unexpected(map_contract_failure(viewer.error()));
			}
			return target->Target.Semantic;
		} catch (...) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
	}

	auto review_mutation_session::submit(
		const application::review_mutation_request& aRequest,
		const std::stop_token                       aStop)
		-> std::expected<application::review_mutation_receipt, application::review_mutation_failure>
	{
		return submit_and_reconcile(aRequest, aStop).Result;
	}

	auto review_mutation_session::resolve_thread(
		const application::review_thread_resolution_request& aRequest,
		const std::stop_token                                aStop)
		-> std::expected<application::review_thread_resolution_receipt, application::review_mutation_failure>
	{
		try {
			std::scoped_lock lock{ Mutex };
			if (aStop.stop_requested()) {
				return std::unexpected(application::review_mutation_failure::cancelled);
			}
			if (Lease == nullptr || !Lease->active()) {
				return std::unexpected(application::review_mutation_failure::unavailable);
			}
			if (!valid(Configuration) || !aRequest.LiveBinding.has_value()
				|| aRequest.LiveBinding->Target != Configuration.Target
				|| aRequest.LiveBinding->ActingIdentity != Configuration.ActingIdentity) {
				return std::unexpected(application::review_mutation_failure::undesignated_target);
			}
			const auto wire = compile_thread_resolution_request(
				aRequest, Configuration.CredentialContext.Authority.ServerContract);
			if (!wire.has_value()) {
				return std::unexpected(map_contract_failure(wire.error()));
			}
			auto response = perform(*wire, aStop, { });
			if (!response.has_value()) {
				return std::unexpected(response.error());
			}
			const auto observed = parse_thread_resolution_response(*response, aRequest);
			if (!observed.has_value()) {
				return std::unexpected(map_contract_failure(observed.error()));
			}
			return observed->Receipt;
		} catch (const std::bad_alloc&) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
	}

	auto review_mutation_session::submit_and_reconcile(
		const application::review_mutation_request& aRequest,
		const std::stop_token                       aStop)
		-> application::review_mutation_submission
	{
		const auto failure = [](const application::review_mutation_failure aFailure) {
			return application::review_mutation_submission{
				.Result = std::unexpected(aFailure),
			};
		};
		try {
			std::scoped_lock lock{ Mutex };
			if (aStop.stop_requested()) {
				return failure(application::review_mutation_failure::cancelled);
			}
			if (Lease == nullptr || !Lease->active() || Recovery == nullptr) {
				return failure(application::review_mutation_failure::unavailable);
			}
			if (!valid(Configuration) || !aRequest.LiveBinding.has_value()
				|| !application::valid(*aRequest.LiveBinding)
				|| aRequest.LiveBinding->Target != Configuration.Target
				|| aRequest.LiveBinding->ActingIdentity != Configuration.ActingIdentity) {
				return failure(application::review_mutation_failure::undesignated_target);
			}
			const auto wire = compile_review_comment_request(
				aRequest,
				Configuration.CredentialContext.Authority.ServerContract);
			if (!wire.has_value()) {
				return failure(map_contract_failure(wire.error()));
			}
			const auto fingerprint = canonical_write_fingerprint(*wire);
			const auto expected    = application::review_mutation_recovery_record{
				.IdempotencyKey       = aRequest.IdempotencyKey,
				.RequestFingerprint   = fingerprint,
				.AuthorityFingerprint = canonical_authority_fingerprint(
					*aRequest.LiveBinding),
				.Body   = aRequest.Proposal.Body,
				.Anchor = aRequest.LiveBinding->Anchor,
			};
			if (!application::valid(expected)) {
				return failure(application::review_mutation_failure::unavailable);
			}

			const auto finishKnown = [&](const application::review_mutation_recovery_record& aKnown)
				-> application::review_mutation_submission {
				if (!application::valid(aKnown) || !same_attempt(aKnown, expected)) {
					return failure(application::review_mutation_failure::unavailable);
				}
				if (aKnown.State == application::review_mutation_recovery_state::succeeded
					&& aKnown.Receipt.has_value()) {
					auto duplicate      = *aKnown.Receipt;
					duplicate.Duplicate = true;
					return { .Result = std::move(duplicate) };
				}
				if (aKnown.State != application::review_mutation_recovery_state::unresolved
					|| aKnown.Receipt.has_value()
					|| !aKnown.BaselineExactCommentIds.has_value()
					|| aKnown.Transmission
						!= application::review_mutation_recovery_transmission::possibly_transmitted) {
					return failure(application::review_mutation_failure::unavailable);
				}
				const auto current = observe_exact(aStop);
				if (!current.has_value()) {
					return failure(current.error());
				}
				auto observed = reconcile(aRequest, aStop);
				if (!observed.has_value()) {
					return failure(observed.error());
				}
				auto submission = application::review_mutation_submission{
					.Reconciliation = observed->Projection,
				};
				auto receipt = new_exact_receipt(
					*observed, *aKnown.BaselineExactCommentIds);
				if (!receipt.has_value()) {
					return submission;
				}
				receipt->Duplicate  = true;
				const auto resolved = Recovery->resolve(
					aRequest.IdempotencyKey, fingerprint, *receipt);
				if (!resolved.has_value()) {
					return submission;
				}
				submission.Result = std::move(*receipt);
				return submission;
			};

			const auto loaded = Recovery->load(aRequest.IdempotencyKey);
			if (!loaded.has_value()) {
				return failure(application::review_mutation_failure::unavailable);
			}
			if (loaded->has_value()) {
				return finishKnown(**loaded);
			}

			auto baseline = reconcile(aRequest, aStop);
			if (!baseline.has_value()) {
				return failure(baseline.error());
			}
			if (aStop.stop_requested()) {
				return failure(application::review_mutation_failure::cancelled);
			}
			auto admittedRecord                    = expected;
			admittedRecord.BaselineExactCommentIds = exact_comment_ids(*baseline);
			admittedRecord.Transmission
				= application::review_mutation_recovery_transmission::prepared;
			if (!application::valid(admittedRecord)) {
				return failure(application::review_mutation_failure::unavailable);
			}
			const auto admitted = Recovery->admit(admittedRecord);
			if (!admitted.has_value()) {
				return failure(application::review_mutation_failure::unavailable);
			}
			if (!admitted->Inserted) {
				return finishKnown(admitted->Record);
			}
			if (const auto current = observe_exact(aStop); !current.has_value()) {
				return failure(current.error());
			}
			if (aStop.stop_requested()) {
				return failure(application::review_mutation_failure::cancelled);
			}

			auto submission = application::review_mutation_submission{
				.MutationAttempted = true,
			};
			auto witnessed = std::make_shared<std::atomic_bool>(false);
			auto response  = perform(
				*wire,
				aStop,
				[this, witnessed, key = aRequest.IdempotencyKey, fingerprint] {
					if (witnessed->exchange(true, std::memory_order_acq_rel)) {
						return false;
					}
					const auto marked = Recovery->mark_possibly_transmitted(
						key, fingerprint);
					return marked.has_value()
						&& marked->Transmission
						== application::review_mutation_recovery_transmission::possibly_transmitted;
				});
			auto postReceipt = std::optional<application::review_mutation_receipt>{ };
			if (response.has_value()) {
				const auto parsed = parse_review_comment_response(*response, aRequest);
				if (parsed.has_value()) {
					postReceipt = parsed->Receipt;
				} else {
					submission.Result = std::unexpected(map_contract_failure(parsed.error()));
				}
			} else {
				submission.Result = std::unexpected(response.error());
			}
			const auto transmission = Recovery->load(aRequest.IdempotencyKey);
			if (!transmission.has_value() || !transmission->has_value()
				|| (**transmission).Transmission
					!= application::review_mutation_recovery_transmission::possibly_transmitted) {
				return submission;
			}

			// Once POST has been invoked its transmission is ambiguous even if the
			// caller cancels. The bounded transport therefore performs this one
			// credentialed GET with an independent token before returning.
			auto observed = reconcile(aRequest, { });
			if (observed.has_value()) {
				submission.Reconciliation = observed->Projection;
			}
			auto confirmed = postReceipt.has_value()
					&& !std::ranges::binary_search(
								 *admittedRecord.BaselineExactCommentIds,
								 postReceipt->ServerCommentId)
				? postReceipt
				: std::nullopt;
			if (!confirmed.has_value() && observed.has_value()) {
				confirmed = new_exact_receipt(
					*observed, *admittedRecord.BaselineExactCommentIds);
			}
			if (!confirmed.has_value()) {
				return submission;
			}
			confirmed->Duplicate = false;
			const auto resolved  = Recovery->resolve(
				aRequest.IdempotencyKey, fingerprint, *confirmed);
			if (!resolved.has_value()) {
				return submission;
			}
			submission.Result = std::move(*confirmed);
			return submission;
		} catch (...) {
			return failure(application::review_mutation_failure::unavailable);
		}
	}
} // namespace bpr::adapters::github::detail
