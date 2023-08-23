#include "mock.h"

namespace yutovo_server_test
{

int argc = 0;
char** argv = nullptr;

//AuthTest

void AuthTest::Register(HttpClientPtr client, std::string login, std::string email, std::string password)
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

void AuthTest::UnRegister(HttpClientPtr client, std::string login, std::string& access_token)
{
    Json::Value body;
    body["login"] = login;
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/register");
    req->addHeader("access_token", access_token);
    req->setPath("/auth/unregister");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_TEXT_PLAIN) << r->getContentType();
}

}
