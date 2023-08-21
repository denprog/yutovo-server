#include <gtest/gtest.h>
#include "mock.h"

namespace yutovo_server_test
{

using namespace drogon;
using namespace std::chrono_literals;

//Create a session without login by access on "/"
TEST_F(SessionTest, session1)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k302Found) << r->getStatusCode();

    std::string location = r->getHeader("location");
    ASSERT_TRUE(location.find("/session/") != std::string::npos);

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath(location);
    resp = client->sendRequest(req);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
}

//Create a session without login and later access it by cookie
TEST_F(SessionTest, session2)
{
    drogon::Cookie session_cookie;
    {
        auto client = HttpClient::newHttpClient("http://localhost:9001");
        client->enableCookies(true);

        auto req = HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);
        req->setPath("/");

        auto resp = client->sendRequest(req);
        ReqResult& res = resp.first;
        HttpResponsePtr& r = resp.second;
        ASSERT_TRUE(res == ReqResult::Ok) << res;
        ASSERT_TRUE(r->getStatusCode() == k302Found) << r->getStatusCode();
        auto c = r->getCookie("user_session");
        ASSERT_TRUE(c.value() != "");
        session_cookie = c;
    }

    {
        //access by the saved cookie
        auto client = HttpClient::newHttpClient("http://localhost:9001");
        client->enableCookies(true);
        client->addCookie(session_cookie);

        auto req = HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);
        req->setPath("/");

        auto resp = client->sendRequest(req);
        ReqResult& res = resp.first;
        HttpResponsePtr& r = resp.second;
        ASSERT_TRUE(res == ReqResult::Ok) << res;
        ASSERT_TRUE(r->getStatusCode() == k302Found) << r->getStatusCode();

        std::string location = r->getHeader("location");
        ASSERT_TRUE(location.find("/session/" + session_cookie.value()) != std::string::npos);

        req = HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);
        req->setPath(location);
        resp = client->sendRequest(req);
        res = resp.first;
        r = resp.second;
        ASSERT_TRUE(res == ReqResult::Ok) << res;
        ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    }
}

//Create a session without login and later access it by its url
TEST_F(SessionTest, session3)
{
    drogon::Cookie session_cookie;
    {
        auto client = HttpClient::newHttpClient("http://localhost:9001");
        client->enableCookies(true);

        auto req = HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);
        req->setPath("/");

        auto resp = client->sendRequest(req);
        ReqResult& res = resp.first;
        HttpResponsePtr& r = resp.second;
        ASSERT_TRUE(res == ReqResult::Ok) << res;
        ASSERT_TRUE(r->getStatusCode() == k302Found) << r->getStatusCode();
        auto c = r->getCookie("user_session");
        ASSERT_TRUE(c.value() != "");
        session_cookie = c;
    }

    {
        //access by the session url
        auto client = HttpClient::newHttpClient("http://localhost:9001");
        client->enableCookies(true);

        auto req = HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);
        req->setPath("/session/" + session_cookie.value());

        auto resp = client->sendRequest(req);
        ReqResult& res = resp.first;
        HttpResponsePtr& r = resp.second;
        ASSERT_TRUE(res == ReqResult::Ok) << res;
        ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    }
}

}
