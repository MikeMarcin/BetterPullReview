#include "github_release_update.hpp"
#include "github_release_update_contract.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QUrl>

#include <cmath>

namespace bpr::adapters::github::detail {
	namespace {
		using reason = core::release_update_reason;
		auto positive_integer(const QJsonValue& aValue) -> std::optional<std::uint64_t>
		{
			const auto number = aValue.toDouble(-1);
			if (!aValue.isDouble() || number < 1 || number > 9'007'199'254'740'991.0 || std::floor(number) != number) {
				return { };
			}
			return static_cast<std::uint64_t>(number);
		}
		class release_access final : public native_release_update_access
		{
		public:
			release_access(std::optional<application::github_credential_material> aCredential, core::release_update_access_id aIdentity,
						   std::shared_ptr<application::release_update_archive_store> aStore)
				: Credential{ std::move(aCredential) }
				, Identity{ aIdentity }
				, Store{ std::move(aStore) }
			{ }
			void revoke() noexcept override { Revocation.request_stop(); }
			auto binding() const noexcept -> core::release_update_access_id override { return Revocation.stop_requested() ? core::release_update_access_id{ } : Identity; }
			auto kind() const noexcept -> core::release_update_access_kind override { return core::release_update_access_kind::native_github_com; }
			auto check(core::release_update_channel aChannel, std::stop_token aStop) -> application::release_update_check_result override
			{
				std::stop_source   stop;
				std::stop_callback caller{ aStop, [&stop] { stop.request_stop(); } };
				std::stop_callback revoked{ Revocation.get_token(), [&stop] { stop.request_stop(); } };
				std::string        body;
				auto               result = perform_release_update_https(credential(), 0, application::release_update_metadata_limit(aChannel), stop.get_token(), [&body](std::span<const std::byte> aBytes) {
															   body.append(reinterpret_cast<const char*>(aBytes.data()), aBytes.size());
															   return true; }, aChannel);
				if (result.Failure != reason::none) {
					return { result.Failure, { }, result.RetryAfterSeconds };
				}
				return parse_release_update_metadata(body, aChannel);
			}
			auto download(const core::release_update_candidate& aCandidate, std::stop_token aStop,
						  const std::function<void(std::uint64_t)>& aProgress) -> application::release_update_download_result override
			{
				if (!Store || aCandidate.Asset.value() == 0U || aCandidate.Size == 0U || aCandidate.Size > core::release_update_maximum_archive_bytes) {
					return { reason::unavailable };
				}
				std::stop_source   stop;
				std::stop_callback caller{ aStop, [&stop] { stop.request_stop(); } };
				std::stop_callback revoked{ Revocation.get_token(), [&stop] { stop.request_stop(); } };
				auto               writer = Store->begin(aCandidate);
				if (!writer) {
					return { reason::unavailable };
				}
				std::uint64_t bytes  = 0;
				const auto    result = perform_release_update_https(credential(), aCandidate.Asset.value(), aCandidate.Size,
																	stop.get_token(), [&](std::span<const std::byte> aBytes) {
																	 if (!writer->append(aBytes)) {
																		 return false;
																	 }
																	 bytes += aBytes.size();
																	 aProgress(bytes);
																	 return true;
																	});
				if (result.Failure != reason::none) {
					return { result.Failure, { }, bytes, result.RetryAfterSeconds };
				}
				if (bytes != aCandidate.Size) {
					return { reason::size_limit };
				}
				if (stop.stop_requested()) {
					return { reason::cancelled };
				}
				auto archive = writer->finish();
				return { archive ? reason::none : reason::unavailable, std::move(archive), bytes };
			}

