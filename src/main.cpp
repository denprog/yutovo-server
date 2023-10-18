#include <iostream>
#include <drogon/HttpAppFramework.h>
#include <yutovo_logger/logger.h>
#include "logic/clear_db.h"

using namespace yutovo;
using namespace std::chrono_literals;

int main(int argc, char *argv[])
{
    Logger* logger = Logger::GetInstance(std::string(std::getenv("YUTOVO_DEPLOY")) + "/log", "server", true, true);
    logger->Info("Yutovo server start");

    {
        std::thread app_thread = std::thread(
            [&]()
            {
                drogon::app().addListener("0.0.0.0", 9001).loadConfigFile("config.json");
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
    return 0;
}
