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
    ADD_METHOD_TO(ServiceController::GetTasks, "/service/get-tasks", Post); //get list of tasks for a language
    ADD_METHOD_TO(ServiceController::LoadTask, "/service/load-task", Post); //load from file
    ADD_METHOD_TO(ServiceController::ListIdentifiers, "/service/list-identifiers", Post);
    ADD_METHOD_TO(ServiceController::NewDocument, "/service/new-document", Post, "yutovo_server::LoginFilter"); //create a document in the DB or replace with an empty one
    ADD_METHOD_TO(ServiceController::SaveDocument, "/service/save-document", Post, "yutovo_server::LoginFilter"); //save to the DB
    ADD_METHOD_TO(ServiceController::SaveAsDocument, "/service/save-as-document", Post, "yutovo_server::LoginFilter"); //save as to the DB
    ADD_METHOD_TO(ServiceController::LoadDocument, "/service/load-document", Post); //load from the DB
    ADD_METHOD_TO(ServiceController::DeleteDocument, "/service/delete-document", Post, "yutovo_server::LoginFilter"); //delete a document from the DB
    ADD_METHOD_TO(ServiceController::ListDocuments, "/service/list-documents", Post, "yutovo_server::LoginFilter"); //list user documents
    ADD_METHOD_TO(ServiceController::GetDocumentName, "/service/get-document-name", Post); //get name of a user document
    ADD_METHOD_TO(ServiceController::RenameDocument, "/service/rename-document", Post, "yutovo_server::LoginFilter"); //rename a user document
    METHOD_LIST_END

    void GetTasks(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void LoadTask(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void ListIdentifiers(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void NewDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void SaveDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void SaveAsDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void LoadDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void DeleteDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void ListDocuments(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void GetDocumentName(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void RenameDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);

private:
    std::string tasks_path;
    WebSocketClientPtr solver_client;
    std::string solver_response;
    const int solver_timeout = 5;
};
};

#endif
