#include "github_mutation_contract.hpp"

#include "bpr/application/review_mutation_coordinator.hpp"
#include "bpr/core/sha256.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <concepts>
#include <limits>
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
				encoded.push_back(hexadecimal[(character >> 4U) & 0x0FU]);
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

		[[nodiscard]] auto mutation_body(
			const application::review_mutation_request& aRequest) -> std::string
		{
			const auto& binding = *aRequest.LiveBinding;
			const auto  commit  = core::canonical_hex(binding.Anchor.Commit);
			const auto  githubLine
				= static_cast<std::uint64_t>(binding.Anchor.Semantic.Line) + 1U;
			QJsonObject object;
			object.insert("body", QString::fromUtf8(aRequest.Proposal.Body.data(), static_cast<qsizetype>(aRequest.Proposal.Body.size())));
			object.insert("commit_id", QString::fromUtf8(commit.view().data(), static_cast<qsizetype>(commit.view().size())));
			object.insert("line", static_cast<qint64>(githubLine));
			object.insert("path", QString::fromUtf8(binding.Anchor.Path.data(), static_cast<qsizetype>(binding.Anchor.Path.size())));
			object.insert(
				"side",
				binding.Anchor.Semantic.Side == core::diff_side::left ? "LEFT" : "RIGHT");
			const auto bytes = QJsonDocument{ object }.toJson(QJsonDocument::Compact);
			return { bytes.constData(), static_cast<std::size_t>(bytes.size()) };
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

		[[nodiscard]] auto document(const review_mutation_wire_response& aResponse)
			-> std::optional<QJsonObject>
		{
			if (aResponse.Body.empty()
				|| aResponse.Body.size() > maximum_review_mutation_response_bytes) {
				return std::nullopt;
			}
			if (!json_preflight{ aResponse.Body }.run()) {
				return std::nullopt;
			}
			QJsonParseError error;
			const auto      parsed = QJsonDocument::fromJson(
				QByteArray{ aResponse.Body.data(), static_cast<qsizetype>(aResponse.Body.size()) },
				&error);
			if (error.error != QJsonParseError::NoError || !parsed.isObject()) {
				return std::nullopt;
			}
			return parsed.object();
		}

		[[nodiscard]] auto array_document(
			const review_mutation_wire_response& aResponse)
			-> std::optional<QJsonArray>
		{
			if (aResponse.Body.empty()
				|| aResponse.Body.size() > maximum_review_mutation_response_bytes
				|| !json_preflight{ aResponse.Body }.run()) {
				return std::nullopt;
			}
			QJsonParseError error;
			const auto      parsed = QJsonDocument::fromJson(
				QByteArray{ aResponse.Body.data(), static_cast<qsizetype>(aResponse.Body.size()) },
				&error);
			if (error.error != QJsonParseError::NoError || !parsed.isArray()) {
				return std::nullopt;
			}
			return parsed.array();
		}

		[[nodiscard]] auto text(const QJsonValue& aValue) -> std::optional<std::string>
		{
			if (!aValue.isString()) {
				return std::nullopt;
			}
			const auto bytes = aValue.toString().toUtf8();
			return std::string{ bytes.constData(), static_cast<std::size_t>(bytes.size()) };
		}

		[[nodiscard]] auto integer(const QJsonValue& aValue) -> std::optional<std::uint64_t>
		{
			if (!aValue.isDouble()) {
				return std::nullopt;
			}
			const auto value = aValue.toDouble();
			if (!std::isfinite(value) || value <= 0.0 || std::floor(value) != value
				|| value > 9'007'199'254'740'991.0) {
				return std::nullopt;
			}
			return static_cast<std::uint64_t>(value);
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
					} else if (aRequest.Operation
							   == review_mutation_operation::create_review_comment) {
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

	auto parse_target_observation(
		const review_mutation_wire_response&               aResponse,
		const application::review_mutation_target_binding& aExpected)
		-> review_mutation_parse_result<review_mutation_target_observation>
	{
		if (aResponse.Status != 200 || !response_headers_valid(aResponse)) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto object = document(aResponse);
		if (!object.has_value()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		const auto id               = integer(object->value("id"));
		const auto node             = text(object->value("node_id"));
		const auto number           = integer(object->value("number"));
		const auto state            = text(object->value("state"));
		const auto head             = object->value("head").toObject();
		const auto headSha          = text(head.value("sha"));
		const auto headRef          = text(head.value("ref"));
		const auto base             = object->value("base").toObject();
		const auto baseSha          = text(base.value("sha"));
		const auto baseRef          = text(base.value("ref"));
		const auto baseRepository   = base.value("repo").toObject();
		const auto repositoryId     = integer(baseRepository.value("id"));
		const auto repositoryNode   = text(baseRepository.value("node_id"));
		const auto fullName         = text(baseRepository.value("full_name"));
		const auto baseRepositoryId = integer(baseRepository.value("id"));
		const auto parsedHead       = headSha.has_value()
			? core::parse_git_commit_oid(*headSha)
			: std::optional<core::git_commit_oid>{ };
		const auto parsedBase       = baseSha.has_value()
			? core::parse_git_commit_oid(*baseSha)
			: std::optional<core::git_commit_oid>{ };
		const auto expectedFullName = aExpected.Repository.Owner + "/" + aExpected.Repository.Name;
		if (!id.has_value() || !node.has_value() || !number.has_value()
			|| !state.has_value() || !repositoryId.has_value()
			|| !repositoryNode.has_value() || !fullName.has_value()
			|| !baseRepositoryId.has_value() || !parsedHead.has_value()
			|| !parsedBase.has_value() || !baseRef.has_value()
			|| !headRef.has_value()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		if (*id != aExpected.PullRequestDatabaseId
			|| *node != aExpected.PullRequestNodeId
			|| *number != aExpected.Number.value() || *state != "open"
			|| *repositoryId != aExpected.RepositoryDatabaseId
			|| *repositoryNode != aExpected.RepositoryNodeId
			|| *fullName != expectedFullName
			|| *baseRepositoryId != aExpected.BaseRepositoryDatabaseId
			|| *parsedBase != aExpected.BaseOid || *baseRef != aExpected.BaseRef
			|| *headRef != aExpected.HeadRef) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		auto observed                  = aExpected;
		observed.Semantic.ExpectedHead = *parsedHead;
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
		const auto object = document(aResponse);
		if (!object.has_value()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		const auto id    = integer(object->value("id"));
		const auto node  = text(object->value("node_id"));
		const auto login = text(object->value("login"));
		if (!id.has_value() || !node.has_value() || !login.has_value()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		if (*id != aExpected.DatabaseId || *node != aExpected.NodeId
			|| *login != aExpected.Login) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		return review_mutation_viewer_observation{ .Identity = aExpected };
	}

	auto parse_review_comment_response(
		const review_mutation_wire_response&        aResponse,
		const application::review_mutation_request& aExpected)
		-> review_mutation_parse_result<review_mutation_comment_observation>
	{
		if (aResponse.Status != 201 || !response_headers_valid(aResponse)
			|| !aExpected.LiveBinding.has_value()) {
			return failed(review_mutation_contract_failure::invalid_status);
		}
		const auto object = document(aResponse);
		if (!object.has_value()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		const auto  id           = integer(object->value("id"));
		const auto  node         = text(object->value("node_id"));
		const auto  path         = text(object->value("path"));
		const auto  commitText   = text(object->value("commit_id"));
		const auto  side         = text(object->value("side"));
		const auto  line         = integer(object->value("line"));
		const auto  body         = text(object->value("body"));
		const auto  user         = object->value("user").toObject();
		const auto  authorId     = integer(user.value("id"));
		const auto  authorNode   = text(user.value("node_id"));
		const auto  authorLogin  = text(user.value("login"));
		const auto  commit       = commitText.has_value()
			? core::parse_git_commit_oid(*commitText)
			: std::optional<core::git_commit_oid>{ };
		const auto& binding      = *aExpected.LiveBinding;
		const auto  expectedSide = binding.Anchor.Semantic.Side == core::diff_side::left
			? std::string_view{ "LEFT" }
			: std::string_view{ "RIGHT" };
		if (!id.has_value() || !node.has_value() || !path.has_value()
			|| !commit.has_value() || !side.has_value() || !line.has_value()
			|| !body.has_value() || !authorId.has_value()
			|| !authorNode.has_value() || !authorLogin.has_value()) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		constexpr auto maximumGithubLine
			= static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1U;
		if (*line > maximumGithubLine) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		const auto semanticLine = static_cast<std::uint32_t>(*line - 1U);
		if (*path != binding.Anchor.Path || *commit != binding.Anchor.Commit
			|| *side != expectedSide || semanticLine != binding.Anchor.Semantic.Line
			|| *body != aExpected.Proposal.Body
			|| *authorId != binding.ActingIdentity.DatabaseId
			|| *authorNode != binding.ActingIdentity.NodeId
			|| *authorLogin != binding.ActingIdentity.Login) {
			return failed(review_mutation_contract_failure::identity_mismatch);
		}
		return review_mutation_comment_observation{
			.Receipt = {
				.ServerCommentId = *node,
				.Duplicate       = false,
			},
			.DatabaseId = *id,
			.NodeId     = *node,
			.Anchor     = binding.Anchor,
			.Body       = *body,
			.Author     = binding.ActingIdentity,
		};
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
		const auto array = array_document(aResponse);
		if (!array.has_value() || array->size() > 100) {
			return failed(review_mutation_contract_failure::malformed_response);
		}
		auto result = review_mutation_server_reconciliation{ };
		result.Projection.reserve(static_cast<std::size_t>(array->size()));
		for (const auto& value : *array) {
			if (!value.isObject()) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			const auto object = value.toObject();
			const auto node   = text(object.value("node_id"));
			const auto body   = text(object.value("body"));
			if (!node.has_value() || !body.has_value()
				|| !bounded_text(*node, 256U)
				|| body->size() > maximum_review_mutation_response_bytes
				|| !utf8_round_trip(*body)) {
				return failed(review_mutation_contract_failure::malformed_response);
			}
			result.Projection.push_back(application::review_comment_projection{
				.ServerId = *node,
				.Body     = *body,
			});

			const auto bytes     = QJsonDocument{ object }.toJson(QJsonDocument::Compact);
			const auto candidate = review_mutation_wire_response{
				.Status = 201,
				.Body   = std::string{
					bytes.constData(), static_cast<std::size_t>(bytes.size()) },
			};
			const auto exact = parse_review_comment_response(candidate, aExpected);
			if (exact.has_value()) {
				if (result.ExactReceipt.has_value()) {
					return failed(review_mutation_contract_failure::identity_mismatch);
				}
				result.ExactReceipt = exact->Receipt;
			} else if (exact.error()
					   != review_mutation_contract_failure::identity_mismatch) {
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
		review_mutation_wire_request aRequest,
		const std::stop_token        aStop)
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
			aStop);
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

			const auto finish_known = [&](
										  const application::review_mutation_recovery_record& aKnown,
										  const bool                                          aAlreadyObserved) -> application::review_mutation_submission {
				if (!same_attempt(aKnown, expected)) {
					return failure(application::review_mutation_failure::unavailable);
				}
				if (aKnown.State == application::review_mutation_recovery_state::succeeded
					&& aKnown.Receipt.has_value()) {
					auto duplicate      = *aKnown.Receipt;
					duplicate.Duplicate = true;
					return { .Result = std::move(duplicate) };
				}
				if (aKnown.State != application::review_mutation_recovery_state::unresolved
					|| aKnown.Receipt.has_value()) {
					return failure(application::review_mutation_failure::unavailable);
				}
				if (!aAlreadyObserved) {
					const auto current = observe_exact(aStop);
					if (!current.has_value()) {
						return failure(current.error());
					}
				}
				auto observed = reconcile(aRequest, aStop);
				if (!observed.has_value()) {
					return failure(observed.error());
				}
				auto submission = application::review_mutation_submission{
					.Reconciliation = observed->Projection,
				};
				if (!observed->ExactReceipt.has_value()) {
					return submission;
				}
				auto receipt        = *observed->ExactReceipt;
				receipt.Duplicate   = true;
				const auto resolved = Recovery->resolve(
					aRequest.IdempotencyKey, fingerprint, receipt);
				if (!resolved.has_value()) {
					return submission;
				}
				submission.Result = std::move(receipt);
				return submission;
			};

			const auto loaded = Recovery->load(aRequest.IdempotencyKey);
			if (!loaded.has_value()) {
				return failure(application::review_mutation_failure::unavailable);
			}
			if (loaded->has_value()) {
				return finish_known(**loaded, false);
			}
			if (const auto current = observe_exact(aStop); !current.has_value()) {
				return failure(current.error());
			}
			if (aStop.stop_requested()) {
				return failure(application::review_mutation_failure::cancelled);
			}
			const auto admitted = Recovery->admit(expected);
			if (!admitted.has_value()) {
				return failure(application::review_mutation_failure::unavailable);
			}
			if (!admitted->Inserted) {
				return finish_known(admitted->Record, true);
			}

			auto submission = application::review_mutation_submission{
				.MutationAttempted = true,
			};
			auto response    = perform(*wire, aStop);
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

			// Once POST has been invoked its transmission is ambiguous even if the
			// caller cancels. The bounded transport therefore performs this one
			// credentialed GET with an independent token before returning.
			auto observed = reconcile(aRequest, { });
			if (observed.has_value()) {
				submission.Reconciliation = observed->Projection;
			}
			auto confirmed = observed.has_value() && observed->ExactReceipt.has_value()
				? observed->ExactReceipt
				: postReceipt;
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
