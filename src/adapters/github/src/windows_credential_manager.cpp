#include "windows_credential_manager.hpp"

#if !defined(NOMINMAX)
	#define NOMINMAX
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
	#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <wincred.h>

#include <map>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <utility>

namespace bpr::adapters::github {
	namespace {
		constexpr std::wstring_view production_target_prefix       = L"org.betterpullreview.github-native-credential."
																	 L"windows-credential-manager.v2/";
		constexpr std::wstring_view integration_test_target_prefix = L"org.betterpullreview.github-native-credential."
																	 L"windows-credential-manager.integration-test.v2/";
		// BPR-0543. The refresh slot's own targets, distinct from the base
		// credential's in the prefix rather than the account, so both slots key
		// the same authority and neither can answer the other's query.
		constexpr std::wstring_view production_refresh_target_prefix       = L"org.betterpullreview.github-native-refresh."
																			 L"windows-credential-manager.v1/";
		constexpr std::wstring_view integration_test_refresh_target_prefix = L"org.betterpullreview.github-native-refresh."
																			 L"windows-credential-manager.integration-test.v1/";

		// Microsoft documents a 5 * 512-byte generic credential blob limit for the
		// supported Windows 10/11 surface. Some MinGW headers retain the legacy
		// 512-byte macro, so the adapter owns the supported-platform contract rather
		// than silently shrinking the portable vault contract by toolchain.
		constexpr std::size_t windows_credential_manager_maximum_blob_bytes = 5U * 512U;

		static_assert(
			production_target_prefix.size()
				+ windows_credential_manager_authority_account_chars
			== 650U);
		static_assert(
			integration_test_target_prefix.size()
				+ windows_credential_manager_authority_account_chars
			== 667U);
		static_assert(
			integration_test_target_prefix.size()
				+ windows_credential_manager_authority_account_chars
			<= CRED_MAX_GENERIC_TARGET_NAME_LENGTH);
		static_assert(
			production_refresh_target_prefix.size()
				+ windows_credential_manager_authority_account_chars
			== 647U);
		static_assert(
			integration_test_refresh_target_prefix.size()
				+ windows_credential_manager_authority_account_chars
			== 664U);
		static_assert(
			integration_test_refresh_target_prefix.size()
				+ windows_credential_manager_authority_account_chars
			<= CRED_MAX_GENERIC_TARGET_NAME_LENGTH);
		// Neither refresh prefix may be a prefix of a base one, or a target
		// built for one slot could name an entry in the other.
		static_assert(
			!production_refresh_target_prefix.starts_with(
				production_target_prefix)
			&& !production_target_prefix.starts_with(
				production_refresh_target_prefix));
		static_assert(
			!integration_test_refresh_target_prefix.starts_with(
				integration_test_target_prefix)
			&& !integration_test_target_prefix.starts_with(
				integration_test_refresh_target_prefix));
		static_assert(
			application::maximum_github_credential_bytes
			== windows_credential_manager_maximum_blob_bytes);

		template <typename... Callables>
		struct overloaded : Callables...
		{
			using Callables::operator()...;
		};

		template <typename... Callables>
		overloaded(Callables...) -> overloaded<Callables...>;

		[[nodiscard]] constexpr auto hex_digit(const std::uint8_t value) noexcept
			-> wchar_t
		{
			constexpr std::array<wchar_t, 16U> digits{
				L'0',
				L'1',
				L'2',
				L'3',
				L'4',
				L'5',
				L'6',
				L'7',
				L'8',
				L'9',
				L'a',
				L'b',
				L'c',
				L'd',
				L'e',
				L'f',
			};
			return digits[value];
		}

		void append_hex_u64(
			windows_credential_manager_authority_account& output,
			std::size_t&                                  offset,
			const std::uint64_t                           value) noexcept
		{
			for (std::size_t nibble = 0U; nibble < 16U; ++nibble) {
				const auto shift = static_cast<unsigned int>((15U - nibble) * 4U);
				const auto digit = static_cast<std::uint8_t>(
					(value >> shift) & 0xFU);
				output[offset] = hex_digit(digit);
				++offset;
			}
		}

