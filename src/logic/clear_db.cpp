/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "clear_db.h"
#include <chrono>
#include <drogon/orm/DbClient.h>
#include <drogon/HttpAppFramework.h>

namespace yutovo_server
{

using namespace std::chrono_literals;
using namespace std::chrono;
using namespace drogon;

//ClearDb

ClearDb::ClearDb() :
    clear_db_thread(std::thread(&ClearDb::ClearDbThread, this))
{
    const Json::Value& v = app().getCustomConfig();
    clear_db_timeout = v.get("clear_db_timeout", 60).asInt();
}

ClearDb::~ClearDb()
{
    exit = true;
    clear_db_thread.join();
}

ClearDb* ClearDb::GetInstance()
{
    static ClearDb clear_db;
    return &clear_db;
}

void ClearDb::TurnOn()
{
    std::unique_lock<std::mutex> lock(turn_on_mutex);
    turn_on = true;
}

void ClearDb::TurnOff()
{
    std::unique_lock<std::mutex> lock(turn_on_mutex);
    turn_on = false;
}

void ClearDb::ClearDbThread()
{
    auto now = trantor::Date::now();
    orm::DbClientPtr db = app().getDbClient();

    while (!exit)
    {
        auto t = trantor::Date::now();

        {
            std::unique_lock<std::mutex> lock(turn_on_mutex);
            if (t.secondsSinceEpoch() - now.secondsSinceEpoch() >= clear_db_timeout && turn_on)
            {
                //check DB and remove expired sessions
                now = trantor::Date::now();
                try
                {
                    int64_t s = now.secondsSinceEpoch();
                    db->execSqlSync("delete from refresh_sessions where expire_time<=$1", s);
                    db->execSqlSync("delete from user_sessions where expire_time<=$1", s);
                    db->execSqlSync("delete from user_documents where (user_id=-1 and document_id not in (select document_id from user_sessions))"\
                        " or (user_id<>-1 and user_id not in (select user_id from users))");
                }
                catch (const orm::DrogonDbException& e)
                {
                    logger->Error("Database error: {}", e.base().what());
                }
                now = trantor::Date::now();
            }
        }

        std::this_thread::sleep_for(1s);
    }
}

//ClearDbTurnOff

ClearDbTurnOff::ClearDbTurnOff() : 
    clear_db(yutovo_server::ClearDb::GetInstance())
{
    clear_db->TurnOff();
}

ClearDbTurnOff::~ClearDbTurnOff()
{
    clear_db->TurnOn();
}

}
