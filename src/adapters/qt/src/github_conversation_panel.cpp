#include "bpr/qt/github_conversation_panel.hpp"

#include "bpr/qt/github_conversation_delegate.hpp"
#include "bpr/qt/github_conversation_model.hpp"

#include "github_presentation_text.hpp"

#include <QAction>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace bpr::qt {
	namespace {
		/// \brief Keeps the threads whose root is anchored in one file.
		///
		/// \details A reply carries no anchor — only a thread's root names a
		/// path — so membership is decided at the root and inherited. Filtering
		/// node by node would keep a root without its replies, or keep a reply
		/// whose root was dropped and present it as a top-level remark.
		///
		/// Roots are found by following `ParentId`, which is single and optional,
		/// so every chain terminates. A parent naming a node that is not in the
		/// projection is treated as a root: the alternative is discarding a
		/// comment because its ancestor is missing, and a visible orphan is
		/// better than a silent omission.
		[[nodiscard]] auto anchored_in(
			const std::vector<application::github_conversation_projection>&
									  aConversations,
			const core::file_identity aFile)
			-> std::vector<application::github_conversation_projection>
		{
			auto byId = std::unordered_map<std::string, std::size_t>{ };
			byId.reserve(aConversations.size());
			for (auto index = std::size_t{ 0U }; index < aConversations.size();
				 ++index) {
				byId.emplace(aConversations[index].Id, index);
			}

			const auto rooted_in_file = [&](std::size_t aIndex) {
				// Bounded by the node count, so a projection that somehow
				// contained a cycle cannot spin here.
				for (auto step = std::size_t{ 0U }; step <= aConversations.size();
					 ++step) {
					const auto& node = aConversations[aIndex];
					if (!node.ParentId.has_value()) {
						return !node.Path.empty()
							&& core::file_identity_of(node.Path) == aFile;
					}
					const auto parent = byId.find(*node.ParentId);
					if (parent == byId.end()) {
						return !node.Path.empty()
							&& core::file_identity_of(node.Path) == aFile;
					}
					aIndex = parent->second;
				}
				return false;
			};

			auto kept = std::vector<application::github_conversation_projection>{ };
			kept.reserve(aConversations.size());
			for (auto index = std::size_t{ 0U }; index < aConversations.size();
				 ++index) {
				if (rooted_in_file(index)) {
					kept.push_back(aConversations[index]);
				}
			}
			return kept;
		}
	} // namespace

	github_conversation_panel::github_conversation_panel(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("pullRequestConversationGroup"));
		setAccessibleName(QStringLiteral("Read-only GitHub conversations"));

		auto* const layout = new QVBoxLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);

		AvailabilityLabel = new QLabel{ this };
		AvailabilityLabel->setObjectName(
			QStringLiteral("reviewThreadAvailability"));
		AvailabilityLabel->setAccessibleName(
			QStringLiteral("Review-thread availability"));
		AvailabilityLabel->setWordWrap(true);
		detail::set_plain_text_label(AvailabilityLabel);

		auto* const header = new QHBoxLayout;
		header->addWidget(AvailabilityLabel, 1);
		RevealButton = new QToolButton{ this };
		RevealButton->setObjectName(QStringLiteral("showReviewThreadsButton"));
		RevealButton->setAccessibleName(QStringLiteral("Show review threads"));
		RevealButton->setFocusPolicy(Qt::StrongFocus);
		RevealButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		header->addWidget(RevealButton);
		layout->addLayout(header);

		Stack = new QStackedWidget{ this };
		Stack->setObjectName(QStringLiteral("githubConversationState"));
		auto* const empty = new QWidget{ Stack };
		empty->setObjectName(QStringLiteral("githubConversationEmptyState"));
		Stack->addWidget(empty);

		Model = new github_conversation_model{ this };
		View  = new QTreeView{ Stack };
		View->setObjectName(QStringLiteral("githubConversationView"));
		View->setAccessibleName(QStringLiteral("Read-only GitHub conversations"));
		View->setAccessibleDescription(
			QStringLiteral("Reviews, review threads, and pull request comments"));
		View->setModel(Model);
		View->setSelectionBehavior(QAbstractItemView::SelectRows);
		View->setSelectionMode(QAbstractItemView::SingleSelection);
		View->setEditTriggers(QAbstractItemView::NoEditTriggers);
		View->setRootIsDecorated(true);
		View->setItemsExpandable(true);
		View->setWordWrap(true);
		// Rows are as tall as their comment, so no two are alike and none of
		// them can be assumed uniform.
		View->setUniformRowHeights(false);
		View->setTextElideMode(Qt::ElideNone);
		View->header()->hide();
		View->setItemDelegate(new github_conversation_delegate{ View });
		Stack->addWidget(View);
		layout->addWidget(Stack, 1);

		setTabOrder(RevealButton, View);
	}

	void github_conversation_panel::adopt(
		const std::vector<application::github_conversation_projection>&
				aConversations,
		QString aAvailabilityText)
	{
		// Captured before the model changes. Restoring by identity rather than
		// by row is what keeps a reviewer on the comment they were reading when
		// a reply lands above it.
		const auto selected = Model->id_for_index(View->currentIndex());

		Retained             = aConversations;
		RetainedAvailability = aAvailabilityText;
		AvailabilityLabel->setText(std::move(aAvailabilityText));
		const auto shown = AnchorFile.has_value()
			? anchored_in(aConversations, *AnchorFile)
			: aConversations;
		Model->apply(shown);
		Stack->setCurrentIndex(shown.empty() ? 0 : 1);
		View->doItemsLayout();
		if (ExpandOnNextProjection && !shown.empty()) {
			View->expandAll();
			ExpandOnNextProjection = false;
		}
		if (!selected.empty()) {
			const auto restored = Model->index_for_id(selected);
			if (restored.isValid()) {
				// Blocked, because restoring a selection is not the reviewer
				// making one, and anything listening should not be told it was.
				const QSignalBlocker blocker{ View->selectionModel() };
				View->setCurrentIndex(restored);
			}
		}
	}

	void github_conversation_panel::clear(QString aAvailabilityText)
	{
		ExpandOnNextProjection = true;
		Retained.clear();
		RetainedAvailability.clear();
		AvailabilityLabel->setText(std::move(aAvailabilityText));
		Model->clear();
		Stack->setCurrentIndex(0);
	}

	void github_conversation_panel::set_anchor_file(
		std::optional<core::file_identity> aFile)
	{
		if (AnchorFile == aFile) {
			return;
		}
		AnchorFile = std::move(aFile);
		// Re-filtered from what was retained rather than waiting for the next
		// projection. Moving between files is a reviewer gesture and the
		// drawer has to follow it, not follow the network.
		if (!Retained.empty()) {
			// Copied deliberately. Passing `Retained` straight back in would
			// make `adopt`'s first act a self-assignment, which is legal and
			// still the kind of thing that becomes a defect the moment someone
			// adds a clear() to the top of it.
			const auto retained     = Retained;
			const auto availability = RetainedAvailability;
			adopt(retained, availability);
		}
	}

	auto github_conversation_panel::anchor_file() const noexcept
		-> const std::optional<core::file_identity>&
	{
		return AnchorFile;
	}

	void github_conversation_panel::set_reveal_action(QAction* const aAction)
	{
		RevealButton->setDefaultAction(aAction);
		// setDefaultAction adopts the action's name, and the action names the
		// gesture while the button names the control. Restored so the two do
		// not answer to one accessible name between them.
		RevealButton->setAccessibleName(QStringLiteral("Show review threads"));
	}

	void github_conversation_panel::relayout()
	{
		View->doItemsLayout();
	}

	auto github_conversation_panel::view() const noexcept -> QTreeView*
	{
		return View;
	}

	auto github_conversation_panel::model() const noexcept
		-> github_conversation_model*
	{
		return Model;
	}

	auto github_conversation_panel::availability_label() const noexcept -> QLabel*
	{
		return AvailabilityLabel;
	}

	auto github_conversation_panel::reveal_button() const noexcept -> QToolButton*
	{
		return RevealButton;
	}
} // namespace bpr::qt
