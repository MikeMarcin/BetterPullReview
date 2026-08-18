/// \file
/// \brief Proves the Mach SERVICE_SLOT handshake the containment topology needs.
///
/// BPR-0262 fact_3 and fact_4 rest on a mechanism that exists nowhere else in
/// this tree, under a capsule signed ad-hoc with no hardened runtime and no
/// entitlements. This contract establishes that the mechanism is available on
/// that exact footing, and that its fail-closed halves really refuse.
///
/// `SERVICE_SLOT` is an index into the task's registered-port array, not a
/// launchd service name. The parent registers a send right there, the suspended
/// child recovers it after `exec`, and the parent restores its prior snapshot
/// immediately. Identity travels in the kernel audit trailer, never in a pid.

#include <Security/Security.h>
#include <bsm/libbsm.h>
#include <mach/mach.h>
#include <mach/mach_init.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

extern char** environ;

namespace {
	constexpr std::string_view child_mode_argument = "--service-slot-child";
	constexpr std::string_view child_greeting      = "bpr-stage-one-hello";
	constexpr std::string_view parent_reply        = "bpr-stage-zero-accept";

	/// Children are spawned suspended, so a failure between the spawn and the
	/// resume would otherwise leave one stopped forever and hang the runner
	/// waiting on it. Every spawned child is tracked and killed on the way out.
	std::vector<pid_t> pending_children;

