/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef __MOCK_H__
#define __MOCK_H__

#include <gmock/gmock.h>
#include <fstream>
#include <drogon/drogon.h>

namespace yutovo_server_test
{

using namespace drogon;

typedef unsigned int uint;

extern int argc;
extern char** argv;

extern std::string address;

struct TestBase
{
    TestBase();
    
    void StartPage(HttpClientPtr client);
    
    void Register(HttpClientPtr client, std::string login, std::string email, std::string password);
    void Register(HttpClientPtr client, std::string login, std::string email, std::string password, std::string name);
    void UnRegister(HttpClientPtr client, std::string login, std::string& access_token);

    void Locate(HttpClientPtr client, const std::string& path);

    void Login(HttpClientPtr client, std::string login, std::string password, HttpResponsePtr& r);
    void Login(HttpClientPtr client, std::string login, std::string password, std::string& access_token, std::string& document_id, std::string& name, 
        std::string& language);
    void Login(HttpClientPtr client, std::string login, std::string password, std::string& access_token, std::string& document_id, std::string& name, 
        std::string& language, std::string& settings);
    void Logout(HttpClientPtr client, const std::string& login, const std::string& access_token);

    void NewDocument(HttpClientPtr client, const std::string& access_token, std::string& document_id);
    void SaveDocument(HttpClientPtr client, const std::string file_name, const std::string& access_token, std::string& document_id);
    void LoadDocument(HttpClientPtr client, const std::string& access_token, const std::string document_id, std::shared_ptr<Json::Value>& document);
    void DeleteDocument(HttpClientPtr client, const std::string& access_token, const std::string document_id = "");
    void RenameDocument(HttpClientPtr client, const std::string& access_token, const std::string& document_id, const std::string& name);
    void GetDocumentName(HttpClientPtr client, const std::string& document_id, std::string& name);
};

struct AuthTest : public testing::Test, TestBase
{
};

struct SessionTest : public testing::Test, TestBase
{
};

struct ServiceTest : public testing::Test, TestBase
{
};

//Post a solver request or reply to the service
struct SolverActionTest : public ServiceTest
{
    static void PostSolverReply(const HttpClientPtr& client, const Json::Value& body)
    {
        auto req = HttpRequest::newHttpJsonRequest(body);
        req->setMethod(drogon::Post);
        req->setPath("/service/solver-action");

        auto resp = client->sendRequest(req);
        ReqResult& res = resp.first;
        HttpResponsePtr& r = resp.second;
        EXPECT_TRUE(res == ReqResult::Ok) << res;
        EXPECT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    }

    //Returns the last message of the calculator or solver log containing substr
    static std::string LastLogLine(const std::string& logger_name, const std::string& guid, const std::string& substr)
    {
        std::string dir = "log/yutovo-server/" + logger_name + "/" + guid;
        std::string line;
        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            std::ifstream f(entry.path());
            std::string s;
            while (std::getline(f, s))
            {
                if (s.find(substr) != std::string::npos)
                    line = s;
            }
        }
        EXPECT_FALSE(line.empty());
        size_t pos = line.find("[info] ");
        if (pos != std::string::npos)
            line = line.substr(pos + 7);
        return line;
    }

    //Post a solver reply and return the last calculator log message for the expression
    std::string PostSolverAction(const HttpClientPtr& client, const Json::Value& body, const std::string& expression)
    {
        PostSolverReply(client, body);
        return LastLogLine("calculator", body["solver_guid"].asString(), expression);
    }

    //A number object with mantissa and optional exponent
    static Json::Value MakeNumber(const std::string& mantissa, const std::string& exponent = "")
    {
        Json::Value obj;
        obj["mantissa"] = mantissa;
        if (!exponent.empty())
            obj["exponent"] = exponent;
        return obj;
    }

    static size_t CountCalculatorLines(const std::string& guid)
    {
        std::string dir = "log/yutovo-server/calculator/" + guid;
        size_t count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            std::ifstream f(entry.path());
            std::string s;
            while (std::getline(f, s))
                ++count;
        }
        return count;
    }
};

}

#endif
