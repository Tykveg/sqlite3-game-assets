# sqlite3-game-assets

This is a project I coded back then in 2024.

This is a C++ library with cmake integration that automatically packs files (e.g. game assets) into sqlite databases with optional encryption and nice interface.

## Requirements
- C++17 compiler (gcc tested)
- cmake

## Usage

### Test

See `test/` (`test/test.cpp`, `test/CMakeLists.txt`, `test/assets/`) for an example. `db1` is not encrypted and `db2` is encrypted.

### cmake

Use (example):
```cmake
add_subdirectory(path/to/sqlite3-game-assets)
bhh_assets_generate_dbs(${CMAKE_CURRENT_BINARY_DIR} "'${CMAKE_CURRENT_SOURCE_DIR}/assets/db1' '${CMAKE_CURRENT_SOURCE_DIR}/assets/db2'")
```

### C++ code

```cpp
bhh::AssetManager db("db_name");
bhh::Asset asset = db.get("table", "dir1/dir2/file");
int blob_size = asset.getBlobSize();
assert(blob_size > 0); // remember to check for a blob size
const char* tmp_blob = static_cast<const char*>(asset.getBlob()); // `getBlob` returns `const void*`
assert(blob); // remember to check the blob
char* blob = new char[blob_size];
std::copy(tmp_blob, tmp_blob + blob_size, blob); // copy blob because it will be freed on `asset` destruction!
// Feel free to use `char* blob`
// ...
delete[] blob; // remember to delete the blob
// You can use `tmp_blob` without copying but remember that the memory will be freed on `asset` destruction
```

#### String obfuscation

Use `BHH_CRYPT_STR("string")` to obfuscate a static string so that the static string would be obfuscated in a binary to prevent some data mining. Works only with -O2 and -O3 (cmake "Release" config basically).

## Project overview

- `include/bhh-asset-packer.hpp` - a library header
- `src/bhh-asset-packer.cpp` - a library implementation
- `external/` - external libraries used in project
  - `sqlite3mc/` - sqlite3 with encryption support (downloads the source code from github) (https://github.com/utelle/SQLite3MultipleCiphers/releases)
  - `StringObfuscator/` - static string obfuscator (https://github.com/katursis/StringObfuscator)
- `scripts/` - "scripts" used to pack assets
  - `asset-creator/` - program that packs assets into sqlite dbs on build
  - `key-converter` - program that sends encryption keys for dbs to cmake for a macro definition. Gets compiled on a cmake configuration stage
  - `generate_dbs.cmake` - `bhh_assets_generate_dbs` definition. Also defines some essential targets
- `test/` - test
  - `test.cpp` - test program
  - `assets/` - test assets
    - `db1/` - test unencrypted assets
    - `db2/` - test encrypted assets
      - `KEY` - encryption key

## Projects and code used

- crc32 sum: http://web.archive.org/web/20080303102530/http://c.snippets.org/snip_lister.php?fname=crc_32.c
- String obfuscator: https://github.com/katursis/StringObfuscator
- sqlite with encryption: https://github.com/utelle/SQLite3MultipleCiphers
