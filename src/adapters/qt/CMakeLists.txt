add_library(better_pull_review_adapter_qt INTERFACE)
add_library(BetterPullReview::AdapterQt ALIAS better_pull_review_adapter_qt)

target_link_libraries(
    better_pull_review_adapter_qt
    INTERFACE
        BetterPullReview::Application
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)
target_compile_features(better_pull_review_adapter_qt INTERFACE cxx_std_23)
