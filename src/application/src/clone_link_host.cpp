#include "bpr/application/clone_link_host.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <utility>

namespace bpr::application {
	namespace {
		[[nodiscard]] constexpr auto lower(const char aCharacter) noexcept -> char
		{
			return aCharacter >= 'A' && aCharacter <= 'Z' ? static_cast<char>(aCharacter - 'A' + 'a') : aCharacter;
		}

		[[nodiscard]] auto same_text(const std::string_view aLeft, const std::string_view aRight) noexcept -> bool
		{
			return aLeft.size() == aRight.size() && std::ranges::equal(aLeft, aRight, [](const char aA, const char aB) { return lower(aA) == lower(aB); });
		}

		/// The link that belongs to a repository: same host, then the same
		/// identity when both know one, else the same owner and name.
		[[nodiscard]] auto belongs(const repository_link& aLink, const clone_repository& aRepository) noexcept -> bool
		{
			if (!same_text(aLink.Host, aRepository.Host)) {
				return false;
			}
			if (aLink.Repository != 0U && aRepository.Repository != 0U) {
				return aLink.Repository == aRepository.Repository;
			}
			return same_text(aLink.Owner, aRepository.Owner) && same_text(aLink.Name, aRepository.Name);
		}

		[[nodiscard]] auto target_of(const clone_repository& aRepository) -> core::clone_target
		{
			return { .Host = aRepository.Host, .Owner = aRepository.Owner, .Name = aRepository.Name };
		}
	} // namespace

	struct clone_link_host::implementation final : std::enable_shared_from_this<implementation>
	{
		explicit implementation(clone_link_dependencies aDependencies)
			: Dependencies{ std::move(aDependencies) }
		{ }

		template <typename Task, typename Settle>
		void off_lane(Task aTask, Settle aSettle)
		{
			if (Closing || Dependencies.Effects == nullptr || Dependencies.Decision == nullptr) {
				return;
			}
			++Outstanding;
			try {
				Dependencies.Effects->post([self = shared_from_this(), task = std::move(aTask), settle = std::move(aSettle)]() mutable {
					if (self->Closing) {
						--self->Outstanding;
						return;
					}
					auto result = std::optional<decltype(task())>{ };
					try {
						result.emplace(task());
					} catch (...) { }
					try {
						self->Dependencies.Decision->post([owner = self, reply = std::move(settle), answer = std::move(result)]() mutable {
							--owner->Outstanding;
							if (!owner->Closing && answer) {
								reply(std::move(*answer));
							}
						});
					} catch (...) {
						--self->Outstanding;
					}
				});
			} catch (...) {
				--Outstanding;
			}
		}

		void publish()
		{
			auto& view       = Projection;
			view.Suggestions = std::vector<std::string>{ };
			for (const auto& suggestion : Existing) {
				if (!std::ranges::contains(view.Folders, suggestion)) {
					view.Suggestions.push_back(suggestion);
				}
			}
			++view.Generation;
			if (Dependencies.Publish) {
				try {
					Dependencies.Publish(view);
				} catch (...) { }
			}
		}

		void save_folders()
		{
			if (Dependencies.Store == nullptr) {
				return;
			}
			off_lane([store = Dependencies.Store, folders = Projection.Folders] { return store->store_clone_folders(folders); }, [](bool) { });
		}

		[[nodiscard]] auto link_for(const clone_repository& aRepository) const -> std::optional<repository_link>
		{
			const auto found = std::ranges::find_if(Projection.Links, [&aRepository](const repository_link& aLink) { return belongs(aLink, aRepository); });
			return found == Projection.Links.end() ? std::nullopt : std::optional{ *found };
		}

		/// Checks the observed repository's link again, by the same metadata
		/// read as discovery.
		void verify()
		{
			const auto generation = ++VerifyGeneration;
			Projection.Linked.reset();
			Projection.Problem   = clone_link_problem::none;
			Projection.Verifying = false;
			if (!Projection.Repository) {
				publish();
				return;
			}
			auto remembered = link_for(*Projection.Repository);
			if (!remembered || remembered->PrivateCopy) {
				Projection.Linked = std::move(remembered);
				publish();
				return;
			}
			Projection.Verifying = true;
			publish();
			auto clonePath = remembered->Path;
			off_lane(
				[read = Dependencies.Reads.Read, path = std::move(clonePath)] { return read ? read(path) : std::optional<core::clone_candidate>{ }; },
				[self = shared_from_this(), generation, link = std::move(*remembered)](std::optional<core::clone_candidate> aRead) mutable {
					if (generation != self->VerifyGeneration || !self->Projection.Repository) {
						return;
					}
					self->Projection.Verifying = false;
					if (!aRead) {
						self->Projection.Problem = clone_link_problem::not_a_clone;
					} else {
						const auto candidates = std::vector{ std::move(*aRead) };
						auto       matches    = core::match_clones(candidates, target_of(*self->Projection.Repository), self->Projection.Folders, link.Path);
						if (matches.empty()) {
							// A renamed repository: its clone's remote still names
							// the name it was linked under, which GitHub redirects.
							matches = core::match_clones(candidates, core::clone_target{ .Host = link.Host, .Owner = link.Owner, .Name = link.Name }, self->Projection.Folders, link.Path);
						}
						if (matches.empty()) {
							self->Projection.Problem = clone_link_problem::no_matching_remote;
						} else {
							link.Remote             = matches.front().Remote;
							self->Projection.Linked = std::move(link);
						}
					}
					self->publish();
				});
		}

