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
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    auto session_cookie = r->getCookie("session_id");
    ASSERT_TRUE(session_cookie.value() != "");

    std::string access_token;
    Login(client, "User1", "11", access_token);

    resp = client->sendRequest(req);
    r = resp.second;

    std::ifstream f("../tests/files11.yut");

    Json::Value save_body;
    f >> save_body;
    req = HttpRequest::newHttpJsonRequest(save_body);
    req->setMethod(drogon::Post);
    req->setPath("/service/save-document");
    client->addCookie(session_cookie);
    req->addHeader("access_token", access_token);
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    req = HttpRequest::newHttpJsonRequest("{}");
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
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    auto session_cookie = r->getCookie("session_id");
    ASSERT_TRUE(session_cookie.value() != "");

    std::string access_token;
    Login(client, "User1", "11", access_token);

    std::ifstream f("../tests/files11.yut");

    Json::Value save_body;
    f >> save_body;
    req = HttpRequest::newHttpJsonRequest(save_body);
    req->setMethod(drogon::Post);
    req->setPath("/service/save-document");
    client->addCookie(session_cookie);
    req->addHeader("access_token", access_token);
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
    req->addHeader("access_token", access_token);
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

//Run a session, login, error of loading the document
TEST_F(ServiceTest, document3)
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
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    auto c = r->getCookie("session_id");
    ASSERT_TRUE(c.value() != "");
    client->addCookie(c);

    std::ifstream f("../tests/files11.yut");

    Json::Value save_body;
    f >> save_body;
    req = HttpRequest::newHttpJsonRequest(save_body);
    req->setMethod(drogon::Post);
    req->setPath("/service/save-document");
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k400BadRequest) << r->getStatusCode();

    Register(client, "User1", "user1@mail.com", "11");

    std::string access_token;
    Login(client, "User1", "11", access_token);

    resp = client->sendRequest(req);
    r = resp.second;

    req = HttpRequest::newHttpJsonRequest("{}");
    req->setMethod(drogon::Post);
    req->setPath("/service/load-document");
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k400BadRequest) << r->getStatusCode();

    UnRegister(client, "User1", access_token);
}

//Save document and access it without login
TEST_F(ServiceTest, document4)
{
    std::string document_id;
    Json::Value save_body;
    Cookie c;
    std::string access_token;

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

        Login(client, "User1", "11", access_token);

        std::ifstream f("../tests/files11.yut");

        f >> save_body;
        req = HttpRequest::newHttpJsonRequest(save_body);
        req->setMethod(drogon::Post);
        req->setPath("/service/save-document");
        req->addHeader("access_token", access_token);
        resp = client->sendRequest(req, 10);
        res = resp.first;
        r = resp.second;
        ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
        SessionPtr session = req->session();
        c = r->getCookie("document_id");
        document_id = c.value();
        ASSERT_TRUE(document_id != "");
    }

    auto client = HttpClient::newHttpClient("http://localhost:9001");
    client->enableCookies(true);

    //load the last document
    auto req = HttpRequest::newHttpJsonRequest("{}");
    req->setMethod(drogon::Post);
    req->setPath("/service/load-document");
    client->addCookie(c);
    auto resp = client->sendRequest(req, 10);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    auto load_json = r->jsonObject();
    ASSERT_TRUE(*load_json == save_body);
}

