#include "bpr/qt/review_cycle_share_page.hpp"

#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QInputMethodEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		auto text_of(const std::string& aBytes) -> QString
		{
			return QString::fromUtf8(aBytes.data(), static_cast<qsizetype>(aBytes.size()));
		}

		auto round_text(const core::cycle_round_id aRound) -> QString
		{
			return QStringLiteral("F%1").arg(aRound.value());
		}

		/// "1,414 bytes": grouped the same way whatever the system locale.
		auto bytes_text(const std::uint64_t aBytes) -> QString
		{
			auto digits = QString::number(aBytes);
			for (auto at = digits.size() - 3; at > 0; at -= 3) {
				digits.insert(at, QLatin1Char(','));
			}
			return digits + (aBytes == 1U ? QStringLiteral(" byte") : QStringLiteral(" bytes"));
		}

		auto count_text(const std::uint64_t aCount, const QString& aOne, const QString& aMany) -> QString
		{
			if (aCount == 0U) {
				return QStringLiteral("no %1").arg(aMany);
			}
			return QStringLiteral("%1 %2").arg(aCount).arg(aCount == 1U ? aOne : aMany);
		}

		auto route_label(const core::cycle_share_route aRoute) -> QString
		{
			switch (aRoute) {
			case core::cycle_share_route::copy_document:  return QStringLiteral("Copy to Clipboard");
			case core::cycle_share_route::save_document:  return QStringLiteral("Save to File…");
			case core::cycle_share_route::save_bundle:    return QStringLiteral("Save as Bundle…");
			case core::cycle_share_route::external_agent: return QStringLiteral("Use in External Agent…");
			case core::cycle_share_route::send_to_agent:  return QStringLiteral("Send to Agent…");
			}
			return { };
		}

		auto route_tip(const core::cycle_share_route aRoute) -> QString
		{
			switch (aRoute) {
			case core::cycle_share_route::copy_document:  return QStringLiteral("Freeze this round and copy its readable document, exactly as previewed. Clipboard history may keep it.");
			case core::cycle_share_route::save_document:  return QStringLiteral("Freeze this round and save its readable document to a file you choose. Synced folders may keep it.");
			case core::cycle_share_route::save_bundle:    return QStringLiteral("Freeze this round and save it as a bundle: the document, the exact round and its selected sources.");
			case core::cycle_share_route::external_agent: return QStringLiteral("Freeze this round and show how your own harness can read it. Nothing is sent or started.");
			case core::cycle_share_route::send_to_agent:  return QStringLiteral("Freeze this round and hand it to the installed agent adapter.");
			}
			return { };
		}

		auto refused_tip(const core::cycle_authorization aDecision) -> QString
		{
			switch (aDecision) {
			case core::cycle_authorization::channel_denied:       return QStringLiteral("Not allowed for this project's source by policy; no other way is used instead.");
			case core::cycle_authorization::channel_unconfigured: return QStringLiteral("Not allowed until it is configured for source of unknown origin.");
			default:                                              return QStringLiteral("Not allowed (%1).").arg(QString::fromLatin1(core::name(aDecision).data(), static_cast<qsizetype>(core::name(aDecision).size())));
			}
		}

		auto action_text(const core::cycle_round_action aAction) -> QString
		{
			switch (aAction) {
			case core::cycle_round_action::discuss:   return QStringLiteral("Discuss");
			case core::cycle_round_action::plan:      return QStringLiteral("Plan");
			case core::cycle_round_action::implement: return QStringLiteral("Implement");
			}
			return { };
		}

		/// Where a note attaches, in a few words.
		auto where_text(const core::cycle_feedback_target& aTarget) -> QString
		{
			if (const auto* anchor = std::get_if<core::cycle_source_anchor>(&aTarget)) {
				return text_of(anchor->Path);
			}
			if (const auto* file = std::get_if<core::cycle_file_target>(&aTarget)) {
				return QStringLiteral("%1 (whole file)").arg(text_of(file->Path));
			}
			if (const auto* module = std::get_if<core::cycle_module_target>(&aTarget)) {
				return QStringLiteral("Folder %1/").arg(text_of(module->Path));
			}
			return QStringLiteral("Whole cycle");
		}

		auto note_text(const application::review_cycle_note_entry& aNote) -> QString
		{
			auto body = text_of(aNote.Body).section(QLatin1Char('\n'), 0, 0).trimmed();
			if (body.isEmpty()) {
				body = aNote.Suggestions > 0U ? QStringLiteral("(suggestion only)") : QStringLiteral("(no text)");
			}
			auto text = QStringLiteral("%1 — %2").arg(where_text(aNote.Primary), body.left(160));
			if (aNote.Suggestions > 0U) {
				text += QStringLiteral(" · %1").arg(count_text(aNote.Suggestions, QStringLiteral("suggested change"), QStringLiteral("suggested changes")));
			}
			if (aNote.Lifecycle == core::private_thread_lifecycle::archived) {
				text += QStringLiteral(" · archived");
			}
			return text;
		}

		/// One observation in its own words: what was seen, never more.
		auto delivery_text(const core::cycle_delivery_record& aRecord) -> QString
		{
			const auto& seen   = aRecord.Observation;
			const auto  size   = seen.Output ? QStringLiteral(" · %1").arg(bytes_text(seen.Output->Bytes)) : QString{ };
			const auto  bundle = seen.Output && seen.Output->Format == core::cycle_round_file_kind::bundle;
			auto        what   = QString{ };
			switch (seen.State) {
			case core::cycle_delivery_state::copied:                 what = QStringLiteral("Copied to the clipboard"); break;
			case core::cycle_delivery_state::saved:                  what = QStringLiteral("Saved %1as %2").arg(bundle ? QStringLiteral("a bundle ") : QString{ }, text_of(seen.Destination)); break;
			case core::cycle_delivery_state::retrieved:              what = seen.Channel == core::cycle_disclosure_channel::terminal_output ? QStringLiteral("Printed in a terminal") : QStringLiteral("Read through the local interface"); break;
			case core::cycle_delivery_state::accepted_by_adapter:    what = QStringLiteral("Accepted by the agent adapter"); break;
			case core::cycle_delivery_state::recipient_acknowledged: what = QStringLiteral("The recipient acknowledged receiving it"); break;
			case core::cycle_delivery_state::failed:                 what = seen.Channel == core::cycle_disclosure_channel::clipboard ? QStringLiteral("Copy failed: there was no clipboard") : QStringLiteral("Delivery failed"); break;
			case core::cycle_delivery_state::uncertain:              what = seen.Channel == core::cycle_disclosure_channel::clipboard ? QStringLiteral("Copy not confirmed by the clipboard") : QStringLiteral("Delivery not confirmed"); break;
			}
			return QStringLiteral("%1 · %2%3").arg(round_text(seen.Round.Id), what, size);
		}

		auto refusal_text(const core::cycle_round_refusal& aRefusal) -> QString
		{
			switch (aRefusal.Problem) {
			case core::cycle_round_problem::empty:         return QStringLiteral("Include at least one note to share.");
			case core::cycle_round_problem::archived_item: return QStringLiteral("A note in this round is archived; leave it out to share.");
			case core::cycle_round_problem::oversized:     return QStringLiteral("This round is over its size budget; leave notes out, or share them as rounds you choose.");
			default:                                       return QStringLiteral("This round cannot be shared as it stands (%1).").arg(QString::fromLatin1(core::stable_name(aRefusal.Problem).data(), static_cast<qsizetype>(core::stable_name(aRefusal.Problem).size())));
			}
		}

		constexpr auto all_routes = std::array{ core::cycle_share_route::copy_document, core::cycle_share_route::save_document, core::cycle_share_route::save_bundle,
												core::cycle_share_route::external_agent, core::cycle_share_route::send_to_agent };
		constexpr auto item_role  = Qt::UserRole + 1;
	}

	review_cycle_share_page::review_cycle_share_page(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("cycleSharePage"));
		setAccessibleName(QStringLiteral("Compose feedback"));
		setAccessibleDescription(QStringLiteral("Choose the notes for one round, read exactly what is shared, then copy or save it"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(8, 6, 8, 6);
		column->setSpacing(6);

		// Back, then the round, as a pull request's Compose review page.
		auto* const header = new QHBoxLayout{ };
		Back               = new QPushButton{ QStringLiteral("Back"), this };
		Back->setObjectName(QStringLiteral("cycleShareBack"));
		Back->setAccessibleDescription(QStringLiteral("Return to the source; the round's draft stays as it is"));
		Back->setToolTip(QStringLiteral("Back to the source; the draft is kept (Esc)"));
		header->addWidget(Back);
		Title = new QLabel{ this };
		Title->setObjectName(QStringLiteral("cycleShareTitle"));
		Title->setTextFormat(Qt::PlainText);
		Title->setWordWrap(true);
		auto font = Title->font();
		font.setBold(true);
		Title->setFont(font);
		header->addWidget(Title, 1);
		column->addLayout(header);

		// Share feedback: Copy and Save first, then what else this installation
		// reaches, on a row of its own so a narrow window keeps them all.
		auto* const shareLabel = new QLabel{ QStringLiteral("Share feedback"), this };
		shareLabel->setObjectName(QStringLiteral("cycleShareLabel"));
		shareLabel->setWordWrap(true);
		column->addWidget(shareLabel);
		auto* const primary = new QHBoxLayout{ };
		OptionalRoutes      = new QWidget{ this };
		OptionalRoutes->setObjectName(QStringLiteral("cycleShareOptionalRoutes"));
		auto* const optional = new QHBoxLayout{ OptionalRoutes };
		optional->setContentsMargins(0, 0, 0, 0);
		for (const auto route : all_routes) {
			const auto  first  = route == core::cycle_share_route::copy_document || route == core::cycle_share_route::save_document || route == core::cycle_share_route::save_bundle;
			auto* const button = new QPushButton{ route_label(route), first ? this : OptionalRoutes };
			button->setObjectName(QStringLiteral("cycleShare_%1").arg(QString::fromLatin1(core::stable_name(route).data(), static_cast<qsizetype>(core::stable_name(route).size()))));
			button->setAccessibleName(route_label(route).remove(QStringLiteral("…")));
			button->hide();
			(first ? primary : optional)->addWidget(button);
			Routes.at(static_cast<std::size_t>(route)) = button;
			connect(button, &QPushButton::clicked, this, [this, route] {
				if (Dispatch.Share) {
					Dispatch.Share(route);
				}
			});
		}
		primary->addStretch(1);
		optional->addStretch(1);
		column->addLayout(primary);
		OptionalRoutes->hide();
		column->addWidget(OptionalRoutes);

		Facts = new QLabel{ this };
		Facts->setObjectName(QStringLiteral("cycleShareFacts"));
		Facts->setTextFormat(Qt::PlainText);
		Facts->setWordWrap(true);
		Facts->setAccessibleName(QStringLiteral("What this round holds"));
		column->addWidget(Facts);

		ReplaceRow                = new QWidget{ this };
		auto* const replaceLayout = new QHBoxLayout{ ReplaceRow };
		replaceLayout->setContentsMargins(0, 0, 0, 0);
		ReplaceText = new QLabel{ ReplaceRow };
		ReplaceText->setObjectName(QStringLiteral("cycleShareReplaceText"));
		ReplaceText->setTextFormat(Qt::PlainText);
		ReplaceText->setWordWrap(true);
		replaceLayout->addWidget(ReplaceText, 1);
		ReplaceIt = new QPushButton{ QStringLiteral("Replace It"), ReplaceRow };
		ReplaceIt->setObjectName(QStringLiteral("cycleShareReplace"));
		KeepIt = new QPushButton{ QStringLiteral("Keep It"), ReplaceRow };
		KeepIt->setObjectName(QStringLiteral("cycleShareKeep"));
		replaceLayout->addWidget(ReplaceIt);
		replaceLayout->addWidget(KeepIt);
		ReplaceRow->hide();
		column->addWidget(ReplaceRow);
		connect(ReplaceIt, &QPushButton::clicked, this, [this] {
			if (Dispatch.Replace) {
				Dispatch.Replace(true);
			}
		});
		connect(KeepIt, &QPushButton::clicked, this, [this] {
			if (Dispatch.Replace) {
				Dispatch.Replace(false);
			}
		});

		Status = new QLabel{ this };
		Status->setObjectName(QStringLiteral("cycleShareStatus"));
		Status->setTextFormat(Qt::PlainText);
		Status->setWordWrap(true);
		Status->setAccessibleName(QStringLiteral("Last share"));
		Status->hide();
		column->addWidget(Status);
		Invocation = new QLineEdit{ this };
		Invocation->setObjectName(QStringLiteral("cycleShareInvocation"));
		Invocation->setAccessibleName(QStringLiteral("Ask your harness to run"));
		Invocation->setReadOnly(true);
		Invocation->hide();
		column->addWidget(Invocation);

		auto* const body = new QSplitter{ Qt::Horizontal, this };
		body->setObjectName(QStringLiteral("cycleShareBody"));
		body->setChildrenCollapsible(false);
		auto* const left       = new QWidget{ body };
		auto* const leftColumn = new QVBoxLayout{ left };
		leftColumn->setContentsMargins(0, 0, 0, 0);
		auto* const assignment = new QHBoxLayout{ };
		auto* const actionName = new QLabel{ QStringLiteral("Asks to"), left };
		assignment->addWidget(actionName);
		Action = new QComboBox{ left };
		Action->setObjectName(QStringLiteral("cycleShareAction"));
		Action->setAccessibleName(QStringLiteral("What the round asks for"));
		for (const auto action : { core::cycle_round_action::implement, core::cycle_round_action::plan, core::cycle_round_action::discuss }) {
			Action->addItem(action_text(action), static_cast<int>(action));
		}
		actionName->setBuddy(Action);
		assignment->addWidget(Action);
		assignment->addStretch(1);
		leftColumn->addLayout(assignment);
		Goal = new QLineEdit{ left };
		Goal->setObjectName(QStringLiteral("cycleShareGoal"));
		Goal->setAccessibleName(QStringLiteral("Goal"));
		Goal->setPlaceholderText(QStringLiteral("Goal for the whole round (optional)"));
		leftColumn->addWidget(Goal);
		Goal->installEventFilter(this);
		auto* const itemsLabel = new QLabel{ QStringLiteral("Notes in this round, in order"), left };
		itemsLabel->setWordWrap(true);
		leftColumn->addWidget(itemsLabel);
		Items = new QListWidget{ left };
		Items->setObjectName(QStringLiteral("cycleShareItems"));
		Items->setAccessibleName(QStringLiteral("Notes in this round"));
		Items->setWordWrap(true);
		Items->setTextElideMode(Qt::ElideNone);
		Items->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		itemsLabel->setBuddy(Items);
		leftColumn->addWidget(Items, 1);
		auto* const moves = new QHBoxLayout{ };
		MoveUp            = new QPushButton{ QStringLiteral("Move Up"), left };
		MoveUp->setObjectName(QStringLiteral("cycleShareMoveUp"));
		MoveDown = new QPushButton{ QStringLiteral("Move Down"), left };
		MoveDown->setObjectName(QStringLiteral("cycleShareMoveDown"));
		moves->addWidget(MoveUp);
		moves->addWidget(MoveDown);
		moves->addStretch(1);
		leftColumn->addLayout(moves);
		auto* const sharedLabel = new QLabel{ QStringLiteral("Shared so far"), left };
		sharedLabel->setWordWrap(true);
		leftColumn->addWidget(sharedLabel);
		Deliveries = new QListWidget{ left };
		Deliveries->setObjectName(QStringLiteral("cycleShareDeliveries"));
		Deliveries->setAccessibleName(QStringLiteral("Shared so far"));
		Deliveries->setWordWrap(true);
		Deliveries->setSelectionMode(QAbstractItemView::NoSelection);
		Deliveries->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		sharedLabel->setBuddy(Deliveries);
		leftColumn->addWidget(Deliveries);
		auto* const meaning = new QLabel{ QStringLiteral("These say only what happened to the bytes. None of them means the round was received, read, acted on or accepted."), left };
		meaning->setObjectName(QStringLiteral("cycleShareMeaning"));
		meaning->setTextFormat(Qt::PlainText);
		meaning->setWordWrap(true);
		leftColumn->addWidget(meaning);

		auto* const right       = new QWidget{ body };
		auto* const rightColumn = new QVBoxLayout{ right };
		rightColumn->setContentsMargins(0, 0, 0, 0);
		auto* const previewLabel = new QLabel{ QStringLiteral("Preview: exactly what Copy and Save send"), right };
		previewLabel->setWordWrap(true);
		rightColumn->addWidget(previewLabel);
		Preview = new QPlainTextEdit{ right };
		Preview->setObjectName(QStringLiteral("cycleSharePreview"));
		Preview->setAccessibleName(QStringLiteral("Preview of the shared document"));
		Preview->setReadOnly(true);
		Preview->setLineWrapMode(QPlainTextEdit::WidgetWidth);
		Preview->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		previewLabel->setBuddy(Preview);
		rightColumn->addWidget(Preview, 1);
		body->addWidget(left);
		body->addWidget(right);
		body->setStretchFactor(0, 2);
		body->setStretchFactor(1, 3);
		column->addWidget(body, 1);

		connect(Back, &QPushButton::clicked, this, [this] {
			if (Dispatch.Back) {
				Dispatch.Back();
			}
		});
		connect(Action, &QComboBox::activated, this, [this](const int aIndex) {
			if (!Presenting && Dispatch.Edit) {
				Dispatch.Edit(core::set_round_action{ static_cast<core::cycle_round_action>(Action->itemData(aIndex).toInt()) });
			}
		});
		connect(Goal, &QLineEdit::editingFinished, this, [this] {
			if (!Presenting) {
				save_draft_editor();
			}
		});
		connect(Items, &QListWidget::itemChanged, this, [this](QListWidgetItem* aItem) {
			if (Presenting || aItem == nullptr || !Dispatch.Edit) {
				return;
			}
			const auto item = core::cycle_feedback_item_id{ aItem->data(item_role).toULongLong() };
			Dispatch.Edit(core::include_round_item{ .Item = item, .Include = aItem->checkState() == Qt::Checked });
		});
		connect(Items, &QListWidget::currentRowChanged, this, [this](int) { refresh_moves(); });
		connect(MoveUp, &QPushButton::clicked, this, [this] { move_current(true); });
		connect(MoveDown, &QPushButton::clicked, this, [this] { move_current(false); });
	}

	void review_cycle_share_page::set_dispatch(review_cycle_share_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void review_cycle_share_page::set_code_font(const QFont& aFont)
	{
		Preview->setFont(aFont);
	}

	void review_cycle_share_page::present(const application::review_cycle_share_view& aShare, const std::vector<application::review_cycle_note_entry>& aNotes, const bool aIdle)
	{
		Presenting       = true;
		Idle             = aIdle;
		const auto round = round_text(aShare.Round);
		if (aShare.Frozen) {
			Title->setText(QStringLiteral("Feedback round %1 · frozen; sharing it again sends the same round").arg(round));
		} else if (aShare.Amends) {
			Title->setText(QStringLiteral("Feedback round %1 · amends %2, which stays as it was shared · not shared yet").arg(round, round_text(*aShare.Amends)));
		} else {
			Title->setText(QStringLiteral("Feedback round %1 · not shared yet").arg(round));
		}

		// The routes, in order; one this installation cannot reach is not shown.
		auto optional = false;
		for (const auto route : all_routes) {
			auto* const button = Routes.at(static_cast<std::size_t>(route));
			const auto  offer  = std::ranges::find(aShare.Routes, route, &core::cycle_share_route_offer::Route);
			button->setVisible(offer != aShare.Routes.end());
			if (offer == aShare.Routes.end()) {
				continue;
			}
			optional = optional || button->parentWidget() == OptionalRoutes;
			button->setEnabled(offer->offered() && !aShare.Refusal && aIdle);
			button->setToolTip(offer->offered() ? route_tip(route) : refused_tip(offer->Decision));
			button->setAccessibleDescription(button->toolTip());
		}

		OptionalRoutes->setVisible(optional);

		auto facts = QStringList{
			count_text(aShare.Items, QStringLiteral("note"), QStringLiteral("notes")),
			count_text(aShare.Suggestions, QStringLiteral("suggested change"), QStringLiteral("suggested changes")),
			count_text(aShare.Sources, QStringLiteral("source file"), QStringLiteral("source files")),
			QStringLiteral("%1 of Markdown").arg(bytes_text(aShare.DocumentBytes)),
		};
		if (aShare.Digest) {
			facts.append(QStringLiteral("round digest %1").arg(QString::fromLatin1(QByteArray{ reinterpret_cast<const char*>(aShare.Digest->bytes().data()), 6 }.toHex())));
		}
		auto line = facts.join(QStringLiteral(" · "));
		if (aShare.Refusal) {
			line += QStringLiteral("\n") + refusal_text(*aShare.Refusal);
		}
		Facts->setText(line);

		ReplaceRow->setVisible(aShare.Replace.has_value());
		if (aShare.Replace) {
			ReplaceText->setText(QStringLiteral("%1 already holds other content (%2). Replace exactly that file, or keep it and choose another.").arg(text_of(aShare.Replace->Name), bytes_text(aShare.Replace->Size)));
		}
		Status->setText(text_of(aShare.Status));
		Status->setVisible(!aShare.Status.empty());
		Invocation->setText(text_of(aShare.Invocation));
		Invocation->setVisible(!aShare.Invocation.empty());

		Action->setCurrentIndex(Action->findData(static_cast<int>(aShare.Selection.Action)));
		ShownGoal = text_of(aShare.Selection.Goal);
		if (aShare.UnsavedGoal && !Goal->isModified()) {
			// Refused because the draft changed elsewhere first: kept to save again.
			Goal->setText(text_of(*aShare.UnsavedGoal));
			Goal->setModified(true);
		} else if (!aShare.UnsavedGoal && (!Goal->hasFocus() || !Goal->isModified())) {
			Goal->setText(ShownGoal);
			Goal->setModified(false);
		}
		Action->setEnabled(aIdle);
		Goal->setEnabled(aIdle || Goal->hasFocus());
		Items->setEnabled(aIdle);
		rebuild_items(aShare, aNotes);
		rebuild_deliveries(aShare);

		const auto document = aShare.DocumentShown ? text_of(aShare.Document)
												   : QStringLiteral("The document is %1, too large to show here. Nothing is cut short: Save to File keeps all of it.").arg(bytes_text(aShare.DocumentBytes));
		if (Preview->toPlainText() != document) {
			Preview->setPlainText(document);
		}
		refresh_moves();
		Presenting = false;
	}

	void review_cycle_share_page::rebuild_items(const application::review_cycle_share_view& aShare, const std::vector<application::review_cycle_note_entry>& aNotes)
	{
		// The round's notes in its order, then open notes left out, in item order.
		auto rows = std::vector<std::pair<const application::review_cycle_note_entry*, bool>>{ };
		for (const auto& selected : aShare.Selection.Items) {
			const auto found = std::ranges::find(aNotes, selected.Item, [](const application::review_cycle_note_entry& aNote) { return aNote.Item.Id; });
			if (found != aNotes.end()) {
				rows.emplace_back(&*found, true);
			}
		}
		for (const auto& note : aNotes) {
			const auto chosen = std::ranges::any_of(aShare.Selection.Items, [&](const core::cycle_round_item_selection& aItem) { return aItem.Item == note.Item.Id; });
			if (!chosen && note.Lifecycle == core::private_thread_lifecycle::open) {
				rows.emplace_back(&note, false);
			}
		}
		auto shown = std::vector<std::pair<core::cycle_feedback_item_id, bool>>{ };
		auto texts = std::vector<QString>{ };
		for (std::size_t index = 0; index < rows.size(); ++index) {
			shown.emplace_back(rows[index].first->Item.Id, rows[index].second);
			const auto number = rows[index].second ? QStringLiteral("%1. ").arg(index + 1U) : QString{ };
			texts.push_back(number + note_text(*rows[index].first));
		}
		auto current = std::optional<core::cycle_feedback_item_id>{ };
		if (Items->currentItem() != nullptr) {
			current = core::cycle_feedback_item_id{ Items->currentItem()->data(item_role).toULongLong() };
		}
		auto same = shown == ShownItems && static_cast<std::size_t>(Items->count()) == texts.size();
		for (std::size_t index = 0; same && index < texts.size(); ++index) {
			same = Items->item(static_cast<int>(index))->text() == texts[index];
		}
		if (same) {
			// A check the host did not take shows what the round holds.
			for (std::size_t index = 0; index < rows.size(); ++index) {
				Items->item(static_cast<int>(index))->setCheckState(rows[index].second ? Qt::Checked : Qt::Unchecked);
			}
			return;
		}
		Items->clear();
		for (std::size_t index = 0; index < rows.size(); ++index) {
			auto* const row = new QListWidgetItem{ texts[index], Items };
			row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
			row->setCheckState(rows[index].second ? Qt::Checked : Qt::Unchecked);
			row->setData(item_role, static_cast<qulonglong>(rows[index].first->Item.Id.value()));
			row->setData(Qt::AccessibleDescriptionRole, rows[index].second ? QStringLiteral("In this round") : QStringLiteral("Left out of this round"));
			if (current && *current == rows[index].first->Item.Id) {
				Items->setCurrentItem(row);
			}
		}
		ShownItems = std::move(shown);
	}

	void review_cycle_share_page::rebuild_deliveries(const application::review_cycle_share_view& aShare)
	{
		// Repeats of the same observation read as one line with a count.
		auto lines = std::vector<QString>{ };
		auto times = std::vector<int>{ };
		for (const auto& record : aShare.Deliveries) {
			const auto text = delivery_text(record);
			if (!lines.empty() && lines.back() == text) {
				++times.back();
			} else {
				lines.push_back(text);
				times.push_back(1);
			}
		}
		for (std::size_t index = 0; index < lines.size(); ++index) {
			if (times[index] > 1) {
				lines[index] += QStringLiteral(" · %1 times").arg(times[index]);
			}
		}
		if (lines.empty()) {
			lines.push_back(QStringLiteral("Nothing shared yet."));
		}
		if (lines == ShownDeliveries) {
			return;
		}
		Deliveries->clear();
		for (const auto& line : lines) {
			Deliveries->addItem(line);
		}
		ShownDeliveries = std::move(lines);
	}

	void review_cycle_share_page::refresh_moves()
	{
		const auto row      = Items->currentRow();
		const auto included = row >= 0 && static_cast<std::size_t>(row) < ShownItems.size() && ShownItems[static_cast<std::size_t>(row)].second;
		const auto count    = static_cast<int>(std::ranges::count_if(ShownItems, [](const auto& aItem) { return aItem.second; }));
		MoveUp->setEnabled(Idle && included && row > 0);
		MoveDown->setEnabled(Idle && included && row + 1 < count);
	}

	void review_cycle_share_page::move_current(const bool aEarlier)
	{
		const auto row = Items->currentRow();
		if (row < 0 || static_cast<std::size_t>(row) >= ShownItems.size() || !Dispatch.Edit) {
			return;
		}
		Dispatch.Edit(core::move_round_item{ .Item = ShownItems[static_cast<std::size_t>(row)].first, .Earlier = aEarlier });
	}

	void review_cycle_share_page::focus_first()
	{
		for (auto* const button : Routes) {
			if (button->isVisible() && button->isEnabled()) {
				button->setFocus(Qt::OtherFocusReason);
				return;
			}
		}
		Back->setFocus(Qt::OtherFocusReason);
	}

	auto review_cycle_share_page::back_button() const noexcept -> QPushButton*
	{
		return Back;
	}
	auto review_cycle_share_page::title() const noexcept -> QLabel*
	{
		return Title;
	}
	auto review_cycle_share_page::facts() const noexcept -> QLabel*
	{
		return Facts;
	}
	auto review_cycle_share_page::status() const noexcept -> QLabel*
	{
		return Status;
	}
	auto review_cycle_share_page::route_button(const core::cycle_share_route aRoute) const noexcept -> QPushButton*
	{
		return Routes.at(static_cast<std::size_t>(aRoute));
	}
	auto review_cycle_share_page::action_box() const noexcept -> QComboBox*
	{
		return Action;
	}
	auto review_cycle_share_page::draft_editor() const -> core::cycle_draft_editor
	{
		if (GoalComposing) {
			return core::cycle_draft_editor::composing;
		}
		return Goal->isModified() && Goal->text() != ShownGoal ? core::cycle_draft_editor::unsaved : core::cycle_draft_editor::settled;
	}

	void review_cycle_share_page::save_draft_editor()
	{
		if (GoalComposing || !Dispatch.Edit || !Goal->isModified()) {
			return;
		}
		Goal->setModified(false);
		const auto bytes = Goal->text().toUtf8();
		Dispatch.Edit(core::set_round_goal{ std::string{ bytes.constData(), static_cast<std::size_t>(bytes.size()) } });
	}

	auto review_cycle_share_page::eventFilter(QObject* aWatched, QEvent* aEvent) -> bool
	{
		if (aWatched == Goal) {
			if (aEvent->type() == QEvent::InputMethod) {
				// Preedit text is not committed text: another interface waits for it.
				GoalComposing = !static_cast<QInputMethodEvent*>(aEvent)->preeditString().isEmpty();
			} else if (aEvent->type() == QEvent::FocusOut || aEvent->type() == QEvent::Hide) {
				GoalComposing = false;
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	auto review_cycle_share_page::goal() const noexcept -> QLineEdit*
	{
		return Goal;
	}
	auto review_cycle_share_page::items() const noexcept -> QListWidget*
	{
		return Items;
	}
	auto review_cycle_share_page::move_up_button() const noexcept -> QPushButton*
	{
		return MoveUp;
	}
	auto review_cycle_share_page::move_down_button() const noexcept -> QPushButton*
	{
		return MoveDown;
	}
	auto review_cycle_share_page::preview() const noexcept -> QPlainTextEdit*
	{
		return Preview;
	}
	auto review_cycle_share_page::deliveries() const noexcept -> QListWidget*
	{
		return Deliveries;
	}
	auto review_cycle_share_page::invocation() const noexcept -> QLineEdit*
	{
		return Invocation;
	}
	auto review_cycle_share_page::replace_button() const noexcept -> QPushButton*
	{
		return ReplaceIt;
	}
	auto review_cycle_share_page::keep_button() const noexcept -> QPushButton*
	{
		return KeepIt;
	}
}
