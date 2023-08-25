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

//Create a session without login and later log into it
TEST_F(SessionTest, session4)
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

    drogon::Cookie session_cookie = c;

    Register(client, "User1", "user1@mail.com", "11");

    Json::Value body;
    body["login"] = "User1";
    body["password"] = "11";
    req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/login");

    resp = client->sendRequest(req);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    std::string access_token = r->getHeader("access_token");
    c = r->getCookie("user_session");
    ASSERT_TRUE(c.value() == session_cookie.value());

    orm::DbClientPtr db = app().getDbClient();
    orm::Result result = db->execSqlSync("select user_id from user_sessions where session_id=$1", c.value());
    ASSERT_TRUE(result.size() != 0);
    auto row = result[0];
    auto user_id = row["user_id"].as<int>();
    ASSERT_TRUE(user_id > 0);

    UnRegister(client, "User1", access_token);
    result = db->execSqlSync("select user_id from user_sessions where session_id=$1", c.value());
    ASSERT_TRUE(result.size() == 0);
    result = db->execSqlSync("select 1 from user_sessions where user_id=$1", user_id);
    ASSERT_TRUE(result.size() == 0);
    result = db->execSqlSync("select 1 from users where user_id=$1", user_id);
    ASSERT_TRUE(result.size() == 0);
}

}
