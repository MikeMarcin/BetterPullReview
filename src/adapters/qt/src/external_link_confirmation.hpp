#ifndef BPR_SRC_ADAPTERS_QT_SRC_EXTERNAL_LINK_CONFIRMATION_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_EXTERNAL_LINK_CONFIRMATION_HPP

#include <QString>

#include <functional>

class QDialog;
class QWidget;

namespace bpr::qt::detail {
	/// \brief Asks, window-modally, whether to open one address in the browser.
	///
	/// \details The question shows the complete address the host admitted, as
	/// plain text that wraps anywhere and can be selected and copied, so a
	/// link whose words say one place and whose destination is another is
	/// seen before anything opens. Open is the default button and Escape
	/// cancels. The dialog deletes itself when it closes and calls `aOpen`
	/// only when the reviewer chose Open; it returns at once rather than
	/// running a nested event loop, so a publication arriving meanwhile is
	/// handled normally and the confirmed gesture is admitted again.
	///
	/// It performs no effect itself: `aOpen` is the caller's dispatch.
	auto show_external_link_confirmation(QWidget& aOwner, const QString& aAddress, std::function<void()> aOpen) -> QDialog*;
} // namespace bpr::qt::detail

#endif // BPR_SRC_ADAPTERS_QT_SRC_EXTERNAL_LINK_CONFIRMATION_HPP
