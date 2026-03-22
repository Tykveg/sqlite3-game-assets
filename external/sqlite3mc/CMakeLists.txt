include(FetchContent)
FetchContent_Declare(
  sqlite3mcfetch
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  URL https://github.com/utelle/SQLite3MultipleCiphers/releases/download/v1.8.4/sqlite3mc-1.8.4-sqlite-3.45.2-amalgamation.zip
  URL_HASH SHA256=e27cd7c5f8e525980f38df25a91b45abd69957397ad2797409abba43c0dffc7c
)
FetchContent_MakeAvailable(sqlite3mcfetch)
project(sqlite3mcLib C)
set(CMAKE_C_FLAGS "-msse4 -maes")
find_package(Threads REQUIRED)
add_library(${PROJECT_NAME} STATIC ${sqlite3mcfetch_SOURCE_DIR}/sqlite3mc_amalgamation.c) # or use sqlite3mc_static if you are just cloning the repo
target_compile_definitions(${PROJECT_NAME} PUBLIC SQLITE_OMIT_DEPRECATED SQLITE_OMIT_SHARED_CACHE)
target_include_directories(${PROJECT_NAME} PUBLIC ${sqlite3mcfetch_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
