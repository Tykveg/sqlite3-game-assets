project(bhh_sqlite3_game_assets_test CXX)
add_executable(${PROJECT_NAME} "test.cpp")
target_link_libraries(${PROJECT_NAME} PUBLIC bhh_sqlite3_game_assets)
bhh_assets_generate_dbs(${CMAKE_CURRENT_BINARY_DIR} "'${CMAKE_CURRENT_SOURCE_DIR}/assets/db1' '${CMAKE_CURRENT_SOURCE_DIR}/assets/db2'")
