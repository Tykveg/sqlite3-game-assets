#include "bhh-asset-packer.hpp"
#include <str_obfuscator.hpp>
#include <filesystem>
#include <iostream>
#include <vector>
#include <string>

static const std::string error_prefix = "ERROR! ";

template<class T>
bool expectEQ(const T& expected, const T& real, bool& ret) {
  if (!(expected == real)) {
    std::cout << error_prefix << "Expected:\n" << expected << "\nBut actually got:\n" << real << '\n';
    ret = false;
    return false;
  } else {
    return true;
  }
}

bool testCase(const std::string& db_name) {
  bool ret = true;
  bhh::AssetManager db(db_name);
  std::vector<std::string> tables = { BHH_CRYPT_STR("sound"), BHH_CRYPT_STR("sprites") };
  std::vector<std::string> files = { BHH_CRYPT_STR("file1.txt"), BHH_CRYPT_STR("file2.txt"), BHH_CRYPT_STR("dir/file.txt") };
  std::cout << "Test case for: " << db.getName() << '\n';
  for (const auto& table : tables) {
    for (const auto& file : files) {
      try {
        bhh::Asset tmp = db.get(table, file);
        std::string result = static_cast<const char*>(tmp.getBlob());
        expectEQ<std::string>("Lorem ipsum " + db.getName() + '/' + table + '/' + file + '\n', result, ret);
      } catch(const std::exception& e) {
        std::cout << error_prefix << "Got an exception, but the test will be continued! " << e.what() << '\n';
        ret = false;
      }
    }
  }
  return ret;
}

int main(int argc, char* argv[]) {
  // https://stackoverflow.com/a/55579815
  // Using argv[0] is good enough on tests.
  std::string full_path = std::filesystem::weakly_canonical(std::filesystem::path(argv[0])).remove_filename();
  std::vector<std::string> dbs = { full_path + BHH_CRYPT_STR("db1"), full_path + BHH_CRYPT_STR("db2") };
  int ret = 0;
  for (const auto& db : dbs) {
    bool is_good = true;
    std::cout << "Testing " << db << '\n';
    try {
      if (!testCase(db)) {
        is_good = false;
        ret = -1;
      }
    } catch (const std::exception& e) {
      std::cout << error_prefix << "Got an unhandled exception! " << e.what() << '\n';
      is_good = false;
      ret = -1;
    }
    if (is_good) {
      std::cout << "All tests passed!\n";
    } else {
      std::cout << "Some tests are failed!\n";
    }
    std::cout << "----------------------------\n";
  }
  return ret;
}
