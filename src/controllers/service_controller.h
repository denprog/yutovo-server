#ifndef __SERVICE_CONTROLLER_H__
#define __SERVICE_CONTROLLER_H__

#include "controller_base.h"
#include <drogon/WebSocketClient.h>

using namespace drogon;

namespace yutovo_server
{
class ServiceController : public drogon::HttpController<ServiceController>, public ControllerBase
{
public:
    ServiceController();

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ServiceController::GetTasks, "/service/get-tasks", Get);
    ADD_METHOD_TO(ServiceController::LoadTask, "/service/load-task", Post); //load from file
    ADD_METHOD_TO(ServiceController::ListIdentifiers, "/service/list-identifiers", Post);
    ADD_METHOD_TO(ServiceController::NewDocument, "/service/new-document", Post); //create a document in the DB or replace an old one with an empty one
    ADD_METHOD_TO(ServiceController::SaveDocument, "/service/save-document", Post); //save to the DB
    ADD_METHOD_TO(ServiceController::LoadDocument, "/service/load-document", Post); //load from the DB
    METHOD_LIST_END

    void GetTasks(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void LoadTask(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void ListIdentifiers(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void NewDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void SaveDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void LoadDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);

private:
    std::string tasks_path;
    WebSocketClientPtr solver_client;
    std::string solver_response;
    const int solver_timeout = 5;
};
};

#endif
