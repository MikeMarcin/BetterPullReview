#include "designated_live_identity_json.hpp"
#include "designated_live_mutation_gate.hpp"

#include <QCoreApplication>
#include <QEventLoop>
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

		constexpr std::string_view PullEndpoint               = "https://api.github.com/repos/MikeMarcin/BPR-Test/pulls/2";
		constexpr qsizetype        ResponseLimit              = 64 * 1024;
		constexpr int              NetworkTimeoutMilliseconds = 10'000;

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
				const auto identity = parse_designated_live_identity(std::string_view{ body.constData(), static_cast<std::size_t>(body.size()) });
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
