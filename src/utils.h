/*
 * Yutovo Server
 * Copyright (C) 2022-2025 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <yutovo-logger/logger.h>

namespace yutovo_server
{

extern yutovo::Logger* logger;

yutovo::Logger* GetLogger(const std::string& session_id);
yutovo::Logger* GetSolverLogger(const std::string& solver_id);
yutovo::Logger* GetCalculatorLogger(const std::string& solver_id);

bool IsGuid(const std::string& str);

extern std::string GetDeployPath();

}

#endif
