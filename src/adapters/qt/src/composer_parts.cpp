#include "composer_parts.hpp"

#include "bpr/qt/drawer_parts.hpp"
#include "side_chat_parts.hpp"

#include <QBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>

namespace bpr::qt::composer {
	void set_field_margins(QBoxLayout& aColumn) { aColumn.setContentsMargins(field_left, field_top, field_right, field_bottom); }

	rule_line::rule_line(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("composerRule"));
		setFixedHeight(1);
	}

	void rule_line::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		painter.fillRect(rect(), side_chat::mixed(palette().color(QPalette::Active, QPalette::Base), palette().color(QPalette::Active, QPalette::WindowText), 0.14));
	}

	auto add_send_row(QWidget& aField, QBoxLayout& aColumn) -> QHBoxLayout*
	{
		aColumn.addWidget(new rule_line{ &aField });
		auto* const row = new QHBoxLayout{ };
		row->setContentsMargins(0, 0, 0, 0);
		row->setSpacing(2);
		aColumn.addLayout(row);
		return row;
	}

	auto make_send_button(QWidget& aField, const QString& aObjectName) -> QPushButton*
	{
		auto* const send = new QPushButton{ &aField };
		send->setObjectName(aObjectName);
		send->setDefault(true);
		send->setIcon(drawer_icon(drawer_glyph::send, aField.palette()));
		send->setAccessibleName(QObject::tr("Send"));
		return send;
	}
} // namespace bpr::qt::composer
