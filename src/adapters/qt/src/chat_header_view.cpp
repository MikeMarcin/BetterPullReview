#include "bpr/qt/chat_header_view.hpp"

#include "pull_request_overview_support.hpp"
#include "side_chat_parts.hpp"

#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/quiet_tool_button.hpp"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QVBoxLayout>

#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto heading(QLabel* const aLabel) -> eliding_heading& { return *static_cast<eliding_heading*>(aLabel); }
	}

	chat_header_view::chat_header_view(const names& aNames, QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(aNames.Row);
		Row = new QHBoxLayout{ this };
		Row->setContentsMargins(0, 0, 0, 0);
		Row->setSpacing(4);

		// What the chat is about, and where: one line each, each ending in
		// an ellipsis where the row is narrow (owner, attended review
		// 2026-11-24, of a title wrapped over five lines: "Those buttons are
		// unreadable and thats not even the narrowest width").
		auto* const about = new QVBoxLayout{ };
		about->setContentsMargins(0, 0, 0, 0);
		about->setSpacing(1);
		Subject = new eliding_heading{ this };
		Subject->setObjectName(aNames.Subject);
		Subject->setTextFormat(Qt::PlainText);
		Subject->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		auto heavy = Subject->font();
		heavy.setWeight(QFont::DemiBold);
		Subject->setFont(heavy);
		Place = new eliding_heading{ this };
		Place->setObjectName(aNames.Place);
		Place->setTextFormat(Qt::PlainText);
		Place->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		side_chat::quiet(side_chat::small(Place));
		Place->hide();
		about->addWidget(Subject);
		about->addWidget(Place);
		Row->addLayout(about, 1);

		Show = new quiet_push_button{ tr("Show"), this };
		Show->setObjectName(aNames.Show);
		Show->setFlat(true);
		Show->setFocusPolicy(Qt::TabFocus);
		Show->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		Show->hide();
		Row->addWidget(Show, 0, Qt::AlignTop);

		History = make_drawer_tool(this, drawer_icon(drawer_glyph::history, palette()), tr("History"), aNames.History);
		History->hide();
		Copy = make_drawer_tool(this, drawer_icon(drawer_glyph::copy, palette()), tr("Copy conversation"), aNames.Copy);
		Copy->setFocusPolicy(Qt::TabFocus);
		Copy->hide();
		Close = make_drawer_tool(this, drawer_icon(drawer_glyph::close, palette()), tr("Close the chat"), aNames.Close);
		Close->setFocusPolicy(Qt::TabFocus);
		Close->hide();
		Row->addWidget(History, 0, Qt::AlignTop);
		Row->addWidget(Copy, 0, Qt::AlignTop);
		Row->addWidget(Close, 0, Qt::AlignTop);

		connect(Show, &QPushButton::clicked, this, [this] {
			if (Dispatch.Show) {
				Dispatch.Show();
			}
		});
		connect(Copy, &QToolButton::clicked, this, [this] {
			if (Dispatch.CopyConversation) {
				Dispatch.CopyConversation();
			}
		});
		connect(Close, &QToolButton::clicked, this, [this] {
			if (Dispatch.Close) {
				Dispatch.Close();
			}
		});
		present(core::chat_header{ });
	}

	void chat_header_view::set_dispatch(dispatch aDispatch) { Dispatch = std::move(aDispatch); }

	void chat_header_view::set_tips(const tips& aTips)
	{
		Tips = aTips;
		Show->setToolTip(Tips.Show);
		Close->setToolTip(Tips.Close.isEmpty() ? tr("Close the chat") : Tips.Close);
	}

	void chat_header_view::present(const core::chat_header& aHeader)
	{
		using action = core::chat_header_action;
		if (Presented && Header == aHeader) {
			return;
		}
		Presented = true;
		Header    = aHeader;
		heading(Subject).present(QString::fromStdString(Header.Title), QString::fromStdString(Header.TitleNote));
		heading(Place).present(QString::fromStdString(Header.Place), QString{ });
		Place->setVisible(!Header.Place.empty());

		// Core's choice of the row's tools; none is shown or hidden by the
		// chat that wears the row.
		ShowWords = Header.Show == core::chat_show::in_code ? tr("Show in code") : tr("Show");
		Show->setVisible(Header.Show != core::chat_show::none);
		History->setVisible(Header.Tool == core::chat_header_tool::history && Header.offers(action::history));
		Copy->setVisible(Header.Tool == core::chat_header_tool::copy_conversation && Header.offers(action::copy_conversation));
		Close->setVisible(Header.Close);

		History->setToolTip(Header.offers(action::show_every_run) ? tr("History: this chat's sessions, New session and every run") : tr("History: this chat's sessions, and New session"));
		History->setAccessibleName(tr("History"));
		const auto copies = tr("Copy this conversation as text");
		Copy->setToolTip(Header.ToolNote.empty() ? copies : copies + QStringLiteral(". ") + QString::fromStdString(Header.ToolNote));
		Copy->setAccessibleName(tr("Copy conversation"));
		Close->setAccessibleName(tr("Close the chat"));
		fit();
	}

	auto chat_header_view::shown_title() const -> QString { return heading(Subject).shown_text(); }

	auto chat_header_view::title_cut() const -> bool { return heading(Subject).elided(); }

	void chat_header_view::repaint_glyphs()
	{
		History->setIcon(drawer_icon(drawer_glyph::history, palette()));
		Copy->setIcon(drawer_icon(drawer_glyph::copy, palette()));
		Close->setIcon(drawer_icon(drawer_glyph::close, palette()));
	}

	void chat_header_view::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		fit();
	}

	void chat_header_view::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange && History != nullptr) {
			repaint_glyphs();
		}
	}

	void chat_header_view::fit()
	{
		// Show in code is said whole while the part beside it keeps room for
		// its number and a few words; in a narrower row it is Show, with the
		// whole of it in its tooltip. Nothing in the row is ever cut.
		Show->setText(ShowWords);
		if (Show->isHidden() || Header.Show != core::chat_show::in_code) {
			return;
		}
		auto tools = Show->sizeHint().width();
		for (const auto* const tool : { static_cast<const QWidget*>(History), static_cast<const QWidget*>(Copy), static_cast<const QWidget*>(Close) }) {
			if (!tool->isHidden()) {
				tools += tool->sizeHint().width() + Row->spacing();
			}
		}
		const auto margins = Row->contentsMargins();
		const auto room    = width() - margins.left() - margins.right() - tools - Row->spacing();
		if (room < Subject->fontMetrics().horizontalAdvance(QStringLiteral("Stop 00, a few words…"))) {
			Show->setText(tr("Show"));
		}
	}
}
