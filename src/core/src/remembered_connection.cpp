#include "bpr/core/remembered_connection.hpp"
#include "bpr/core/github_hostname.hpp"

#include <algorithm>
#include <limits>

namespace bpr::core {
	namespace {
		auto same_identity(const remembered_connection& aLeft, const remembered_connection& aRight) noexcept -> bool
		{
			return aLeft.Profile == aRight.Profile && aLeft.ProfileRevision == aRight.ProfileRevision
				&& aLeft.Host == aRight.Host && aLeft.Hostname == aRight.Hostname && aLeft.Account == aRight.Account
				&& aLeft.Login == aRight.Login && aLeft.Server == aRight.Server && aLeft.Transport == aRight.Transport;
		}
		auto prepare(const remembered_connection_state& aState, const std::uint64_t aExpectedRevision) -> remembered_connection_result
		{
			if (!valid(aState)) {
				return std::unexpected(remembered_connection_failure::invalid);
			}
			if (aState.Revision != aExpectedRevision) {
				return std::unexpected(remembered_connection_failure::stale);
			}
			if (aState.Revision == std::numeric_limits<std::uint64_t>::max()) {
				return std::unexpected(remembered_connection_failure::exhausted);
			}
			auto next = aState;
			++next.Revision;
			return next;
		}
	}

