#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mock.h"

int main(int argc, char** argv)
{
	::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
	yutovo_server_test::argc = argc;
	yutovo_server_test::argv = argv;

    std::promise<void> p;
    std::future<void> f = p.get_future();

    std::thread app_thread = std::thread(
        [&]()
        {
            p.set_value();
            drogon::app().run();
        });
    
    f.get();

	int r = RUN_ALL_TESTS();

    drogon::app().getLoop()->queueInLoop(
        []()
        {
            drogon::app().quit();
        });
    app_thread.join();

	return r;
}
