#include "about_desktop.hpp"

#include "bpr/application/reference_page.hpp"
#include "bpr_build_identity.hpp"
#include "system_browser_opener.hpp"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QString>
#include <QSysInfo>
#include <QUrl>
#include <QtVersion>

#include <system_error>
#include <utility>

#if !defined(BPR_ABOUT_VERSION) || !defined(BPR_ABOUT_CONFIGURATION)
	#error "src/app/CMakeLists.txt gives this file BPR_ABOUT_VERSION and BPR_ABOUT_CONFIGURATION"
#endif

namespace bpr::app {
	namespace {
		[[nodiscard]] auto document_name(const qt::about_document aDocument) -> const char*
		{
			switch (aDocument) {
			case qt::about_document::third_party_notices: return "THIRD-PARTY-NOTICES.txt";
			case qt::about_document::source_offer:        return "SOURCE-OFFERS.txt";
			}
			return "";
		}

		/// A regular file at the path itself, not a link to one.
		[[nodiscard]] auto regular_file(const std::filesystem::path& aPath) -> bool
		{
			auto error = std::error_code{ };
			return std::filesystem::is_regular_file(std::filesystem::symlink_status(aPath, error)) && !error;
		}

		[[nodiscard]] auto native_text(const std::filesystem::path& aPath) -> QString
		{
			return QDir::toNativeSeparators(QString::fromStdU16String(aPath.u16string()));
		}
	}

	auto installed_legal_folder() -> std::filesystem::path
	{
		const auto executable = std::filesystem::path{ QCoreApplication::applicationDirPath().toStdU16String() };
#if defined(Q_OS_MACOS)
		return (executable / ".." / "Resources" / "ThirdParty").lexically_normal();
#else
		return executable / "ThirdParty";
#endif
	}

	auto about_facts_of_this_build(const std::filesystem::path& aLegalFolder) -> qt::about_facts
	{
		auto facts                 = qt::about_facts{ };
		facts.Version              = QStringLiteral(BPR_ABOUT_VERSION);
		facts.Commit               = QStringLiteral(BPR_BUILD_COMMIT);
		facts.CommitDate           = QStringLiteral(BPR_BUILD_COMMIT_DATE);
		facts.LocalChanges         = BPR_BUILD_LOCAL_CHANGES != 0;
		facts.Configuration        = QStringLiteral(BPR_ABOUT_CONFIGURATION);
		facts.QtVersion            = QString::fromLatin1(qVersion());
		facts.System               = QStringLiteral("%1 (%2)").arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());
		facts.Copyright            = QStringLiteral("© 2026 BetterPullReview contributors.");
		facts.NoticesInstalled     = regular_file(aLegalFolder / document_name(qt::about_document::third_party_notices));
		facts.SourceOfferInstalled = regular_file(aLegalFolder / document_name(qt::about_document::source_offer));
		if (facts.NoticesInstalled || facts.SourceOfferInstalled) {
			facts.LegalFolder = native_text(aLegalFolder);
		}
		return facts;
	}

	auto make_about_menu() -> std::unique_ptr<qt::about_menu>
	{
		auto folder      = installed_legal_folder();
		auto facts       = about_facts_of_this_build(folder);
		auto actions     = qt::about_actions{ };
		actions.OpenPage = [](const core::reference_page aPage) {
			auto opener = system_browser_opener{ };
			return application::open_reference_page(aPage, &opener);
		};
		actions.OpenDocument = [legal = std::move(folder)](const qt::about_document aDocument) {
			const auto path = legal / document_name(aDocument);
			if (!regular_file(path)) {
				return false;
			}
			return QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdU16String(path.u16string())));
		};
		return std::make_unique<qt::about_menu>(std::move(facts), std::move(actions));
	}
} // namespace bpr::app
