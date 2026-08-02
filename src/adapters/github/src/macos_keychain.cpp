#include "macos_keychain.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <map>
#include <mutex>
#include <new>
#include <optional>
#include <utility>

namespace bpr::adapters::github {
	namespace {
		constexpr std::string_view production_service       = "org.betterpullreview.github-native-credential.file-keychain.v2";
		constexpr std::string_view integration_test_service = "org.betterpullreview.github-native-credential.file-keychain."
															  "integration-test.v2";

		template <typename... Callables>
		struct overloaded : Callables...
		{
			using Callables::operator()...;
		};

		template <typename... Callables>
		overloaded(Callables...) -> overloaded<Callables...>;

		template <typename Reference>
		class cf_owner final
		{
		public:
			cf_owner() = default;
			explicit cf_owner(Reference aValue) noexcept
				: Value{ aValue }
			{
			}

			~cf_owner() noexcept
			{
				reset();
			}

			cf_owner(const cf_owner&)                    = delete;
			auto operator=(const cf_owner&) -> cf_owner& = delete;

			cf_owner(cf_owner&& aOther) noexcept
				: Value{ std::exchange(aOther.Value, nullptr) }
			{
			}

			auto operator=(cf_owner&& aOther) noexcept -> cf_owner&
			{
				if (this == &aOther) {
					return *this;
				}

				reset();
				Value = std::exchange(aOther.Value, nullptr);
				return *this;
			}

			[[nodiscard]] auto get() const noexcept -> Reference
			{
				return Value;
			}

			[[nodiscard]] auto release() noexcept -> Reference
			{
				return std::exchange(Value, nullptr);
			}

			void reset(Reference aValue = nullptr) noexcept
			{
				if (Value != nullptr) {
					CFRelease(Value);
				}
				Value = aValue;
			}

		private:
			Reference Value = nullptr;
		};

		[[nodiscard]] constexpr auto hex_digit(const std::uint8_t aValue) noexcept
			-> char
		{
			constexpr std::array<char, 16U> digits{
				'0',
				'1',
				'2',
				'3',
				'4',
				'5',
				'6',
				'7',
				'8',
				'9',
				'a',
				'b',
				'c',
				'd',
				'e',
				'f',
			};
			return digits[aValue];
		}

		void append_hex_u64(
			macos_keychain_authority_account& aOutput,
			std::size_t&                      aOffset,
			const std::uint64_t               aValue) noexcept
		{
			for (std::size_t nibble = 0U; nibble < 16U; ++nibble) {
				const auto shift = static_cast<unsigned int>((15U - nibble) * 4U);
				const auto digit = static_cast<std::uint8_t>(
					(aValue >> shift) & 0xFU);
				aOutput[aOffset] = hex_digit(digit);
				++aOffset;
			}
		}

		void append_hex_u8(
			macos_keychain_authority_account& aOutput,
			std::size_t&                      aOffset,
			const std::uint8_t                aValue) noexcept
		{
			aOutput[aOffset] = hex_digit(static_cast<std::uint8_t>(aValue >> 4U));
			++aOffset;
			aOutput[aOffset] = hex_digit(static_cast<std::uint8_t>(aValue & 0xFU));
			++aOffset;
		}

		[[nodiscard]] constexpr auto server_contract_discriminator(
			const application::github_server_contract aContract) noexcept
			-> std::uint8_t
		{
			switch (aContract) {
			case application::github_server_contract::github_com_2022_11_28:
				return 0x01U;
			case application::github_server_contract::ghes_3_18_2022_11_28:
				return 0x02U;
			}
			return 0x00U;
		}

		[[nodiscard]] auto failure_mapping(
			const application::github_credential_vault_failure aFailure) noexcept
			-> macos_keychain_status_mapping
		{
			return {
				.Kind    = macos_keychain_status_kind::failure,
				.Failure = aFailure,
			};
		}

		[[nodiscard]] constexpr auto normalize_failure(
			const application::github_credential_vault_failure aFailure) noexcept
			-> application::github_credential_vault_failure
		{
			return application::valid(aFailure)
				? aFailure
				: application::github_credential_vault_failure::operation_failed;
		}

		[[nodiscard]] auto mapped_failure(const OSStatus aStatus) noexcept
			-> application::github_credential_vault_failure
		{
			const auto mapping = classify_macos_keychain_status(static_cast<std::int32_t>(aStatus));
			if (mapping.Kind != macos_keychain_status_kind::failure) {
				return application::github_credential_vault_failure::
					operation_failed;
			}
			return normalize_failure(mapping.Failure);
		}

