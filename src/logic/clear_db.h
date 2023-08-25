#ifndef __CLEAR_DB_H__
#define __CLEAR_DB_H__

#include <thread>
#include <yutovo_logger/logger.h>

namespace yutovo_server
{

using namespace yutovo;

class ClearDb
{
public:
    ClearDb();
    ~ClearDb();

    void ClearDbThread();

private:
    std::thread clear_db_thread;
    int clear_db_timeout = 0; //seconds
    Logger* logger = Logger::GetInstance(std::string(std::getenv("YUTOVO_DEPLOY")) + "/log", "server", true, true);
    bool exit = false;
};
}

#endif
