#ifndef BPR_APPLICATION_DISCUSS_PREFERENCES_HOST_HPP
#define BPR_APPLICATION_DISCUSS_PREFERENCES_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/core/discuss_preferences.hpp"

#include <memory>
#include <string_view>

/// \file
/// \brief [[BPR-2171]]. The profile's Discuss preferences: each repository's
/// bot marks and the column's width. Read once when the profile starts,
/// written off the decision lane after each change.
namespace bpr::application {
	/// \brief The store the preferences are read from and written to.
	class discuss_preferences_port
	{
	public:
		virtual ~discuss_preferences_port() = default;

		/// \brief Writes the whole preferences. False when it could not.
		[[nodiscard]] virtual auto store(const core::discuss_preferences& aPreferences) -> bool = 0;
	};

	/// \brief Holds one profile's Discuss preferences for every window.
	/// \details A change is the profile's at once: each window asks for what
	/// it shows. Writes are serialized on the effect lane, one at a time,
	/// each carrying the latest preferences. A store a newer build wrote
	/// keeps them for this session only.
	class discuss_preferences_host final
	{
	public:
		discuss_preferences_host(discuss_preferences_port& aStore, effect_executor& aEffects, decision_executor& aDecisions, core::discuss_preferences_read aLoaded);
		~discuss_preferences_host();

		discuss_preferences_host(const discuss_preferences_host&)                    = delete;
		auto operator=(const discuss_preferences_host&) -> discuss_preferences_host& = delete;
		discuss_preferences_host(discuss_preferences_host&&)                         = delete;
		auto operator=(discuss_preferences_host&&) -> discuss_preferences_host&      = delete;

		[[nodiscard]] auto preferences() const noexcept -> const core::discuss_preferences&;
		/// \brief Mark as bot (\p aBot) or Not a bot in one repository. False
		/// when nothing changed.
		auto mark(std::string_view aRepository, std::string_view aLogin, bool aBot) -> bool;
		/// \brief The column's width. False when nothing changed.
		auto set_column(core::discuss_column aColumn) -> bool;
		[[nodiscard]] auto recovered() const noexcept -> bool;
		[[nodiscard]] auto durable() const noexcept -> bool;
		/// \brief No write running or waiting.
		[[nodiscard]] auto quiescent() const noexcept -> bool;
		/// \brief Takes no more changes; one waiting is posted at once.
		void shutdown() noexcept;

	private:
		struct implementation;
		std::shared_ptr<implementation> Impl;
	};
}

#endif
