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
    ADD_METHOD_TO(SessionController::Root, "/{1}", Get);
    ADD_METHOD_TO(SessionController::Assets, "/assets/{1}", Get);
    ADD_METHOD_TO(SessionController::Icons, "/icons/{1}", Get);
    ADD_METHOD_TO(SessionController::Images, "/images/{1}", Get);
    ADD_METHOD_TO(SessionController::UserDocument, "/document/{1}", Get);
    ADD_METHOD_TO(SessionController::LibraryDocument1, "/library/{1}/{2}", Get);
    ADD_METHOD_TO(SessionController::LibraryDocument2, "/library/{1}/{2}/{3}", Get);
    ADD_METHOD_TO(SessionController::LibraryDocument3, "/library/{1}/{2}/{3}/{4}", Get);
    ADD_METHOD_TO(SessionController::LibraryDocument4, "/library/{1}/{2}/{3}/{4}/{5}", Get);
    ADD_METHOD_TO(SessionController::Downloads, "/downloads/{1}", Get);
    ADD_METHOD_VIA_REGEX(SessionController::Cors, "/cors/(.*)", Get); //reverse proxy for Yandex Metrica
    ADD_METHOD_VIA_REGEX(SessionController::Cors, "/cors/(.*)", Post);
    METHOD_LIST_END

    void Root(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param);
    void Assets(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param);
    void Icons(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param);
    void Images(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param);
    void UserDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param);
    void LibraryDocument1(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, 
        std::string language, std::string filename);
    void LibraryDocument2(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, 
        std::string language, std::string dir1, std::string filename);
    void LibraryDocument3(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, 
        std::string language, std::string dir1, std::string dir2, std::string filename);
    void LibraryDocument4(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, 
        std::string language, std::string dir1, std::string dir2, std::string dir3, std::string filename);
    void Downloads(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param);
    void Cors(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param);

private:
    void LibraryDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>& callback, std::string language, std::string path);

private:
    std::map<std::string, HttpClientPtr> cors_clients;
};

}

#endif
