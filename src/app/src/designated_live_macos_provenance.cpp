#include "designated_live_macos_provenance.hpp"

#if defined(__APPLE__)

	#include <Security/Security.h>
	#include <bsm/libbsm.h>
	#include <mach-o/dyld.h>
	#include <mach/mach_init.h>

	#include <algorithm>
	#include <array>
	#include <cstdint>
	#include <cstring>
	#include <system_error>
	#include <utility>

namespace bpr::app::designated_live_provenance {
	namespace {
		/// Where the supervisor sits relative to the application inside one
		/// capsule. Fixed by the capsule builder, not discovered at runtime.
		constexpr auto application_suffix = std::string_view{
			"BetterPullReview.app/Contents/MacOS/BetterPullReview"
		};
		constexpr auto helper_relative = std::string_view{
			"BetterPullReview.app/Contents/Helpers/"
			"better_pull_review_designated_live_supervisor"
		};

		/// Wide enough for both fixed words with room to spare, and fixed so a
		/// message is one size on both sides. A peer that sends a different
		/// size is refused by the receive itself.
		constexpr auto payload_bytes = std::size_t{ 64U };

		struct handshake_message
		{
			mach_msg_header_t               Header{ };
			std::array<char, payload_bytes> Payload{ };
		};

		struct handshake_reception
		{
			mach_msg_header_t               Header{ };
			std::array<char, payload_bytes> Payload{ };
			mach_msg_audit_trailer_t        Trailer{ };
		};

		/// A CoreFoundation reference that releases itself.
		template <typename Type>
		class cf_reference
		{
		public:
			cf_reference() = default;

			explicit cf_reference(Type aValue) noexcept
				: Value{ aValue }
			{
			}

			cf_reference(const cf_reference&)                    = delete;
			auto operator=(const cf_reference&) -> cf_reference& = delete;

			cf_reference(cf_reference&& aOther) noexcept
				: Value{ std::exchange(aOther.Value, nullptr) }
			{
			}

			auto operator=(cf_reference&& aOther) noexcept -> cf_reference&
			{
				if (this != &aOther) {
					reset();
					Value = std::exchange(aOther.Value, nullptr);
				}
				return *this;
			}

			~cf_reference() { reset(); }

			[[nodiscard]] auto get() const noexcept -> Type { return Value; }

			explicit operator bool() const noexcept { return Value != nullptr; }

		private:
			void reset() noexcept
			{
				if (Value != nullptr) {
					CFRelease(Value);
					Value = nullptr;
				}
			}

			Type Value = nullptr;
		};

		[[nodiscard]] auto registered_ports() -> std::vector<mach_port_t>
		{
			mach_port_array_t      ports = nullptr;
			mach_msg_type_number_t count = 0U;
			if (mach_ports_lookup(mach_task_self(), &ports, &count)
				!= KERN_SUCCESS) {
				return { };
			}
			return std::vector<mach_port_t>{ ports, ports + count };
		}

		[[nodiscard]] auto service_slot_port() -> mach_port_t
		{
			const auto ports = registered_ports();
			return ports.size() <= SERVICE_SLOT ? MACH_PORT_NULL
												: ports[SERVICE_SLOT];
		}

		[[nodiscard]] auto publish_in_service_slot(const mach_port_t aPort)
			-> bool
		{
			auto ports = registered_ports();
			if (ports.size() <= SERVICE_SLOT) {
				ports.resize(SERVICE_SLOT + 1U, MACH_PORT_NULL);
			}
			ports[SERVICE_SLOT] = aPort;
			return mach_ports_register(
					   mach_task_self(),
					   ports.data(),
					   static_cast<mach_msg_type_number_t>(ports.size()))
				== KERN_SUCCESS;
		}

		/// The code-directory hash of whatever the given signing reference
		/// names, or nothing when it cannot be established.
		[[nodiscard]] auto unique_identity(const CFDictionaryRef aInformation)
			-> std::optional<std::vector<std::uint8_t>>
		{
			if (aInformation == nullptr) {
				return std::nullopt;
			}
			const auto* const unique = static_cast<CFDataRef>(
				CFDictionaryGetValue(aInformation, kSecCodeInfoUnique));
			if (unique == nullptr) {
				return std::nullopt;
			}
			const auto* const bytes  = CFDataGetBytePtr(unique);
			const auto        length = CFDataGetLength(unique);
			if (bytes == nullptr || length <= 0) {
				return std::nullopt;
			}
			return std::vector<std::uint8_t>{
				bytes, bytes + static_cast<std::size_t>(length)
			};
		}

