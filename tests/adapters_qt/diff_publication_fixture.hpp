#ifndef BPR_TESTS_ADAPTERS_QT_DIFF_PUBLICATION_FIXTURE_HPP
#define BPR_TESTS_ADAPTERS_QT_DIFF_PUBLICATION_FIXTURE_HPP

#include "bpr/application/diff_runtime_ports.hpp"

#include <memory>
#include <utility>

namespace bpr::qt::test {
	/// Prepares immutable fixture input before entering the Qt publication path.
	[[nodiscard]] inline auto prepare_publication(application::diff_publication aPublication)
		-> application::diff_publication
	{
		if (!aPublication.Index) {
			auto index = core::build_diff_semantic_index(aPublication.Result);
			if (index) {
				aPublication.Index = std::make_shared<const core::diff_semantic_index>(std::move(*index));
			}
		}
		return aPublication;
	}
} // namespace bpr::qt::test

#endif // BPR_TESTS_ADAPTERS_QT_DIFF_PUBLICATION_FIXTURE_HPP
