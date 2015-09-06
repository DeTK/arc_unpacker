#include "fmt/tactics/arc_archive.h"
#include "test_support/archive_support.h"
#include "test_support/catch.hh"
#include "test_support/file_support.h"

using namespace au;
using namespace au::fmt::tactics;

static void do_test(const std::string &input_path)
{
    std::vector<std::shared_ptr<File>> expected_files
    {
        tests::stub_file("123.txt", "123123123123123123123"_b),
        tests::stub_file("abc.txt", "abcdefghijklmnopqrstuvwxyz"_b),
    };

    ArcArchive archive;
    auto actual_files = tests::unpack_to_memory(input_path, archive);
    tests::compare_files(expected_files, actual_files, true);
}

TEST_CASE("Unpacking TACTICS's uncompressed ARC archives works")
{
    do_test("tests/fmt/tactics/files/arc/uncompressed.arc");
}

TEST_CASE("Unpacking TACTICS's compressed ARC archives works")
{
    do_test("tests/fmt/tactics/files/arc/compressed.arc");
}
