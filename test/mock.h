/*
 * Yutovo Server
 * Copyright (C) 2022-2025 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef __MOCK_H__
#define __MOCK_H__

#include <gmock/gmock.h>
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

}

#endif
