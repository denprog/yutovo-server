/*
 * Yutovo Server
 * Copyright (C) 2022-2025 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <iostream>
#include <drogon/HttpAppFramework.h>
#include <yutovo-logger/logger.h>
#include "logic/clear_db.h"
#include "utils.h"

using namespace yutovo;
using namespace std::chrono_literals;

int main(int argc, char *argv[])
{
    const char* version = "1.1.7";

    Logger* logger = Logger::GetInstance(yutovo_server::GetDeployPath() + "/log/yutovo-server/", "server", true, true);
    logger->Info("Yutovo server start, version: {}", version);

    Logger* server_logger = Logger::GetInstance(yutovo_server::GetDeployPath() + "/log/yutovo-server/server", "server", true, true);
    server_logger->Info("Yutovo server start, version: {}", version);

    const char* db_host = std::getenv("DB_HOST");
    int db_port = 5432;
    if (std::getenv("DB_PORT"))
        db_port = std::stoi(std::getenv("DB_PORT"));
    const char* db_name = std::getenv("DB_NAME");
    const char* db_user = std::getenv("DB_USER");
    const char* db_password = std::getenv("DB_PASSWORD");
    if (!db_name || !db_user || !db_password)
    {
        logger->Error("Enviroment variables not found");
        return 1;
    }

    std::string document_root;
    for (size_t i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if ((arg == "-d") || (arg == "--document-root"))
        {
            if (i + 1 < argc)
                document_root = argv[++i];
            else
            {
                logger->Critical("--document_root option requires one argument");
                return 1;
            }
        }
    }

    if (!document_root.empty())
        logger->Info("Document root={}", document_root);

    {
        std::thread app_thread = std::thread(
            [&]()
            {
                drogon::app().
                    createDbClient("postgresql", db_host, db_port, db_name, db_user, db_password, 1, "", "default", false, "", DB_TIMEOUT, false).
                    addListener("0.0.0.0", 9001).
                    loadConfigFile("config.json");
                if (!document_root.empty())
                    drogon::app().setDocumentRoot(document_root);
                drogon::app().enableSession();
                drogon::app().run();
            });
        
    	while (!drogon::app().isRunning())
        {
            std::this_thread::sleep_for(10ms);
        }

        yutovo_server::ClearDb* clear_db = yutovo_server::ClearDb::GetInstance();

        app_thread.join();
    }

    logger->Info("Yutovo server finish");
    server_logger->Info("Yutovo server finish");
    return 0;
}
