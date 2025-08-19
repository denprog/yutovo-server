/*
 * Yutovo Server
 * Copyright (C) 2022-2025 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef __CLEAR_DB_H__
#define __CLEAR_DB_H__

#include <thread>
#include <yutovo-logger/logger.h>
#include "utils.h"

namespace yutovo_server
{

using namespace yutovo;

class ClearDb
{
private:
    ClearDb();
    ~ClearDb();

public:
    ClearDb(Logger const&) = delete;
    void operator=(ClearDb const&) = delete;

    static ClearDb* GetInstance();

    void TurnOn();
    void TurnOff();

private:
    void ClearDbThread();

private:
    std::thread clear_db_thread;
    int clear_db_timeout = 0; //seconds
    Logger* logger = Logger::GetInstance(GetDeployPath() + "/log/yutovo_server/", "server", true, true);
    bool exit = false;
    std::atomic<bool> turn_on = true;
    std::mutex turn_on_mutex;
};

class ClearDbTurnOff
{
public:
    ClearDbTurnOff();
    ~ClearDbTurnOff();

private:
    yutovo_server::ClearDb* clear_db;
};
}

#endif
