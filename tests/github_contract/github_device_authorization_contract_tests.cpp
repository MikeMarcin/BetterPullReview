#include "github_device_authorization_contract.hpp"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {
	using namespace bpr::adapters::github::detail;

	constexpr std::string_view synthetic_client_id   = "Iv23synthetic0000000";
	constexpr std::string_view synthetic_device_code = "synthetic-device-code-708d";
	constexpr std::string_view synthetic_user_code   = "ABCD-1234";
	constexpr std::string_view synthetic_access_token
		= "synthetic_access_token_708d";
	constexpr std::string_view synthetic_refresh_token
		= "synthetic_refresh_token_708d";
	constexpr std::string_view synthetic_description = "synthetic description";
	constexpr std::string_view synthetic_official_uri
		= "https://docs.github.com/synthetic";

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto read_all(const std::filesystem::path& aPath) -> std::string
	{
		std::ifstream input{ aPath, std::ios::binary };
		expect(input.good(), "fixture file opens");
		return {
			std::istreambuf_iterator<char>{ input },
			std::istreambuf_iterator<char>{ },
		};
	}

	[[nodiscard]] auto parse_object(const std::filesystem::path& aPath)
		-> QJsonObject
	{
		QJsonParseError error;
		const auto      bytes    = QByteArray::fromStdString(read_all(aPath));
		const auto      document = QJsonDocument::fromJson(bytes, &error);
		expect(
			error.error == QJsonParseError::NoError && document.isObject(),
			"fixture is valid JSON");
		return document.object();
	}

	[[nodiscard]] auto outcome_fixture(const std::string_view aName)
		-> QJsonObject
	{
		const auto root = std::filesystem::path{
			BPR_GITHUB_DEVICE_AUTHORIZATION_FIXTURE_ROOT
		};
		const auto fixture
			= parse_object(root / "outcomes" / (std::string{ aName } + ".json"));
		expect(
			fixture.value(QLatin1StringView{ "schema_name" }).toString()
					== QStringLiteral("bpr.rfc0016.device_contract")
				&& fixture.value(QLatin1StringView{ "schema_version" })
						.toInt()
					== 1,
			"recorded outcome carries the device contract schema");
		return fixture;
	}

	/// Load one documented-not-recorded shape. It lives in a sibling tree
	/// rather than in the recorded capture, because the recorded manifest is
	/// digest-bound to an owner-accepted attended sitting that contained no
	/// renewal course: adding a tenth artifact to it would either break that
	/// binding or forge membership in a sitting that never happened.
	[[nodiscard]] auto documented_fixture(const std::string_view aName)
		-> QJsonObject
	{
		const auto root = std::filesystem::path{
			BPR_GITHUB_DEVICE_DOCUMENTED_FIXTURE_ROOT
		};
		const auto fixture
			= parse_object(root / "outcomes" / (std::string{ aName } + ".json"));
		expect(
			fixture.value(QLatin1StringView{ "schema_name" }).toString()
					== QStringLiteral("bpr.rfc0016.device_documented_contract")
				&& fixture.value(QLatin1StringView{ "schema_version" }).toInt()
					== 1,
			"documented outcome carries the documented contract schema");
		const auto provenance
			= fixture.value(QLatin1StringView{ "provenance" }).toObject();
		expect(
			provenance.value(QLatin1StringView{ "source_kind" }).toString()
				== QStringLiteral("documented_not_recorded"),
			"and says so in its provenance");
		expect(
			provenance.value(QLatin1StringView{ "recorded" }).isBool()
				&& !provenance.value(QLatin1StringView{ "recorded" }).toBool(),
			"and claims no recording");
		return fixture;
	}

	/// Rebuild the provider response body from the recording: the retained
	/// values verbatim, and synthetic bytes standing in for each recorded
	/// observed-but-omitted transient field. The recorded field inventory is
	/// authoritative; nothing else may appear.
	[[nodiscard]] auto synthetic_body(const QJsonObject& aFixture) -> QJsonObject
	{
		const auto projection
			= aFixture.value(QLatin1StringView{ "projection" }).toObject();
		const auto values
			= projection.value(QLatin1StringView{ "values" }).toObject();
		auto body = QJsonObject{ };
		for (const auto& field :
			 projection.value(QLatin1StringView{ "observed_fields" }).toArray()) {
			const auto name = field.toString();
			if (values.contains(name)) {
				body.insert(name, values.value(name));
				continue;
			}
			if (name == QStringLiteral("device_code")) {
				body.insert(name, QString::fromUtf8(synthetic_device_code));
			} else if (name == QStringLiteral("user_code")) {
				body.insert(name, QString::fromUtf8(synthetic_user_code));
			} else if (name == QStringLiteral("access_token")) {
				body.insert(name, QString::fromUtf8(synthetic_access_token));
			} else if (name == QStringLiteral("refresh_token")) {
				body.insert(name, QString::fromUtf8(synthetic_refresh_token));
			} else if (name == QStringLiteral("error_description")) {
				body.insert(name, QString::fromUtf8(synthetic_description));
			} else if (name == QStringLiteral("error_uri")) {
				body.insert(name, QString::fromUtf8(synthetic_official_uri));
			} else {
				expect(false, "every omitted transient field has a stand-in");
			}
		}
		return body;
	}

	[[nodiscard]] auto response_from(
		const QJsonObject& aFixture,
		const QJsonObject& aBody) -> wire_response
	{
		return wire_response{
			.Status = aFixture.value(QLatin1StringView{ "http_status" }).toInt(),
			.Headers
			= { { .Name = "Content-Type", .Value = "application/json" } },
			.Body = QJsonDocument{ aBody }
						.toJson(QJsonDocument::Compact)
						.toStdString(),
		};
	}

	[[nodiscard]] auto compiled_for(const QJsonObject& aFixture)
		-> device_authorization_wire_request
	{
		const auto endpoint
			= aFixture.value(QLatin1StringView{ "endpoint" }).toString();
		if (endpoint == QStringLiteral("device_code")) {
			const auto request = compile_device_code_request(
				server_contract::github_com,
				synthetic_client_id);
			expect(request.has_value(), "device code request compiles");
			return *request;
		}
		const auto request = compile_device_token_request(
			server_contract::github_com,
			synthetic_client_id,
			synthetic_device_code);
		expect(request.has_value(), "token request compiles");
		return *request;
	}

	void test_compiled_requests_match_every_recorded_endpoint()
	{
		for (const auto name :
			 { "device_code_success", "device_flow_disabled",
			   "authorization_pending", "slow_down", "access_denied",
			   "expired_token", "token_success" }) {
			const auto fixture = outcome_fixture(name);
			const auto request = compiled_for(fixture);
			expect(
				request.Url
					== fixture.value(QLatin1StringView{ "endpoint_url" })
						   .toString()
						   .toStdString(),
				"compiled URL equals the recorded endpoint URL");
			expect(
				fixture.value(QLatin1StringView{ "request_method" }).toString()
						== QStringLiteral("POST")
					&& request.Method == http_method::post,
				"the recorded and compiled methods are both POST");
			const auto contentType = std::ranges::find_if(
				request.Headers,
				[](const http_header& aHeader) {
					return aHeader.Name == "Content-Type";
				});
			expect(
				contentType != request.Headers.end()
					&& contentType->Value
						== fixture
							   .value(QLatin1StringView{
								   "request_content_type" })
							   .toString()
							   .toStdString(),
				"the compiled content type equals the recorded one");
		}
	}

	void test_compiled_bodies_are_exact()
	{
		const auto code = compile_device_code_request(
			server_contract::github_com,
			synthetic_client_id);
		expect(
			code.has_value()
				&& code->Body == "client_id=Iv23synthetic0000000",
			"the device code body carries exactly the client id");
		expect(
			code->Headers.size() == 3U,
			"the device code request carries exactly three headers");

		const auto token = compile_device_token_request(
			server_contract::github_com,
			synthetic_client_id,
			synthetic_device_code);
		expect(
			token.has_value()
				&& token->Body
					== "client_id=Iv23synthetic0000000"
					   "&device_code=synthetic-device-code-708d"
					   "&grant_type=urn%3Aietf%3Aparams%3Aoauth%3A"
					   "grant-type%3Adevice_code",
			"the token body form-encodes client id, device code, and grant type");
	}

	void test_ghes_and_invalid_inputs_are_refused()
	{
		expect(
			!compile_device_code_request(
				 server_contract::ghes_3_18,
				 synthetic_client_id)
				 .has_value(),
			"GHES has no registered device client and is refused");
		expect(
			!compile_device_token_request(
				 server_contract::ghes_3_18,
				 synthetic_client_id,
				 synthetic_device_code)
				 .has_value(),
			"GHES token requests are refused");
		for (const auto bad :
			 { "", "has space", "amp&ersand", "equals=", "slash/" }) {
			expect(
				!compile_device_code_request(server_contract::github_com, bad)
					 .has_value(),
				"a malformed client id never compiles");
		}
		expect(
			!compile_device_code_request(
				 server_contract::github_com,
				 std::string(65U, 'a'))
				 .has_value(),
			"an oversized client id never compiles");
		expect(
			!compile_device_token_request(
				 server_contract::github_com,
				 synthetic_client_id,
				 "")
				 .has_value(),
			"an empty device code never compiles");
		expect(
			!compile_device_token_request(
				 server_contract::github_com,
				 synthetic_client_id,
				 std::string(513U, 'a'))
				 .has_value(),
			"an oversized device code never compiles");
	}

	void test_recorded_registration_client_id_is_well_formed()
	{
		const auto root = std::filesystem::path{
			BPR_GITHUB_DEVICE_AUTHORIZATION_FIXTURE_ROOT
		};
		const auto registration = parse_object(root / "registration.json");
		const auto clientId
			= registration.value(QLatin1StringView{ "public_client_id" })
				  .toString()
				  .toStdString();
		expect(
			valid_device_client_id(clientId),
			"the recorded public client id satisfies the compiled validation");
	}

	void test_device_code_success_parses_to_the_recorded_grant()
	{
		const auto fixture = outcome_fixture("device_code_success");
		const auto values  = fixture.value(QLatin1StringView{ "projection" })
								 .toObject()
								 .value(QLatin1StringView{ "values" })
								 .toObject();
		const auto outcome = parse_device_code_response(
			response_from(fixture, synthetic_body(fixture)));
		expect(outcome.has_value(), "the recorded success shape parses");
		const auto* grant = std::get_if<device_code_grant>(&*outcome);
		expect(grant != nullptr, "the recorded success is a grant");
		expect(
			grant->DeviceCode == synthetic_device_code
				&& grant->UserCode == synthetic_user_code,
			"the grant surfaces the transient codes to the host");
		expect(
			grant->VerificationUri
					== values.value(QLatin1StringView{ "verification_uri" })
						   .toString()
						   .toStdString()
				&& grant->ExpiresInSeconds
					== static_cast<std::uint32_t>(
						values.value(QLatin1StringView{ "expires_in" }).toInt())
				&& grant->IntervalSeconds
					== static_cast<std::uint32_t>(
						values.value(QLatin1StringView{ "interval" }).toInt()),
			"the grant carries exactly the recorded retained values");
	}

	void test_device_flow_disabled_parses_to_the_typed_refusal()
	{
		const auto fixture = outcome_fixture("device_flow_disabled");
		expect(
			fixture.value(QLatin1StringView{ "http_status" }).toInt() == 400,
			"the recorded disabled refusal uses status 400");
		const auto outcome = parse_device_code_response(
			response_from(fixture, synthetic_body(fixture)));
		expect(outcome.has_value(), "the recorded disabled shape parses");
		const auto* refusal = std::get_if<device_code_refusal>(&*outcome);
		expect(
			refusal != nullptr
				&& refusal->Kind
					== device_code_refusal_kind::device_flow_disabled,
			"the disabled response is the typed refusal");
	}

	void test_token_endpoint_errors_arrive_with_status_200()
	{
		for (const auto name :
			 { "authorization_pending", "slow_down", "access_denied",
			   "expired_token" }) {
			const auto fixture = outcome_fixture(name);
			expect(
				fixture.value(QLatin1StringView{ "http_status" }).toInt() == 200,
				"the recorded token errors use status 200");
		}
	}

	void test_each_token_outcome_parses_to_its_type()
	{
		const auto parsed = [](const std::string_view aName)
			-> device_token_outcome {
			const auto fixture = outcome_fixture(aName);
			const auto outcome = parse_device_token_response(
				response_from(fixture, synthetic_body(fixture)));
			expect(outcome.has_value(), "each recorded token shape parses");
			return *outcome;
		};

		const auto  pending      = parsed("authorization_pending");
		const auto* pendingDelay = std::get_if<device_token_delay>(&pending);
		expect(
			pendingDelay != nullptr
				&& pendingDelay->Kind
					== device_token_delay_kind::authorization_pending
				&& pendingDelay->SlowDownIntervalSeconds == 0U,
			"pending is a delay without an interval");

		const auto  slow      = parsed("slow_down");
		const auto* slowDelay = std::get_if<device_token_delay>(&slow);
		expect(
			slowDelay != nullptr
				&& slowDelay->Kind == device_token_delay_kind::slow_down
				&& slowDelay->SlowDownIntervalSeconds == 10U,
			"slow_down carries exactly the recorded interval");

		const auto  denied        = parsed("access_denied");
		const auto* deniedRefusal = std::get_if<device_token_refusal>(&denied);
		expect(
			deniedRefusal != nullptr
				&& deniedRefusal->Kind
					== device_token_refusal_kind::access_denied,
			"denial is the typed refusal");

		const auto  expired        = parsed("expired_token");
		const auto* expiredRefusal = std::get_if<device_token_refusal>(&expired);
		expect(
			expiredRefusal != nullptr
				&& expiredRefusal->Kind
					== device_token_refusal_kind::expired_token,
			"expiry is the typed refusal");

		const auto  success      = parsed("token_success");
		const auto* successGrant = std::get_if<device_token_grant>(&success);
		expect(
			successGrant != nullptr
				&& successGrant->AccessToken == synthetic_access_token
				&& successGrant->RefreshToken == synthetic_refresh_token
				&& successGrant->ExpiresInSeconds == 28800U
				&& successGrant->RefreshTokenExpiresInSeconds == 15897600U,
			"token success carries the recorded lifetimes and the tokens");
	}

	[[nodiscard]] auto mutated(
		const QJsonObject&                       aBody,
		const std::function<void(QJsonObject&)>& aMutate) -> QJsonObject
	{
		auto body = aBody;
		aMutate(body);
		return body;
	}

	void test_hostile_device_code_shapes_fail_closed()
	{
		const auto fixture = outcome_fixture("device_code_success");
		const auto body    = synthetic_body(fixture);
		const auto failure
			= [&fixture](
				  const QJsonObject&                          aBody,
				  const device_authorization_contract_failure aExpected,
				  const std::string_view                      aMessage) {
				  const auto outcome = parse_device_code_response(
					  response_from(fixture, aBody));
				  expect(
					  !outcome.has_value() && outcome.error() == aExpected,
					  aMessage);
			  };

		failure(
			mutated(
				body,
				[](QJsonObject& aObject) {
					aObject.insert(
						QStringLiteral("verification_uri_complete"),
						QStringLiteral("https://github.com/login/device"));
				}),
			device_authorization_contract_failure::unexpected_field,
			"an unrecorded extra field is refused");
		failure(
			mutated(
				body,
				[](QJsonObject& aObject) {
					aObject.remove(QStringLiteral("interval"));
				}),
			device_authorization_contract_failure::unexpected_field,
			"a missing recorded field is refused");
		failure(
			mutated(
				body,
				[](QJsonObject& aObject) {
					aObject.insert(
						QStringLiteral("interval"),
						QStringLiteral("5"));
				}),
			device_authorization_contract_failure::invalid_field_value,
			"a string interval is refused");
		failure(
			mutated(
				body,
				[](QJsonObject& aObject) {
					aObject.insert(QStringLiteral("interval"), -5);
				}),
			device_authorization_contract_failure::invalid_field_value,
			"a negative interval is refused");
		failure(
			mutated(
				body,
				[](QJsonObject& aObject) {
					aObject.insert(QStringLiteral("interval"), 5.5);
				}),
			device_authorization_contract_failure::invalid_field_value,
			"a fractional interval is refused");
		failure(
			mutated(
				body,
				[](QJsonObject& aObject) {
					aObject.insert(
						QStringLiteral("verification_uri"),
						QStringLiteral("https://example.com/login/device"));
				}),
			device_authorization_contract_failure::off_authority_verification_uri,
			"an off-authority verification address is refused");
		failure(
			mutated(
				body,
				[](QJsonObject& aObject) {
					aObject.insert(
						QStringLiteral("verification_uri"),
						QStringLiteral("http://github.com/login/device"));
				}),
			device_authorization_contract_failure::off_authority_verification_uri,
			"a non-https verification address is refused");
		failure(
			mutated(
				body,
				[](QJsonObject& aObject) {
					aObject.insert(QStringLiteral("device_code"), QString{ });
				}),
			device_authorization_contract_failure::invalid_field_value,
			"an empty device code is refused");

		auto wrongStatus   = response_from(fixture, body);
		wrongStatus.Status = 500;
		expect(
			!parse_device_code_response(wrongStatus).has_value(),
			"an unexpected status is refused");

		auto wrongType                  = response_from(fixture, body);
		wrongType.Headers.front().Value = "text/html";
		expect(
			!parse_device_code_response(wrongType).has_value(),
			"an unexpected content type is refused");

		auto oversized = response_from(fixture, body);
		oversized.Body.append(
			maximum_device_authorization_response_bytes,
			' ');
		const auto oversizedOutcome = parse_device_code_response(oversized);
		expect(
			!oversizedOutcome.has_value()
				&& oversizedOutcome.error()
					== device_authorization_contract_failure::response_limit,
			"an oversized response is refused before parsing");

		auto nonJson = response_from(fixture, body);
		nonJson.Body = "not json";
		expect(
			!parse_device_code_response(nonJson).has_value(),
			"a non-JSON body is refused");
	}

	void test_hostile_token_shapes_fail_closed()
	{
		const auto success     = outcome_fixture("token_success");
		const auto successBody = synthetic_body(success);
		const auto failure
			= [&success](
				  const QJsonObject&                          aBody,
				  const device_authorization_contract_failure aExpected,
				  const std::string_view                      aMessage) {
				  const auto outcome = parse_device_token_response(
					  response_from(success, aBody));
				  expect(
					  !outcome.has_value() && outcome.error() == aExpected,
					  aMessage);
			  };

		failure(
			mutated(
				successBody,
				[](QJsonObject& aObject) {
					aObject.insert(
						QStringLiteral("token_type"),
						QStringLiteral("Bearer"));
				}),
			device_authorization_contract_failure::invalid_field_value,
			"a non-lowercase token type is refused");
		failure(
			mutated(
				successBody,
				[](QJsonObject& aObject) {
					aObject.insert(
						QStringLiteral("scope"),
						QStringLiteral("repo"));
				}),
			device_authorization_contract_failure::invalid_field_value,
			"a non-empty scope contradicts the registration and is refused");
		failure(
			mutated(
				successBody,
				[](QJsonObject& aObject) {
					aObject.remove(QStringLiteral("refresh_token"));
				}),
			device_authorization_contract_failure::unexpected_field,
			"a success without a refresh token is refused");

		const auto slow     = outcome_fixture("slow_down");
		const auto slowBody = synthetic_body(slow);
		failure(
			mutated(
				slowBody,
				[](QJsonObject& aObject) {
					aObject.remove(QStringLiteral("interval"));
				}),
			device_authorization_contract_failure::missing_field,
			"slow_down without its interval is refused");
		failure(
			mutated(
				slowBody,
				[](QJsonObject& aObject) {
					aObject.insert(
						QStringLiteral("error"),
						QStringLiteral("authorization_pending"));
				}),
			device_authorization_contract_failure::unexpected_field,
			"an interval on a non-slow_down error is refused");
		failure(
			mutated(
				slowBody,
				[](QJsonObject& aObject) {
					aObject.insert(
						QStringLiteral("error_uri"),
						QStringLiteral("https://example.com/errors"));
				}),
			device_authorization_contract_failure::unofficial_error_uri,
			"an unofficial error documentation authority is refused");
		failure(
			mutated(
				slowBody,
				[](QJsonObject& aObject) {
					aObject.insert(
						QStringLiteral("error"),
						QStringLiteral("incorrect_client_credentials"));
					aObject.remove(QStringLiteral("interval"));
				}),
			device_authorization_contract_failure::unknown_error_code,
			"an error outside the recorded closed set is refused");
	}

	// -- refresh: documented, not recorded ---------------------------------

	void test_the_documented_tree_is_disjoint_from_the_recorded_capture()
	{
		const auto documented = std::filesystem::path{
			BPR_GITHUB_DEVICE_DOCUMENTED_FIXTURE_ROOT
		};
		const auto recorded = std::filesystem::path{
			BPR_GITHUB_DEVICE_AUTHORIZATION_FIXTURE_ROOT
		};
		expect(
			documented != recorded,
			"the documented shapes do not live in the recorded capture tree");

		const auto manifest = parse_object(documented / "manifest.json");
		expect(
			manifest.value(QLatin1StringView{ "schema_name" }).toString()
				== QStringLiteral(
					"bpr.rfc0016.device_documented_contract_manifest"),
			"the documented manifest is its own schema");
		expect(
			manifest.value(QLatin1StringView{ "state" }).toString()
				== QStringLiteral("documented_not_recorded"),
			"and its state never claims a capture");
		expect(
			manifest.value(QLatin1StringView{ "recorded" }).isBool()
				&& !manifest.value(QLatin1StringView{ "recorded" }).toBool(),
			"nor does its recorded flag");

		// Every documented artifact is digest-pinned exactly as the recorded
		// ones are, so an edit that changes a shape cannot pass unnoticed.
		const auto artifacts
			= manifest.value(QLatin1StringView{ "artifacts" }).toArray();
		expect(artifacts.size() == 2, "two documented artifacts are pinned");
		for (const auto& entry : artifacts) {
			const auto artifact = entry.toObject();
			const auto relative
				= artifact.value(QLatin1StringView{ "path" }).toString();
			expect(
				artifact.value(QLatin1StringView{ "source_kind" }).toString()
					== QStringLiteral("documented_not_recorded"),
				"every documented artifact is labeled");
			const auto bytes = read_all(
				documented / std::filesystem::path{ relative.toStdString() });
			const auto digest = QCryptographicHash::hash(
				QByteArray::fromStdString(bytes),
				QCryptographicHash::Sha256);
			expect(
				artifact.value(QLatin1StringView{ "sha256" }).toString()
					== QStringLiteral("sha256:")
						+ QString::fromLatin1(digest.toHex()),
				"and its pinned digest matches its bytes");
		}
	}

	void test_the_documented_refresh_success_matches_the_recorded_token_body()
	{
		// The only genuinely unrecorded parts of the renewal are the request's
		// grant type and the refusal code. Its success body is the body the
		// attended sitting already recorded, and this proves that rather than
		// asserting it in a comment.
		const auto recorded   = outcome_fixture("token_success")
									.value(QLatin1StringView{ "projection" })
									.toObject();
		const auto documented = documented_fixture("refresh_success")
									.value(QLatin1StringView{ "projection" })
									.toObject();
		expect(
			documented.value(QLatin1StringView{ "observed_fields" })
				== recorded.value(QLatin1StringView{ "observed_fields" }),
			"the documented renewal body has exactly the recorded fields");
		expect(
			documented.value(QLatin1StringView{ "omitted_transient_fields" })
				== recorded.value(QLatin1StringView{ "omitted_transient_fields" }),
			"omitting exactly the same transient material");
		expect(
			documented.value(QLatin1StringView{ "values" })
				== recorded.value(QLatin1StringView{ "values" }),
			"with the same retained non-secret values");
	}

	void test_the_compiled_refresh_body_is_exact()
	{
		const auto request = compile_device_refresh_request(
			server_contract::github_com,
			synthetic_client_id,
			synthetic_refresh_token);
		expect(request.has_value(), "the refresh request compiles");
		expect(
			request->Body
				== "client_id=Iv23synthetic0000000&grant_type=refresh_token"
				   "&refresh_token=synthetic_refresh_token_708d",
			"three fields, in the documented order, form-encoded");
		expect(
			request->Endpoint == device_authorization_endpoint::token
				&& request->Method == http_method::post,
			"posted to the token endpoint");
		expect(
			request->Url
				== documented_fixture("refresh_success")
					   .value(QLatin1StringView{ "endpoint_url" })
					   .toString()
					   .toStdString(),
			"at the documented endpoint URL");
		expect(
			request->Body.find("device_code") == std::string::npos
				&& request->Body.find("urn:ietf") == std::string::npos,
			"and carries nothing from the device-code grant");
	}

	void test_refresh_inputs_are_refused_before_the_wire()
	{
		expect(
			!compile_device_refresh_request(
				 server_contract::ghes_3_18,
				 synthetic_client_id,
				 synthetic_refresh_token)
				 .has_value(),
			"GHES has no device refresh contract");
		const auto badClient = compile_device_refresh_request(
			server_contract::github_com,
			"bad client id",
			synthetic_refresh_token);
		expect(
			!badClient.has_value()
				&& badClient.error()
					== device_authorization_contract_failure::invalid_client_id,
			"a malformed client id is refused");
		for (const auto refresh : { "", "with space", "semi;colon",
									"newline\ninjected", "amp&ersand" }) {
			const auto outcome = compile_device_refresh_request(
				server_contract::github_com,
				synthetic_client_id,
				refresh);
			expect(
				!outcome.has_value()
					&& outcome.error()
						== device_authorization_contract_failure::
							invalid_refresh_token,
				"refresh material that could not have come from this contract "
				"never reaches the wire");
		}
		const auto oversized = std::string(600U, 'a');
		const auto outcome   = compile_device_refresh_request(
			server_contract::github_com,
			synthetic_client_id,
			oversized);
		expect(
			!outcome.has_value()
				&& outcome.error()
					== device_authorization_contract_failure::
						invalid_refresh_token,
			"and neither does an unbounded one");
	}

	void test_each_refresh_outcome_parses_to_its_type()
	{
		const auto success = documented_fixture("refresh_success");
		const auto issued  = parse_device_refresh_response(
			response_from(success, synthetic_body(success)));
		expect(
			issued.has_value()
				&& std::holds_alternative<device_token_grant>(*issued),
			"the documented renewal success parses to a grant");
		const auto& grant = std::get<device_token_grant>(*issued);
		expect(
			grant.AccessToken == synthetic_access_token
				&& grant.RefreshToken == synthetic_refresh_token,
			"carrying both rotated secrets");
		expect(
			grant.ExpiresInSeconds == 28800U
				&& grant.RefreshTokenExpiresInSeconds == 15897600U,
			"and both documented lifetimes");

		const auto refused = documented_fixture("bad_refresh_token");
		const auto outcome = parse_device_refresh_response(
			response_from(refused, synthetic_body(refused)));
		expect(
			outcome.has_value()
				&& std::holds_alternative<device_refresh_refusal>(*outcome),
			"the documented refusal parses to the typed refusal");
		expect(
			std::get<device_refresh_refusal>(*outcome).Kind
				== device_refresh_refusal_kind::bad_refresh_token,
			"with the one admitted code");
	}

	void test_hostile_refresh_shapes_fail_closed()
	{
		const auto success     = documented_fixture("refresh_success");
		const auto successBody = synthetic_body(success);
		const auto refused     = documented_fixture("bad_refresh_token");
		const auto refusedBody = synthetic_body(refused);
		const auto failure
			= [&success](
				  const QJsonObject&                          aBody,
				  const device_authorization_contract_failure aExpected,
				  const std::string_view                      aMessage) {
				  const auto outcome = parse_device_refresh_response(
					  response_from(success, aBody));
				  expect(
					  !outcome.has_value() && outcome.error() == aExpected,
					  aMessage);
			  };

		failure(
			mutated(
				successBody,
				[](QJsonObject& aObject) {
					aObject.remove(QStringLiteral("refresh_token"));
				}),
			device_authorization_contract_failure::unexpected_field,
			"a renewal that rotates no refresh material is refused");
		failure(
			mutated(
				successBody,
				[](QJsonObject& aObject) {
					aObject.insert(
						QStringLiteral("scope"),
						QStringLiteral("repo"));
				}),
			device_authorization_contract_failure::invalid_field_value,
			"a renewal that silently widens scope is refused");
		failure(
			mutated(
				successBody,
				[](QJsonObject& aObject) {
					aObject.insert(
						QStringLiteral("verification_uri"),
						QStringLiteral("https://github.com/login/device"));
				}),
			device_authorization_contract_failure::unexpected_field,
			"an unrecorded extra field is refused");
		failure(
			mutated(
				successBody,
				[](QJsonObject& aObject) {
					aObject.insert(QStringLiteral("expires_in"), -1);
				}),
			device_authorization_contract_failure::invalid_field_value,
			"a negative lifetime is refused");

		// The renewal has no pending case, so the codes the poll exchange
		// admits must not leak into it. `slow_down` is refused one step
		// earlier, by the envelope rule that a slow-down without its interval
		// is malformed, which is a refusal all the same.
		for (const auto code :
			 { "authorization_pending", "access_denied", "expired_token" }) {
			const auto body = mutated(
				refusedBody,
				[code](QJsonObject& aObject) {
					aObject.insert(
						QStringLiteral("error"),
						QString::fromUtf8(code));
				});
			const auto outcome = parse_device_refresh_response(
				response_from(refused, body));
			expect(
				!outcome.has_value()
					&& outcome.error()
						== device_authorization_contract_failure::
							unknown_error_code,
				"a poll-exchange error code is not a renewal answer");
		}
		failure(
			mutated(
				refusedBody,
				[](QJsonObject& aObject) {
					aObject.insert(
						QStringLiteral("error"),
						QStringLiteral("slow_down"));
				}),
			device_authorization_contract_failure::missing_field,
			"and neither is a slow-down, which the renewal has no use for");

		const auto unofficial = parse_device_refresh_response(response_from(
			refused,
			mutated(refusedBody, [](QJsonObject& aObject) {
				aObject.insert(
					QStringLiteral("error_uri"),
					QStringLiteral("https://evil.invalid/docs"));
			})));
		expect(
			!unofficial.has_value()
				&& unofficial.error()
					== device_authorization_contract_failure::
						unofficial_error_uri,
			"an unofficial error documentation authority is refused");

		const auto oversized = parse_device_refresh_response(wire_response{
			.Status = 200,
			.Headers
			= { { .Name = "Content-Type", .Value = "application/json" } },
			.Body = std::string(
				maximum_device_authorization_response_bytes + 1U,
				'a'),
		});
		expect(
			!oversized.has_value()
				&& oversized.error()
					== device_authorization_contract_failure::response_limit,
			"an oversized renewal body is refused before it is parsed");

		const auto wrongStatus = parse_device_refresh_response(wire_response{
			.Status = 400,
			.Headers
			= { { .Name = "Content-Type", .Value = "application/json" } },
			.Body = "{}",
		});
		expect(
			!wrongStatus.has_value()
				&& wrongStatus.error()
					== device_authorization_contract_failure::unexpected_status,
			"the renewal reports through status 200 like every token answer");
	}
}

auto main() -> int
{
	test_compiled_requests_match_every_recorded_endpoint();
	test_compiled_bodies_are_exact();
	test_ghes_and_invalid_inputs_are_refused();
	test_recorded_registration_client_id_is_well_formed();
	test_device_code_success_parses_to_the_recorded_grant();
	test_device_flow_disabled_parses_to_the_typed_refusal();
	test_token_endpoint_errors_arrive_with_status_200();
	test_each_token_outcome_parses_to_its_type();
	test_hostile_device_code_shapes_fail_closed();
	test_hostile_token_shapes_fail_closed();

	test_the_documented_tree_is_disjoint_from_the_recorded_capture();
	test_the_documented_refresh_success_matches_the_recorded_token_body();
	test_the_compiled_refresh_body_is_exact();
	test_refresh_inputs_are_refused_before_the_wire();
	test_each_refresh_outcome_parses_to_its_type();
	test_hostile_refresh_shapes_fail_closed();

	std::cout << "github device authorization contract tests passed\n";
	return EXIT_SUCCESS;
}
