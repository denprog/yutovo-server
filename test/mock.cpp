#include "mock.h"
#include <fstream>

namespace yutovo_server_test
{

int argc = 0;
char** argv = nullptr;

//TestBase

TestBase::TestBase()
{
    drogon::orm::DbClientPtr db = drogon::app().getDbClient();
    db->execSqlSync("delete from user_sessions where user_id in (select user_id from users where login='User1' or login='User2')");
    db->execSqlSync("delete from user_documents where user_id in (select user_id from users where login='User1' or login='User2')");
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

// void TestBase::Login(HttpClientPtr client, std::string login, std::string password, std::string& access_token, std::string& document_id)
// {
//     HttpResponsePtr r;
//     Login(client, login, password, r);
//     access_token = r->getHeader("access_token");
//     const auto json = r->jsonObject();
// }

void TestBase::Login(HttpClientPtr client, std::string login, std::string password, std::string& access_token, std::string& document_id, std::string& name)
{
    HttpResponsePtr r;
    Login(client, login, password, r);
    access_token = r->getHeader("access_token");
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    const auto json = r->jsonObject();
    document_id = (*json)["document_id"].asString();
    name = (*json)["name"].asString();
}

void TestBase::NewDocument(HttpClientPtr client, const std::string& access_token, std::string& document_id)
{
    auto req = HttpRequest::newHttpJsonRequest("{}");
    req->setMethod(drogon::Post);
    req->setPath("/service/new-document");
    req->addHeader("access_token", access_token);
    auto resp = client->sendRequest(req, 10);
    auto r = resp.second;
    const auto json = r->jsonObject();
    document_id = (*json)["document_id"].asString();
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
}

void TestBase::SaveDocument(HttpClientPtr client, const std::string file_name, const std::string& access_token, std::string& document_id)
{
    Json::Value save_body;
    std::ifstream f(file_name);
    f >> save_body;
    auto req = HttpRequest::newHttpJsonRequest(save_body);
    req->setMethod(drogon::Post);
    req->setPath("/service/save-document");
    req->addHeader("access_token", access_token);
    auto resp = client->sendRequest(req, 10);
    auto r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    const auto json = r->jsonObject();
    document_id = (*json)["document_id"].asString();
    //SessionPtr session = req->session();
    // auto c = r->getCookie("document_id");
    // document_id = c.value();
    // ASSERT_TRUE(document_id != "");
}

void TestBase::LoadDocument(HttpClientPtr client, const std::string& access_token, const std::string document_id)
{
    Json::Value v;
    Json::Reader reader;
    reader.parse("{\"document_id\":" + document_id + "}", v);
    auto req = HttpRequest::newHttpJsonRequest(v);
    req->setMethod(drogon::Post);
    req->setPath("/service/load-document");
    auto resp = client->sendRequest(req, 10);
    auto r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
}

void TestBase::DeleteDocument(HttpClientPtr client, const std::string& access_token, const std::string document_id)
{
    HttpRequestPtr req;
    if (document_id.empty())
        req = HttpRequest::newHttpJsonRequest("");
    else
    {
        Json::Value v;
        Json::Reader reader;
        reader.parse("{\"document_id\":" + document_id + "}", v);
        req = HttpRequest::newHttpJsonRequest(v);
    }
    req->setMethod(drogon::Post);
    req->setPath("/service/delete-document");
    req->addHeader("access_token", access_token);
    auto resp = client->sendRequest(req, 10);
    auto r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
}

void TestBase::RenameDocument(HttpClientPtr client, const std::string& access_token, const std::string& document_id, const std::string& name)
{
    Json::Value body;
    body["document_id"] = document_id;
    body["name"] = name;
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/rename-document");
    req->addHeader("access_token", access_token);
    auto resp = client->sendRequest(req, 10);
    auto r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
}

void TestBase::GetDocumentName(HttpClientPtr client, const std::string& document_id, std::string& name)
{
    Json::Value body;
    body["document_id"] = document_id;
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/get-document-name");
    auto resp = client->sendRequest(req, 10);
    auto r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    const auto json = r->jsonObject();
    name = (*json)["name"].asString();
}

}
