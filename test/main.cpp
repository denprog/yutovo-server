/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mock.h"

using namespace std::chrono_literals;

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

    std::thread app_thread = std::thread(
        [&]()
        {
            drogon::app().
                createDbClient("postgresql", DB_HOST, DB_PORT, db_name, db_user, db_password, 1, "", "default", false, "", DB_TIMEOUT, false).
                addListener("127.0.0.1", 9001).
                loadConfigFile("config.json");
            drogon::app().enableSession();
            drogon::app().setThreadNum(1);
            drogon::app().run();
        });
    
	while (!drogon::app().isRunning())
    {
        std::this_thread::sleep_for(10ms);
    }

	int r = RUN_ALL_TESTS();

    drogon::app().getLoop()->queueInLoop(
        []()
        {
            drogon::app().quit();
        });
    app_thread.join();

	return r;
}
