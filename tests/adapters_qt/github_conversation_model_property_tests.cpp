#include "bpr/qt/github_conversation_model.hpp"

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QModelIndex>
#include <QObject>
#include <QPersistentModelIndex>
#include <QString>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using conversation      = application::github_conversation_projection;
	using conversation_kind = application::github_conversation_kind;

	[[nodiscard]] auto text(const std::string_view aValue) -> QString
	{
		return QString::fromUtf8(
			aValue.data(), static_cast<qsizetype>(aValue.size()));
	}

	[[noreturn]] auto fail(
		const std::string_view aSuite,
		const std::size_t      aCaseNumber,
		const std::string_view aMessage) -> void
	{
		std::cerr << "FAILED [" << aSuite << " case " << aCaseNumber
				  << "]: " << aMessage << '\n';
		std::exit(EXIT_FAILURE);
	}

	auto expect(
		const bool             aCondition,
		const std::string_view aSuite,
		const std::size_t      aCaseNumber,
		const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			fail(aSuite, aCaseNumber, aMessage);
		}
	}

	class fixed_generator
	{
	public:
		explicit fixed_generator(const std::uint64_t aSeed)
			: State{ aSeed }
		{ }

		[[nodiscard]] auto bounded(const std::size_t aBound) -> std::size_t
		{
			State += UINT64_C(0x9e3779b97f4a7c15);
			auto value = State;
			value      = (value ^ (value >> 30U))
				* UINT64_C(0xbf58476d1ce4e5b9);
			value = (value ^ (value >> 27U))
				* UINT64_C(0x94d049bb133111eb);
			value ^= value >> 31U;
			return static_cast<std::size_t>(
				value % static_cast<std::uint64_t>(aBound));
		}

	private:
		std::uint64_t State;
	};

	struct expected_tree
	{
		std::vector<std::string>                                    Roots;
		std::unordered_map<std::string, std::vector<std::string>>   Children;
		std::unordered_map<std::string, std::optional<std::string>> ParentById;
		std::unordered_map<std::string, conversation>               ValueById;
	};

	[[nodiscard]] auto expected_for(
		const std::vector<conversation>& aValues) -> expected_tree
	{
		expected_tree result;
		result.ValueById.reserve(aValues.size());
		result.ParentById.reserve(aValues.size());
		result.Children.reserve(aValues.size());

		for (const auto& value : aValues) {
			if (!value.Id.empty()) {
				result.ValueById.try_emplace(value.Id, value);
			}
		}

		for (const auto& [identity, value] : result.ValueById) {
			std::optional<std::string> parent;
			if (
				value.Kind == conversation_kind::review_comment
				&& value.ParentId.has_value() && *value.ParentId != identity) {
				if (const auto candidate = result.ValueById.find(*value.ParentId);
					candidate != result.ValueById.end()
					&& candidate->second.Kind
						== conversation_kind::review_thread) {
					parent = *value.ParentId;
				}
			}
			result.ParentById.emplace(identity, std::move(parent));
		}

		std::unordered_set<std::string> ordered;
		ordered.reserve(result.ValueById.size());
		result.Roots.reserve(result.ValueById.size());
		for (const auto& value : aValues) {
			if (
				value.Id.empty()
				|| !ordered.insert(value.Id).second) {
				continue;
			}

			const auto& parent = result.ParentById.at(value.Id);
			if (parent.has_value()) {
				result.Children[*parent].push_back(value.Id);
			} else {
				result.Roots.push_back(value.Id);
			}
		}
		return result;
	}

	struct signal_audit
	{
		std::size_t InsertedRows = 0;
		std::size_t RemovedRows  = 0;
		std::size_t MovedRows    = 0;
		std::size_t MoveEvents   = 0;
		std::size_t ChangedRows  = 0;
		std::size_t ModelResets  = 0;

		void clear_operations() noexcept
		{
			InsertedRows = 0;
			RemovedRows  = 0;
			MovedRows    = 0;
			MoveEvents   = 0;
			ChangedRows  = 0;
		}
	};

	auto observe(qt::github_conversation_model& aModel, signal_audit& aAudit) -> void
	{
		QObject::connect(
			&aModel,
			&QAbstractItemModel::rowsInserted,
			[&aAudit](const QModelIndex&, const int aFirst, const int aLast) {
				aAudit.InsertedRows += static_cast<std::size_t>(aLast - aFirst + 1);
			});
		QObject::connect(
			&aModel,
			&QAbstractItemModel::rowsRemoved,
			[&aAudit](const QModelIndex&, const int aFirst, const int aLast) {
				aAudit.RemovedRows += static_cast<std::size_t>(aLast - aFirst + 1);
			});
		QObject::connect(
			&aModel,
			&QAbstractItemModel::rowsMoved,
			[&aAudit](
				const QModelIndex&,
				const int aFirst,
				const int aLast,
				const QModelIndex&,
				const int) {
				aAudit.MovedRows += static_cast<std::size_t>(aLast - aFirst + 1);
				++aAudit.MoveEvents;
			});
		QObject::connect(
			&aModel,
			&QAbstractItemModel::dataChanged,
			[&aAudit](
				const QModelIndex& aTopLeft,
				const QModelIndex& aBottomRight,
				const QList<int>&) {
				aAudit.ChangedRows += static_cast<std::size_t>(
					aBottomRight.row() - aTopLeft.row() + 1);
			});
		QObject::connect(
			&aModel,
			&QAbstractItemModel::modelReset,
			[&aAudit] { ++aAudit.ModelResets; });
	}

	auto validate_index(
		const qt::github_conversation_model& aModel,
		const QModelIndex&                   aIndex,
		const std::string&                   aIdentity,
		const expected_tree&                 aExpected,
		const std::string_view               aSuite,
		const std::size_t                    aCaseNumber,
		const bool                           aValidateLookup) -> void
	{
		const auto& projection = aExpected.ValueById.at(aIdentity);
		expect(aIndex.isValid(), aSuite, aCaseNumber, "expected index is valid");
		expect(aIndex.column() == 0, aSuite, aCaseNumber, "index uses column zero");
		expect(
			aModel.id_for_index(aIndex) == aIdentity,
			aSuite,
			aCaseNumber,
			"index identity matches expected order");
		const auto identityValue = aModel.data(aIndex, qt::github_conversation_model::identity_role);
		expect(
			identityValue.isValid()
				&& identityValue.toString() == text(aIdentity),
			aSuite,
			aCaseNumber,
			"identity role matches stable identity");
		const auto kindValue = aModel.data(aIndex, qt::github_conversation_model::kind_role);
		expect(
			kindValue.isValid()
				&& kindValue.toInt() == static_cast<int>(projection.Kind),
			aSuite,
			aCaseNumber,
			"kind role matches the first unique projection");

		const auto  parentValue     = aModel.data(aIndex, qt::github_conversation_model::parent_identity_role);
		const auto& projectedParent = projection.ParentId;
		expect(
			projectedParent.has_value()
				? parentValue.isValid()
					&& parentValue.toString() == text(*projectedParent)
				: !parentValue.isValid(),
			aSuite,
			aCaseNumber,
			"parent identity role matches the first unique projection");

		const auto authorValue = aModel.data(aIndex, qt::github_conversation_model::author_login_role);
		expect(
			authorValue.isValid()
				&& authorValue.toString() == text(projection.AuthorLogin),
			aSuite,
			aCaseNumber,
			"author role matches the first unique projection");

		const auto bodyValue    = aModel.data(aIndex, qt::github_conversation_model::body_role);
		const auto expectedBody = projection.Deleted ? QString{ } : text(projection.Body);
		expect(
			bodyValue.isValid() && bodyValue.toString() == expectedBody,
			aSuite,
			aCaseNumber,
			"body role matches deletion-aware projection semantics");

		const auto pathValue = aModel.data(aIndex, qt::github_conversation_model::path_role);
		expect(
			pathValue.isValid()
				&& pathValue.toString() == text(projection.Path),
			aSuite,
			aCaseNumber,
			"path role matches the first unique projection");

		const auto lineValue = aModel.data(aIndex, qt::github_conversation_model::line_role);
		expect(
			projection.Line.has_value()
				? lineValue.isValid()
					&& lineValue.toULongLong()
						== static_cast<qulonglong>(*projection.Line)
				: !lineValue.isValid(),
			aSuite,
			aCaseNumber,
			"line role preserves exact optional presence and value");

		const auto reviewStateValue = aModel.data(aIndex, qt::github_conversation_model::review_state_role);
		expect(
			reviewStateValue.isValid()
				&& reviewStateValue.toInt()
					== static_cast<int>(projection.ReviewState),
			aSuite,
			aCaseNumber,
			"review-state role matches the first unique projection");

		const auto resolvedValue = aModel.data(aIndex, qt::github_conversation_model::resolved_role);
		const auto outdatedValue = aModel.data(aIndex, qt::github_conversation_model::outdated_role);
		const auto deletedValue  = aModel.data(aIndex, qt::github_conversation_model::deleted_role);
		expect(
			resolvedValue.isValid()
				&& resolvedValue.toBool() == projection.Resolved
				&& outdatedValue.isValid()
				&& outdatedValue.toBool() == projection.Outdated
				&& deletedValue.isValid()
				&& deletedValue.toBool() == projection.Deleted,
			aSuite,
			aCaseNumber,
			"resolved, outdated, and deleted roles match the projection");

		const auto display        = aModel.data(aIndex, Qt::DisplayRole).toString();
		const auto accessibleText = aModel.data(aIndex, Qt::AccessibleTextRole).toString();
		expect(
			!display.isEmpty() && accessibleText == display,
			aSuite,
			aCaseNumber,
			"accessible text is the complete nonempty display text");
		if (projection.Deleted) {
			expect(
				display.contains(QStringLiteral("Deleted comment")),
				aSuite,
				aCaseNumber,
				"deleted display text exposes the deletion state");
		} else if (!projection.Body.empty()) {
			expect(
				display.contains(text(projection.Body)),
				aSuite,
				aCaseNumber,
				"nondeleted display text contains the literal projected body");
		}

		const auto accessibleDescriptionValue = aModel.data(aIndex, Qt::AccessibleDescriptionRole);
		const auto accessibleDescription      = accessibleDescriptionValue.toString();
		const auto hasDescription             = projection.Resolved || projection.Outdated
			|| projection.Deleted || projection.ParentId.has_value();
		expect(
			accessibleDescriptionValue.isValid()
				&& accessibleDescription.isEmpty() == !hasDescription
				&& accessibleDescription.contains(QStringLiteral("Resolved"))
					== projection.Resolved
				&& accessibleDescription.contains(QStringLiteral("Outdated"))
					== projection.Outdated
				&& accessibleDescription.contains(QStringLiteral("Deleted"))
					== projection.Deleted
				&& accessibleDescription.contains(
					   QStringLiteral("Reply in a review thread"))
					== projection.ParentId.has_value(),
			aSuite,
			aCaseNumber,
			"accessible description exposes exactly the applicable state facts");

		if (aValidateLookup) {
			expect(
				aModel.index_for_id(aIdentity) == aIndex,
				aSuite,
				aCaseNumber,
				"stable identity lookup returns the exact index");
		}
	}

	auto validate_model(
		const qt::github_conversation_model& aModel,
		const expected_tree&                 aExpected,
		const std::string_view               aSuite,
		const std::size_t                    aCaseNumber) -> void
	{
		expect(
			aModel.columnCount() == 1,
			aSuite,
			aCaseNumber,
			"root exposes exactly one column");
		expect(
			aModel.rowCount()
				== static_cast<int>(aExpected.Roots.size()),
			aSuite,
			aCaseNumber,
			"root row count matches normalized projection");

		const auto  validateEveryLookup = aExpected.ValueById.size() <= 512U;
		std::size_t visited             = 0;
		for (std::size_t rootRow = 0; rootRow < aExpected.Roots.size(); ++rootRow) {
			const auto  root     = aModel.index(static_cast<int>(rootRow), 0, QModelIndex{ });
			const auto& identity = aExpected.Roots[rootRow];
			expect(
				root.row() == static_cast<int>(rootRow),
				aSuite,
				aCaseNumber,
				"root index reports its exact row");
			expect(
				!aModel.parent(root).isValid(),
				aSuite,
				aCaseNumber,
				"root index has no parent");
			validate_index(
				aModel,
				root,
				identity,
				aExpected,
				aSuite,
				aCaseNumber,
				validateEveryLookup);
			++visited;

			const auto children   = aExpected.Children.find(identity);
			const auto childCount = children == aExpected.Children.end()
				? 0U
				: children->second.size();
			expect(
				aModel.rowCount(root) == static_cast<int>(childCount),
				aSuite,
				aCaseNumber,
				"child row count matches normalized projection");
			expect(
				aModel.columnCount(root) == 1,
				aSuite,
				aCaseNumber,
				"root child collection exposes exactly one column");

			for (std::size_t childRow = 0; childRow < childCount; ++childRow) {
				const auto  child         = aModel.index(static_cast<int>(childRow), 0, root);
				const auto& childIdentity = children->second[childRow];
				expect(
					child.row() == static_cast<int>(childRow),
					aSuite,
					aCaseNumber,
					"child index reports its exact row");
				expect(
					aModel.parent(child) == root,
					aSuite,
					aCaseNumber,
					"child index returns the exact root parent");
				expect(
					aModel.rowCount(child) == 0,
					aSuite,
					aCaseNumber,
					"normalized conversation tree has only root and child levels");
				validate_index(
					aModel,
					child,
					childIdentity,
					aExpected,
					aSuite,
					aCaseNumber,
					validateEveryLookup);
				++visited;
			}
		}

		expect(
			visited == aExpected.ValueById.size(),
			aSuite,
			aCaseNumber,
			"every unique nonempty identity is reachable exactly once");
		expect(
			!aModel.index_for_id("property-test:absent").isValid(),
			aSuite,
			aCaseNumber,
			"unknown identity lookup is invalid");

		if (!validateEveryLookup && !aExpected.Roots.empty()) {
			const std::size_t sampledRows[]{
				0U,
				aExpected.Roots.size() / 2U,
				aExpected.Roots.size() - 1U,
			};
			for (const auto row : sampledRows) {
				expect(
					aModel.index_for_id(aExpected.Roots[row])
						== aModel.index(static_cast<int>(row), 0),
					aSuite,
					aCaseNumber,
					"sampled large-model identity lookup returns the exact row");
			}
		}
	}

	using persistent_indexes = std::unordered_map<std::string, QPersistentModelIndex>;

	[[nodiscard]] auto capture_persistent_indexes(
		const qt::github_conversation_model& aModel,
		const expected_tree&                 aExpected) -> persistent_indexes
	{
		persistent_indexes result;
		result.reserve(aExpected.ValueById.size());
		for (const auto& entry : aExpected.ValueById) {
			result.emplace(entry.first, aModel.index_for_id(entry.first));
		}
		return result;
	}

	[[nodiscard]] auto preserves_persistent_index(
		const expected_tree& aBefore,
		const expected_tree& aAfter,
		const std::string&   aIdentity) -> bool
	{
		const auto priorParent = aBefore.ParentById.find(aIdentity);
		const auto nextParent  = aAfter.ParentById.find(aIdentity);
		return priorParent != aBefore.ParentById.end()
			&& nextParent != aAfter.ParentById.end()
			&& priorParent->second == nextParent->second;
	}

	struct varied_role_coverage
	{
		bool Kind        = false;
		bool ParentId    = false;
		bool AuthorLogin = false;
		bool Body        = false;
		bool Path        = false;
		bool Line        = false;
		bool ReviewState = false;
		bool Resolved    = false;
		bool Outdated    = false;
		bool Deleted     = false;
	};

	[[nodiscard]] auto visible_body(const conversation& aValue) noexcept
		-> std::string_view
	{
		return aValue.Deleted ? std::string_view{ } : aValue.Body;
	}

	[[nodiscard]] auto observe_preserved_role_changes(
		varied_role_coverage& aCoverage,
		const expected_tree&  aBefore,
		const expected_tree&  aAfter) -> std::size_t
	{
		std::size_t changedRows = 0U;
		for (const auto& [identity, prior] : aBefore.ValueById) {
			if (!preserves_persistent_index(aBefore, aAfter, identity)) {
				continue;
			}

			const auto& next      = aAfter.ValueById.at(identity);
			aCoverage.Kind        = aCoverage.Kind || prior.Kind != next.Kind;
			aCoverage.ParentId    = aCoverage.ParentId || prior.ParentId != next.ParentId;
			aCoverage.AuthorLogin = aCoverage.AuthorLogin || prior.AuthorLogin != next.AuthorLogin;
			aCoverage.Body        = aCoverage.Body || visible_body(prior) != visible_body(next);
			aCoverage.Path        = aCoverage.Path || prior.Path != next.Path;
			aCoverage.Line        = aCoverage.Line || prior.Line != next.Line;
			aCoverage.ReviewState = aCoverage.ReviewState || prior.ReviewState != next.ReviewState;
			aCoverage.Resolved    = aCoverage.Resolved || prior.Resolved != next.Resolved;
			aCoverage.Outdated    = aCoverage.Outdated || prior.Outdated != next.Outdated;
			aCoverage.Deleted     = aCoverage.Deleted || prior.Deleted != next.Deleted;
			changedRows += prior != next ? 1U : 0U;
		}
		return changedRows;
	}

	auto validate_persistent_indexes(
		const qt::github_conversation_model& aModel,
		const expected_tree&                 aBefore,
		const expected_tree&                 aAfter,
		const persistent_indexes&            aIndexes,
		const std::string_view               aSuite,
		const std::size_t                    aCaseNumber) -> void
	{
		for (const auto& [identity, index] : aIndexes) {
			if (preserves_persistent_index(aBefore, aAfter, identity)) {
				expect(
					index.isValid(),
					aSuite,
					aCaseNumber,
					"same-parent stable identity preserves persistent index");
				expect(
					QModelIndex{ index } == aModel.index_for_id(identity),
					aSuite,
					aCaseNumber,
					"persistent index follows exact insert and move position");
			} else {
				expect(
					!index.isValid(),
					aSuite,
					aCaseNumber,
					"removed or reparented identity invalidates old index");
			}
		}
	}

	[[nodiscard]] auto mixed_projection(
		fixed_generator&  aGenerator,
		const std::size_t aCaseNumber,
		const std::size_t aSlot) -> conversation
	{
		const auto identityNumber = aGenerator.bounded(96U);
		auto       identity       = std::string{ "mixed:" } + std::to_string(identityNumber);
		if (aGenerator.bounded(29U) == 0U) {
			identity.clear();
		}

		std::optional<std::string> parent;
		if (aGenerator.bounded(4U) != 0U) {
			if (aGenerator.bounded(7U) == 0U) {
				parent = std::string{ "missing:" }
					+ std::to_string(aGenerator.bounded(16U));
			} else if (aGenerator.bounded(11U) == 0U) {
				parent = identity;
			} else {
				parent = std::string{ "mixed:" }
					+ std::to_string(aGenerator.bounded(96U));
			}
		}

		return conversation{
			.Id          = std::move(identity),
			.ParentId    = std::move(parent),
			.Kind        = static_cast<conversation_kind>(aGenerator.bounded(4U)),
			.AuthorLogin = std::string{ "author:" }
				+ std::to_string(aGenerator.bounded(13U)),
			.Body = std::string{ "body:" } + std::to_string(aCaseNumber % 17U) + ':'
				+ std::to_string(aSlot) + ':'
				+ std::to_string(aGenerator.bounded(9U)),
			.Path = std::string{ "src/property-" }
				+ std::to_string(aGenerator.bounded(11U)) + ".cpp",
			.Line        = aGenerator.bounded(3U) == 0U
				? std::nullopt
				: std::optional<std::uint32_t>{
					  static_cast<std::uint32_t>(
						  1U + aGenerator.bounded(500U)) },
			.ReviewState = static_cast<application::github_review_state_projection>(aGenerator.bounded(6U)),
			.Resolved    = aGenerator.bounded(2U) != 0U,
			.Outdated    = aGenerator.bounded(2U) != 0U,
			.Deleted     = aGenerator.bounded(5U) == 0U,
		};
	}

	[[nodiscard]] auto mixed_case(
		fixed_generator&  aGenerator,
		const std::size_t aCaseNumber) -> std::vector<conversation>
	{
		if (aCaseNumber % 31U == 0U) {
			return { };
		}

		std::vector<conversation> result;
		const auto                randomCount = 20U + aGenerator.bounded(81U);
		result.reserve(randomCount + 3U);
		for (std::size_t slot = 0; slot < randomCount; ++slot) {
			result.push_back(mixed_projection(aGenerator, aCaseNumber, slot));
		}

		auto anchorThread = conversation{
			.Id          = "mixed:anchor-thread",
			.Kind        = aCaseNumber % 23U == 0U
				? conversation_kind::issue_comment
				: conversation_kind::review_thread,
			.AuthorLogin = "anchor",
			.Body        = std::string{ "anchor-thread:" }
				+ std::to_string(aCaseNumber % 7U),
		};
		auto anchorChild = conversation{
			.Id          = "mixed:anchor-child",
			.ParentId    = aCaseNumber % 19U == 0U
				? std::optional<std::string>{ "missing:anchor" }
				: std::optional<std::string>{ "mixed:anchor-thread" },
			.Kind        = conversation_kind::review_comment,
			.AuthorLogin = "anchor",
			.Body        = std::string{ "anchor-child:" }
				+ std::to_string(aCaseNumber % 5U),
		};
		result.push_back(std::move(anchorThread));
		result.push_back(std::move(anchorChild));

		if (!result.empty() && aCaseNumber % 5U == 0U) {
			result.push_back(result[aGenerator.bounded(result.size())]);
		}
		if (result.size() > 1U) {
			std::rotate(
				result.begin(),
				result.begin()
					+ static_cast<std::ptrdiff_t>(
						aGenerator.bounded(result.size())),
				result.end());
		}
		return result;
	}

	auto test_mixed_cases() -> void
	{
		constexpr std::size_t         caseCount = 3'000U;
		qt::github_conversation_model model;
		signal_audit                  audit;
		observe(model, audit);
		fixed_generator generator{ UINT64_C(0x4250522d4d495845) };
		expected_tree   prior;

		std::size_t          casesWithInserts   = 0;
		std::size_t          casesWithRemoves   = 0;
		std::size_t          casesWithMoves     = 0;
		std::size_t          casesWithChanges   = 0;
		std::size_t          preservedIndexes   = 0;
		std::size_t          invalidatedIndexes = 0;
		varied_role_coverage variedRoles;

		for (std::size_t caseNumber = 0; caseNumber < caseCount; ++caseNumber) {
			const auto values              = mixed_case(generator, caseNumber);
			auto       expected            = expected_for(values);
			const auto expectedChangedRows = observe_preserved_role_changes(variedRoles, prior, expected);
			const auto persistent          = capture_persistent_indexes(model, prior);
			audit.clear_operations();

			model.apply(values);

			validate_persistent_indexes(
				model,
				prior,
				expected,
				persistent,
				"mixed",
				caseNumber);
			for (const auto& [identity, index] : persistent) {
				static_cast<void>(index);
				if (preserves_persistent_index(prior, expected, identity)) {
					++preservedIndexes;
				} else {
					++invalidatedIndexes;
				}
			}
			validate_model(model, expected, "mixed", caseNumber);
			expect(
				audit.ChangedRows == expectedChangedRows
					&& audit.ModelResets == 0U
					&& model.statistics().ModelResets == 0U,
				"mixed",
				caseNumber,
				"mixed projection changes emit exact data rows without reset");

			casesWithInserts += audit.InsertedRows != 0U ? 1U : 0U;
			casesWithRemoves += audit.RemovedRows != 0U ? 1U : 0U;
			casesWithMoves += audit.MovedRows != 0U ? 1U : 0U;
			casesWithChanges += audit.ChangedRows != 0U ? 1U : 0U;
			prior = std::move(expected);
		}

		expect(
			casesWithInserts != 0U && casesWithRemoves != 0U
				&& casesWithMoves != 0U && casesWithChanges != 0U,
			"mixed",
			caseCount,
			"fixed mixed cases exercise insert, remove, move, and change paths");
		expect(
			preservedIndexes != 0U && invalidatedIndexes != 0U,
			"mixed",
			caseCount,
			"fixed mixed cases exercise persistent preservation and invalidation");
		expect(
			variedRoles.Kind && variedRoles.ParentId
				&& variedRoles.AuthorLogin && variedRoles.Body
				&& variedRoles.Path && variedRoles.Line
				&& variedRoles.ReviewState && variedRoles.Resolved
				&& variedRoles.Outdated && variedRoles.Deleted,
			"mixed",
			caseCount,
			"fixed mixed cases vary every public projection role on preserved "
			"rows");
	}

	[[nodiscard]] auto permutation_rows()
		-> std::vector<conversation>
	{
		std::vector<conversation> result;
		result.reserve(100U);
		for (std::size_t index = 0; index < 100U; ++index) {
			result.push_back({
				.Id          = std::string{ "permutation:" } + std::to_string(index),
				.Kind        = conversation_kind::issue_comment,
				.AuthorLogin = "permutation",
				.Body        = std::string{ "row:" } + std::to_string(index),
			});
		}
		return result;
	}

	[[nodiscard]] auto projections_in_order(
		const std::vector<conversation>& aRows,
		const std::vector<std::size_t>&  aOrder) -> std::vector<conversation>
	{
		std::vector<conversation> result;
		result.reserve(aOrder.size());
		for (const auto row : aOrder) {
			result.push_back(aRows[row]);
		}
		return result;
	}

	auto shuffle(
		std::vector<std::size_t>& aValues,
		fixed_generator&          aGenerator) -> void
	{
		for (auto remaining = aValues.size(); remaining > 1U; --remaining) {
			std::swap(
				aValues[remaining - 1U],
				aValues[aGenerator.bounded(remaining)]);
		}
	}

	[[nodiscard]] auto minimum_moves(
		const std::vector<std::size_t>& aCurrent,
		const std::vector<std::size_t>& aDesired) -> std::size_t
	{
		std::vector<std::size_t> desiredRank(aDesired.size());
		for (std::size_t rank = 0; rank < aDesired.size(); ++rank) {
			desiredRank[aDesired[rank]] = rank;
		}

		std::vector<std::size_t> tails;
		tails.reserve(aCurrent.size());
		for (const auto identity : aCurrent) {
			const auto rank      = desiredRank[identity];
			const auto insertion = std::lower_bound(tails.begin(), tails.end(), rank);
			if (insertion == tails.end()) {
				tails.push_back(rank);
			} else {
				*insertion = rank;
			}
		}
		return aCurrent.size() - tails.size();
	}

	auto test_arbitrary_permutations() -> void
	{
		constexpr std::size_t    caseCount = 5'000U;
		const auto               rows      = permutation_rows();
		std::vector<std::size_t> current(rows.size());
		for (std::size_t index = 0; index < current.size(); ++index) {
			current[index] = index;
		}

		qt::github_conversation_model model;
		signal_audit                  audit;
		observe(model, audit);
		const auto initial = projections_in_order(rows, current);
		model.apply(initial);
		std::vector<QPersistentModelIndex> persistent;
		persistent.reserve(rows.size());
		for (std::size_t row = 0; row < rows.size(); ++row) {
			persistent.emplace_back(model.index(static_cast<int>(row), 0));
		}

		fixed_generator generator{ UINT64_C(0x4250522d5045524d) };
		for (std::size_t caseNumber = 0; caseNumber < caseCount; ++caseNumber) {
			auto desiredOrder = current;
			shuffle(desiredOrder, generator);
			const auto expectedMoves    = minimum_moves(current, desiredOrder);
			const auto desired          = projections_in_order(rows, desiredOrder);
			const auto expected         = expected_for(desired);
			const auto beforeStatistics = model.statistics();
			audit.clear_operations();

			model.apply(desired);

			const auto afterStatistics = model.statistics();
			expect(
				audit.MoveEvents == expectedMoves
					&& audit.MovedRows == expectedMoves
					&& afterStatistics.RowsMoved
							- beforeStatistics.RowsMoved
						== expectedMoves,
				"permutation",
				caseNumber,
				"reconciliation emits the LIS-minimum number of one-row moves");
			expect(
				audit.InsertedRows == 0U && audit.RemovedRows == 0U
					&& audit.ChangedRows == 0U,
				"permutation",
				caseNumber,
				"pure permutation emits no insert, remove, or data change");
			expect(
				audit.ModelResets == 0U
					&& model.statistics().ModelResets == 0U,
				"permutation",
				caseNumber,
				"arbitrary permutations never reset the model");
			validate_model(
				model, expected, "permutation", caseNumber);

			for (std::size_t identity = 0; identity < rows.size(); ++identity) {
				expect(
					persistent[identity].isValid()
						&& QModelIndex{ persistent[identity] }
							== model.index_for_id(rows[identity].Id),
					"permutation",
					caseNumber,
					"every stable row preserves its persistent index");
			}
			current = std::move(desiredOrder);
		}
	}

	[[nodiscard]] auto rotation_rows(
		const std::size_t aCount) -> std::vector<conversation>
	{
		std::vector<conversation> result;
		result.reserve(aCount);
		for (std::size_t index = 0; index < aCount; ++index) {
			result.push_back({
				.Id   = std::string{ "rotation:" } + std::to_string(index),
				.Kind = conversation_kind::issue_comment,
			});
		}
		return result;
	}

	auto expect_one_rotation_move(
		qt::github_conversation_model&   aModel,
		signal_audit&                    aAudit,
		const std::vector<conversation>& aDesired,
		const std::string_view           aSuite,
		const std::size_t                aCaseNumber) -> void
	{
		const auto before = aModel.statistics();
		aAudit.clear_operations();
		aModel.apply(aDesired);
		const auto after = aModel.statistics();

		expect(
			aAudit.MoveEvents == 1U && aAudit.MovedRows == 1U
				&& after.RowsMoved - before.RowsMoved == 1U,
			aSuite,
			aCaseNumber,
			"50,000-row rotation emits exactly one one-row move");
		expect(
			aAudit.InsertedRows == 0U && aAudit.RemovedRows == 0U
				&& aAudit.ChangedRows == 0U,
			aSuite,
			aCaseNumber,
			"50,000-row rotation emits no insert, remove, or data change");
		expect(
			aAudit.ModelResets == 0U && after.ModelResets == 0U,
			aSuite,
			aCaseNumber,
			"50,000-row rotation never resets the model");
		validate_model(
			aModel, expected_for(aDesired), aSuite, aCaseNumber);
	}

	auto test_large_rotations() -> void
	{
		constexpr std::size_t         rowCount = 50'000U;
		auto                          rows     = rotation_rows(rowCount);
		qt::github_conversation_model model;
		signal_audit                  audit;
		observe(model, audit);
		model.apply(rows);
		validate_model(model, expected_for(rows), "rotation", 0U);

		const QPersistentModelIndex first{ model.index(0, 0) };
		const QPersistentModelIndex middle{
			model.index(static_cast<int>(rowCount / 2U), 0)
		};
		const QPersistentModelIndex last{
			model.index(static_cast<int>(rowCount - 1U), 0)
		};

		std::rotate(rows.begin(), rows.begin() + 1, rows.end());
		expect_one_rotation_move(model, audit, rows, "left-rotation", 0U);
		expect(
			first.isValid()
				&& QModelIndex{ first }
					== model.index(static_cast<int>(rowCount - 1U), 0)
				&& middle.isValid()
				&& QModelIndex{ middle }
					== model.index(
						static_cast<int>(rowCount / 2U - 1U), 0)
				&& last.isValid()
				&& QModelIndex{ last }
					== model.index(static_cast<int>(rowCount - 2U), 0),
			"left-rotation",
			0U,
			"large left rotation preserves sampled persistent indexes");

		std::rotate(rows.begin(), rows.end() - 1, rows.end());
		expect_one_rotation_move(model, audit, rows, "right-rotation", 0U);
		expect(
			first.isValid() && QModelIndex{ first } == model.index(0, 0)
				&& middle.isValid()
				&& QModelIndex{ middle }
					== model.index(static_cast<int>(rowCount / 2U), 0)
				&& last.isValid()
				&& QModelIndex{ last }
					== model.index(static_cast<int>(rowCount - 1U), 0),
			"right-rotation",
			0U,
			"large right rotation preserves sampled persistent indexes");
	}
}

auto main(int argc, char** argv) -> int
{
	QCoreApplication application{ argc, argv };
	QCoreApplication::setApplicationName(
		QStringLiteral("BetterPullReviewConversationModelPropertyTests"));

	test_mixed_cases();
	test_arbitrary_permutations();
	test_large_rotations();

	std::cout << "github-conversation-model-property-tests:pass\n";
	return EXIT_SUCCESS;
}
