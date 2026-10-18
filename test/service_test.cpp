/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <gtest/gtest.h>
#include "mock.h"

namespace yutovo_server_test
{

using namespace drogon;
using namespace std::chrono_literals;

//Get list of library documents
TEST_F(ServiceTest, library1)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/service/get-library-documents");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
}

//Load a library document
TEST_F(ServiceTest, library2)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    Json::Value body;
    body["document"] = "/Physics/Dynamics/Kinetic energy.yut";
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/load-library-document");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
}

//Wrong library document path
TEST_F(ServiceTest, library3)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    Json::Value body;
    body["document"] = "/Physics/Dynamics/../Kinetic energy.yut";
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/load-library-document");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k404NotFound) << r->getStatusCode();

    body["document"] = "/Physics/Dynamics/../../../CMakeLists.txt";
    req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/load-library-document");

    resp = client->sendRequest(req);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k404NotFound) << r->getStatusCode();
}

#ifdef REMOTE_SOLVER
//List identifiers from the service
TEST_F(ServiceTest, service1)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

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
    ASSERT_TRUE((*json)["Functions"].isArray());
    ASSERT_TRUE((*json)["Variables"].isArray());
}
#endif

//Save/load a user document in the DB
TEST_F(ServiceTest, document1)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");
    auto resp = client->sendRequest(req);

    Register(client, "User1", "user1@mail.com", "11");

    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    auto session_cookie = r->getCookie("session_id");
    ASSERT_TRUE(session_cookie.value() != "");

    std::string access_token, document_id, name, language;
    Login(client, "User1", "11", access_token, document_id, name, language);

    resp = client->sendRequest(req);
    r = resp.second;

    std::ifstream f("../../tests/files11.yut");

    Json::Value save_body;
    f >> save_body;
    req = HttpRequest::newHttpJsonRequest(save_body);
    req->setMethod(drogon::Post);
    req->setPath("/service/save-document");
    client->addCookie(session_cookie);
    client->addCookie("document_id", document_id);
    req->addHeader("access_token", access_token);
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    Json::Value s;
    s["document_id"] = -1;
    req = HttpRequest::newHttpJsonRequest(s);
    req->setMethod(drogon::Post);
    req->setPath("/service/load-document");
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    const auto load_json = r->jsonObject();
    ASSERT_TRUE(save_body == *load_json) << save_body << "\n" << *load_json;

    UnRegister(client, "User1", access_token);
}

//Save/load a part of a user document in the DB
TEST_F(ServiceTest, document2)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

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

    std::string access_token, document_id, name, language;
    Login(client, "User1", "11", access_token, document_id, name, language);

    std::ifstream f("../../tests/files11.yut");

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
    client->addCookie("document_id", document_id);
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
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

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

    std::ifstream f("../../tests/files11.yut");

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

    std::string access_token, document_id, name, language;
    Login(client, "User1", "11", access_token, document_id, name, language);

    resp = client->sendRequest(req);
    r = resp.second;

    Json::Value s;
    s["document_id"] = -1;
    req = HttpRequest::newHttpJsonRequest(s);
    req->setMethod(drogon::Post);
    client->addCookie("document_id", document_id);
    req->setPath("/service/load-document");
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    UnRegister(client, "User1", access_token);
}

//Save document and access it without login
TEST_F(ServiceTest, document4)
{
    Json::Value save_body;
    std::string access_token, document_id, name, language;

    {
        auto client = HttpClient::newHttpClient(address);
        client->enableCookies(true);
        StartPage(client);

        Register(client, "User1", "user1@mail.com", "11");

        auto req = HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);
        req->setPath("/");
        auto resp = client->sendRequest(req);
        ReqResult& res = resp.first;
        HttpResponsePtr& r = resp.second;

        Login(client, "User1", "11", access_token, document_id, name, language);

        std::ifstream f("../../tests/files11.yut");
        f >> save_body;
        req = HttpRequest::newHttpJsonRequest(save_body);
        req->setMethod(drogon::Post);
        req->setPath("/service/save-document");
        req->addHeader("access_token", access_token);
        resp = client->sendRequest(req, 10);
        res = resp.first;
        r = resp.second;
        ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
        const auto json = r->jsonObject();
        document_id = (*json)["document_id"].asString();
    }

    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);
    StartPage(client);

    //load the last document
    Json::Value s;
    s["document_id"] = -1;
    auto req = HttpRequest::newHttpJsonRequest(s);
    req->setMethod(drogon::Post);
    req->setPath("/service/load-document");
    client->addCookie("document_id", document_id);
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
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    Register(client, "User1", "user1@mail.com", "11");

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");
    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    auto session_id = r->getCookie("session_id");

    std::string access_token, document_id, name, language;
    Login(client, "User1", "11", access_token, document_id, name, language);

    std::ifstream f("../../tests/files11.yut");

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

    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/document/" + document_id);
    req->addHeader("access_token", access_token);
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    std::string new_document_id;
    NewDocument(client, access_token, new_document_id);

    //switch to the last document
    req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/document/" + document_id);
    client->addCookie("document_id", document_id);
    req->addHeader("access_token", access_token);
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    //load the last document
    Json::Value s;
    s["document_id"] = -1;
    req = HttpRequest::newHttpJsonRequest(s);
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
    req = HttpRequest::newHttpJsonRequest(s);
    req->setMethod(drogon::Post);
    client->addCookie("document_id", new_document_id);
    req->setPath("/service/load-document");
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    auto load_json_str = r->jsonObject()->toStyledString();
    Json::Value v;
    Json::Reader reader;
    reader.parse("{\"caret\" : {\"id\" : \"0,0,0,0,0,0,0\"}, \"selection\" : [], \"text\" : {\"elements\" : [{\"elements\" : [{\"elements\" : \
        [{\"code_id\" : 1, \"elements\" : [{\"elements\" : [{\"elements\" : [{\"elements\" : \"\", \"id\" : \"0,0,0,0,0,0,0\", \"type\" : 8}], \
        \"id\" : \"0,0,0,0,0,0\", \"type\" : 7}], \"format_alignment\" : 0, \"format_name\" : \"Code\", \"id\" : \"0,0,0,0,0\", \"type\" : 6}], \
        \"id\" : \"0,0,0,0\", \"type\" : 5}],	\"id\" : \"0,0,0\",	\"type\" : 3}],	\"id\" : \"0,0\", \"type\" : 2}], \"id\" : \"0\", \"type\" : 1}}", v);
    ASSERT_TRUE(load_json_str == v.toStyledString()) << load_json_str;

    UnRegister(client, "User1", access_token);
}

