#include "bhh-asset-packer.hpp"
#include "sqlite3mc_amalgamation.h"
#include <iostream>
#include <string>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <map>

namespace fs = std::filesystem;

// Just an alias for using reference to a pointer.
using sqlite3_ptr = sqlite3*;

// A counter so that handleDBConnection recursion (when doing something with keys) will be limited (should never go deeper than 5 levels (or even 2)).
int connection_recursion_calls = 0;

#ifndef BHH_ASSET_PACKER_DB_SAVE_PATH
#warning BHH_ASSET_PACKER_DB_SAVE_PATH is undefined so the save path for data bases will be ""
#define BHH_ASSET_PACKER_DB_SAVE_PATH ""
#endif // BHH_ASSET_PACKER_DB_SAVE_PATH

const std::map<uint32_t, bhh::DBKey> keys = {
  BHH_ASSET_PACKER_ALL_KEYS
};

/**
 * Gets key from DBKey.
 * By the C++ standard, for an empty vector it is not guaranteed, that `data()` method will return nullptr.
 * Because I need nullptr when using `data()` on an empty vector, I have to wrap this method into a function.
 * So expect this function to behave like `data()` method, that returns nullptr on empty vectors.
 * And everything is const because I don't use it in non-const situations.
 *
 * @param key The key.
 * @return Pointer to key data or nullptr if key is empty.
*/
const unsigned char* extractKey(const bhh::DBKey& key) noexcept {
  if (!key.empty()) {
    return key.data();
  } else {
    return nullptr;
  }
}

/**
 * Replaces substring `what` with substring `with` in string `s`.
 * @note Modifies string s.
 * @note On exceptions does not modify the string. Only modifies on successful replace.
 * @note Complexity O(`s.size`), memory O(`s.size`).
 * @throws std::runtime_error Throws if for whatever reason while loop had too many iterations (more than `s.size()`). Should never happen.
*/
void replaceAll(std::string& s, const std::string& what, const std::string& with) {
  // https://stackoverflow.com/questions/5878775/how-to-find-and-replace-string/5878802#5878802
  std::string buf;
  std::size_t pos = 0;
  std::size_t prev_pos;
  buf.reserve(s.size());
  std::size_t i = 0;
  // let the while loop have upper bound for safety.
  std::size_t i_max = s.size() + 1;
  while (i < i_max) {
    prev_pos = pos;
    pos = s.find(what, pos);
    if (pos == std::string::npos) {
      break;
    }
    buf.append(s, prev_pos, pos - prev_pos);
    buf += with;
    pos += with.size();
    i++;
  }
  if (i == i_max) {
    throw std::runtime_error("replaceAll() had too many loop iterations. Something is wrong.");
  }
  buf.append(s, prev_pos, s.size() - prev_pos);
  s.swap(buf);
}

/**
 * Creates an empty file at path `path`.
 *
 * @param path Path for file.
 * @throws std::runtime_error Throws if unable to open the file for write for whatever reason.
*/
void createEmptyFile(const std::string& path) {
  std::ofstream f;
  f.open(path);
  if (!f.is_open()) {
    f.close();
    throw std::runtime_error("Cannot open the file for write: " + path);
  }
  f.close();
}

