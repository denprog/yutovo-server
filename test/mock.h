#ifndef __MOCK_H__
#define __MOCK_H__

#include <gmock/gmock.h>
#include <drogon/drogon.h>

namespace yutovo_server_test
{

typedef unsigned int uint;

extern int argc;
extern char** argv;

struct RegisterTest : public testing::Test
{
    RegisterTest()
    {
        std::promise<void> p;
        std::future<void> f = p.get_future();

        app_thread = std::thread(
            [&]()
            {
                drogon::app().getLoop()->queueInLoop(
                        [&p]()
                        {
                            p.set_value();
                        });
                drogon::app().run();
            });
        
        f.get();
    }

    ~RegisterTest()
    {
        drogon::app().getLoop()->queueInLoop(
            []()
            {
                drogon::app().quit();
            });
        app_thread.join();
    }

    std::thread app_thread;
};

}

#endif