//Load a foreign document and getting error of saving it with own id
TEST_F(ServiceTest, document6)
{
    Json::Value save_body;
    Cookie c;
    std::string access_token, document_id, name, language;

    {
        auto client = HttpClient::newHttpClient(address);
        client->enableCookies(true);
        StartPage(client);

        Register(client, "User1", "user1@mail.com", "11");

        auto req = HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);
        req->setPath("/");
        auto resp = client->sendRequest(req);
        ReqResult& res = resp.first;
        HttpResponsePtr& r = resp.second;

        Login(client, "User1", "11", access_token, document_id, name, language);

        std::ifstream f("../../tests/files11.yut");

        f >> save_body;
        req = HttpRequest::newHttpJsonRequest(save_body);
        req->setMethod(drogon::Post);
        req->setPath("/service/save-document");
        req->addHeader("access_token", access_token);
        resp = client->sendRequest(req, 10);
        res = resp.first;
        r = resp.second;
        ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    }

    {
        auto client = HttpClient::newHttpClient(address);
        client->enableCookies(true);
        StartPage(client);

        Register(client, "User2", "user2@mail.com", "22");

        auto req = HttpRequest::newHttpRequest();
        req->setMethod(drogon::Get);
        req->setPath("/");
        auto resp = client->sendRequest(req);
        ReqResult& res = resp.first;
        HttpResponsePtr& r = resp.second;

        std::string document2_id;
        Login(client, "User2", "22", access_token, document2_id, name, language);

        //load a foreign document
        Json::Value s;
        s["document_id"] = -1;
        req = HttpRequest::newHttpJsonRequest(s);
        req->setMethod(drogon::Post);
        req->setPath("/service/load-document");
        client->addCookie("document_id", document_id);
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

//List documents of a user
TEST_F(ServiceTest, document7)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);
    StartPage(client);

    Register(client, "User1", "user1@mail.com", "11");

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");
    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;

    std::string access_token, document_id, name, language;
    Login(client, "User1", "11", access_token, document_id, name, language);

    Json::Value save_body;
    std::ifstream f("../../tests/files11.yut");
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

    Json::Value s;
    s["document_id"] = -1;
    req = HttpRequest::newHttpJsonRequest(s);
    req->setMethod(drogon::Post);
    req->setPath("/service/list-documents");
    req->addHeader("access_token", access_token);
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    const auto list_json = r->jsonObject();
    ASSERT_TRUE(list_json->isArray());
    ASSERT_TRUE(list_json->size() == 1);
    ASSERT_TRUE((*list_json)[0]["id"].asString() == document_id) << (*list_json)[0]["id"];

    UnRegister(client, "User1", access_token);
}

//Delete current document of a user
TEST_F(ServiceTest, document8)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);
    StartPage(client);

    Register(client, "User1", "user1@mail.com", "11");

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");
    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;

    std::string access_token, document_id, name, language;
    Login(client, "User1", "11", access_token, document_id, name, language);

    SaveDocument(client, "../../tests/files11.yut", access_token, document_id);

    client->addCookie("document_id", document_id);
    DeleteDocument(client, access_token);

    Json::Value s;
    s["document_id"] = document_id;
    req = HttpRequest::newHttpJsonRequest(s);
    req->setMethod(drogon::Post);
    req->setPath("/service/load-document");
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k404NotFound) << r->getStatusCode();

    UnRegister(client, "User1", access_token);
}

//Delete a document of a user
TEST_F(ServiceTest, document9)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);
    StartPage(client);

    Register(client, "User1", "user1@mail.com", "11");

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");
    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;

    std::string access_token, document_id, name, language;
    Login(client, "User1", "11", access_token, document_id, name, language);

    std::string document1_id, document2_id;
    SaveDocument(client, "../../tests/files11.yut", access_token, document1_id);

    NewDocument(client, access_token, document2_id);
    SaveDocument(client, "../../tests/plus6_1.yut", access_token, document2_id);

    DeleteDocument(client, access_token, document2_id);
    ASSERT_TRUE(document1_id != document2_id);

    Json::Value v;
    Json::Reader reader;
    reader.parse("{\"document_id\":" + document2_id + "}", v);
    req = HttpRequest::newHttpJsonRequest(v);
    req->setMethod(drogon::Post);
    req->setPath("/service/load-document");
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k404NotFound) << r->getStatusCode();

    std::shared_ptr<Json::Value> doc;
    LoadDocument(client, access_token, document1_id, doc);

    UnRegister(client, "User1", access_token);
}

//Save and load documents of a user
TEST_F(ServiceTest, document10)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);
    StartPage(client);

    Register(client, "User1", "user1@mail.com", "11");

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");
    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;

    std::string access_token, document_id, name, language;
    Login(client, "User1", "11", access_token, document_id, name, language);

    std::string document1_id, document2_id;
    SaveDocument(client, "../../tests/files11.yut", access_token, document1_id);

    NewDocument(client, access_token, document2_id);
    SaveDocument(client, "../../tests/plus6_1.yut", access_token, document2_id);

    std::shared_ptr<Json::Value> doc;
    LoadDocument(client, access_token, document1_id, doc);
    LoadDocument(client, access_token, document2_id, doc);

    UnRegister(client, "User1", access_token);
}

