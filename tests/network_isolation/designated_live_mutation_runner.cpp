#include "designated_live_mutation_gate.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::tests::network_isolation {
	namespace {

		constexpr std::string_view PullEndpoint               = "https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/1";
		constexpr qsizetype        ResponseLimit              = 64 * 1024;
		constexpr int              NetworkTimeoutMilliseconds = 10'000;

		[[nodiscard]] std::optional<QJsonObject> required_object(
			const QJsonObject& aParent,
			const char*        aName)
		{
			const auto value = aParent.value(QLatin1StringView(aName));
			if (!value.isObject()) {
				return std::nullopt;
			}
			return value.toObject();
		}

		[[nodiscard]] std::optional<std::string> required_string(
			const QJsonObject& aParent,
			const char*        aName)
		{
			const auto value = aParent.value(QLatin1StringView(aName));
			if (!value.isString()) {
				return std::nullopt;
			}
			return value.toString().toStdString();
		}

		[[nodiscard]] std::optional<std::uint64_t> required_unsigned(
			const QJsonObject& aParent,
			const char*        aName)
		{
			const auto value = aParent.value(QLatin1StringView(aName));
			if (!value.isDouble()) {
				return std::nullopt;
			}
			const auto integer = value.toInteger(-1);
			if (integer < 0) {
				return std::nullopt;
			}
			return static_cast<std::uint64_t>(integer);
		}

		[[nodiscard]] std::optional<designated_live_identity> parse_identity(
			const QByteArray& aBody)
		{
			QJsonParseError error{ };
			const auto      document = QJsonDocument::fromJson(aBody, &error);
			if (error.error != QJsonParseError::NoError || !document.isObject()) {
				return std::nullopt;
			}

			const auto root = document.object();
			const auto head = required_object(root, "head");
			const auto base = required_object(root, "base");
			if (!head || !base) {
				return std::nullopt;
			}
			const auto repository = required_object(*base, "repo");
			if (!repository) {
				return std::nullopt;
			}
			const auto owner = required_object(*repository, "owner");
			if (!owner) {
				return std::nullopt;
			}

			const auto repositoryId      = required_unsigned(*repository, "id");
			const auto repositoryNodeId  = required_string(*repository, "node_id");
			const auto repositoryName    = required_string(*repository, "name");
			const auto repositoryOwner   = required_string(*owner, "login");
			const auto pullRequestNumber = required_unsigned(root, "number");
			const auto pullRequestId     = required_unsigned(root, "id");
			const auto pullRequestNodeId = required_string(root, "node_id");
			const auto state             = required_string(root, "state");
			const auto headSha           = required_string(*head, "sha");
			const auto headBranch        = required_string(*head, "ref");
			const auto baseSha           = required_string(*base, "sha");
			if (!repositoryId || !repositoryNodeId || !repositoryName || !repositoryOwner || !pullRequestNumber || !pullRequestId || !pullRequestNodeId || !state || !headSha || !headBranch || !baseSha) {
				return std::nullopt;
			}

			return designated_live_identity{
				.Host                     = "github.com",
				.Owner                    = *repositoryOwner,
				.Repository               = *repositoryName,
				.RepositoryDatabaseId     = *repositoryId,
				.RepositoryNodeId         = *repositoryNodeId,
				.PullRequestNumber        = *pullRequestNumber,
				.PullRequestDatabaseId    = *pullRequestId,
				.PullRequestNodeId        = *pullRequestNodeId,
				.BaseRepositoryDatabaseId = *repositoryId,
				.State                    = *state,
				.HeadSha                  = *headSha,
				.BaseSha                  = *baseSha,
				.HeadBranch               = *headBranch,
			};
		}

		class github_designated_identity_reader final : public remote_identity_port
		{
		public:
			[[nodiscard]] remote_read_result read() override
			{
				QNetworkAccessManager manager;
				manager.setProxy(QNetworkProxy::NoProxy);

				QNetworkRequest request{ QUrl(QString::fromLatin1(PullEndpoint.data(), static_cast<qsizetype>(PullEndpoint.size()))) };
				request.setAttribute(
					QNetworkRequest::RedirectPolicyAttribute,
					QNetworkRequest::ManualRedirectPolicy);
				request.setRawHeader("Accept", "application/vnd.github+json");
				request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
				request.setRawHeader("User-Agent", "BetterPullReview-designated-live-gate/1");

				QNetworkReply* reply = manager.get(request);
				reply->setReadBufferSize(ResponseLimit + 1);
				QByteArray body;
				bool       oversize{ };
				bool       timedOut{ };
				QEventLoop loop;
				QTimer     timer;
				timer.setSingleShot(true);
				QObject::connect(reply, &QNetworkReply::readyRead, [&body, &oversize, reply]() {
					body.append(reply->readAll());
					if (body.size() > ResponseLimit) {
						oversize = true;
						reply->abort();
					}
				});
				QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
				QObject::connect(&timer, &QTimer::timeout, [&timedOut, reply]() {
					timedOut = true;
					reply->abort();
				});
				timer.start(NetworkTimeoutMilliseconds);
				loop.exec();
				timer.stop();
				body.append(reply->readAll());

				const auto networkError = reply->error();
				const auto status       = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
				const bool redirected   = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid() || (status >= 300 && status < 400);
				reply->deleteLater();

				if (oversize || body.size() > ResponseLimit) {
					return { .Status = remote_read_status::oversize };
				}
				if (timedOut) {
					return { .Status = remote_read_status::timeout };
				}
				if (networkError != QNetworkReply::NoError) {
					return { .Status = remote_read_status::network_failure };
				}
				if (redirected || status != 200) {
					return { .Status = remote_read_status::http_failure };
				}
				const auto identity = parse_identity(body);
				if (!identity) {
					return { .Status = remote_read_status::malformed };
				}
				return {
					.Status   = remote_read_status::ready,
					.Identity = *identity,
				};
			}
		};

	} // namespace
} // namespace bpr::tests::network_isolation

