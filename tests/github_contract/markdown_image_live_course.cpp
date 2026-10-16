// RFC-BPR-0084. The live image course's own process: never a CTest row.
//
// `--resolution-body OWNER NAME NUMBER NODE...` prints the exact GraphQL body
// the shipped `resolve_body_images` read compiles, so the driver
// (tests/scripts/markdown_image_live_course.py) can send those bytes through
// `gh api graphql`. `--render INPUT OUTPUT_DIRECTORY` reads the driver's JSON
// (each body's Markdown and GitHub's `bodyHTML` for it), and for every image
// the reader asks for runs the shipped path: core admission and served-address
// matching, the shipped credential-free native HTTPS media transport with core
// redirect admission, the Qt decoder, and the reader's projection. It prints
// one line per image (hosts only, never a signed address) and writes each
// body's rendered reader as a PNG. CTest runs only the offline request mode.
#include "github_host_profile.hpp"
#include "github_read_contract.hpp"
#include "native_https_transport.hpp"

#include "bpr/application/github_projection.hpp"
#include "bpr/core/markdown_image.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/markdown_image_decoder.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QTextDocument>
#include <QTextEdit>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::github;
	using namespace bpr::adapters::github::detail;

	auto host_of(const std::string_view aAddress) -> std::string
	{
		const auto start = aAddress.find("://");
		if (start == std::string_view::npos) {
			return "(relative)";
		}
		const auto rest = aAddress.substr(start + 3U);
		return std::string{ rest.substr(0U, rest.find_first_of("/?#")) };
	}

	auto state_name(const core::markdown_image_state aState) -> std::string_view
	{
		switch (aState) {
		case core::markdown_image_state::loading:     return "loading";
		case core::markdown_image_state::loaded:      return "loaded";
		case core::markdown_image_state::failed:      return "failed";
		case core::markdown_image_state::too_large:   return "too_large";
		case core::markdown_image_state::blocked:     return "blocked";
		case core::markdown_image_state::unsupported: return "unsupported";
		case core::markdown_image_state::over_limit:  return "over_limit";
		}
		return "unknown";
	}

	auto resolution_body(const std::vector<std::string_view>& aArguments) -> int
	{
		auto semantic              = semantic_read_request{ };
		semantic.Server            = server_contract::github_com;
		semantic.Operation         = read_operation::resolve_body_images;
		semantic.Owner             = std::string{ aArguments[0] };
		semantic.Repository        = std::string{ aArguments[1] };
		semantic.PullRequestNumber = static_cast<std::uint32_t>(std::stoul(std::string{ aArguments[2] }));
		semantic.Page              = 1U;
		for (std::size_t index = 3; index < aArguments.size(); ++index) {
			semantic.Nodes.emplace_back(aArguments[index]);
		}
		const auto compiled = compile_read_request(semantic);
		if (!compiled || !validate_outbound_read_request(*compiled)) {
			std::cerr << "markdown-image-live:fail:request\n";
			return EXIT_FAILURE;
		}
		std::cout << compiled->Body;
		return EXIT_SUCCESS;
	}

	auto media_profile() -> validated_github_host_profile
	{
		auto profile = validate_github_host_profile({
			.Profile        = core::host_profile_id{ 1 },
			.Host           = core::git_host_id{ 1 },
			.Revision       = application::github_profile_revision{ 1 },
			.Server         = application::github_server_contract::github_com_2022_11_28,
			.Authentication = application::github_authentication_transport::native_https,
			.Proxy          = application::github_proxy_mode::system,
			.Trust          = application::github_trust_mode::system,
			.Provenance     = github_profile_provenance::managed,
			.Hostname       = "github.com",
			.RestRoot       = "https://api.github.com",
			.GraphqlRoot    = "https://api.github.com/graphql",
		});
		if (!profile) {
			std::cerr << "markdown-image-live:fail:profile\n";
			std::exit(EXIT_FAILURE);
		}
		return *profile;
	}

	auto render(const QString& aInput, const QString& aOutput) -> int
	{
		QFile input{ aInput };
		if (!input.open(QIODevice::ReadOnly)) {
			std::cerr << "markdown-image-live:fail:input\n";
			return EXIT_FAILURE;
		}
		const auto document = QJsonDocument::fromJson(input.readAll()).object();
		const auto hostname = document.value(QStringLiteral("hostname")).toString().toStdString();
		const auto width    = document.value(QStringLiteral("width")).toInt(880);
		QDir{ }.mkpath(aOutput);
		auto       transport = make_native_https_media_transport(media_profile());
		const auto decoder   = qt::markdown_image_decoder{ };
		if (!transport) {
			std::cerr << "markdown-image-live:fail:transport\n";
			return EXIT_FAILURE;
		}
		const auto policy = core::markdown_image_fetch_policy{ .Contract = core::markdown_image_host_contract::github_com, .Hostname = hostname, .CredentialAvailable = false };
		auto       failed = false;
		for (const auto& value : document.value(QStringLiteral("bodies")).toArray()) {
			const auto             body     = value.toObject();
			const auto             name     = body.value(QStringLiteral("name")).toString();
			const auto             markdown = body.value(QStringLiteral("markdown")).toString().toStdString();
			const auto             served   = core::served_markdown_images(body.value(QStringLiteral("html")).toString().toStdString());
			qt::markdown_body_view view;
			view.resize(width, 600);
			view.show();
			std::vector<core::markdown_image_reference> asked;
			view.set_image_dispatch([&asked](std::vector<core::markdown_image_reference> aReferences) { asked = std::move(aReferences); });
			view.set_body(markdown, name.toStdString());
			QApplication::processEvents();
			for (const auto& reference : asked) {
				auto projection       = application::markdown_image_projection{ };
				projection.Ordinal    = reference.Ordinal;
				projection.Source     = reference.Source;
				projection.State      = core::markdown_image_state::failed;
				auto       servedHost = std::string{ "-" };
				auto       detail     = std::string{ };
				const auto source     = core::admit_markdown_image_source(reference.Source, hostname);
				const auto address    = source && served ? core::match_served_markdown_image(*source, *served) : std::nullopt;
				if (!source) {
					projection.State = source.error();
				} else if (!address) {
					projection.State = core::markdown_image_state::blocked;
				} else if (const auto hop = core::admit_markdown_image_fetch(*address, policy); !hop) {
					servedHost       = host_of(*address);
					projection.State = hop.error();
				} else {
					servedHost         = host_of(*address);
					auto request       = application::markdown_image_fetch_request{ };
					request.First      = *hop;
					request.Policy     = policy;
					const auto fetched = (*transport)->perform_markdown_image(request, { });
					if (!fetched) {
						projection.State = fetched.error();
					} else if (!fetched->ContentType.starts_with("image/") || fetched->ContentType.starts_with("image/svg")) {
						projection.State = core::markdown_image_state::unsupported;
						detail           = fetched->ContentType;
					} else if (auto image = decoder.decode(fetched->Bytes)) {
						detail           = std::to_string(image->Width) + "x" + std::to_string(image->Height) + " frames=" + std::to_string(image->Frames.size()) + " bytes=" + std::to_string(fetched->Bytes.size());
						projection.State = core::markdown_image_state::loaded;
						projection.Image = std::make_shared<const application::markdown_decoded_image>(std::move(*image));
					} else {
						projection.State = image.error();
					}
				}
				std::cout << "markdown-image-live:" << name.toStdString() << ':' << reference.Ordinal << ':' << host_of(reference.Source) << ':' << servedHost << ':'
						  << state_name(projection.State) << (detail.empty() ? std::string{ } : ":" + detail) << '\n';
				view.apply_image(projection);
			}
			QApplication::processEvents();
			auto* const reader = view.rendered_view();
			const auto  height = static_cast<int>(reader->document()->size().height()) + 64;
			view.resize(width, std::min(height, 8000));
			QApplication::processEvents();
			if (!view.grab().save(aOutput + QLatin1Char{ '/' } + name + QStringLiteral(".png"))) {
				failed = true;
			}
		}
		std::cout << (failed ? "markdown-image-live:fail:capture\n" : "markdown-image-live:done\n");
		return failed ? EXIT_FAILURE : EXIT_SUCCESS;
	}
} // namespace

int main(int aArgc, char** aArgv)
{
	std::vector<std::string_view> arguments{ aArgv + 1, aArgv + aArgc };
	if (arguments.size() >= 5U && arguments[0] == "--resolution-body") {
		return resolution_body({ arguments.begin() + 1, arguments.end() });
	}
	if (arguments.size() == 3U && arguments[0] == "--render") {
		QApplication application{ aArgc, aArgv };
		return render(QString::fromUtf8(arguments[1].data(), static_cast<qsizetype>(arguments[1].size())), QString::fromUtf8(arguments[2].data(), static_cast<qsizetype>(arguments[2].size())));
	}
	std::cerr << "usage: --resolution-body OWNER NAME NUMBER NODE... | --render INPUT OUTPUT_DIRECTORY\n";
	return EXIT_FAILURE;
}
