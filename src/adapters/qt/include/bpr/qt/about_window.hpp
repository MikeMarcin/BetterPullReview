#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ABOUT_WINDOW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ABOUT_WINDOW_HPP

#include "bpr/core/markdown_navigation.hpp"
#include "bpr/core/reference_page.hpp"

#include <QDialog>
#include <QObject>
#include <QPointer>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

class QAbstractButton;
class QLabel;
class QMenuBar;
class QPushButton;
class QTimer;
class QVBoxLayout;

namespace bpr::qt {
	/// \brief [[BPR-1720]]. A legal document a release installs beside
	/// the application, which Help › About opens.
	enum class about_document : std::uint8_t {
		/// `THIRD-PARTY-NOTICES.txt`: the MIT grant, every component's licence
		/// and the corresponding-source reference.
		third_party_notices,
		/// `SOURCE-OFFERS.txt`: the exact corresponding-source asset, its
		/// address and digest, and how to replace the LGPL libraries.
		source_offer,
	};

	/// \brief [[BPR-1720]]. What Help › About says about this build.
	///
	/// \details Facts the composition reads once: the build identity compiled
	/// in, the runtime Qt and system, and whether the installed legal
	/// documents are present. The page formats them and decides nothing else;
	/// no address is held here, because a link names a `core::reference_page`.
	struct about_facts
	{
		/// The application version, "0.14.0".
		QString Version;
		/// The release candidate cohort, or zero for an ordinary local build.
		unsigned BuildNumber = 0U;
		/// Frozen cross-platform candidate configuration and cohort identities.
		QString ConfigurationDigest;
		QString CohortDigest;
		/// The full commit the build was made from, or empty outside a Git checkout.
		QString Commit;
		/// That commit's date, ISO "2026-10-21", or empty with no commit.
		QString CommitDate;
		/// Whether tracked files differed from the commit when it was built.
		bool LocalChanges = false;
		/// The build configuration, "Debug" or "Release".
		QString Configuration;
		/// The Qt the application runs on, "6.12.0".
		QString QtVersion;
		/// The operating system and architecture, "macOS 26.0 (arm64)".
		QString System;
		/// The copyright line, as the licence states it.
		QString Copyright;
		/// The folder holding the installed legal documents, as a reader would
		/// type it, or empty when this build installs none.
		QString LegalFolder;
		/// Whether `THIRD-PARTY-NOTICES.txt` is installed.
		bool NoticesInstalled = false;
		/// Whether `SOURCE-OFFERS.txt` is installed.
		bool SourceOfferInstalled = false;
	};

	/// \brief What the page asks its host to do. Each is optional; an absent
	/// one answers as unavailable.
	struct about_actions
	{
		/// Opens a named page in the reviewer's browser.
		std::function<core::markdown_link_result(core::reference_page)> OpenPage;
		/// Opens an installed document with the system's viewer; false when it
		/// did not open.
		std::function<bool(about_document)> OpenDocument;
	};

	/// \brief The text Copy version info puts on the clipboard: one fact a
	/// line, in the order a bug report reads them.
	[[nodiscard]] auto about_version_report(const about_facts& aFacts) -> QString;

	/// \brief The build line under the version: "Commit 1a2b3c4d5e from
	/// 2026-10-21", with ", with local changes" when there were.
	[[nodiscard]] auto about_build_line(const about_facts& aFacts) -> QString;

	/// \brief [[BPR-1720]]. Help › About BetterPullReview.
	///
	/// \details Game Core Tech's logo, the application's name, version and
	/// build, Copy version info, the application's site and source links,
	/// the maker and its site, and the
	/// licence: the copyright, Qt's LGPL terms, and the installed third-party
	/// notices and corresponding-source offer. Presentation only: the links
	/// and documents go to the host through `about_actions`, and the one thing
	/// done here is writing the version report to the clipboard. Non-modal;
	/// Esc and Close hide it, and its owner reuses it.
	class about_window final : public QDialog
	{
	public:
		about_window(about_facts aFacts, about_actions aActions, QWidget* aParent = nullptr);
		~about_window() override;

		[[nodiscard]] auto facts() const noexcept -> const about_facts&;
		[[nodiscard]] auto logo() const noexcept -> QLabel*;
		[[nodiscard]] auto version_label() const noexcept -> QLabel*;
		[[nodiscard]] auto build_label() const noexcept -> QLabel*;
		[[nodiscard]] auto copy_button() const noexcept -> QPushButton*;
		[[nodiscard]] auto page_button(core::reference_page aPage) const noexcept -> QAbstractButton*;
		[[nodiscard]] auto document_button(about_document aDocument) const noexcept -> QAbstractButton*;
		/// The sentence shown in place of the document buttons when this build
		/// installs neither document.
		[[nodiscard]] auto documents_absent_label() const noexcept -> QLabel*;
		/// The line that says a link or document did not open; hidden otherwise.
		[[nodiscard]] auto status_label() const noexcept -> QLabel*;
		[[nodiscard]] auto close_button() const noexcept -> QPushButton*;

	protected:
		[[nodiscard]] auto event(QEvent* aEvent) -> bool override;
		void               changeEvent(QEvent* aEvent) override;
		void               showEvent(QShowEvent* aEvent) override;

	private:
		void open_page(core::reference_page aPage);
		void open_document(about_document aDocument);
		void copy_version_report();
		void show_status(const QString& aMessage);
		void render_logo();
		void apply_link_ink();
		void apply_metrics();

		about_facts          Facts;
		about_actions        Actions;
		QVBoxLayout*         Column    = nullptr;
		QLabel*              Name      = nullptr;
		QLabel*              MakerLine = nullptr;
		QWidget*             Rule      = nullptr;
		std::vector<QLabel*> Small;
		QLabel*              Logo            = nullptr;
		QLabel*              Version         = nullptr;
		QLabel*              Build           = nullptr;
		QPushButton*         Copy            = nullptr;
		QAbstractButton*     Product         = nullptr;
		QAbstractButton*     Maker           = nullptr;
		QAbstractButton*     Source          = nullptr;
		QAbstractButton*     Notices         = nullptr;
		QAbstractButton*     Offer           = nullptr;
		QLabel*              DocumentsAbsent = nullptr;
		QLabel*              Status          = nullptr;
		QPushButton*         Close           = nullptr;
		QTimer*              CopiedReset     = nullptr;
		qreal                LogoScale       = 0.0;
	};

	/// \brief [[BPR-1720]]. Puts About BetterPullReview on every menu
	/// bar it is attached to and presents the page for the window that owns
	/// the bar.
	///
	/// \details The item joins the bar's Help menu (`releaseUpdateMenu`),
	/// last, with the About role, so macOS shows it first in the application
	/// menu. A window's page is its child and takes its theme; a bar no
	/// window owns (the empty desktop's) shows one page of its own.
	class about_menu final : public QObject
	{
	public:
		about_menu(about_facts aFacts, about_actions aActions);
		~about_menu() override;

		void attach(QMenuBar& aMenu);
		/// \brief Shows and raises the page for `aMenu`'s window, made on first use.
		auto present(QMenuBar& aMenu) -> about_window*;

	private:
		about_facts                     Facts;
		about_actions                   Actions;
		std::vector<QPointer<QMenuBar>> Menus;
		std::unique_ptr<about_window>   Unowned;
	};
}

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ABOUT_WINDOW_HPP
