/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mock.h"
#include <filesystem>
#include <fstream>

using namespace std::chrono_literals;
namespace fs = std::filesystem;

int main(int argc, char** argv)
{
	::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
	yutovo_server_test::argc = argc;
	yutovo_server_test::argv = argv;

    const char* db_name = std::getenv("DB_NAME");
    const char* db_user = std::getenv("DB_USER");
    const char* db_password = std::getenv("DB_PASSWORD");
    if (!db_name || !db_user || !db_password)
    {
        printf("Enviroment variables not found");
        return 1;
    }

    fs::path test_docroot = "test_docroot";
    fs::remove_all(test_docroot);
    fs::create_directories(test_docroot / "downloads");
    fs::create_directories(test_docroot / "icons");
    fs::create_directories(test_docroot / "images" / "logical");

    {
        std::ofstream f(test_docroot / "index.html");
        f << "<html></html>\n";
    }
    std::ofstream(test_docroot / "yutovo.png").close();
    std::ofstream(test_docroot / "icons" / "favicon-16x16.png").close();
    std::ofstream(test_docroot / "images" / "logical" / "not.png").close();

    {
        Json::Value downloads(Json::arrayValue);

        Json::Value windows;
        windows["system"] = "Windows";
        windows["link"] = "/downloads/yutovo-1.7.1.exe";
        windows["version"] = "1.7.1";
        downloads.append(windows);

        Json::Value linux_item;
        linux_item["system"] = "Linux";
        linux_item["link"] = "https://yutovo.com/yutovo-1.7.1.deb";
        linux_item["version"] = "1.7.1";
        downloads.append(linux_item);

        std::ofstream f(test_docroot / "downloads" / "downloads.json");
        Json::StreamWriterBuilder builder;
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        writer->write(downloads, &f);
    }

    std::thread app_thread = std::thread(
        [&]()
        {
            drogon::app().
                createDbClient("postgresql", DB_HOST, DB_PORT, db_name, db_user, db_password, 1, "", "default", false, "", DB_TIMEOUT, false).
                addListener("127.0.0.1", 9001).
                loadConfigFile("config.json");
            std::string doc_root = fs::absolute(test_docroot).string();
            if (!doc_root.empty() && doc_root.back() != '/')
                doc_root += '/';
            drogon::app().setDocumentRoot(doc_root);
            drogon::app().enableSession();
            drogon::app().setThreadNum(1);
            drogon::app().run();
        });
    
	while (!drogon::app().isRunning())
    {
        std::this_thread::sleep_for(10ms);
    }

	int r = RUN_ALL_TESTS();

    fs::remove_all(test_docroot);

    drogon::app().getLoop()->queueInLoop(
        []()
        {
            drogon::app().quit();
        });
    app_thread.join();

	return r;
}
