#pragma once
#include <string>
#include <cstdint>
#include <map>
#include <vector>
#include <stdexcept>

#ifndef BHH_ASSET_PACKER_NO_SQLITE_IMPLEMENTATION

#ifndef BHH_ASSET_PACKER_ALL_KEYS
#warning BHH_ASSET_PACKER_ALL_KEYS is undefined so no encryption will be used
#define BHH_ASSET_PACKER_ALL_KEYS
#endif // BHH_ASSET_PACKER_ALL_KEYS

#include "sqlite3mc_amalgamation.h"
//#include "sqlite3mc.h"
#include <utility>
#include <filesystem>

#endif // BHH_ASSET_PACKER_NO_SQLITE_IMPLEMENTATION

namespace bhh {

/**
 * Calculate `crc32` hash for `buf`.
 * @param buf Array buffer to calculate crc32 from.
 * @param len The length of `buf`.
 * @return crc32 hash.
*/
uint32_t crc32(const void* buf, int len) noexcept;

/**
 * Calculate `crc32` hash for string `str`.
 * @param str String to calculate crc32.
 * @return crc32 hash.
*/
inline uint32_t crc32(const std::string& str) noexcept {
  return bhh::crc32(str.c_str(), str.length());
}

/**
 * Calculates crc32 for file at `path` and for not reading the same file twice, also writes file content to `buffer`.
 *
 * @param path Path for file.
 * @param buffer Reference for vector , that will get the file content. Also will be resized and the previous data will be erased.
 * @return crc32 result.
 * @throws std::runtime_error Throws if failed to open the file.
*/
uint32_t crc32ForFile(const std::string& path, std::vector<unsigned char>& buffer);

/**
 * Calculates crc32 for file at `path` without saving file content.
 *
 * @param path Path for file.
 * @return crc32 result.
 * @throws std::runtime_error Throws if failed to open the file.
*/
uint32_t crc32ForFile(const std::string& path);

/**
 * Get error for sqlite
 * @note std::runtime_error("${text} | ${rc}");
 * @param text Error text.
 * @param rc Return code / error code from sqlite3 function.
 * @return std::runtime_error object that you can throw.
*/
inline std::runtime_error SQLiteError(const std::string& text, const int& rc) {
  return std::runtime_error(text + " | " + std::to_string(rc));
}

// Unimplemented
//const std::string reserved_tables[] = { "BHH/RESERVED1", }

// This is a file, where you read the encryption key from. This file must be in the root of asset DB dir.
const std::string key_file_path = "KEY";

/**
 * Default table, that is used in `AssetManager::get(const std::string& filename)`.
 * Why? Because why not.
*/
extern const std::string default_table;

using DBKey = std::vector<unsigned char>;

/**
 * Get the key from keys map. If key somehow does not exist, returns static empty key.
 * @param keys The keys map.
 * @param db_name_crc32 DB name's crc32
 * @return Key
*/
const DBKey& tryFindKey(const std::map<uint32_t, DBKey>& keys, const uint32_t& db_name_crc32);

class AssetManager;
class Asset;

#ifndef BHH_ASSET_PACKER_NO_SQLITE_IMPLEMENTATION

/**
 * Encrypt string at compile time and decrypt it!
 * The string will be obfuscated inside executable.
 * @note Depends on compiler and compile options.
 * @note https://github.com/katursis/StringObfuscator
 * @param str String (literal).
 * @return const char* original string.
*/
#define BHH_CRYPT_STR(str) cryptor::create((str)).decrypt()

class Asset {
  public:
    Asset(const Asset&) = delete;

    Asset& operator=(const Asset&) = delete;

    Asset(Asset&& a) noexcept {
      swap(a);
    }

    Asset& operator=(Asset&& a) noexcept {
      swap(a);
      return *this;
    }

    /**
     * Get pointer to blob from asset.
     * @note After Asset object deletion, you *cannot* use the pointer anymore (unless `manual_destructor` is set to true). So you have to copy the blob.
     * @return Pointer to blob.
     * @throws std::runtime_error Throws if Asset is clean.
    */
    const void* getBlob() const {
      if (isClean()) {
        throw std::runtime_error("Cannot getBlob() because asset is deleted.");
      }
      return sqlite3_column_blob(stmt, 0);
    }

    /**
     * Get the blob size.
     * @return The blob size.
     * @throws std::runtime_error Throws if Asset is clean.
    */
    int getBlobSize() const {
      if (isClean()) {
        throw std::runtime_error("Cannot getBlobSize() because asset is deleted.");
      }
      return sqlite3_column_bytes(stmt, 0);
    }

    /**
     * Get the table name, from which this asset came from.
     * @return The table name string.
    */
    std::string getTableName() const {
      return table;
    }

