# The syntax adapter exists only where the packaged KSyntaxHighlighting
# definitions exist. The Qt-free configuration declares no target at all, so a
# core-only build cannot acquire the framework by accident.
if(NOT BPR_BUILD_QT)
    return()
endif()

add_library(
    better_pull_review_adapter_syntax
    STATIC
        src/packaged_syntax_classifier.cpp
        src/cpp_template_arguments.cpp
        src/cpp_template_arguments.hpp
)
add_library(
    BetterPullReview::AdapterSyntax
    ALIAS
    better_pull_review_adapter_syntax
)
target_include_directories(
    better_pull_review_adapter_syntax
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
    better_pull_review_adapter_syntax
    PUBLIC
        BetterPullReview::Application
    PRIVATE
        KF6::SyntaxHighlighting
        Qt6::Core
)
target_compile_features(
    better_pull_review_adapter_syntax
    PUBLIC
        cxx_std_23
)
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_definitions(
        better_pull_review_adapter_syntax
        PUBLIC
            _LIBCPP_ENABLE_EXPERIMENTAL
    )
endif()
bpr_enable_hardening(better_pull_review_adapter_syntax)
bpr_enable_warnings(better_pull_review_adapter_syntax)

# [[BPR-0974]]. The packaged definitions this adapter classifies with. Where the
# image keeps its Qt resources beside it, the framework is built without them
# and they reach the application through the resource bundle, from the
# framework's own collection; everywhere else the framework embeds them.
get_property(bpr_syntax_collection GLOBAL PROPERTY BPR_KSYNTAXHIGHLIGHTING_SYNTAX_COLLECTION)
get_property(bpr_syntax_index GLOBAL PROPERTY BPR_KSYNTAXHIGHLIGHTING_SYNTAX_INDEX)
bpr_add_qt_resource_collection(
    QRC "${bpr_syntax_collection}"
    DEPENDS "${bpr_syntax_index}"
)
