add_library(
    better_pull_review_adapter_qt
    STATIC
        src/event_loop_executor.cpp
        src/epoch_comparison_control.cpp
        src/epoch_history_model.cpp
        src/foundation_window.cpp
        src/github_conversation_delegate.cpp
        src/github_conversation_model.cpp
        src/github_window.cpp
        src/measured_alignment_axis.cpp
        src/sensitive_text_input.cpp
)
add_library(BetterPullReview::AdapterQt ALIAS better_pull_review_adapter_qt)

target_link_libraries(
    better_pull_review_adapter_qt
    PUBLIC
        BetterPullReview::Application
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)
target_include_directories(
    better_pull_review_adapter_qt
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_features(better_pull_review_adapter_qt PUBLIC cxx_std_23)

bpr_enable_hardening(better_pull_review_adapter_qt)
bpr_enable_warnings(better_pull_review_adapter_qt)
