cmake_minimum_required(VERSION 3.16)
project(bhh_sqlite3_game_assets_key_converter CXX)
add_executable(${PROJECT_NAME}
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/bhh-asset-packer.cpp
)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_compile_definitions(${PROJECT_NAME} PRIVATE BHH_ASSET_PACKER_NO_SQLITE_IMPLEMENTATION)