		void append_hex_u8(
			windows_credential_manager_authority_account& output,
			std::size_t&                                  offset,
			const std::uint8_t                            value) noexcept
		{
			output[offset] = hex_digit(static_cast<std::uint8_t>(value >> 4U));
			++offset;
			output[offset] = hex_digit(static_cast<std::uint8_t>(value & 0xFU));
			++offset;
		}

		[[nodiscard]] constexpr auto server_contract_discriminator(
			const application::github_server_contract contract) noexcept
			-> std::uint8_t
		{
			switch (contract) {
			case application::github_server_contract::github_com_2022_11_28:
				return 0x01U;
			case application::github_server_contract::ghes_3_18_2022_11_28:
				return 0x02U;
			}
			return 0x00U;
		}

		[[nodiscard]] auto failure_mapping(
			const application::github_credential_vault_failure failure) noexcept
			-> windows_credential_manager_status_mapping
		{
			return {
				.Kind    = windows_credential_manager_status_kind::failure,
				.Failure = failure,
			};
		}

		[[nodiscard]] constexpr auto normalize_failure(
			const application::github_credential_vault_failure failure) noexcept
			-> application::github_credential_vault_failure
		{
			return application::valid(failure)
				? failure
				: application::github_credential_vault_failure::operation_failed;
		}

		[[nodiscard]] auto mapped_failure(
			const windows_credential_manager_status_mapping& mapping) noexcept
			-> application::github_credential_vault_failure
		{
			if (mapping.Kind != windows_credential_manager_status_kind::failure) {
				return application::github_credential_vault_failure::
					operation_failed;
			}
			return normalize_failure(mapping.Failure);
		}

		[[nodiscard]] auto mapped_failure(const DWORD status) noexcept
			-> application::github_credential_vault_failure
		{
			return mapped_failure(classify_windows_credential_manager_status(
				static_cast<std::uint32_t>(status)));
		}

