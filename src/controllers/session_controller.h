/*
 * Yutovo Server
 * Copyright (C) 2022-2025 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef __SESSION_CONTROLLER_H__
#define __SESSION_CONTROLLER_H__

#include <map>
#include <drogon/HttpClient.h>
#include "controller_base.h"

using namespace drogon;
using namespace yutovo;

namespace yutovo_server
{

class SessionController : public drogon::HttpController<SessionController>, public ControllerBase
{
public:
    SessionController();

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(SessionController::Root, "/{1}?ref={2}", Get);
    ADD_METHOD_TO(SessionController::Assets, "/assets/{1}", Get);
    ADD_METHOD_TO(SessionController::Icons, "/icons/{1}", Get);
    ADD_METHOD_TO(SessionController::Images, "/images/{1}", Get);
    ADD_METHOD_TO(SessionController::UserDocument, "/document/{1}", Get);
    ADD_METHOD_VIA_REGEX(SessionController::LibraryDocument, "/library/(.*)", Get);
    ADD_METHOD_TO(SessionController::Downloads, "/downloads/{1}", Get);
    METHOD_LIST_END

    void Root(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param, std::string ref);
    void Assets(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param);
    void Icons(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param);
    void Images(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param);
    void UserDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param);
    void LibraryDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string path);
    void Downloads(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param);

private:
    std::map<std::string, HttpClientPtr> cors_clients;

    Logger* ref_logger = Logger::GetInstance(GetDeployPath() + "/log/yutovo_server/references", "server", true, true);
    Logger* downloads_logger = Logger::GetInstance(GetDeployPath() + "/log/yutovo_server/downloads", "server", true, true);
};

}

#endif
