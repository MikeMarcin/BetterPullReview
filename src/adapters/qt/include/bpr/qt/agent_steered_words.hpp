#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_STEERED_WORDS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_STEERED_WORDS_HPP

#include "bpr/core/agent_steering.hpp"

#include <QString>

/// \file
/// \brief RFC-BPR-0126 AC05 ([[BPR-2153]], [[BPR-2154]]). What a steered
/// message's turn says about it. The Agent tab drew these words (look
/// steer-v002); every tour chat and a local draft's discussion say the same.
namespace bpr::qt {
	/// \brief Where it went in and how it ended: "Steered after step 3", "...
	/// The agent may not have taken it."
	[[nodiscard]] auto agent_steered_state_words(const core::agent_steered_message& aMessage) -> QString;
	/// \brief "You steered: WORDS. STATE", for assistive technology.
	[[nodiscard]] auto agent_steered_accessible_name(const core::agent_steered_message& aMessage) -> QString;
} // namespace bpr::qt

#endif
