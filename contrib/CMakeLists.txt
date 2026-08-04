set(FETCHCONTENT_QUIET FALSE)
include(FetchContent)

FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 12.1.0
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