/**
 * Adds file to `db` at table `table_name`.
 *
 * @param db db
 * @param file_path Path to file.
 * @param file_name File name, that will be `filename` column in db.
 * @param table_name Table's name for current db.
 * @param is_new_db Flag if it's an unpopulated db.
 * @throws std::runtime_error Thrown if failed to open the file or db is failed to add the file.
*/
void addFile(sqlite3_ptr& db, const fs::path& file_path, const std::string& file_name, const std::string& table_name, const bool& is_new_db = false) {
  std::cout << "Handling file " << file_path << " with name " << file_name << '\n';
  std::vector<unsigned char> buffer;
  uint32_t file_crc32 = bhh::crc32ForFile(file_path.string(), buffer);
  std::string comm = "INSERT INTO " + table_name + "(filename, data, hash, processed) VALUES (?, ?, ?, TRUE)";
  sqlite3_stmt* stmt = nullptr;
  bool is_update = false;
  int rc;
  if (!is_new_db) {
    try {
      std::string comm2 = "SELECT hash FROM " + table_name + " WHERE filename = ?";
      sqlite3_prepare_v2(db, comm2.c_str(), comm2.size() + 1, &stmt, nullptr);
      sqlite3_bind_text(stmt, 1, file_name.c_str(), file_name.size(), SQLITE_STATIC);
      rc = sqlite3_step(stmt);
      if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
        throw bhh::SQLiteError("Error searching inside db for file: " + file_name, rc);
      }
      if (rc == SQLITE_ROW) {
        uint32_t cur_crc = sqlite3_column_int(stmt, 0);
        if (cur_crc == file_crc32) {
          sqlite3_finalize(stmt);
          stmt = nullptr;
          comm = "UPDATE " + table_name + " SET processed = TRUE WHERE filename = ?";
          sqlite3_prepare_v2(db, comm.c_str(), comm.size() + 1, &stmt, nullptr);
          sqlite3_bind_text(stmt, 1, file_name.c_str(), file_name.size(), SQLITE_STATIC);
          if ((rc = sqlite3_step(stmt)) != SQLITE_DONE) {
            throw bhh::SQLiteError("Error settings processed to true (should never happen) on file: " + file_name, rc);
          }
          sqlite3_finalize(stmt);
          std::cout << "Skipping the file " << file_name << " because it's already presented\n";
          return;
        } else {
          std::cout << "File " << file_name << " is already presented, but has been changed so updating\n";
          comm = "UPDATE " + table_name + " SET filename = ?, data = ?, hash = ?, processed = TRUE WHERE filename = ?";
          is_update = true;
        }
      }
      sqlite3_finalize(stmt);
      stmt = nullptr;
    } catch(...) {
      sqlite3_finalize(stmt);
      throw;
    }
  }
  try {
    sqlite3_prepare_v2(db, comm.c_str(), comm.size() + 1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, file_name.c_str(), file_name.size(), SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, buffer.data(), static_cast<int>(buffer.size()), SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, static_cast<int32_t>(file_crc32));
    if (is_update) {
      sqlite3_bind_text(stmt, 4, file_name.c_str(), file_name.size(), SQLITE_STATIC);
    }
    if ((rc = sqlite3_step(stmt)) != SQLITE_DONE) {
      throw bhh::SQLiteError("Error adding file: " + file_name, rc);
    }
  } catch(...) {
    sqlite3_finalize(stmt);
    throw;
  }
  sqlite3_finalize(stmt);
}

/**
 * Adds dir as a table to db and calls `addFile` for each of the file inside dir.
 * Calls `addFile`.
 *
 * @param db db
 * @param dir_path The path for dir. The dir's name will be a table's name.
 * @param is_new_db Flag if it's an unpopulated db.
 * @throws std::runtime_error Throws if got errors on creating table (should never happen).
*/
void handleDir(sqlite3_ptr& db, const fs::path& dir_path, const bool& is_new_db = false) {
  std::cout << "Handling table " << dir_path << '\n';
  std::string table_name = dir_path.filename().string();
  std::string comm = "CREATE TABLE IF NOT EXISTS " + table_name + " (filename TEXT PRIMARY KEY NOT NULL UNIQUE, data BLOB, hash INT(32) NOT NULL, processed BOOLEAN DEFAULT FALSE);";
  int rc;
  if ((rc = sqlite3_exec(db, comm.c_str(), nullptr, nullptr, nullptr)) != SQLITE_OK) {
    throw bhh::SQLiteError("Error creating table: " + table_name, rc);
  }
  comm = "UPDATE " + table_name + " SET processed = FALSE";
  if ((rc = sqlite3_exec(db, comm.c_str(), nullptr, nullptr, nullptr)) != SQLITE_OK) {
    throw bhh::SQLiteError("Error setting 'processed' to false for all rows for table: " + table_name, rc);
  }
  for (const auto& file : fs::recursive_directory_iterator(dir_path)) {
    if (file.is_regular_file()) {
      const fs::path file_path = file.path();
      std::string file_name = file_path.lexically_relative(dir_path).string();
      if (fs::path::preferred_separator != '/') {
        replaceAll(file_name, std::string(1, fs::path::preferred_separator), std::string(1, '/'));
      }
      addFile(db, file_path, file_name, table_name, is_new_db);
    }
  }
  comm = "DELETE FROM " + table_name + " WHERE processed = FALSE";
  if ((rc = sqlite3_exec(db, comm.c_str(), nullptr, nullptr, nullptr)) != SQLITE_OK) {
    throw bhh::SQLiteError("Error deleting files with processed == false", rc);
  }
}

/**
 * Handles configuration files inside db dir.
 * Uses the following files: `CHUNKS`, bhh::key_file_path
 * For now does literally nothing. Only prints warnings, if it sees unknown files.
 *
 * @param db db
 * @param path Path to dir for db.
 * @throws std::runtime_error Should throw if it sees something wrong with the configuration files, but since it does nothing, it doesn't throw anything.
*/
void handleConfigFiles(sqlite3_ptr& db, const fs::path& path) {
  for (const auto& file : fs::directory_iterator(path)) {
    if (file.is_regular_file()) {
      const fs::path filepath = file.path();
      const std::string filename = filepath.filename();
      if (filename == "CHUNKS") {
        // Unimplemented
      } else if (filename == bhh::key_file_path) {
        // This one should be handled in CMakeLists.txt and the result should be added as compile definition, so ignore it.
      } else {
        std::cout << "Warning! Unknown config file " << filename << ". Skipping...\n";
      }
    }
  }
}

