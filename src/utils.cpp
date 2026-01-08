/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "utils.h"
#include <regex>

namespace yutovo_server
{

extern std::string GetDeployPath();

yutovo::Logger* logger = yutovo::Logger::GetInstance(GetDeployPath() + "/log/yutovo-server", "server", true, true);

yutovo::Logger* GetLogger(const std::string& session_id)
{
    if (session_id.empty())
        return logger;
    return yutovo::Logger::GetInstance(GetDeployPath() + "/log/yutovo-server/sessions/" + session_id, "server", true, true);
}

yutovo::Logger* GetSolverLogger(const std::string& solver_id)
{
    return yutovo::Logger::GetInstance(GetDeployPath() + "/log/yutovo-server/solver/" + solver_id, "solver", true, true);
}

yutovo::Logger* GetCalculatorLogger(const std::string& solver_id)
{
    return yutovo::Logger::GetInstance(GetDeployPath() + "/log/yutovo-server/calculator/" + solver_id, "calculator", true, true);
}

bool IsGuid(const std::string& str)
{
    static std::regex guid("(^([0-9A-Fa-f]{8}[-]?[0-9A-Fa-f]{4}[-]?[0-9A-Fa-f]{4}[-]?[0-9A-Fa-f]{4}[-]?[0-9A-Fa-f]{12})$)");
    return std::regex_match(str, guid);
}

std::string GetDeployPath()
{
    char* p = std::getenv("YUTOVO_DEPLOY");
    return p ? p : ".";
}

}
