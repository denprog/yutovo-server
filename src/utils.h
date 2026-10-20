/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <string>
#include <yutovo-logger/logger.h>

namespace yutovo_server
{

extern yutovo::Logger* logger;

yutovo::Logger* GetLogger(const std::string& session_id);
yutovo::Logger* GetSolverLogger(const std::string& solver_id);
yutovo::Logger* GetCalculatorLogger(const std::string& solver_id);
yutovo::Logger* GetUpdatesLogger(const std::string& session_id);

bool IsGuid(const std::string& str);

class MailUtils
{
public:
    static bool IsEmail(const std::string& str);
    static std::string StripHeaderNewlines(const std::string& value);
    static std::string EncodeSubject(const std::string& subject);
    static std::string QuotedPrintableEncode(const std::string& value);
};

extern std::string GetDeployPath();

}

#endif