		void add_noninteractive_policy(CFMutableDictionaryRef aDictionary) noexcept
		{
#if defined(__clang__)
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
			CFDictionarySetValue(
				aDictionary,
				kSecUseAuthenticationUI,
				kSecUseAuthenticationUIFail);
#if defined(__clang__)
	#pragma clang diagnostic pop
#elif defined(__GNUC__)
	#pragma GCC diagnostic pop
#endif
		}

		[[nodiscard]] auto make_mutable_dictionary() noexcept
			-> cf_owner<CFMutableDictionaryRef>
		{
			return cf_owner<CFMutableDictionaryRef>{ CFDictionaryCreateMutable(
				kCFAllocatorDefault,
				0,
				&kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks) };
		}

		[[nodiscard]] auto make_cf_string(
			const std::span<const char> aBytes) noexcept -> cf_owner<CFStringRef>
		{
			return cf_owner<CFStringRef>{ CFStringCreateWithBytes(
				kCFAllocatorDefault,
				reinterpret_cast<const UInt8*>(aBytes.data()),
				static_cast<CFIndex>(aBytes.size()),
				kCFStringEncodingASCII,
				false) };
		}

		[[nodiscard]] auto make_cf_string(
			const std::string_view aBytes) noexcept -> cf_owner<CFStringRef>
		{
			return make_cf_string(std::span<const char>{ aBytes });
		}

		[[nodiscard]] auto make_borrowed_data(
			const application::github_credential_material& aCredential) noexcept
			-> cf_owner<CFDataRef>
		{
			const auto bytes = aCredential.bytes();
			return cf_owner<CFDataRef>{ CFDataCreateWithBytesNoCopy(
				kCFAllocatorDefault,
				reinterpret_cast<const UInt8*>(bytes.data()),
				static_cast<CFIndex>(bytes.size()),
				kCFAllocatorNull) };
		}

		class native_macos_keychain_backend final : public macos_keychain_backend
		{
		public:
			native_macos_keychain_backend(
				SecKeychainRef aKeychain,
				CFArrayRef     aSearchList,
				CFStringRef    aService) noexcept
				: Keychain{ aKeychain }
				, SearchList{ aSearchList }
				, Service{ aService }
			{
			}

			~native_macos_keychain_backend() override = default;

			[[nodiscard]] auto store(
				const application::github_credential_authority_key& aAuthority,
				const application::github_credential_material&      aCredential)
				-> macos_keychain_backend_store_result override
			{
				const auto lock = std::scoped_lock{ OperationMutex };
				if (!application::valid(aAuthority) || !application::valid(aCredential)) {
					return std::unexpected{
						application::github_credential_vault_failure::
							invalid_request
					};
				}

				const auto account = account_string(aAuthority);
				if (account.get() == nullptr) {
					return std::unexpected{
						application::github_credential_vault_failure::
							allocation_failure
					};
				}

				const auto value = make_borrowed_data(aCredential);
				if (value.get() == nullptr) {
					return std::unexpected{
						application::github_credential_vault_failure::
							allocation_failure
					};
				}

				auto add = add_dictionary(account.get(), value.get());
				if (add.get() == nullptr) {
					return std::unexpected{
						application::github_credential_vault_failure::
							allocation_failure
					};
				}

				auto status  = SecItemAdd(add.get(), nullptr);
				auto mapping = classify_macos_keychain_status(
					static_cast<std::int32_t>(status));
				if (mapping.Kind == macos_keychain_status_kind::success) {
					return application::github_credential_store_disposition::
						stored;
				}
				if (mapping.Kind != macos_keychain_status_kind::duplicate_item) {
					return std::unexpected{ mapped_failure(status) };
				}

				auto query  = exact_query(account.get());
				auto update = value_dictionary(value.get());
				if (query.get() == nullptr || update.get() == nullptr) {
					return std::unexpected{
						application::github_credential_vault_failure::
							allocation_failure
					};
				}

				status  = SecItemUpdate(query.get(), update.get());
				mapping = classify_macos_keychain_status(
					static_cast<std::int32_t>(status));
				if (mapping.Kind == macos_keychain_status_kind::success) {
					return application::github_credential_store_disposition::
						stored;
				}
				if (mapping.Kind != macos_keychain_status_kind::item_not_found) {
					return std::unexpected{ mapped_failure(status) };
				}

				status  = SecItemAdd(add.get(), nullptr);
				mapping = classify_macos_keychain_status(
					static_cast<std::int32_t>(status));
				if (mapping.Kind == macos_keychain_status_kind::success) {
					return application::github_credential_store_disposition::
						stored;
				}
				return std::unexpected{ mapped_failure(status) };
			}

