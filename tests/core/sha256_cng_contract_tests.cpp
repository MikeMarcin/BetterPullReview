#include "bpr/core/sha256.hpp"

#include <bcrypt.h>
extern "C" {
#include <Hacl_Hash_SHA2.h>
}

#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {
	std::string_view failure;
	std::size_t      liveHashes = 0;
	std::size_t      updates    = 0;
	std::size_t      duplicates = 0;

	struct hash_state
	{
		std::vector<std::uint8_t> Bytes;
	};

	void expect(const bool aCondition)
	{
		if (!aCondition) {
			std::exit(EXIT_FAILURE);
		}
	}

	auto bytes(const std::string_view aText) -> std::span<const std::uint8_t>
	{
		return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
	}
}

auto BCryptCreateHash(void* aAlgorithm, void** aResult, PUCHAR aStorage, ULONG aStorageSize, PUCHAR aSecret, ULONG aSecretSize, ULONG aFlags) -> NTSTATUS
{
	expect(aAlgorithm == BCRYPT_SHA256_ALG_HANDLE && aStorage == nullptr && aStorageSize == 0 && aSecret == nullptr && aSecretSize == 0 && aFlags == 0);
	if (failure == "create") {
		return -1;
	}
	*aResult = new hash_state{ };
	++liveHashes;
	return 0;
}

auto BCryptDuplicateHash(void* aSource, void** aResult, PUCHAR aStorage, ULONG aStorageSize, ULONG aFlags) -> NTSTATUS
{
	expect(aStorage == nullptr && aStorageSize == 0 && aFlags == 0);
	if (failure == "duplicate") {
		return -1;
	}
	*aResult = new hash_state{ *static_cast<hash_state*>(aSource) };
	++liveHashes;
	++duplicates;
	return 0;
}

auto BCryptDestroyHash(void* aHash) -> NTSTATUS
{
	expect(aHash != nullptr && liveHashes != 0);
	delete static_cast<hash_state*>(aHash);
	--liveHashes;
	return 0;
}

auto BCryptHashData(void* aHash, PUCHAR aBytes, ULONG aSize, ULONG aFlags) -> NTSTATUS
{
	expect(aHash != nullptr && aSize != 0 && aFlags == 0);
	if (failure == "update") {
		return -1;
	}
	auto& data = static_cast<hash_state*>(aHash)->Bytes;
	data.insert(data.end(), aBytes, aBytes + aSize);
	++updates;
	return 0;
}

auto BCryptFinishHash(void* aHash, PUCHAR aDigest, ULONG aSize, ULONG aFlags) -> NTSTATUS
{
	expect(aHash != nullptr && aSize == 32 && aFlags == 0);
	if (failure == "finish") {
		return -1;
	}
	auto& data = static_cast<hash_state*>(aHash)->Bytes;
	Hacl_Hash_SHA2_hash_256(aDigest, data.data(), static_cast<std::uint32_t>(data.size()));
	return 0;
}

int main(int aCount, char** aArguments)
{
	using bpr::core::sha256;
	using bpr::core::sha256_stream;
	expect(aCount == 2);
	failure = aArguments[1];
	if (failure == "duplicate" || failure == "update" || failure == "finish") {
		std::set_terminate([] { std::cerr << "native SHA-256 failure refused\n"; std::_Exit(EXIT_SUCCESS); });
		auto stream = sha256_stream{ };
		if (failure == "duplicate") {
			auto copy = stream;
			(void)copy;
		} else if (failure == "update") {
			stream.update(bytes("abc"));
		} else {
			(void)stream.finish();
		}
		return EXIT_FAILURE;
	}
	const auto abc = std::array<std::uint8_t, 32>{ 0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad };
	expect(sha256(bytes("abc")) == abc);
	{
		auto stream = sha256_stream{ };
		stream.update(bytes("a"));
		stream.update({ });
		auto copy     = stream;
		auto assigned = sha256_stream{ };
		assigned      = stream;
		copy.update(bytes("bc"));
		assigned.update(bytes("bc"));
		expect(copy.finish() == abc && assigned.finish() == abc);
		stream.update(bytes("different"));
		expect(stream.finish() != abc);
	}
	expect(liveHashes == 0);
	if (failure == "create") {
		expect(updates == 0 && duplicates == 0);
	} else {
		expect(updates != 0 && duplicates == 2);
	}
	std::cout << "CNG contract:pass (stub; not Windows evidence)\n";
}