//Rename a document
TEST_F(ServiceTest, document11)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);
    StartPage(client);

    Register(client, "User1", "user1@mail.com", "11");

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");
    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;

    std::string access_token;
    std::string document_id, name, language;
    Login(client, "User1", "11", access_token, document_id, name, language);
    ASSERT_TRUE(name == "document_1") << name;

    RenameDocument(client, access_token, document_id, "new_name");

    GetDocumentName(client, document_id, name);
    ASSERT_TRUE(name == "new_name") << name;

    UnRegister(client, "User1", access_token);
}

//Create an empty document
TEST_F(ServiceTest, document12)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);
    StartPage(client);

    Register(client, "User1", "user1@mail.com", "11");

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");
    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;

    std::string access_token, name, document_id, language;
    Login(client, "User1", "11", access_token, document_id, name, language);
    ASSERT_TRUE(name == "document_1") << name;

    GetDocumentName(client, document_id, name);
    ASSERT_TRUE(name == "document_1") << name;

    NewDocument(client, access_token, document_id);

    GetDocumentName(client, document_id, name);
    ASSERT_TRUE(name == "document_2") << name;

    UnRegister(client, "User1", access_token);
}

//Save document with another name
TEST_F(ServiceTest, document13)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);
    StartPage(client);

    Register(client, "User1", "user1@mail.com", "11");

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");
    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;

    std::string access_token, document_id, name, language;
    Login(client, "User1", "11", access_token, document_id, name, language);

    Json::Value save_body;
    std::ifstream f("../../tests/files11.yut");
    f >> save_body;

    std::string document1_id, document2_id;
    SaveDocument(client, "../../tests/files11.yut", access_token, document1_id);

    Json::Value body;
    body["document_id"] = document1_id;
    body["name"] = "new_name";
    req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/save-as-document");
    req->addHeader("access_token", access_token);
    resp = client->sendRequest(req, 10);
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    const auto json = r->jsonObject();
    document2_id = (*json)["document_id"].asString();
    ASSERT_TRUE(document1_id != document2_id);

    GetDocumentName(client, document2_id, name);
    ASSERT_TRUE(name == "new_name") << name;

    std::shared_ptr<Json::Value> doc1, doc2;
    LoadDocument(client, access_token, document2_id, doc2);
    LoadDocument(client, access_token, document1_id, doc1);

    ASSERT_TRUE(save_body == *doc1);
    ASSERT_TRUE(save_body == *doc2);

    req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/save-as-document");
    req->addHeader("access_token", access_token);
    resp = client->sendRequest(req, 10);
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k409Conflict) << r->getStatusCode();

    UnRegister(client, "User1", access_token);
}

//Load first empty document of a user
TEST_F(ServiceTest, document14)
{
    std::string document1_id;
    {
        auto client = HttpClient::newHttpClient(address);
        client->enableCookies(true);
        StartPage(client);

        Register(client, "User1", "user1@mail.com", "11");

        Locate(client, "/");

        std::string access_token, name, language;
        Login(client, "User1", "11", access_token, document1_id, name, language);

        std::string document2_id;
        NewDocument(client, access_token, document2_id);
        SaveDocument(client, "../../tests/files11.yut", access_token, document2_id);

        Logout(client, "User1", access_token);
    }

    {
        auto client = HttpClient::newHttpClient(address);
        client->enableCookies(true);

        Locate(client, "/");

        std::string access_token, document3_id, name, language;
        Login(client, "User1", "11", access_token, document3_id, name, language);

        ASSERT_TRUE(document3_id == document1_id) << document3_id;

        UnRegister(client, "User1", access_token);
    }
}

//Check max files count
TEST_F(ServiceTest, document15)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);
    StartPage(client);

    Register(client, "User1", "user1@mail.com", "11");

    Locate(client, "/");

    std::string access_token, document_id, name, language;
    Login(client, "User1", "11", access_token, document_id, name, language);

    orm::DbClientPtr db = app().getDbClient();
    orm::Result plan_result = db->execSqlSync("select max_files from user_plans where plan_id=(select plan_id from users where login=$1)", "User1");
    ASSERT_TRUE(plan_result.size() != 0);
    int max_files = plan_result[0]["max_files"].as<int>();

    for (int i = 0; i < max_files - 1; ++i)
        NewDocument(client, access_token, document_id);

    Json::Value s;
    s["document_id"] = -1;
    auto req = HttpRequest::newHttpJsonRequest(s);
    req->setMethod(drogon::Post);
    req->setPath("/service/new-document");
    req->addHeader("access_token", access_token);
    auto resp = client->sendRequest(req, 10);
    auto r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k403Forbidden) << r->getStatusCode();

    UnRegister(client, "User1", access_token);
}

//Check max file size
TEST_F(ServiceTest, document16)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);
    StartPage(client);

    Register(client, "User1", "user1@mail.com", "11");

    Locate(client, "/");

    std::string access_token, document_id, name, language;
    Login(client, "User1", "11", access_token, document_id, name, language);

    Json::Value save_body;
    std::ifstream f("../../tests/file_size_limit.yut");
    f >> save_body;
    auto req = HttpRequest::newHttpJsonRequest(save_body);
    req->setMethod(drogon::Post);
    req->setPath("/service/save-document");
    req->addHeader("access_token", access_token);
    auto resp = client->sendRequest(req, 10);
    auto r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k400BadRequest) << r->getStatusCode();

    Logout(client, "User1", access_token);
}

