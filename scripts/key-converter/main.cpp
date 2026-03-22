#include "bhh-asset-packer.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <filesystem>

/**
 * Prints result, if there is no key file.
 * @note `{crc32, {}}`.
 *
 * @param name_crc32 crc32 hash.
*/
inline void print_no_key(const uint32_t& name_crc32) {
  std::cout << "{" << name_crc32 << "u,{}},";
}

/**
 * The purpose of this program is to print an encryption key, that got from key file.
 * Basically prints `{crc32_hash,{key_as_uchar_array}}`.
 * If fails to open the key file (even if it exist), defaults to no encryption key.
 * @note This program is meant to use internally within the library.
 *
 * @param argv Path for dir for db.
 * @return If everything is good, returns 0 and prints the result. If no path was givin as an argument, returns -1 and prints the error.
*/
int main(int argc, char* argv[]) {
  if (argc <= 1) {
    std::cout << "Need at least 1 argument which is DB dir path\n";
    return -1;
  }
  const std::filesystem::path path(argv[1]);
  uint32_t name_crc32 = bhh::crc32(path.filename().string());
  std::vector<unsigned char> buffer;
  try {
    bhh::crc32ForFile((path / bhh::key_file_path).string(), buffer);
  } catch (const std::runtime_error&) {}
  if (buffer.empty()) {
    print_no_key(name_crc32);
    return 0;
  }
  std::cout << "{" << name_crc32 << "u,{";
  for (const auto& e : buffer) {
    std::cout << +e << ',';
  }
  std::cout << "}},";
  return 0;
}