		/// The identity of the live process a kernel audit trailer names.
		///
		/// Keyed by `kSecGuestAttributeAudit` rather than by pid: the token
		/// names the process that sent the message, and cannot be satisfied by
		/// a different process that inherited the same pid.
		[[nodiscard]] auto peer_identity(const audit_token_t& aToken)
			-> std::optional<std::vector<std::uint8_t>>
		{
			const auto audit = cf_reference<CFDataRef>{ CFDataCreate(
				nullptr,
				reinterpret_cast<const UInt8*>(&aToken),
				static_cast<CFIndex>(sizeof(aToken))) };
			if (!audit) {
				return std::nullopt;
			}
			const void* keys[]   = { kSecGuestAttributeAudit };
			const void* values[] = { audit.get() };
			const auto  attributes
				= cf_reference<CFDictionaryRef>{ CFDictionaryCreate(
					nullptr,
					keys,
					values,
					1,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks) };
			if (!attributes) {
				return std::nullopt;
			}
			SecCodeRef guestRaw = nullptr;
			if (SecCodeCopyGuestWithAttributes(
					nullptr, attributes.get(), kSecCSDefaultFlags, &guestRaw)
					!= errSecSuccess
				|| guestRaw == nullptr) {
				return std::nullopt;
			}
			const auto guest = cf_reference<SecCodeRef>{ guestRaw };
			// Dynamic validity, not merely presence. A peer whose pages no
			// longer match its signature must not answer this successfully.
			if (SecCodeCheckValidity(guest.get(), kSecCSDefaultFlags, nullptr)
				!= errSecSuccess) {
				return std::nullopt;
			}
			CFDictionaryRef informationRaw = nullptr;
			if (SecCodeCopySigningInformation(
					guest.get(), kSecCSDefaultFlags, &informationRaw)
				!= errSecSuccess) {
				return std::nullopt;
			}
			const auto information
				= cf_reference<CFDictionaryRef>{ informationRaw };
			return unique_identity(information.get());
		}

		/// The identity of an image at rest.
		[[nodiscard]] auto image_identity(const std::filesystem::path& aPath)
			-> std::optional<std::vector<std::uint8_t>>
		{
			const auto text = aPath.string();
			const auto url  = cf_reference<CFURLRef>{
				CFURLCreateFromFileSystemRepresentation(
					nullptr,
					reinterpret_cast<const UInt8*>(text.data()),
					static_cast<CFIndex>(text.size()),
					FALSE)
			};
			if (!url) {
				return std::nullopt;
			}
			SecStaticCodeRef codeRaw = nullptr;
			if (SecStaticCodeCreateWithPath(
					url.get(), kSecCSDefaultFlags, &codeRaw)
					!= errSecSuccess
				|| codeRaw == nullptr) {
				return std::nullopt;
			}
			const auto      code           = cf_reference<SecStaticCodeRef>{ codeRaw };
			CFDictionaryRef informationRaw = nullptr;
			if (SecCodeCopySigningInformation(
					code.get(), kSecCSDefaultFlags, &informationRaw)
				!= errSecSuccess) {
				return std::nullopt;
			}
			const auto information
				= cf_reference<CFDictionaryRef>{ informationRaw };
			return unique_identity(information.get());
		}

		[[nodiscard]] auto payload_equals(
			const std::array<char, payload_bytes>& aPayload,
			const std::string_view                 aExpected) -> bool
		{
			if (aExpected.size() > aPayload.size()) {
				return false;
			}
			// The whole fixed-size payload is compared, so trailing bytes
			// cannot smuggle anything past a prefix match.
			return std::string_view{ aPayload.data(), aExpected.size() }
				== aExpected
				&& std::all_of(
					   aPayload.begin() + aExpected.size(),
					   aPayload.end(),
					   [](const char aByte) { return aByte == '\0'; });
		}

		void fill_payload(
			std::array<char, payload_bytes>& aPayload,
			const std::string_view           aText)
		{
			aPayload.fill('\0');
			std::memcpy(aPayload.data(), aText.data(), aText.size());
		}
	} // namespace

