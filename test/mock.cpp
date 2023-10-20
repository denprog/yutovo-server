#include "mock.h"

namespace yutovo_server_test
{

int argc = 0;
char** argv = nullptr;

//TestBase

TestBase::TestBase()
{
    drogon::orm::DbClientPtr db = drogon::app().getDbClient();
    db->execSqlSync("delete from user_sessions where user_id in (select user_id from users where login='User1' or login='User2')");
    db->execSqlSync("delete from users where login='User1' or login='User2'");
}

void TestBase::Register(HttpClientPtr client, std::string login, std::string email, std::string password)
{
    Json::Value body;
    body["login"] = login;
    body["email"] = email;
    body["password"] = password;
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/register");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_TEXT_PLAIN) << r->getContentType();
}

void TestBase::UnRegister(HttpClientPtr client, std::string login, std::string& access_token)
{
    Json::Value body;
    body["login"] = login;
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->addHeader("access_token", access_token);
    req->setPath("/auth/unregister");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_TEXT_PLAIN) << r->getContentType();
}

void TestBase::Login(HttpClientPtr client, std::string login, std::string password, HttpResponsePtr& r)
{
    Json::Value login_body;
    login_body["login"] = login;
    login_body["password"] = password;
    auto req = HttpRequest::newHttpJsonRequest(login_body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/login");
    auto resp = client->sendRequest(req, 10);
    ReqResult& res = resp.first;
    r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
}

void TestBase::Login(HttpClientPtr client, std::string login, std::string password, std::string& access_token)
{
    HttpResponsePtr r;
    Login(client, login, password, r);
    access_token = r->getHeader("access_token");
}
}