		void store_link(repository_link aLink)
		{
			auto& links = Projection.Links;
			std::erase_if(links, [&aLink](const repository_link& aOther) {
				return same_text(aOther.Host, aLink.Host) && aOther.Repository == aLink.Repository && same_text(aOther.Owner, aLink.Owner) && same_text(aOther.Name, aLink.Name);
			});
			if (Projection.Repository) {
				std::erase_if(links, [this](const repository_link& aOther) { return belongs(aOther, *Projection.Repository); });
			}
			links.push_back(aLink);
			Projection.Linked  = aLink;
			Projection.Problem = clone_link_problem::none;
			if (Dependencies.Store != nullptr) {
				off_lane([store = Dependencies.Store, link = std::move(aLink)] { return store->store_link(link); }, [](bool) { });
			}
		}

		void discover()
		{
			const auto generation = ++DiscoverGeneration;
			if (!Projection.Repository || Projection.Folders.empty()) {
				Projection.Matches.clear();
				Projection.Discovering = false;
				Projection.Discovered  = true;
				publish();
				return;
			}
			Projection.Discovering = true;
			publish();
			off_lane(
				[scan = Dependencies.Reads.Scan, folders = Projection.Folders] { return scan ? scan(folders, { }) : std::vector<core::clone_candidate>{ }; },
				[self = shared_from_this(), generation](std::vector<core::clone_candidate> aFound) {
					if (generation != self->DiscoverGeneration || !self->Projection.Repository) {
						return;
					}
					const auto linked            = self->Projection.Linked ? self->Projection.Linked->Path : std::string{ };
					self->Projection.Matches     = core::match_clones(aFound, target_of(*self->Projection.Repository), self->Projection.Folders, linked);
					self->Projection.Discovering = false;
					self->Projection.Discovered  = true;
					self->publish();
				});
		}

		clone_link_dependencies  Dependencies;
		clone_link_projection    Projection;
		std::vector<std::string> Existing;
		std::uint64_t            VerifyGeneration   = 0U;
		std::uint64_t            DiscoverGeneration = 0U;
		std::uint64_t            ChoiceGeneration   = 0U;
		std::atomic_bool         Closing{ false };
		std::atomic_size_t       Outstanding{ 0U };
	};

	clone_link_host::clone_link_host(clone_link_dependencies aDependencies)
		: Implementation{ std::make_shared<implementation>(std::move(aDependencies)) }
	{ }

	clone_link_host::~clone_link_host() { shutdown(); }

	void clone_link_host::load()
	{
		auto& host = *Implementation;
		struct loaded
		{
			std::optional<std::vector<std::string>>     Folders;
			std::optional<std::vector<repository_link>> Links;
			std::vector<std::string>                    Existing;
		};
		host.off_lane(
			[store = host.Dependencies.Store, existing = host.Dependencies.Reads.Existing, home = host.Dependencies.Home, windows = host.Dependencies.Windows] {
				auto answer = loaded{ };
				if (store != nullptr) {
					answer.Folders = store->load_clone_folders();
					answer.Links   = store->load_links();
				}
				if (existing) {
					const auto usual = core::usual_clone_folders(home, windows);
					answer.Existing  = existing(usual);
				}
				return answer;
			},
			[self = Implementation](loaded aLoaded) {
				if (aLoaded.Folders) {
					self->Projection.Folders = std::move(*aLoaded.Folders);
				}
				if (aLoaded.Links) {
					self->Projection.Links = std::move(*aLoaded.Links);
				}
				self->Existing          = std::move(aLoaded.Existing);
				self->Projection.Loaded = true;
				self->verify();
			});
	}

