#include "bpr/qt/github_conversation_model.hpp"

#include "bpr/qt/feedback_label_marks.hpp"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QVariant>

#include <algorithm>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace bpr::qt {
	namespace {
		using conversation = application::github_conversation_projection;

		[[nodiscard]] auto text(const std::string& aValue) -> QString
		{
			return QString::fromUtf8(
				aValue.data(), static_cast<qsizetype>(aValue.size()));
		}

		[[nodiscard]] auto review_state_text(
			const application::github_review_state_projection aState) -> QString
		{
			switch (aState) {
			case application::github_review_state_projection::pending:
				return QStringLiteral("Pending review");
			case application::github_review_state_projection::commented:
				return QStringLiteral("Commented");
			case application::github_review_state_projection::approved:
				return QStringLiteral("Approved");
			case application::github_review_state_projection::changes_requested:
				return QStringLiteral("Changes requested");
			case application::github_review_state_projection::dismissed:
				return QStringLiteral("Dismissed review");
			case application::github_review_state_projection::none:
				return QStringLiteral("Review");
			}
			return QStringLiteral("Review");
		}

		[[nodiscard]] auto location_text(const conversation& aValue) -> QString
		{
			auto result = text(aValue.Path);
			if (aValue.Line.has_value()) {
				if (!result.isEmpty()) {
					result.append(QLatin1Char(':'));
				}
				result.append(QString::number(*aValue.Line));
				if (aValue.DiffSide.has_value()) {
					if (const auto side = github_conversation_source_side(
							static_cast<int>(*aValue.DiffSide));
						side.has_value()) {
						result.append(*side == core::diff_side::left
										  ? QStringLiteral(" (base/left side)")
										  : QStringLiteral(" (head/right side)"));
					}
				}
			}
			return result;
		}

		[[nodiscard]] auto anchor_text(const conversation& aValue) -> QString
		{
			if (aValue.Kind == application::github_conversation_kind::review
				|| aValue.Kind == application::github_conversation_kind::issue_comment) {
				return { };
			}
			switch (aValue.AnchorAvailability) {
			case application::github_conversation_anchor_availability::current:
				return { };
			case application::github_conversation_anchor_availability::selected_comparison_mismatch:
				return QStringLiteral(
					"Anchor unavailable: selected comparison does not expose the current pull-request diff revision for this side");
			case application::github_conversation_anchor_availability::selected_line_out_of_bounds:
				return QStringLiteral(
					"Anchor unavailable: line is outside the selected comparison");
			case application::github_conversation_anchor_availability::outdated_revision:
				return aValue.OriginalLine.has_value()
					? QStringLiteral("Anchor unavailable: outdated revision (original line %1)")
						  .arg(*aValue.OriginalLine)
					: QStringLiteral("Anchor unavailable: outdated revision");
			case application::github_conversation_anchor_availability::file_level:
				return QStringLiteral("File-level thread; no source line anchor");
			case application::github_conversation_anchor_availability::missing_side:
				return QStringLiteral("Anchor unavailable: diff side is missing");
			case application::github_conversation_anchor_availability::missing_current_line:
				return QStringLiteral("Anchor unavailable: current line is missing");
			}
			return QStringLiteral("Anchor unavailable");
		}

		[[nodiscard]] auto author_text(const conversation& aValue) -> QString
		{
			if (aValue.AuthorLogin.empty()) {
				return QStringLiteral("Unknown author");
			}
			return QStringLiteral("@%1").arg(text(aValue.AuthorLogin));
		}

		[[nodiscard]] auto display_text(const conversation& aValue) -> QString
		{
			QString heading;
			switch (aValue.Kind) {
			case application::github_conversation_kind::review:
				heading = QStringLiteral("%1 by %2")
							  .arg(review_state_text(aValue.ReviewState),
								   author_text(aValue));
				break;
			case application::github_conversation_kind::review_thread:
				heading = aValue.Resolved
					? QStringLiteral("Resolved review thread")
					: QStringLiteral("Open review thread");
				if (const auto location = location_text(aValue); !location.isEmpty()) {
					heading.append(QStringLiteral(" — %1").arg(location));
				}
				if (aValue.Outdated) {
					heading.append(QStringLiteral(" — Outdated"));
				}
				break;
			case application::github_conversation_kind::review_comment:
				heading = QStringLiteral("Review comment by %1").arg(author_text(aValue));
				break;
			case application::github_conversation_kind::issue_comment:
				heading = QStringLiteral("Pull request comment by %1").arg(author_text(aValue));
				break;
			}

			if (aValue.Deleted) {
				heading.append(QStringLiteral("\nDeleted comment"));
			} else if (!aValue.Body.empty()) {
				heading.append(QLatin1Char('\n'));
				heading.append(text(aValue.ReviewerProse.value_or(aValue.Body)));
			}

			if (aValue.Kind != application::github_conversation_kind::review_thread) {
				if (const auto location = location_text(aValue); !location.isEmpty()) {
					heading.append(QStringLiteral("\n%1").arg(location));
				}
				if (aValue.Outdated) {
					heading.append(QStringLiteral("\nOutdated"));
				}
			}
			if (const auto anchor = anchor_text(aValue); !anchor.isEmpty()) {
				heading.append(QStringLiteral("\n%1").arg(anchor));
			}
			return heading;
		}

		[[nodiscard]] auto description_text(const conversation& aValue) -> QString
		{
			QStringList facts;
			if (aValue.Resolved) {
				facts.push_back(QStringLiteral("Resolved"));
			}
			if (aValue.Outdated) {
				facts.push_back(QStringLiteral("Outdated"));
			}
			if (aValue.Deleted) {
				facts.push_back(QStringLiteral("Deleted"));
			}
			if (aValue.ParentId.has_value()) {
				facts.push_back(QStringLiteral("Reply in a review thread"));
			}
			if (const auto anchor = anchor_text(aValue); !anchor.isEmpty()) {
				facts.push_back(anchor);
			}
			return facts.join(QStringLiteral(", "));
		}

		[[nodiscard]] auto changed_roles() -> const QList<int>&
		{
			static const QList<int> roles{
				Qt::DisplayRole,
				Qt::AccessibleTextRole,
				Qt::AccessibleDescriptionRole,
				github_conversation_model::parent_identity_role,
				github_conversation_model::kind_role,
				github_conversation_model::author_login_role,
				github_conversation_model::body_role,
				github_conversation_model::feedback_label_role,
				github_conversation_model::feedback_blocking_role,
				github_conversation_model::feedback_name_role,
				github_conversation_model::feedback_emphasis_role,
				github_conversation_model::path_role,
				github_conversation_model::line_role,
				github_conversation_model::original_line_role,
				github_conversation_model::diff_side_role,
				github_conversation_model::anchor_availability_role,
				github_conversation_model::review_state_role,
				github_conversation_model::resolved_role,
				github_conversation_model::outdated_role,
				github_conversation_model::deleted_role,
			};
			return roles;
		}
	}

	auto github_conversation_source_side(const int aProjectedSide) noexcept
		-> std::optional<core::diff_side>
	{
		if (aProjectedSide
			== static_cast<int>(
				application::github_conversation_diff_side::left)) {
			return core::diff_side::left;
		}
		if (aProjectedSide
			== static_cast<int>(
				application::github_conversation_diff_side::right)) {
			return core::diff_side::right;
		}
		return std::nullopt;
	}

	struct github_conversation_model::node
	{
		application::github_conversation_projection Value;
		node*                                       Parent = nullptr;
		/// [[BPR-1083]]. The body's label mark, read once per accepted body.
		mutable std::optional<feedback_body_mark> Mark;
		std::size_t                               Row = 0;
		std::vector<std::unique_ptr<node>>        Children;
	};

	github_conversation_model::github_conversation_model(QObject* const aParent)
		: QAbstractItemModel{ aParent }
	{
	}

	github_conversation_model::~github_conversation_model() = default;

	auto github_conversation_model::index(
		const int          aRow,
		const int          aColumn,
		const QModelIndex& aParentIndex) const -> QModelIndex
	{
		if (!hasIndex(aRow, aColumn, aParentIndex) || aColumn != 0) {
			return { };
		}

		const auto* const parentNode = aParentIndex.isValid()
			? static_cast<const node*>(aParentIndex.internalPointer())
			: nullptr;
		const auto&       values     = parentNode == nullptr ? Roots : parentNode->Children;
		return createIndex(
			aRow, aColumn, values[static_cast<std::size_t>(aRow)].get());
	}

	auto github_conversation_model::parent(const QModelIndex& aChild) const
		-> QModelIndex
	{
		if (!aChild.isValid()) {
			return { };
		}
		const auto* const value = static_cast<const node*>(aChild.internalPointer());
		return value == nullptr ? QModelIndex{ } : index_for_node(value->Parent);
	}

	auto github_conversation_model::rowCount(
		const QModelIndex& aParentIndex) const -> int
	{
		if (aParentIndex.isValid() && aParentIndex.column() != 0) {
			return 0;
		}
		const auto* const parentNode = aParentIndex.isValid()
			? static_cast<const node*>(aParentIndex.internalPointer())
			: nullptr;
		const auto&       values     = parentNode == nullptr ? Roots : parentNode->Children;
		return static_cast<int>(values.size());
	}

	auto github_conversation_model::columnCount(
		const QModelIndex& aParentIndex) const -> int
	{
		static_cast<void>(aParentIndex);
		return 1;
	}

	auto github_conversation_model::data(
		const QModelIndex& aModelIndex,
		const int          aRequestedRole) const -> QVariant
	{
		if (!aModelIndex.isValid() || aModelIndex.column() != 0) {
			return { };
		}
		const auto* const value = static_cast<const node*>(aModelIndex.internalPointer());
		if (value == nullptr) {
			return { };
		}
		const auto& conversation = value->Value;

		switch (aRequestedRole) {
		case Qt::DisplayRole:
			return display_text(conversation);
		case Qt::AccessibleTextRole: {
			auto shown = display_text(conversation);
			if (!value->Mark.has_value()) {
				value->Mark = conversation.Deleted ? feedback_body_mark{ } : feedback_body_mark_of(conversation.ReviewerProse.value_or(conversation.Body));
			}
			// [[BPR-1083]]. A post is named with its author and then its mark,
			// as in "Review comment by @tbutler, issue (blocking)"; its text
			// reads as written, header included.
			if (const auto& mark = *value->Mark; !mark.Name.isEmpty()) {
				const auto line = shown.indexOf(QLatin1Char{ '\n' });
				shown.insert(line < 0 ? shown.size() : line, QStringLiteral(", %1").arg(mark.Name));
			}
			return shown;
		}
		case Qt::AccessibleDescriptionRole:
			return description_text(conversation);
		case identity_role:
			return text(conversation.Id);
		case parent_identity_role:
			return conversation.ParentId.has_value()
				? QVariant{ text(*conversation.ParentId) }
				: QVariant{ };
		case kind_role:
			return static_cast<int>(conversation.Kind);
		case author_login_role:
			return text(conversation.AuthorLogin);
		case body_role:
			return conversation.Deleted ? QString{ } : text(conversation.ReviewerProse.value_or(conversation.Body));
		case feedback_label_role:
		case feedback_blocking_role:
		case feedback_name_role:
		case feedback_emphasis_role: {
			if (!value->Mark.has_value()) {
				value->Mark = conversation.Deleted ? feedback_body_mark{ } : feedback_body_mark_of(conversation.ReviewerProse.value_or(conversation.Body));
			}
			const auto& mark = *value->Mark;
			if (aRequestedRole == feedback_label_role) {
				return mark.Label.has_value() ? QVariant{ static_cast<int>(*mark.Label) } : QVariant{ };
			}
			if (aRequestedRole == feedback_blocking_role) {
				return mark.Emphasis.Weight == feedback_mark_weight::blocking;
			}
			if (aRequestedRole == feedback_emphasis_role) {
				return feedback_mark_emphasis_code(mark.Emphasis);
			}
			return mark.Name;
		}
		case path_role:
			return text(conversation.Path);
		case line_role:
			return conversation.Line.has_value()
				? QVariant::fromValue(static_cast<qulonglong>(*conversation.Line))
				: QVariant{ };
		case original_line_role:
			return conversation.OriginalLine.has_value()
				? QVariant::fromValue(
					  static_cast<qulonglong>(*conversation.OriginalLine))
				: QVariant{ };
		case diff_side_role:
			return conversation.DiffSide.has_value()
				? QVariant{ static_cast<int>(*conversation.DiffSide) }
				: QVariant{ };
		case anchor_availability_role:
			return static_cast<int>(conversation.AnchorAvailability);
		case review_state_role:
			return static_cast<int>(conversation.ReviewState);
		case resolved_role:
			return conversation.Resolved;
		case outdated_role:
			return conversation.Outdated;
		case deleted_role:
			return conversation.Deleted;
		default:
			return { };
		}
	}

	auto github_conversation_model::headerData(
		const int             aSection,
		const Qt::Orientation aOrientation,
		const int             aRequestedRole) const -> QVariant
	{
		if (aSection == 0 && aOrientation == Qt::Horizontal
			&& aRequestedRole == Qt::DisplayRole) {
			return QStringLiteral("Conversations");
		}
		return { };
	}

	auto github_conversation_model::flags(const QModelIndex& aModelIndex) const
		-> Qt::ItemFlags
	{
		return aModelIndex.isValid()
			? Qt::ItemIsEnabled | Qt::ItemIsSelectable
			: Qt::NoItemFlags;
	}

	auto github_conversation_model::roleNames() const -> QHash<int, QByteArray>
	{
		auto roles = QAbstractItemModel::roleNames();
		roles.insert(identity_role, QByteArrayLiteral("identity"));
		roles.insert(parent_identity_role, QByteArrayLiteral("parentIdentity"));
		roles.insert(kind_role, QByteArrayLiteral("kind"));
		roles.insert(author_login_role, QByteArrayLiteral("authorLogin"));
		roles.insert(body_role, QByteArrayLiteral("body"));
		roles.insert(path_role, QByteArrayLiteral("path"));
		roles.insert(line_role, QByteArrayLiteral("line"));
		roles.insert(original_line_role, QByteArrayLiteral("originalLine"));
		roles.insert(diff_side_role, QByteArrayLiteral("diffSide"));
		roles.insert(
			anchor_availability_role, QByteArrayLiteral("anchorAvailability"));
		roles.insert(review_state_role, QByteArrayLiteral("reviewState"));
		roles.insert(resolved_role, QByteArrayLiteral("resolved"));
		roles.insert(outdated_role, QByteArrayLiteral("outdated"));
		roles.insert(deleted_role, QByteArrayLiteral("deleted"));
		return roles;
	}

	void github_conversation_model::apply(
		const std::vector<application::github_conversation_projection>&
			aConversations)
	{
		const auto&                                          roles = changed_roles();
		std::unordered_map<std::string, const conversation*> desiredByIdentity;
		desiredByIdentity.reserve(aConversations.size());
		for (const auto& value : aConversations) {
			if (!value.Id.empty()) {
				desiredByIdentity.try_emplace(value.Id, &value);
			}
		}

		std::unordered_map<std::string, std::optional<std::string>> desiredParent;
		desiredParent.reserve(desiredByIdentity.size());
		for (const auto& [identity, value] : desiredByIdentity) {
			std::optional<std::string> parentIdentity;
			if (
				value->Kind == application::github_conversation_kind::review_comment
				&& value->ParentId.has_value() && *value->ParentId != identity) {
				// A comment nests under its thread or under the comment it
				// replied to. The projection resolves the reply edge and
				// guarantees every chain terminates at a thread, so accepting a
				// comment parent here cannot orphan a row.
				if (const auto parent = desiredByIdentity.find(*value->ParentId);
					parent != desiredByIdentity.end()
					&& (parent->second->Kind
							== application::github_conversation_kind::review_thread
						|| parent->second->Kind
							== application::github_conversation_kind::review_comment)) {
					parentIdentity = *value->ParentId;
				}
			}
			desiredParent.emplace(identity, std::move(parentIdentity));
		}

		// A parent chain that never terminates descends from no root, so the
		// rows in it would be built and then attached to nothing. Promote every
		// node on such a chain to a root instead: an unexpected shape should
		// cost nesting, never a row. The projection resolves reply edges so
		// this cannot arise from GitHub, but the model is fed by whoever calls
		// it.
		//
		// The decision reads the parent map and never writes it, so it does not
		// depend on the order an unordered container happens to iterate in.
		// Breaking a cycle in place does: whichever member was visited first
		// became the root, and the same input could produce two different
		// trees.
		std::unordered_set<std::string> promoteToRoot;
		for (const auto& [identity, parentIdentity] : desiredParent) {
			auto walk = parentIdentity;
			for (auto remaining = desiredParent.size(); walk.has_value();
				 --remaining) {
				if (remaining == 0U) {
					promoteToRoot.insert(identity);
					break;
				}
				const auto next = desiredParent.find(*walk);
				if (next == desiredParent.end()) {
					break;
				}
				walk = next->second;
			}
		}
		for (const auto& identity : promoteToRoot) {
			desiredParent.at(identity).reset();
		}

		std::vector<const conversation*> desiredRoots;
		std::unordered_map<std::string, std::vector<const conversation*>>
			desiredChildren;
		desiredRoots.reserve(desiredByIdentity.size());
		desiredChildren.reserve(desiredByIdentity.size());
		std::unordered_set<std::string> ordered;
		ordered.reserve(desiredByIdentity.size());
		for (const auto& value : aConversations) {
			const auto selected = desiredByIdentity.find(value.Id);
			if (
				selected == desiredByIdentity.end()
				|| selected->second != &value || !ordered.insert(value.Id).second) {
				continue;
			}
			const auto& parentIdentity = desiredParent.at(value.Id);
			if (parentIdentity.has_value()) {
				desiredChildren[*parentIdentity].push_back(&value);
			} else {
				desiredRoots.push_back(&value);
			}
		}

		std::function<void(
			std::vector<std::unique_ptr<node>>&,
			const QModelIndex&,
			const std::optional<std::string>&)>
			removeObsolete;
		removeObsolete =
			[this, &desiredParent, &removeObsolete](
				std::vector<std::unique_ptr<node>>& aValues,
				const QModelIndex&                  aParentIndex,
				const std::optional<std::string>&   aActualParent) {
				for (std::size_t offset = aValues.size(); offset != 0U; --offset) {
					const auto  row     = offset - 1U;
					auto* const value   = aValues[row].get();
					const auto  desired = desiredParent.find(value->Value.Id);
					if (
						desired == desiredParent.end()
						|| desired->second != aActualParent) {
						beginRemoveRows(
							aParentIndex,
							static_cast<int>(row),
							static_cast<int>(row));
						aValues.erase(
							aValues.begin() + static_cast<std::ptrdiff_t>(row));
						update_rows(aValues, row);
						endRemoveRows();
						++Statistics.RowsRemoved;
						continue;
					}
					removeObsolete(
						value->Children,
						index_for_node(value),
						std::optional<std::string>{ value->Value.Id });
				}
			};
		removeObsolete(Roots, { }, std::nullopt);

		static const std::vector<const conversation*> noChildren;
		std::function<void(
			std::vector<std::unique_ptr<node>>&,
			node*,
			const QModelIndex&,
			const std::vector<const conversation*>&)>
			synchronize;
		synchronize =
			[this, &desiredChildren, &roles, &synchronize](
				std::vector<std::unique_ptr<node>>&     aValues,
				node* const                             aParentNode,
				const QModelIndex&                      aParentIndex,
				const std::vector<const conversation*>& aDesired) {
				aValues.reserve(aDesired.size());

				std::unordered_map<node*, std::size_t> desiredExistingPositions;
				desiredExistingPositions.reserve(aValues.size());
				std::unordered_map<std::string_view, node*> existingByIdentity;
				existingByIdentity.reserve(aValues.size());
				for (const auto& value : aValues) {
					existingByIdentity.emplace(value->Value.Id, value.get());
				}

				std::vector<node*> desiredExisting;
				desiredExisting.reserve(aValues.size());
				for (const auto* const desiredValue : aDesired) {
					const auto existing = existingByIdentity.find(desiredValue->Id);
					if (existing != existingByIdentity.end()) {
						desiredExistingPositions.emplace(
							existing->second, desiredExisting.size());
						desiredExisting.push_back(existing->second);
					}
				}

				std::vector<std::size_t> currentRanks;
				currentRanks.reserve(aValues.size());
				for (const auto& value : aValues) {
					currentRanks.push_back(
						desiredExistingPositions.at(value.get()));
				}

				std::vector<std::size_t> tailRanks;
				std::vector<std::size_t> tailIndices;
				std::vector<std::size_t> predecessors(
					currentRanks.size(), currentRanks.size());
				tailRanks.reserve(currentRanks.size());
				tailIndices.reserve(currentRanks.size());
				for (std::size_t index = 0; index < currentRanks.size(); ++index) {
					const auto insertion = std::lower_bound(
						tailRanks.begin(), tailRanks.end(), currentRanks[index]);
					const auto length = static_cast<std::size_t>(
						std::distance(tailRanks.begin(), insertion));
					if (length == tailRanks.size()) {
						tailRanks.push_back(currentRanks[index]);
						tailIndices.push_back(index);
					} else {
						*insertion          = currentRanks[index];
						tailIndices[length] = index;
					}
					if (length != 0U) {
						predecessors[index] = tailIndices[length - 1U];
					}
				}

				std::vector<bool> retained(desiredExisting.size(), false);
				if (!tailIndices.empty()) {
					auto index = tailIndices.back();
					while (index != currentRanks.size()) {
						retained[currentRanks[index]] = true;
						index                         = predecessors[index];
					}
				}

				node* anchor = nullptr;
				for (auto offset = desiredExisting.size(); offset != 0U; --offset) {
					const auto  desiredRow = offset - 1U;
					auto* const value      = desiredExisting[desiredRow];
					if (retained[desiredRow]) {
						anchor = value;
						continue;
					}

					const auto sourceRow        = value->Row;
					const auto destinationChild = anchor == nullptr ? aValues.size() : anchor->Row;
					const auto insertionRow     = sourceRow < destinationChild
						? destinationChild - 1U
						: destinationChild;
					if (sourceRow != insertionRow) {
						if (!beginMoveRows(
								aParentIndex,
								static_cast<int>(sourceRow),
								static_cast<int>(sourceRow),
								aParentIndex,
								static_cast<int>(destinationChild))) {
							return;
						}
						auto moved = std::move(aValues[sourceRow]);
						aValues.erase(
							aValues.begin()
							+ static_cast<std::ptrdiff_t>(sourceRow));
						aValues.insert(
							aValues.begin()
								+ static_cast<std::ptrdiff_t>(insertionRow),
							std::move(moved));
						update_rows(aValues, std::min(sourceRow, insertionRow));
						endMoveRows();
						++Statistics.RowsMoved;
					}
					anchor = value;
				}

				for (std::size_t row = 0; row < aDesired.size(); ++row) {
					if (
						row < aValues.size()
						&& aValues[row]->Value.Id == aDesired[row]->Id) {
						continue;
					}

					auto inserted    = std::make_unique<node>();
					inserted->Value  = *aDesired[row];
					inserted->Parent = aParentNode;
					inserted->Row    = row;
					if (const auto children = desiredChildren.find(inserted->Value.Id);
						children != desiredChildren.end()) {
						inserted->Children.reserve(children->second.size());
					}
					beginInsertRows(
						aParentIndex,
						static_cast<int>(row),
						static_cast<int>(row));
					aValues.insert(
						aValues.begin() + static_cast<std::ptrdiff_t>(row),
						std::move(inserted));
					update_rows(aValues, row);
					endInsertRows();
					++Statistics.RowsInserted;
				}

				for (std::size_t row = 0; row < aDesired.size(); ++row) {
					auto* const current = aValues[row].get();
					if (current->Value != *aDesired[row]) {
						auto replacement = *aDesired[row];
						current->Value   = std::move(replacement);
						current->Mark.reset();
						const auto changedIndex = index_for_node(current);
						emit       dataChanged(changedIndex, changedIndex, roles);
						++Statistics.RowsChanged;
					}

					const auto children = desiredChildren.find(current->Value.Id);
					synchronize(
						current->Children,
						current,
						index_for_node(current),
						children == desiredChildren.end()
							? noChildren
							: children->second);
				}
			};
		synchronize(Roots, nullptr, { }, desiredRoots);
	}

	void github_conversation_model::set_anchor_availability(
		const QModelIndex&                                         aIndex,
		const application::github_conversation_anchor_availability aAvailability)
	{
		if (!aIndex.isValid() || aIndex.model() != this || aIndex.column() != 0) {
			return;
		}
		auto* const value = static_cast<node*>(aIndex.internalPointer());
		if (value == nullptr || value->Value.AnchorAvailability == aAvailability) {
			return;
		}
		value->Value.AnchorAvailability = aAvailability;
		++Statistics.RowsChanged;
		emit dataChanged(
			aIndex,
			aIndex,
			{
				Qt::DisplayRole,
				Qt::AccessibleTextRole,
				Qt::AccessibleDescriptionRole,
				anchor_availability_role,
			});
	}

	void github_conversation_model::clear()
	{
		apply({ });
	}

	auto github_conversation_model::index_for_id(
		const std::string_view aIdentity) const -> QModelIndex
	{
		return index_for_node(find_node(Roots, aIdentity));
	}

	auto github_conversation_model::id_for_index(
		const QModelIndex& aModelIndex) const -> std::string
	{
		if (!aModelIndex.isValid()) {
			return { };
		}
		const auto* const value = static_cast<const node*>(aModelIndex.internalPointer());
		return value == nullptr ? std::string{ } : value->Value.Id;
	}

	auto github_conversation_model::semantic_anchor(const QModelIndex& aIndex) const -> std::optional<core::source_line_ref>
	{
		if (!aIndex.isValid() || aIndex.model() != this) {
			return std::nullopt;
		}
		const auto* value = static_cast<const node*>(aIndex.internalPointer());
		return value ? value->Value.SemanticAnchor : std::nullopt;
	}

	auto github_conversation_model::statistics() const noexcept
		-> github_conversation_model_statistics
	{
		return Statistics;
	}

	auto github_conversation_model::index_for_node(const node* const aValue) const
		-> QModelIndex
	{
		return aValue == nullptr
			? QModelIndex{ }
			: createIndex(static_cast<int>(aValue->Row), 0, aValue);
	}

	auto github_conversation_model::find_node(
		const std::vector<std::unique_ptr<node>>& aValues,
		const std::string_view                    aIdentity) -> const node*
	{
		for (const auto& value : aValues) {
			if (value->Value.Id == aIdentity) {
				return value.get();
			}
			if (const auto* const child = find_node(value->Children, aIdentity);
				child != nullptr) {
				return child;
			}
		}
		return nullptr;
	}

	void github_conversation_model::update_rows(
		std::vector<std::unique_ptr<node>>& aValues,
		const std::size_t                   aFirst) noexcept
	{
		for (auto row = aFirst; row < aValues.size(); ++row) {
			aValues[row]->Row = row;
		}
	}
}
