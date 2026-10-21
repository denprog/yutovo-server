/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <string>
#include <cstdint>
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

    enum class Verdict
    {
        kAccepted, //the submitted code matches the live issued code
        kRejected, //the code is wrong, but the attempt limit is not reached yet
        kInvalidated //the code is expired or the attempt limit is reached; it must be erased
    };

    static constexpr int max_attempts = 5;
    static constexpr int ttl_seconds = 600;

    static Verdict Check(const std::string& stored, const std::string& submitted, int failed_attempts, int64_t age_seconds);
};

extern std::string GetDeployPath();

}

#endif