		[[nodiscard]] auto make_target_name(
			const windows_credential_manager_target_scope       scope,
			const application::github_credential_authority_key& authority) noexcept
			-> std::expected<
				std::wstring,
				application::github_credential_vault_failure>
		{
			if (!valid(scope)) {
				return std::unexpected{
					application::github_credential_vault_failure::invalid_request
				};
			}

			const auto encoded = encode_windows_credential_manager_authority_account(authority);
			if (!encoded.has_value()) {
				return std::unexpected{ encoded.error() };
			}

			const auto prefix = windows_credential_manager_target_prefix(scope);
			try {
				std::wstring target;
				target.reserve(prefix.size() + encoded->size());
				target.append(prefix);
				target.append(encoded->data(), encoded->size());
				return target;
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

		[[nodiscard]] constexpr auto valid_credential_blob_extent(
			const CREDENTIALW* const credential) noexcept -> bool
		{
			return credential != nullptr
				&& credential->CredentialBlob != nullptr
				&& credential->CredentialBlobSize != 0U
				&& credential->CredentialBlobSize
				<= windows_credential_manager_maximum_blob_bytes;
		}

		class credential_owner final
		{
		public:
			credential_owner() = default;
			explicit credential_owner(PCREDENTIALW credential) noexcept
				: Credential{ credential }
			{
			}

			~credential_owner() noexcept
			{
				reset();
			}

			credential_owner(const credential_owner&)                    = delete;
			auto operator=(const credential_owner&) -> credential_owner& = delete;
			credential_owner(credential_owner&&)                         = delete;
			auto operator=(credential_owner&&) -> credential_owner&      = delete;

			[[nodiscard]] auto get() const noexcept -> PCREDENTIALW
			{
				return Credential;
			}

			void reset() noexcept
			{
				if (Credential == nullptr) {
					return;
				}

				if (valid_credential_blob_extent(Credential)) {
					SecureZeroMemory(
						Credential->CredentialBlob,
						Credential->CredentialBlobSize);
				}
				CredFree(Credential);
				Credential = nullptr;
			}

		private:
			PCREDENTIALW Credential = nullptr;
		};

		class native_windows_credential_manager_backend final
			: public windows_credential_manager_backend
		{
		public:
			explicit native_windows_credential_manager_backend(
				const windows_credential_manager_target_scope scope) noexcept
				: Scope{ scope }
			{
			}

			~native_windows_credential_manager_backend() override = default;

			[[nodiscard]] auto store(
				const application::github_credential_authority_key& authority,
				const application::github_credential_material&      credential)
				-> windows_credential_manager_backend_store_result override
			{
				const auto lock = std::scoped_lock{ OperationMutex };
				if (!application::valid(authority) || !application::valid(credential)) {
					return std::unexpected{
						application::github_credential_vault_failure::
							invalid_request
					};
				}

				const auto target = make_target_name(Scope, authority);
				if (!target.has_value()) {
					return std::unexpected{ target.error() };
				}

				const auto  bytes = credential.bytes();
				CREDENTIALW native{ };
				native.Flags              = 0U;
				native.Type               = CRED_TYPE_GENERIC;
				native.TargetName         = const_cast<LPWSTR>(target->c_str());
				native.Comment            = nullptr;
				native.CredentialBlobSize = static_cast<DWORD>(bytes.size());
				native.CredentialBlob     = reinterpret_cast<LPBYTE>(
					const_cast<std::byte*>(bytes.data()));
				native.Persist        = CRED_PERSIST_LOCAL_MACHINE;
				native.AttributeCount = 0U;
				native.Attributes     = nullptr;
				native.TargetAlias    = nullptr;
				native.UserName       = nullptr;

				const auto succeeded = CredWriteW(&native, 0U) != FALSE;
				const auto status    = succeeded ? ERROR_SUCCESS : GetLastError();
				SecureZeroMemory(&native, sizeof(native));
				if (succeeded) {
					return application::github_credential_store_disposition::
						stored;
				}
				return std::unexpected{ mapped_failure(status) };
			}

			[[nodiscard]] auto retrieve(
				const application::github_credential_authority_key& authority)
				-> windows_credential_manager_backend_retrieve_result override
			{
				const auto lock = std::scoped_lock{ OperationMutex };
				if (!application::valid(authority)) {
					return application::github_credential_vault_failure::
						invalid_request;
				}

				const auto target = make_target_name(Scope, authority);
				if (!target.has_value()) {
					return target.error();
				}

				PCREDENTIALW raw_credential = nullptr;
				const auto   succeeded      = CredReadW(
												  target->c_str(),
												  CRED_TYPE_GENERIC,
												  0U,
												  &raw_credential)
					!= FALSE;
				const auto       status = succeeded ? ERROR_SUCCESS : GetLastError();
				credential_owner result{ raw_credential };

				if (!succeeded) {
					const auto mapping = classify_windows_credential_manager_status(
						static_cast<std::uint32_t>(status));
					if (
						mapping.Kind
						== windows_credential_manager_status_kind::item_not_found) {
						return windows_credential_manager_credential_absent{ };
					}
					return mapped_failure(mapping);
				}

				if (
					result.get() == nullptr
					|| result.get()->Type != CRED_TYPE_GENERIC
					|| result.get()->Persist != CRED_PERSIST_LOCAL_MACHINE
					|| !valid_credential_blob_extent(result.get())) {
					return application::github_credential_vault_failure::corrupt;
				}

				auto material = application::github_credential_material::create(
					std::span<const std::byte>{
						reinterpret_cast<const std::byte*>(
							result.get()->CredentialBlob),
						static_cast<std::size_t>(
							result.get()->CredentialBlobSize) });
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
				const application::github_credential_authority_key& authority)
				-> windows_credential_manager_backend_delete_result override
			{
				const auto lock = std::scoped_lock{ OperationMutex };
				if (!application::valid(authority)) {
					return std::unexpected{
						application::github_credential_vault_failure::
							invalid_request
					};
				}

				const auto target = make_target_name(Scope, authority);
				if (!target.has_value()) {
					return std::unexpected{ target.error() };
				}

				const auto succeeded = CredDeleteW(
										   target->c_str(),
										   CRED_TYPE_GENERIC,
										   0U)
					!= FALSE;
				const auto status = succeeded ? ERROR_SUCCESS : GetLastError();
				if (succeeded) {
					return application::github_credential_delete_disposition::
						deleted;
				}

				const auto mapping = classify_windows_credential_manager_status(
					static_cast<std::uint32_t>(status));
				if (
					mapping.Kind
					== windows_credential_manager_status_kind::item_not_found) {
					return application::github_credential_delete_disposition::
						absent;
				}
				return std::unexpected{ mapped_failure(mapping) };
			}

		private:
			windows_credential_manager_target_scope Scope;
			std::mutex                              OperationMutex;
		};

		[[nodiscard]] auto make_native_backend(
			const windows_credential_manager_target_scope scope) noexcept
			-> std::expected<
				std::shared_ptr<windows_credential_manager_backend>,
				application::github_credential_vault_failure>
		{
			if (!valid(scope)) {
				return std::unexpected{
					application::github_credential_vault_failure::invalid_request
				};
			}

			try {
				return std::shared_ptr<windows_credential_manager_backend>{
					std::make_shared<native_windows_credential_manager_backend>(
						scope)
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

		// BPR-0543. Which of the two Credential Manager targets an operation
		// belongs to. Carried on the queued work rather than read from the
		// context, so a queued operation cannot be re-routed after the fact.
		enum class credential_slot : std::uint8_t {
			credential,
			refresh,
		};

		struct pending_store
		{
			application::github_credential_enrollment               Enrollment;
			application::github_credential_store_completion_handler Completion;
			credential_slot                                         Slot = credential_slot::credential;
		};

		struct pending_retrieve
		{
			application::github_credential_operation_context           Context;
			application::github_credential_retrieve_completion_handler Completion;
			credential_slot                                            Slot = credential_slot::credential;
		};

		struct pending_delete
		{
			application::github_credential_operation_context         Context;
			application::github_credential_delete_completion_handler Completion;
			credential_slot                                          Slot = credential_slot::credential;
		};

		using pending_operation  = std::variant<pending_store, pending_retrieve, pending_delete>;
		using pending_operations = std::map<
			application::github_credential_operation_context,
			pending_operation>;

		[[nodiscard]] auto slot_of(const pending_operation& aOperation) noexcept
			-> credential_slot
		{
			return std::visit(
				[](const auto& aPending) noexcept {
					return aPending.Slot;
				},
				aOperation);
		}

		[[nodiscard]] auto extract_pending(
			pending_operations&                                     operations,
			const application::github_credential_operation_context& context)
			-> std::optional<pending_operation>
		{
			auto node = operations.extract(context);
			if (node.empty()) {
				return std::nullopt;
			}
			return std::move(node.mapped());
		}

		void publish_store(
			pending_store&                                  pending,
			application::github_credential_store_completion terminal)
		{
			pending.Enrollment.Credential.reset();
			auto completion = std::move(pending.Completion);
			if (completion) {
				completion(std::move(terminal));
			}
		}

		void publish_retrieve(
			pending_retrieve&                                  pending,
			application::github_credential_retrieve_completion terminal)
		{
			auto completion = std::move(pending.Completion);
			if (completion) {
				completion(std::move(terminal));
			}
		}

		void publish_delete(
			pending_delete&                                  pending,
			application::github_credential_delete_completion terminal)
		{
			auto completion = std::move(pending.Completion);
			if (completion) {
				completion(std::move(terminal));
			}
		}

		void publish_failure(
			pending_operation&                                 operation,
			const application::github_credential_vault_failure failure)
		{
			const auto effective_failure = normalize_failure(failure);
			std::visit(
				overloaded{
					[effective_failure](pending_store& pending) {
						publish_store(
							pending,
							application::github_credential_store_error{
								.Context = pending.Enrollment.Context,
								.Failure = effective_failure,
							});
					},
					[effective_failure](pending_retrieve& pending) {
						publish_retrieve(
							pending,
							application::github_credential_retrieve_error{
								.Context = pending.Context,
								.Failure = effective_failure,
							});
					},
					[effective_failure](pending_delete& pending) {
						publish_delete(
							pending,
							application::github_credential_delete_error{
								.Context = pending.Context,
								.Failure = effective_failure,
							});
					},
				},
				operation);
		}

		void publish_cancelled(pending_operation& operation)
		{
			std::visit(
				overloaded{
					[](pending_store& pending) {
						publish_store(
							pending,
							application::github_credential_store_cancelled{
								.Context = pending.Enrollment.Context,
							});
					},
					[](pending_retrieve& pending) {
						publish_retrieve(
							pending,
							application::github_credential_retrieve_cancelled{
								.Context = pending.Context,
							});
					},
					[](pending_delete& pending) {
						publish_delete(
							pending,
							application::github_credential_delete_cancelled{
								.Context = pending.Context,
							});
					},
				},
				operation);
		}

		class windows_credential_manager_vault final
			: public application::github_credential_vault_port
		{
		private:
			struct shared_state
			{
				shared_state(
					std::shared_ptr<windows_credential_manager_backend> backend,
					std::shared_ptr<windows_credential_manager_backend> aRefreshBackend) noexcept
					: Backend{ std::move(backend) }
					, RefreshBackend{ std::move(aRefreshBackend) }
				{
				}

				std::mutex                                          Mutex;
				pending_operations                                  Pending;
				std::shared_ptr<windows_credential_manager_backend> Backend;
				std::shared_ptr<windows_credential_manager_backend> RefreshBackend;
				bool                                                Closing = false;
			};

		public:
			windows_credential_manager_vault(
				application::effect_executor&                       effect_lane,
				std::shared_ptr<windows_credential_manager_backend> backend,
				std::shared_ptr<windows_credential_manager_backend> aRefreshBackend)
				: EffectLane{ effect_lane }
				, State{ std::make_shared<shared_state>(
					  std::move(backend),
					  std::move(aRefreshBackend)) }
			{
			}

			~windows_credential_manager_vault() override
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
				application::github_credential_enrollment               enrollment,
				application::github_credential_store_completion_handler completion)
				override
			{
				submit_store(
					credential_slot::credential,
					std::move(enrollment),
					std::move(completion));
			}

			void retrieve(
				application::github_credential_operation_context           context,
				application::github_credential_retrieve_completion_handler completion)
				override
			{
				submit_retrieve(
					credential_slot::credential,
					context,
					std::move(completion));
			}

			void delete_credential(
				application::github_credential_operation_context         context,
				application::github_credential_delete_completion_handler completion)
				override
			{
				submit_delete(
					credential_slot::credential,
					context,
					std::move(completion));
			}

			// BPR-0543. The refresh slot is a second Credential Manager target
			// under the same authority account, reached through the same queue
			// and the same effect lane as the base credential. It answers
			// unavailable only when that store cannot be reached at all.
			void store_refresh(
				application::github_credential_enrollment               aEnrollment,
				application::github_credential_store_completion_handler aCompletion)
				override
			{
				submit_store(
					credential_slot::refresh,
					std::move(aEnrollment),
					std::move(aCompletion));
			}

			void retrieve_refresh(
				application::github_credential_operation_context           aContext,
				application::github_credential_retrieve_completion_handler aCompletion)
				override
			{
				submit_retrieve(
					credential_slot::refresh,
					aContext,
					std::move(aCompletion));
			}

			void delete_refresh(
				application::github_credential_operation_context         aContext,
				application::github_credential_delete_completion_handler aCompletion)
				override
			{
				submit_delete(
					credential_slot::refresh,
					aContext,
					std::move(aCompletion));
			}

			void cancel(
				application::github_credential_operation_context context) override
			{
				std::optional<pending_operation> pending;
				{
					const auto lock = std::scoped_lock{ State->Mutex };
					pending         = extract_pending(State->Pending, context);
				}
				if (!pending.has_value()) {
					return;
				}
				publish_cancelled(*pending);
			}

		private:
			void submit_store(
				const credential_slot                                   aSlot,
				application::github_credential_enrollment               aEnrollment,
				application::github_credential_store_completion_handler aCompletion)
			{
				const auto operation = aEnrollment.Context;
				if (!application::valid(aEnrollment) || !aCompletion) {
					aEnrollment.Credential.reset();
					if (aCompletion) {
						aCompletion(application::github_credential_store_error{
							.Context = operation,
							.Failure = application::
								github_credential_vault_failure::invalid_request,
						});
					}
					return;
				}

				queue(
					operation,
					pending_store{
						.Enrollment = std::move(aEnrollment),
						.Completion = std::move(aCompletion),
						.Slot       = aSlot,
					});
			}

			void submit_retrieve(
				const credential_slot                                      aSlot,
				const application::github_credential_operation_context&    aContext,
				application::github_credential_retrieve_completion_handler aCompletion)
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
						.Slot       = aSlot,
					});
			}

			void submit_delete(
				const credential_slot                                    aSlot,
				const application::github_credential_operation_context&  aContext,
				application::github_credential_delete_completion_handler aCompletion)
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
						.Slot       = aSlot,
					});
			}