//Check new document with login
TEST_F(ServiceTest, document5)
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
    auto session_id = r->getCookie("session_id");

    std::string access_token;
    Login(client, "User1", "11", access_token);

    std::ifstream f("../tests/files11.yut");

    Json::Value save_body;
    f >> save_body;
    req = HttpRequest::newHttpJsonRequest(save_body);
    req->setMethod(drogon::Post);
    req->setPath("/service/save-document");
    req->addHeader("access_token", access_token);
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    SessionPtr session = req->session();
    auto c = r->getCookie("document_id");
    std::string document_id = c.value();
    ASSERT_TRUE(document_id != "");

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/document/" + document_id);
    req->addHeader("access_token", access_token);
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    req = HttpRequest::newHttpJsonRequest("{}");
    req->setMethod(drogon::Post);
    req->setPath("/service/new-document");
    req->addHeader("access_token", access_token);
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    c = r->getCookie("document_id");
    std::string new_document_id = c.value();
    ASSERT_TRUE(document_id != new_document_id);

    //switch to the last document
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/document/" + document_id);
    req->addHeader("access_token", access_token);
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(document_id == r->getCookie("document_id").value());

    //load the last document
    req = HttpRequest::newHttpJsonRequest("{}");
    req->setMethod(drogon::Post);
    req->setPath("/service/load-document");
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    auto load_json = r->jsonObject();
    ASSERT_TRUE(*load_json == save_body);

    //switch to the new document
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/document/" + new_document_id);
    req->addHeader("access_token", access_token);
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(document_id != r->getCookie("document_id").value());

    orm::DbClientPtr db = app().getDbClient();
    orm::Result result = db->execSqlSync("select document_id from user_sessions where session_id=$1", session_id.value());
    ASSERT_TRUE(result.size() != 0);
    auto row = result[0];
    auto d = row["document_id"].as<std::string>();
    ASSERT_TRUE(d == new_document_id);

    //load the new document
    req = HttpRequest::newHttpJsonRequest("{}");
    req->setMethod(drogon::Post);
    req->setPath("/service/load-document");
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    auto load_json_str = r->jsonObject()->toStyledString();
    Json::Value v;
    Json::Reader reader;
    reader.parse("{\"text\":{\"id\":\"0\",\"type\":1,\"elements\":[{\"id\":\"0,0\",\"type\":2,\
        \"elements\":[{\"id\":\"0,0,0\",\"type\":3,\"elements\":[{\"id\":\"0,0,0,0\",\"type\":4,\"elements\":\"\"}]}]}]}}", v);
    ASSERT_TRUE(load_json_str == v.toStyledString()) << load_json_str;

    UnRegister(client, "User1", access_token);
}

//Load a foreign document and error of saving it with own id
TEST_F(ServiceTest, document6)
{
    std::string document_id;
    Json::Value save_body;
    Cookie c;
    std::string access_token;

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

        Login(client, "User1", "11", access_token);

        std::ifstream f("../tests/files11.yut");

        f >> save_body;
        req = HttpRequest::newHttpJsonRequest(save_body);
        req->setMethod(drogon::Post);
        req->setPath("/service/save-document");
        req->addHeader("access_token", access_token);
        resp = client->sendRequest(req, 10);
        res = resp.first;
        r = resp.second;
        ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
        SessionPtr session = req->session();
        c = r->getCookie("document_id");
        document_id = c.value();
        ASSERT_TRUE(document_id != "");
    }

    {
        auto client = HttpClient::newHttpClient("http://localhost:9001");
        client->enableCookies(true);

        Register(client, "User2", "user1@mail.com", "22");

        auto req = HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);
        req->setPath("/");
        auto resp = client->sendRequest(req);
        ReqResult& res = resp.first;
        HttpResponsePtr& r = resp.second;

        Login(client, "User2", "22", access_token);

        //load the foreing document
        req = HttpRequest::newHttpJsonRequest("{}");
        req->setMethod(drogon::Post);
        req->setPath("/service/load-document");
        client->addCookie(c);
        resp = client->sendRequest(req, 10);
        res = resp.first;
        r = resp.second;
        ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
        ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
        auto load_json = r->jsonObject();
        ASSERT_TRUE(*load_json == save_body);

        req = HttpRequest::newHttpJsonRequest(load_json->toStyledString());
        req->setMethod(drogon::Post);
        req->setPath("/service/save-document");
        req->addHeader("access_token", access_token);
        resp = client->sendRequest(req, 10);
        res = resp.first;
        r = resp.second;
        ASSERT_TRUE(r->getStatusCode() == k400BadRequest) << r->getStatusCode();
        auto document_cookie = r->getCookie("document_id");
        ASSERT_TRUE(document_cookie.value() != document_id);
    }
}
}
