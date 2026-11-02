/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "mock.h"
#include <fstream>

namespace yutovo_server_test
{

int argc = 0;
char** argv = nullptr;

std::string address = "http://www.yutovo.ru:9001";

//TestBase

TestBase::TestBase()
{
    drogon::orm::DbClientPtr db = drogon::app().getDbClient();
    db->execSqlSync("delete from user_sessions where user_id in (select user_id from users where login='User1' or login='User2')");
    db->execSqlSync("delete from user_documents where user_id in (select user_id from users where login='User1' or login='User2')");
    db->execSqlSync("delete from users where login='User1' or login='User2'");
}

void TestBase::StartPage(HttpClientPtr client)
{
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");
    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
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

void TestBase::Register(HttpClientPtr client, std::string login, std::string email, std::string password, std::string name)
{
    Json::Value body;
    body["login"] = login;
    body["email"] = email;
    body["password"] = password;
    body["name"] = name;
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

void TestBase::Locate(HttpClientPtr client, const std::string& path)
{
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath(path);
    client->sendRequest(req);
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

void TestBase::Login(HttpClientPtr client, std::string login, std::string password, std::string& access_token, std::string& document_id, std::string& name, 
    std::string& language)
{
    HttpResponsePtr r;
    Login(client, login, password, r);
    access_token = r->getHeader("access_token");
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    const auto json = r->jsonObject();
    document_id = (*json)["document_id"].asString();
    name = (*json)["name"].asString();
    language = (*json)["language"].asString();
}

void TestBase::Login(HttpClientPtr client, std::string login, std::string password, std::string& access_token, std::string& document_id, std::string& name, 
    std::string& language, std::string& settings)
{
    HttpResponsePtr r;
    Login(client, login, password, r);
    access_token = r->getHeader("access_token");
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_JSON) << r->getContentType();
    const auto json = r->jsonObject();
    document_id = (*json)["document_id"].asString();
    name = (*json)["name"].asString();
    language = (*json)["language"].asString();
    settings = (*json)["settings"].asString();
}

void TestBase::Logout(HttpClientPtr client, const std::string& login, const std::string& access_token)
{
    Json::Value login_body;
    login_body["login"] = login;
    auto req = HttpRequest::newHttpJsonRequest(login_body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/logout");
    req->addHeader("access_token", access_token);
    auto resp = client->sendRequest(req, 10);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
}

void TestBase::NewDocument(HttpClientPtr client, const std::string& access_token, std::string& document_id)
{
    Json::Value s;
    s["document_id"] = -1;
    auto req = HttpRequest::newHttpJsonRequest(s);
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
}

void TestBase::LoadDocument(HttpClientPtr client, const std::string& access_token, const std::string document_id, std::shared_ptr<Json::Value>& document)
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
    document = r->jsonObject();
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

//SsrTest

Json::Value SsrTest::MakeString(const std::string& text)
{
    Json::Value element;
    element["type"] = 4;
    element["elements"] = text;
    element["format_id"] = "02546dfa-a44e-4781-8924-23b0f7a49fb0";
    return element;
}

Json::Value SsrTest::MakeRow(const Json::Value& child)
{
    Json::Value element;
    element["type"] = 3;
    element["elements"].append(child);
    return element;
}

Json::Value SsrTest::MakeParagraph(const std::string& format_name, const Json::Value& row, const std::string& marker)
{
    Json::Value element;
    element["type"] = 2;
    element["elements"].append(row);
    element["format_name"] = format_name;
    element["format_alignment"] = 0;
    if (!marker.empty())
        element["marker"] = marker;
    return element;
}

Json::Value SsrTest::MakeDocument(const Json::Value& paragraphs)
{
    Json::Value format;
    format["id"] = "02546dfa-a44e-4781-8924-23b0f7a49fb0";
    format["family"] = "Arial";
    format["size"] = 14;

    Json::Value document;
    document["config"]["language"] = 2;
    document["string_formats"].append(format);
    document["text"]["type"] = 1;
    document["text"]["elements"] = paragraphs;
    return document;
}

Json::Value SsrTest::MakeCodeString(const std::string& text)
{
    Json::Value element;
    element["type"] = 8;
    element["elements"] = text;
    element["can_merge"] = true;
    return element;
}

Json::Value SsrTest::MakeShape()
{
    Json::Value element;
    element["type"] = 10;
    return element;
}

Json::Value SsrTest::MakeOperator(int type, const std::string& symbol)
{
    Json::Value element;
    element["type"] = type;
    element["elements"].append(MakeShape());
    element["symbol"] = symbol;
    return element;
}

Json::Value SsrTest::MakeCodeRow(const Json::Value& first)
{
    Json::Value element;
    element["type"] = 7;
    element["elements"].append(first);
    return element;
}

Json::Value SsrTest::MakeLeftRight(int type, const Json::Value& first, const Json::Value& second)
{
    Json::Value element;
    element["type"] = type;
    element["elements"].append(first);
    element["elements"].append(MakeShape());
    element["elements"].append(second);
    return element;
}

Json::Value SsrTest::MakeCodeParagraph(const Json::Value& child)
{
    Json::Value element;
    element["type"] = 6;
    element["format_name"] = "Код";
    element["elements"].append(child);
    return element;
}

Json::Value SsrTest::MakeCodeBlock(const Json::Value& paragraph)
{
    Json::Value element;
    element["type"] = 5;
    element["code_id"] = 1;
    element["elements"].append(paragraph);
    return element;
}

}
