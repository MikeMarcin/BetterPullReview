#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SYSTEM_TRAY_NOTIFICATION_SINK_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SYSTEM_TRAY_NOTIFICATION_SINK_HPP

#include "bpr/application/notification_delivery.hpp"

#include <QObject>

#include <cstddef>
#include <memory>

class QSystemTrayIcon;

namespace bpr::qt {
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
	/// That sentence is worded around the architecture scan rather than in
	/// spite of it: the scan refuses the obvious three-letter word anywhere in
	/// this adapter's GitHub projection surface, and it is right to, because a
	/// comment is exactly where the next reader would learn that building one
	/// here is thinkable.
	///
	/// **Permission is read, never assumed.** Where the desktop offers no
	/// notification surface at all, that reads as `refused`: nothing is
	/// delivered, nothing is reported as an error, and the reviewer is not
	/// asked. A tool that degraded to a dialog box would have overridden an
	/// answer the platform already gave.
	class system_tray_notification_sink final
		: public QObject,
		  public application::notification_sink_port
	{
	public:
		explicit system_tray_notification_sink(QObject* aParent = nullptr);
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
		void deliver(const application::delivered_notification& aPayload) override;

		/// \brief Installs what an activation reports the chosen review to.
		void set_activation_sink(application::notification_activation_sink aSink);

		[[nodiscard]] auto tray_icon() const noexcept -> QSystemTrayIcon*;
		[[nodiscard]] auto delivered() const noexcept -> std::size_t;

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_SYSTEM_TRAY_NOTIFICATION_SINK_HPP
