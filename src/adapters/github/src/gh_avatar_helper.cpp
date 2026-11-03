/// \file
/// \brief [[BPR-1846]]. The GitHub CLI avatar helper: a small shipped program
/// that fetches a batch of GHES avatars API addresses with `gh`'s token, so
/// the token never enters the application.
///
/// \details The application launches it through its gh process runner, with
/// the gh child environment and private account configuration it gives `gh`
/// itself, and the arguments `--gh <gh> --hostname <host> --user <login>`. It
/// reads a request batch on standard input (`core::decode_gh_cli_avatar_requests`),
/// admits each address with `core::admit_gh_cli_avatar_address`, runs
/// `gh auth token --hostname <host> --user <login>` once, fetches every
/// admitted address with `Authorization: token <token>` on the hops the core
/// marks, follows only redirects `core::admit_markdown_image_redirect` admits,
/// and writes `core::encode_gh_cli_avatar_batch` on standard output. It writes
/// nothing on standard error, and the token goes nowhere but those request
/// headers: not into an argument, the environment, a file or its output.
///
/// It links the core, Qt Core and Qt Network and nothing else.

#include "bpr/core/gh_cli_avatar.hpp"
#include "bpr/core/identity_avatar_picture.hpp"
#include "bpr/core/markdown_image.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QtGlobal>

#if defined(BPR_GH_AVATAR_HELPER_TESTING)
	#include <QFile>
	#include <QNetworkProxy>
	#include <QSslCertificate>
	#include <QSslConfiguration>
#endif

#if defined(_WIN32)
	#include "bpr/platform/windows_standard_handles.hpp"

	#include <fcntl.h>
	#include <io.h>
#else
	#include "bpr/platform/unix_descriptor_sweep.hpp"
#endif

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	namespace core = bpr::core;

	/// The longest request batch: its protocol line and every address line.
	constexpr std::size_t request_bytes = core::gh_cli_avatar_protocol.size() + 1U + core::gh_cli_avatar_batch_limit * (core::gh_cli_avatar_address_bytes + 1U);

	/// The token, held only here and wiped when the helper is done.
	class held_token final
	{
	public:
		held_token()                                     = default;
		held_token(const held_token&)                    = delete;
		auto operator=(const held_token&) -> held_token& = delete;
		held_token(held_token&&)                         = delete;
		auto operator=(held_token&&) -> held_token&      = delete;
		~held_token() { wipe(); }

		void wipe() noexcept
		{
			std::fill(Value.begin(), Value.end(), '\0');
			Value.clear();
		}

		QByteArray Value;
	};

	/// The pinned `gh` account, as the application admits one ([[BPR-1373]]):
	/// 1 to 64 ASCII letters, digits, `-` or `_`, starting with a letter or digit.
	[[nodiscard]] auto login_argument(const std::string_view aLogin) -> bool
	{
		const auto alphanumeric = [](const char aChar) { return (aChar >= 'a' && aChar <= 'z') || (aChar >= 'A' && aChar <= 'Z') || (aChar >= '0' && aChar <= '9'); };
		return !aLogin.empty() && aLogin.size() <= 64U && alphanumeric(aLogin.front())
			&& std::ranges::all_of(aLogin, [&alphanumeric](const char aChar) { return alphanumeric(aChar) || aChar == '-' || aChar == '_'; });
	}

	[[nodiscard]] auto read_standard_input() -> std::optional<std::string>
	{
		std::string text;
		char        block[4096];
		for (;;) {
			const auto count = std::fread(block, 1U, sizeof(block), stdin);
			if (count != 0U) {
				if (text.size() + count > request_bytes) {
					return std::nullopt;
				}
				text.append(block, count);
			}
			if (count < sizeof(block)) {
				break;
			}
		}
		if (std::ferror(stdin) != 0) {
			return std::nullopt;
		}
		return text;
	}

	auto write_standard_output(const core::gh_cli_avatar_batch& aBatch) -> int
	{
		const auto text = core::encode_gh_cli_avatar_batch(aBatch);
		if (!text) {
			return 2;
		}
		const auto written = std::fwrite(text->data(), 1U, text->size(), stdout);
		return written == text->size() && std::fflush(stdout) == 0 ? 0 : 2;
	}

	/// Runs `gh auth token` for the pinned host and account, in this process's
	/// environment, which is the gh child environment the application built.
	[[nodiscard]] auto acquire_token(const QString& aGh, const QString& aHostname, const QString& aLogin, held_token& aToken) -> core::gh_cli_avatar_batch_state
	{
		QProcess process;
		process.setProgram(aGh);
		process.setArguments({ QStringLiteral("auth"), QStringLiteral("token"), QStringLiteral("--hostname"), aHostname, QStringLiteral("--user"), aLogin });
		process.setStandardInputFile(QProcess::nullDevice());
		process.setStandardErrorFile(QProcess::nullDevice());
		process.setProcessChannelMode(QProcess::SeparateChannels);
#if defined(_WIN32)
		// [[BPR-1115]]. gh inherits its three standard handles and nothing else.
		bpr::platform::windows_standard_handles::launch_restriction inheritance;
		process.setCreateProcessArgumentsModifier([&inheritance](QProcess::CreateProcessArguments* const aArguments) {
			inheritance.apply(aArguments->startupInfo, aArguments->flags, aArguments->inheritHandles);
		});
#else
		// gh stays in this helper's process group, which the application's
		// runner kills when the helper ends.
		process.setChildProcessModifier([] { bpr::platform::unix_descriptor_sweep::bound(); });
		process.setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors | QProcess::UnixProcessFlag::DisableCoreDumps
										 | QProcess::UnixProcessFlag::ResetSignalHandlers);
