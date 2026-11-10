#include "github_mutation_contract_support.hpp"

#include "bpr/core/sha256.hpp"
#include "github_paged_read.hpp"

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <limits>
#include <span>
#include <system_error>
#include <tuple>
#include <utility>

namespace bpr::adapters::github::detail {
	auto failed(const review_mutation_contract_failure aFailure)
		-> std::unexpected<review_mutation_contract_failure>
	{
		return std::unexpected{ aFailure };
	}

	auto bounded_text(
		const std::string_view aText,
		const std::size_t      aMaximum) noexcept -> bool
	{
		return !aText.empty() && aText.size() <= aMaximum
			&& std::ranges::all_of(aText, [](const unsigned char aCharacter) {
				   return aCharacter >= 0x20U && aCharacter != 0x7FU;
			   });
	}

	auto graphql_token(const std::string_view aValue) noexcept -> bool
	{
		return !aValue.empty() && aValue.size() <= 256U
			&& std::ranges::all_of(aValue, [](const char aCharacter) {
				   const auto byte = static_cast<unsigned char>(aCharacter);
				   return byte >= 0x21U && byte <= 0x7eU && aCharacter != '"' && aCharacter != '\\';
			   });
	}

	auto utf8_round_trip(const std::string_view aText) -> bool
	{
		const auto decoded = QString::fromUtf8(
			aText.data(), static_cast<qsizetype>(aText.size()));
		const auto encoded = decoded.toUtf8();
		return static_cast<std::size_t>(encoded.size()) == aText.size()
			&& std::equal(encoded.begin(), encoded.end(), aText.begin());
	}

	auto common_headers() -> std::vector<http_header>
	{
		return {
			{ .Name = "Accept", .Value = "application/vnd.github+json" },
			{ .Name = "User-Agent", .Value = "BetterPullReview/0.1" },
			{ .Name = "X-GitHub-Api-Version", .Value = std::string{ rest_version } },
		};
	}

	auto pull_request_path(
		const application::review_mutation_target_binding& aTarget,
		const application::github_server_contract          aServer)
		-> std::string
	{
		return std::string{ rest_prefix(aServer) } + "/repos/"
			+ percent_encode(aTarget.Repository.Owner) + "/"
			+ percent_encode(aTarget.Repository.Name) + "/pulls/"
			+ std::to_string(aTarget.Number.value());
	}

	auto binds_designated_pull_request(
		const thread_pull_request_json&                    aPullRequest,
		const application::review_mutation_target_binding& aTarget) -> bool
	{
		return aPullRequest.Id == aTarget.PullRequestNodeId
			&& aPullRequest.DatabaseId == aTarget.PullRequestDatabaseId
			&& aPullRequest.Number == aTarget.Number.value()
			&& aPullRequest.Repository.Id == aTarget.RepositoryNodeId
			&& aPullRequest.Repository.DatabaseId == aTarget.RepositoryDatabaseId
			&& aPullRequest.Repository.NameWithOwner
			== aTarget.Repository.Owner + "/" + aTarget.Repository.Name;
	}