			void queue(
				const application::github_credential_operation_context& context,
				pending_operation                                       operation)
			{
				auto rejected = false;
				auto closing  = false;
				try {
					const auto lock = std::scoped_lock{ State->Mutex };
					closing         = State->Closing;
					if (!closing) {
						rejected = !State->Pending.try_emplace(
													  context,
													  std::move(operation))
										.second;
					}
				} catch (const std::bad_alloc&) {
					publish_failure(
						operation,
						application::github_credential_vault_failure::
							allocation_failure);
					return;
				} catch (...) {
					publish_failure(
						operation,
						application::github_credential_vault_failure::
							operation_failed);
					return;
				}

				if (closing) {
					publish_failure(
						operation,
						application::github_credential_vault_failure::unavailable);
					return;
				}
				if (rejected) {
					publish_failure(
						operation,
						application::github_credential_vault_failure::
							invalid_request);
					return;
				}

				const auto weak_state = std::weak_ptr<shared_state>{ State };
				try {
					EffectLane.post([weak_state, context] {
						const auto state = weak_state.lock();
						if (state != nullptr) {
							execute(state, context);
						}
					});
				} catch (const std::bad_alloc&) {
					fail_queued(
						context,
						application::github_credential_vault_failure::
							allocation_failure);
				} catch (...) {
					fail_queued(
						context,
						application::github_credential_vault_failure::
							operation_failed);
				}
			}

