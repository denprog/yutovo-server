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

//Create a session without login by access on "/"
TEST_F(SessionTest, session1)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    std::string location = r->getHeader("location");
    ASSERT_TRUE(location == "") << location;
}

//Create a session without login and later log into it
TEST_F(SessionTest, session2)
{
    auto client = HttpClient::newHttpClient(address);
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

    drogon::Cookie session_cookie = c;

    Register(client, "User1", "user1@mail.com", "11");

    Login(client, "User1", "11", r);
    std::string access_token = r->getHeader("access_token");
    c = r->getCookie("session_id");
    ASSERT_TRUE(c.value() == session_cookie.value());

    orm::DbClientPtr db = app().getDbClient();
    orm::Result result = db->execSqlSync("select user_id from user_sessions where session_id=$1", c.value());
    ASSERT_TRUE(result.size() != 0);
    auto row = result[0];
    auto user_id = row["user_id"].as<int>();
    ASSERT_TRUE(user_id > 0);

    UnRegister(client, "User1", access_token);
    result = db->execSqlSync("select user_id from user_sessions where session_id=$1", c.value());
    ASSERT_TRUE(result.size() == 0);
    result = db->execSqlSync("select 1 from user_sessions where user_id=$1", user_id);
    ASSERT_TRUE(result.size() == 0);
    result = db->execSqlSync("select 1 from users where user_id=$1", user_id);
    ASSERT_TRUE(result.size() == 0);
}

}
