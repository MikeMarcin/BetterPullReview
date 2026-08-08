#ifndef BPR_SRC_APP_INCLUDE_BPR_APP_GITHUB_APPLICATION_HPP
#define BPR_SRC_APP_INCLUDE_BPR_APP_GITHUB_APPLICATION_HPP

#include "bpr/application/notification_delivery.hpp"
#include "bpr/qt/github_window.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace bpr::app {
	namespace detail {
		class github_application_fixture_access;
	}

	/// \brief Describes the github application config record.
	struct github_application_config
	{
		std::optional<std::string>           EnterpriseHostname  = std::nullopt;
		std::optional<std::filesystem::path> ApplicationDataRoot = std::nullopt;
		std::optional<std::filesystem::path> GitExecutable       = std::nullopt;
	};

	/// \brief Describes the github application status record.
	struct github_application_status
	{
		bool Closing                       = false;
		bool ShutdownComplete              = false;
		bool ShutdownCleanupFailed         = false;
		bool NativePlatformVaultSelected   = false;
		bool EpochProjectionDeliveryFailed = false;
		bool EpochHandoffDeliveryFailed    = false;
		// Whether the composition actually constructed the workbench host, as
		// opposed to merely linking it. `nm` cannot tell those apart: the code
		// is compiled and its symbols are present whether or not the
		// constructor ever runs. This is the runtime half of that evidence.
		bool WorkbenchHostComposed  = false;
		bool AppearanceHostComposed = false;
		// The window palette's selection and code-background colours after the
		// profile was applied, as #rrggbb. Empty when no profile was applied.
		// Reported rather than a bare boolean because "a profile exists" and
		// "the palette reached the window" are different claims, and only the
		// second one matters.
		std::string AppliedSelectionColor;
		std::string AppliedBaseColor;
		// Whether those colours came from the appearance profile's semantic
		// roles rather than from the platform style. Reporting the colours
		// alone is not evidence: a window that was never given the profile
		// still has well-formed colours.
		bool AppliedFromProfile = false;
		// Whether the composition constructed the packaged syntax classifier.
		// Separate from the count below because "the classifier exists" and "a
		// pane took its answer" fail independently: a classifier nothing asks
		// and a pane nothing tells look identical from a symbol table.
		bool        SyntaxClassifierComposed = false;
		std::size_t AdoptedClassifications   = 0U;
		// Complete-blob passes the appearance host actually submitted. It
		// advances exactly when lexical work was unavoidable, so a repaint that
		// moves it is a repaint that reparsed.
		std::size_t ClassificationSubmissions = 0U;
		// The appearance settings in effect, as the core's stable names and
		// values.
		//
		// Deliberately not "what the record held on entry". `restore()` is an
		// effect: it returns before the durable record has been read, so a
		// value sampled just after it names what the composition started from
		// and not what was stored. That marker was written, measured, and
		// found to report `system` on a process whose window was already
		// showing the stored dark palette.
		//
		// The restart claim survives the correction. A composition starts from
		// `system` and a tab width of four, so a run that makes no gesture and
		// reports anything else read it from the durable record.
		std::string   AppearanceMode;
		std::uint32_t AppearanceTabWidth    = 0U;
		std::size_t   AppearanceDivergences = 0U;
		// The composed profile's selected mode and the variant it resolved to.
		// BPR-0114's claim is about these two, not about the platform palette:
		// the existing native probe proves Qt's palette follows the operating
		// system, which it would whether or not this product had an appearance
		// model at all.
		core::appearance_mode    ProfileMode = core::appearance_mode::system;
		core::appearance_variant ProfileVariant
			= core::appearance_variant::light;
		// Whether the composition constructed the queue notification delivery
		// path, and what the desktop says about showing one. Reported rather
		// than assumed: a refused or absent notification surface is a normal
		// outcome and must not read as a failure.
		bool                                 NotificationDeliveryComposed = false;
		application::notification_permission NotificationPermission
			= application::notification_permission::refused;
		core::epoch_status EpochStatus = core::epoch_status::inactive;
	};

	/// \brief Provides the github application contract.
	class github_application final
	{
	public:
		explicit github_application(github_application_config aConfig = { });
		~github_application();

		github_application(const github_application&)                    = delete;
		auto operator=(const github_application&) -> github_application& = delete;
		github_application(github_application&&)                         = delete;
		auto operator=(github_application&&) -> github_application&      = delete;

		void show();
		void begin_shutdown();

		[[nodiscard]] auto window() noexcept -> qt::github_window&;
		[[nodiscard]] auto status() const noexcept -> github_application_status;

	private:
		class implementation;
		explicit github_application(
			std::unique_ptr<implementation> aImplementation) noexcept;

		friend class detail::github_application_fixture_access;

		std::unique_ptr<implementation> Implementation;
	};
}

#endif // BPR_SRC_APP_INCLUDE_BPR_APP_GITHUB_APPLICATION_HPP
