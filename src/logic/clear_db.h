#ifndef __CLEAR_DB_H__
#define __CLEAR_DB_H__

#include <thread>
#include <yutovo_logger/logger.h>

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
    Logger* logger = Logger::GetInstance(std::string(std::getenv("YUTOVO_DEPLOY")) + "/log", "server", true, true);
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
