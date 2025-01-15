#include <iostream>
#include <drogon/HttpAppFramework.h>
#include <yutovo_logger/logger.h>
#include "logic/clear_db.h"

using namespace yutovo;
using namespace std::chrono_literals;

int main(int argc, char *argv[])
{
    Logger* logger = Logger::GetInstance(std::string(std::getenv("YUTOVO_DEPLOY")) + "/log/yutovo_server/", "server", true, true);
    logger->Info("Yutovo server start");

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
                drogon::app().addListener("0.0.0.0", 9001).loadConfigFile("config.json");
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
    return 0;
}