	auto supervisor_channel::claim() -> std::optional<supervisor_channel>
	{
		auto snapshot = registered_ports();
		if (snapshot.size() > SERVICE_SLOT
			&& snapshot[SERVICE_SLOT] != MACH_PORT_NULL) {
			// fact_6: an occupied slot fails closed. It is not ours to take,
			// and taking it would be the substitution this refuses.
			return std::nullopt;
		}
		mach_port_t service = MACH_PORT_NULL;
		if (mach_port_allocate(
				mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &service)
			!= KERN_SUCCESS) {
			return std::nullopt;
		}
		if (mach_port_insert_right(
				mach_task_self(), service, service, MACH_MSG_TYPE_MAKE_SEND)
			!= KERN_SUCCESS) {
			static_cast<void>(mach_port_deallocate(mach_task_self(), service));
			return std::nullopt;
		}
		if (!publish_in_service_slot(service)) {
			static_cast<void>(mach_port_deallocate(mach_task_self(), service));
			return std::nullopt;
		}
		auto claimed     = supervisor_channel{ };
		claimed.Service  = service;
		claimed.Snapshot = std::move(snapshot);
		return claimed;
	}

	supervisor_channel::supervisor_channel(supervisor_channel&& aOther) noexcept
		: Service{ std::exchange(aOther.Service, MACH_PORT_NULL) }
		, Snapshot{ std::move(aOther.Snapshot) }
		, Restored{ std::exchange(aOther.Restored, true) }
	{
	}

	auto supervisor_channel::operator=(supervisor_channel&& aOther) noexcept
		-> supervisor_channel&
	{
		if (this != &aOther) {
			static_cast<void>(restore());
			if (Service != MACH_PORT_NULL) {
				static_cast<void>(
					mach_port_deallocate(mach_task_self(), Service));
			}
			Service  = std::exchange(aOther.Service, MACH_PORT_NULL);
			Snapshot = std::move(aOther.Snapshot);
			Restored = std::exchange(aOther.Restored, true);
		}
		return *this;
	}

	supervisor_channel::~supervisor_channel()
	{
		// Restore even when the caller did not. A right left published in the
		// slot would be inherited by the next spawn from this task, which is
		// precisely what the snapshot discipline exists to prevent.
		static_cast<void>(restore());
		if (Service != MACH_PORT_NULL) {
			static_cast<void>(mach_port_deallocate(mach_task_self(), Service));
			Service = MACH_PORT_NULL;
		}
	}

	auto supervisor_channel::restore() -> bool
	{
		if (Restored) {
			return true;
		}
		Restored = true;
		return mach_ports_register(
				   mach_task_self(),
				   Snapshot.data(),
				   static_cast<mach_msg_type_number_t>(Snapshot.size()))
			== KERN_SUCCESS;
	}