			[[nodiscard]] auto retrieve(
				const application::github_credential_authority_key& aAuthority)
				-> macos_keychain_backend_retrieve_result override
			{
				const auto lock = std::scoped_lock{ OperationMutex };
				if (!application::valid(aAuthority)) {
					return application::github_credential_vault_failure::
						invalid_request;
				}

				const auto account = account_string(aAuthority);
				if (account.get() == nullptr) {
					return application::github_credential_vault_failure::
						allocation_failure;
				}

				auto query = exact_query(account.get());
				if (query.get() == nullptr) {
					return application::github_credential_vault_failure::
						allocation_failure;
				}
				CFDictionarySetValue(
					query.get(),
					kSecReturnData,
					kCFBooleanTrue);
				CFDictionarySetValue(
					query.get(),
					kSecMatchLimit,
					kSecMatchLimitOne);

				CFTypeRef           rawResult = nullptr;
				const auto          status    = SecItemCopyMatching(query.get(), &rawResult);
				cf_owner<CFTypeRef> result{ rawResult };
				const auto          mapping = classify_macos_keychain_status(
					static_cast<std::int32_t>(status));
				if (mapping.Kind == macos_keychain_status_kind::item_not_found) {
					return macos_keychain_credential_absent{ };
				}
				if (mapping.Kind != macos_keychain_status_kind::success) {
					return mapped_failure(status);
				}
				if (
					result.get() == nullptr
					|| CFGetTypeID(result.get()) != CFDataGetTypeID()) {
					return application::github_credential_vault_failure::corrupt;
				}

				const auto data   = static_cast<CFDataRef>(result.get());
				const auto length = CFDataGetLength(data);
				if (
					length <= 0
					|| static_cast<std::uint64_t>(length)
						> application::maximum_github_credential_bytes) {
					return application::github_credential_vault_failure::corrupt;
				}

				const auto* bytes = CFDataGetBytePtr(data);
				if (bytes == nullptr) {
					return application::github_credential_vault_failure::corrupt;
				}

				auto material = application::github_credential_material::create(
					std::span<const std::byte>{
						reinterpret_cast<const std::byte*>(bytes),
						static_cast<std::size_t>(length) });
				if (!material.has_value()) {
					return material.error()
							== application::github_credential_material_error::
								allocation_failure
						? application::github_credential_vault_failure::
							  allocation_failure
						: application::github_credential_vault_failure::corrupt;
				}
				return std::move(*material);
			}

			[[nodiscard]] auto delete_credential(
				const application::github_credential_authority_key& aAuthority)
				-> macos_keychain_backend_delete_result override
			{
				const auto lock = std::scoped_lock{ OperationMutex };
				if (!application::valid(aAuthority)) {
					return std::unexpected{
						application::github_credential_vault_failure::
							invalid_request
					};
				}

				const auto account = account_string(aAuthority);
				if (account.get() == nullptr) {
					return std::unexpected{
						application::github_credential_vault_failure::
							allocation_failure
					};
				}

				const auto query = exact_query(account.get());
				if (query.get() == nullptr) {
					return std::unexpected{
						application::github_credential_vault_failure::
							allocation_failure
					};
				}

				const auto status  = SecItemDelete(query.get());
				const auto mapping = classify_macos_keychain_status(
					static_cast<std::int32_t>(status));
				if (mapping.Kind == macos_keychain_status_kind::success) {
					return application::github_credential_delete_disposition::
						deleted;
				}
				if (mapping.Kind == macos_keychain_status_kind::item_not_found) {
					return application::github_credential_delete_disposition::
						absent;
				}
				return std::unexpected{ mapped_failure(status) };
			}

		private:
			[[nodiscard]] auto account_string(
				const application::github_credential_authority_key& aAuthority) const
				noexcept -> cf_owner<CFStringRef>
			{
				const auto encoded = encode_macos_keychain_authority_account(aAuthority);
				if (!encoded.has_value()) {
					return { };
				}
				return make_cf_string(std::span<const char>{ *encoded });
			}

			[[nodiscard]] auto add_dictionary(
				CFStringRef aAccount,
				CFDataRef   aValue) const noexcept
				-> cf_owner<CFMutableDictionaryRef>
			{
				auto dictionary = make_mutable_dictionary();
				if (dictionary.get() == nullptr) {
					return { };
				}

				CFDictionarySetValue(
					dictionary.get(),
					kSecClass,
					kSecClassGenericPassword);
				CFDictionarySetValue(
					dictionary.get(),
					kSecAttrService,
					Service.get());
				CFDictionarySetValue(dictionary.get(), kSecAttrAccount, aAccount);
				CFDictionarySetValue(dictionary.get(), kSecValueData, aValue);
				CFDictionarySetValue(
					dictionary.get(),
					kSecUseKeychain,
					Keychain.get());
				add_noninteractive_policy(dictionary.get());
				return dictionary;
			}