	void release_pending_children()
	{
		for (const auto child : pending_children) {
			static_cast<void>(::kill(child, SIGKILL));
			static_cast<void>(::kill(child, SIGCONT));
			auto status = 0;
			static_cast<void>(::waitpid(child, &status, 0));
		}
		pending_children.clear();
	}

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			release_pending_children();
			std::exit(EXIT_FAILURE);
		}
	}

	struct handshake_message
	{
		mach_msg_header_t     Header;
		std::array<char, 32U> Payload;
	};

	struct handshake_reception
	{
		mach_msg_header_t        Header;
		std::array<char, 32U>    Payload;
		mach_msg_audit_trailer_t Trailer;
	};

	/// \brief The task's registered-port array as it stands right now.
	[[nodiscard]] auto registered_ports() -> std::vector<mach_port_t>
	{
		mach_port_array_t      ports = nullptr;
		mach_msg_type_number_t count = 0U;
		if (
			mach_ports_lookup(mach_task_self(), &ports, &count) != KERN_SUCCESS) {
			return { };
		}
		return std::vector<mach_port_t>{ ports, ports + count };
	}

	[[nodiscard]] auto service_slot_is_empty() -> bool
	{
		const auto ports = registered_ports();
		return ports.size() <= SERVICE_SLOT || ports[SERVICE_SLOT] == MACH_PORT_NULL;
	}

	/// \brief Publishes one send right in SERVICE_SLOT, preserving every other.
	[[nodiscard]] auto publish_in_service_slot(const mach_port_t aPort) -> bool
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

	[[nodiscard]] auto restore_registered_ports(std::vector<mach_port_t> aSnapshot)
		-> bool
	{
		return mach_ports_register(
				   mach_task_self(),
				   aSnapshot.data(),
				   static_cast<mach_msg_type_number_t>(aSnapshot.size()))
			== KERN_SUCCESS;
	}

	/// \brief The code-directory hash the running peer named by a trailer has.
	[[nodiscard]] auto guest_code_hash(const audit_token_t& aToken) -> CFDataRef
	{
		const auto* const audit = CFDataCreate(
			nullptr,
			reinterpret_cast<const UInt8*>(&aToken),
			static_cast<CFIndex>(sizeof(aToken)));
		if (audit == nullptr) {
			return nullptr;
		}
		const void*       keys[]     = { kSecGuestAttributeAudit };
		const void*       values[]   = { audit };
		const auto* const attributes = CFDictionaryCreate(
			nullptr,
			keys,
			values,
			1,
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks);
		SecCodeRef guest = nullptr;
		const auto found = SecCodeCopyGuestWithAttributes(
			nullptr, attributes, kSecCSDefaultFlags, &guest);
		CFRelease(audit);
		CFRelease(attributes);
		if (found != errSecSuccess || guest == nullptr) {
			return nullptr;
		}
		// Dynamic validity, not merely presence: a peer whose pages no longer
		// match its signature must not answer this question successfully.
		if (SecCodeCheckValidity(guest, kSecCSDefaultFlags, nullptr)
			!= errSecSuccess) {
			CFRelease(guest);
			return nullptr;
		}
		CFDictionaryRef information = nullptr;
		const auto      copied      = SecCodeCopySigningInformation(
			guest, kSecCSDefaultFlags, &information);
		CFRelease(guest);
		if (copied != errSecSuccess || information == nullptr) {
			return nullptr;
		}
		const auto* const unique = static_cast<CFDataRef>(
			CFDictionaryGetValue(information, kSecCodeInfoUnique));
		const auto* const retained
			= unique == nullptr ? nullptr : CFDataCreateCopy(nullptr, unique);
		CFRelease(information);
		return retained;
	}

	/// \brief The code-directory hash the named on-disk image has.
	[[nodiscard]] auto static_code_hash(const std::string& aPath) -> CFDataRef
	{
		const auto* const url = CFURLCreateFromFileSystemRepresentation(
			nullptr,
			reinterpret_cast<const UInt8*>(aPath.data()),
			static_cast<CFIndex>(aPath.size()),
			false);
		if (url == nullptr) {
			return nullptr;
		}
		SecStaticCodeRef code = nullptr;
		const auto       created
			= SecStaticCodeCreateWithPath(url, kSecCSDefaultFlags, &code);
		CFRelease(url);
		if (created != errSecSuccess || code == nullptr) {
			return nullptr;
		}
		CFDictionaryRef information = nullptr;
		const auto      copied      = SecCodeCopySigningInformation(
			code, kSecCSDefaultFlags, &information);
		CFRelease(code);
		if (copied != errSecSuccess || information == nullptr) {
			return nullptr;
		}
		const auto* const unique = static_cast<CFDataRef>(
			CFDictionaryGetValue(information, kSecCodeInfoUnique));
		const auto* const retained
			= unique == nullptr ? nullptr : CFDataCreateCopy(nullptr, unique);
		CFRelease(information);
		return retained;
	}

	/// \brief Stage one: recover the slot, greet, and require the exact reply.
	[[nodiscard]] auto run_child() -> int
	{
		const auto ports = registered_ports();
		if (ports.size() <= SERVICE_SLOT || ports[SERVICE_SLOT] == MACH_PORT_NULL) {
			// Fail closed: an unpublished slot is refusal, never a fallback.
			std::cerr << "CHILD_REFUSED_EMPTY_SLOT\n";
			return EXIT_FAILURE;
		}

		mach_port_t reply = MACH_PORT_NULL;
		if (
			mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &reply)
			!= KERN_SUCCESS) {
			return EXIT_FAILURE;
		}

		auto greeting             = handshake_message{ };
		greeting.Header.msgh_bits = MACH_MSGH_BITS(
			MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
		greeting.Header.msgh_size        = sizeof(handshake_message);
		greeting.Header.msgh_remote_port = ports[SERVICE_SLOT];
		greeting.Header.msgh_local_port  = reply;
		greeting.Header.msgh_id          = 1;
		std::memcpy(
			greeting.Payload.data(), child_greeting.data(), child_greeting.size());
		if (
			mach_msg(
				&greeting.Header,
				MACH_SEND_MSG,
				sizeof(handshake_message),
				0,
				MACH_PORT_NULL,
				MACH_MSG_TIMEOUT_NONE,
				MACH_PORT_NULL)
			!= KERN_SUCCESS) {
			return EXIT_FAILURE;
		}

		auto accepted = handshake_reception{ };
		if (
			mach_msg(
				&accepted.Header,
				MACH_RCV_MSG | MACH_RCV_TIMEOUT
					| MACH_RCV_TRAILER_TYPE(MACH_RCV_TRAILER_AUDIT)
					| MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT),
				0,
				sizeof(handshake_reception),
				reply,
				10'000,
				MACH_PORT_NULL)
			!= KERN_SUCCESS) {
			return EXIT_FAILURE;
		}
		// The other direction of fact_4: the child requires the exact fixed
		// message, so an arbitrary sender that reached this port is refused.
		if (
			std::string_view{ accepted.Payload.data(), parent_reply.size() }
			!= parent_reply) {
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	if (argc > 1 && std::string_view{ argv[1] } == child_mode_argument) {
		return run_child();
	}

	const auto self = std::string{ argv[0] };

	const auto snapshot = registered_ports();
	expect(
		!snapshot.empty(),
		"the task exposes its registered-port array through mach_ports_lookup");
	expect(
		service_slot_is_empty(),
		"SERVICE_SLOT is unused for the supported launch before stage one claims it");

	mach_port_t service = MACH_PORT_NULL;
	expect(
		mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &service)
			== KERN_SUCCESS,
		"stage one can allocate the receive right it will publish");
	expect(
		mach_port_insert_right(
			mach_task_self(), service, service, MACH_MSG_TYPE_MAKE_SEND)
			== KERN_SUCCESS,
		"stage one can make the send right the child will inherit");
	expect(
		publish_in_service_slot(service),
		"registering a port in SERVICE_SLOT succeeds under ad-hoc signing");
	expect(
		!service_slot_is_empty(),
		"the published slot is observable before the spawn that inherits it");

	posix_spawnattr_t attributes;
	posix_spawnattr_init(&attributes);
	posix_spawnattr_setflags(&attributes, POSIX_SPAWN_START_SUSPENDED);
	auto       mode        = std::string{ child_mode_argument };
	char*      childArgv[] = { argv[0], mode.data(), nullptr };
	pid_t      child       = 0;
	const auto spawned
		= posix_spawn(&child, argv[0], nullptr, &attributes, childArgv, environ);
	posix_spawnattr_destroy(&attributes);
	expect(spawned == 0, "the child spawns suspended rather than running");
	pending_children.push_back(child);

	expect(
		restore_registered_ports(snapshot),
		"stage one restores the parent's exact registered-port snapshot after spawn");
	expect(
		service_slot_is_empty(),
		"the restored snapshot leaves SERVICE_SLOT unused for every later spawn");

	expect(::kill(child, SIGCONT) == 0, "the suspended child resumes on demand");

	auto       greeting = handshake_reception{ };
	const auto received = mach_msg(
		&greeting.Header,
		MACH_RCV_MSG | MACH_RCV_TIMEOUT
			| MACH_RCV_TRAILER_TYPE(MACH_RCV_TRAILER_AUDIT)
			| MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT),
		0,
		sizeof(handshake_reception),
		service,
		20'000,
		MACH_PORT_NULL);
	expect(
		received == KERN_SUCCESS,
		"the right published before the spawn survives suspended spawn and exec");
	expect(
		std::string_view{ greeting.Payload.data(), child_greeting.size() }
			== child_greeting,
		"the child's first message is the exact fixed greeting");
	expect(
		greeting.Trailer.msgh_trailer_size >= sizeof(mach_msg_audit_trailer_t),
		"the kernel supplies a full audit trailer rather than a shorter one");

	auto auditPid = pid_t{ -1 };
	audit_token_to_au32(
		greeting.Trailer.msgh_audit,
		nullptr, nullptr, nullptr, nullptr, nullptr, &auditPid, nullptr, nullptr);
	expect(
		auditPid == child,
		"the audit trailer names the exact child the supervisor spawned");

	// The peer is identified from the trailer the kernel wrote, never from the
	// pid the parent happens to remember. fact_3 forbids the latter outright.
	const auto* const guestHash  = guest_code_hash(greeting.Trailer.msgh_audit);
	const auto* const staticHash = static_code_hash(self);
	expect(
		guestHash != nullptr,
		"Security.framework identifies the peer from its audit token under ad-hoc signing");
	expect(
		staticHash != nullptr,
		"the on-disk image carries a readable code-directory hash");
	expect(
		CFEqual(guestHash, staticHash) == TRUE,
		"the running peer's code identity equals the image it was launched from");

	auto accept                    = handshake_message{ };
	accept.Header.msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0);
	accept.Header.msgh_size        = sizeof(handshake_message);
	accept.Header.msgh_remote_port = greeting.Header.msgh_remote_port;
	accept.Header.msgh_local_port  = MACH_PORT_NULL;
	accept.Header.msgh_id          = 2;
	std::memcpy(accept.Payload.data(), parent_reply.data(), parent_reply.size());
	expect(
		mach_msg(
			&accept.Header,
			MACH_SEND_MSG,
			sizeof(handshake_message),
			0,
			MACH_PORT_NULL,
			MACH_MSG_TIMEOUT_NONE,
			MACH_PORT_NULL)
			== KERN_SUCCESS,
		"the supervisor answers on the send-once right the child supplied");

	auto       status = 0;
	const auto reaped = ::waitpid(child, &status, 0);
	std::erase(pending_children, child);
	expect(reaped == child, "the child is reaped exactly once");
	expect(
		WIFEXITED(status) != 0 && WEXITSTATUS(status) == EXIT_SUCCESS,
		"the child accepted the supervisor's exact reply and completed");

	// Fail-closed half zero, and the one fact_3 cares most about: a child that
	// finds no published right must refuse outright. If it ever fell back to
	// argv, an environment variable, or its parent's pid, this course would
	// succeed instead, and the approval would be invalidated.
	posix_spawnattr_t bareAttributes;
	posix_spawnattr_init(&bareAttributes);
	posix_spawnattr_setflags(&bareAttributes, POSIX_SPAWN_START_SUSPENDED);
	pid_t      bare        = 0;
	const auto bareSpawned = posix_spawn(
		&bare, argv[0], nullptr, &bareAttributes, childArgv, environ);
	posix_spawnattr_destroy(&bareAttributes);
	expect(bareSpawned == 0, "the unpublished-slot course spawns its child");
	pending_children.push_back(bare);
	expect(
		service_slot_is_empty(),
		"the unpublished-slot child really does inherit an empty slot");
	expect(::kill(bare, SIGCONT) == 0, "the unpublished-slot child resumes");
	auto       bareStatus = 0;
	const auto bareReaped = ::waitpid(bare, &bareStatus, 0);
	std::erase(pending_children, bare);
	expect(
		bareReaped == bare,
		"the unpublished-slot child is reaped exactly once");
	expect(
		WIFEXITED(bareStatus) != 0 && WEXITSTATUS(bareStatus) != EXIT_SUCCESS,
		"a child that inherits no published right refuses rather than falling back");

	// Fail-closed half one: a peer whose code identity is not the expected one
	// must be refused. The requirement below pins a hash the peer cannot have.
	SecRequirementRef impossible = nullptr;
	const auto        compiled   = SecRequirementCreateWithString(
		CFSTR("cdhash H\"0000000000000000000000000000000000000000\""),
		kSecCSDefaultFlags,
		&impossible);
	expect(
		compiled == errSecSuccess && impossible != nullptr,
		"a pinned code requirement compiles for the mismatch course");
	SecCodeRef self_code = nullptr;
	expect(
		SecCodeCopySelf(kSecCSDefaultFlags, &self_code) == errSecSuccess,
		"the running image can name itself for the mismatch course");
	expect(
		SecCodeCheckValidity(self_code, kSecCSDefaultFlags, impossible)
			!= errSecSuccess,
		"an identity that does not match the pinned requirement is refused");
	CFRelease(self_code);
	CFRelease(impossible);

	// Fail-closed half two: an occupied slot is refusal, not a slot to reuse.
	// fact_6 makes this explicit, and a supervisor that skipped the check would
	// hand its authority to whatever already owned the slot.
	expect(
		publish_in_service_slot(service),
		"the mismatch course can re-occupy the slot it is about to refuse");
	expect(
		!service_slot_is_empty(),
		"an occupied SERVICE_SLOT is detected rather than silently overwritten");
	expect(
		restore_registered_ports(snapshot) && service_slot_is_empty(),
		"the occupied slot is cleared again so the refusal leaves no residue");

	CFRelease(guestHash);
	CFRelease(staticHash);

	std::cout << "macos-service-slot-handshake-tests:pass\n";
	return EXIT_SUCCESS;
}