	void clone_link_host::observe_repository(std::optional<clone_repository> aRepository)
	{
		auto& host = *Implementation;
		if (host.Projection.Repository == aRepository) {
			return;
		}
		host.Projection.Repository = std::move(aRepository);
		host.Projection.Matches.clear();
		host.Projection.Discovered = false;
		host.verify();
	}

	void clone_link_host::discover() { Implementation->discover(); }

	void clone_link_host::set_clone_folders(std::vector<std::string> aFolders)
	{
		auto& host = *Implementation;
		auto  kept = std::vector<std::string>{ };
		for (auto& folder : aFolders) {
			if (kept.size() < core::maximum_clone_folders && core::valid_clone_folder(folder, host.Dependencies.Windows) && !std::ranges::contains(kept, folder)) {
				kept.push_back(std::move(folder));
			}
		}
		if (kept == host.Projection.Folders) {
			return;
		}
		host.Projection.Folders = std::move(kept);
		host.save_folders();
		host.discover();
	}

	void clone_link_host::add_clone_folder(std::string aFolder)
	{
		auto folders = Implementation->Projection.Folders;
		folders.push_back(std::move(aFolder));
		set_clone_folders(std::move(folders));
	}

	void clone_link_host::remove_clone_folder(const std::string& aFolder)
	{
		auto folders = Implementation->Projection.Folders;
		std::erase(folders, aFolder);
		set_clone_folders(std::move(folders));
	}

	void clone_link_host::choose_clone(std::string aPath, std::function<void(clone_choice_answer)> aAnswered)
	{
		auto& host = *Implementation;
		if (!host.Projection.Repository) {
			if (aAnswered) {
				aAnswered(clone_choice_answer::not_saved);
			}
			return;
		}
		const auto generation = ++host.ChoiceGeneration;
		auto       inspected  = aPath;
		host.off_lane(
			[read = host.Dependencies.Reads.Read, path = std::move(inspected)] { return read ? read(path) : std::optional<core::clone_candidate>{ }; },
			[self = Implementation, generation, answered = std::move(aAnswered)](std::optional<core::clone_candidate> aRead) {
				if (generation != self->ChoiceGeneration || !self->Projection.Repository) {
					return;
				}
				const auto reply = [&answered](const clone_choice_answer aAnswer) {
					if (answered) {
						answered(aAnswer);
					}
				};
				if (!aRead) {
					reply(clone_choice_answer::not_a_clone);
					return;
				}
				const auto& repository = *self->Projection.Repository;
				const auto  candidates = std::vector{ *aRead };
				const auto  matches    = core::match_clones(candidates, target_of(repository), self->Projection.Folders, aRead->Path);
				if (matches.empty()) {
					reply(clone_choice_answer::no_matching_remote);
					return;
				}
				auto link       = repository_link{ };
				link.Host       = repository.Host;
				link.Repository = repository.Repository;
				link.Owner      = repository.Owner;
				link.Name       = repository.Name;
				link.Path       = matches.front().Path;
				link.Remote     = matches.front().Remote;
				if (!valid(link)) {
					reply(clone_choice_answer::not_saved);
					return;
				}
				self->store_link(std::move(link));
				self->publish();
				reply(clone_choice_answer::linked);
			});
	}

	void clone_link_host::use_private_copy()
	{
		auto& host = *Implementation;
		if (!host.Projection.Repository) {
			return;
		}
		const auto& repository = *host.Projection.Repository;
		auto        link       = repository_link{ };
		link.Host              = repository.Host;
		link.Repository        = repository.Repository;
		link.Owner             = repository.Owner;
		link.Name              = repository.Name;
		link.PrivateCopy       = true;
		if (!valid(link)) {
			return;
		}
		host.store_link(std::move(link));
		host.publish();
	}

	void clone_link_host::forget(const repository_link& aLink)
	{
		auto& host = *Implementation;
		std::erase(host.Projection.Links, aLink);
		if (host.Projection.Linked == aLink) {
			host.Projection.Linked.reset();
		}
		if (host.Dependencies.Store != nullptr) {
			host.off_lane([store = host.Dependencies.Store, link = aLink] { return store->forget_link(link.Host, link.Repository, link.Owner, link.Name); }, [](bool) { });
		}
		host.publish();
	}

	auto clone_link_host::repository_chosen() const noexcept -> bool
	{
		const auto& view = Implementation->Projection;
		return view.Repository && view.Linked && view.Problem == clone_link_problem::none;
	}

	auto clone_link_host::projection() const -> clone_link_projection { return Implementation->Projection; }

	void clone_link_host::shutdown() noexcept { Implementation->Closing = true; }

	auto clone_link_host::is_quiescent() const noexcept -> bool { return Implementation->Outstanding == 0U; }
} // namespace bpr::application