			void fail_queued(
				const application::github_credential_operation_context& context,
				const application::github_credential_vault_failure      failure)
			{
				std::optional<pending_operation> pending;
				{
					const auto lock = std::scoped_lock{ State->Mutex };
					pending         = extract_pending(State->Pending, context);
				}
				if (!pending.has_value()) {
					return;
				}
				publish_failure(*pending, failure);
			}

			static void execute(
				const std::shared_ptr<shared_state>&                    state,
				const application::github_credential_operation_context& context)
			{
				std::optional<pending_operation>                    pending;
				std::shared_ptr<windows_credential_manager_backend> backend;
				{
					const auto lock = std::scoped_lock{ state->Mutex };
					pending         = extract_pending(state->Pending, context);
					if (pending.has_value()) {
						backend = slot_of(*pending) == credential_slot::refresh
							? state->RefreshBackend
							: state->Backend;
					}
				}
				if (!pending.has_value()) {
					return;
				}
				// A vault built without a refresh backend has no second slot to
				// reach; that store is unreachable, which is what unavailable
				// means here and the only thing it means (BPR-0543).
				if (backend == nullptr) {
					publish_failure(
						*pending,
						application::github_credential_vault_failure::
							unavailable);
					return;
				}

				std::visit(
					overloaded{
						[&backend](pending_store& operation) {
							execute_store(backend, operation);
						},
						[&backend](pending_retrieve& operation) {
							execute_retrieve(backend, operation);
						},
						[&backend](pending_delete& operation) {
							execute_delete(backend, operation);
						},
					},
					*pending);
			}

