#include "designated_live_public_observer.hpp"

#include "bpr/core/epoch_model.hpp"
#include "bpr/core/utf8.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <map>
#include <optional>
#include <signal.h>
#include <span>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::adapters::github::detail {
	namespace {
		constexpr auto response_header_limit = std::size_t{ 64U * 1024U };
		constexpr auto response_body_limit   = std::size_t{ 256U * 1024U };
		constexpr auto curl_path             = std::string_view{ "/usr/bin/curl" };
		constexpr auto designated_base_oid
			= std::string_view{ "a939a85a5525994fe9e9cb4c973ad9362ed427c7" };
		constexpr auto designated_head_oid
			= std::string_view{ "61e8dbcd1c3940f6df0fa720bec5485dc25d4a3a" };
		constexpr auto designated_path
			= std::string_view{ "/repos/MikeMarcin/BPR-Test/pulls/2" };
		constexpr auto designated_origin
			= std::string_view{ "https://api.github.com" };

		class file_descriptor final
		{
		public:
			explicit file_descriptor(const int aValue = -1) noexcept
				: Value{ aValue }
			{
			}
			~file_descriptor()
			{
				if (Value >= 0) {
					static_cast<void>(::close(Value));
				}
			}
			file_descriptor(const file_descriptor&)                    = delete;
			auto operator=(const file_descriptor&) -> file_descriptor& = delete;
			file_descriptor(file_descriptor&& aOther) noexcept
				: Value{ std::exchange(aOther.Value, -1) }
			{
			}
			auto operator=(file_descriptor&& aOther) noexcept -> file_descriptor&
			{
				if (this != &aOther) {
					if (Value >= 0) {
						static_cast<void>(::close(Value));
					}
					Value = std::exchange(aOther.Value, -1);
				}
				return *this;
			}
			[[nodiscard]] auto get() const noexcept -> int { return Value; }
			[[nodiscard]] auto release() noexcept -> int
			{
				return std::exchange(Value, -1);
			}
			[[nodiscard]] explicit operator bool() const noexcept
			{
				return Value >= 0;
			}

		private:
			int Value;
		};

		template <typename Type>
		class cf_reference final
		{
		public:
			explicit cf_reference(Type aValue = nullptr) noexcept
				: Value{ aValue }
			{
			}
			~cf_reference()
			{
				if (Value != nullptr) {
					CFRelease(Value);
				}
			}
			cf_reference(const cf_reference&)                    = delete;
			auto operator=(const cf_reference&) -> cf_reference& = delete;
			cf_reference(cf_reference&& aOther) noexcept
				: Value{ std::exchange(aOther.Value, nullptr) }
			{
			}
			[[nodiscard]] auto     get() const noexcept -> Type { return Value; }
			[[nodiscard]] explicit operator bool() const noexcept
			{
				return Value != nullptr;
			}

		private:
			Type Value;
		};

		struct retained_curl
		{
			file_descriptor           Parent;
			file_descriptor           Executable;
			struct stat               Identity{ };
			std::vector<std::uint8_t> SigningDigest;
		};

		[[nodiscard]] auto signing_digest(SecStaticCodeRef aCode)
			-> std::optional<std::vector<std::uint8_t>>
		{
			CFDictionaryRef informationRaw = nullptr;
			if (SecCodeCopySigningInformation(
					aCode, kSecCSSigningInformation, &informationRaw)
				!= errSecSuccess) {
				return std::nullopt;
			}
			auto  information = cf_reference<CFDictionaryRef>{ informationRaw };
			auto* value       = CFDictionaryGetValue(information.get(), kSecCodeInfoUnique);
			if (value == nullptr || CFGetTypeID(value) != CFDataGetTypeID()) {
				return std::nullopt;
			}
			auto* data = static_cast<CFDataRef>(value);
			return std::vector<std::uint8_t>{
				CFDataGetBytePtr(data),
				CFDataGetBytePtr(data) + CFDataGetLength(data),
			};
		}

		[[nodiscard]] auto retain_trusted_curl() -> std::optional<retained_curl>
		{
			auto parent = file_descriptor{
				::open("/usr/bin", O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW)
			};
			auto executable = file_descriptor{
				::openat(parent.get(), "curl", O_RDONLY | O_CLOEXEC | O_NOFOLLOW)
			};
			struct stat parentStatus{ };
			struct stat identity{ };
			if (!parent || !executable || ::fstat(parent.get(), &parentStatus) != 0
				|| ::fstat(executable.get(), &identity) != 0
				|| !S_ISDIR(parentStatus.st_mode) || parentStatus.st_uid != 0U
				|| (parentStatus.st_mode & static_cast<mode_t>(0022)) != 0 || !S_ISREG(identity.st_mode)
				|| identity.st_uid != 0U || (identity.st_mode & static_cast<mode_t>(0022)) != 0
				|| identity.st_nlink != 1) {
				return std::nullopt;
			}
			auto url = cf_reference<CFURLRef>{
				CFURLCreateFromFileSystemRepresentation(
					kCFAllocatorDefault,
					reinterpret_cast<const UInt8*>(curl_path.data()),
					static_cast<CFIndex>(curl_path.size()),
					false)
			};
			if (!url) {
				return std::nullopt;
			}
			SecStaticCodeRef codeRaw = nullptr;
			if (SecStaticCodeCreateWithPath(url.get(), kSecCSDefaultFlags, &codeRaw)
					!= errSecSuccess
				|| codeRaw == nullptr) {
				return std::nullopt;
			}
			auto              code           = cf_reference<SecStaticCodeRef>{ codeRaw };
			SecRequirementRef requirementRaw = nullptr;
			if (SecRequirementCreateWithString(
					CFSTR("anchor apple and identifier \"com.apple.curl\""),
					kSecCSDefaultFlags,
					&requirementRaw)
					!= errSecSuccess
				|| requirementRaw == nullptr) {
				return std::nullopt;
			}
			auto       requirement = cf_reference<SecRequirementRef>{ requirementRaw };
			const auto flags       = static_cast<SecCSFlags>(
				kSecCSStrictValidate | kSecCSCheckAllArchitectures);
			if (SecStaticCodeCheckValidity(code.get(), flags, requirement.get())
				!= errSecSuccess) {
				return std::nullopt;
			}
			auto digest = signing_digest(code.get());
			if (!digest.has_value() || digest->empty()) {
				return std::nullopt;
			}
			return retained_curl{
				.Parent        = std::move(parent),
				.Executable    = std::move(executable),
				.Identity      = identity,
				.SigningDigest = std::move(*digest),
			};
		}

		[[nodiscard]] auto exact_retained_curl(const retained_curl& aCurl) noexcept
			-> bool
		{
			struct stat observed{ };
			return ::fstatat(
					   aCurl.Parent.get(), "curl", &observed, AT_SYMLINK_NOFOLLOW)
				== 0
				&& observed.st_dev == aCurl.Identity.st_dev
				&& observed.st_ino == aCurl.Identity.st_ino
				&& observed.st_size == aCurl.Identity.st_size
				&& observed.st_mtimespec.tv_sec == aCurl.Identity.st_mtimespec.tv_sec
				&& observed.st_mtimespec.tv_nsec == aCurl.Identity.st_mtimespec.tv_nsec;
		}

		[[nodiscard]] auto guest_signing_digest(const pid_t aProcess)
			-> std::optional<std::vector<std::uint8_t>>
		{
			auto processNumber = cf_reference<CFNumberRef>{ CFNumberCreate(
				kCFAllocatorDefault, kCFNumberIntType, &aProcess) };
			auto attributes    = cf_reference<CFMutableDictionaryRef>{
				CFDictionaryCreateMutable(
					kCFAllocatorDefault,
					1,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks)
			};
			if (!processNumber || !attributes) {
				return std::nullopt;
			}
			CFDictionarySetValue(
				attributes.get(), kSecGuestAttributePid, processNumber.get());
			SecCodeRef guestRaw = nullptr;
			if (SecCodeCopyGuestWithAttributes(
					nullptr, attributes.get(), kSecCSDefaultFlags, &guestRaw)
					!= errSecSuccess
				|| guestRaw == nullptr) {
				return std::nullopt;
			}
			auto guest = cf_reference<SecCodeRef>{ guestRaw };
			if (SecCodeCheckValidity(guest.get(), kSecCSStrictValidate, nullptr)
				!= errSecSuccess) {
				return std::nullopt;
			}
			return signing_digest(reinterpret_cast<SecStaticCodeRef>(guest.get()));
		}

		struct bounded_capture
		{
			std::vector<std::uint8_t> Bytes;
			bool                      Failed = false;
			bool                      Limit  = false;
		};

		[[nodiscard]] auto target() -> std::optional<application::review_mutation_target_binding>
		{
			const auto base = core::parse_git_commit_oid(designated_base_oid);
			const auto head = core::parse_git_commit_oid(designated_head_oid);
			if (!base.has_value() || !head.has_value()) {
				return std::nullopt;
			}
			return application::review_mutation_target_binding{
				.Semantic = {
					.Host         = core::git_host_id{ 1U },
					.Repository   = core::repository_id{ 1'328'277'631U },
					.PullRequest  = core::pull_request_id{ 4'650'736'621U },
					.ExpectedHead = *head,
				},
				.Hostname                 = application::github_canonical_hostname{ "github.com" },
				.ServerContract           = application::github_server_contract::github_com_2022_11_28,
				.Repository               = { .Owner = "MikeMarcin", .Name = "BPR-Test" },
				.Number                   = core::pull_request_number{ 2U },
				.RepositoryDatabaseId     = 1'328'277'631U,
				.RepositoryNodeId         = "R_kgDOTyvofw",
				.PullRequestDatabaseId    = 4'650'736'621U,
				.PullRequestNodeId        = "PR_kwDOTyvof88AAAABFTSb7Q",
				.BaseRepositoryDatabaseId = 1'328'277'631U,
				.BaseOid                  = *base,
				.BaseRef                  = "codex/rfc0022-fixture-base-20260926",
				.HeadRef                  = "codex/rfc0022-live-comments-20260926",
				.Open                     = true,
			};
		}

		struct json_number
		{
			std::string Value;
		};

		struct json_value
		{
			using object = std::map<std::string, json_value, std::less<>>;
			using array  = std::vector<json_value>;
			std::variant<std::monostate, bool, json_number, std::string, object, array>
				Value;
		};

		class json_parser final
		{
		public:
			explicit json_parser(const std::string_view aText) noexcept
				: Text{ aText }
			{
			}

			[[nodiscard]] auto parse() -> std::optional<json_value>
			{
				auto result = value(0U);
				space();
				return result.has_value() && Position == Text.size()
					? result
					: std::nullopt;
			}

		private:
			void space() noexcept
			{
				while (Position != Text.size()
					   && (Text[Position] == ' ' || Text[Position] == '\n'
						   || Text[Position] == '\r' || Text[Position] == '\t')) {
					++Position;
				}
			}

			[[nodiscard]] auto take(const char aCharacter) noexcept -> bool
			{
				space();
				if (Position == Text.size() || Text[Position] != aCharacter) {
					return false;
				}
				++Position;
				return true;
			}

			[[nodiscard]] static auto hex(const char aCharacter) noexcept
				-> std::optional<std::uint32_t>
			{
				if (aCharacter >= '0' && aCharacter <= '9') {
					return static_cast<std::uint32_t>(aCharacter - '0');
				}
				if (aCharacter >= 'a' && aCharacter <= 'f') {
					return static_cast<std::uint32_t>(aCharacter - 'a' + 10);
				}
				if (aCharacter >= 'A' && aCharacter <= 'F') {
					return static_cast<std::uint32_t>(aCharacter - 'A' + 10);
				}
				return std::nullopt;
			}

			[[nodiscard]] auto unicode_escape() -> std::optional<std::uint32_t>
			{
				if (Position + 4U > Text.size()) {
					return std::nullopt;
				}
				auto result = std::uint32_t{ 0U };
				for (auto count = 0U; count != 4U; ++count) {
					const auto digit = hex(Text[Position++]);
					if (!digit.has_value()) {
						return std::nullopt;
					}
					result = result * 16U + *digit;
				}
				return result;
			}

			/// \brief The shared core encoder; false for a lone surrogate or a
			/// value above U+10FFFF, which have no UTF-8 encoding ([[BPR-1434]]).
			[[nodiscard]] static auto append_utf8(
				std::string& aOutput, const std::uint32_t aCodePoint) -> bool
			{
				return core::utf8::append(aOutput, static_cast<char32_t>(aCodePoint));
			}

			[[nodiscard]] auto string() -> std::optional<std::string>
			{
				if (!take('"')) {
					return std::nullopt;
				}
				auto result = std::string{ };
				while (Position != Text.size()) {
					const auto character = static_cast<unsigned char>(Text[Position++]);
					if (character == '"') {
						// Raw bytes are copied as read, so the whole string is checked once.
						return core::utf8::valid(result) ? std::optional{ std::move(result) } : std::nullopt;
					}
					if (character < 0x20U) {
						return std::nullopt;
					}
					if (character != '\\') {
						result.push_back(static_cast<char>(character));
						continue;
					}
					if (Position == Text.size()) {
						return std::nullopt;
					}
					const auto escaped = Text[Position++];
					switch (escaped) {
					case '"':  result.push_back('"'); break;
					case '\\': result.push_back('\\'); break;
					case '/':  result.push_back('/'); break;
					case 'b':  result.push_back('\b'); break;
					case 'f':  result.push_back('\f'); break;
					case 'n':  result.push_back('\n'); break;
					case 'r':  result.push_back('\r'); break;
					case 't':  result.push_back('\t'); break;
					case 'u':  {
						auto codePoint = unicode_escape();
						if (!codePoint.has_value()) {
							return std::nullopt;
						}
						if (*codePoint >= 0xd800U && *codePoint <= 0xdbffU) {
							if (Position + 2U > Text.size()
								|| Text.substr(Position, 2U) != "\\u") {
								return std::nullopt;
							}
							Position += 2U;
							const auto low = unicode_escape();
							if (!low.has_value() || *low < 0xdc00U || *low > 0xdfffU) {
								return std::nullopt;
							}
							*codePoint = 0x10000U + ((*codePoint - 0xd800U) << 10U)
								+ (*low - 0xdc00U);
						}
						if (!append_utf8(result, *codePoint)) {
							return std::nullopt;
						}
						break;
					}
					default: return std::nullopt;
					}
				}
				return std::nullopt;
			}

			[[nodiscard]] auto number() -> std::optional<json_value>
			{
				space();
				const auto begin = Position;
				if (Position != Text.size() && Text[Position] == '-') {
					++Position;
				}
				if (Position == Text.size()) {
					return std::nullopt;
				}
				if (Text[Position] == '0') {
					++Position;
				} else if (Text[Position] >= '1' && Text[Position] <= '9') {
					while (Position != Text.size() && std::isdigit(static_cast<unsigned char>(Text[Position]))) {
						++Position;
					}
				} else {
					return std::nullopt;
				}
				if (Position != Text.size() && Text[Position] == '.') {
					++Position;
					const auto fraction = Position;
					while (Position != Text.size() && std::isdigit(static_cast<unsigned char>(Text[Position]))) {
						++Position;
					}
					if (Position == fraction) {
						return std::nullopt;
					}
				}
				if (Position != Text.size()
					&& (Text[Position] == 'e' || Text[Position] == 'E')) {
					++Position;
					if (Position != Text.size()
						&& (Text[Position] == '+' || Text[Position] == '-')) {
						++Position;
					}
					const auto exponent = Position;
					while (Position != Text.size() && std::isdigit(static_cast<unsigned char>(Text[Position]))) {
						++Position;
					}
					if (Position == exponent) {
						return std::nullopt;
					}
				}
				return json_value{ .Value = json_number{
									   .Value = std::string{ Text.substr(begin, Position - begin) } } };
			}

			[[nodiscard]] auto object(const std::size_t aDepth)
				-> std::optional<json_value>
			{
				if (!take('{')) {
					return std::nullopt;
				}
				auto result = json_value::object{ };
				space();
				if (take('}')) {
					return json_value{ .Value = std::move(result) };
				}
				for (;;) {
					auto key = string();
					if (!key.has_value() || !take(':')) {
						return std::nullopt;
					}
					auto member = value(aDepth + 1U);
					if (!member.has_value()
						|| !result.emplace(std::move(*key), std::move(*member)).second) {
						return std::nullopt;
					}
					if (take('}')) {
						return json_value{ .Value = std::move(result) };
					}
					if (!take(',')) {
						return std::nullopt;
					}
				}
			}

			[[nodiscard]] auto array(const std::size_t aDepth)
				-> std::optional<json_value>
			{
				if (!take('[')) {
					return std::nullopt;
				}
				auto result = json_value::array{ };
				space();
				if (take(']')) {
					return json_value{ .Value = std::move(result) };
				}
				for (;;) {
					auto member = value(aDepth + 1U);
					if (!member.has_value()) {
						return std::nullopt;
					}
					result.push_back(std::move(*member));
					if (take(']')) {
						return json_value{ .Value = std::move(result) };
					}
					if (!take(',')) {
						return std::nullopt;
					}
				}
			}

			[[nodiscard]] auto literal(
				const std::string_view aText, json_value aValue)
				-> std::optional<json_value>
			{
				space();
				if (Text.substr(Position, aText.size()) != aText) {
					return std::nullopt;
				}
				Position += aText.size();
				return aValue;
			}

			[[nodiscard]] auto value(const std::size_t aDepth)
				-> std::optional<json_value>
			{
				if (aDepth > 32U) {
					return std::nullopt;
				}
				space();
				if (Position == Text.size()) {
					return std::nullopt;
				}
				switch (Text[Position]) {
				case '{': return object(aDepth);
				case '[': return array(aDepth);
				case '"': {
					auto text = string();
					return text.has_value()
						? std::optional{ json_value{ .Value = std::move(*text) } }
						: std::nullopt;
				}
				case 't': return literal("true", json_value{ .Value = true });
				case 'f': return literal("false", json_value{ .Value = false });
				case 'n': return literal("null", json_value{ });
				default:  return number();
				}
			}

			std::string_view Text;
			std::size_t      Position = 0U;
		};

		[[nodiscard]] auto member(
			const json_value::object& aObject, const std::string_view aName)
			-> const json_value*
		{
			const auto found = aObject.find(aName);
			return found == aObject.end() ? nullptr : &found->second;
		}

		[[nodiscard]] auto object_member(
			const json_value::object& aObject, const std::string_view aName)
			-> const json_value::object*
		{
			const auto* value = member(aObject, aName);
			return value == nullptr
				? nullptr
				: std::get_if<json_value::object>(&value->Value);
		}

		[[nodiscard]] auto text_member(
			const json_value::object& aObject, const std::string_view aName)
			-> const std::string*
		{
			const auto* value = member(aObject, aName);
			return value == nullptr ? nullptr : std::get_if<std::string>(&value->Value);
		}

		[[nodiscard]] auto integer_member(
			const json_value::object& aObject, const std::string_view aName)
			-> std::optional<std::uint64_t>
		{
			const auto* value  = member(aObject, aName);
			const auto* number = value == nullptr
				? nullptr
				: std::get_if<json_number>(&value->Value);
			if (number == nullptr || number->Value.empty()) {
				return std::nullopt;
			}
			auto       result = std::uint64_t{ 0U };
			const auto parsed = std::from_chars(
				number->Value.data(), number->Value.data() + number->Value.size(), result);
			return parsed.ec == std::errc{ }
					&& parsed.ptr == number->Value.data() + number->Value.size()
				? std::optional{ result }
				: std::nullopt;
		}

		[[nodiscard]] auto exact_target_body(
			const std::span<const std::uint8_t> aBody) -> bool
		{
			const auto text = std::string_view{
				reinterpret_cast<const char*>(aBody.data()), aBody.size()
			};
			auto        parsed = json_parser{ text }.parse();
			const auto* root   = parsed.has_value()
				? std::get_if<json_value::object>(&parsed->Value)
				: nullptr;
			if (root == nullptr) {
				return false;
			}
			const auto* head           = object_member(*root, "head");
			const auto* base           = object_member(*root, "base");
			const auto* repo           = base == nullptr ? nullptr : object_member(*base, "repo");
			const auto* node           = text_member(*root, "node_id");
			const auto* state          = text_member(*root, "state");
			const auto* headSha        = head == nullptr ? nullptr : text_member(*head, "sha");
			const auto* headRef        = head == nullptr ? nullptr : text_member(*head, "ref");
			const auto* baseSha        = base == nullptr ? nullptr : text_member(*base, "sha");
			const auto* baseRef        = base == nullptr ? nullptr : text_member(*base, "ref");
			const auto* repositoryNode = repo == nullptr ? nullptr : text_member(*repo, "node_id");
			const auto* repositoryName = repo == nullptr ? nullptr : text_member(*repo, "full_name");
			return integer_member(*root, "id") == 4'650'736'621U
				&& integer_member(*root, "number") == 2U
				&& node != nullptr && *node == "PR_kwDOTyvof88AAAABFTSb7Q"
				&& state != nullptr && *state == "open"
				&& headSha != nullptr && *headSha == designated_head_oid
				&& headRef != nullptr
				&& *headRef == "codex/rfc0022-live-comments-20260926"
				&& baseSha != nullptr && *baseSha == designated_base_oid
				&& baseRef != nullptr && *baseRef == "codex/rfc0022-fixture-base-20260926"
				&& repo != nullptr
				&& integer_member(*repo, "id") == 1'328'277'631U
				&& repositoryNode != nullptr && *repositoryNode == "R_kgDOTyvofw"
				&& repositoryName != nullptr
				&& *repositoryName == "MikeMarcin/BPR-Test";
		}

		void capture(const int aDescriptor, const std::size_t aLimit, bounded_capture& aCapture)
		{
			auto descriptor = file_descriptor{ aDescriptor };
			auto block      = std::array<std::uint8_t, 16U * 1024U>{ };
			for (;;) {
				const auto count = ::read(descriptor.get(), block.data(), block.size());
				if (count < 0 && errno == EINTR) {
					continue;
				}
				if (count < 0) {
					aCapture.Failed = true;
					return;
				}
				if (count == 0) {
					return;
				}
				const auto size = static_cast<std::size_t>(count);
				if (size > aLimit - aCapture.Bytes.size()) {
					aCapture.Limit = true;
					return;
				}
				aCapture.Bytes.insert(
					aCapture.Bytes.end(), block.begin(), block.begin() + count);
			}
		}

		enum class child_outcome : std::uint8_t {
			success,
			failure,
			timeout,
			cancelled,
			response_limit,
		};

		[[nodiscard]] auto wait_child(
			const pid_t aProcess, const std::stop_token aStop) noexcept -> child_outcome
		{
			const auto deadline = std::chrono::steady_clock::now()
				+ std::chrono::seconds{ 17 };
			for (;;) {
				int        status = 0;
				const auto waited = ::waitpid(aProcess, &status, WNOHANG);
				if (waited < 0 && errno == EINTR) {
					continue;
				}
				if (waited == aProcess) {
					if (!WIFEXITED(status)) {
						return child_outcome::failure;
					}
					if (WEXITSTATUS(status) == EXIT_SUCCESS) {
						return child_outcome::success;
					}
					if (WEXITSTATUS(status) == 28) {
						return child_outcome::timeout;
					}
					return WEXITSTATUS(status) == 63
						? child_outcome::response_limit
						: child_outcome::failure;
				}
				if (waited < 0) {
					return child_outcome::failure;
				}
				const auto cancelled = aStop.stop_requested();
				const auto timedOut  = std::chrono::steady_clock::now() >= deadline;
				if (cancelled || timedOut) {
					static_cast<void>(::kill(aProcess, SIGKILL));
					while (::waitpid(aProcess, &status, 0) < 0 && errno == EINTR) {
					}
					return cancelled ? child_outcome::cancelled : child_outcome::timeout;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
			}
		}

		struct curl_response
		{
			std::vector<std::uint8_t> Headers;
			std::vector<std::uint8_t> Body;
		};

		[[nodiscard]] auto perform_curl(
			std::vector<std::string> aArguments, const std::stop_token aStop)
			-> std::expected<curl_response, designated_live_public_observation_failure>
		{
			if (aStop.stop_requested()) {
				return std::unexpected(
					designated_live_public_observation_failure::cancelled);
			}
			auto curl = retain_trusted_curl();
			if (!curl.has_value()) {
				return std::unexpected(
					designated_live_public_observation_failure::unavailable);
			}
			auto outputPipe = std::array<int, 2>{ -1, -1 };
			auto errorPipe  = std::array<int, 2>{ -1, -1 };
			if (::pipe(outputPipe.data()) != 0 || ::pipe(errorPipe.data()) != 0) {
				return std::unexpected(
					designated_live_public_observation_failure::unavailable);
			}
			auto outputRead  = file_descriptor{ outputPipe[0] };
			auto outputWrite = file_descriptor{ outputPipe[1] };
			auto errorRead   = file_descriptor{ errorPipe[0] };
			auto errorWrite  = file_descriptor{ errorPipe[1] };
			for (const auto descriptor : {
					 outputRead.get(), outputWrite.get(), errorRead.get(), errorWrite.get() }) {
				if (::fcntl(descriptor, F_SETFD, FD_CLOEXEC) != 0) {
					return std::unexpected(
						designated_live_public_observation_failure::unavailable);
				}
			}
			posix_spawn_file_actions_t actions{ };
			posix_spawnattr_t          attributes{ };
			if (posix_spawn_file_actions_init(&actions) != 0
				|| posix_spawnattr_init(&attributes) != 0) {
				return std::unexpected(
					designated_live_public_observation_failure::unavailable);
			}
			auto cleanup = [&] {
				static_cast<void>(posix_spawn_file_actions_destroy(&actions));
				static_cast<void>(posix_spawnattr_destroy(&attributes));
			};
			const auto spawnFlags = static_cast<short>(
				POSIX_SPAWN_CLOEXEC_DEFAULT | POSIX_SPAWN_START_SUSPENDED);
			if (posix_spawn_file_actions_adddup2(&actions, outputWrite.get(), STDOUT_FILENO) != 0
				|| posix_spawn_file_actions_adddup2(
					   &actions, errorWrite.get(), STDERR_FILENO)
					!= 0
				|| posix_spawnattr_setflags(&attributes, spawnFlags)
					!= 0) {
				cleanup();
				return std::unexpected(
					designated_live_public_observation_failure::unavailable);
			}
			aArguments.insert(aArguments.begin(), std::string{ curl_path });
			auto argumentPointers = std::vector<char*>{ };
			argumentPointers.reserve(aArguments.size() + 1U);
			for (auto& argument : aArguments) {
				argumentPointers.push_back(argument.data());
			}
			argumentPointers.push_back(nullptr);
			auto       environment = std::array<char*, 1>{ nullptr };
			pid_t      process     = -1;
			const auto spawned     = posix_spawn(
				&process,
				aArguments.front().c_str(),
				&actions,
				&attributes,
				argumentPointers.data(),
				environment.data());
			cleanup();
			outputWrite = file_descriptor{ };
			errorWrite  = file_descriptor{ };
			if (spawned != 0) {
				return std::unexpected(
					designated_live_public_observation_failure::unavailable);
			}
			auto terminate = [&] {
				static_cast<void>(::kill(process, SIGKILL));
				int status = 0;
				while (::waitpid(process, &status, 0) < 0 && errno == EINTR) {
				}
			};
			const auto guestDigest = guest_signing_digest(process);
			if (!exact_retained_curl(*curl) || !guestDigest.has_value()
				|| *guestDigest != curl->SigningDigest || ::kill(process, SIGCONT) != 0) {
				terminate();
				return std::unexpected(
					designated_live_public_observation_failure::unavailable);
			}
			auto output       = bounded_capture{ };
			auto errors       = bounded_capture{ };
			auto outputThread = std::jthread{
				[descriptor = outputRead.release(), &output] {
					capture(
						descriptor,
						response_header_limit + response_body_limit,
						output);
				}
			};
			auto errorThread = std::jthread{
				[descriptor = errorRead.release(), &errors] {
					capture(descriptor, 16U * 1024U, errors);
				}
			};
			const auto outcome = wait_child(process, aStop);
			outputThread.join();
			errorThread.join();
			if (output.Limit || errors.Limit
				|| outcome == child_outcome::response_limit) {
				return std::unexpected(
					designated_live_public_observation_failure::response_limit);
			}
			if (outcome == child_outcome::cancelled) {
				return std::unexpected(
					designated_live_public_observation_failure::cancelled);
			}
			if (outcome == child_outcome::timeout) {
				return std::unexpected(
					designated_live_public_observation_failure::timeout);
			}
			if (outcome != child_outcome::success || output.Failed || errors.Failed) {
				return std::unexpected(
					designated_live_public_observation_failure::network);
			}
			const auto marker = std::ranges::search(
				output.Bytes, std::array<std::uint8_t, 4>{ '\r', '\n', '\r', '\n' });
			if (marker.empty()) {
				return std::unexpected(
					designated_live_public_observation_failure::malformed);
			}
			const auto boundary = static_cast<std::size_t>(
									  std::distance(output.Bytes.begin(), marker.begin()))
				+ 4U;
			if (boundary > response_header_limit) {
				return std::unexpected(
					designated_live_public_observation_failure::response_limit);
			}
			return curl_response{
				.Headers = std::vector<std::uint8_t>{
					output.Bytes.begin(), output.Bytes.begin() + static_cast<std::ptrdiff_t>(boundary) },
				.Body = std::vector<std::uint8_t>{ output.Bytes.begin() + static_cast<std::ptrdiff_t>(boundary), output.Bytes.end() },
			};
		}

		[[nodiscard]] auto observe(
			const std::optional<std::uint16_t>          aTestPort,
			const std::optional<std::filesystem::path>& aTestAuthority,
			const std::stop_token                       aStop)
			-> designated_live_public_observation
		{
			const auto expected = target();
			if (!expected.has_value() || !application::valid(*expected)) {
				return std::unexpected(
					designated_live_public_observation_failure::unavailable);
			}
			auto url = std::string{ designated_origin };
#if defined(BPR_DESIGNATED_LIVE_PUBLIC_OBSERVER_TESTING)
			if (aTestPort.has_value()) {
				url.append(":").append(std::to_string(*aTestPort));
			}
#else
			if (aTestPort.has_value() || aTestAuthority.has_value()) {
				return std::unexpected(
					designated_live_public_observation_failure::unavailable);
			}
#endif
			url.append(designated_path);
			auto arguments = std::vector<std::string>{
				"--disable",
				"--silent",
				"--request",
				"GET",
				"--proto",
				"=https",
				"--tlsv1.2",
				"--tls-max",
				"1.3",
				"--http1.1",
				"--noproxy",
				"*",
				"--proxy",
				"",
				"--max-redirs",
				"0",
				"--connect-timeout",
				"15",
				"--max-time",
				"15",
				"--max-filesize",
				std::to_string(response_body_limit),
				"--include",
				"--output",
				"-",
			};
			arguments.insert(
				arguments.end(),
				{
					"--header",
					"Accept: application/vnd.github+json",
					"--header",
					"User-Agent: BetterPullReview/0.1",
					"--header",
					"X-GitHub-Api-Version: 2022-11-28",
					"--header",
					"Accept-Encoding: identity",
					"--header",
					"Accept-Language: *",
				});
#if defined(BPR_DESIGNATED_LIVE_PUBLIC_OBSERVER_TESTING)
			if (aTestPort.has_value() && aTestAuthority.has_value()) {
				arguments.insert(
					arguments.end(),
					{
						"--resolve",
						"api.github.com:" + std::to_string(*aTestPort) + ":127.0.0.1",
						"--cacert",
						aTestAuthority->string(),
					});
			}
#endif
			arguments.push_back(url);
			const auto response = perform_curl(std::move(arguments), aStop);
			if (!response.has_value()) {
				return std::unexpected(response.error());
			}
			const auto headers = std::string_view{
				reinterpret_cast<const char*>(response->Headers.data()),
				response->Headers.size(),
			};
			if (!headers.ends_with("\r\n\r\n") || headers.contains('\0')) {
				return std::unexpected(
					designated_live_public_observation_failure::malformed);
			}
			const auto firstLineEnd = headers.find("\r\n");
			if (firstLineEnd == std::string_view::npos
				|| !headers.substr(0U, firstLineEnd).starts_with("HTTP/1.1 ")) {
				return std::unexpected(
					designated_live_public_observation_failure::malformed);
			}
			auto       status       = 0;
			const auto statusText   = headers.substr(9U, 3U);
			const auto parsedStatus = std::from_chars(
				statusText.data(), statusText.data() + statusText.size(), status);
			if (parsedStatus.ec != std::errc{ }
				|| parsedStatus.ptr != statusText.data() + statusText.size()) {
				return std::unexpected(
					designated_live_public_observation_failure::malformed);
			}
			auto wireHeaders = std::vector<std::pair<std::string, std::string>>{ };
			auto cursor      = firstLineEnd + 2U;
			while (cursor + 2U < headers.size()) {
				const auto end = headers.find("\r\n", cursor);
				if (end == cursor) {
					break;
				}
				if (end == std::string_view::npos || wireHeaders.size() == 64U) {
					return std::unexpected(
						designated_live_public_observation_failure::response_limit);
				}
				const auto line  = headers.substr(cursor, end - cursor);
				const auto colon = line.find(':');
				if (colon == std::string_view::npos || colon == 0U) {
					return std::unexpected(
						designated_live_public_observation_failure::malformed);
				}
				auto value = line.substr(colon + 1U);
				while (value.starts_with(' ')) {
					value.remove_prefix(1U);
				}
				wireHeaders.emplace_back(
					std::string{ line.substr(0U, colon) }, std::string{ value });
				cursor = end + 2U;
			}
			const auto hasHeader = [&](const std::string_view aName) {
				return std::ranges::any_of(wireHeaders, [&](const auto& aHeader) {
					return std::ranges::equal(
						aHeader.first,
						aName,
						[](const char aLeft, const char aRight) {
							return static_cast<char>(std::tolower(
									   static_cast<unsigned char>(aLeft)))
								== static_cast<char>(std::tolower(
									static_cast<unsigned char>(aRight)));
						});
				});
			};
			if ((status >= 300 && status < 400) || hasHeader("location")) {
				return std::unexpected(
					designated_live_public_observation_failure::redirect);
			}
			if (status != 200) {
				return std::unexpected(
					designated_live_public_observation_failure::http);
			}
			if (!exact_target_body(response->Body)) {
				return std::unexpected(
					designated_live_public_observation_failure::identity_mismatch);
			}
			return *expected;
		}
	} // namespace

	auto observe_designated_live_public_target(const std::stop_token aStop)
		-> designated_live_public_observation
	{
		return observe(std::nullopt, std::nullopt, aStop);
	}

#if defined(BPR_DESIGNATED_LIVE_PUBLIC_OBSERVER_TESTING)
	auto observe_designated_live_public_target_for_test(
		const designated_live_public_observer_test_configuration& aConfiguration,
		const std::stop_token                                     aStop)
		-> designated_live_public_observation
	{
		if (aConfiguration.Port == 0U
			|| !aConfiguration.CertificateAuthority.is_absolute()) {
			return std::unexpected(
				designated_live_public_observation_failure::unavailable);
		}
		return observe(aConfiguration.Port, aConfiguration.CertificateAuthority, aStop);
	}
#endif
} // namespace bpr::adapters::github::detail
