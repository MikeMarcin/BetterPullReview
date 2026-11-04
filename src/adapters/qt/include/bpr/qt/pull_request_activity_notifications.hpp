#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PULL_REQUEST_ACTIVITY_NOTIFICATIONS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PULL_REQUEST_ACTIVITY_NOTIFICATIONS_HPP

#include "bpr/application/pull_request_activity_host.hpp"
#include "bpr/qt/system_tray_notification_sink.hpp"

#include <QObject>

#include <cstddef>
#include <functional>
#include <memory>

class QSystemTrayIcon;
class QTimer;

namespace bpr::qt {
	/// \brief [[BPR-1850]]. Posts activity notifications through the desktop's
	/// own surface, so the operating system's Do Not Disturb and permission
	/// apply, and reports a click.
	/// \details `QSystemTrayIcon` gives a click no message identity; the host's
	/// core decides where a click goes. A tray shared with the review queue's
	/// sink (the macOS status item) reaches only whichever sink posted last.
	class pull_request_activity_notification_sink final
		: public QObject,
		  public application::pull_request_activity_sink_port
	{
	public:
		/// \brief Over a caller-owned tray that outlives the sink.
		explicit pull_request_activity_notification_sink(QSystemTrayIcon& aTray, QObject* aParent = nullptr);
		/// \brief Owns a tray, when the platform has one.
		explicit pull_request_activity_notification_sink(QObject* aParent = nullptr);
		/// \brief Owns a tray and hands each post to \p aDisplay instead of the
		/// operating system, for courses.
		explicit pull_request_activity_notification_sink(system_notification_display_boundary aDisplay, QObject* aParent = nullptr);
		~pull_request_activity_notification_sink() override;

		pull_request_activity_notification_sink(const pull_request_activity_notification_sink&)                    = delete;
		auto operator=(const pull_request_activity_notification_sink&) -> pull_request_activity_notification_sink& = delete;
		pull_request_activity_notification_sink(pull_request_activity_notification_sink&&)                         = delete;
		auto operator=(pull_request_activity_notification_sink&&) -> pull_request_activity_notification_sink&      = delete;

		[[nodiscard]] auto post(const std::string& aTitle, const std::string& aBody) -> bool override;
		/// \brief What a click on a notification this sink posted calls.
		void               set_activation(std::function<void()> aActivation);
		[[nodiscard]] auto tray_icon() const noexcept -> QSystemTrayIcon*;
		[[nodiscard]] auto posted() const noexcept -> std::size_t;

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};

	/// \brief [[BPR-1850]]. The activity host's one timer: a single-shot
	/// `QTimer` on the main thread, rearmed after each poll.
	class pull_request_activity_timer final
		: public QObject,
		  public application::pull_request_activity_timer_port
	{
	public:
		explicit pull_request_activity_timer(std::function<void()> aFired, QObject* aParent = nullptr);
		~pull_request_activity_timer() override;

		pull_request_activity_timer(const pull_request_activity_timer&)                    = delete;
		auto operator=(const pull_request_activity_timer&) -> pull_request_activity_timer& = delete;
		pull_request_activity_timer(pull_request_activity_timer&&)                         = delete;
		auto operator=(pull_request_activity_timer&&) -> pull_request_activity_timer&      = delete;

		void               schedule(std::uint32_t aSeconds) override;
		void               cancel() override;
		[[nodiscard]] auto timer() const noexcept -> QTimer*;

	private:
		QTimer*               Timer = nullptr;
		std::function<void()> Fired;
	};
} // namespace bpr::qt

#endif
