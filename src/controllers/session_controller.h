#ifndef __SESSION_CONTROLLER_H__
#define __SESSION_CONTROLLER_H__

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
    ADD_METHOD_TO(SessionController::Root, "/{1}", Get);
    ADD_METHOD_TO(SessionController::Assets, "/assets/{1}", Get);
    ADD_METHOD_TO(SessionController::Icons, "/icons/{1}", Get);
    ADD_METHOD_TO(SessionController::Images, "/images/{1}", Get);
    ADD_METHOD_TO(SessionController::Document, "/document/{1}", Get);
    METHOD_LIST_END

    void Root(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param);
    void Assets(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param);
    void Icons(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param);
    void Images(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param);
    void Document(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param);
};
}

#endif