			[[nodiscard]] auto exact_query(CFStringRef aAccount) const noexcept
				-> cf_owner<CFMutableDictionaryRef>
			{
				auto dictionary = make_mutable_dictionary();
				if (dictionary.get() == nullptr) {
					return { };
				}

				CFDictionarySetValue(
					dictionary.get(),
					kSecClass,
					kSecClassGenericPassword);
				CFDictionarySetValue(
					dictionary.get(),
					kSecAttrService,
					Service.get());
				CFDictionarySetValue(dictionary.get(), kSecAttrAccount, aAccount);
				CFDictionarySetValue(
					dictionary.get(),
					kSecMatchSearchList,
					SearchList.get());
				add_noninteractive_policy(dictionary.get());
				return dictionary;
			}

			[[nodiscard]] static auto value_dictionary(CFDataRef aValue) noexcept
				-> cf_owner<CFMutableDictionaryRef>
			{
				auto dictionary = make_mutable_dictionary();
				if (dictionary.get() != nullptr) {
					CFDictionarySetValue(
						dictionary.get(),
						kSecValueData,
						aValue);
				}
				return dictionary;
			}

			cf_owner<SecKeychainRef> Keychain;
			cf_owner<CFArrayRef>     SearchList;
			cf_owner<CFStringRef>    Service;
			std::mutex               OperationMutex;
		};

		[[nodiscard]] auto make_native_backend(
			const macos_keychain_service_scope aScope) noexcept
			-> std::expected<
				std::shared_ptr<macos_keychain_backend>,
				application::github_credential_vault_failure>
		{
			if (!valid(aScope)) {
				return std::unexpected{
					application::github_credential_vault_failure::invalid_request
				};
			}

			SecKeychainRef rawKeychain = nullptr;
#if defined(__clang__)
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
			const auto status = SecKeychainCopyDefault(&rawKeychain);
#if defined(__clang__)
	#pragma clang diagnostic pop
#elif defined(__GNUC__)
	#pragma GCC diagnostic pop
#endif
			cf_owner<SecKeychainRef> keychain{ rawKeychain };
			if (status != errSecSuccess) {
				return std::unexpected{ mapped_failure(status) };
			}
			if (keychain.get() == nullptr) {
				return std::unexpected{
					application::github_credential_vault_failure::corrupt
				};
			}

			const void*          values[]{ keychain.get() };
			cf_owner<CFArrayRef> searchList{ CFArrayCreate(
				kCFAllocatorDefault,
				values,
				1,
				&kCFTypeArrayCallBacks) };
			if (searchList.get() == nullptr) {
				return std::unexpected{
					application::github_credential_vault_failure::
						allocation_failure
				};
			}

			const auto serviceIdentifier = macos_keychain_service_identifier(aScope);
			auto       service           = make_cf_string(serviceIdentifier);
			if (service.get() == nullptr) {
				return std::unexpected{
					application::github_credential_vault_failure::
						allocation_failure
				};
			}

			auto* backend = new (std::nothrow) native_macos_keychain_backend{
				keychain.release(),
				searchList.release(),
				service.release()
			};
			if (backend == nullptr) {
				return std::unexpected{
					application::github_credential_vault_failure::
						allocation_failure
				};
			}

			try {
				return std::shared_ptr<macos_keychain_backend>{ backend };
			} catch (const std::bad_alloc&) {
				delete backend;
				return std::unexpected{
					application::github_credential_vault_failure::
						allocation_failure
				};
			} catch (...) {
				delete backend;
				return std::unexpected{
					application::github_credential_vault_failure::
						operation_failed
				};
			}
		}

		struct pending_store
		{
			application::github_credential_enrollment               Enrollment;
			application::github_credential_store_completion_handler Completion;
		};

		struct pending_retrieve
		{
			application::github_credential_operation_context           Context;
			application::github_credential_retrieve_completion_handler Completion;
		};

		struct pending_delete
		{
			application::github_credential_operation_context         Context;
			application::github_credential_delete_completion_handler Completion;
		};

		using pending_operation  = std::variant<pending_store, pending_retrieve, pending_delete>;
		using pending_operations = std::map<
			application::github_credential_operation_context,
			pending_operation>;

