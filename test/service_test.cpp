#include <gtest/gtest.h>
#include <fstream>
#include "mock.h"

namespace yutovo_server_test
{

using namespace drogon;
using namespace std::chrono_literals;

//Get list of tasks
TEST_F(ServiceTest, tasks1)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/service/get-tasks");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
}

//Load a task
TEST_F(ServiceTest, tasks2)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    Json::Value body;
    body["task"] = "/Physics/Dynamics/Elastic force";
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/load-task");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_OCTET_STREAM) << r->getContentType();
}

//Wrong task path
TEST_F(ServiceTest, tasks3)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    Json::Value body;
    body["task"] = "/Physics/Dynamics/../Elastic force";
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/load-task");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k404NotFound) << r->getStatusCode();

    body["task"] = "/Physics/Dynamics/../../../CMakeLists.txt";
    req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/load-task");

    resp = client->sendRequest(req);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k404NotFound) << r->getStatusCode();
}

//List identifiers from the service
TEST_F(ServiceTest, service1)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    Json::Value body;
    body["guid"] = "ac28c4aa-de4b-42da-b8c5-47aca826b608";
    body["code_id"] = 1;
    body["solver_type"] = 1;

    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/list-identifiers");

    auto resp = client->sendRequest(req, 10);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    const auto json = r->jsonObject();
    ASSERT_TRUE((*json)["builtin_functions"].isArray());
    ASSERT_TRUE((*json)["user_functions"].isArray());
    ASSERT_TRUE((*json)["builtin_functions"].isArray());
    ASSERT_TRUE((*json)["user_variables"].isArray());
}

//Save/load a user document in the DB
TEST_F(ServiceTest, document1)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    Register(client, "User1", "user1@mail.com", "11");

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k302Found) << r->getStatusCode();
    auto session_cookie = r->getCookie("user_session");
    ASSERT_TRUE(session_cookie.value() != "");

    Json::Value login_body;
    login_body["login"] = "User1";
    login_body["password"] = "11";
    req = HttpRequest::newHttpJsonRequest(login_body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/login");

    resp = client->sendRequest(req);
    r = resp.second;
    std::string access_token = r->getHeader("access_token");

    std::ifstream f("../tests/files11.yut");

    Json::Value save_body;
    f >> save_body;
    req = HttpRequest::newHttpJsonRequest(save_body);
    req->setMethod(drogon::Post);
    req->setPath("/service/save-document");
    client->addCookie(session_cookie);

    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    Json::Value load_body;
    req = HttpRequest::newHttpJsonRequest(load_body);
    req->setMethod(drogon::Post);
    req->setPath("/service/load-document");

    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    const auto load_json = r->jsonObject();
    ASSERT_TRUE(save_body == *load_json);

    UnRegister(client, "User1", access_token);
}

//Save/load a part of a user document in the DB
TEST_F(ServiceTest, document2)
{
    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    Register(client, "User1", "user1@mail.com", "11");

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k302Found) << r->getStatusCode();
    auto session_cookie = r->getCookie("user_session");
    ASSERT_TRUE(session_cookie.value() != "");

    Json::Value login_body;
    login_body["login"] = "User1";
    login_body["password"] = "11";
    req = HttpRequest::newHttpJsonRequest(login_body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/login");

    resp = client->sendRequest(req);
    r = resp.second;
    std::string access_token = r->getHeader("access_token");

    std::ifstream f("../tests/files11.yut");

    Json::Value save_body;
    f >> save_body;
    req = HttpRequest::newHttpJsonRequest(save_body);
    req->setMethod(drogon::Post);
    req->setPath("/service/save-document");
    client->addCookie(session_cookie);

    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    auto& text = save_body["text"];
    auto& el = text["elements"][0]["elements"][0]["elements"][0];
    el["elements"] = "Replace ";

    Json::Value s;
    s["text"] = el;
    req = HttpRequest::newHttpJsonRequest(s);
    req->setMethod(drogon::Post);
    req->setPath("/service/save-document");

    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    Json::Value load_body;
    load_body["id"] = "0,0,0,0";
    req = HttpRequest::newHttpJsonRequest(load_body);
    req->setMethod(drogon::Post);
    req->setPath("/service/load-document");

    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    const auto load_json = r->jsonObject();
    ASSERT_TRUE(el == *load_json);

    UnRegister(client, "User1", access_token);
}

}