//Set/get settings
TEST_F(ServiceTest, document17)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);
    StartPage(client);

    Register(client, "User1", "user1@mail.com", "11");

    drogon::orm::DbClientPtr db = drogon::app().getDbClient();
    db->execSqlSync("update users set settings='{}'::jsonb where login='User1'");

     Locate(client, "/");

    std::string access_token, document_id, name, language, settings;
    Login(client, "User1", "11", access_token, document_id, name, language, settings);

    Json::Value doc;
    Json::Reader reader;
    reader.parse(settings, doc);
    doc["with_border"] = true;

    Json::Value body;
    body["settings"] = doc.toStyledString();

    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/set-user-settings");
    req->addHeader("access_token", access_token);
    auto resp = client->sendRequest(req, 10);
    auto r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    Json::Value body2;
    Json::Value doc2;
    doc2["real_result"]["precision"] = 10;
    body2["settings"] = doc2.toStyledString();

    req = HttpRequest::newHttpJsonRequest(body2);
    req->setMethod(drogon::Post);
    req->setPath("/service/set-user-settings");
    req->addHeader("access_token", access_token);
    resp = client->sendRequest(req, 10);
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    Logout(client, "User1", access_token);

    std::string config2;
    Login(client, "User1", "11", access_token, document_id, name, language, config2);

    Json::Value doc3;
    reader.parse(config2, doc3);
    ASSERT_TRUE(doc3["with_border"] == true) << doc3["with_border"];
    ASSERT_TRUE(doc3["real_result"]["precision"] == 10) << doc3["real_result"]["precision"];

    doc2["real_result"]["precision"] = 20;
    body2["settings"] = doc2.toStyledString();
    req = HttpRequest::newHttpJsonRequest(body2);
    req->setMethod(drogon::Post);
    req->setPath("/service/set-user-settings");
    req->addHeader("access_token", access_token);
    resp = client->sendRequest(req, 10);
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    Logout(client, "User1", access_token);

    Login(client, "User1", "11", access_token, document_id, name, language, config2);

    reader.parse(config2, doc3);
    ASSERT_TRUE(doc3["with_border"] == true);
    ASSERT_TRUE(doc3["real_result"]["precision"] == 20);

    Json::Value v;
    req = HttpRequest::newHttpJsonRequest(v);
    req->setMethod(drogon::Post);
    req->setPath("/service/get-user-settings");
    req->addHeader("access_token", access_token);
    resp = client->sendRequest(req);
    r = resp.second;
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    const auto json = r->jsonObject();
    ASSERT_TRUE((*json)["settings"]["real_result"]["precision"] == 20) << json;

    Logout(client, "User1", access_token);
}

//Set password
TEST_F(ServiceTest, document18)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);
    StartPage(client);

    Register(client, "User1", "user1@mail.com", "11");

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");
    auto resp = client->sendRequest(req);

    std::string access_token;

    {
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
        access_token = r->getHeader("access_token");
        ASSERT_TRUE(res == ReqResult::Ok) << res;
        ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    }

    {
        //change password
        Json::Value body;
        body["old_password"] = "11";
        body["password"] = "5555";
        req = HttpRequest::newHttpJsonRequest(body);
        req->setMethod(drogon::Post);
        req->setPath("/service/set-user-settings");
        req->addHeader("access_token", access_token);
        resp = client->sendRequest(req);
        ReqResult& res = resp.first;
        HttpResponsePtr& r = resp.second;
        ASSERT_TRUE(res == ReqResult::Ok) << res;
        ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    }

    {
        //logout
        Json::Value body;
        body["login"] = "User1";
        req = HttpRequest::newHttpJsonRequest(body);
        req->setMethod(drogon::Post);
        req->setPath("/auth/logout");
        req->addHeader("access_token", access_token);
    }

    {
        //login
        Json::Value body;
        body["login"] = "User1";
        body["password"] = "5555";
        req = HttpRequest::newHttpJsonRequest(body);
        req->setMethod(drogon::Post);
        req->setPath("/auth/login");
        resp = client->sendRequest(req);
        ReqResult& res = resp.first;
        HttpResponsePtr& r = resp.second;
        access_token = r->getHeader("access_token");
        ASSERT_TRUE(res == ReqResult::Ok) << res;
        ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    }
}

//Check a wrong SessionId
TEST_F(ServiceTest, document19)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);
    StartPage(client);

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

    std::string access_token, document_id, name, language;
    Login(client, "User1", "11", access_token, document_id, name, language);

    resp = client->sendRequest(req);
    r = resp.second;

    std::ifstream f("../../tests/files11.yut");

    Json::Value save_body;
    f >> save_body;
    req = HttpRequest::newHttpJsonRequest(save_body);
    req->setMethod(drogon::Post);
    req->setPath("/service/save-document");
    client->addCookie("session_id", "1234324");
    client->addCookie("document_id", document_id);
    req->addHeader("access_token", access_token);
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k403Forbidden) << r->getStatusCode();

    req = HttpRequest::newHttpJsonRequest(save_body);
    req->setMethod(drogon::Post);
    req->setPath("/service/save-document");
    client->addCookie("session_id", "12344567-2345-5467-234578901234deffe");
    client->addCookie("document_id", document_id);
    req->addHeader("access_token", access_token);
    resp = client->sendRequest(req, 10);
    res = resp.first;
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k403Forbidden) << r->getStatusCode();
}