			static void execute_store(
				const std::shared_ptr<windows_credential_manager_backend>& backend,
				pending_store&                                             pending)
			{
				try {
					const auto result = backend->store(
						pending.Enrollment.Context.Authority,
						pending.Enrollment.Credential);
					if (result.has_value() && application::valid(*result)) {
						publish_store(
							pending,
							application::github_credential_store_success{
								.Context     = pending.Enrollment.Context,
								.Disposition = *result,
							});
						return;
					}

					const auto failure = result.has_value()
						? application::github_credential_vault_failure::
							  operation_failed
						: result.error();
					publish_store(
						pending,
						application::github_credential_store_error{
							.Context = pending.Enrollment.Context,
							.Failure = normalize_failure(failure),
						});
				} catch (const std::bad_alloc&) {
					publish_store(
						pending,
						application::github_credential_store_error{
							.Context = pending.Enrollment.Context,
							.Failure = application::
								github_credential_vault_failure::allocation_failure,
						});
				} catch (...) {
					publish_store(
						pending,
						application::github_credential_store_error{
							.Context = pending.Enrollment.Context,
							.Failure = application::
								github_credential_vault_failure::operation_failed,
						});
				}
			}

			static void execute_retrieve(
				const std::shared_ptr<windows_credential_manager_backend>& backend,
				pending_retrieve&                                          pending)
			{
				try {
					auto result = backend->retrieve(pending.Context.Authority);
					std::visit(
						overloaded{
							[&pending](
								application::github_credential_material& credential) {
								if (!application::valid(credential)) {
									credential.reset();
									publish_retrieve(
										pending,
										application::github_credential_retrieve_error{
											.Context = pending.Context,
											.Failure = application::
												github_credential_vault_failure::
													corrupt,
										});
									return;
								}
								publish_retrieve(
									pending,
									application::github_credential_retrieve_success{
										.Context    = pending.Context,
										.Credential = std::move(credential),
									});
							},
							[&pending](
								windows_credential_manager_credential_absent) {
								publish_retrieve(
									pending,
									application::github_credential_retrieve_absent{
										.Context = pending.Context,
									});
							},
							[&pending](
								const application::github_credential_vault_failure
									failure) {
								publish_retrieve(
									pending,
									application::github_credential_retrieve_error{
										.Context = pending.Context,
										.Failure = normalize_failure(failure),
									});
							},
						},
						result);
				} catch (const std::bad_alloc&) {
					publish_retrieve(
						pending,
						application::github_credential_retrieve_error{
							.Context = pending.Context,
							.Failure = application::
								github_credential_vault_failure::allocation_failure,
						});
				} catch (...) {
					publish_retrieve(
						pending,
						application::github_credential_retrieve_error{
							.Context = pending.Context,
							.Failure = application::
								github_credential_vault_failure::operation_failed,
						});
				}
			}

