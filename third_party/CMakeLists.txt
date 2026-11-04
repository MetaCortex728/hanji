# Vendored third-party code. Each component keeps its licence next to its
# files and is listed in NOTICE. Built without the project's warning flags.

add_library(hanji_doctest INTERFACE)
target_include_directories(hanji_doctest SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/doctest)

add_library(hanji_json INTERFACE)
target_include_directories(hanji_json SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json/include)