//Access to another user's documents via save or rename is denied
TEST_F(ServiceTest, document20)
{
    auto client1 = HttpClient::newHttpClient(address);
    client1->enableCookies(true);
    StartPage(client1);

    auto client2 = HttpClient::newHttpClient(address);
    client2->enableCookies(true);
    StartPage(client2);

    Register(client1, "User1", "user1@mail.com", "11");
    Register(client2, "User2", "user2@mail.com", "22");

    std::string access_token1, document_id1, name1, language1;
    Login(client1, "User1", "11", access_token1, document_id1, name1, language1);

    std::string access_token2, document_id2, name2, language2;
    Login(client2, "User2", "22", access_token2, document_id2, name2, language2);

    std::string document_id;
    SaveDocument(client1, "../../tests/files11.yut", access_token1, document_id);
    RenameDocument(client1, access_token1, document_id, "user1_secret_name");

    //save-as of another user's document is forbidden
    Json::Value body;
    body["document_id"] = document_id;
    body["name"] = "stolen";
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/save-as-document");
    req->addHeader("access_token", access_token2);
    auto resp = client2->sendRequest(req, 10);
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k403Forbidden) << r->getStatusCode();

    //get-document-id does not find another user's document by name
    Json::Value name_body;
    name_body["name"] = "user1_secret_name";
    req = HttpRequest::newHttpJsonRequest(name_body);
    req->setMethod(drogon::Post);
    req->setPath("/service/get-document-id");
    resp = client2->sendRequest(req, 10);
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k404NotFound) << r->getStatusCode();

    //an anonymous session does not find the document either
    auto client3 = HttpClient::newHttpClient(address);
    client3->enableCookies(true);
    StartPage(client3);
    req = HttpRequest::newHttpJsonRequest(name_body);
    req->setMethod(drogon::Post);
    req->setPath("/service/get-document-id");
    resp = client3->sendRequest(req, 10);
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k404NotFound) << r->getStatusCode();

    //the owner finds the document by name
    req = HttpRequest::newHttpJsonRequest(name_body);
    req->setMethod(drogon::Post);
    req->setPath("/service/get-document-id");
    resp = client1->sendRequest(req, 10);
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    const auto json = r->jsonObject();
    ASSERT_TRUE(std::to_string((*json)["document_id"].as<int>()) == document_id);

    //renaming of another user's document is forbidden
    body["name"] = "hacked";
    req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/rename-document");
    req->addHeader("access_token", access_token2);
    resp = client2->sendRequest(req, 10);
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k403Forbidden) << r->getStatusCode();

    //the name of the document is not changed
    GetDocumentName(client1, document_id, name1);
    ASSERT_TRUE(name1 == "user1_secret_name") << name1;

    UnRegister(client1, "User1", access_token1);
    UnRegister(client2, "User2", access_token2);
}

//All documents are public for reading by default
TEST_F(ServiceTest, document21)
{
    auto client1 = HttpClient::newHttpClient(address);
    client1->enableCookies(true);
    StartPage(client1);

    auto client2 = HttpClient::newHttpClient(address);
    client2->enableCookies(true);
    StartPage(client2);

    auto client3 = HttpClient::newHttpClient(address);
    client3->enableCookies(true);
    StartPage(client3);

    Register(client1, "User1", "user1@mail.com", "11");
    Register(client2, "User2", "user2@mail.com", "22");

    std::string access_token1, document_id1, name1, language1;
    Login(client1, "User1", "11", access_token1, document_id1, name1, language1);

    std::string access_token2, document_id2, name2, language2;
    Login(client2, "User2", "22", access_token2, document_id2, name2, language2);

    std::string document_id;
    SaveDocument(client1, "../../tests/files11.yut", access_token1, document_id);
    RenameDocument(client1, access_token1, document_id, "user1_secret_name");

    Json::Value save_body;
    std::ifstream f("../../tests/files11.yut");
    f >> save_body;

    //another user loads the whole document
    Json::Value body;
    body["document_id"] = document_id;
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/load-document");
    auto resp = client2->sendRequest(req, 10);
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    const auto json = r->jsonObject();
    ASSERT_TRUE(save_body == *json);

    //an anonymous session loads the whole document
    req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/load-document");
    resp = client3->sendRequest(req, 10);
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    //another user gets the document name
    req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/get-document-name");
    resp = client2->sendRequest(req, 10);
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    const auto name_json = r->jsonObject();
    ASSERT_TRUE((*name_json)["name"].asString() == "user1_secret_name");

    UnRegister(client1, "User1", access_token1);
    UnRegister(client2, "User2", access_token2);
}

//Check for desktop application updates
TEST_F(ServiceTest, get_updates)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);
    StartPage(client);

    //update is available, local link must be converted to absolute URL
    {
        Json::Value body;
        body["version"] = "1.6.2";
        body["system"] = "Windows";
        body["language"] = "en";
        auto req = HttpRequest::newHttpJsonRequest(body);
        req->setMethod(drogon::Post);
        req->setPath("/service/get-updates");

        auto resp = client->sendRequest(req);
        ReqResult& res = resp.first;
        HttpResponsePtr& r = resp.second;
        ASSERT_TRUE(res == ReqResult::Ok) << res;
        ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
        ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
        const auto json = r->jsonObject();
        ASSERT_TRUE((*json)["hasUpdate"].asBool());
        ASSERT_EQ((*json)["version"].asString(), "1.7.1");
        ASSERT_EQ((*json)["url"].asString(), "http://www.yutovo.ru:9001/downloads/yutovo-1.7.1.exe");
    }

    //no update for the current version
    {
        Json::Value body;
        body["version"] = "1.7.1";
        body["system"] = "Windows";
        body["language"] = "en";
        auto req = HttpRequest::newHttpJsonRequest(body);
        req->setMethod(drogon::Post);
        req->setPath("/service/get-updates");

        auto resp = client->sendRequest(req);
        HttpResponsePtr& r = resp.second;
        ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
        const auto json = r->jsonObject();
        ASSERT_FALSE((*json)["hasUpdate"].asBool());
    }

    //external link is returned as is
    {
        Json::Value body;
        body["version"] = "1.6.2";
        body["system"] = "Linux";
        body["language"] = "en";
        auto req = HttpRequest::newHttpJsonRequest(body);
        req->setMethod(drogon::Post);
        req->setPath("/service/get-updates");

        auto resp = client->sendRequest(req);
        HttpResponsePtr& r = resp.second;
        ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
        const auto json = r->jsonObject();
        ASSERT_TRUE((*json)["hasUpdate"].asBool());
        ASSERT_EQ((*json)["url"].asString(), "https://yutovo.com/yutovo-1.7.1.deb");
    }

    //missing required field
    {
        Json::Value body;
        body["version"] = "1.6.2";
        body["system"] = "Windows";
        auto req = HttpRequest::newHttpJsonRequest(body);
        req->setMethod(drogon::Post);
        req->setPath("/service/get-updates");

        auto resp = client->sendRequest(req);
        HttpResponsePtr& r = resp.second;
        ASSERT_TRUE(r->getStatusCode() == k400BadRequest) << r->getStatusCode();
    }
}