		[[nodiscard]] auto extract_pending(
			pending_operations&                                     aOperations,
			const application::github_credential_operation_context& aContext)
			-> std::optional<pending_operation>
		{
			auto node = aOperations.extract(aContext);
			if (node.empty()) {
				return std::nullopt;
			}
			return std::move(node.mapped());
		}

		void publish_store(
			pending_store&                                  aPending,
			application::github_credential_store_completion aTerminal)
		{
			aPending.Enrollment.Credential.reset();
			auto completion = std::move(aPending.Completion);
			if (completion) {
				completion(std::move(aTerminal));
			}
		}

		void publish_retrieve(
			pending_retrieve&                                  aPending,
			application::github_credential_retrieve_completion aTerminal)
		{
			auto completion = std::move(aPending.Completion);
			if (completion) {
				completion(std::move(aTerminal));
			}
		}

		void publish_delete(
			pending_delete&                                  aPending,
			application::github_credential_delete_completion aTerminal)
		{
			auto completion = std::move(aPending.Completion);
			if (completion) {
				completion(std::move(aTerminal));
			}
		}

		void publish_failure(
			pending_operation&                                 aOperation,
			const application::github_credential_vault_failure aFailure)
		{
			const auto effectiveFailure = normalize_failure(aFailure);
			std::visit(
				overloaded{
					[effectiveFailure](pending_store& aPending) {
						publish_store(
							aPending,
							application::github_credential_store_error{
								.Context = aPending.Enrollment.Context,
								.Failure = effectiveFailure,
							});
					},
					[effectiveFailure](pending_retrieve& aPending) {
						publish_retrieve(
							aPending,
							application::github_credential_retrieve_error{
								.Context = aPending.Context,
								.Failure = effectiveFailure,
							});
					},
					[effectiveFailure](pending_delete& aPending) {
						publish_delete(
							aPending,
							application::github_credential_delete_error{
								.Context = aPending.Context,
								.Failure = effectiveFailure,
							});
					},
				},
				aOperation);
		}

		void publish_cancelled(pending_operation& aOperation)
		{
			std::visit(
				overloaded{
					[](pending_store& aPending) {
						publish_store(
							aPending,
							application::github_credential_store_cancelled{
								.Context = aPending.Enrollment.Context,
							});
					},
					[](pending_retrieve& aPending) {
						publish_retrieve(
							aPending,
							application::github_credential_retrieve_cancelled{
								.Context = aPending.Context,
							});
					},
					[](pending_delete& aPending) {
						publish_delete(
							aPending,
							application::github_credential_delete_cancelled{
								.Context = aPending.Context,
							});
					},
				},
				aOperation);
		}