	auto supervisor_channel::authenticate(
		const pid_t                  aChild,
		const std::filesystem::path& aImage) -> bool
	{
		if (Service == MACH_PORT_NULL) {
			return false;
		}
		auto greeting = handshake_reception{ };
		if (mach_msg(
				&greeting.Header,
				MACH_RCV_MSG | MACH_RCV_TIMEOUT
					| MACH_RCV_TRAILER_TYPE(MACH_RCV_TRAILER_AUDIT)
					| MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT),
				0,
				sizeof(handshake_reception),
				Service,
				handshake_timeout_milliseconds,
				MACH_PORT_NULL)
			!= KERN_SUCCESS) {
			return false;
		}
		if (!payload_equals(greeting.Payload, application_greeting)
			|| greeting.Trailer.msgh_trailer_size
				< sizeof(mach_msg_audit_trailer_t)) {
			return false;
		}
		auto auditProcess = pid_t{ -1 };
		audit_token_to_au32(
			greeting.Trailer.msgh_audit,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			&auditProcess,
			nullptr,
			nullptr);
		if (auditProcess != aChild) {
			return false;
		}
		const auto peer  = peer_identity(greeting.Trailer.msgh_audit);
		const auto image = image_identity(aImage);
		if (!peer.has_value() || !image.has_value() || *peer != *image) {
			return false;
		}
		auto accept                    = handshake_message{ };
		accept.Header.msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0);
		accept.Header.msgh_size        = sizeof(handshake_message);
		accept.Header.msgh_remote_port = greeting.Header.msgh_remote_port;
		accept.Header.msgh_local_port  = MACH_PORT_NULL;
		accept.Header.msgh_id          = 2;
		fill_payload(accept.Payload, supervisor_reply);
		return mach_msg(
				   &accept.Header,
				   MACH_SEND_MSG,
				   sizeof(handshake_message),
				   0,
				   MACH_PORT_NULL,
				   handshake_timeout_milliseconds,
				   MACH_PORT_NULL)
			== KERN_SUCCESS;
	}

	auto authenticate_supervisor(const std::filesystem::path& aSupervisorImage)
		-> bool
	{
		const auto service = service_slot_port();
		if (service == MACH_PORT_NULL) {
			// The whole of fact_3's prohibition, in one branch. Nothing is
			// consulted here but the slot: not argv, not the environment, not
			// a parent pid, not a file somebody left behind.
			return false;
		}
		mach_port_t reply = MACH_PORT_NULL;
		if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &reply)
			!= KERN_SUCCESS) {
			return false;
		}
		auto greeting             = handshake_message{ };
		greeting.Header.msgh_bits = MACH_MSGH_BITS(
			MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
		greeting.Header.msgh_size        = sizeof(handshake_message);
		greeting.Header.msgh_remote_port = service;
		greeting.Header.msgh_local_port  = reply;
		greeting.Header.msgh_id          = 1;
		fill_payload(greeting.Payload, application_greeting);
		const auto sent = mach_msg(
			&greeting.Header,
			MACH_SEND_MSG,
			sizeof(handshake_message),
			0,
			MACH_PORT_NULL,
			handshake_timeout_milliseconds,
			MACH_PORT_NULL);
		if (sent != KERN_SUCCESS) {
			static_cast<void>(mach_port_mod_refs(
				mach_task_self(), reply, MACH_PORT_RIGHT_RECEIVE, -1));
			return false;
		}
		auto       accepted = handshake_reception{ };
		const auto received = mach_msg(
			&accepted.Header,
			MACH_RCV_MSG | MACH_RCV_TIMEOUT
				| MACH_RCV_TRAILER_TYPE(MACH_RCV_TRAILER_AUDIT)
				| MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT),
			0,
			sizeof(handshake_reception),
			reply,
			handshake_timeout_milliseconds,
			MACH_PORT_NULL);
		static_cast<void>(mach_port_mod_refs(
			mach_task_self(), reply, MACH_PORT_RIGHT_RECEIVE, -1));
		if (received != KERN_SUCCESS) {
			return false;
		}
		if (!payload_equals(accepted.Payload, supervisor_reply)
			|| accepted.Trailer.msgh_trailer_size
				< sizeof(mach_msg_audit_trailer_t)) {
			return false;
		}
		// The other direction of fact_4. Answering with the right word is not
		// enough; the answer has to come from the supervisor image this
		// capsule actually contains.
		const auto peer  = peer_identity(accepted.Trailer.msgh_audit);
		const auto image = image_identity(aSupervisorImage);
		return peer.has_value() && image.has_value() && *peer == *image;
	}

	auto running_executable_path() -> std::optional<std::filesystem::path>
	{
		auto required = std::uint32_t{ 0U };
		static_cast<void>(::_NSGetExecutablePath(nullptr, &required));
		if (required == 0U) {
			return std::nullopt;
		}
		auto bytes = std::vector<char>(required, '\0');
		if (::_NSGetExecutablePath(bytes.data(), &required) != 0) {
			return std::nullopt;
		}
		std::error_code error;
		auto            result = std::filesystem::canonical(bytes.data(), error);
		return error || !result.is_absolute() ? std::nullopt
											  : std::optional{ std::move(result) };
	}

	auto supervisor_image_for(const std::filesystem::path& aApplicationImage)
		-> std::optional<std::filesystem::path>
	{
		const auto text = aApplicationImage.generic_string();
		if (!text.ends_with(application_suffix)
			|| text.size() <= application_suffix.size()) {
			return std::nullopt;
		}
		const auto root = std::filesystem::path{
			text.substr(0U, text.size() - application_suffix.size())
		};
		if (root.empty() || root.lexically_normal() != root) {
			return std::nullopt;
		}
		return root / helper_relative;
	}
}

#endif // defined(__APPLE__)
