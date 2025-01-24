//
// Created by shawnfeng on 23-6-27.
//
#include "ulog/file/file.h"

#include <gtest/gtest.h>

using namespace ulog::file;

TEST(file, SplitByExtension) {
  ASSERT_EQ(SplitByExtension("mylog.txt"), std::make_tuple("mylog", ".txt"));
  ASSERT_EQ(SplitByExtension("mylog"), std::make_tuple("mylog", ""));
  ASSERT_EQ(SplitByExtension("mylog."), std::make_tuple("mylog.", ""));
  ASSERT_EQ(SplitByExtension("/dir1/dir2/mylog.txt"), std::make_tuple("/dir1/dir2/mylog", ".txt"));

  ASSERT_EQ(SplitByExtension(".mylog"), std::make_tuple(".mylog", ""));
  ASSERT_EQ(SplitByExtension(".mylog.zst.tar"), std::make_tuple(".mylog", ".zst.tar"));
  ASSERT_EQ(SplitByExtension("a.zst.tar"), std::make_tuple("a", ".zst.tar"));
  ASSERT_EQ(SplitByExtension("/my_folder/a.zst.tar"), std::make_tuple("/my_folder/a", ".zst.tar"));
  ASSERT_EQ(SplitByExtension("/my_folder/a"), std::make_tuple("/my_folder/a", ""));
  ASSERT_EQ(SplitByExtension("my_folder/.mylog"), std::make_tuple("my_folder/.mylog", ""));
  ASSERT_EQ(SplitByExtension("my_folder/.mylog.txt"), std::make_tuple("my_folder/.mylog", ".txt"));

  ASSERT_EQ(SplitByExtension("my_folder/.mylog.txt.zst"), std::make_tuple("my_folder/.mylog", ".txt.zst"));
  ASSERT_EQ(SplitByExtension("/etc/rc.d/somelogfile"), std::make_tuple("/etc/rc.d/somelogfile", ""));
}