		class macos_keychain_vault final
			: public application::github_credential_vault_port
		{
		private:
			struct shared_state
			{
				explicit shared_state(
					std::shared_ptr<macos_keychain_backend> aBackend) noexcept
					: Backend{ std::move(aBackend) }
				{
				}

				std::mutex                              Mutex;
				pending_operations                      Pending;
				std::shared_ptr<macos_keychain_backend> Backend;
				bool                                    Closing = false;
			};

		public:
			macos_keychain_vault(
				application::effect_executor&           aEffectLane,
				std::shared_ptr<macos_keychain_backend> aBackend)
				: EffectLane{ aEffectLane }
				, State{ std::make_shared<shared_state>(std::move(aBackend)) }
			{
			}

			~macos_keychain_vault() override
			{
				pending_operations cancelled;
				{
					const auto lock = std::scoped_lock{ State->Mutex };
					State->Closing  = true;
					cancelled.swap(State->Pending);
				}

				while (!cancelled.empty()) {
					auto node = cancelled.extract(cancelled.begin());
					try {
						publish_cancelled(node.mapped());
					} catch (...) {
					}
				}
			}

			void store(
				application::github_credential_enrollment               aEnrollment,
				application::github_credential_store_completion_handler aCompletion)
				override
			{
				const auto context = aEnrollment.Context;
				if (!application::valid(aEnrollment) || !aCompletion) {
					aEnrollment.Credential.reset();
					if (aCompletion) {
						aCompletion(application::github_credential_store_error{
							.Context = context,
							.Failure = application::
								github_credential_vault_failure::invalid_request,
						});
					}
					return;
				}

				queue(
					context,
					pending_store{
						.Enrollment = std::move(aEnrollment),
						.Completion = std::move(aCompletion),
					});
			}

			void retrieve(
				application::github_credential_operation_context           aContext,
				application::github_credential_retrieve_completion_handler aCompletion)
				override
			{
				if (!application::valid(aContext) || !aCompletion) {
					if (aCompletion) {
						aCompletion(application::github_credential_retrieve_error{
							.Context = aContext,
							.Failure = application::
								github_credential_vault_failure::invalid_request,
						});
					}
					return;
				}

				queue(
					aContext,
					pending_retrieve{
						.Context    = aContext,
						.Completion = std::move(aCompletion),
					});
			}

			void delete_credential(
				application::github_credential_operation_context         aContext,
				application::github_credential_delete_completion_handler aCompletion)
				override
			{
				if (!application::valid(aContext) || !aCompletion) {
					if (aCompletion) {
						aCompletion(application::github_credential_delete_error{
							.Context = aContext,
							.Failure = application::
								github_credential_vault_failure::invalid_request,
						});
					}
					return;
				}

				queue(
					aContext,
					pending_delete{
						.Context    = aContext,
						.Completion = std::move(aCompletion),
					});
			}

			void cancel(
				application::github_credential_operation_context aContext) override
			{
				std::optional<pending_operation> pending;
				{
					const auto lock = std::scoped_lock{ State->Mutex };
					pending         = extract_pending(State->Pending, aContext);
				}
				if (!pending.has_value()) {
					return;
				}
				publish_cancelled(*pending);
			}

		private:
			void queue(
				const application::github_credential_operation_context& aContext,
				pending_operation                                       aOperation)
			{
				auto rejected = false;
				auto closing  = false;
				try {
					const auto lock = std::scoped_lock{ State->Mutex };
					closing         = State->Closing;
					if (!closing) {
						rejected = !State->Pending.try_emplace(
													  aContext,
													  std::move(aOperation))
										.second;
					}
				} catch (const std::bad_alloc&) {
					publish_failure(
						aOperation,
						application::github_credential_vault_failure::
							allocation_failure);
					return;
				} catch (...) {
					publish_failure(
						aOperation,
						application::github_credential_vault_failure::
							operation_failed);
					return;
				}

				if (closing) {
					publish_failure(
						aOperation,
						application::github_credential_vault_failure::unavailable);
					return;
				}
				if (rejected) {
					publish_failure(
						aOperation,
						application::github_credential_vault_failure::
							invalid_request);
					return;
				}

				const auto weakState = std::weak_ptr<shared_state>{ State };
				try {
					EffectLane.post([weakState, aContext] {
						const auto state = weakState.lock();
						if (state != nullptr) {
							execute(state, aContext);
						}
					});
				} catch (const std::bad_alloc&) {
					fail_queued(
						aContext,
						application::github_credential_vault_failure::
							allocation_failure);
				} catch (...) {
					fail_queued(
						aContext,
						application::github_credential_vault_failure::
							operation_failed);
				}
			}

			void fail_queued(
				const application::github_credential_operation_context& aContext,
				const application::github_credential_vault_failure      aFailure)
			{
				std::optional<pending_operation> pending;
				{
					const auto lock = std::scoped_lock{ State->Mutex };
					pending         = extract_pending(State->Pending, aContext);
				}
				if (!pending.has_value()) {
					return;
				}
				publish_failure(*pending, aFailure);
			}

			static void execute(
				const std::shared_ptr<shared_state>&                    aState,
				const application::github_credential_operation_context& aContext)
			{
				std::optional<pending_operation>        pending;
				std::shared_ptr<macos_keychain_backend> backend;
				{
					const auto lock = std::scoped_lock{ aState->Mutex };
					pending         = extract_pending(aState->Pending, aContext);
					if (pending.has_value()) {
						backend = aState->Backend;
					}
				}
				if (!pending.has_value()) {
					return;
				}

				std::visit(
					overloaded{
						[&backend](pending_store& aOperation) {
							execute_store(backend, aOperation);
						},
						[&backend](pending_retrieve& aOperation) {
							execute_retrieve(backend, aOperation);
						},
						[&backend](pending_delete& aOperation) {
							execute_delete(backend, aOperation);
						},
					},
					*pending);
			}

			static void execute_store(
				const std::shared_ptr<macos_keychain_backend>& aBackend,
				pending_store&                                 aPending)
			{
				try {
					const auto result = aBackend->store(
						aPending.Enrollment.Context.Authority,
						aPending.Enrollment.Credential);
					if (result.has_value() && application::valid(*result)) {
						publish_store(
							aPending,
							application::github_credential_store_success{
								.Context     = aPending.Enrollment.Context,
								.Disposition = *result,
							});
						return;
					}

					const auto failure = result.has_value()
						? application::github_credential_vault_failure::
							  operation_failed
						: result.error();
					publish_store(
						aPending,
						application::github_credential_store_error{
							.Context = aPending.Enrollment.Context,
							.Failure = normalize_failure(failure),
						});
				} catch (const std::bad_alloc&) {
					publish_store(
						aPending,
						application::github_credential_store_error{
							.Context = aPending.Enrollment.Context,
							.Failure = application::
								github_credential_vault_failure::allocation_failure,
						});
				} catch (...) {
					publish_store(
						aPending,
						application::github_credential_store_error{
							.Context = aPending.Enrollment.Context,
							.Failure = application::
								github_credential_vault_failure::operation_failed,
						});
				}
			}

			static void execute_retrieve(
				const std::shared_ptr<macos_keychain_backend>& aBackend,
				pending_retrieve&                              aPending)
			{
				try {
					auto result = aBackend->retrieve(aPending.Context.Authority);
					std::visit(
						overloaded{
							[&aPending](
								application::github_credential_material& aCredential) {
								if (!application::valid(aCredential)) {
									aCredential.reset();
									publish_retrieve(
										aPending,
										application::github_credential_retrieve_error{
											.Context = aPending.Context,
											.Failure = application::
												github_credential_vault_failure::
													corrupt,
										});
									return;
								}
								publish_retrieve(
									aPending,
									application::github_credential_retrieve_success{
										.Context    = aPending.Context,
										.Credential = std::move(aCredential),
									});
							},
							[&aPending](macos_keychain_credential_absent) {
								publish_retrieve(
									aPending,
									application::github_credential_retrieve_absent{
										.Context = aPending.Context,
									});
							},
							[&aPending](
								const application::github_credential_vault_failure
									aFailure) {
								publish_retrieve(
									aPending,
									application::github_credential_retrieve_error{
										.Context = aPending.Context,
										.Failure = normalize_failure(aFailure),
									});
							},
						},
						result);
				} catch (const std::bad_alloc&) {
					publish_retrieve(
						aPending,
						application::github_credential_retrieve_error{
							.Context = aPending.Context,
							.Failure = application::
								github_credential_vault_failure::allocation_failure,
						});
				} catch (...) {
					publish_retrieve(
						aPending,
						application::github_credential_retrieve_error{
							.Context = aPending.Context,
							.Failure = application::
								github_credential_vault_failure::operation_failed,
						});
				}
			}

			static void execute_delete(
				const std::shared_ptr<macos_keychain_backend>& aBackend,
				pending_delete&                                aPending)
			{
				try {
					const auto result = aBackend->delete_credential(aPending.Context.Authority);
					if (result.has_value() && application::valid(*result)) {
						publish_delete(
							aPending,
							application::github_credential_delete_success{
								.Context     = aPending.Context,
								.Disposition = *result,
							});
						return;
					}

					const auto failure = result.has_value()
						? application::github_credential_vault_failure::
							  operation_failed
						: result.error();
					publish_delete(
						aPending,
						application::github_credential_delete_error{
							.Context = aPending.Context,
							.Failure = normalize_failure(failure),
						});
				} catch (const std::bad_alloc&) {
					publish_delete(
						aPending,
						application::github_credential_delete_error{
							.Context = aPending.Context,
							.Failure = application::
								github_credential_vault_failure::allocation_failure,
						});
				} catch (...) {
					publish_delete(
						aPending,
						application::github_credential_delete_error{
							.Context = aPending.Context,
							.Failure = application::
								github_credential_vault_failure::operation_failed,
						});
				}
			}

			application::effect_executor& EffectLane;
			std::shared_ptr<shared_state> State;
		};

