#include "bpr/qt/agent_report_view.hpp"

#include "bpr/core/agent_report.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

namespace bpr::qt {
	agent_report_view::agent_report_view(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("agentReportView"));
		setAccessibleName(tr("Agent's report"));
		auto* const row = new QHBoxLayout{ this };
		row->setContentsMargins(0, 0, 0, 0);
		row->setSpacing(0);
		auto* const side   = new QWidget{ this };
		auto* const column = new QVBoxLayout{ side };
		column->setContentsMargins(12, 12, 8, 12);
		column->setSpacing(6);
		auto* const glyph = new QLabel{ side };
		glyph->setPixmap(agent_glyph(palette()).pixmap(16, 16));
		Title = new QLabel{ side };
		Title->setObjectName(QStringLiteral("agentReportTitle"));
		Title->setWordWrap(true);
		Title->setTextFormat(Qt::PlainText);
		auto bold = Title->font();
		bold.setBold(true);
		Title->setFont(bold);
		auto* const head = new QHBoxLayout{ };
		head->addWidget(glyph, 0, Qt::AlignTop);
		head->addWidget(Title, 1);
		column->addLayout(head);
		Outline = new QListWidget{ side };
		Outline->setObjectName(QStringLiteral("agentReportOutline"));
		Outline->setAccessibleName(tr("Report outline"));
		Outline->setFrameShape(QFrame::NoFrame);
		Outline->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		column->addWidget(Outline, 1);
		side->setFixedWidth(220);
		row->addWidget(side);
		Scroll = new QScrollArea{ this };
		Scroll->setObjectName(QStringLiteral("agentReportScroll"));
		Scroll->setWidgetResizable(true);
		Scroll->setFrameShape(QFrame::NoFrame);
		auto* const page       = new QWidget{ Scroll };
		auto* const pageColumn = new QVBoxLayout{ page };
		pageColumn->setContentsMargins(20, 16, 20, 24);
		Body = new markdown_body_view{ page };
		Body->setObjectName(QStringLiteral("agentReportBody"));
		Body->setAccessibleName(tr("Report"));
		Body->set_link_dispatch([this](const std::string& aLink) {
			if (LinkDispatch) {
				LinkDispatch(QString::fromStdString(aLink));
			}
		});
		pageColumn->addWidget(Body);
		pageColumn->addStretch(1);
		Scroll->setWidget(page);
		row->addWidget(Scroll, 1);
		connect(Outline, &QListWidget::currentRowChanged, this, [this](const int aRow) { go_to_heading(aRow); });
		connect(Outline, &QListWidget::itemActivated, this, [this](QListWidgetItem* const aItem) { go_to_heading(Outline->row(aItem)); });
	}

	void agent_report_view::present(const QString& aTitle, const QString& aMarkdown)
	{
		Title->setText(aTitle);
		const auto text = aMarkdown.toStdString();
		if (Dresser) {
			Dresser(*Body);
		}
		Body->set_body(text, "agent-report");
		const QSignalBlocker blocker{ Outline };
		Outline->clear();
		for (const auto& heading : core::report_outline(text)) {
			auto* const item = new QListWidgetItem{ QString::fromStdString(heading.Title), Outline };
			item->setData(Qt::UserRole, static_cast<int>(heading.Level));
			// Deeper headings indent, so the outline reads as the report does.
			item->setText(QString(static_cast<qsizetype>((heading.Level > 1U ? heading.Level - 1U : 0U) * 3U), QLatin1Char(' ')) + item->text());
			item->setToolTip(QString::fromStdString(heading.Title));
		}
		Scroll->verticalScrollBar()->setValue(0);
	}

	void agent_report_view::set_link_dispatch(std::function<void(const QString&)> aDispatch) { LinkDispatch = std::move(aDispatch); }

	void agent_report_view::set_dresser(std::function<void(markdown_body_view&)> aDresser) { Dresser = std::move(aDresser); }

	auto agent_report_view::title() const -> QString { return Title->text(); }

	void agent_report_view::go_to_heading(const int aIndex)
	{
		auto* const item = Outline->item(aIndex);
		auto* const view = Body->rendered_view();
		if (item == nullptr || view == nullptr) {
			return;
		}
		const auto found = view->document()->find(item->toolTip());
		if (found.isNull()) {
			return;
		}
		const auto at = view->cursorRect(found).topLeft();
		Scroll->verticalScrollBar()->setValue(view->mapTo(Scroll->widget(), at).y());
	}
} // namespace bpr::qt
