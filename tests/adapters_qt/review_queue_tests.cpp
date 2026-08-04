#include "bpr/qt/review_queue_model.hpp"

#include <QAbstractItemModel>
#include <QCoreApplication>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	/// \brief Counts the model signals this suite reasons about.
	///
	/// \details `QSignalSpy` lives in Qt6::Test, which this build does not
	/// link. Connecting directly costs a few lines and adds no dependency.
	struct signal_recorder
	{
		explicit signal_recorder(QAbstractItemModel& aModel)
		{
			QObject::connect(
				&aModel,
				&QAbstractItemModel::modelReset,
				[this]() { ++Resets; });
			QObject::connect(
				&aModel,
				&QAbstractItemModel::rowsInserted,
				[this](const QModelIndex&, int, int) { ++Insertions; });
			QObject::connect(
				&aModel,
				&QAbstractItemModel::rowsRemoved,
				[this](const QModelIndex&, int, int) { ++Removals; });
			QObject::connect(
				&aModel,
				&QAbstractItemModel::dataChanged,
				[this](
					const QModelIndex& aFirst,
					const QModelIndex& aLast,
					const QList<int>&) {
					++Changes;
					FirstChangedRow = aFirst.row();
					LastChangedRow  = aLast.row();
				});
		}

		int Resets          = 0;
		int Insertions      = 0;
		int Removals        = 0;
		int Changes         = 0;
		int FirstChangedRow = -1;
		int LastChangedRow  = -1;
	};

	[[nodiscard]] auto item(
		const std::uint8_t               aSeed,
		const core::actionability_reason aReason) -> core::actionable_item
	{
		return core::actionable_item{
			.File     = opaque<core::file_identity>(aSeed),
			.Material = opaque<core::file_material_id>(aSeed),
			.Reason   = aReason,
		};
	}

	void test_the_queue_presents_what_the_core_ranked()
	{
		auto       model  = qt::review_queue_model{ };
		const auto ranked = std::array{
			item(1U, core::actionability_reason::unresolved_defect),
			item(2U, core::actionability_reason::unread_material),
		};
		model.apply(ranked);

		expect(model.rowCount() == 2, "every ranked item reaches the model");
		expect(
			model.item_at(model.index(0)) == ranked[0]
				&& model.item_at(model.index(1)) == ranked[1],
			"the model preserves the order the core decided");
	}

	// The explanation is a core value the adapter renders. An adapter that
	// composed its own reason text would be deciding policy while looking like
	// presentation, so the rendered text must follow the core's reason.
	void test_the_explanation_follows_the_core_reason()
	{
		auto model = qt::review_queue_model{ };
		model.apply(std::array{ item(1U, core::actionability_reason::unread_material) });
		const auto before
			= model.data(model.index(0), qt::review_queue_model::reason_role).toString();

		model.apply(std::array{ item(1U, core::actionability_reason::unresolved_defect) });
		const auto after
			= model.data(model.index(0), qt::review_queue_model::reason_role).toString();

		expect(!before.isEmpty() && !after.isEmpty(), "both reasons render");
		expect(
			before != after,
			"changing the core's reason changes the rendered explanation");
		expect(
			after == qt::explanation_for(core::actionability_reason::unresolved_defect),
			"the rendered text is the one the mapping publishes for that reason");
	}

	// Colour is never the only signal. Every reason carries a distinct textual
	// marker that survives greyscale, a screen reader, and a colour-vision
	// deficiency.
	void test_every_reason_carries_a_distinct_non_colour_marker()
	{
		constexpr auto reasons = std::array{
			core::actionability_reason::unresolved_defect,
			core::actionability_reason::awaiting_verification,
			core::actionability_reason::acceptance_outdated,
			core::actionability_reason::unread_conversation,
			core::actionability_reason::unread_material,
			core::actionability_reason::nothing_to_do,
			core::actionability_reason::excluded,
		};

		auto markers = std::vector<QString>{ };
		for (const auto reason : reasons) {
			const auto marker = qt::marker_for(reason);
			expect(!marker.isEmpty(), "every reason has a marker");
			expect(
				!qt::explanation_for(reason).isEmpty(),
				"every reason has an explanation");
			markers.push_back(marker);
		}

		for (auto first = std::size_t{ 0U }; first < markers.size(); ++first) {
			for (auto second = first + 1U; second < markers.size(); ++second) {
				expect(
					markers[first] != markers[second],
					"two reasons never share one marker");
			}
		}

		// The marker leads the visible label, so the state is readable without
		// consulting a colour.
		auto model = qt::review_queue_model{ };
		model.apply(std::array{ item(1U, core::actionability_reason::unresolved_defect) });
		expect(
			model.data(model.index(0), Qt::DisplayRole)
				.toString()
				.startsWith(qt::marker_for(core::actionability_reason::unresolved_defect)),
			"the visible label leads with the non-colour marker");
		expect(
			model.data(model.index(0), Qt::AccessibleTextRole)
				.toString()
				.contains(
					qt::explanation_for(core::actionability_reason::unresolved_defect)),
			"the accessible text carries the explanation, not just the marker");
	}

	// A reset discards selection and scroll position and tells assistive
	// technology that everything it knew is gone. A bounded change must not.
	void test_a_bounded_change_uses_incremental_signals()
	{
		auto       model   = qt::review_queue_model{ };
		const auto initial = std::array{
			item(1U, core::actionability_reason::unresolved_defect),
			item(2U, core::actionability_reason::unread_material),
			item(3U, core::actionability_reason::unread_conversation),
		};
		model.apply(initial);

		auto       recorder = signal_recorder{ model };
		const auto before   = model.statistics();

		auto moved = initial;
		moved[1]   = item(2U, core::actionability_reason::acceptance_outdated);
		model.apply(moved);

		expect(recorder.Resets == 0, "a changed row does not reset the model");
		expect(recorder.Changes == 1, "exactly one change is announced");
		expect(
			model.statistics().RowsChanged == before.RowsChanged + 1U,
			"only the row that moved is reported changed");

		expect(
			recorder.FirstChangedRow == 1 && recorder.LastChangedRow == 1,
			"the change names the row that actually moved");

		// Re-applying the identical queue announces nothing at all.
		const auto quiet = recorder.Changes;
		model.apply(moved);
		expect(
			recorder.Changes == quiet && recorder.Resets == 0,
			"an unchanged queue produces no signal");
	}

	void test_growth_and_shrink_use_row_signals()
	{
		auto model = qt::review_queue_model{ };
		model.apply(std::array{ item(1U, core::actionability_reason::unread_material) });

		auto recorder = signal_recorder{ model };

		model.apply(std::array{
			item(1U, core::actionability_reason::unread_material),
			item(2U, core::actionability_reason::unread_conversation),
		});
		expect(
			recorder.Insertions == 1 && model.rowCount() == 2,
			"a longer queue inserts rather than resets");

		model.apply(std::array{ item(1U, core::actionability_reason::unread_material) });
		expect(
			recorder.Removals == 1 && model.rowCount() == 1,
			"a shorter queue removes rather than resets");
		expect(
			model.statistics().RowsRemoved == 1U,
			"the removal is counted, so a miscounted erase cannot pass");

		model.clear();
		expect(model.rowCount() == 0, "clearing empties the queue");
		expect(
			recorder.Resets == 0,
			"no path through the model resets it");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QCoreApplication application{ argc, argv };

	test_the_queue_presents_what_the_core_ranked();
	test_the_explanation_follows_the_core_reason();
	test_every_reason_carries_a_distinct_non_colour_marker();
	test_a_bounded_change_uses_incremental_signals();
	test_growth_and_shrink_use_row_signals();

	std::cout << "review-queue-tests:pass\n";
	return EXIT_SUCCESS;
}
