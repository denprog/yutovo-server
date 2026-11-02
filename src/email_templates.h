/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef __EMAIL_TEMPLATES_H__
#define __EMAIL_TEMPLATES_H__

#include <string>

namespace yutovo_server
{

struct EmailTemplate
{
    std::string subject;
    std::string body_html;
};

class EmailTemplates
{
public:
    static EmailTemplate GetVerificationEmailTemplate(const std::string& action, const std::string& language);

private:
    static std::string BuildVerificationHtml(const std::string& subject, const std::string& greeting, const std::string& body1,
        const std::string& body2, const std::string& ignore, const std::string& sign);
};

}

#endif
