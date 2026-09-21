#include "bpr/adapters/update/release_update_installation.hpp"
#include "release_update_transaction.hpp"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <copyfile.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <map>
#include <utility>

namespace bpr::adapters::update {
	namespace {
		namespace fs                          = std::filesystem;
		using reason                          = core::release_update_reason;
		constexpr auto        executable_name = "Contents/MacOS/BetterPullReview";
		constexpr std::size_t maximum_journal = 65536U;
		constexpr std::size_t maximum_cleanup = 16U * 1024U * 1024U;
		struct fd_owner
		{
			int Value = -1;
			explicit fd_owner(int aValue = -1)
				: Value{ aValue }
			{ }
			~fd_owner()
			{
				if (Value >= 0) {
					close(Value);
				}
			}
			fd_owner(const fd_owner&)                    = delete;
			auto operator=(const fd_owner&) -> fd_owner& = delete;
			fd_owner(fd_owner&& aOther) noexcept
				: Value{ std::exchange(aOther.Value, -1) }
			{ }
			auto operator=(fd_owner&& aOther) noexcept -> fd_owner&
			{
				if (this != &aOther) {
					if (Value >= 0) {
						close(Value);
					}
					Value = std::exchange(aOther.Value, -1);
				}
				return *this;
			}
		};
		auto text(std::string_view aText) -> QString { return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())); }
		auto unique_name() -> std::string { return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(); }
		auto valid_id(std::string_view aId) -> bool
		{
			if (aId.size() != 36U) {
				return false;
			}
			for (std::size_t index = 0U; index < aId.size(); ++index) {
				const auto byte = aId[index];
				if (index == 8U || index == 13U || index == 18U || index == 23U) {
					if (byte != '-') {
						return false;
					}
				} else if (!((byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f'))) {
					return false;
				}
			}
			return true;
		}
		auto inode(const fs::path& aPath) -> std::string
		{
			struct stat info{ };
			if (lstat(aPath.c_str(), &info) != 0 || S_ISLNK(info.st_mode)) {
				return { };
			}
			return std::to_string(info.st_dev) + ":" + std::to_string(info.st_ino);
		}
		auto private_directory(const fs::path& aPath) -> bool
		{
			std::error_code error;
			struct stat     info{ };
			return aPath.is_absolute() && fs::canonical(aPath, error) == aPath && !error
				&& lstat(aPath.c_str(), &info) == 0 && S_ISDIR(info.st_mode)
				&& info.st_uid == getuid() && (info.st_mode & 0077U) == 0;
		}
		auto open_private(const fs::path& aPath, bool aCreate = false) -> fd_owner
		{
			fd_owner    result{ open(aPath.c_str(), O_RDWR | O_NOFOLLOW | O_CLOEXEC | (aCreate ? O_CREAT : 0), 0600) };
			struct stat info{ };
			if (result.Value < 0 || fstat(result.Value, &info) != 0 || !S_ISREG(info.st_mode)
				|| info.st_nlink != 1 || info.st_uid != getuid() || (info.st_mode & 0077U) != 0) {
				return fd_owner{ };
			}
			return result;
		}
		auto read_record(const fs::path& aPath, std::size_t aMaximum = maximum_journal) -> std::optional<QJsonObject>
		{
			auto        file = open_private(aPath);
			struct stat info{ };
			if (file.Value < 0 || fstat(file.Value, &info) != 0 || info.st_size <= 0
				|| info.st_size > static_cast<off_t>(aMaximum)) {
				return { };
			}
			QByteArray bytes(static_cast<qsizetype>(info.st_size), '\0');
			if (pread(file.Value, bytes.data(), static_cast<std::size_t>(bytes.size()), 0) != bytes.size()) {
				return { };
			}
			QJsonParseError error{ };
			const auto      document = QJsonDocument::fromJson(bytes, &error);
			return error.error == QJsonParseError::NoError && document.isObject() ? std::optional{ document.object() } : std::nullopt;
		}
		auto write_bytes(int aFile, const QByteArray& aBytes) -> bool
		{
			std::size_t offset = 0;
			while (offset < static_cast<std::size_t>(aBytes.size())) {
				const auto count = write(aFile, aBytes.constData() + offset, static_cast<std::size_t>(aBytes.size()) - offset);
				if (count <= 0) {
					return false;
				}
				offset += static_cast<std::size_t>(count);
			}
			return fsync(aFile) == 0;
		}
		auto atomic_record(const fs::path& aDirectory, const char* aName, const QJsonObject& aRecord, std::size_t aMaximum = maximum_journal) -> bool
		{
			if (!private_directory(aDirectory)) {
				return false;
			}
			fd_owner directory{ open(aDirectory.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
			if (directory.Value < 0) {
				return false;
			}
			const auto bytes = QJsonDocument{ aRecord }.toJson(QJsonDocument::Compact);
			if (bytes.size() > static_cast<qsizetype>(aMaximum)) {
				return false;
			}
			const auto temporary = std::string{ aName } + ".next-" + unique_name();
			fd_owner   file{ openat(directory.Value, temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600) };
			if (file.Value < 0 || !write_bytes(file.Value, bytes)) {
				return false;
			}
			return renameat(directory.Value, temporary.c_str(), directory.Value, aName) == 0 && fsync(directory.Value) == 0;
		}
		auto control_for(const fs::path& aInstalled) -> fs::path
		{
			const auto digest = QCryptographicHash::hash(text(aInstalled.string()).toUtf8(), QCryptographicHash::Sha256).toHex();
			return aInstalled.parent_path() / (".bpr-update-" + digest.left(24).toStdString());
		}
		auto valid_installed(const fs::path& aInstalled) -> bool
		{
			std::error_code error;
			return aInstalled.is_absolute() && aInstalled.filename() == "BetterPullReview.app"
				&& aInstalled.lexically_normal() == aInstalled
				&& fs::canonical(aInstalled.parent_path(), error) == aInstalled.parent_path() && !error;
		}
		auto initialize_control(const fs::path& aInstalled) -> bool
		{
			if (!valid_installed(aInstalled)) {
				return false;
			}
			const auto control = control_for(aInstalled);
			if (mkdir(control.c_str(), 0700) != 0 && errno != EEXIST) {
				return false;
			}
			return private_directory(control);
		}
		auto admission_lock(const fs::path& aControl) -> fd_owner
		{
			auto lock = open_private(aControl / "admission.lock", true);
			if (lock.Value < 0 || flock(lock.Value, LOCK_EX | LOCK_NB) != 0) {
				return fd_owner{ };
			}
			return lock;
		}
		auto number(const QJsonObject& aRecord, const char* aKey) -> std::optional<std::uint64_t>
		{
			const auto    value  = aRecord.value(aKey).toString().toStdString();
			std::uint64_t result = 0;
			const auto    parsed = std::from_chars(value.data(), value.data() + value.size(), result);
			return !value.empty() && parsed.ec == std::errc{ } && parsed.ptr == value.data() + value.size() ? std::optional{ result } : std::nullopt;
		}
		auto candidate_record(const core::release_update_candidate& aCandidate) -> QJsonObject
		{
			QJsonObject result{ { "release", text(std::to_string(aCandidate.Release.value())) },
								{ "asset", text(std::to_string(aCandidate.Asset.value())) },
								{ "tag", text(aCandidate.Tag) },
								{ "name", text(aCandidate.AssetName) },
								{ "size", text(std::to_string(aCandidate.Size)) } };
			if (aCandidate.Sha256) {
				result["digest"] = QStringLiteral("sha256:") + QByteArray{ reinterpret_cast<const char*>(aCandidate.Sha256->data()), 32 }.toHex();
			}
			return result;
		}
		auto read_candidate(const QJsonObject& aRecord) -> std::optional<core::release_update_candidate>
		{
			const auto release = number(aRecord, "release"), asset = number(aRecord, "asset"), size = number(aRecord, "size");
			const auto tag     = aRecord.value("tag").toString().toStdString();
			const auto version = core::parse_release_update_version(tag);
			if (!release || !asset || !size || !version || *version == core::release_update_version{ }) {
				return { };
			}
			const auto selected = core::select_release_update({ }, { core::release_update_source::better_pull_review_github_com, core::release_update_release_id{ *release }, tag, false, false, { { core::release_update_asset_id{ *asset }, aRecord.value("name").toString().toStdString(), *size, aRecord.contains("digest") ? std::optional{ aRecord.value("digest").toString().toStdString() } : std::nullopt, true } } });
			return selected.Candidate;
		}
		auto file_digest(const fs::path& aPath) -> std::string
		{
			fd_owner    file{ open(aPath.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC) };
			struct stat info{ };
			if (file.Value < 0 || fstat(file.Value, &info) != 0 || !S_ISREG(info.st_mode) || info.st_nlink != 1
				|| info.st_size <= 0 || info.st_size > static_cast<off_t>(core::release_update_maximum_archive_bytes)) {
				return { };
			}
			QCryptographicHash      hash{ QCryptographicHash::Sha256 };
			std::array<char, 65536> bytes{ };
			off_t                   offset = 0;
			while (offset < info.st_size) {
				const auto count = pread(file.Value, bytes.data(), bytes.size(), offset);
				if (count <= 0) {
					return { };
				}
				offset += count;
				hash.addData(QByteArrayView{ bytes.data(), count });
			}
			struct stat after{ };
			if (fstat(file.Value, &after) != 0 || after.st_size != info.st_size || after.st_mtimespec.tv_sec != info.st_mtimespec.tv_sec
				|| after.st_mtimespec.tv_nsec != info.st_mtimespec.tv_nsec) {
				return { };
			}
			return hash.result().toHex().toStdString();
		}
		auto sync_rename(const fs::path& aFrom, const fs::path& aTo) -> bool
		{
			struct stat occupied{ };
			if (lstat(aTo.c_str(), &occupied) == 0 || errno != ENOENT) {
				return false;
			}
			fd_owner source{ open(aFrom.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
			fd_owner target{ open(aTo.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
			return source.Value >= 0 && target.Value >= 0
				&& renameatx_np(source.Value, aFrom.filename().c_str(), target.Value, aTo.filename().c_str(), RENAME_EXCL) == 0
				&& fsync(source.Value) == 0 && fsync(target.Value) == 0;
		}
		auto sync_tree(const fs::path& aRoot) -> bool
		{
			std::error_code       error;
			std::vector<fs::path> directories{ aRoot };
			std::size_t           count = 0;
			for (fs::recursive_directory_iterator item{ aRoot, error }; !error && item != fs::recursive_directory_iterator{ }; item.increment(error)) {
				if (++count > application::release_update_maximum_entries) {
					return false;
				}
				const auto status = item->symlink_status(error);
				if (error) {
					return false;
				}
				if (fs::is_symlink(status)) {
					continue;
				}
				if (fs::is_directory(status)) {
					directories.push_back(item->path());
					continue;
				}
				fd_owner file{ open(item->path().c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC) };
				if (!fs::is_regular_file(status) || file.Value < 0 || fsync(file.Value) != 0) {
					return false;
				}
			}
			if (error) {
				return false;
			}
			for (auto item = directories.rbegin(); item != directories.rend(); ++item) {
				fd_owner directory{ open(item->c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
				if (directory.Value < 0 || fsync(directory.Value) != 0) {
					return false;
				}
			}
			return true;
		}
		auto copy_candidate(const fs::path& aSource, const fs::path& aTarget, const std::function<bool()>& aAuthorized) -> bool
		{
			struct copy_state
			{
				const std::function<bool()>&          Authorized;
				std::chrono::steady_clock::time_point Deadline;
			} progress{ aAuthorized, std::chrono::steady_clock::now() + std::chrono::minutes{ 5 } };
			const auto state = copyfile_state_alloc();
			if (!state) {
				return false;
			}
			const auto callback = +[](int, int, copyfile_state_t, const char*, const char*, void* aContext) -> int {
				const auto& pending = *static_cast<copy_state*>(aContext);
				try {
					return pending.Authorized() && std::chrono::steady_clock::now() < pending.Deadline ? COPYFILE_CONTINUE : COPYFILE_QUIT;
				} catch (...) {
					return COPYFILE_QUIT;
				}
			};
			const bool configured = copyfile_state_set(state, COPYFILE_STATE_STATUS_CB, reinterpret_cast<const void*>(callback)) == 0
				&& copyfile_state_set(state, COPYFILE_STATE_STATUS_CTX, &progress) == 0;
			const bool copied = configured && copyfile(aSource.c_str(), aTarget.c_str(), state, COPYFILE_ALL | COPYFILE_RECURSIVE | COPYFILE_NOFOLLOW) == 0;
			static_cast<void>(copyfile_state_free(state));
			return copied;
		}
		auto live_participants(const fs::path& aControl) -> std::optional<QJsonArray>
		{
			QJsonArray      result;
			std::error_code error;
			std::size_t     inspected = 0;
			for (fs::directory_iterator item{ aControl, error }; !error && item != fs::directory_iterator{ }; item.increment(error)) {
				if (++inspected > 4096U) {
					return { };
				}
				const auto name = item->path().filename().string();
				if (!name.starts_with("process-")) {
					continue;
				}
				if (!valid_id(name.substr(8)) || result.size() >= 64) {
					return { };
				}
				auto file = open_private(item->path());
				if (file.Value < 0) {
					return { };
				}
				if (flock(file.Value, LOCK_EX | LOCK_NB) == 0) {
					continue;
				}
				if (errno != EWOULDBLOCK) {
					return { };
				}
				result.append(QJsonObject{ { "name", text(name) }, { "inode", text(inode(item->path())) } });
			}
			return error ? std::nullopt : std::optional{ result };
		}
		auto all_acknowledged(const fs::path& aControl, const QJsonObject& aJournal) -> bool
		{
			const auto participants = aJournal.value("participants").toArray();
			for (const auto& item : participants) {
				const auto owner = item.toObject();
				const auto name  = owner.value("name").toString().toStdString();
				if (!name.starts_with("process-") || !valid_id(name.substr(8))) {
					return false;
				}
				const auto path   = aControl / name;
				const auto record = read_record(path);
				if (inode(path) != owner.value("inode").toString().toStdString() || !record
					|| record->value("transaction") != aJournal.value("transaction") || !record->value("durable").toBool()) {
					return false;
				}
			}
			return true;
		}
		auto cleanup_identity(const struct stat& aInfo) -> std::string
		{
			const auto kind = S_ISDIR(aInfo.st_mode) ? 'd' : S_ISREG(aInfo.st_mode) && aInfo.st_nlink == 1 ? 'f'
				: S_ISLNK(aInfo.st_mode)                                                                   ? 'l'
																										   : '?';
			return kind == '?' ? std::string{ } : std::string{ kind } + ":" + std::to_string(aInfo.st_dev) + ":" + std::to_string(aInfo.st_ino);
		}
		auto cleanup_entries(const fs::path& aRoot) -> std::optional<QJsonObject>
		{
			QJsonObject     entries;
			std::error_code error;
			std::size_t     bytes = 0;
			for (fs::recursive_directory_iterator item{ aRoot, error }; !error && item != fs::recursive_directory_iterator{ }; item.increment(error)) {
				const auto  path = item->path().lexically_relative(aRoot).string();
				struct stat info{ };
				if (entries.size() >= static_cast<qsizetype>(application::release_update_maximum_entries) || path.size() > 4096U
					|| lstat(item->path().c_str(), &info) != 0) {
					return { };
				}
				const auto identity = cleanup_identity(info);
				bytes += path.size() + identity.size() + 16U;
				if (identity.empty() || bytes > maximum_cleanup) {
					return { };
				}
				entries[text(path)] = text(identity);
			}
			return error ? std::nullopt : std::optional{ entries };
		}
	}

	struct detail::install_transaction::implementation
	{
		fs::path                                      Installed, Control, Directory;
		install_effects&                              Effects;
		boundary_observer                             Observer;
		QJsonObject                                   Journal;
		std::optional<core::release_update_candidate> Candidate;
		fd_owner                                      Owner;
		std::string                                   ControlInode;
		std::string                                   UnpublishedDirectoryInode;

		implementation(fs::path aInstalled, install_effects& aEffects, boundary_observer aObserver)
			: Installed{ std::move(aInstalled) }
			, Control{ control_for(Installed) }
			, Effects{ aEffects }
			, Observer{ std::move(aObserver) }
		{ }
		~implementation()
		{
			if (Journal.isEmpty() && !UnpublishedDirectoryInode.empty() && private_directory(Control)
				&& inode(Control) == ControlInode && private_directory(Directory) && inode(Directory) == UnpublishedDirectoryInode) {
				std::error_code error;
				fs::remove_all(Directory, error);
			}
		}
		auto safe() const -> bool
		{
			return private_directory(Control) && inode(Control) == ControlInode && private_directory(Directory)
				&& inode(Directory) == Journal.value("directory_inode").toString().toStdString()
				&& inode(Installed.parent_path()) == Journal.value("parent_inode").toString().toStdString();
		}
		auto own() -> bool
		{
			if (!initialize_control(Installed)) {
				return false;
			}
			ControlInode = inode(Control);
			Owner        = open_private(Control / "transaction.lock", true);
			return Owner.Value >= 0 && flock(Owner.Value, LOCK_EX | LOCK_NB) == 0;
		}
		auto load() -> bool
		{
			if (!own()) {
				return false;
			}
			const auto record = read_record(Control / "active.json");
			if (!record || record->value("schema") != "bpr.update.transaction.v1"
				|| record->value("installed").toString().toStdString() != Installed.string()) {
				return false;
			}
			const auto id = record->value("transaction").toString().toStdString();
			if (!valid_id(id)) {
				return false;
			}
			Directory = Control / ("transaction-" + id);
			Journal   = *record;
			Candidate = read_candidate(Journal.value("candidate").toObject());
			return Candidate && safe();
		}
		auto boundary(install_boundary aPhase) -> bool
		{
			Journal["phase"] = static_cast<int>(aPhase);
			if (!safe() || !atomic_record(Control, "active.json", Journal)) {
				return false;
			}
			return !Observer || Observer(aPhase);
		}
		auto matches(const fs::path& aPath, const char* aInode, const char* aTree) const -> bool
		{
			const auto tree = install_tree_identity(aPath);
			return safe() && inode(aPath) == Journal.value(aInode).toString().toStdString()
				&& tree && *tree == Journal.value(aTree).toString().toStdString();
		}
		auto exclusive_running() -> fd_owner
		{
			auto lock = open_private(Control / "running.lock", true);
			if (lock.Value < 0 || flock(lock.Value, LOCK_EX | LOCK_NB) != 0) {
				return fd_owner{ };
			}
			return lock;
		}
		auto has_receipt() const -> bool
		{
			const auto receipt = read_record(Directory / "receipt.json");
			if (!receipt) {
				return false;
			}
			const auto process = receipt->value("process").toString().toStdString();
			if (!process.starts_with("process-") || !valid_id(process.substr(8))) {
				return false;
			}
			const auto owner = read_record(Control / process);
			return receipt && receipt->value("schema") == "bpr.update.receipt.v1"
				&& receipt->value("transaction") == Journal.value("transaction")
				&& receipt->value("executable") == Journal.value("executable")
				&& receipt->value("version") == Journal.value("candidate").toObject().value("tag")
				&& owner && owner->value("startup").toBool()
				&& owner->value("transaction") == Journal.value("transaction")
				&& owner->value("executable") == Journal.value("executable")
				&& receipt->value("process_inode").toString().toStdString() == inode(Control / process)
				&& file_digest(Installed / executable_name) == Journal.value("executable").toString().toStdString();
		}
		auto cleanup_backup() -> bool
		{
			const auto  backup = Directory / "backup.app";
			struct stat backupInfo{ };
			if (lstat(backup.c_str(), &backupInfo) != 0) {
				return errno == ENOENT;
			}
			const auto  planPath = Directory / "cleanup.json";
			struct stat planInfo{ };
			if (lstat(planPath.c_str(), &planInfo) != 0) {
				if (errno != ENOENT || !matches(backup, "old_inode", "old_tree")) {
					return false;
				}
				const auto entries = cleanup_entries(backup);
				if (!entries || !matches(backup, "old_inode", "old_tree")
					|| !atomic_record(Directory, "cleanup.json", { { "schema", "bpr.update.cleanup.v1" }, { "transaction", Journal.value("transaction") }, { "root_inode", Journal.value("old_inode") }, { "entries", *entries } }, maximum_cleanup)) {
					return false;
				}
			}
			const auto plan = read_record(planPath, maximum_cleanup);
			if (!plan || plan->value("schema") != "bpr.update.cleanup.v1" || plan->value("transaction") != Journal.value("transaction")
				|| plan->value("root_inode") != Journal.value("old_inode") || !plan->value("entries").isObject()
				|| inode(backup) != Journal.value("old_inode").toString().toStdString()) {
				return false;
			}
			const auto recorded = plan->value("entries").toObject();
			if (recorded.size() > static_cast<qsizetype>(application::release_update_maximum_entries)) {
				return false;
			}
			std::map<std::string, std::string> entries;
			for (auto item = recorded.begin(); item != recorded.end(); ++item) {
				const auto path = fs::path{ item.key().toStdString() };
				if (path.empty() || path.is_absolute() || path.lexically_normal() != path || path.string().size() > 4096U
					|| std::ranges::any_of(path, [](const auto& aPart) { return aPart == "." || aPart == ".."; }) || !item.value().isString()) {
					return false;
				}
				entries.emplace(path.string(), item.value().toString().toStdString());
			}
			fd_owner    root{ open(backup.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
			struct stat rootInfo{ };
			if (root.Value < 0 || fstat(root.Value, &rootInfo) != 0 || cleanup_identity(rootInfo) != "d:" + Journal.value("old_inode").toString().toStdString()) {
				return false;
			}
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes{ 5 };
			// A durable inode inventory, not a later directory name or recursive sweep,
			// authorizes each unlink. Missing entries are already-cleaned successes;
			// replacements and newly added files remain untouched on every retry.
			for (auto item = entries.rbegin(); item != entries.rend(); ++item) {
				if (std::chrono::steady_clock::now() >= deadline || !safe()) {
					return false;
				}
				const fs::path path{ item->first };
				fd_owner       parent{ fcntl(root.Value, F_DUPFD_CLOEXEC, 0) };
				fs::path       traversed;
				bool           missing = false;
				for (const auto& part : path.parent_path()) {
					traversed /= part;
					const auto expected = entries.find(traversed.string());
					if (parent.Value < 0 || expected == entries.end() || !expected->second.starts_with("d:")) {
						return false;
					}
					fd_owner next{ openat(parent.Value, part.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
					if (next.Value < 0 && errno == ENOENT) {
						missing = true;
						break;
					}
					struct stat info{ };
					if (next.Value < 0 || fstat(next.Value, &info) != 0 || cleanup_identity(info) != expected->second) {
						return false;
					}
					parent = std::move(next);
				}
				if (missing) {
					continue;
				}
				struct stat info{ };
				if (parent.Value < 0 || fstatat(parent.Value, path.filename().c_str(), &info, AT_SYMLINK_NOFOLLOW) != 0) {
					if (parent.Value >= 0 && errno == ENOENT) {
						continue;
					}
					return false;
				}
				if (cleanup_identity(info) != item->second || unlinkat(parent.Value, path.filename().c_str(), S_ISDIR(info.st_mode) ? AT_REMOVEDIR : 0) != 0
					|| fsync(parent.Value) != 0 || (Observer && !Observer(install_boundary::cleanup_entry))) {
					return false;
				}
			}
			fd_owner directory{ open(Directory.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
			return directory.Value >= 0 && safe() && inode(backup) == Journal.value("old_inode").toString().toStdString()
				&& unlinkat(directory.Value, "backup.app", AT_REMOVEDIR) == 0 && fsync(directory.Value) == 0;
		}
		auto release_active() -> bool
		{
			// Keep the completed journal as the sole cleanup authority. No age/name sweep.
			fd_owner directory{ open(Control.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC) };
			if (directory.Value < 0 || !safe() || !atomic_record(Directory, "result.json", Journal) || !atomic_record(Control, "last-result.json", Journal)
				|| unlinkat(directory.Value, "active.json", 0) != 0 || fsync(directory.Value) != 0) {
				return false;
			}
			for (const auto& participant : Journal.value("participants").toArray()) {
				const auto record = participant.toObject();
				const auto name   = record.value("name").toString().toStdString();
				if (!name.starts_with("process-") || !valid_id(name.substr(8))) {
					continue;
				}
				auto file = open_private(Control / name);
				if (file.Value >= 0 && inode(Control / name) == record.value("inode").toString().toStdString()
					&& flock(file.Value, LOCK_EX | LOCK_NB) == 0) {
					static_cast<void>(unlinkat(directory.Value, name.c_str(), 0));
				}
			}
			return fsync(directory.Value) == 0;
		}
	};
	detail::install_transaction::install_transaction(fs::path aInstalled, install_effects& aEffects, boundary_observer aObserver)
		: Impl{ std::make_unique<implementation>(std::move(aInstalled), aEffects, std::move(aObserver)) }
	{ }
	detail::install_transaction::~install_transaction() = default;
	auto detail::install_transaction::identity() const -> std::string { return Impl->Journal.value("transaction").toString().toStdString(); }
	auto detail::install_transaction::control_path() const -> fs::path { return Impl->Control; }
	auto detail::install_transaction::transaction_path() const -> fs::path { return Impl->Directory; }
	auto detail::install_transaction::resume(std::string_view aTransaction) -> bool
	{
		return valid_id(aTransaction) && Impl->load() && identity() == aTransaction;
	}
	auto detail::install_transaction::abort_prepared() -> reason
	{
		auto& state = *Impl;
		if (state.Journal.isEmpty() && !state.load()) {
			return reason::recovery_required;
		}
		auto gate = admission_lock(state.Control);
		if (gate.Value < 0 || state.Journal.value("phase").toInt(-1) != static_cast<int>(install_boundary::prepared)
			|| !state.matches(state.Installed, "old_inode", "old_tree")) {
			return reason::recovery_required;
		}
		if (!state.boundary(install_boundary::recovered)) {
			return reason::recovery_required;
		}
		return state.release_active() ? reason::none : reason::recovery_required;
	}
	auto detail::install_transaction::prepare(const fs::path& aVerified, const core::release_update_candidate& aCandidate, const std::function<bool()>& aAuthorized, const fs::path& aApplicationRoot) -> reason
	{
		if (!aApplicationRoot.empty() && (aApplicationRoot.string().size() > 4096U || !private_directory(aApplicationRoot))) {
			return reason::flush_failed;
		}
		auto& state = *Impl;
		if (!state.own()) {
			return reason::busy;
		}
		auto gate = admission_lock(state.Control);
		if (gate.Value < 0) {
			return reason::busy;
		}
		struct stat active{ };
		if (lstat((state.Control / "active.json").c_str(), &active) == 0 || errno != ENOENT) {
			return reason::recovery_required;
		}
		const auto participants = live_participants(state.Control);
		const auto original = install_tree_identity(aVerified), old = install_tree_identity(state.Installed);
		if (!participants || !original || !old || !state.Effects.validate(aVerified, state.Installed, aCandidate)) {
			return reason::invalid_bundle;
		}
		const auto id = unique_name();
		if (!aAuthorized || !aAuthorized()) {
			return reason::access_required;
		}
		state.Directory = state.Control / ("transaction-" + id);
		if (mkdir(state.Directory.c_str(), 0700) != 0) {
			return reason::unavailable;
		}
		state.UnpublishedDirectoryInode = inode(state.Directory);
		const auto      staged          = state.Directory / "candidate.app";
		std::error_code error;
		const auto      space = fs::space(state.Control, error);
		if (error || space.available < application::release_update_maximum_expanded_bytes * 2U
			|| !copy_candidate(aVerified, staged, aAuthorized)) {
			return reason::unavailable;
		}
		const auto copied     = install_tree_identity(staged);
		const auto executable = file_digest(staged / executable_name);
		if (!copied || executable.empty() || install_tree_identity(aVerified) != original
			|| !state.Effects.validate(staged, state.Installed, aCandidate)) {
			return reason::changed_candidate;
		}
		if (!sync_tree(staged)) {
			return reason::unavailable;
		}
		if (!aAuthorized()) {
			return reason::access_required;
		}
		state.Candidate = aCandidate;
		state.Journal   = { { "schema", "bpr.update.transaction.v1" }, { "transaction", text(id) }, { "installed", text(state.Installed.string()) }, { "parent_inode", text(inode(state.Installed.parent_path())) }, { "directory_inode", text(inode(state.Directory)) }, { "old_inode", text(inode(state.Installed)) }, { "new_inode", text(inode(staged)) }, { "old_tree", text(*old) }, { "new_tree", text(*copied) }, { "candidate", candidate_record(aCandidate) }, { "executable", text(executable) }, { "participants", *participants } };
		if (!aApplicationRoot.empty()) {
			state.Journal["state_root"]       = text(aApplicationRoot.string());
			state.Journal["state_root_inode"] = text(inode(aApplicationRoot));
		}
		return state.boundary(install_boundary::prepared) ? reason::none : reason::unavailable;
	}
	auto detail::install_transaction::replace() -> reason
	{
		auto& state = *Impl;
		if (state.Journal.isEmpty() && !state.load()) {
			return reason::recovery_required;
		}
		auto gate    = admission_lock(state.Control);
		auto running = state.exclusive_running();
		if (gate.Value < 0 || running.Value < 0 || !all_acknowledged(state.Control, state.Journal)) {
			return reason::busy;
		}
		if (state.Journal.value("phase").toInt(-1) != static_cast<int>(install_boundary::prepared)) {
			return reason::recovery_required;
		}
		const auto staged = state.Directory / "candidate.app", backup = state.Directory / "backup.app";
		if (!state.matches(state.Installed, "old_inode", "old_tree") || !state.matches(staged, "new_inode", "new_tree")
			|| !state.Effects.validate(staged, state.Installed, *state.Candidate)
			|| !state.matches(staged, "new_inode", "new_tree")) {
			return reason::changed_candidate;
		}
		if (!state.boundary(install_boundary::before_old_rename) || !sync_rename(state.Installed, backup)
			|| !state.boundary(install_boundary::after_old_rename) || !state.boundary(install_boundary::before_new_rename)
			|| !sync_rename(staged, state.Installed) || !state.boundary(install_boundary::after_new_rename)
			|| !state.boundary(install_boundary::before_launch)) {
			return reason::recovery_required;
		}
		// The active journal admits only the exact new executable and transaction.
		// Release shared-process exclusion for that startup; admission remains locked
		// until launch returns, so unrelated starts cannot enter the gap.
		running = fd_owner{ };
		if (!state.Effects.launch(state.Installed / executable_name, identity())) {
			return reason::recovery_required;
		}
		return state.boundary(install_boundary::after_launch) ? reason::none : reason::recovery_required;
	}
	auto detail::install_transaction::finish() -> reason
	{
		auto& state = *Impl;
		if (state.Journal.isEmpty() && !state.load()) {
			return reason::recovery_required;
		}
		auto gate = admission_lock(state.Control);
		if (gate.Value < 0 || !state.safe() || !state.has_receipt()
			|| !state.matches(state.Installed, "new_inode", "new_tree")) {
			return reason::recovery_required;
		}
		if (!state.boundary(install_boundary::healthy) || !state.boundary(install_boundary::before_cleanup)) {
			return reason::recovery_required;
		}
		if (!state.cleanup_backup()) {
			return reason::recovery_required;
		}
		if (!state.boundary(install_boundary::complete)) {
			return reason::recovery_required;
		}
		return state.release_active() ? reason::none : reason::recovery_required;
	}
	auto detail::install_transaction::recover() -> reason
	{
		auto& state = *Impl;
		if (state.Journal.isEmpty() && !state.load()) {
			return reason::recovery_required;
		}
		if (state.has_receipt()) {
			return finish();
		}
		auto gate    = admission_lock(state.Control);
		auto running = state.exclusive_running();
		if (gate.Value < 0 || running.Value < 0 || !state.safe()) {
			return reason::busy;
		}
		const auto backup = state.Directory / "backup.app", staged = state.Directory / "candidate.app";
		const auto installedIdentity = inode(state.Installed);
		if (!state.boundary(install_boundary::before_rollback)) {
			return reason::recovery_required;
		}
		if (installedIdentity != state.Journal.value("old_inode").toString().toStdString()) {
			if (!state.matches(backup, "old_inode", "old_tree")) {
				return reason::changed_candidate;
			}
			if (!installedIdentity.empty()) {
				if (!state.matches(state.Installed, "new_inode", "new_tree") || !sync_rename(state.Installed, staged)) {
					return reason::changed_candidate;
				}
			}
			if (!sync_rename(backup, state.Installed)) {
				return reason::recovery_required;
			}
		}
		if (!state.matches(state.Installed, "old_inode", "old_tree") || !state.boundary(install_boundary::recovered)) {
			return reason::recovery_required;
		}
		return state.release_active() ? reason::none : reason::recovery_required;
	}

	struct release_update_installation::implementation
	{
		fs::path                Installed, Control, Participant;
		std::optional<fs::path> StartupRoot;
		std::string             StartupTransaction, Identity, ParticipantInode;
		fd_owner                Running, Owner;
		bool                    ProcessLifetime = false;
	};
	release_update_installation::release_update_installation(std::unique_ptr<implementation> aImpl)
		: Impl{ std::move(aImpl) }
	{ }
	release_update_installation::~release_update_installation()
	{
		auto        gate = admission_lock(Impl->Control);
		struct stat active{ };
		if (gate.Value >= 0 && lstat((Impl->Control / "active.json").c_str(), &active) != 0 && errno == ENOENT
			&& inode(Impl->Participant) == Impl->ParticipantInode) {
			static_cast<void>(unlink(Impl->Participant.c_str()));
		}
		if (Impl->ProcessLifetime) {
			// These CLOEXEC descriptors now belong to the process, not the desktop.
			// The kernel closes them on exit; there is no reusable PID authority.
			Impl->Running.Value = -1;
			Impl->Owner.Value   = -1;
		}
	}
	void release_update_installation::keep_until_process_exit() noexcept { Impl->ProcessLifetime = true; }
	auto release_update_installation::startup_root() const -> std::optional<fs::path> { return Impl->StartupRoot; }
	auto release_update_installation::join(fs::path aInstalled, std::string aStartupTransaction, std::string_view aRunningVersion)
		-> std::expected<std::unique_ptr<release_update_installation>, reason>
	{
		if (!initialize_control(aInstalled)) {
			return std::unexpected(reason::read_only);
		}
		auto state       = std::make_unique<implementation>();
		state->Installed = std::move(aInstalled);
		state->Control   = control_for(state->Installed);
		auto gate        = admission_lock(state->Control);
		if (gate.Value < 0) {
			return std::unexpected(reason::busy);
		}
		struct stat activeInfo{ };
		if (lstat((state->Control / "active.json").c_str(), &activeInfo) == 0) {
			const auto active  = read_record(state->Control / "active.json");
			const auto tag     = active ? active->value("candidate").toObject().value("tag").toString().toStdString() : std::string{ };
			const auto version = std::string_view{ tag }.starts_with('v') ? std::string_view{ tag }.substr(1) : std::string_view{ tag };
			if (!active || !valid_id(aStartupTransaction) || active->value("transaction").toString().toStdString() != aStartupTransaction
				|| aRunningVersion != version || version.empty()
				|| (active->value("phase").toInt(-1) != static_cast<int>(detail::install_boundary::before_launch)
					&& active->value("phase").toInt(-1) != static_cast<int>(detail::install_boundary::after_launch))
				|| active->value("executable").toString().toStdString() != file_digest(state->Installed / executable_name)) {
				return std::unexpected(reason::recovery_required);
			}
			if (active->contains("state_root")) {
				const fs::path root{ active->value("state_root").toString().toStdString() };
				if (root.string().size() > 4096U || !private_directory(root)
					|| inode(root) != active->value("state_root_inode").toString().toStdString()) {
					return std::unexpected(reason::recovery_required);
				}
				state->StartupRoot = root;
			}
		} else if (errno != ENOENT || !aStartupTransaction.empty()) {
			return std::unexpected(reason::recovery_required);
		}
		state->Running = open_private(state->Control / "running.lock", true);
		if (state->Running.Value < 0 || flock(state->Running.Value, LOCK_SH | LOCK_NB) != 0) {
			return std::unexpected(reason::busy);
		}
		state->Identity    = "process-" + unique_name();
		state->Participant = state->Control / state->Identity;
		state->Owner       = fd_owner{ open(state->Participant.c_str(), O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600) };
		if (state->Owner.Value < 0 || flock(state->Owner.Value, LOCK_EX | LOCK_NB) != 0
			|| !write_bytes(state->Owner.Value, QJsonDocument{ QJsonObject{ { "schema", "bpr.update.process.v1" }, { "startup", !aStartupTransaction.empty() }, { "transaction", text(aStartupTransaction) }, { "executable", text(file_digest(state->Installed / executable_name)) } } }.toJson(QJsonDocument::Compact))) {
			return std::unexpected(reason::unavailable);
		}
		state->StartupTransaction = std::move(aStartupTransaction);
		state->ParticipantInode   = inode(state->Participant);
		return std::unique_ptr<release_update_installation>{ new release_update_installation{ std::move(state) } };
	}
	auto release_update_installation::shutdown_requested() -> std::optional<std::string>
	{
		auto gate = admission_lock(Impl->Control);
		if (gate.Value < 0) {
			return { };
		}
		const auto active = read_record(Impl->Control / "active.json");
		if (!active || active->value("phase").toInt(-1) != static_cast<int>(detail::install_boundary::prepared)) {
			return { };
		}
		for (const auto& item : active->value("participants").toArray()) {
			if (item.toObject().value("name").toString().toStdString() == Impl->Identity
				&& item.toObject().value("inode").toString().toStdString() == inode(Impl->Participant)) {
				return active->value("transaction").toString().toStdString();
			}
		}
		return { };
	}
	auto release_update_installation::last_result() -> std::optional<release_update_installation_result>
	{
		const auto record = read_record(Impl->Control / "last-result.json");
		if (!record || record->value("schema") != "bpr.update.transaction.v1"
			|| record->value("installed").toString().toStdString() != Impl->Installed.string()) {
			return { };
		}
		const auto id    = record->value("transaction").toString().toStdString();
		const auto phase = record->value("phase").toInt(-1);
		if (!valid_id(id) || (phase != static_cast<int>(detail::install_boundary::complete) && phase != static_cast<int>(detail::install_boundary::recovered))) {
			return { };
		}
		return release_update_installation_result{ id, phase == static_cast<int>(detail::install_boundary::complete) };
	}
	auto release_update_installation::acknowledge_shutdown(std::string_view aTransaction, bool aDurable) -> bool
	{
		if (shutdown_requested() != std::optional{ std::string{ aTransaction } }) {
			return false;
		}
		// The locked inode is never replaced; the helper reads it only after the
		// process releases its shared lifetime lease.
		const auto bytes = QJsonDocument{ QJsonObject{ { "schema", "bpr.update.process.v1" },
													   { "transaction", text(aTransaction) },
													   { "durable", aDurable } } }
							   .toJson(QJsonDocument::Compact);
		return ftruncate(Impl->Owner.Value, 0) == 0 && lseek(Impl->Owner.Value, 0, SEEK_SET) == 0 && write_bytes(Impl->Owner.Value, bytes);
	}
	auto release_update_installation::healthy_start(std::string_view aVersion) -> bool
	{
		if (Impl->StartupTransaction.empty()) {
			return false;
		}
		auto gate = admission_lock(Impl->Control);
		if (gate.Value < 0) {
			return false;
		}
		const auto active = read_record(Impl->Control / "active.json");
		if (!active || active->value("transaction").toString().toStdString() != Impl->StartupTransaction) {
			return false;
		}
		const auto tag          = active->value("candidate").toObject().value("tag").toString().toStdString();
		const auto digest       = file_digest(Impl->Installed / executable_name);
		const auto exactVersion = std::string_view{ tag }.starts_with('v') ? std::string_view{ tag }.substr(1) : std::string_view{ tag };
		if (aVersion != exactVersion
			|| digest.empty() || digest != active->value("executable").toString().toStdString()
			|| inode(Impl->Installed) != active->value("new_inode").toString().toStdString()
			|| (Impl->StartupRoot && (!private_directory(*Impl->StartupRoot) || inode(*Impl->StartupRoot) != active->value("state_root_inode").toString().toStdString()))) {
			return false;
		}
		const auto directory = Impl->Control / ("transaction-" + Impl->StartupTransaction);
		if (inode(directory) != active->value("directory_inode").toString().toStdString()) {
			return false;
		}
		return atomic_record(directory, "receipt.json", { { "schema", "bpr.update.receipt.v1" }, { "transaction", text(Impl->StartupTransaction) }, { "executable", text(digest) }, { "version", text(tag) }, { "process", text(Impl->Identity) }, { "process_inode", text(inode(Impl->Participant)) } });
	}
}