	auto reviewer_login(const std::string_view aLogin) -> bool
	{
		return !aLogin.empty() && aLogin.size() <= 39U
			&& std::ranges::all_of(aLogin, [](const char aCharacter) {
				   return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= 'A' && aCharacter <= 'Z')
					   || (aCharacter >= '0' && aCharacter <= '9') || aCharacter == '-' || aCharacter == '_' || aCharacter == '[' || aCharacter == ']';
			   });
	}

	auto reviewer_standing_facts(
		const std::optional<reviewer_login_json>& aAuthor,
		const reviewer_request_nodes_json&        aRequests,
		const reviewer_review_nodes_json&         aReviews)
		-> review_mutation_parse_result<core::reviewer_search_answer>
	{
		auto answer = core::reviewer_search_answer{ };
		if (aAuthor.has_value()) {
			if (!reviewer_login(aAuthor->Login)) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			answer.Author = aAuthor->Login;
		}
		if (aRequests.Nodes.size() > maximum_reviewer_list_items || aReviews.Nodes.size() > maximum_reviewer_list_items) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		for (const auto& node : aRequests.Nodes) {
			if (!node.RequestedReviewer.has_value() || node.RequestedReviewer->Typename != "User") {
				continue;
			}
			if (!node.RequestedReviewer->Login || !reviewer_login(*node.RequestedReviewer->Login) || !node.RequestedReviewer->Id || node.RequestedReviewer->Id->empty()) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			answer.Requested.push_back(*node.RequestedReviewer->Login);
		}
		for (const auto& node : aReviews.Nodes) {
			if (!node.Author.has_value()) {
				continue;
			}
			if (!reviewer_login(node.Author->Login)) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			answer.Reviewed.push_back(node.Author->Login);
		}
		return answer;
	}

	auto reviewer_binds(
		const std::string& aId, const std::uint64_t aDatabaseId, const std::uint64_t aNumber,
		const thread_repository_json& aRepository, const application::review_reviewer_request& aExpected) -> bool
	{
		return aExpected.LiveBinding.has_value()
			&& binds_designated_pull_request(thread_pull_request_json{ .Id = aId, .DatabaseId = aDatabaseId, .Number = aNumber, .Repository = aRepository }, aExpected.LiveBinding->Target);
	}

	auto comment_observation(
		const comment_subject_json&                     aComment,
		const application::review_comment_edit_request& aExpected)
		-> review_mutation_parse_result<review_description_observation>
	{
		if (aComment.Typename != comment_edit_typename(aExpected.Kind) || aComment.Id != aExpected.Subject
			|| !aExpected.LiveBinding.has_value() || !binds_designated_pull_request(aComment.PullRequest, aExpected.LiveBinding->Target)) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		// Only the viewer's own comment is theirs to edit here (owner,
		// 2026-10-27). GitHub also lets a repository's maintainers update
		// another account's comment (live on BPR-Test), so a comment the
		// viewer did not write is held not permitted, whatever
		// `viewerCanUpdate` says.
		return review_description_observation{ .Body = aComment.Body, .UpdatedAt = aComment.UpdatedAt, .ViewerCanUpdate = aComment.ViewerCanUpdate && aComment.ViewerDidAuthor };
	}

	auto permalink_source(const application::review_mutation_source_binding& aSource, const application::review_mutation_target_binding& aTarget,
						  const core::diff_side aSide) -> review_permalink_source
	{
		// [[BPR-1736]]. And an off-hunk base line the core placed, whose
		// file comment follows a refusal.
		const auto left = aSide == core::diff_side::left
			&& (aSource.Placement == core::review_placement::unresolvable_line || (aSource.CorePlacedOffHunk && aSource.LeftEndpoint));
		return { .Target = aTarget, .Endpoint = left ? aSource.LeftEndpoint : aSource.Endpoint, .Comparison = aSource.Comparison, .LineCount = aSource.LineCount };
	}

	auto publication_body(const application::review_mutation_request& aRequest) -> std::optional<std::string>
	{
		if (aRequest.Representation == core::review_publication_representation::native_line) {
			return aRequest.Proposal.Body;
		}
		if (aRequest.Representation != core::review_publication_representation::file_comment_with_permalink
			|| aRequest.Proposal.ReplyToThread || aRequest.Proposal.StartLine || !aRequest.Source || !aRequest.Source->Endpoint || !aRequest.LiveBinding
			|| aRequest.Source->Endpoint->path().generic_string() != aRequest.LiveBinding->Anchor.Path
			|| aRequest.Source->Comparison.Authority.Account != aRequest.ActingIdentity
			|| core::classify_review_body(aRequest.Proposal.Body) != core::review_body_compatibility::portable) {
			return std::nullopt;
		}
		return compile_review_permalink_body(permalink_source(*aRequest.Source, aRequest.LiveBinding->Target, aRequest.Proposal.Anchor.Side), aRequest.Proposal.Anchor,
											 aRequest.Proposal.Body);
	}

	auto submission_names(const core::review_disposition aDisposition)
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

	auto submitted_timestamp(const std::string_view aText) -> bool
	{
		if (aText.size() != 20U) {
			return false;
		}
		const auto value     = QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size()));
		const auto timestamp = QDateTime::fromString(value, Qt::ISODate);
		return timestamp.isValid() && timestamp.toString(Qt::ISODate) == value
			&& aText.back() == 'Z';
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

	auto canonical_write_fingerprint(
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

	auto canonical_authority_fingerprint(
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
		// [[BPR-1560]]. Only a comparison base that differs from the
		// recorded base adds material, so every earlier fingerprint holds.
		if (application::comparison_base(target) != target.BaseOid) {
			append_fingerprint_oid(material, application::comparison_base(target));
		}
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

	auto valid_file_subject_metadata(const comment_response_json& aComment) -> bool
	{
		// GitHub file subjects may carry its synthetic first-line position.
		// This is receipt metadata; the permalink alone supplies our source anchor.
		return !aComment.StartLine && !aComment.StartSide && !aComment.InReplyToId
			&& (!aComment.Line || (aComment.Line == 1U && aComment.OriginalLine == 1U && aComment.Side == "RIGHT"));
	}

	auto comment_observation(const comment_response_json&                aResponse,
							 const application::review_mutation_request& aExpected)
		-> review_mutation_parse_result<review_mutation_comment_observation>
	{
		const auto     commit            = core::parse_git_commit_oid(aResponse.CommitId);
		constexpr auto maximumGithubLine = static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1U;
		if (aResponse.Id == 0U || aResponse.User.Id == 0U || !commit
			|| !bounded_text(aResponse.NodeId, 256U)
			|| (aResponse.Line && (*aResponse.Line == 0U || *aResponse.Line > maximumGithubLine))) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		const auto& binding      = *aExpected.LiveBinding;
		const auto  expectedSide = binding.Anchor.Semantic.Side == core::diff_side::left ? "LEFT" : "RIGHT";
		const auto  fileSubject  = aResponse.SubjectType == "file";
		const auto  wantsFile    = aExpected.Representation == core::review_publication_representation::file_comment_with_permalink;
		const auto  expectedBody = publication_body(aExpected);
		if ((aResponse.SubjectType && *aResponse.SubjectType != "line" && !fileSubject)
			|| (fileSubject && !valid_file_subject_metadata(aResponse))
			|| (aResponse.OriginalLine && (*aResponse.OriginalLine == 0U || *aResponse.OriginalLine > maximumGithubLine))) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		// RFC-BPR-0089 M4. A comment written on an earlier commit at a line
		// a later one changed is outdated from the start (measured on
		// BPR-Test #116): GitHub keeps `commit_id` there and reports no
		// `line`. It is ours by the commit and line it was written at. Any
		// other outdated comment, anyone's, is well formed and not ours.
		const auto earlier = binding.Anchor.Commit != binding.Target.Semantic.ExpectedHead;
		if (!fileSubject && !aResponse.Line && !earlier) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		const auto writtenCommit = earlier ? core::parse_git_commit_oid(aResponse.OriginalCommitId.value_or(std::string{ })) : commit;
		const auto writtenLine   = earlier ? aResponse.OriginalLine : aResponse.Line;
		// [[BPR-1212]]. A range is ours only with its exact first line on
		// the same side; a single line only without one, so a range ending
		// at the same line with the same text is never taken for it.
		const auto expectedStart = aExpected.Proposal.StartLine ? std::optional<std::uint64_t>{ static_cast<std::uint64_t>(*aExpected.Proposal.StartLine) + 1U } : std::nullopt;
		if (aResponse.Path != binding.Anchor.Path || writtenCommit != std::optional{ binding.Anchor.Commit }
			|| fileSubject != wantsFile || !expectedBody || aResponse.Body != *expectedBody
			|| (!fileSubject && (aResponse.Side != expectedSide || !writtenLine || *writtenLine - 1U != application::remote_line(binding.Anchor)))
			|| (!fileSubject && (aResponse.StartLine != expectedStart || (expectedStart ? aResponse.StartSide != expectedSide : aResponse.StartSide.has_value())))
			|| aResponse.User.Id != binding.ActingIdentity.DatabaseId
			|| aResponse.User.NodeId != binding.ActingIdentity.NodeId
			|| aResponse.User.Login != binding.ActingIdentity.Login) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		// [[BPR-0816]]. Name the nested type: GCC 15 destroys a braced
		// aggregate member twice when a later member initializer throws.
		return review_mutation_comment_observation{
			.Receipt       = application::review_mutation_receipt{ .ServerCommentId = aResponse.NodeId, .Duplicate = false, .Representation = aExpected.Representation },
			.DatabaseId    = aResponse.Id,
			.NodeId        = aResponse.NodeId,
			.Anchor        = binding.Anchor,
			.Body          = aResponse.Body,
			.Author        = binding.ActingIdentity,
			.RemoteSubject = fileSubject ? application::review_remote_comment_subject::file : application::review_remote_comment_subject::line,
		};
	}

	auto response_headers_valid(
		const review_mutation_wire_response& aResponse) noexcept -> bool
	{
		return aResponse.Headers.size() <= 64U
			&& std::ranges::all_of(aResponse.Headers, [](const http_header& aHeader) {
				   return bounded_text(aHeader.Name, 256U)
					   && bounded_text(aHeader.Value, 8U * 1024U)
					   && aHeader.Name != "Location" && aHeader.Name != "location";
			   });
	}

	auto has_header(
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

	auto map_contract_failure(
		const review_mutation_contract_failure aFailure) noexcept
		-> application::review_mutation_failure
	{
		switch (aFailure) {
		case review_mutation_contract_failure::identity_mismatch:
		case review_mutation_contract_failure::invalid_request:
			return application::review_mutation_failure::undesignated_target;
		case review_mutation_contract_failure::target_unavailable:
			return application::review_mutation_failure::target_unavailable;
		case review_mutation_contract_failure::malformed_response:
			return application::review_mutation_failure::malformed_response;
		default:
			return application::review_mutation_failure::unavailable;
		}
	}

	auto staged_api_origin(const application::review_mutation_target_binding& aTarget) -> std::string
	{
		return "https://" + (aTarget.ServerContract == application::github_server_contract::github_com_2022_11_28 ? std::string{ "api.github.com" } : std::string{ aTarget.Hostname.value() });
	}

	auto staged_web_root(const application::review_mutation_target_binding& aTarget) -> std::string
	{
		return "https://" + std::string{ aTarget.Hostname.value() } + "/" + aTarget.Repository.Owner + "/" + aTarget.Repository.Name + "/pull/" + std::to_string(aTarget.Number.value());
	}

	auto staged_actor(const review_author_json& aActor) -> std::optional<application::staged_review_actor>
	{
		if (!aActor.Id || !bounded_text(aActor.NodeId, 256) || !bounded_text(aActor.Login, 256)) {
			return std::nullopt;
		}
		return application::staged_review_actor{ aActor.Id, aActor.NodeId, aActor.Login };
	}

	auto staged_page_suffix(std::uint32_t aPage) -> std::string
	{
		return "?per_page=100&page=" + std::to_string(aPage);
	}

	auto paged_next(const review_mutation_wire_response& aResponse, const application::review_mutation_target_binding& aTarget, std::string_view aWirePath, std::uint32_t aPage, std::uint32_t aMaximum, bool aListed) -> std::optional<bool>
	{
		bool next = false, seen = false;
		for (const auto& header : aResponse.Headers) {
			if (header.Name.size() != 4 || !std::ranges::equal(header.Name, std::string_view{ "link" }, [](unsigned char aLeft, unsigned char aRight) { return std::tolower(aLeft) == std::tolower(aRight); })) {
				continue;
			}
			if (seen || !aListed) {
				return std::nullopt;
			}
			seen             = true;
			const auto links = parse_link_header(header.Value);
			if (!links) {
				return std::nullopt;
			}
			std::vector<std::string_view> relations;
			for (const auto& link : *links) {
				// Exactly `<url>; rel="relation"`, one relation per link.
				const auto names = link_relations(link);
				if (link.Parameters.size() != 1 || !link.Parameters.front().Quoted || !names || names->size() != 1) {
					return std::nullopt;
				}
				const auto url      = link.Target;
				const auto relation = names->front();
				if (std::ranges::find(relations, relation) != relations.end()) {
					return std::nullopt;
				}
				relations.push_back(relation);
				const auto prefix = staged_api_origin(aTarget) + std::string{ aWirePath.substr(0, aWirePath.find('?')) };
				// [[BPR-1538]]. GitHub names the next page by the
				// repository's number, not its name:
				// `https://api.github.com/repositories/1328277631/pulls/70/comments?per_page=100&page=2`
				// on MikeMarcin/BPR-Test#70, under `/api/v3` on GHES. Only
				// this repository's number names the same list.
				const auto byName   = std::string{ rest_prefix(aTarget.ServerContract) } + "/repos/" + percent_encode(aTarget.Repository.Owner) + "/" + percent_encode(aTarget.Repository.Name) + "/";
				const auto path     = std::string{ aWirePath.substr(0, aWirePath.find('?')) };
				const auto byNumber = path.starts_with(byName)
					? std::optional{ staged_api_origin(aTarget) + std::string{ rest_prefix(aTarget.ServerContract) } + "/repositories/" + std::to_string(aTarget.RepositoryDatabaseId) + "/" + path.substr(byName.size()) }
					: std::nullopt;
				// [[BPR-1540]]. The page a link names, read from it: a list
				// longer than the bound names its last page past it.
				const auto namedPage = [&url](const std::string& aPrefix) -> std::optional<std::uint32_t> {
					if (!url.starts_with(aPrefix)) {
						return std::nullopt;
					}
					const auto rest = url.substr(aPrefix.size());
					for (const auto& [lead, tail] : std::array<std::pair<std::string_view, std::string_view>, 2>{ { { "?per_page=100&page=", "" }, { "?page=", "&per_page=100" } } }) {
						if (!rest.starts_with(lead) || !rest.ends_with(tail) || rest.size() <= lead.size() + tail.size()) {
							continue;
						}
						const auto digits = rest.substr(lead.size(), rest.size() - lead.size() - tail.size());
						auto       number = std::uint32_t{ 0U };
						const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), number);
						if (parsed.ec == std::errc{ } && parsed.ptr == digits.data() + digits.size() && digits.front() != '0' && number <= 1'000'000U) {
							return number;
						}
					}
					return std::nullopt;
				};
				auto page = namedPage(prefix);
				if (!page && byNumber && aTarget.RepositoryDatabaseId != 0U) {
					page = namedPage(*byNumber);
				}
				if (page && relation != "last" && *page > aMaximum) {
					page.reset();
				}
				if (!page) {
					return std::nullopt;
				}
				if (relation == "next") {
					if (*page != aPage + 1) {
						return std::nullopt;
					}
					next = true;
				} else if (relation == "prev") {
					if (*page + 1 != aPage) {
						return std::nullopt;
					}
				} else if (relation == "first") {
					if (*page != 1) {
						return std::nullopt;
					}
				} else if (relation == "last") {
					if (*page < aPage) {
						return std::nullopt;
					}
				} else {
					return std::nullopt;
				}
			}
		}
		if (next && aPage >= aMaximum) {
			return std::nullopt;
		}
		return next;
	}

	auto staged_comment_entity(const staged_comment_json& aComment, const application::staged_review_request& aExpected, const bool aLines) -> std::optional<application::staged_review_comment>
	{
		const auto& target = aExpected.Binding.Target;
		const auto  actor  = staged_actor(aComment.User);
		const auto  commit = core::parse_git_commit_oid(aComment.Commit);
		if (!actor || !commit || *commit != target.Semantic.ExpectedHead || !aComment.Id || !aComment.Review || !bounded_text(aComment.NodeId, 256)
			|| !utf8_round_trip(aComment.Body) || aComment.Body.size() > maximum_review_mutation_body_bytes
			|| aComment.PullRequestUrl != staged_api_origin(target) + pull_request_path(target, target.ServerContract)
			|| aComment.WebUrl != staged_web_root(target) + "#discussion_r" + std::to_string(aComment.Id)
			|| (aExpected.Review.value() && aExpected.Review.value() != aComment.Review)
			|| (aExpected.Comment && aExpected.Comment != aComment.Id)
			|| (aComment.Subject && *aComment.Subject != "line" && *aComment.Subject != "file")) {
			return std::nullopt;
		}
		if (!aLines) {
			if (aComment.Path.empty() || !utf8_round_trip(aComment.Path)) {
				return std::nullopt;
			}
			return application::staged_review_comment{ aComment.Id, aComment.NodeId, aComment.WebUrl, *actor, application::review_database_id{ aComment.Review }, std::nullopt, aComment.Body, aComment.Reply.has_value() };
		}
		if (aComment.Subject == std::optional<std::string>{ "file" }) {
			if (aComment.Line || aComment.StartLine || aComment.Side || aComment.StartSide || aComment.Reply) {
				return { };
			}
			const auto source = core::tour_code_destination{ .Commit = *commit, .Path = aComment.Path };
			if (!core::valid(source)) {
				return { };
			}
			return application::staged_review_comment{ aComment.Id, aComment.NodeId, aComment.WebUrl, *actor, application::review_database_id{ aComment.Review }, source, aComment.Body, false };
		}
		constexpr auto maximumLine = std::uint64_t{ std::numeric_limits<std::uint32_t>::max() } + 1;
		if (!aComment.Line || *aComment.Line == 0 || *aComment.Line > maximumLine
			|| (aComment.Side != "LEFT" && aComment.Side != "RIGHT")
			|| aComment.StartLine.has_value() != aComment.StartSide.has_value()
			|| (aComment.StartLine && (*aComment.StartLine == 0 || *aComment.StartLine >= *aComment.Line || aComment.StartSide != aComment.Side))) {
			return std::nullopt;
		}
		const auto source = core::tour_code_destination{ .Commit = aComment.Side == "LEFT" ? application::comparison_base(target) : *commit, .Path = aComment.Path, .Side = aComment.Side == "LEFT" ? core::diff_side::left : core::diff_side::right, .FirstLine = static_cast<std::uint32_t>(aComment.StartLine.value_or(*aComment.Line) - 1), .LastLine = static_cast<std::uint32_t>(*aComment.Line - 1) };
		if (!core::valid(source)) {
			return std::nullopt;
		}
		return application::staged_review_comment{ aComment.Id, aComment.NodeId, aComment.WebUrl, *actor, application::review_database_id{ aComment.Review }, source, aComment.Body, aComment.Reply.has_value() };
	}

	auto text_digest(const std::string_view aText) -> std::array<std::uint8_t, 32>
	{
		return core::sha256(std::span{ reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() });
	}

	auto digest_hex(const std::array<std::uint8_t, 32>& aDigest) -> std::string
	{
		return QByteArray::fromRawData(reinterpret_cast<const char*>(aDigest.data()), static_cast<qsizetype>(aDigest.size())).toHex().toStdString();
	}

	auto publication_fingerprint(const application::review_mutation_request& aRequest, const live_review_mutation_configuration& aConfig) -> std::array<std::uint8_t, 32>
	{
		auto material = std::string{ "bpr-logical-publication-v1" };
		append_fingerprint_text(material, aRequest.IdempotencyKey);
		append_fingerprint_text(material, digest_hex(canonical_authority_fingerprint(*aRequest.LiveBinding)));
		append_fingerprint_text(material, digest_hex(canonical_write_fingerprint(*compile_review_comment_request(aRequest, aConfig.Target.ServerContract))));
		append_fingerprint_integer(material, aConfig.CredentialContext.Authority.Profile.value());
		append_fingerprint_integer(material, aConfig.CredentialContext.Authority.ProfileRevision.value());
		append_fingerprint_integer(material, aConfig.CredentialContext.Authority.BprProfile.value());
		return text_digest(material);
	}

	auto utc_now_ms() -> std::int64_t
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}
}
