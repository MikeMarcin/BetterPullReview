add_library(
    better_pull_review_adapter_qt
    STATIC
        src/foundation_window.cpp
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
