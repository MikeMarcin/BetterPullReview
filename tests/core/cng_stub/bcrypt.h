#ifndef BPR_TESTS_CORE_CNG_STUB_BCRYPT_H
#define BPR_TESTS_CORE_CNG_STUB_BCRYPT_H
#include <windows.h>
using BCRYPT_HASH_HANDLE                   = void*;
inline const auto BCRYPT_SHA256_ALG_HANDLE = reinterpret_cast<void*>(0x41U);
auto              BCryptCreateHash(void*, void**, PUCHAR, ULONG, PUCHAR, ULONG, ULONG) -> NTSTATUS;
auto              BCryptDuplicateHash(void*, void**, PUCHAR, ULONG, ULONG) -> NTSTATUS;
auto              BCryptDestroyHash(void*) -> NTSTATUS;
auto              BCryptHashData(void*, PUCHAR, ULONG, ULONG) -> NTSTATUS;
auto              BCryptFinishHash(void*, PUCHAR, ULONG, ULONG) -> NTSTATUS;
#endif