//Variable definition has no result and is logged without '='
TEST_F(SolverActionTest, solver_action1)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    Json::Value body;
    body["solver_guid"] = "00000000-0000-0000-0000-000000000001";
    body["result_type"] = 0;
    body["expression"] = "length=1m;";
    body["id"] = "0,2,0,1,0,0";

    EXPECT_EQ(PostSolverAction(client, body, "length=1m;"), "length=1m;, id=\"0,2,0,1,0,0\"");
}

//Graph reply is stripped by the client to guid, expression and id and is logged without '='
TEST_F(SolverActionTest, solver_action2)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    Json::Value body;
    body["solver_guid"] = "00000000-0000-0000-0000-000000000002";
    body["expression"] = "graph_line(sin(x),x,-π,π,-1,1,493);";
    body["id"] = "0,29,0,0,0,0,1,0";
    EXPECT_EQ(PostSolverAction(client, body, "graph_line(sin(x),x,-π,π,-1,1,493);"), "graph_line(sin(x),x,-π,π,-1,1,493);, id=\"0,29,0,0,0,0,1,0\"");
}

//Array result is logged as a list of its elements
TEST_F(SolverActionTest, solver_action3)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    Json::Value body;
    body["solver_guid"] = "00000000-0000-0000-0000-000000000003";
    body["result_type"] = 6;
    body["expression"] = "[1,3,4]+[3,4,(5/4)];";
    body["id"] = "0,21,0,0,0,0,2,0";

    Json::Value results(Json::arrayValue);
    Json::Value element;
    element["mantissa"] = "4";
    results.append(element);
    element = Json::Value();
    element["mantissa"] = "7";
    results.append(element);
    element = Json::Value();
    element["mantissa"] = "5.25";
    results.append(element);
    body["results"] = results;
    EXPECT_EQ(PostSolverAction(client, body, "[1,3,4]+[3,4,(5/4)];"), "[1,3,4]+[3,4,(5/4)];=[4,7,5.25], id=\"0,21,0,0,0,0,2,0\"");
}

//Complex results with re and im members are separated between elements only
TEST_F(SolverActionTest, solver_action4)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    Json::Value body;
    body["solver_guid"] = "00000000-0000-0000-0000-000000000004";
    body["result_type"] = 4;
    body["expression"] = "solve(x^2+4,x);";
    body["id"] = "0,4,0,0,0,0";

    Json::Value results(Json::arrayValue);
    Json::Value element;
    Json::Value re;
    re["mantissa"] = "0";
    Json::Value im;
    im["mantissa"] = "2";
    element["re"] = re;
    element["im"] = im;
    results.append(element);
    element = Json::Value();
    re = Json::Value();
    re["mantissa"] = "0";
    im = Json::Value();
    im["mantissa"] = "-2";
    element["re"] = re;
    element["im"] = im;
    results.append(element);
    body["results"] = results;
    EXPECT_EQ(PostSolverAction(client, body, "solve(x^2+4,x);"), "solve(x^2+4,x);=[0+i*2,0+i*-2], id=\"0,4,0,0,0,0\"");
}

//Symbolic result is rendered from the json tree of elements
TEST_F(SolverActionTest, solver_action5)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    Json::Value body;
    body["solver_guid"] = "00000000-0000-0000-0000-000000000005";
    body["result_type"] = 7;
    body["expression"] = "indefinite_integral(4*sin(x)+pow(e,x),x);";
    body["id"] = "0,27,0,3,0,0,2,0";
    body["json"] = "{\"type\":45,\"elements\":[{\"type\":7,\"elements\":[{\"type\":12,\"symbol\":\"-\"},"
        "{\"type\":8,\"elements\":\"4\"},{\"type\":13,\"symbol\":\"·\"},{\"type\":8,\"elements\":\"cos\"},"
        "{\"type\":19,\"symbol\":\"(\"},{\"type\":8,\"elements\":\"x\"},{\"type\":20,\"symbol\":\")\"},"
        "{\"type\":11,\"symbol\":\"+\"},{\"type\":15,\"elements\":[{\"type\":7,\"elements\":"
        "[{\"type\":8,\"elements\":\"e\"}]},{\"type\":10,\"elements\":[]},{\"type\":7,\"elements\":"
        "[{\"type\":8,\"elements\":\"x\"}]}]}]}]}";
    EXPECT_EQ(PostSolverAction(client, body, "indefinite_integral(4*sin(x)+pow(e,x),x);"),
        "indefinite_integral(4*sin(x)+pow(e,x),x);=-4*cos(x)+e^x, id=\"0,27,0,3,0,0,2,0\"");
}

//Error without a result is logged after '='
TEST_F(SolverActionTest, solver_action6)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    Json::Value body;
    body["solver_guid"] = "00000000-0000-0000-0000-000000000006";
    body["result_type"] = 0;
    body["expression"] = "1/0;";
    body["id"] = "0,0,0,1,0,0";
    body["error"]["description"] = "Division by zero";
    EXPECT_EQ(PostSolverAction(client, body, "1/0;"), "1/0;=Division by zero, id=\"0,0,0,1,0,0\"");
}

//Real result with exponent and unit is logged as before
TEST_F(SolverActionTest, solver_action7)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    Json::Value body;
    body["solver_guid"] = "00000000-0000-0000-0000-000000000007";
    body["result_type"] = 1;
    body["expression"] = "L;";
    body["id"] = "0,2,0,1,0,0";
    body["mantissa"] = "1.55";
    body["exponent"] = "-2";
    Json::Value unit_name;
    unit_name["name"] = "m";
    Json::Value unit_value(Json::arrayValue);
    unit_value.append(unit_name);
    body["unit"]["value"] = unit_value;
    body["unit"]["system"] = "SI";
    EXPECT_EQ(PostSolverAction(client, body, "L;"), "L;=1.55*10^-2[m]{SI}, id=\"0,2,0,1,0,0\"");
}

