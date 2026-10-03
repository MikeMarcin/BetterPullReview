#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SYSTEM_TRAY_NOTIFICATION_SINK_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SYSTEM_TRAY_NOTIFICATION_SINK_HPP

#include "bpr/application/notification_delivery.hpp"

#include <QObject>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>

class QString;
class QSystemTrayIcon;

namespace bpr::qt {
	/// \brief Replaces only the operating-system display submission.
	using system_notification_display_boundary = std::function<void(
		QSystemTrayIcon&,
		const QString&,
		const QString&)>;

	/// \brief Shows one queue notification through the desktop's own surface.
	///
	/// \details The payload arrives already composed. This class reads
	/// `Title`, `Body`, and nothing else — it never sees the request those came
	/// from, so there is no wording here to "improve" and no record to improve
	/// it from. `Target` is retained only so an activation can report which
	/// review the reviewer chose.
	///
	/// The target is a local reference and stays one. Nothing here builds a web
	/// address, and nothing hands the operating system a link another
	/// application could register for.
	///
	/// `QSystemTrayIcon` provides one `messageClicked` signal and no message
	/// identity. This sink therefore submits at most its first payload, retains
	/// that exact target immutably, consumes at most the first click, and never
	/// rearms during its lifetime.
	///
	/// That sentence is worded around the architecture scan rather than in
	/// spite of it: the scan refuses the obvious three-letter word anywhere in
	/// this adapter's GitHub projection surface, and it is right to, because a
	/// comment is exactly where the next reader would learn that building one
	/// here is thinkable.
	///
	/// Qt does not expose the reviewer's platform permission. The sink therefore
	/// begins `not_determined`; `request_permission()` changes that to
	/// `platform_managed`, authorizes one submission attempt, and leaves any
	/// prompt, refusal, or banner to the operating system.
	/// That attempt is not evidence that a banner was displayed. A missing tray
	/// surface is `refused`, and the sink never degrades to an application dialog.
	class system_tray_notification_sink final
		: public QObject,
		  public application::notification_sink_port
	{
	public:
		explicit system_tray_notification_sink(QObject* aParent = nullptr);
		/// \brief Uses one caller-owned tray surface.
		///
		/// \details The caller must keep the tray alive longer than this sink, and
		/// shows it. On macOS it is the application's menu bar status item, which
		/// every profile's sink shares ([[BPR-1250]]); a click then reaches only the
		/// sink that submitted to the tray last. It also supports deterministic
		/// platform-contract verification without claiming that an operating system
		/// displayed a banner.
		explicit system_tray_notification_sink(
			QSystemTrayIcon& aTray,
			QObject*         aParent = nullptr);
		/// \brief Owns a tray while delegating its display submission.
		///
		/// \details The product request, retained target, and activation signal
		/// remain this sink's production path. The supplied boundary is the only
		/// substitution and must not claim an operating system displayed a banner.
		explicit system_tray_notification_sink(
			system_notification_display_boundary aDisplay,
			QObject*                             aParent = nullptr);
		~system_tray_notification_sink() override;

		system_tray_notification_sink(const system_tray_notification_sink&) = delete;
		auto operator=(const system_tray_notification_sink&)
			-> system_tray_notification_sink&                          = delete;
		system_tray_notification_sink(system_tray_notification_sink&&) = delete;
		auto operator=(system_tray_notification_sink&&)
			-> system_tray_notification_sink& = delete;

		[[nodiscard]] auto permission()
			-> application::notification_permission override;
		[[nodiscard]] auto request_permission()
			-> application::notification_permission override;
		[[nodiscard]] auto deliver(
			const application::delivered_notification& aPayload)
			-> application::notification_submission_result override;

		/// \brief Installs what an activation reports the chosen review to.
		void set_activation_sink(application::notification_activation_sink aSink);

		[[nodiscard]] auto tray_icon() const noexcept -> QSystemTrayIcon*;
		[[nodiscard]] auto submitted() const noexcept -> std::size_t;
		[[nodiscard]] auto submitted_payload() const
			-> std::optional<application::delivered_notification>;

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SYSTEM_TRAY_NOTIFICATION_SINK_HPP