/**
 * Handles db connection.
 * Will add a new db file if db is new or the encryption key is changed.
 * Will use old db file if the file exists (and not empty) and the encryption key worked.
 *
 * @tparam do_announce_db Used internally. Specify if some messages needs to be printed on db creation.
 *         The function calls itself with `false` value when the encryption key is expired and db needs to be re-created.
 * @param db db that will be created.
 * @param dirpath db dir path.
 * @param is_new_db The function will write to this variable true if the db file is unpopulated (so the new file has been created) or false otherwise.
 * @throws std::runtime_error Throws if have troubles opening db file or encrypting (not decrypting) db.
*/
template<bool do_announce_db = true>
void handleDBConnection(sqlite3_ptr& db, const fs::path& dirpath, bool& is_new_db) {
  if (connection_recursion_calls > 5) {
    connection_recursion_calls = 0;
    throw std::runtime_error("handleDBConnection was called too times, something is really wrong.");
  }
  if (do_announce_db) {
    std::cout << "Handling DB at path " << dirpath << '\n';
  }
  if (!fs::is_directory(dirpath)) {
    std::cout << "WARNING!!! Selected path " << dirpath.string() << " is NOT a dir (probably even does not exist). Skipping...\n";
    return;
  }
  const std::string dbpath = std::string(BHH_ASSET_PACKER_DB_SAVE_PATH) + fs::path::preferred_separator + dirpath.filename().string();
  const std::string dbname = dirpath.filename().string();
  const bhh::DBKey& dbkey = bhh::tryFindKey(keys, bhh::crc32(dbname));
  auto retryDBConnection = [&](){
    sqlite3_close(db);
    db = nullptr;
    createEmptyFile(dbpath);
    is_new_db = true;
    // We connect again, but this time the db is guaranteed to be empty (I guess).
    connection_recursion_calls++;
    handleDBConnection<false>(db, dirpath, is_new_db);
  };
  int rc;
  if (do_announce_db) {
    std::cout << "Processing DB file at '" << dbpath << "'\n";
  }
  is_new_db = true;
  if (fs::exists(dbpath) && fs::file_size(dbpath) > 0) {
    is_new_db = false;
  }
  if ((rc = sqlite3_open(dbpath.c_str(), &db)) != SQLITE_OK) {
    throw bhh::SQLiteError("Error opening db " + dbpath, rc);
  }
  if (do_announce_db && dbkey.empty()) {
    std::cout << "Warning! " << dbname << " won't use any encryption!\n";
  }
  if (!dbkey.empty() && !is_new_db && (sqlite3_key(db, extractKey(dbkey), dbkey.size()) != SQLITE_OK)) {
    std::cout << "Warning! Old key failed to decrypt db " << dbname << " so creating a new db file\n";
    retryDBConnection();
    return;
  }
  if (is_new_db && (sqlite3_rekey(db, extractKey(dbkey), dbkey.size()) != SQLITE_OK)) {
    std::cout << "Warning! Old key failed to ecrypt db (possibly the key has been removed) " << dbname << " so creating a new db file\n";
    retryDBConnection();
    return;
  }
  std::cout << "Warning! ";
  if (is_new_db) {
    std::cout << "Creating a new file for DB\n";
  } else {
    std::cout << "Using already generated DB\n";
  }
}

/**
 * Creates db and populates it.
 * Calls `handleDBConnection`, `handleConfigFiles` and then `handleDir`.
 *
 * @param dirpath Path for db.
 * @throws std::runtime_error
*/
void packDB(const std::string& dirpath) {
  sqlite3_ptr db = nullptr;
  std::cout << dirpath << '\n';
  try {
    fs::path _dirpath(dirpath);
    bool is_new_db;
    handleDBConnection(db, _dirpath, is_new_db);
    connection_recursion_calls = 0;
    int tables_count = 0;
    handleConfigFiles(db, _dirpath);
    for (const auto& dir : fs::directory_iterator(_dirpath)) {
      if (dir.is_directory()) {
        handleDir(db, dir.path(), is_new_db);
        tables_count++;
      }
    }
    std::cout << "Handled " << tables_count << " tables!\n\n";
  } catch (...) {
    sqlite3_close(db);
    throw;
  }
}

/**
 * The purpose of this program is to populate a db with files, that will have a file system structure.
 * Main dir - the db itself. Inside main dir - bunch of dirs - the tables. Inside each of those dirs are files - rows in the table.
 * @note This program is meant to use internally within the library.
 * @note Path for saving all dbs is dictated by `BHH_ASSET_PACKER_DB_SAVE_PATH` macro, which defined by cmake.
 *
 * @param argv Paths for each db dir (please use full paths).
*/
int main(int argc, char* argv[]) {
  for (int i = 1; i < argc; i++) {
    packDB(argv[i]);
  }
  return 0;
}
