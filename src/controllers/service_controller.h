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
    ADD_METHOD_TO(ServiceController::GetLibraryDocuments, "/service/get-library-documents", Post); //get list of library documents for the language
    ADD_METHOD_TO(ServiceController::LoadLibraryDocument, "/service/load-library-document", Post); //load from file
    ADD_METHOD_TO(ServiceController::SaveLibraryDocument, "/service/save-library-document", Post, "yutovo_server::LoginFilter"); //save to the DB
#ifdef REMOTE_SOLVER
    ADD_METHOD_TO(ServiceController::ListIdentifiers, "/service/list-identifiers", Post);
#endif
    ADD_METHOD_TO(ServiceController::NewDocument, "/service/new-document", Post, "yutovo_server::LoginFilter"); //create a document in the DB or replace with an empty one
    ADD_METHOD_TO(ServiceController::SaveDocument, "/service/save-document", Post, "yutovo_server::LoginFilter"); //save to the DB
    ADD_METHOD_TO(ServiceController::SaveAsDocument, "/service/save-as-document", Post, "yutovo_server::LoginFilter"); //save as to the DB
    ADD_METHOD_TO(ServiceController::LoadDocument, "/service/load-document", Post); //load from the DB
    ADD_METHOD_TO(ServiceController::LoadIncludeDocument, "/service/load-include-document", Post); //load from the DB
    ADD_METHOD_TO(ServiceController::DeleteDocument, "/service/delete-document", Post, "yutovo_server::LoginFilter"); //delete a document from the DB
    ADD_METHOD_TO(ServiceController::ListDocuments, "/service/list-documents", Post, "yutovo_server::LoginFilter"); //list user documents
    ADD_METHOD_TO(ServiceController::GetDocumentId, "/service/get-document-id", Post); //get id of a user document
    ADD_METHOD_TO(ServiceController::GetDocumentName, "/service/get-document-name", Post); //get name of a user document
    ADD_METHOD_TO(ServiceController::RenameDocument, "/service/rename-document", Post, "yutovo_server::LoginFilter"); //rename a user document
    ADD_METHOD_TO(ServiceController::SetUserSettings, "/service/set-user-settings", Post, "yutovo_server::LoginFilter"); //set user config
    ADD_METHOD_TO(ServiceController::GetUserSettings, "/service/get-user-settings", Post, "yutovo_server::LoginFilter"); //get user config
    ADD_METHOD_TO(ServiceController::RecoverPassword, "/service/recover-password", Post); //recover user password
    ADD_METHOD_TO(ServiceController::SolverAction, "/service/solver-action", Post); //solver ation
    METHOD_LIST_END

    void GetLibraryDocuments(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void LoadLibraryDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void SaveLibraryDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void ListIdentifiers(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void NewDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void SaveDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void SaveAsDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void LoadDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void LoadIncludeDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void DeleteDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void ListDocuments(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void GetDocumentId(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void GetDocumentName(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void RenameDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void SetUserSettings(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void GetUserSettings(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void RecoverPassword(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void SolverAction(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);

private:
    void SendLibraryDocument(const HttpRequestPtr& req, const std::string& document, std::function<void (const HttpResponsePtr &)>& callback);

#ifdef REMOTE_SOLVER
    WebSocketClientPtr solver_client;
    std::string solver_response;
    const int solver_timeout = 5;
#endif
};
};

#endif