#endif
		process.start();
		if (!process.waitForStarted(static_cast<int>(core::gh_cli_avatar_token_deadline_ms))) {
			return core::gh_cli_avatar_batch_state::cli_unavailable;
		}
#if defined(_WIN32)
		if (!inheritance.restricted()) {
			process.kill();
			process.waitForFinished(1000);
			return core::gh_cli_avatar_batch_state::cli_unavailable;
		}
#endif
		QByteArray output;
		QTimer     deadline;
		deadline.setSingleShot(true);
		deadline.start(static_cast<int>(core::gh_cli_avatar_token_deadline_ms));
		auto overflow = false;
		while (process.state() != QProcess::NotRunning && deadline.remainingTime() > 0) {
			process.waitForReadyRead(std::max(1, std::min(100, deadline.remainingTime())));
			output += process.readAllStandardOutput();
			if (static_cast<std::size_t>(output.size()) > core::gh_cli_avatar_token_bytes) {
				overflow = true;
				break;
			}
		}
		if (process.state() != QProcess::NotRunning) {
			process.kill();
			process.waitForFinished(1000);
			output.fill('\0');
			return core::gh_cli_avatar_batch_state::cli_unavailable;
		}
		output += process.readAllStandardOutput();
		if (overflow || process.exitStatus() != QProcess::NormalExit) {
			output.fill('\0');
			return core::gh_cli_avatar_batch_state::cli_unavailable;
		}
		if (process.exitCode() != 0) {
			output.fill('\0');
			return core::gh_cli_avatar_batch_state::cli_signed_out;
		}
		while (!output.isEmpty() && (output.back() == '\n' || output.back() == '\r')) {
			output.back() = '\0';
			output.chop(1);
		}
		const auto usable = !output.isEmpty() && static_cast<std::size_t>(output.size()) <= core::gh_cli_avatar_token_bytes
			&& std::ranges::all_of(output, [](const char aChar) { return aChar > ' ' && aChar < '\x7f'; });
		if (!usable) {
			output.fill('\0');
			return output.isEmpty() ? core::gh_cli_avatar_batch_state::cli_signed_out : core::gh_cli_avatar_batch_state::cli_unavailable;
		}
		aToken.Value = output;
		output.fill('\0');
		return core::gh_cli_avatar_batch_state::answered;
	}

	/// The media type alone, lower case, as one printable word.
	[[nodiscard]] auto media_type(const QByteArray& aHeader) -> std::string
	{
		auto text = aHeader.left(aHeader.indexOf(';') < 0 ? aHeader.size() : aHeader.indexOf(';')).trimmed().toLower().toStdString();
		if (text.size() > core::gh_cli_avatar_content_type_bytes || !std::ranges::all_of(text, [](const char aChar) { return aChar > ' ' && aChar < '\x7f'; })) {
			return { };
		}
		return text;
	}

	/// One address's hops, from its admitted first hop to its answer.
	class avatar_fetch final : public QObject
	{
	public:
		avatar_fetch(QNetworkAccessManager& aManager, const held_token& aToken, core::markdown_image_fetch_policy aPolicy, core::gh_cli_avatar_answer& aAnswer)
			: Manager{ aManager }
			, Token{ aToken }
			, Policy{ std::move(aPolicy) }
			, Answer{ aAnswer }
		{
			Deadline.setSingleShot(true);
			QObject::connect(&Deadline, &QTimer::timeout, this, [this] { end(core::markdown_image_state::failed, 0U); });
		}

		void start(core::markdown_image_hop aHop)
		{
			Deadline.start(static_cast<int>(core::identity_avatar_fetch_deadline_ms));
			send(std::move(aHop));
		}

		[[nodiscard]] auto done() const noexcept -> bool { return Done; }

		std::function<void()> Finished;

	private:
		void send(core::markdown_image_hop aHop)
		{
			Hop = std::move(aHop);
			Body.clear();
			auto request = QNetworkRequest{ QUrl::fromEncoded(QByteArray::fromStdString(Hop.Address), QUrl::StrictMode) };
			request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
			request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
			request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
			request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
			request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
			request.setRawHeader("Accept", "image/*");
			request.setRawHeader("Accept-Encoding", "identity");
			request.setRawHeader("User-Agent", "BetterPullReview");
			if (Hop.Credential && !Token.Value.isEmpty()) {
				auto authorization = QByteArray{ "token " } + Token.Value;
				request.setRawHeader("Authorization", authorization);
				authorization.fill('\0');
			}
			Reply = Manager.get(request);
			QObject::connect(Reply, &QNetworkReply::metaDataChanged, this, [this] {
				const auto length = Reply->header(QNetworkRequest::ContentLengthHeader);
				if (length.isValid() && length.toULongLong() > core::identity_avatar_encoded_bytes) {
					end(core::markdown_image_state::too_large, status());
				}
			});
			QObject::connect(Reply, &QNetworkReply::readyRead, this, [this] {
				Body += Reply->readAll();
				if (static_cast<std::size_t>(Body.size()) > core::identity_avatar_encoded_bytes) {
					end(core::markdown_image_state::too_large, status());
				}
			});
			QObject::connect(Reply, &QNetworkReply::finished, this, [this] { finished(); });
		}

		[[nodiscard]] auto status() const -> std::uint16_t
		{
			const auto value = Reply == nullptr ? 0 : Reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
			return value > 0 && value <= 999 ? static_cast<std::uint16_t>(value) : std::uint16_t{ 0U };
		}

		void finished()
		{
			if (Done) {
				return;
			}
			Body += Reply->readAll();
			const auto code = status();
			if (code >= 300U && code < 400U && Reply->hasRawHeader("Location")) {
				const auto location = Reply->rawHeader("Location").toStdString();
				auto       next     = core::admit_markdown_image_redirect(Hop, location, Policy);
				release();
				if (!next) {
					end(next.error(), code);
					return;
				}
				send(std::move(*next));
				return;
			}
			if (Reply->error() != QNetworkReply::NoError && code == 0U) {
				end(core::markdown_image_state::failed, 0U);
				return;
			}
			if (code != 200U || Body.isEmpty()) {
				end(core::markdown_image_state::failed, code);
				return;
			}
			if (static_cast<std::size_t>(Body.size()) > core::identity_avatar_encoded_bytes) {
				end(core::markdown_image_state::too_large, code);
				return;
			}
			Answer.ContentType = media_type(Reply->rawHeader("Content-Type"));
			Answer.Bytes       = Body.toStdString();
			end(core::markdown_image_state::loaded, code);
		}

		void release()
		{
			if (Reply != nullptr) {
				QObject::disconnect(Reply, nullptr, this, nullptr);
				Reply->abort();
				Reply->deleteLater();
				Reply = nullptr;
			}
		}

		void end(const core::markdown_image_state aState, const std::uint16_t aStatus)
		{
			if (Done) {
				return;
			}
			Done = true;
			Deadline.stop();
			release();
			Answer.State      = aState;
			Answer.HttpStatus = aStatus;
			if (aState != core::markdown_image_state::loaded) {
				Answer.ContentType.clear();
				Answer.Bytes.clear();
			}
			Body.clear();
			if (Finished) {
				Finished();
			}
		}

		QNetworkAccessManager&            Manager;
		const held_token&                 Token;
		core::markdown_image_fetch_policy Policy;
		core::gh_cli_avatar_answer&       Answer;
		core::markdown_image_hop          Hop;
		QNetworkReply*                    Reply = nullptr;
		QByteArray                        Body;
		QTimer                            Deadline;
		bool                              Done = false;
	};