    /**
     * Get asset's file name.
     * @return The asset's file name string.
    */
    std::string getFilename() const {
      return filename;
    }

    /**
     * Get a pointer to Asset Manager, that created this asset.
     * @note You probably don't want to use it.
     * @return Pointer to Asset Manager. Can be `nullptr`.
    */
    const AssetManager* getAssetManager() const noexcept {
      return manager;
    }

    /**
     * Get a pointer to the internal sqlite3 statement.
     * @note You probably don't want to use it.
     * @return Pointer to internal sqlite3 statement. Can be `nullptr` if Asset was cleared.
    */
    sqlite3_stmt* getSQLiteStatement() const noexcept {
      return stmt;
    }

    /**
     * Clears Asset by freeing the blob. You can no longer use a pointer to blob!
     * @note This method is safe to call any amount of times. Also can be called with `manual_destructor` == false.
    */
    void clear() noexcept {
      sqlite3_finalize(stmt);
      stmt = nullptr;
    }

    /**
     * Checks if Asset is clean.
     * @return True if is clean. False otherwise.
    */
    bool isClean() const noexcept {
      return stmt == nullptr;
    }

    ~Asset() {
      if (!manual_destructor) {
        clear();
      }
    }

  private:
    friend AssetManager;
    Asset(const std::string& table, const std::string& filename, sqlite3_stmt* stmt, const AssetManager* manager = nullptr) : table(table), filename(filename), stmt(stmt), manager(manager) {}
    void swap(Asset& a) {
      std::swap(table, a.table);
      std::swap(filename, a.filename);
      std::swap(stmt, a.stmt);
      std::swap(manager, a.manager);
    }
    std::string table;
    std::string filename;
    sqlite3_stmt* stmt = nullptr;
    const AssetManager* manager = nullptr;

  public:
  /**
   * If true, the Asset won't be cleared at object destruction, so you have to manually call `clear()` to free resources.
   * False by default.
   * @note You probably shouldn't set it to true. Please copy the blob instead of manually freeing resources.
  */
  bool manual_destructor = false;
};

class AssetManager {
  public:
    /**
     * AssetManager default constructor.
     * @note In order to use Asset Manager, please call method `connect`.
    */
    AssetManager() {}

    /**
     * AssetManager constructor.
     * Basically calls `connect` on object construction.
     * If something goes wrong, the object clears itself.
     * @param db_path DB path. Can also be string.
    */
    AssetManager(const std::filesystem::path& db_path) {
      try {
        connect(db_path);
      } catch (...) {
        disconnect();
        throw;
      }
    }

    AssetManager& operator=(const AssetManager&) = delete;

    AssetManager(const AssetManager&) = delete;

    AssetManager& operator=(AssetManager&& m) noexcept {
      swap(m);
      return *this;
    }

    AssetManager(AssetManager&& m) noexcept {
      swap(m);
    }

    /**
     * Connects do a database.
     * @param db_path DB path. Can also be string.
     * @throws std::runtime_error Throws if: is already connected, fails to open DB or fails to decrypt DB.
    */
    void connect(const std::filesystem::path& db_path);

    /**
     * Get Asset from DB.
     * @param table Table name.
     * @param filename File name.
     * @throws std::runtime_error Throws if got internal sqlite errors. Also throws if fails to find table and/or file.
    */
    Asset get(const std::string& table, const std::string& filename) const;

    /**
     * Get Asset from DB. The table name will be `default_table`. 
     * @param filename File name.
     * @throws std::runtime_error Throws if got internal sqlite errors. Also throws if fails to find table and/or file.
    */
    inline Asset get(const std::string& filename) const {
      return get(default_table, filename);
    }

    /**
     * Get path to current DB.
     * @return Path as string.
    */
    std::string getPath() const {
      return path;
    }

    /**
     * Get a current DB (file) name.
     * @return Name as string.
    */
    std::string getName() const {
      return name;
    }

    /**
     * Checks if DB is connected.
     * @return True if connected, false otherwise.
    */
    bool isConnected() const noexcept {
      return connected && (db != nullptr);
    }

    /**
     * Disconnects DB.
     * @note Since DB is connected with read-only mode, I don't think you need to use this method.
     * @note Safe to call multiple times.
    */
    void disconnect() noexcept {
      sqlite3_close(db);
      connected = false;
      db = nullptr;
    }

    ~AssetManager() {
      disconnect();
    }

  private:
    void swap(AssetManager& m) {
      std::swap(path, m.path);
      std::swap(name, m.name);
      std::swap(connected, m.connected);
      std::swap(db, m.db);
    }
    std::string path;
    std::string name;
    bool connected = false;
    sqlite3* db = nullptr;
    const static std::map<uint32_t, DBKey> keys;
};

#endif // BHH_ASSET_PACKER_NO_SQLITE_IMPLEMENTATION

} // bhh
