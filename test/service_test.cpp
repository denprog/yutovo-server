/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <gtest/gtest.h>
#include <fstream>
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
    orm::Result plan_result = db->execSqlSync(
        "select max_files from user_plans where plan_id=(select plan_id from users where login=$1)",
        "User1");
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

}