#if defined(BPR_GH_AVATAR_HELPER_TESTING)
	/// Test builds only: a loopback CONNECT proxy and a disposable authority,
	/// named in `bpr_gh_avatar_helper_test.conf` beside this executable, since
	/// the application gives the helper no other channel.
	void configure_for_test(QNetworkAccessManager& aManager)
	{
		QFile file{ QCoreApplication::applicationDirPath() + QStringLiteral("/bpr_gh_avatar_helper_test.conf") };
		if (!file.open(QIODevice::ReadOnly)) {
			return;
		}
		for (const auto& line : file.readAll().split('\n')) {
			if (line.startsWith("proxy_port=")) {
				aManager.setProxy(QNetworkProxy{ QNetworkProxy::HttpProxy, QStringLiteral("127.0.0.1"), static_cast<quint16>(line.mid(11).toUShort()) });
			} else if (line.startsWith("ca=")) {
				auto configuration = QSslConfiguration::defaultConfiguration();
				configuration.addCaCertificates(QSslCertificate::fromPath(QString::fromUtf8(line.mid(3))));
				QSslConfiguration::setDefaultConfiguration(configuration);
			}
		}
	}
#endif

	auto run(const QStringList& aArguments) -> int
	{
		if (aArguments.size() != 7 || aArguments[1] != QStringLiteral("--gh") || aArguments[3] != QStringLiteral("--hostname") || aArguments[5] != QStringLiteral("--user")) {
			return write_standard_output({ .State = core::gh_cli_avatar_batch_state::refused, .Answers = { } });
		}
		const auto& gh       = aArguments[2];
		const auto  hostname = aArguments[4].toStdString();
		const auto  login    = aArguments[6].toStdString();
		if (gh.isEmpty() || !QDir::isAbsolutePath(gh) || !core::gh_cli_avatar_hostname(hostname) || !login_argument(login)) {
			return write_standard_output({ .State = core::gh_cli_avatar_batch_state::refused, .Answers = { } });
		}
		const auto input     = read_standard_input();
		const auto addresses = input ? core::decode_gh_cli_avatar_requests(*input) : std::nullopt;
		if (!addresses) {
			return write_standard_output({ .State = core::gh_cli_avatar_batch_state::refused, .Answers = { } });
		}

		auto batch  = core::gh_cli_avatar_batch{ };
		batch.State = core::gh_cli_avatar_batch_state::answered;
		batch.Answers.resize(addresses->size());
		std::vector<std::optional<core::markdown_image_hop>> hops;
		for (std::size_t index = 0U; index < addresses->size(); ++index) {
			auto hop = core::admit_gh_cli_avatar_address((*addresses)[index], hostname);
			if (!hop) {
				batch.Answers[index].State = hop.error();
				hops.emplace_back();
				continue;
			}
			hops.emplace_back(std::move(*hop));
		}
		if (std::ranges::none_of(hops, [](const auto& aHop) { return aHop.has_value(); })) {
			return write_standard_output(batch);
		}

		held_token token;
		const auto acquired = acquire_token(gh, aArguments[4], aArguments[6], token);
		if (acquired != core::gh_cli_avatar_batch_state::answered) {
			return write_standard_output({ .State = acquired, .Answers = { } });
		}

		QNetworkAccessManager manager;
		QNetworkProxyFactory::setUseSystemConfiguration(true);
#if defined(BPR_GH_AVATAR_HELPER_TESTING)
		configure_for_test(manager);
#endif
		QEventLoop                                 loop;
		std::vector<std::unique_ptr<avatar_fetch>> fetches;
		const auto                                 pending = [&fetches] {
			return std::ranges::any_of(fetches, [](const auto& aFetch) { return !aFetch->done(); });
		};
		for (std::size_t index = 0U; index < hops.size(); ++index) {
			if (!hops[index]) {
				continue;
			}
			auto fetch      = std::make_unique<avatar_fetch>(manager, token, core::gh_cli_avatar_policy(hostname), batch.Answers[index]);
			fetch->Finished = [&loop, &pending] {
				if (!pending()) {
					loop.quit();
				}
			};
			fetches.push_back(std::move(fetch));
		}
		for (std::size_t index = 0U, fetch = 0U; index < hops.size(); ++index) {
			if (hops[index]) {
				fetches[fetch++]->start(std::move(*hops[index]));
			}
		}
		QTimer launch;
		launch.setSingleShot(true);
		QObject::connect(&launch, &QTimer::timeout, &loop, &QEventLoop::quit);
		launch.start(static_cast<int>(core::gh_cli_avatar_launch_deadline_ms));
		if (pending()) {
			loop.exec();
		}
		token.wipe();
		for (std::size_t index = 0U, fetch = 0U; index < hops.size(); ++index) {
			if (hops[index] && !fetches[fetch++]->done()) {
				batch.Answers[index] = { };
			}
		}
		fetches.clear();
		return write_standard_output(batch);
	}

	/// The helper says nothing on standard error: a Qt warning there would make
	/// the application's runner refuse the whole launch.
	void silent(QtMsgType, const QMessageLogContext&, const QString&) { }
} // namespace

auto main(int aArgc, char** aArgv) -> int
{
#if defined(_WIN32)
	static_cast<void>(::_setmode(::_fileno(stdin), _O_BINARY));
	static_cast<void>(::_setmode(::_fileno(stdout), _O_BINARY));
#endif
	qInstallMessageHandler(silent);
	try {
		const QCoreApplication application{ aArgc, aArgv };
		return run(QCoreApplication::arguments());
	} catch (...) {
		return 2;
	}
}