		[[nodiscard]] auto make_vault(
			application::effect_executor&           aEffectLane,
			std::shared_ptr<macos_keychain_backend> aBackend) noexcept
			-> macos_keychain_vault_factory_result
		{
			if (aBackend == nullptr) {
				return std::unexpected{
					application::github_credential_vault_failure::invalid_request
				};
			}

			try {
				return std::unique_ptr<application::github_credential_vault_port>{
					std::make_unique<macos_keychain_vault>(
						aEffectLane,
						std::move(aBackend))
				};
			} catch (const std::bad_alloc&) {
				return std::unexpected{
					application::github_credential_vault_failure::
						allocation_failure
				};
			} catch (...) {
				return std::unexpected{
					application::github_credential_vault_failure::
						operation_failed
				};
			}
		}
	}

	auto macos_keychain_service_identifier(
		const macos_keychain_service_scope aScope) noexcept -> std::string_view
	{
		switch (aScope) {
		case macos_keychain_service_scope::production:
			return production_service;
		case macos_keychain_service_scope::integration_test:
			return integration_test_service;
		}
		return { };
	}

	auto encode_macos_keychain_authority_account(
		const application::github_credential_authority_key& aAuthority) noexcept
		-> std::expected<
			macos_keychain_authority_account,
			application::github_credential_vault_failure>
	{
		if (!application::valid(aAuthority)) {
			return std::unexpected{
				application::github_credential_vault_failure::invalid_request
			};
		}

		macos_keychain_authority_account encoded{ };
		std::size_t                      offset = 0U;
		append_hex_u64(encoded, offset, aAuthority.Profile.value());
		append_hex_u64(encoded, offset, aAuthority.ProfileRevision.value());
		append_hex_u64(encoded, offset, aAuthority.Host.value());
		append_hex_u64(encoded, offset, aAuthority.Account.value());
		append_hex_u8(
			encoded,
			offset,
			server_contract_discriminator(aAuthority.ServerContract));
		const auto hostname = aAuthority.Hostname.value();
		append_hex_u8(
			encoded, offset, static_cast<std::uint8_t>(hostname.size()));
		for (std::size_t index = 0U;
			 index < application::maximum_github_hostname_bytes;
			 ++index) {
			const std::uint8_t value = index < hostname.size()
				? static_cast<std::uint8_t>(
					  static_cast<unsigned char>(hostname[index]))
				: std::uint8_t{ 0U };
			append_hex_u8(encoded, offset, value);
		}
		return encoded;
	}

