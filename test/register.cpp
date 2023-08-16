#include <gtest/gtest.h>
#include "mock.h"

namespace yutovo_server_test
{

using namespace drogon;
using namespace std::chrono_literals;

//Register, login and delete a user
TEST_F(RegisterTest, register1)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);
    Cookie cookie("", "");
    client->addCookie(cookie);

    //register
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setParameter("login", "User1");
    req->setParameter("email", "user1@mail.com");
    req->setParameter("password", "11");
    req->setPath("/api/register");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_TEXT_PLAIN) << r->getContentType();

    //login
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setParameter("login", "User1");
    req->setParameter("password", "11");
    req->setPath("/api/login");

    resp = client->sendRequest(req);
    res = resp.first;
    r = resp.second;
    std::string access_token = r->getHeader("access_token");
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_TEXT_PLAIN) << r->getContentType();
    ASSERT_TRUE(r->getCookie("refresh_token").cookieString() != "") << r->getCookie("refresh_token").cookieString();
    ASSERT_TRUE(access_token != "") << access_token;

    //unregister
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setParameter("login", "User1");
    req->addHeader("access_token", access_token);
    req->setPath("/api/unregister");
    resp = client->sendRequest(req);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_TEXT_PLAIN) << r->getContentType();
}

}