//Real result without exponent
TEST_F(SolverActionTest, solver_action8)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    Json::Value body;
    body["solver_guid"] = "00000000-0000-0000-0000-000000000008";
    body["result_type"] = 1;
    body["expression"] = "2+2;";
    body["id"] = "0,1,0,0,0,0";
    body["mantissa"] = "4";
    EXPECT_EQ(PostSolverAction(client, body, "2+2;"), "2+2;=4, id=\"0,1,0,0,0,0\"");
}

//Mixed fraction, rational, integer and value results
TEST_F(SolverActionTest, solver_action9)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    //mixed fraction
    Json::Value body;
    body["solver_guid"] = "00000000-0000-0000-0000-000000000009";
    body["result_type"] = 3;
    body["expression"] = "5+1/3;";
    body["id"] = "0,3,0,1,0,0";
    body["integer"] = "5";
    body["numerator"] = "1";
    body["denomerator"] = "3";
    EXPECT_EQ(PostSolverAction(client, body, "5+1/3;"), "5+1/3;=5(1)/(3), id=\"0,3,0,1,0,0\"");

    //rational number
    body = Json::Value();
    body["solver_guid"] = "00000000-0000-0000-0000-000000000009";
    body["result_type"] = 3;
    body["expression"] = "2/3;";
    body["id"] = "0,3,0,2,0,0";
    body["numerator"] = "2";
    body["denomerator"] = "3";
    EXPECT_EQ(PostSolverAction(client, body, "2/3;"), "2/3;=(2)/(3), id=\"0,3,0,2,0,0\"");

    //integer number
    body = Json::Value();
    body["solver_guid"] = "00000000-0000-0000-0000-000000000009";
    body["result_type"] = 2;
    body["expression"] = "6*7;";
    body["id"] = "0,2,0,1,0,0";
    body["integer"] = "42";
    EXPECT_EQ(PostSolverAction(client, body, "6*7;"), "6*7;=42, id=\"0,2,0,1,0,0\"");

    //integer value
    body = Json::Value();
    body["solver_guid"] = "00000000-0000-0000-0000-000000000009";
    body["result_type"] = 2;
    body["expression"] = "pow(2,10);";
    body["id"] = "0,2,0,2,0,0";
    body["value"] = "1024";
    EXPECT_EQ(PostSolverAction(client, body, "pow(2,10);"), "pow(2,10);=1024, id=\"0,2,0,2,0,0\"");
}

//Array elements with exponent and unit, im only, re only, single complex pair, trigonometric form and empty elements
TEST_F(SolverActionTest, solver_action10)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    Json::Value body;
    body["solver_guid"] = "00000000-0000-0000-0000-000000000010";
    body["result_type"] = 6;
    body["id"] = "0,21,0,0,0,0,2,0";

    //elements with exponent and unit
    body["expression"] = "[1.5*10^3,2];";
    Json::Value results(Json::arrayValue);
    Json::Value element = MakeNumber("1.5", "3");
    Json::Value unit_name;
    unit_name["name"] = "m";
    element["unit"]["value"].append(unit_name);
    results.append(element);
    results.append(MakeNumber("2"));
    body["results"] = results;
    EXPECT_EQ(PostSolverAction(client, body, "[1.5*10^3,2];"), "[1.5*10^3,2];=[1.5*10^3[m],2], id=\"0,21,0,0,0,0,2,0\"");

    //element with im only
    body["expression"] = "arr2;";
    results = Json::Value(Json::arrayValue);
    element = Json::Value();
    element["im"] = MakeNumber("2");
    results.append(element);
    body["results"] = results;
    EXPECT_EQ(PostSolverAction(client, body, "arr2;"), "arr2;=[i*2], id=\"0,21,0,0,0,0,2,0\"");

    //element with re only
    body["expression"] = "arr3;";
    results = Json::Value(Json::arrayValue);
    element = Json::Value();
    element["re"] = MakeNumber("2");
    results.append(element);
    body["results"] = results;
    EXPECT_EQ(PostSolverAction(client, body, "arr3;"), "arr3;=[2], id=\"0,21,0,0,0,0,2,0\"");

    //single element with re and im
    body["expression"] = "arr4;";
    results = Json::Value(Json::arrayValue);
    element = Json::Value();
    element["re"] = MakeNumber("1");
    element["im"] = MakeNumber("2");
    results.append(element);
    body["results"] = results;
    EXPECT_EQ(PostSolverAction(client, body, "arr4;"), "arr4;=[1+i*2], id=\"0,21,0,0,0,0,2,0\"");

    //trigonometric form
    body["expression"] = "arr5;";
    results = Json::Value(Json::arrayValue);
    element = Json::Value();
    element["module"] = MakeNumber("2");
    element["argument"] = MakeNumber("0.5236");
    results.append(element);
    body["results"] = results;
    EXPECT_EQ(PostSolverAction(client, body, "arr5;"), "arr5;=[2∠0.5236], id=\"0,21,0,0,0,0,2,0\"");

    //all elements are empty, so the log has no result
    body["expression"] = "arr6;";
    results = Json::Value(Json::arrayValue);
    results.append(Json::Value());
    body["results"] = results;
    EXPECT_EQ(PostSolverAction(client, body, "arr6;"), "arr6;, id=\"0,21,0,0,0,0,2,0\"");
}

