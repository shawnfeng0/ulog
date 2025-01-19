find_package(zstd)
if (NOT zstd_FOUND)
    include(FetchContent)
    FetchContent_Declare(
            zstd
            URL https://github.com/facebook/zstd/archive/refs/tags/v1.5.6.zip
            SOURCE_SUBDIR build/cmake
    )
    FetchContent_MakeAvailable(zstd)
    add_library(zstd::libzstd_shared ALIAS libzstd_shared)
    add_library(zstd::libzstd_static ALIAS libzstd_static)
endif ()
