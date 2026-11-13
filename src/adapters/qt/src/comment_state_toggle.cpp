#include "comment_state_toggle.hpp"

#include "bpr/qt/quiet_tool_button.hpp"
#include "conversation_glyphs.hpp"

#include <QEvent>
#include <QHBoxLayout>
#include <QSize>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto segment(QWidget* const aParent, const QPalette::ColorRole aSurface) -> quiet_tool_button*
		{
			auto* const button = new quiet_tool_button{ aParent };
			button->set_surface(aSurface);
			button->setAutoRaise(false);
			button->setCheckable(true);
			button->setToolButtonStyle(Qt::ToolButtonIconOnly);
			button->setIconSize(QSize{ 16, 16 });
			button->setFocusPolicy(Qt::StrongFocus);
			button->setCursor(Qt::PointingHandCursor);
			return button;
		}
	} // namespace

	comment_state_toggle::comment_state_toggle(const QPalette::ColorRole aSurface, QWidget* const aParent)
		: QWidget{ aParent }
		, Local{ segment(this, aSurface) }
		, Draft{ segment(this, aSurface) }
	{
		setAccessibleName(QStringLiteral("Comment state"));
		auto* const row = new QHBoxLayout{ this };
		row->setContentsMargins(0, 0, 0, 0);
		row->setSpacing(0);
		row->addWidget(Local);
		row->addWidget(Draft);
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		Local->setText(QStringLiteral("Local"));
		Draft->setText(QStringLiteral("Draft"));
		Local->setProperty("conversationGlyph", static_cast<int>(conversation_glyph::computer));
		set_names(QStringLiteral("Local comment, only on this computer"), QStringLiteral("Draft"));
		set_local(true, QStringLiteral("Only on this computer"));
		set_draft(true, QStringLiteral("A draft you can add to your review or post"));
		connect(Local, &QToolButton::clicked, this, [this] { press(true); });
		connect(Draft, &QToolButton::clicked, this, [this] { press(false); });
		refresh_icons();
	}

	void comment_state_toggle::set_state(const state aState)
	{
		State = aState;
		Local->setChecked(aState == state::local);
		Draft->setChecked(aState == state::draft);
	}

	void comment_state_toggle::set_local(const bool aAllowed, const QString& aTip)
	{
		Local->setEnabled(aAllowed);
		if (Local->toolTip() != aTip) {
			Local->setToolTip(aTip);
			Local->setAccessibleDescription(aTip);
		}
	}

	void comment_state_toggle::set_draft(const bool aAllowed, const QString& aTip)
	{
		Draft->setEnabled(aAllowed);
		// Read by tests and assistive tooling: the pencil is struck through.
		Draft->setProperty("draftRefused", !aAllowed);
		if (Draft->toolTip() != aTip) {
			Draft->setToolTip(aTip);
			Draft->setAccessibleDescription(aTip);
		}
		if (DraftRefused != !aAllowed) {
			DraftRefused = !aAllowed;
			refresh_icons();
		}
	}

	void comment_state_toggle::set_names(const QString& aLocal, const QString& aDraft)
	{
		Local->setAccessibleName(aLocal);
		Draft->setAccessibleName(aDraft);
	}

	void comment_state_toggle::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) {
			refresh_icons();
		}
	}

	void comment_state_toggle::refresh_icons()
	{
		const auto pencil = DraftRefused ? conversation_glyph::edit_refused : conversation_glyph::edit;
		Local->setIcon(conversation_icon(conversation_glyph::computer, palette()));
		Draft->setIcon(conversation_icon(pencil, palette()));
		Draft->setProperty("conversationGlyph", static_cast<int>(pencil));
	}

	void comment_state_toggle::press(const bool aLocal)
	{
		// The state is the one the comment is in until its owner says it
		// moved: pressing the other segment asks for the move, and the
		// segments show the present state again until it is made.
		const auto was = State;
		set_state(was);
		if (aLocal && was != state::local && LocalChosen) {
			LocalChosen();
		} else if (!aLocal && was != state::draft && DraftChosen) {
			DraftChosen();
		}
	}
} // namespace bpr::qt
