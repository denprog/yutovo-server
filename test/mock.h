#ifndef __MOCK_H__
#define __MOCK_H__

#include <gmock/gmock.h>
#include <drogon/drogon.h>

namespace yutovo_server_test
{

using namespace drogon;

typedef unsigned int uint;

extern int argc;
extern char** argv;

struct TestBase
{
    void Register(HttpClientPtr client, std::string login, std::string email, std::string password);
    void UnRegister(HttpClientPtr client, std::string login, std::string& access_token);
};

struct AuthTest : public testing::Test, TestBase
{
};

struct SessionTest : public testing::Test, TestBase
{
};

struct ServiceTest : public testing::Test, TestBase
{
};

}

#endif
