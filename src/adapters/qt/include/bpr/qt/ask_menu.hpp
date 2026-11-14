#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ASK_MENU_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ASK_MENU_HPP

#include "bpr/core/quick_ask.hpp"
#include "bpr/qt/menu_tool_button.hpp"

#include <QString>

#include <functional>
#include <optional>

class QMenu;

/// \file
/// \brief RFC-BPR-0128 ([[BPR-2164]]). The Ask control and its menu: one
/// control at the review bar, the file header and a thread's reply row, and
/// one Ask item in a thread's and a file tab's right-click menus. The core
/// decides the rows (`core::decide_quick_asks`); this lists them, greys a row
/// with its reason, and sends the press. Building a menu makes no agent call.
namespace bpr::qt {
	/// \brief Which menu, and what it is about.
	struct ask_menu_request
	{
		core::quick_ask_anchor Anchor = core::quick_ask_anchor::review_bar;
		/// The menu's file (the open file, or the tab's), empty for none.
		QString File;
		/// The menu's thread, by identity, empty for none.
		QString Thread;
		/// The thread section's title: "Thread on line 10".
		QString ThreadTitle;
	};

	/// \brief The rows for a request, or none while the agent's features are
	/// not offered (no Ask control is shown then).
	using ask_menu_source = std::function<std::optional<core::quick_ask_menu>(const ask_menu_request&)>;

	/// \brief What a menu's items do.
	struct ask_menu_dispatch
	{
		/// A quick ask was pressed.
		std::function<void(const ask_menu_request&, const core::quick_ask_row&)> Ask;
		/// Custom question…: the Agent tab on the narrowest subject the
		/// request names; nothing is sent.
		std::function<void(const ask_menu_request&)> Custom;
		/// Edit Quick Asks…: Settings at the suggested prompts.
		std::function<void()> Edit;
	};

	/// \brief A greyed row's reason, in words.
	[[nodiscard]] auto quick_ask_reason_words(core::quick_ask_reason aReason, core::prompt_scope aScope) -> QString;
	/// \brief What a press reads: "18 commits", "12 threads"; empty for none.
	[[nodiscard]] auto quick_ask_count_words(const core::quick_ask_row& aRow) -> QString;

	/// \brief Fills `aMenu` with `aRows`: a section a scope (titled when the
	/// menu has more than one), each row with the agent glyph in the accent
	/// and what it reads at its right, a greyed row's reason as its tooltip,
	/// status tip and accessible description, then Custom question… and Edit
	/// Quick Asks…. Greyed rows can be reached by keyboard, which shows their
	/// reason.
	void fill_ask_menu(QMenu& aMenu, const core::quick_ask_menu& aRows, const ask_menu_request& aRequest, const ask_menu_dispatch& aDispatch);

	/// \brief The Ask control: an ordinary button with the agent glyph in the
	/// accent whose menu is decided each time it opens.
	class ask_menu_button final : public menu_tool_button
	{
	public:
		explicit ask_menu_button(QWidget* aParent = nullptr);

		void set_source(ask_menu_source aSource);
		void set_dispatch(ask_menu_dispatch aDispatch);
		/// \brief What the menu is about at the moment it opens.
		void set_request(std::function<ask_menu_request()> aRequest);
		/// \brief Or: fills the menu itself each time it opens (a thread's Ask,
		/// whose window decides the rows).
		void set_filler(std::function<void(QMenu&)> aFiller);
		/// \brief Fills the menu now, as opening it does (tests, keyboard).
		void refresh_menu();

	protected:
		void changeEvent(QEvent* aEvent) override;

	private:
		void wear_glyph();

		ask_menu_source                   Source;
		ask_menu_dispatch                 Dispatch;
		std::function<ask_menu_request()> Request;
		std::function<void(QMenu&)>       Filler;
	};
} // namespace bpr::qt

#endif