			static void execute_delete(
				const std::shared_ptr<windows_credential_manager_backend>& backend,
				pending_delete&                                            pending)
			{
				try {
					const auto result = backend->delete_credential(pending.Context.Authority);
					if (result.has_value() && application::valid(*result)) {
						publish_delete(
							pending,
							application::github_credential_delete_success{
								.Context     = pending.Context,
								.Disposition = *result,
							});
						return;
					}

					const auto failure = result.has_value()
						? application::github_credential_vault_failure::
							  operation_failed
						: result.error();
					publish_delete(
						pending,
						application::github_credential_delete_error{
							.Context = pending.Context,
							.Failure = normalize_failure(failure),
						});
				} catch (const std::bad_alloc&) {
					publish_delete(
						pending,
						application::github_credential_delete_error{
							.Context = pending.Context,
							.Failure = application::
								github_credential_vault_failure::allocation_failure,
						});
				} catch (...) {
					publish_delete(
						pending,
						application::github_credential_delete_error{
							.Context = pending.Context,
							.Failure = application::
								github_credential_vault_failure::operation_failed,
						});
				}
			}

			application::effect_executor& EffectLane;
			std::shared_ptr<shared_state> State;
		};

		[[nodiscard]] auto make_vault(
			application::effect_executor&                       effect_lane,
			std::shared_ptr<windows_credential_manager_backend> backend,
			std::shared_ptr<windows_credential_manager_backend> aRefreshBackend) noexcept
			-> windows_credential_manager_vault_factory_result
		{
			if (backend == nullptr) {
				return std::unexpected{
					application::github_credential_vault_failure::invalid_request
				};
			}

			try {
				return std::unique_ptr<application::github_credential_vault_port>{
					std::make_unique<windows_credential_manager_vault>(
						effect_lane,
						std::move(backend),
						std::move(aRefreshBackend))
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

	auto windows_credential_manager_target_prefix(
		const windows_credential_manager_target_scope scope) noexcept
		-> std::wstring_view
	{
		switch (scope) {
		case windows_credential_manager_target_scope::production:
			return production_target_prefix;
		case windows_credential_manager_target_scope::integration_test:
			return integration_test_target_prefix;
		case windows_credential_manager_target_scope::production_refresh:
			return production_refresh_target_prefix;
		case windows_credential_manager_target_scope::integration_test_refresh:
			return integration_test_refresh_target_prefix;
		}
		return { };
	}

	auto encode_windows_credential_manager_authority_account(
		const application::github_credential_authority_key& authority) noexcept
		-> std::expected<
			windows_credential_manager_authority_account,
			application::github_credential_vault_failure>
	{
		if (!application::valid(authority)) {
			return std::unexpected{
				application::github_credential_vault_failure::invalid_request
			};
		}

		windows_credential_manager_authority_account encoded{ };
		std::size_t                                  offset = 0U;
		append_hex_u64(encoded, offset, authority.Profile.value());
		append_hex_u64(encoded, offset, authority.ProfileRevision.value());
		append_hex_u64(encoded, offset, authority.Host.value());
		append_hex_u64(encoded, offset, authority.Account.value());
		append_hex_u8(
			encoded,
			offset,
			server_contract_discriminator(authority.ServerContract));
		const auto hostname = authority.Hostname.value();
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

	auto classify_windows_credential_manager_status(
		const std::uint32_t status) noexcept
		-> windows_credential_manager_status_mapping
	{
		switch (status) {
		case ERROR_SUCCESS:
			return {
				.Kind    = windows_credential_manager_status_kind::success,
				.Failure = application::github_credential_vault_failure::none,
			};
		case ERROR_NOT_FOUND:
			return {
				.Kind    = windows_credential_manager_status_kind::item_not_found,
				.Failure = application::github_credential_vault_failure::none,
			};
		case ERROR_NO_SUCH_LOGON_SESSION:
			return failure_mapping(
				application::github_credential_vault_failure::unavailable);
		case ERROR_ACCESS_DENIED:
		case ERROR_PRIVILEGE_NOT_HELD:
			return failure_mapping(
				application::github_credential_vault_failure::access_denied);
		case ERROR_INVALID_DATA:
		case ERROR_BAD_FORMAT:
		case ERROR_FILE_CORRUPT:
			return failure_mapping(
				application::github_credential_vault_failure::corrupt);
		case ERROR_NOT_ENOUGH_MEMORY:
		case ERROR_OUTOFMEMORY:
			return failure_mapping(
				application::github_credential_vault_failure::
					allocation_failure);
		case ERROR_INVALID_PARAMETER:
		case ERROR_INVALID_FLAGS:
		case ERROR_BAD_USERNAME:
		case ERROR_BAD_LENGTH:
			return failure_mapping(
				application::github_credential_vault_failure::
					invalid_request);
		default:
			return failure_mapping(
				application::github_credential_vault_failure::
					operation_failed);
		}
	}

	auto make_windows_credential_manager_vault(
		application::effect_executor& effect_lane)
		-> windows_credential_manager_vault_factory_result
	{
		auto backend = make_native_backend(
			windows_credential_manager_target_scope::production);
		if (!backend.has_value()) {
			return std::unexpected{ backend.error() };
		}
		auto refresh = make_native_backend(
			windows_credential_manager_target_scope::production_refresh);
		if (!refresh.has_value()) {
			return std::unexpected{ refresh.error() };
		}
		return make_vault(
			effect_lane,
			std::move(*backend),
			std::move(*refresh));
	}

	auto make_windows_credential_manager_integration_test_vault(
		application::effect_executor& effect_lane)
		-> windows_credential_manager_vault_factory_result
	{
		auto backend = make_native_backend(
			windows_credential_manager_target_scope::integration_test);
		if (!backend.has_value()) {
			return std::unexpected{ backend.error() };
		}
		auto refresh = make_native_backend(
			windows_credential_manager_target_scope::integration_test_refresh);
		if (!refresh.has_value()) {
			return std::unexpected{ refresh.error() };
		}
		return make_vault(
			effect_lane,
			std::move(*backend),
			std::move(*refresh));
	}

	auto make_windows_credential_manager_vault_with_backend(
		application::effect_executor&                       effect_lane,
		std::shared_ptr<windows_credential_manager_backend> backend,
		std::shared_ptr<windows_credential_manager_backend> aRefreshBackend) noexcept
		-> windows_credential_manager_vault_factory_result
	{
		return make_vault(
			effect_lane,
			std::move(backend),
			std::move(aRefreshBackend));
	}
}