	auto valid(const remembered_connection& aRecord) noexcept -> bool
	{
		if (!valid_github_canonical_hostname(aRecord.Hostname)
			|| (aRecord.Server != remembered_connection_server::github_com && aRecord.Server != remembered_connection_server::ghes_3_18)
			|| (aRecord.Server == remembered_connection_server::github_com) != (aRecord.Hostname == "github.com")
			|| (aRecord.Transport != remembered_connection_transport::native_device && aRecord.Transport != remembered_connection_transport::github_cli)) {
			return false;
		}
		if (!aRecord.ReuseEnabled && aRecord.Profile.value() == 0U) {
			return aRecord.ProfileRevision == 0U && aRecord.Host.value() == 0U && aRecord.Account.value() == 0U
				&& aRecord.Login.empty() && aRecord.SelectedOrder == 0U && aRecord.Transport == remembered_connection_transport::native_device;
		}
		return aRecord.Profile.value() != 0U && aRecord.ProfileRevision != 0U && aRecord.Host.value() != 0U && aRecord.Account.value() != 0U
			&& aRecord.SelectedOrder != 0U && !aRecord.Login.empty() && aRecord.Login.size() <= 64U
			&& aRecord.Login.front() != '-' && aRecord.Login.front() != '_'
			&& std::ranges::all_of(aRecord.Login, [](char aCharacter) {
				   return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= 'A' && aCharacter <= 'Z')
					   || (aCharacter >= '0' && aCharacter <= '9') || aCharacter == '-' || aCharacter == '_';
			   });
	}

	auto valid(const remembered_connection_state& aState) noexcept -> bool
	{
		if (aState.BprProfile.value() == 0U || aState.Revision == 0U || aState.Connections.size() > maximum_remembered_connections) {
			return false;
		}
		for (std::size_t index = 0U; index < aState.Connections.size(); ++index) {
			const auto& row = aState.Connections[index];
			if ((index != 0U && aState.Connections[index - 1U].Hostname >= row.Hostname) || !valid(row) || row.Profile.value() > aState.LastProfileId || row.Host.value() > aState.LastHostId || row.SelectedOrder > aState.LastSelectedOrder) {
				return false;
			}
			for (std::size_t prior = 0U; prior < index; ++prior) {
				const auto& other = aState.Connections[prior];
				if (other.Hostname == row.Hostname || (row.Profile.value() != 0U && other.Profile == row.Profile)
					|| (row.Host.value() != 0U && other.Host == row.Host) || (row.SelectedOrder != 0U && other.SelectedOrder == row.SelectedOrder)) {
					return false;
				}
			}
		}
		if (aState.Retired.size() > maximum_retired_connections) {
			return false;
		}
		for (std::size_t index = 0U; index < aState.Retired.size(); ++index) {
			const auto& row = aState.Retired[index];
			if (row.ReuseEnabled || row.Profile.value() == 0U || row.Transport != remembered_connection_transport::native_device || !valid(row)
				|| row.Profile.value() > aState.LastProfileId || row.Host.value() > aState.LastHostId || row.SelectedOrder > aState.LastSelectedOrder
				|| (index != 0U && aState.Retired[index - 1U].SelectedOrder >= row.SelectedOrder)) {
				return false;
			}
			for (std::size_t prior = 0U; prior < index; ++prior) {
				const auto& other = aState.Retired[prior];
				if (other.Profile == row.Profile || (other.Hostname == row.Hostname && other.Account == row.Account)) {
					return false;
				}
			}
			if (std::ranges::any_of(aState.Connections, [&row](const auto& aSelected) { return aSelected.Profile == row.Profile || aSelected.SelectedOrder == row.SelectedOrder; })) {
				return false;
			}
		}
		return true;
	}

	auto valid_remembered_connection_update(const remembered_connection_state& aBefore, const remembered_connection_state& aAfter) noexcept -> bool
	{
		if (!valid(aBefore) || !valid(aAfter) || aBefore.BprProfile != aAfter.BprProfile
			|| aBefore.Revision == std::numeric_limits<std::uint64_t>::max() || aAfter.Revision != aBefore.Revision + 1U) {
			return false;
		}
		auto profileHigh = aBefore.LastProfileId;
		auto hostHigh    = aBefore.LastHostId;
		auto orderHigh   = aBefore.LastSelectedOrder;
		for (const auto& row : aAfter.Connections) {
			const auto oldProfile = std::ranges::find(aBefore.Connections, row.Profile, &remembered_connection::Profile);
			// [[BPR-1494]]. An identity this profile retired may return only exactly as it was.
			const auto retired = std::ranges::find(aBefore.Retired, row.Profile, &remembered_connection::Profile);
			if (row.Profile.value() != 0U
				&& (oldProfile != aBefore.Connections.end() ? !same_identity(*oldProfile, row)
						: retired != aBefore.Retired.end()  ? !same_identity(*retired, row)
															: row.Profile.value() <= aBefore.LastProfileId)) {
				return false;
			}
			const auto oldHost = std::ranges::find(aBefore.Connections, row.Hostname, &remembered_connection::Hostname);
			if (row.Host.value() != 0U && (oldHost != aBefore.Connections.end() && oldHost->Host.value() != 0U ? row.Host != oldHost->Host : row.Host.value() <= aBefore.LastHostId)) {
				return false;
			}
			if (row.SelectedOrder != 0U && (oldHost == aBefore.Connections.end() || row.SelectedOrder != oldHost->SelectedOrder) && row.SelectedOrder <= aBefore.LastSelectedOrder) {
				return false;
			}
			profileHigh = std::max(profileHigh, row.Profile.value());
			hostHigh    = std::max(hostHigh, row.Host.value());
			orderHigh   = std::max(orderHigh, row.SelectedOrder);
		}
		// A retired identity is one this profile already held, unchanged: no update invents history.
		for (const auto& row : aAfter.Retired) {
			const auto  selected = std::ranges::find(aBefore.Connections, row.Profile, &remembered_connection::Profile);
			const auto  retired  = std::ranges::find(aBefore.Retired, row.Profile, &remembered_connection::Profile);
			const auto* source   = selected != aBefore.Connections.end() ? &*selected : retired != aBefore.Retired.end() ? &*retired
																														 : nullptr;
			if (source == nullptr || !same_identity(*source, row) || source->SelectedOrder != row.SelectedOrder) {
				return false;
			}
		}
		return aAfter.LastProfileId == profileHigh && aAfter.LastHostId == hostHigh && aAfter.LastSelectedOrder == orderHigh;
	}

	auto remember_connection(const remembered_connection_state& aState, const std::uint64_t aExpectedRevision, remembered_connection aRecord) -> remembered_connection_result
	{
		auto next = prepare(aState, aExpectedRevision);
		if (!next) {
			return next;
		}
		if (aState.LastSelectedOrder == std::numeric_limits<std::uint64_t>::max()) {
			return std::unexpected(remembered_connection_failure::exhausted);
		}
		aRecord.ReuseEnabled  = true;
		aRecord.SelectedOrder = ++next->LastSelectedOrder;
		next->LastProfileId   = std::max(next->LastProfileId, aRecord.Profile.value());
		next->LastHostId      = std::max(next->LastHostId, aRecord.Host.value());
		auto found            = std::ranges::find(next->Connections, aRecord.Hostname, &remembered_connection::Hostname);
		// [[BPR-1494]]. A CLI selection leaves no native history for its server, so a
		// later native sign-in there is a fresh identity. A native selection supersedes
		// its own account's retired identity and retires another account's.
		if (aRecord.Transport == remembered_connection_transport::github_cli) {
			std::erase_if(next->Retired, [&aRecord](const auto& aRow) { return aRow.Hostname == aRecord.Hostname; });
		} else {
			std::erase_if(next->Retired, [&aRecord](const auto& aRow) { return aRow.Hostname == aRecord.Hostname && (aRow.Account == aRecord.Account || aRow.Profile == aRecord.Profile); });
			if (found != next->Connections.end() && found->Profile.value() != 0U && found->Transport == remembered_connection_transport::native_device
				&& found->Profile != aRecord.Profile && found->Account != aRecord.Account) {
				auto retired         = *found;
				retired.ReuseEnabled = false;
				next->Retired.push_back(std::move(retired));
				std::ranges::sort(next->Retired, { }, &remembered_connection::SelectedOrder);
				if (next->Retired.size() > maximum_retired_connections) {
					next->Retired.erase(next->Retired.begin());
				}
			}
		}
		if (found == next->Connections.end()) {
			if (next->Connections.size() == maximum_remembered_connections) {
				return std::unexpected(remembered_connection_failure::capacity);
			}
			next->Connections.push_back(std::move(aRecord));
		} else {
			*found = std::move(aRecord);
		}
		std::ranges::sort(next->Connections, { }, &remembered_connection::Hostname);
		return valid_remembered_connection_update(aState, *next) ? std::move(next) : std::unexpected(remembered_connection_failure::invalid);
	}

	auto disable_connection_reuse(const remembered_connection_state& aState, const std::uint64_t aExpectedRevision, const std::string_view aHostname) -> remembered_connection_result
	{
		auto next = prepare(aState, aExpectedRevision);
		if (!next) {
			return next;
		}
		auto found = std::ranges::find(next->Connections, aHostname, &remembered_connection::Hostname);
		if (found == next->Connections.end()) {
			if (next->Connections.size() == maximum_remembered_connections) {
				return std::unexpected(remembered_connection_failure::capacity);
			}
			remembered_connection row;
			row.Hostname     = aHostname;
			row.Server       = aHostname == "github.com" ? remembered_connection_server::github_com : remembered_connection_server::ghes_3_18;
			row.ReuseEnabled = false;
			next->Connections.push_back(std::move(row));
		} else {
			found->ReuseEnabled = false;
		}
		std::ranges::sort(next->Connections, { }, &remembered_connection::Hostname);
		return valid_remembered_connection_update(aState, *next) ? std::move(next) : std::unexpected(remembered_connection_failure::invalid);
	}

	auto forget_connection(const remembered_connection_state& aState, const std::uint64_t aExpectedRevision, const std::string_view aHostname) -> remembered_connection_result
	{
		auto next = prepare(aState, aExpectedRevision);
		if (!next) {
			return next;
		}
		if (!valid_github_canonical_hostname(aHostname)) {
			return std::unexpected(remembered_connection_failure::invalid);
		}
		std::erase_if(next->Connections, [aHostname](const auto& aRow) { return aRow.Hostname == aHostname; });
		std::erase_if(next->Retired, [aHostname](const auto& aRow) { return aRow.Hostname == aHostname; });
		return next;
	}

	auto select_remembered_connection(const remembered_connection_state& aState, const std::optional<std::string_view> aHostname) -> std::optional<remembered_connection>
	{
		if (!valid(aState)) {
			return std::nullopt;
		}
		const remembered_connection* selected = nullptr;
		for (const auto& row : aState.Connections) {
			if (row.ReuseEnabled && (!aHostname || row.Hostname == *aHostname) && (!selected || row.SelectedOrder > selected->SelectedOrder)) {
				selected = &row;
			}
		}
		return selected ? std::optional{ *selected } : std::nullopt;
	}

	auto adopt_native_connection(const remembered_connection_state& aState, const remembered_connection& aSignIn) -> std::optional<remembered_connection>
	{
		// Only the provisional identity a sign-in began on, never a remembered one, re-binds.
		if (!valid(aState) || aSignIn.Transport != remembered_connection_transport::native_device || aSignIn.Account.value() == 0U
			|| aSignIn.Profile.value() <= aState.LastProfileId) {
			return std::nullopt;
		}
		const auto selected = std::ranges::find(aState.Connections, aSignIn.Hostname, &remembered_connection::Hostname);
		if (selected != aState.Connections.end() && selected->Transport == remembered_connection_transport::github_cli) {
			return std::nullopt;
		}
		const auto same = [&aSignIn](const remembered_connection& aRow) {
			return aRow.Profile.value() != 0U && aRow.Profile != aSignIn.Profile && aRow.Transport == remembered_connection_transport::native_device
				&& aRow.Hostname == aSignIn.Hostname && aRow.Server == aSignIn.Server && aRow.Host == aSignIn.Host
				&& aRow.Account == aSignIn.Account && aRow.Login == aSignIn.Login;
		};
		if (selected != aState.Connections.end() && same(*selected)) {
			return *selected;
		}
		const auto retired = std::ranges::find_if(aState.Retired.rbegin(), aState.Retired.rend(), same);
		return retired != aState.Retired.rend() ? std::optional{ *retired } : std::nullopt;
	}
}
