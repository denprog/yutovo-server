#include "mock.h"

namespace yutovo_server_test
{

int argc = 0;
char** argv = nullptr;

//ServerTest

void ServerTest::Register(HttpClientPtr client, std::string login, std::string email, std::string password)
{
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setParameter("login", login);
    req->setParameter("email", email);
    req->setParameter("password", password);
    req->setPath("/auth/register");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_TEXT_PLAIN) << r->getContentType();
}

void ServerTest::UnRegister(HttpClientPtr client, std::string login, std::string& access_token)
{
    auto req = HttpRequest::newHttpRequest();
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

}
