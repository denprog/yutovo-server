/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "mock.h"
#include <yutovo-logger/logger.h>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <dirent.h>
#include <unistd.h>

namespace yutovo_server_test
{

using namespace yutovo;

static const std::string cache_dir = "log/test_logger_cache";

void LoggerTest::SetUp()
{
    std::error_code ec;
    std::filesystem::remove_all(cache_dir, ec);
    //a small limit with a zero grace interval forces eviction of the previously created loggers
    Logger::SetCacheLimits(8, std::chrono::milliseconds(0));
}

void LoggerTest::TearDown()
{
    Logger::SetCacheLimits(256, std::chrono::minutes(10));
}

int LoggerTest::OpenLogFiles(const std::string& dir)
{
    std::string prefix = std::filesystem::absolute(dir).string();
    int count = 0;
    DIR* d = opendir("/proc/self/fd");
    if (!d)
        return -1;
    dirent* entry;
    while ((entry = readdir(d)) != nullptr)
    {
        if (entry->d_name[0] == '.')
            continue;
        char target[4096];
        ssize_t len = readlink((std::string("/proc/self/fd/") + entry->d_name).c_str(), target, sizeof(target) - 1);
        if (len <= 0)
            continue;
        target[len] = '\0';
        if (strncmp(target, prefix.c_str(), prefix.size()) == 0)
            ++count;
    }
    closedir(d);
    return count;
}

std::string LoggerTest::ReadLogFile(const std::string& dir)
{
    std::string text;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        std::ifstream f(entry.path());
        std::string s;
        while (std::getline(f, s))
            text += s + "\n";
    }
    return text;
}

//Creating more loggers than the cache limit keeps the number of open files at the limit, the files of the evicted loggers are closed
TEST_F(LoggerTest, cache_eviction)
{
    Logger::GetInstance(cache_dir + "/s0", "server", true, true)->Info("first message");

    for (int i = 1; i <= 12; ++i)
        Logger::GetInstance(cache_dir + "/s" + std::to_string(i), "server", true, true)->Info("filler");

    ASSERT_TRUE(OpenLogFiles(cache_dir) == 8) << OpenLogFiles(cache_dir);

    //an evicted logger is recreated on demand and keeps appending to the same file
    Logger::GetInstance(cache_dir + "/s0", "server", true, true)->Info("second message");
    std::string text = ReadLogFile(cache_dir + "/s0");
    ASSERT_TRUE(text.find("first message") != std::string::npos) << text;
    ASSERT_TRUE(text.find("second message") != std::string::npos) << text;
}

//The same path returns the cached logger instance, a different path gets its own file
TEST_F(LoggerTest, cache_hit)
{
    Logger* logger = Logger::GetInstance(cache_dir + "/hit", "server", true, true);
    ASSERT_TRUE(logger == Logger::GetInstance(cache_dir + "/hit", "server", true, true));

    logger->Info("cache hit message");
    std::string text = ReadLogFile(cache_dir + "/hit");
    ASSERT_TRUE(text.find("cache hit message") != std::string::npos) << text;
}

}
