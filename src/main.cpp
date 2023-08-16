#include <iostream>
#include <drogon/HttpAppFramework.h>
#include <yutovo_logger/logger.h>

using namespace yutovo;

int main(int argc, char *argv[])
{
    Logger* logger = Logger::GetInstance(std::string(std::getenv("YUTOVO_DEPLOY")) + "/log", "server", true, true);
    logger->Info("Yutovo server start");

    drogon::app().addListener("0.0.0.0", 9001).loadConfigFile("config.json");
    drogon::app().enableSession();
    drogon::app().run();

    logger->Info("Yutovo server finish");
    return 0;
}
