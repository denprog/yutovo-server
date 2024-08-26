#ifndef __SERVICE_CONTROLLER_H__
#define __SERVICE_CONTROLLER_H__

#include "controller_base.h"
#include <drogon/WebSocketClient.h>

using namespace drogon;

namespace yutovo_server
{
class SolverController : public drogon::HttpController<SolverController>, public ControllerBase
{
public:
    SolverController();

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(SolverController::GetTasks, "/service/get-tasks", Post); //get list of tasks for a language
    ADD_METHOD_TO(SolverController::LoadTask, "/service/load-task", Post); //load from file
    ADD_METHOD_TO(SolverController::SaveTask, "/service/save-task", Post, "yutovo_server::LoginFilter"); //save to the DB
#ifdef REMOTE_SOLVER
    ADD_METHOD_TO(SolverController::ListIdentifiers, "/service/list-identifiers", Post);
#endif
    ADD_METHOD_TO(SolverController::NewDocument, "/service/new-document", Post, "yutovo_server::LoginFilter"); //create a document in the DB or replace with an empty one
    ADD_METHOD_TO(SolverController::SaveDocument, "/service/save-document", Post, "yutovo_server::LoginFilter"); //save to the DB
    ADD_METHOD_TO(SolverController::SaveAsDocument, "/service/save-as-document", Post, "yutovo_server::LoginFilter"); //save as to the DB
    ADD_METHOD_TO(SolverController::LoadDocument, "/service/load-document", Post); //load from the DB
    ADD_METHOD_TO(SolverController::DeleteDocument, "/service/delete-document", Post, "yutovo_server::LoginFilter"); //delete a document from the DB
    ADD_METHOD_TO(SolverController::ListDocuments, "/service/list-documents", Post, "yutovo_server::LoginFilter"); //list user documents
    ADD_METHOD_TO(SolverController::GetDocumentName, "/service/get-document-name", Post); //get name of a user document
    ADD_METHOD_TO(SolverController::RenameDocument, "/service/rename-document", Post, "yutovo_server::LoginFilter"); //rename a user document
    ADD_METHOD_TO(SolverController::SetSettings, "/service/set-settings", Post, "yutovo_server::LoginFilter"); //set user config
    ADD_METHOD_TO(SolverController::GetSettings, "/service/get-settings", Post, "yutovo_server::LoginFilter"); //get user config
    METHOD_LIST_END

    void GetTasks(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void LoadTask(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void SaveTask(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void ListIdentifiers(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void NewDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void SaveDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void SaveAsDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void LoadDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void DeleteDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void ListDocuments(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void GetDocumentName(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void RenameDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void SetSettings(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void GetSettings(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);

private:
#ifdef REMOTE_SOLVER
    WebSocketClientPtr solver_client;
    std::string solver_response;
    const int solver_timeout = 5;
#endif
};
};

#endif