		private:
			auto                                                       credential() const noexcept -> std::span<const std::byte> { return Credential ? Credential->bytes() : std::span<const std::byte>{ }; }
			std::optional<application::github_credential_material>     Credential;
			core::release_update_access_id                             Identity;
			std::shared_ptr<application::release_update_archive_store> Store;
			std::stop_source                                           Revocation;
		};
	}
	auto make_native_release_update_access(application::github_credential_retrieve_success aCredential,
										   core::release_update_access_id aIdentity, std::shared_ptr<application::release_update_archive_store> aStore)
		-> std::shared_ptr<native_release_update_access>
	{
		if (!application::valid(aCredential) || aIdentity.value() == 0U
			|| aCredential.Context.Authority.ServerContract != application::github_server_contract::github_com_2022_11_28
			|| aCredential.Context.Authority.Hostname.value() != "github.com") {
			return { };
		}
		return std::make_shared<release_access>(std::move(aCredential.Credential), aIdentity, std::move(aStore));
	}
	auto make_public_release_update_access(core::release_update_access_id aIdentity, std::shared_ptr<application::release_update_archive_store> aStore)
		-> std::shared_ptr<native_release_update_access>
	{
		if (aIdentity.value() == 0U) {
			return { };
		}
		// Releases are public: the pinned routes need no sign-in, so a
		// GHE-only profile still updates ([[BPR-1669]]).
		return std::make_shared<release_access>(std::nullopt, aIdentity, std::move(aStore));
	}
	namespace {
		auto parse_release(const QJsonObject& aObject) -> std::expected<application::release_update_listing, reason>
		{
			const auto id         = positive_integer(aObject.value(QStringLiteral("id")));
			const auto tag        = aObject.value(QStringLiteral("tag_name"));
			const auto draft      = aObject.value(QStringLiteral("draft"));
			const auto prerelease = aObject.value(QStringLiteral("prerelease"));
			const auto assets     = aObject.value(QStringLiteral("assets"));
			if (!id || !tag.isString() || !draft.isBool() || !prerelease.isBool() || !assets.isArray()) {
				return std::unexpected(reason::malformed_release);
			}
			application::release_update_listing result;
			result.Release.Source     = core::release_update_source::better_pull_review_github_com;
			result.Release.Id         = core::release_update_release_id{ *id };
			result.Release.Tag        = tag.toString().toStdString();
			result.Release.Draft      = draft.toBool();
			result.Release.Prerelease = prerelease.toBool();
			if (assets.toArray().size() > static_cast<qsizetype>(core::release_update_maximum_assets)) {
				return std::unexpected(reason::size_limit);
			}
			for (const auto& asset : assets.toArray()) {
				if (!asset.isObject()) {
					return std::unexpected(reason::malformed_release);
				}
				const auto record  = asset.toObject();
				const auto assetId = positive_integer(record.value(QStringLiteral("id")));
				const auto size    = positive_integer(record.value(QStringLiteral("size")));
				const auto name    = record.value(QStringLiteral("name"));
				const auto state   = record.value(QStringLiteral("state"));
				const auto digest  = record.value(QStringLiteral("digest"));
				if (!assetId || !size || !name.isString() || !state.isString()
					|| (!digest.isUndefined() && !digest.isNull() && !digest.isString()) || name.toString().size() > 256) {
					return std::unexpected(reason::malformed_release);
				}
				result.Release.Assets.push_back({ core::release_update_asset_id{ *assetId }, name.toString().toStdString(), *size,
												  digest.isString() ? std::optional{ digest.toString().toStdString() } : std::nullopt, state.toString() == QStringLiteral("uploaded") });
			}
			const auto body = aObject.value(QStringLiteral("body"));
			if (!body.isUndefined() && !body.isNull() && !body.isString()) {
				return std::unexpected(reason::malformed_release);
			}
			auto notes = body.toString().toUtf8();
			if (notes.size() > static_cast<qsizetype>(application::release_update_maximum_notes_bytes)) {
				notes.truncate(static_cast<qsizetype>(application::release_update_maximum_notes_bytes));
				while (!notes.isEmpty() && (static_cast<unsigned char>(notes.back()) & 0xc0U) == 0x80U) {
					notes.chop(1);
				}
				if (!notes.isEmpty() && static_cast<unsigned char>(notes.back()) >= 0xc0U) {
					notes.chop(1);
				}
			}
			result.Notes = notes.toStdString();
			return result;
		}
	}

	auto parse_release_update_metadata(std::string_view aBody, core::release_update_channel aChannel) -> application::release_update_check_result
	{
		if (aBody.size() > application::release_update_metadata_limit(aChannel)) {
			return { reason::size_limit };
		}
		QJsonParseError error;
		const auto      document = QJsonDocument::fromJson(QByteArray{ aBody.data(), static_cast<qsizetype>(aBody.size()) }, &error);
		const bool      listing  = aChannel == core::release_update_channel::include_prereleases;
		if (error.error != QJsonParseError::NoError || (listing ? !document.isArray() : !document.isObject())) {
			return { reason::malformed_release };
		}
		if (!listing) {
			auto release = parse_release(document.object());
			return release ? application::release_update_check_result{ reason::none, { std::move(*release) } }
						   : application::release_update_check_result{ release.error() };
		}
		const auto records = document.array();
		if (records.size() > static_cast<qsizetype>(core::release_update_maximum_releases)) {
			return { reason::size_limit };
		}
		application::release_update_check_result result;
		for (const auto& record : records) {
			if (auto release = parse_release(record.toObject())) {
				result.Releases.push_back(std::move(*release));
			}
		}
		if (!records.isEmpty() && result.Releases.empty()) {
			return { reason::malformed_release };
		}
		return result;
	}
	auto release_update_browser_address(std::string_view aTag) -> std::string
	{
		const std::string base = "https://github.com/MikeMarcin/BetterPullReview/releases";
		return (core::parse_release_update_version(aTag) || core::parse_release_update_prerelease_tag(aTag)) ? base + "/tag/" + QUrl::toPercentEncoding(QString::fromUtf8(aTag.data(), static_cast<qsizetype>(aTag.size()))).toStdString() : base;
	}
}
