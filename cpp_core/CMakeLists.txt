cmake_minimum_required(VERSION 3.18)
project(talentmatch_core CXX)

# ============================================================================
# C++ standard
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ============================================================================
# Compiler flags
# ============================================================================
if(MSVC)
    add_compile_options(/W3 /O2 /EHsc)
    add_definitions(-DTALENTMATCH_EXPORTS -D_CRT_SECURE_NO_WARNINGS -DNOMINMAX)
else()
    add_compile_options(-Wall -Wextra -O3 -fPIC -fvisibility=hidden)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static -static-libstdc++ -static-libgcc")
endif()

# ============================================================================
# Fetch nlohmann/json (header-only, no compile step needed)
# ============================================================================
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ============================================================================
# Optional XGBoost support
# Compile with: cmake -DENABLE_XGBOOST=ON ..
# ============================================================================
option(ENABLE_XGBOOST "Enable XGBoost ML ranking (requires libxgboost)" OFF)

if(ENABLE_XGBOOST)
    find_package(xgboost QUIET)
    if(xgboost_FOUND)
        message(STATUS "XGBoost found — ML ranking enabled")
        add_definitions(-DHAVE_XGBOOST)
    else()
        # Try manual include/library discovery
        find_path(XGBOOST_INCLUDE_DIR xgboost/c_api.h
            PATHS /usr/local/include /usr/include ${XGBOOST_ROOT}/include)
        find_library(XGBOOST_LIB xgboost
            PATHS /usr/local/lib /usr/lib ${XGBOOST_ROOT}/lib)
        if(XGBOOST_INCLUDE_DIR AND XGBOOST_LIB)
            message(STATUS "XGBoost found manually at ${XGBOOST_LIB}")
            add_definitions(-DHAVE_XGBOOST)
            set(XGBOOST_LIBRARIES ${XGBOOST_LIB})
            include_directories(${XGBOOST_INCLUDE_DIR})
        else()
            message(WARNING "XGBoost not found — using linear fallback scorer")
        endif()
    endif()
else()
    message(STATUS "XGBoost disabled — using linear fallback scorer (recommended for dev)")
endif()

# ============================================================================
# Source files
# ============================================================================
set(TALENTMATCH_SOURCES
    engine.cpp

    parser/section_detector.cpp
    parser/resume_parser.cpp

    skills/trie.cpp
    skills/synonym_matcher.cpp
    skills/skill_engine.cpp

    features/experience.cpp
    features/education.cpp
    features/skills_features.cpp

    retrieval/bm25.cpp

    ranking/xgboost_ranker.cpp

    explainability/explanation_engine.cpp
)

# ============================================================================
# Shared library target
# ============================================================================
add_library(talentmatch SHARED ${TALENTMATCH_SOURCES})

target_include_directories(talentmatch
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(talentmatch
    PRIVATE
        nlohmann_json::nlohmann_json
        $<$<BOOL:${XGBOOST_LIBRARIES}>:${XGBOOST_LIBRARIES}>
)

if(xgboost_FOUND)
    target_link_libraries(talentmatch PRIVATE xgboost::xgboost)
endif()

# Output library name without platform prefix confusion
set_target_properties(talentmatch PROPERTIES
    OUTPUT_NAME "talentmatch"
    PREFIX ""
)

# On Windows generate the import library alongside the DLL
if(MSVC)
    set_target_properties(talentmatch PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS OFF
    )
endif()

# ============================================================================
# Optional: standalone CLI test binary
# ============================================================================
option(BUILD_CLI "Build standalone CLI test binary" OFF)
if(BUILD_CLI)
    add_executable(tm_cli cli_main.cpp)
    target_include_directories(tm_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(tm_cli PRIVATE talentmatch nlohmann_json::nlohmann_json)
endif()

# ============================================================================
# Install rules (optional)
# ============================================================================
install(TARGETS talentmatch
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES include/engine.h DESTINATION include/talentmatch)

# ============================================================================
# Print summary
# ============================================================================
message(STATUS "")
message(STATUS "=== TalentMatch Core Build Configuration ===")
message(STATUS "  Build type    : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard  : ${CMAKE_CXX_STANDARD}")
message(STATUS "  XGBoost       : ${ENABLE_XGBOOST}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "============================================")
message(STATUS "")
message(STATUS "Build with:")
message(STATUS "  mkdir build && cd build && cmake .. && cmake --build . --config Release")
message(STATUS "")
