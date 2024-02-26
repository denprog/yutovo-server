#include <gtest/gtest.h>
#include "mock.h"

namespace yutovo_server_test
{

using namespace drogon;
using namespace std::chrono_literals;

//Register, login and delete a user
TEST_F(AuthTest, register1)
{
    auto client = HttpClient::newHttpClient(address);
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
    auto client1 = HttpClient::newHttpClient(address);
    client1->enableCookies(true);
    auto client2 = HttpClient::newHttpClient(address);
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
    auto client = HttpClient::newHttpClient(address);
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

//Register with a name
TEST_F(AuthTest, register4)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    Register(client, "User1", "user1@mail.com", "11", "user_name");

    orm::DbClientPtr db = app().getDbClient();
    orm::Result result = db->execSqlSync("select name from users where login='User1'");
    ASSERT_TRUE(result.size() != 0);
    auto row = result[0];
    auto name = row["name"].as<std::string>();
    ASSERT_TRUE(name == "user_name");

    std::string access_token, document_id, language;
    Login(client, "User1", "11", access_token, document_id, name, language);

    UnRegister(client, "User1", access_token);
}

//Login and logout
TEST_F(AuthTest, login1)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    Register(client, "User1", "user1@mail.com", "11");

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");
    auto resp = client->sendRequest(req);

    //login
    Json::Value body;
    body["login"] = "User1";
    body["password"] = "11";
    req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/login");

    resp = client->sendRequest(req);
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
    auto c = r->getCookie("session_id");

    resp = client->sendRequest(req);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_TEXT_PLAIN) << r->getContentType();

    orm::DbClientPtr db = app().getDbClient();
    orm::Result result = db->execSqlSync("select user_id from user_sessions where session_id=$1", c.value());
    ASSERT_TRUE(result.size() != 0);
    auto row = result[0];
    auto user_id = row["user_id"].as<int>();
    ASSERT_TRUE(user_id == -1);

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
    auto client = HttpClient::newHttpClient(address);
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
    auto client = HttpClient::newHttpClient(address);
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

//Login, logout, login, logout
TEST_F(AuthTest, login4)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    Register(client, "User1", "user1@mail.com", "11");

    Locate(client, "/");

    std::string access_token, document_id, name, language;
    Login(client, "User1", "11", access_token, document_id, name, language);
    Logout(client, "User1", access_token);

    Login(client, "User1", "11", access_token, document_id, name, language);
    Logout(client, "User1", access_token);

    Login(client, "User1", "11", access_token, document_id, name, language);

    UnRegister(client, "User1", access_token);
}

//Login, logout, login, logout, check documents
TEST_F(AuthTest, login5)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    Register(client, "User1", "user1@mail.com", "11");
    Register(client, "User2", "user2@mail.com", "22");

    Locate(client, "/");

    std::string access_token, document_id, name, language;
    Login(client, "User1", "11", access_token, document_id, name, language);
    std::string new_document_id;
    NewDocument(client, access_token, new_document_id);
    RenameDocument(client, access_token, new_document_id, "new_name_1");
    Logout(client, "User1", access_token);

    Login(client, "User2", "22", access_token, document_id, name, language);
    NewDocument(client, access_token, new_document_id);
    RenameDocument(client, access_token, new_document_id, "new_name_2");
    Logout(client, "User2", access_token);

    std::string document_id1, document_id2;
    Login(client, "User1", "11", access_token, document_id1, name, language);
    GetDocumentName(client, document_id, name);
    ASSERT_TRUE(name == "new_name_1") << name;
    UnRegister(client, "User1", access_token);

    Login(client, "User2", "22", access_token, document_id2, name, language);
    GetDocumentName(client, document_id, name);
    ASSERT_TRUE(name == "new_name_1") << name;
    ASSERT_TRUE(document_id1 == document_id2);
    UnRegister(client, "User2", access_token);
}

//Refresh session
TEST_F(AuthTest, refresh_session1)
{
    auto client = HttpClient::newHttpClient(address);
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
    auto client = HttpClient::newHttpClient(address);
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

//Set language
TEST_F(AuthTest, set_language1)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    Locate(client, "/");

    Register(client, "User1", "user1@mail.com", "11");

    std::string access_token, document_id, name, language;
    Login(client, "User1", "11", access_token, document_id, name, language);

    Json::Value body;
    body["language"] = "ru_RU";
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/set-language");
    req->addHeader("access_token", access_token);
    auto resp = client->sendRequest(req);
    auto r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    Logout(client, "User1", access_token);
    Login(client, "User1", "11", access_token, document_id, name, language);
    ASSERT_TRUE(language == "ru_RU") << language;

    Json::Value v;
    req = HttpRequest::newHttpJsonRequest(v);
    req->setMethod(drogon::Post);
    req->setPath("/auth/get-language");
    req->addHeader("access_token", access_token);
    resp = client->sendRequest(req);
    r = resp.second;
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    const auto json = r->jsonObject();
    language = (*json)["language"].asString();
    ASSERT_TRUE(language == "ru_RU") << language;

    UnRegister(client, "User1", access_token);
}

}