#if !defined(BPR_DESIGNATED_LIVE_RUNNER_NO_MAIN)
int main(int argc, char* argv[])
{
	std::vector<std::string_view> arguments;
	arguments.reserve(static_cast<std::size_t>(argc));
	for (int index = 0; index < argc; ++index) {
		arguments.emplace_back(argv[index]);
	}
	if (!bpr::tests::network_isolation::exact_runner_arguments(arguments)) {
		std::cerr << "DESIGNATED_LIVE_NO_LAUNCH:invalid_command:child_launch_count=0\n";
		return 2;
	}

	QCoreApplication application(argc, argv);

	const std::filesystem::path approvalPath(argv[2]);
	if (!approvalPath.is_absolute()) {
		std::cerr << "DESIGNATED_LIVE_NO_LAUNCH:non_absolute_path:child_launch_count=0\n";
		return 2;
	}

	bpr::tests::network_isolation::native_approval_artifact          approval(approvalPath);
	bpr::tests::network_isolation::github_designated_identity_reader remote;
	class fixed_child final : public bpr::tests::network_isolation::capability_child_port
	{
	public:
		[[nodiscard]] bpr::tests::network_isolation::child_start_result start(
			std::string_view aCapability,
			std::string_view aExpectedCanarySurfaceSha256) override
		{
			return bpr::tests::network_isolation::start_fixed_canary(
				aCapability,
				aExpectedCanarySurfaceSha256);
		}
	} child;
	const bpr::tests::network_isolation::designated_live_gate gate;
	const auto                                                result = gate.run(approval, remote, child);
	if (result.Status != bpr::tests::network_isolation::live_gate_status::launched) {
		std::cerr << "DESIGNATED_LIVE_NO_LAUNCH:"
				  << bpr::tests::network_isolation::name_of(result.Status)
				  << ":mismatch="
				  << bpr::tests::network_isolation::name_of(result.Mismatch)
				  << ":child_launch_count=" << result.ChildLaunchCount << '\n';
		return 1;
	}

	std::cout << "DESIGNATED_LIVE_CANARY_OK:child_launch_count=1\n";
	return 0;
}
#endif
