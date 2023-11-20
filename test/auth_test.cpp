#include <gtest/gtest.h>
#include "mock.h"

namespace yutovo_server_test
{

using namespace drogon;
using namespace std::chrono_literals;

//Register, login and delete a user
TEST_F(AuthTest, register1)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    Register(client, "User1", "user1@mail.com", "11");

    //login
    Json::Value body;
    body["login"] = "User1";
    body["password"] = "11";
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/login");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    std::string access_token = r->getHeader("access_token");
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    ASSERT_TRUE(r->getCookie("refresh_token").cookieString() != "") << r->getCookie("refresh_token").cookieString();
    ASSERT_TRUE(access_token != "") << access_token;

    UnRegister(client, "User1", access_token);
}

//Register, login and delete a two users
TEST_F(AuthTest, register2)
{
    auto client1 = HttpClient::newHttpClient("http://localhost:9001");
    client1->enableCookies(true);
    auto client2 = HttpClient::newHttpClient("http://localhost:9001");
    client2->enableCookies(true);

    Register(client1, "User1", "user1@mail.com", "11");
    Register(client2, "User2", "user2@mail.com", "22");

    //login User1
    Json::Value body;
    body["login"] = "User1";
    body["password"] = "11";
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/login");

    auto resp = client1->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    std::string access_token1 = r->getHeader("access_token");
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    ASSERT_TRUE(r->getCookie("refresh_token").cookieString() != "") << r->getCookie("refresh_token").cookieString();
    ASSERT_TRUE(access_token1 != "") << access_token1;

    //login User2
    body["login"] = "User2";
    body["password"] = "22";
    req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/login");

    resp = client2->sendRequest(req);
    res = resp.first;
    r = resp.second;
    std::string access_token2 = r->getHeader("access_token");
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    ASSERT_TRUE(r->getCookie("refresh_token").cookieString() != "") << r->getCookie("refresh_token").cookieString();
    ASSERT_TRUE(access_token2 != "") << access_token2;

    UnRegister(client1, "User1", access_token1);
    UnRegister(client2, "User2", access_token2);
}

//Wrong access token
TEST_F(AuthTest, register3)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    auto req = HttpRequest::newHttpJsonRequest(Json::Value{});
    req->setMethod(drogon::Post);
    req->addHeader("access_token", "12345");
    req->setPath("/auth/unregister");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k400BadRequest) << r->getStatusCode();
}

//Login and logout
TEST_F(AuthTest, login1)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    Register(client, "User1", "user1@mail.com", "11");

    //login
    Json::Value body;
    body["login"] = "User1";
    body["password"] = "11";
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/login");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    std::string access_token = r->getHeader("access_token");
    const Cookie refresh_token = r->getCookie("refresh_token");
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    ASSERT_TRUE(refresh_token.cookieString() != "") << refresh_token.cookieString();
    ASSERT_TRUE(access_token != "") << access_token;

    //logout
    body["login"] = "User1";
    req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/logout");
    req->addHeader("access_token", access_token);
    client->addCookie(refresh_token);

    resp = client->sendRequest(req);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_TEXT_PLAIN) << r->getContentType();

    //login back for unregister
    body["login"] = "User1";
    body["password"] = "11";
    req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/login");

    resp = client->sendRequest(req);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();

    UnRegister(client, "User1", access_token);
}

//Wrong login
TEST_F(AuthTest, login2)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    Register(client, "User1", "user1@mail.com", "11");

    //login with wrong login
    Json::Value body;
    body["login"] = "User2";
    body["password"] = "11";
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/login");

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
    body["login"] = "User1";
    body["password"] = "11";
    req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/login");

    resp = client->sendRequest(req);
    res = resp.first;
    r = resp.second;
    access_token = r->getHeader("access_token");
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    ASSERT_TRUE(r->getCookie("refresh_token").cookieString() != "") << r->getCookie("refresh_token").cookieString();
    ASSERT_TRUE(access_token != "") << access_token;

    UnRegister(client, "User1", access_token);
}

//Wrong password
TEST_F(AuthTest, login3)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    Register(client, "User1", "user1@mail.com", "11");

    //login with wrong login
    Json::Value body;
    body["login"] = "User2";
    body["password"] = "22";
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/login");

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
    body["login"] = "User1";
    body["password"] = "11";
    req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/login");

    resp = client->sendRequest(req);
    res = resp.first;
    r = resp.second;
    access_token = r->getHeader("access_token");
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    ASSERT_TRUE(r->getCookie("refresh_token").cookieString() != "") << r->getCookie("refresh_token").cookieString();
    ASSERT_TRUE(access_token != "") << access_token;

    UnRegister(client, "User1", access_token);
}

//Refresh session
TEST_F(AuthTest, refresh_session1)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    Register(client, "User1", "user1@mail.com", "11");

    //login
    Json::Value body;
    body["login"] = "User1";
    body["password"] = "11";
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/login");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    std::string access_token1 = r->getHeader("access_token");
    const Cookie refresh_token1 = r->getCookie("refresh_token");
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    //refresh session
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->addHeader("access_token", access_token1);
    client->addCookie(refresh_token1);
    req->setPath("/auth/refresh-token");

    resp = client->sendRequest(req);
    res = resp.first;
    r = resp.second;
    std::string access_token2 = r->getHeader("access_token");
    const Cookie refresh_token2 = r->getCookie("refresh_token");
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(refresh_token1.cookieString() != refresh_token2.cookieString());
    ASSERT_TRUE(access_token1 != access_token2);

    UnRegister(client, "User1", access_token2);
}

//Check expired session
TEST_F(AuthTest, refresh_session2)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    //set very short expire time for the tokens
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setParameter("access_token_expires", "1");
    req->setParameter("refresh_token_expires", "10");
    req->setPath("/auth/set-params");
    auto resp = client->sendRequest(req);
    auto r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    Register(client, "User1", "user1@mail.com", "11");

    //login
    Json::Value body;
    body["login"] = "User1";
    body["password"] = "11";
    req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/login");

    resp = client->sendRequest(req);
    r = resp.second;
    std::string access_token = r->getHeader("access_token");
    const Cookie refresh_token = r->getCookie("refresh_token");
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    std::this_thread::sleep_for(2s); //wait until the access token expires

    req = HttpRequest::newHttpJsonRequest(Json::Value{});
    req->setMethod(drogon::Post);
    req->addHeader("access_token", access_token);
    req->setPath("/auth/unregister");

    resp = client->sendRequest(req);
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k403Forbidden) << r->getStatusCode();

    //return the expire timeouts
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setParameter("access_token_expires", "120");
    req->setParameter("refresh_token_expires", "86400");
    req->setPath("/auth/set-params");
    resp = client->sendRequest(req);
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    std::this_thread::sleep_for(2s);

    //login once again
    body["login"] = "User1";
    body["password"] = "11";
    req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/login");

    resp = client->sendRequest(req);
    r = resp.second;
    access_token = r->getHeader("access_token");
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    UnRegister(client, "User1", access_token);
}

}
