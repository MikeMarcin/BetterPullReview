#include "bpr/qt/agent_pending_list.hpp"

#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "conversation_glyphs.hpp"

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		/// [[BPR-2151]] look prompts-v001 ("some defined borders maybe";
		/// "drag to reorder instead of up/down arrows, a trash can to delete
		/// and a pencil to edit"). The pending list is a bordered panel of
		/// bordered cards: a grip to drag a card, the message, a pencil and a
		/// trash can. Colours come from the palette.
		constexpr auto queue_mime = "application/x-bpr-queued-message";

		[[nodiscard]] auto blend(const QColor& aFrom, const QColor& aTo, const float aShare) -> QColor
		{
			return QColor::fromRgbF(aFrom.redF() + (aTo.redF() - aFrom.redF()) * aShare, aFrom.greenF() + (aTo.greenF() - aFrom.greenF()) * aShare,
									aFrom.blueF() + (aTo.blueF() - aFrom.blueF()) * aShare);
		}

		/// One waiting message: a card with a defined edge, dragged by its
		/// grip, moved by Option-Up and Option-Down, removed by Delete.
		class queue_card final : public QFrame
		{
		public:
			queue_card(const std::uint64_t aId, const bool aMovable, std::function<void(std::uint64_t, int)> aMove, std::function<void(std::uint64_t)> aRemove, QWidget* const aParent)
				: QFrame{ aParent }
				, Id{ aId }
				, Movable{ aMovable }
				, Move{ std::move(aMove) }
				, Remove{ std::move(aRemove) }
			{
				setObjectName(QStringLiteral("agentQueueRow"));
				setFocusPolicy(aMovable ? Qt::StrongFocus : Qt::NoFocus);
				setAttribute(Qt::WA_Hover);
				setMinimumHeight(30);
			}

			[[nodiscard]] auto id() const noexcept -> std::uint64_t { return Id; }

		protected:
			void paintEvent(QPaintEvent* const aEvent) override
			{
				static_cast<void>(aEvent);
				auto       painter = QPainter{ this };
				const auto ground  = palette().color(QPalette::Base);
				const auto ink     = palette().color(QPalette::WindowText);
				const auto lit     = hasFocus() || underMouse();
				painter.setRenderHint(QPainter::Antialiasing);
				painter.setPen(QPen{ lit ? palette().color(QPalette::Highlight) : blend(ground, ink, 0.22F), lit ? 1.5 : 1.0 });
				painter.setBrush(ground);
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.75, 0.75, -0.75, -0.75), 6, 6);
				if (Movable) {
					// The grip: two columns of three dots.
					painter.setPen(Qt::NoPen);
					painter.setBrush(blend(ground, ink, 0.45F));
					for (int column = 0; column < 2; ++column) {
						for (int dot = 0; dot < 3; ++dot) {
							painter.drawEllipse(QPointF{ 9.0 + column * 4.0, height() / 2.0 - 4.0 + dot * 4.0 }, 1.1, 1.1);
						}
					}
				}
			}

			void mousePressEvent(QMouseEvent* const aEvent) override
			{
				Pressed = aEvent->position().toPoint();
				QFrame::mousePressEvent(aEvent);
			}

			void mouseMoveEvent(QMouseEvent* const aEvent) override
			{
				if (!Movable || !(aEvent->buttons() & Qt::LeftButton) || (aEvent->position().toPoint() - Pressed).manhattanLength() < QApplication::startDragDistance()) {
					QFrame::mouseMoveEvent(aEvent);
					return;
				}
				auto* const drag    = new QDrag{ this };
				auto* const payload = new QMimeData;
				payload->setData(QString::fromLatin1(queue_mime), QByteArray::number(static_cast<qulonglong>(Id)));
				drag->setMimeData(payload);
				drag->setPixmap(grab());
				drag->setHotSpot(Pressed);
				static_cast<void>(drag->exec(Qt::MoveAction));
			}

			void keyPressEvent(QKeyEvent* const aEvent) override
			{
				if (Movable && (aEvent->modifiers() & Qt::AltModifier) != 0 && (aEvent->key() == Qt::Key_Up || aEvent->key() == Qt::Key_Down)) {
					if (Move) {
						Move(Id, aEvent->key() == Qt::Key_Up ? -1 : 1);
					}
					return;
				}
				if (Movable && (aEvent->key() == Qt::Key_Delete || aEvent->key() == Qt::Key_Backspace)) {
					if (Remove) {
						Remove(Id);
					}
					return;
				}
				QFrame::keyPressEvent(aEvent);
			}

			void focusInEvent(QFocusEvent* const aEvent) override
			{
				update();
				QFrame::focusInEvent(aEvent);
			}

			void focusOutEvent(QFocusEvent* const aEvent) override
			{
				update();
				QFrame::focusOutEvent(aEvent);
			}

		private:
			std::uint64_t                           Id      = 0U;
			bool                                    Movable = false;
			std::function<void(std::uint64_t, int)> Move;
			std::function<void(std::uint64_t)>      Remove;
			QPoint                                  Pressed;
		};

		/// Look prompts-v003 (owner: "Long messages should not grow to
		/// multi-line they should just end with ellipsis"): a queued
		/// message on one line, elided at its end; `text()` stays whole, and
		/// the tooltip and the editor show it all.
		class elided_label final : public QLabel
		{
		public:
			using QLabel::QLabel;

			[[nodiscard]] auto minimumSizeHint() const -> QSize override { return { 0, QLabel::minimumSizeHint().height() }; }

			[[nodiscard]] auto sizeHint() const -> QSize override { return { std::min(QLabel::sizeHint().width(), 200), QLabel::sizeHint().height() }; }

		protected:
			void paintEvent(QPaintEvent* const aEvent) override
			{
				static_cast<void>(aEvent);
				auto       painter = QPainter{ this };
				const auto shown   = fontMetrics().elidedText(text(), Qt::ElideRight, contentsRect().width());
				painter.setPen(palette().color(foregroundRole()));
				painter.drawText(contentsRect(), static_cast<int>(Qt::AlignVCenter | Qt::AlignLeft) | Qt::TextSingleLine, shown);
			}
		};

	} // namespace

	agent_pending_list::agent_pending_list(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setAcceptDrops(true);
		setAccessibleName(tr("Queued messages"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(8, 6, 8, 8);
		column->setSpacing(4);
		hide();
	}

	void agent_pending_list::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto       painter = QPainter{ this };
		const auto ground  = palette().color(QPalette::Window);
		const auto ink     = palette().color(QPalette::WindowText);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setPen(QPen{ blend(ground, ink, 0.28F), 1.0 });
		painter.setBrush(blend(ground, ink, 0.035F));
		painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
		if (Line >= 0) {
			painter.setPen(QPen{ palette().color(QPalette::Highlight), 2.0 });
			painter.drawLine(QPointF{ 10.0, static_cast<qreal>(Line) }, QPointF{ static_cast<qreal>(width() - 10), static_cast<qreal>(Line) });
		}
	}

	void agent_pending_list::dragEnterEvent(QDragEnterEvent* const aEvent)
	{
		if (aEvent->mimeData()->hasFormat(QString::fromLatin1(queue_mime))) {
			aEvent->acceptProposedAction();
		}
	}

	void agent_pending_list::dragMoveEvent(QDragMoveEvent* const aEvent)
	{
		if (!aEvent->mimeData()->hasFormat(QString::fromLatin1(queue_mime))) {
			return;
		}
		Line = landing(aEvent->position().toPoint().y()).second;
		update();
		aEvent->acceptProposedAction();
	}

	void agent_pending_list::dragLeaveEvent(QDragLeaveEvent* const aEvent)
	{
		Line = -1;
		update();
		QWidget::dragLeaveEvent(aEvent);
	}

	void agent_pending_list::dropEvent(QDropEvent* const aEvent)
	{
		Line = -1;
		update();
		aEvent->acceptProposedAction();
		land(aEvent->mimeData()->data(QString::fromLatin1(queue_mime)).toULongLong(), landing(aEvent->position().toPoint().y()).first);
	}

	void agent_pending_list::land(const std::uint64_t aId, int aAt)
	{
		const auto cards = movable();
		const auto from  = std::ranges::find_if(cards, [aId](const QWidget* aCard) { return static_cast<const queue_card*>(aCard)->id() == aId; });
		if (from == cards.end() || !Dispatch.Move) {
			return;
		}
		const auto index = static_cast<int>(from - cards.begin());
		if (aAt > index) {
			--aAt;
		}
		if (aAt != index) {
			// After the drag returns: the move rebuilds the cards.
			QTimer::singleShot(0, this, [move = Dispatch.Move, aId, by = aAt - index] { move(aId, by); });
		}
	}

	auto agent_pending_list::movable() const -> std::vector<QWidget*>
	{
		auto cards = std::vector<QWidget*>{ };
		for (auto* const child : findChildren<QFrame*>(QStringLiteral("agentQueueRow"), Qt::FindDirectChildrenOnly)) {
			if (auto* const card = dynamic_cast<queue_card*>(child); card != nullptr && !card->isHidden() && card->focusPolicy() != Qt::NoFocus) {
				cards.push_back(card);
			}
		}
		std::ranges::sort(cards, { }, [](const QWidget* aCard) { return aCard->y(); });
		return cards;
	}

	auto agent_pending_list::landing(const int aY) const -> std::pair<int, int>
	{
		const auto cards = movable();
		for (std::size_t index = 0U; index < cards.size(); ++index) {
			if (aY < cards[index]->geometry().center().y()) {
				return { static_cast<int>(index), cards[index]->geometry().top() - 2 };
			}
		}
		return { static_cast<int>(cards.size()), cards.empty() ? 4 : cards.back()->geometry().bottom() + 3 };
	}

	void agent_pending_list::present(std::vector<agent_pending_row> aRows, const QString& aState, const bool aPaused)
	{
		Rows               = aRows.size();
		auto* const column = static_cast<QVBoxLayout*>(layout());
		while (auto* const item = column->takeAt(0)) {
			// Out of the list at once (a control pressed may be inside it),
			// deleted when the press is done.
			if (auto* const gone = item->widget(); gone != nullptr) {
				gone->hide();
				gone->setParent(nullptr);
				gone->deleteLater();
			}
			delete item;
		}
		if (aRows.empty()) {
			Editing.reset();
			hide();
			return;
		}
		const auto button = [this](QWidget* aParent, const QString& aText, const QString& aName, const QString& aSpoken) {
			auto* const made = new quiet_push_button{ aParent };
			made->setText(aText);
			made->setObjectName(aName);
			made->setFlat(true);
			made->setAccessibleName(aSpoken);
			made->setToolTip(aSpoken);
			made->setFocusPolicy(Qt::StrongFocus);
			static_cast<void>(this);
			return made;
		};
		// An icon alone, named and tipped: the pencil and the trash can.
		const auto glyph = [this, &button](QWidget* aParent, const conversation_glyph aGlyph, const QString& aName, const QString& aSpoken) {
			auto* const made = button(aParent, QString{ }, aName, aSpoken);
			made->setIcon(conversation_icon(aGlyph, palette()));
			made->setIconSize(QSize{ 14, 14 });
			// An icon alone takes an icon's room, so the message keeps the
			// rest of the line (look prompts-v004: "a bit of wasted space
			// around these icons we could claw back").
			made->setFixedSize(QSize{ 22, 22 });
			return made;
		};
		// The panel's head: what waits, how many, the queue's state, and
		// Pause or Resume.
		{
			auto* const head  = new QWidget{ this };
			auto* const heads = new QHBoxLayout{ head };
			heads->setContentsMargins(2, 0, 0, 2);
			auto* const title = new QLabel{ tr("Waiting"), head };
			title->setObjectName(QStringLiteral("agentQueueTitle"));
			auto titleFont = title->font();
			titleFont.setWeight(QFont::DemiBold);
			title->setFont(titleFont);
			heads->addWidget(title);
			const auto  waiting = std::ranges::count_if(aRows, [](const agent_pending_row& aRow) { return !aRow.Sending; });
			auto* const count   = new QLabel{ QString::number(waiting), head };
			count->setForegroundRole(QPalette::PlaceholderText);
			heads->addWidget(count);
			heads->addStretch(1);
			auto* const toggle = button(head, aPaused ? tr("Resume queue") : tr("Pause queue"), QStringLiteral("agentQueuePauseResume"), aPaused ? tr("Resume queue") : tr("Pause queue"));
			connect(toggle, &QPushButton::clicked, this, [this, aPaused] {
				const auto act = aPaused ? Dispatch.Resume : Dispatch.Pause;
				if (act) {
					act();
				}
			});
			heads->addWidget(toggle);
			column->addWidget(head);
			// Look prompts-v002 (owner: "the drawer is back to having a
			// horizontal panning/scrolling"): the queue's state is a line of
			// its own under the head and wraps, so nothing in the list is
			// wider than the drawer.
			if (!aState.isEmpty()) {
				auto* const state = new QLabel{ aState, this };
				state->setObjectName(QStringLiteral("agentQueueState"));
				state->setForegroundRole(QPalette::PlaceholderText);
				state->setWordWrap(true);
				state->setContentsMargins(2, 0, 0, 2);
				column->addWidget(state);
			}
		}
		// Each control calls a copy of its dispatch: the call may present the
		// list again, and a new dispatch then replaces the one it runs in
		// (on Linux the freed closure crashed the tour chat queue courses).
		const auto move = [this](const std::uint64_t aId, const int aBy) {
			if (const auto act = Dispatch.Move) {
				act(aId, aBy);
			}
		};
		const auto drop = [this](const std::uint64_t aId) {
			if (const auto act = Dispatch.Remove) {
				act(aId);
			}
		};
		for (std::size_t index = 0U; index < aRows.size(); ++index) {
			const auto& entry = aRows[index];
			const auto  id    = entry.Id;
			auto* const row   = new queue_card{ id, !entry.Sending && !entry.Held, move, drop, this };
			row->setProperty("agentQueueId", QVariant::fromValue<qulonglong>(entry.Id));
			row->setAccessibleName(tr("Queued %1 of %2: %3").arg(index + 1U).arg(aRows.size()).arg(entry.Label));
			row->setAccessibleDescription(entry.Uncertain ? tr("Delivery unknown") : entry.Sent ? tr("Sent to agent")
											  : entry.Sending                                   ? tr("Sending")
																								: tr("Drag, or Option-Up and Option-Down, to reorder; Delete removes it"));
			auto* const line = new QHBoxLayout{ row };
			line->setContentsMargins(22, 3, 2, 3);
			line->setSpacing(0);
			if (Editing == entry.Id && entry.Held) {
				// Edited in place, held: never sent, and nothing passes it.
				line->setContentsMargins(8, 3, 4, 3);
				auto* const field = new QLineEdit{ entry.Text, row };
				field->setObjectName(QStringLiteral("agentQueueEditField"));
				field->setMinimumWidth(40);
				field->setAccessibleName(tr("Queued message"));
				line->addWidget(field, 1);
				auto* const save   = button(row, tr("Save"), QStringLiteral("agentQueueSave"), tr("Save the queued message"));
				auto* const cancel = button(row, tr("Cancel"), QStringLiteral("agentQueueCancel"), tr("Keep the queued message as it was"));
				connect(save, &QPushButton::clicked, this, [this, id, field] {
					Editing.reset();
					if (const auto act = Dispatch.Save) {
						act(id, field->text());
					}
				});
				connect(field, &QLineEdit::returnPressed, save, &QPushButton::click);
				connect(cancel, &QPushButton::clicked, this, [this, id] {
					Editing.reset();
					if (const auto act = Dispatch.Cancel) {
						act(id);
					}
				});
				line->addWidget(save);
				line->addWidget(cancel);
				column->addWidget(row);
				QTimer::singleShot(0, field, [field] { field->setFocus(Qt::OtherFocusReason); });
				continue;
			}
			if (entry.Sending) {
				line->setContentsMargins(8, 3, 4, 3);
			}
			auto* const label = new elided_label{ row };
			label->setObjectName(QStringLiteral("agentQueueLabel"));
			label->setText(entry.Held ? tr("%1 (being edited)").arg(entry.Label) : entry.Label);
			label->setTextFormat(Qt::PlainText);
			label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
			label->setToolTip(entry.Text.isEmpty() ? entry.Label : entry.Text);
			label->setForegroundRole(entry.Sending ? QPalette::PlaceholderText : QPalette::WindowText);
			line->addWidget(label, 1);
			if (entry.Uncertain) {
				// RFC-BPR-0126 AC04: whether the agent took it is not known.
				// The reviewer discards it; Resume does not send it again. The
				// label says so and elides, and Discard is an icon with a tip,
				// so the card is never wider than the drawer.
				label->setText(tr("%1 (may not have been delivered)").arg(entry.Label));
				auto* const discard = glyph(row, conversation_glyph::trash, QStringLiteral("agentQueueDiscard"), tr("Discard the message that may not have been delivered: %1").arg(entry.Label));
				discard->setToolTip(tr("Discard: it may not have been delivered"));
				connect(discard, &QPushButton::clicked, this, [this, id] {
					if (const auto act = Dispatch.Discard) {
						act(id);
					}
				});
				line->addWidget(discard);
			} else if (entry.Sending) {
				line->addSpacing(6);
				// RFC-BPR-0126: Sent means the agent's runtime took the
				// correction, not that the agent acted on it.
				auto* const sending = new QLabel{ entry.Sent ? tr("Sent to agent") : tr("Sending"), row };
				sending->setObjectName(QStringLiteral("agentQueueSending"));
				apply_agent_accent(*sending, palette());
				line->addWidget(sending);
			} else if (!entry.Held) {
				// RFC-BPR-0126 AC04: a message being edited in another window
				// showing this session has no controls here until Save or
				// Cancel there.
				if (!entry.Text.isEmpty()) {
					auto* const edit = glyph(row, conversation_glyph::edit, QStringLiteral("agentQueueEdit"), tr("Edit the queued message: %1").arg(entry.Label));
					edit->setToolTip(tr("Edit"));
					connect(edit, &QPushButton::clicked, this, [this, id] {
						Editing = id;
						if (const auto act = Dispatch.Edit) {
							act(id);
						}
					});
					line->addWidget(edit);
				}
				// Look prompts-v003 (owner: "Queued items need the ability to
				// say steer instead"): Steer sends it into the running turn
				// rather than after it (RFC-BPR-0126 Q14).
				// Look prompts-v004 (owner: "I like your steer icon I don't
				// think we need the word. Just add tooltips"): the icon alone.
				auto* const steer = glyph(row, conversation_glyph::send, QStringLiteral("agentQueueSteer"), tr("Steer the running turn with it instead of waiting: %1").arg(entry.Label));
				steer->setToolTip(tr("Steer: send it into the running turn instead of waiting"));
				connect(steer, &QPushButton::clicked, this, [this, id] {
					if (const auto act = Dispatch.Steer) {
						act(id);
					}
				});
				line->addWidget(steer);
				auto* const remove = glyph(row, conversation_glyph::trash, QStringLiteral("agentQueueRemove"), tr("Remove from the queue: %1").arg(entry.Label));
				remove->setToolTip(tr("Remove from the queue"));
				connect(remove, &QPushButton::clicked, this, [this, id] {
					if (const auto act = Dispatch.Remove) {
						act(id);
					}
				});
				line->addWidget(remove);
			}
			column->addWidget(row);
		}
		show();
	}

} // namespace bpr::qt
