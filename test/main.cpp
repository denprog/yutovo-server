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

    std::thread app_thread = std::thread(
        [&]()
        {
            drogon::app().loadConfigFile("config.json");
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