	auto classify_macos_keychain_status(const std::int32_t aStatus) noexcept
		-> macos_keychain_status_mapping
	{
		switch (aStatus) {
		case errSecSuccess:
			return {
				.Kind    = macos_keychain_status_kind::success,
				.Failure = application::github_credential_vault_failure::none,
			};
		case errSecItemNotFound:
			return {
				.Kind    = macos_keychain_status_kind::item_not_found,
				.Failure = application::github_credential_vault_failure::none,
			};
		case errSecDuplicateItem:
			return {
				.Kind    = macos_keychain_status_kind::duplicate_item,
				.Failure = application::github_credential_vault_failure::none,
			};
		case errSecNotAvailable:
		case errSecNoDefaultKeychain:
		case errSecNoSuchKeychain:
		case errSecNoStorageModule:
			return failure_mapping(
				application::github_credential_vault_failure::unavailable);
		case errSecInteractionNotAllowed:
		case errSecInteractionRequired:
		case errSecInDarkWake:
			return failure_mapping(
				application::github_credential_vault_failure::locked);
		case errSecAuthFailed:
		case errSecUserCanceled:
		case errSecWrPerm:
		case errSecReadOnly:
		case errSecReadOnlyAttr:
		case errSecMissingEntitlement:
		case errSecRestrictedAPI:
		case errSecNoAccessForItem:
		case errSecInvalidOwnerEdit:
		case errSecDataNotModifiable:
			return failure_mapping(
				application::github_credential_vault_failure::access_denied);
		case errSecDecode:
		case errSecInvalidKeychain:
		case errSecDataNotAvailable:
			return failure_mapping(
				application::github_credential_vault_failure::corrupt);
		case errSecAllocate:
			return failure_mapping(
				application::github_credential_vault_failure::
					allocation_failure);
		case errSecParam:
		case errSecBadReq:
			return failure_mapping(
				application::github_credential_vault_failure::
					invalid_request);
		default:
			return failure_mapping(
				application::github_credential_vault_failure::
					operation_failed);
		}
	}

	auto make_macos_keychain_vault(
		application::effect_executor& aEffectLane)
		-> macos_keychain_vault_factory_result
	{
		auto backend = make_native_backend(macos_keychain_service_scope::production);
		if (!backend.has_value()) {
			return std::unexpected{ backend.error() };
		}
		return make_vault(aEffectLane, std::move(*backend));
	}

	auto make_macos_keychain_integration_test_vault(
		application::effect_executor& aEffectLane)
		-> macos_keychain_vault_factory_result
	{
		auto backend = make_native_backend(macos_keychain_service_scope::integration_test);
		if (!backend.has_value()) {
			return std::unexpected{ backend.error() };
		}
		return make_vault(aEffectLane, std::move(*backend));
	}

	auto make_macos_keychain_vault_with_backend(
		application::effect_executor&           aEffectLane,
		std::shared_ptr<macos_keychain_backend> aBackend) noexcept
		-> macos_keychain_vault_factory_result
	{
		return make_vault(aEffectLane, std::move(aBackend));
	}
}
