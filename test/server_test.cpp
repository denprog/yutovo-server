#include <gtest/gtest.h>
#include "mock.h"

namespace yutovo_server_test
{

using namespace drogon;
using namespace std::chrono_literals;

//Register, login and delete a user
TEST_F(ServerTest, register1)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    Register(client, "User1", "user1@mail.com", "11");

    //login
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setParameter("login", "User1");
    req->setParameter("password", "11");
    req->setPath("/api/login");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    std::string access_token = r->getHeader("access_token");
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_TEXT_PLAIN) << r->getContentType();
    ASSERT_TRUE(r->getCookie("refresh_token").cookieString() != "") << r->getCookie("refresh_token").cookieString();
    ASSERT_TRUE(access_token != "") << access_token;

    UnRegister(client, "User1", access_token);
}

//Register, login and delete a two users
TEST_F(ServerTest, register2)
{
    auto client1 = HttpClient::newHttpClient("http://localhost:9001");
    client1->enableCookies(true);
    auto client2 = HttpClient::newHttpClient("http://localhost:9001");
    client2->enableCookies(true);

    Register(client1, "User1", "user1@mail.com", "11");
    Register(client2, "User2", "user2@mail.com", "22");

    //login User1
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setParameter("login", "User1");
    req->setParameter("password", "11");
    req->setPath("/api/login");

    auto resp = client1->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    std::string access_token1 = r->getHeader("access_token");
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_TEXT_PLAIN) << r->getContentType();
    ASSERT_TRUE(r->getCookie("refresh_token").cookieString() != "") << r->getCookie("refresh_token").cookieString();
    ASSERT_TRUE(access_token1 != "") << access_token1;

    //login User2
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setParameter("login", "User2");
    req->setParameter("password", "22");
    req->setPath("/api/login");

    resp = client2->sendRequest(req);
    res = resp.first;
    r = resp.second;
    std::string access_token2 = r->getHeader("access_token");
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_TEXT_PLAIN) << r->getContentType();
    ASSERT_TRUE(r->getCookie("refresh_token").cookieString() != "") << r->getCookie("refresh_token").cookieString();
    ASSERT_TRUE(access_token2 != "") << access_token2;

    UnRegister(client1, "User1", access_token1);
    UnRegister(client2, "User2", access_token2);
}

//Wrong access token
TEST_F(ServerTest, register3)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->addHeader("access_token", "12345");
    req->setPath("/api/unregister");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k400BadRequest) << r->getStatusCode();
}

//Login and logout
TEST_F(ServerTest, login1)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    Register(client, "User1", "user1@mail.com", "11");

    //login
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setParameter("login", "User1");
    req->setParameter("password", "11");
    req->setPath("/api/login");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    std::string access_token = r->getHeader("access_token");
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_TEXT_PLAIN) << r->getContentType();
    ASSERT_TRUE(r->getCookie("refresh_token").cookieString() != "") << r->getCookie("refresh_token").cookieString();
    ASSERT_TRUE(access_token != "") << access_token;

    //logout
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->addHeader("access_token", access_token);
    req->setPath("/api/logout");

    resp = client->sendRequest(req);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_TEXT_PLAIN) << r->getContentType();

    //login back for unregister
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setParameter("login", "User1");
    req->setParameter("password", "11");
    req->setPath("/api/login");
    resp = client->sendRequest(req);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_TEXT_PLAIN) << r->getContentType();

    UnRegister(client, "User1", access_token);
}

//Wrong login
TEST_F(ServerTest, login2)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    Register(client, "User1", "user1@mail.com", "11");

    //login with wrong login
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setParameter("login", "User2");
    req->setParameter("password", "11");
    req->setPath("/api/login");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    std::string access_token = r->getHeader("access_token");
    auto cookie = r->getCookie("refresh_token");
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k401Unauthorized) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    ASSERT_TRUE(cookie.path() == "") << cookie.path();
    ASSERT_TRUE(access_token == "") << access_token;

    //correct login
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setParameter("login", "User1");
    req->setParameter("password", "11");
    req->setPath("/api/login");

    resp = client->sendRequest(req);
    res = resp.first;
    r = resp.second;
    access_token = r->getHeader("access_token");
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_TEXT_PLAIN) << r->getContentType();
    ASSERT_TRUE(r->getCookie("refresh_token").cookieString() != "") << r->getCookie("refresh_token").cookieString();
    ASSERT_TRUE(access_token != "") << access_token;

    UnRegister(client, "User1", access_token);
}

//Wrong password
TEST_F(ServerTest, login3)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    Register(client, "User1", "user1@mail.com", "11");

    //login with wrong login
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setParameter("login", "User2");
    req->setParameter("password", "22");
    req->setPath("/api/login");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    std::string access_token = r->getHeader("access_token");
    auto cookie = r->getCookie("refresh_token");
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k401Unauthorized) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    ASSERT_TRUE(cookie.path() == "") << cookie.path();
    ASSERT_TRUE(access_token == "") << access_token;

    //correct login
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setParameter("login", "User1");
    req->setParameter("password", "11");
    req->setPath("/api/login");

    resp = client->sendRequest(req);
    res = resp.first;
    r = resp.second;
    access_token = r->getHeader("access_token");
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_TEXT_PLAIN) << r->getContentType();
    ASSERT_TRUE(r->getCookie("refresh_token").cookieString() != "") << r->getCookie("refresh_token").cookieString();
    ASSERT_TRUE(access_token != "") << access_token;

    UnRegister(client, "User1", access_token);
}

}