//Symbolic result with division, roots and powers and a broken json tree
TEST_F(SolverActionTest, solver_action11)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    Json::Value body;
    body["solver_guid"] = "00000000-0000-0000-0000-000000000011";
    body["result_type"] = 7;
    body["id"] = "0,27,0,3,0,0,2,0";
    body["expression"] = "simplify(root(x,3)+sqrt(x)+sin(x)/x+x^2);";
    body["json"] = "{\"type\":45,\"elements\":[{\"type\":7,\"elements\":["
        "{\"type\":17,\"elements\":[{\"type\":8,\"elements\":\"3\"},{\"type\":10,\"elements\":[]},"
        "{\"type\":7,\"elements\":[{\"type\":8,\"elements\":\"x\"}]}]},"
        "{\"type\":11,\"symbol\":\"+\"},"
        "{\"type\":16,\"elements\":[{\"type\":10,\"elements\":[]},{\"type\":7,\"elements\":[{\"type\":8,\"elements\":\"x\"}]}]},"
        "{\"type\":11,\"symbol\":\"+\"},"
        "{\"type\":14,\"elements\":[{\"type\":7,\"elements\":[{\"type\":8,\"elements\":\"sin\"},{\"type\":19,\"symbol\":\"(\"},"
        "{\"type\":8,\"elements\":\"x\"},{\"type\":20,\"symbol\":\")\"}]},{\"type\":10,\"elements\":[]},"
        "{\"type\":7,\"elements\":[{\"type\":8,\"elements\":\"x\"}]}]},"
        "{\"type\":11,\"symbol\":\"+\"},"
        "{\"type\":15,\"elements\":[{\"type\":7,\"elements\":[{\"type\":8,\"elements\":\"x\"}]},{\"type\":10,\"elements\":[]},"
        "{\"type\":7,\"elements\":[{\"type\":8,\"elements\":\"2\"}]}]}]}]}";
    EXPECT_EQ(PostSolverAction(client, body, "simplify(root(x,3)+sqrt(x)+sin(x)/x+x^2);"),
        "simplify(root(x,3)+sqrt(x)+sin(x)/x+x^2);=root(x,3)+sqrt(x)+(sin(x))/(x)+x^2, id=\"0,27,0,3,0,0,2,0\"");

    //broken json tree is logged without a result
    body["expression"] = "sum(x);";
    body["json"] = "{\"type\":45, broken";
    EXPECT_EQ(PostSolverAction(client, body, "sum(x);"), "sum(x);, id=\"0,27,0,3,0,0,2,0\"");
}

//Error code without description and a result together with an error
TEST_F(SolverActionTest, solver_action12)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    //error code only
    Json::Value body;
    body["solver_guid"] = "00000000-0000-0000-0000-000000000012";
    body["result_type"] = 0;
    body["expression"] = "e1;";
    body["id"] = "0,0,0,1,0,0";
    body["error"]["error_code"] = 5;
    EXPECT_EQ(PostSolverAction(client, body, "e1;"), "e1;=error_code:5, id=\"0,0,0,1,0,0\"");

    //result and error
    body = Json::Value();
    body["solver_guid"] = "00000000-0000-0000-0000-000000000012";
    body["result_type"] = 1;
    body["expression"] = "2+2;";
    body["id"] = "0,1,0,0,0,0";
    body["mantissa"] = "4";
    body["error"]["description"] = "Division by zero";
    EXPECT_EQ(PostSolverAction(client, body, "2+2;"), "2+2;=4Division by zero, id=\"0,1,0,0,0,0\"");
}

//SOLVE_CODE request is logged to the solver log
TEST_F(SolverActionTest, solver_action13)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    Json::Value body;
    body["solver_guid"] = "00000000-0000-0000-0000-000000000013";
    body["command"] = "SOLVE_CODE";
    body["expression"] = "2+2;";
    body["id"] = "0,1,0,0,0,0";
    body["expression_type"] = 1;
    body["result_type"] = 5;
    Json::Value order(Json::arrayValue);
    order.append(1);
    order.append(2);
    order.append(3);
    body["results_order"] = order;

    PostSolverReply(client, body);
    std::string line = LastLogLine("solver", "00000000-0000-0000-0000-000000000013", "SOLVE_CODE");
    EXPECT_TRUE(line.find("expression=2+2;") != std::string::npos) << line;
    EXPECT_TRUE(line.find("expression_type=1") != std::string::npos) << line;
    EXPECT_TRUE(line.find("id=\"0,1,0,0,0,0\"") != std::string::npos) << line;
    EXPECT_TRUE(line.find("results_order=[1,2,3]") != std::string::npos) << line;
    EXPECT_TRUE(line.find("result_type=5") != std::string::npos) << line;
}

//Empty expression writes nothing to the calculator log
TEST_F(SolverActionTest, solver_action14)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    Json::Value body;
    body["solver_guid"] = "00000000-0000-0000-0000-000000000014";
    body["result_type"] = 1;
    body["expression"] = "2+2;";
    body["id"] = "0,1,0,0,0,0";
    body["mantissa"] = "4";
    PostSolverReply(client, body);
    size_t count = CountCalculatorLines("00000000-0000-0000-0000-000000000014");

    body.removeMember("expression");
    PostSolverReply(client, body);
    EXPECT_EQ(CountCalculatorLines("00000000-0000-0000-0000-000000000014"), count);
}

//solver_guid with a path traversal is rejected and writes no files outside the solver log directory
TEST_F(SolverActionTest, solver_action15)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    Json::Value body;
    body["solver_guid"] = "../../tmp/x";
    body["result_type"] = 1;
    body["expression"] = "2+2;";
    body["id"] = "0,1,0,0,0,0";
    body["mantissa"] = "4";

    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/solver-action");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k400BadRequest) << r->getStatusCode();
    const auto json = r->jsonObject();
    ASSERT_TRUE(json != nullptr);
    EXPECT_EQ((*json)["error"].asString(), "Guid error");

    //log/yutovo-server/solver/../../tmp/x resolves to log/tmp/x
    EXPECT_FALSE(std::filesystem::exists("log/tmp"));
}

//session_id cookie with a path traversal is rejected and writes no files outside the sessions log directory
TEST_F(SolverActionTest, solver_action16)
{
    auto client = HttpClient::newHttpClient(address);

    Json::Value body;
    body["solver_guid"] = "00000000-0000-0000-0000-000000000016";
    body["result_type"] = 1;
    body["expression"] = "2+2;";
    body["id"] = "0,1,0,0,0,0";

    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/service/solver-action");
    req->addHeader("cookie", "session_id=../../evil");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k403Forbidden) << r->getStatusCode();

    //log/yutovo-server/sessions/../../evil resolves to log/evil
    EXPECT_FALSE(std::filesystem::exists("log/evil"));
}

}
