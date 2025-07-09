#include "auth_controller.h"
#include "../logic/clear_db.h"
#include "utils.h"
#include <jwt-cpp/jwt.h>
#include <fstream>
#include <system_error>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include <functional>
#include <random>
#include <drogon/plugins/RealIpResolver.h>

namespace yutovo_server
{

//AuthController

using namespace cimg_library;

AuthController::AuthController()
{
    const Json::Value& v = app().getCustomConfig();
    session_expires = v.get("user_session_expire_timeout", 84600).asInt();
}

void AuthController::GetCaptcha(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;
    GetLogger(session_id)->Debug("Get captcha request");

    //generate captcha text (6 char max).
    const char *predef_words[] = 
        {
            "aarrgh", "abacas", "abacus", "abakas", "abamps", "abased", "abaser", "abases", "abasia", "abated", "abater",
            "abates", "abatis", "abator", "baobab", "barbal", "barbed", "barbel", "barber", "barbes", "barbet", "barbie",
            "barbut", "barcas", "barded", "bardes", "bardic", "barege", "cavies", "cavils", "caving", "cavity", "cavort",
            "cawing", "cayman", "cayuse", "ceased", "ceases", "cebids", "ceboid", "cecity", "cedarn", "dicast", "dicers",
            "dicier", "dicing", "dicker", "dickey", "dickie", "dicots", "dictum", "didact", "diddle", "diddly", "didies",
            "didoes", "emails", "embalm", "embank", "embark", "embars", "embays", "embeds", "embers", "emblem", "embody",
            "emboli", "emboly", "embosk", "emboss", "fluffy", "fluids", "fluish", "fluked", "flukes", "flukey", "flumed",
            "flumes", "flumps", "flunks", "flunky", "fluors", "flurry", "fluted", "genome", "genoms", "genres", "genros",
            "gentes", "gentil", "gentle", "gently", "gentry", "geodes", "geodic", "geoids", "gerahs", "gerbil", "hotter",
            "hottie", "houdah", "hounds", "houris", "hourly", "housed", "housel", "houser", "houses", "hovels", "hovers",
            "howdah", "howdie", "inland", "inlays", "inlets", "inlier", "inmate", "inmesh", "inmost", "innage", "innate",
            "inners", "inning", "inpour", "inputs", "inroad", "joypop", "jubbah", "jubhah", "jubile", "judder", "judged",
            "judger", "judges", "judoka", "jugate", "jugful", "jugged", "juggle", "jugula", "knifer", "knifes", "knight",
            "knives", "knobby", "knocks", "knolls", "knolly", "knosps", "knotty", "knouts", "knower", "knowns", "knubby",
            "legate", "legato", "legend", "legers", "legged", "leggin", "legion", "legist", "legits", "legman", "legmen",
            "legong", "legume", "lehuas", "mammal", "mammas", "mammee", "mammer", "mammet", "mammey", "mammie", "mammon",
            "mamzer", "manage", "manana", "manats", "manche", "manege", "nihils", "nilgai", "nilgau", "nilled", "nimble",
            "nimbly", "nimbus", "nimmed", "nimrod", "ninety", "ninjas", "ninons", "ninths", "niobic", "offish", "offkey",
            "offset", "oftest", "ogdoad", "oghams", "ogival", "ogives", "oglers", "ogling", "ogress", "ogrish", "ogrism",
            "ohmage", "papaws", "papaya", "papers", "papery", "pappus", "papula", "papule", "papyri", "parade", "paramo",
            "parang", "paraph", "parcel", "pardah", "quasar", "quatre", "quaver", "qubits", "qubyte", "queans", "queasy",
            "queazy", "queens", "queers", "quelea", "quells", "quench", "querns", "raised", "raiser", "raises", "raisin",
            "raitas", "rajahs", "rakees", "rakers", "raking", "rakish", "rallye", "ralphs", "ramada", "ramate", "savory",
            "savour", "savoys", "sawers", "sawfly", "sawing", "sawlog", "sawney", "sawyer", "saxony", "sayeds", "sayers",
            "sayest", "sayids", "tondos", "toneme", "toners", "tongas", "tonged", "tonger", "tongue", "tonics", "tonier",
            "toning", "tonish", "tonlet", "tonner", "tonnes", "uredia", "uredos", "ureide", "uremia", "uremic", "ureter",
            "uretic", "urgent", "urgers", "urging", "urials", "urinal", "urines", "uropod", "villus", "vimina", "vinals",
            "vincas", "vineal", "vinery", "vinier", "vinify", "vining", "vinous", "vinyls", "violas", "violet", "violin",
            "webfed", "weblog", "wechts", "wedded", "wedder", "wedeln", "wedels", "wedged", "wedges", "wedgie", "weeded",
            "weeder", "weekly", "weened", "xystoi", "xystos", "xystus", "yabber", "yabbie", "yachts", "yacked", "yaffed",
            "yagers", "yahoos", "yairds", "yakked", "yakker", "yakuza", "zigged", "zigzag", "zillah", "zinced", "zincic",
            "zincky", "zinebs", "zinged", "zinger", "zinnia", "zipped", "zipper", "zirams", "zircon"
        };
    
    cimg::srand();
    const char *const captcha_text = predef_words[std::rand() % (sizeof(predef_words) / sizeof(char *))];

    //create captcha image, write colored and distorted text
    CImg<unsigned char> captcha(256, 64, 1, 3, 0), color(3);
    char letter[2] = {0};
    for (unsigned int k = 0; k < 6; ++k)
    {
        CImg<unsigned char> tmp;
        *letter = captcha_text[k];
        if (*letter)
        {
            cimg_forX(color, i) color[i] = (unsigned char)(128 + (std::rand() % 127));
            tmp.draw_text((int)(2 + 8 * cimg::rand()), (int)(12 * cimg::rand()), letter, color.data(), 0, 1, std::rand() % 2 ? 38 : 57).resize(-100, -100, 1, 3);
            const unsigned int dir = std::rand() % 4, wph = tmp.width() + tmp.height();
            cimg_forXYC(tmp, x, y, v)
            {
                const int val = dir == 0 ? x + y : (dir == 1 ? x + tmp.height() - y : (dir == 2 ? y + tmp.width() - x : tmp.width() - x + tmp.height() - y));
                tmp(x, y, v) = (unsigned char)std::max(0.0f, std::min(255.0f, 1.5f * tmp(x, y, v) * val / wph));
            }
            if (std::rand() % 2)
                tmp = (tmp.get_dilate(3) -= tmp);
            tmp.blur((float)cimg::rand() * 0.8f).normalize(0, 255);
            const float sin_offset = (float)cimg::rand(-1, 1) * 3, sin_freq = (float)cimg::rand(-1, 1) / 7;
            cimg_forYC(captcha, y, v) captcha.get_shared_row(y, 0, v).shift((int)(4 * std::cos(y * sin_freq + sin_offset)));
            captcha.draw_image(6 + 40 * k, tmp);
        }
    }

    //add geometric and random noise
    CImg<unsigned char> copy = (+captcha).fill(0);
    for (unsigned int l = 0; l < 3; ++l)
    {
        if (l)
            copy.blur(0.5f).normalize(0, 148);
        for (unsigned int k = 0; k < 10; ++k)
        {
            cimg_forX(color, i) color[i] = (unsigned char)(128 + cimg::rand() * 127);
            if (cimg::rand() < 0.5f)
                copy.draw_circle((int)(cimg::rand() * captcha.width()), (int)(cimg::rand() * captcha.height()), (int)(cimg::rand() * 30), color.data(), 0.6f, ~0U);
            else
                copy.draw_line((int)(cimg::rand() * captcha.width()), (int)(cimg::rand() * captcha.height()), (int)(cimg::rand() * captcha.width()),
                    (int)(cimg::rand() * captcha.height()), color.data(), 0.6f);
        }
    }

    captcha |= copy;
    captcha.noise(10, 2);
    captcha = (+captcha).fill(255) - captcha;

    SessionPtr session = req->session();
    session->erase("captcha");
    session->insert("captcha", std::string(captcha_text));

    SendCaptcha(callback, captcha);

    GetLogger(session_id)->Debug("Sent captcha: {}", captcha_text);
}

void AuthController::SendEmailCode(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    SessionPtr session = req->session();
    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }

    auto send_email = 
        [this, session_id, session](const std::string& email, std::string& subject, std::string& message)
        {
            std::random_device dev;
            std::mt19937 rng(dev());
            std::uniform_int_distribution<std::mt19937::result_type> dist6(0, 9);
            std::string email_code;
            for (int i = 0; i < 6; ++i)
                email_code += std::to_string(dist6(rng));
        
            size_t p = message.find("EMAIL_CODE");
            if (p != std::string::npos)
                message.replace(p, strlen("EMAIL_CODE"), email_code);
            const Json::Value& v = app().getCustomConfig();
            std::string email_name = v.get("email_name", "").asString();

            session->erase("email_code");
            session->erase("email_code_email");
            CURLcode r = SendEmail(email_name, email, subject, message);
            if (r == CURLE_OK)
            {
                session->insert("email_code", email_code);
                session->insert("email_code_email", email);
                GetLogger(session_id)->Info("Sent email code: {} to {}", email_code, email);
            }
            else
                GetLogger(session_id)->Error("Error sending email code: {} to {}: {}", email_code, email, (int)r);
        };

    std::string login, email, subject, message;

    orm::DbClientPtr db = app().getDbClient();
    ClearDbTurnOff t; //skip the clear db circles for a while

    if (!json->isMember("login") || !(*json)["login"].isString() || !json->isMember("email") || !(*json)["email"].isString())
    {
        if (!json->isMember("subject") || !(*json)["subject"].isString() || !json->isMember("message") || !(*json)["message"].isString() || 
            !json->isMember("captcha") || !(*json)["captcha"].isString())
        {
            SendError(k400BadRequest, "Wrong json in the request", session_id, callback);
            return;
        }

        subject = (*json)["subject"].asString();
        message = (*json)["message"].asString();
        GetLogger(session_id)->Info("SendEmailCode request: subject={}", subject);
        if (subject.empty() || message.empty())
        {
            SendError(k400BadRequest, "Fields must not be empty", session_id, callback);
            return;
        }

#ifndef TEST
        auto captcha = (*json)["captcha"].asString();
        if (captcha.empty() || session->get<std::string>("captcha") != captcha)
        {
            SendError(k400BadRequest, "Wrong captcha", session_id, callback);
            return;
        }
#endif

        if (json->isMember("email") && (*json)["email"].isString())
        {
            email = (*json)["email"].asString();
            if (email.empty())
            {
                SendError(k400BadRequest, "Fields must not be empty", session_id, callback);
                return;
            }
        }
        else if (json->isMember("login") && (*json)["login"].isString())
        {
            login = (*json)["login"].asString();
            if (login.empty())
            {
                SendError(k400BadRequest, "Fields must not be empty", session_id, callback);
                return;
            }

            try
            {
                //it may be email
                orm::Result result = db->execSqlSync("select 1 from users where email=$1", login);
                if (result.size() == 0)
                {
                    //it may be login
                    result = db->execSqlSync("select email from users where login=$1", login);
                    if (result.size() == 0)
                    {
                        SendError(k401Unauthorized, "Login or email are incorrect", session_id, callback);
                        return;
                    }
                    auto row = result[0];
                    email = row["email"].as<std::string>();
                }
                else
                {
                    email = login;
                }
            } 
            catch (const orm::DrogonDbException& e)
            {
                GetLogger(req->getCookie("session_id"))->Error("Database error: {}", e.base().what());
                SendError(k500InternalServerError, e.base().what(), session_id, callback);
                return;
            }
        }
        else
        {
            std::string user_id = session->get<std::string>("user_id");

            try
            {
                //get email
                orm::Result result = db->execSqlSync("select email from users where user_id=$1", user_id);
                if (result.size() == 0)
                {
                    SendError(k401Unauthorized, "Login or password are incorrect", session_id, callback);
                    return;
                }
                auto row = result[0];
                email = row["email"].as<std::string>();
            } 
            catch (const orm::DrogonDbException& e)
            {
                GetLogger(req->getCookie("session_id"))->Error("Database error: {}", e.base().what());
                SendError(k500InternalServerError, e.base().what(), session_id, callback);
                return;
            }
        }
    }
    else
    {
        if (!json->isMember("login") || !(*json)["login"].isString() || !json->isMember("email") || !(*json)["email"].isString() || 
            !json->isMember("subject") || !(*json)["subject"].isString() || !json->isMember("message") || !(*json)["message"].isString() || 
            !json->isMember("captcha") || !(*json)["captcha"].isString())
        {
            SendError(k400BadRequest, "Wrong json in the request", session_id, callback);
            return;
        }

        login = (*json)["login"].asString();
        email = (*json)["email"].asString();
        subject = (*json)["subject"].asString();
        message = (*json)["message"].asString();
        GetLogger(session_id)->Info("SendEmailCode request: login={}, email={}", login, email);
        if (login.empty() || email.empty() || subject.empty() || message.empty())
        {
            SendError(k400BadRequest, "Fields must not be empty", session_id, callback);
            return;
        }

#ifndef TEST
        auto captcha = (*json)["captcha"].asString();
        if (captcha.empty() || session->get<std::string>("captcha") != captcha)
        {
            SendError(k400BadRequest, "Wrong captcha", session_id, callback);
            return;
        }
#endif
    }

    if (!json->isMember("recover") || !(*json)["recover"].isBool() || !(*json)["recover"].asBool())
    {
        try
        {
            //check if such login or email already exists
            orm::Result result = db->execSqlSync("select 1 from users where login=$1 or email=$2", login, email);
            if (result.size() > 0)
            {
                GetLogger(req->getCookie("session_id"))->Error("Login or e-mail already exists: {}, {}", login, email);
                SendError(k409Conflict, "Login or e-mail already exists", session_id, callback);
                return;
            }
        } 
        catch (const orm::DrogonDbException& e)
        {
            GetLogger(req->getCookie("session_id"))->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), session_id, callback);
        }
    }

    send_email(email, subject, message);
    SendOk(callback);
}

void AuthController::Register(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, User&& user)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    GetLogger(session_id)->Info("Register request: login={}, email={}, name={}", user.login, user.email, user.name);

    SessionPtr session = req->session();
    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }

#ifndef TEST
    if (!json->isMember("email_code") || !(*json)["email_code"].isString() || !json->isMember("captcha") || !(*json)["captcha"].isString())
    {
        SendError(k400BadRequest, "Wrong json in the request", session_id, callback);
        return;
    }

    auto email_code = (*json)["email_code"].asString();
    if (email_code.empty() || session->get<std::string>("email_code") != email_code)
    {
        SendError(k400BadRequest, "Wrong email code", session_id, callback);
        return;
    }

    auto captcha = (*json)["captcha"].asString();
    if (captcha.empty() || session->get<std::string>("captcha") != captcha)
    {
        SendError(k400BadRequest, "Wrong captcha", session_id, callback);
        return;
    }
#endif

    orm::DbClientPtr db = app().getDbClient();
    if (user.login.empty() || user.email.empty() || user.password.empty())
    {
        SendError(k400BadRequest, "Fields must not be empty", session_id, callback);
        return;
    }

    try
    {
        //check if such login or email already exists
        orm::Result result = db->execSqlSync("select 1 from users where login=$1 or email=$2", user.login, user.email);
        if (result.size() > 0)
        {
            SendError(k409Conflict, "Login or e-mail already exists", session_id, callback);
            return;
        }

        //generate the hash of the password with salt
        std::string salt(boost::lexical_cast<std::string>(boost::uuids::random_generator()()));
        salt.erase(std::remove(salt.begin(), salt.end(), '-'), salt.end());
        std::string hash = GetHash(user.password, salt);

        //insert new user
        result = db->execSqlSync("insert into users (login, password, email, name) values ($1, $2, $3, $4)", user.login, salt + hash, user.email, user.name);
        if (result.size() == 0)
        {
            SendOk(callback);
            register_logger->Info("Register: login={}, ip={}", user.login, drogon::plugin::RealIpResolver::GetRealAddr(req).toIp());
        }
        else
        {
            GetLogger(req->getCookie("session_id"))->Error("Database error: {}", "Error of insert");
            SendError(k500InternalServerError, "Error of insert", session_id, callback);
        }
    } 
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(req->getCookie("session_id"))->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), session_id, callback);
    }
}

void AuthController::UnRegister(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;
    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }

    if (!json->isMember("login") || !(*json)["login"].isString())
    {
        SendError(k400BadRequest, "Wrong json in the request", session_id, callback);
        return;
    }

    auto login = (*json)["login"].asString();
    SessionPtr session = req->session();
    GetLogger(session_id)->Info("UnRegister request: login={}", login);
    orm::DbClientPtr db = app().getDbClient();
    std::string user_id = session->get<std::string>("user_id");
    if (user_id.empty() || user_id == "-1")
    {
        SendError(k400BadRequest, "User not found", session_id, callback);
        return;
    }

    try
    {
        orm::Result result = db->execSqlSync("delete from users where login=$1", login);
        if (result.affectedRows() == 0)
        {
            SendError(k404NotFound, "Login not found", session_id, callback);
            return;
        }
        db->execSqlSync("delete from user_sessions where user_id=$1", user_id);
        session->erase("user_id");
        SendOk(callback);
        register_logger->Info("Unregister: user_id={}, ip={}", user_id, drogon::plugin::RealIpResolver::GetRealAddr(req).toIp());
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), session_id, callback);
    }
}

void AuthController::Login(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;
    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }

    if (!json->isMember("login") || !(*json)["login"].isString() || !json->isMember("password") || !(*json)["password"].isString())
    {
        SendError(k400BadRequest, "Wrong json in the request", session_id, callback);
        return;
    }

    SessionPtr session = req->session();

    auto login = (*json)["login"].asString();
    auto password = (*json)["password"].asString();
    bool load_last = true;
    if (json->isMember("load_last") && (*json)["load_last"].isBool())
        load_last = (*json)["load_last"].asBool();

#ifndef TEST
    auto captcha = (*json)["captcha"].asString();
    if (captcha.empty() || session->get<std::string>("captcha") != captcha)
    {
        SendError(k400BadRequest, "Wrong captcha", session_id, callback);
        return;
    }
#endif

    logger->Info("Login request: login={}, load_last={}", login, load_last);
    orm::DbClientPtr db = app().getDbClient();

    std::string user_id;
    try
    {
        ClearDbTurnOff t; //skip the clear db circles for a while

        orm::Result result = db->execSqlSync("select user_id, password, language, settings from users where login=$1", login);
        if (result.size() == 0)
        {
            SendError(k401Unauthorized, "Login or password are incorrect", session_id, callback);
            return;
        }

        auto row = result[0];
        std::string hash = row["password"].as<std::string>();
        std::string salt = hash.substr(0, 32);
        std::string h = GetHash(password, salt);
        if (salt + h != hash)
        {
            SendError(k401Unauthorized, "Login or password are incorrect", session_id, callback);
            return;
        }

        //create a session with refresh and access tokens
        session->erase("login");
        session->insert("login", login);

        std::string refresh_uuid(boost::uuids::to_string(boost::uuids::random_generator()()));
        std::string access_uuid(boost::uuids::to_string(boost::uuids::random_generator()()));
        trantor::Date access_expires = trantor::Date::now().after(access_token_expires);
        trantor::Date refresh_expires = trantor::Date::now().after(refresh_token_expires);

        user_id = row["user_id"].as<std::string>();
        result = db->execSqlSync("insert into refresh_sessions (user_id, refresh_uuid, expire_time) values ($1, $2, $3)", 
            user_id, refresh_uuid, refresh_expires.secondsSinceEpoch());
        session->erase("user_id");
        session->insert("user_id", user_id);

        std::string document_id = "-1";
        std::string name;
        session->insert("session_id", session_id);
        auto session_expires_date = trantor::Date::now().after(session_expires);
        if (!session_id.empty())
        {
            if (load_last)
            {
                result = db->execSqlSync("select document_id from user_sessions where session_id=$1", session_id);
                if (result.size() > 0)
                {
                    auto row = result[0];
                    document_id = row["document_id"].as<std::string>();
                }
            }
            if (document_id == "-1")
            {
                int d = GetFirstEmptyDocument(user_id);
                if (d == -1)
                {
                    if (!AddDocument(req, user_id, document_id, name))
                    {
                        GetLogger(session_id)->Error("Database error: Error inserting a document");
                        SendError(k500InternalServerError, "Error inserting a document", session_id, callback);
                        return;
                    }
                }
                else
                    document_id = std::to_string(d);
            }

            //if a user has many logins, a session may have another login
            db->execSqlSync("delete from user_sessions where session_id=$1", session_id);
            //the user session starts to have an owner
            db->execSqlSync("insert into user_sessions (session_id, user_id, expire_time, document_id) values ($1, $2, $3, $4)", 
                session_id, user_id, session_expires_date.secondsSinceEpoch(), document_id);
        }

        std::string language = row["language"].as<std::string>();
        std::string settings = row["settings"].as<std::string>();

        result = db->execSqlSync("select max_files, max_solving_time, max_file_size from user_plans where plan_id=(select plan_id from users where user_id=$1)", user_id);
        if (result.size() == 0)
        {
            SendError(k500InternalServerError, "User plan is incorrect", session_id, callback);
            return;
        }
        auto r = result[0];
        session->insert("max_solving_time", r["max_solving_time"].as<int>());
        session->insert("max_file_size", r["max_file_size"].as<int>() * 1024);
        session->insert("max_files", r["max_files"].as<int>());

        SendOkTokens(callback, login, access_uuid, refresh_uuid, session_id, access_expires, refresh_expires, session_expires_date, 
            document_id, name, language, settings);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(req->getCookie("session_id"))->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), session_id, callback);
    }

    session->erase("captcha");
    auth_logger->Info("Login: login={}, user_id={}, ip={}", login, user_id, drogon::plugin::RealIpResolver::GetRealAddr(req).toIp());
}

void AuthController::Logout(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;
    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }

    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    std::string refresh_token = req->getCookie("refresh_token");

    if (!json->isMember("login") || !(*json)["login"].isString())
    {
        SendError(k400BadRequest, "Wrong json in the request", session_id, callback);
        return;
    }

    auto login = (*json)["login"].asString();
    std::string refresh_uuid;
    if (!ParseRefreshToken(refresh_token, refresh_uuid, login, session_id, callback))
        return;

    GetLogger(session_id)->Info("Logout request: login={}", login);
    orm::DbClientPtr db = app().getDbClient();

    try
    {
        orm::Result result = db->execSqlSync("update user_sessions set user_id=-1 where session_id=$1", session_id);
        result = db->execSqlSync("delete from refresh_sessions where user_id=$1 and refresh_uuid=$2", user_id, refresh_uuid);
        if (result.affectedRows() > 0)
            SendOk(callback);
        else
            SendError(k401Unauthorized, "Login or password are incorrect", session_id, callback);
        session->erase("user_id");
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), session_id, callback);
    }

    auth_logger->Info("Logout: login={}, user_id={}", login, user_id);
}

void AuthController::RefreshToken(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;
    std::string refresh_token = req->getCookie("refresh_token");
    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();

    try
    {
        std::string refresh_uuid;
        std::string login;
        if (!ParseRefreshToken(refresh_token, refresh_uuid, login, session_id, callback))
            return;

        GetLogger(session_id)->Info("RefreshToken request: login={}, refresh_uuid={}", login, refresh_uuid);

        orm::Result result = db->execSqlSync("select user_id from users where login=$1", login);
        if (result.size() == 0)
        {
            SendError(k401Unauthorized, "User not found", session_id, callback);
            return;
        }

        auto row = result[0];
        auto user_id = row["user_id"].as<std::string>();

        result = db->execSqlSync("delete from refresh_sessions where refresh_uuid=$1", refresh_uuid);
        if (result.affectedRows() == 0)
        {
            SendError(k401Unauthorized, "Refresh session is incorrect", session_id, callback);
            return;
        }

        refresh_uuid = std::string(boost::uuids::to_string(boost::uuids::random_generator()()));
        std::string access_uuid(boost::uuids::to_string(boost::uuids::random_generator()()));
        trantor::Date access_expires = trantor::Date::now().after(access_token_expires);
        trantor::Date refresh_expires = trantor::Date::now().after(refresh_token_expires);

        result = db->execSqlSync("insert into refresh_sessions (user_id, refresh_uuid, expire_time) values ($1, $2, $3)", 
            user_id, refresh_uuid, refresh_expires.secondsSinceEpoch());
        
        auto last_login = session->get<std::string>("login");
        session->insert("login", login);
        session->erase("user_id");
        session->insert("user_id", user_id);

        if (!session_id.empty())
            UpdateSessionTime(session_id);

        if (user_id != "-1" && session->get<int>("max_file_size") == 0)
        {
            result = db->execSqlSync("select max_files, max_solving_time, max_file_size from user_plans where plan_id=(select plan_id from users where user_id=$1)", user_id);
            if (result.size() == 0)
            {
                GetLogger(session_id)->Error("Database error: user plan not found");
                SendError(k500InternalServerError, "User plan not found", session_id, callback);
                return;
            }
            auto row = result[0];
            session->insert("max_solving_time", row["max_solving_time"].as<int>());
            session->insert("max_file_size", row["max_file_size"].as<int>() * 1024);
            session->insert("max_files", row["max_files"].as<int>());
        }

        session->insert("session_id", session_id);

        if (last_login != login)
            auth_logger->Info("Login: login={}, user_id={}", login, user_id);

        auto session_expires_date = trantor::Date::now().after(session_expires);
        SendOkTokens(callback, login, access_uuid, refresh_uuid, session_id, access_expires, refresh_expires, session_expires_date);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), session_id, callback);
    }
}

void AuthController::SetLanguage(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }

    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");

    if (!json->isMember("language") || !(*json)["language"].isString())
    {
        SendError(k400BadRequest, "Wrong json in the request", session_id, callback);
        return;
    }

    auto language = (*json)["language"].asString();

    GetLogger(session_id)->Debug("Set language request: user_id={}, language={}", user_id, language);
    orm::DbClientPtr db = app().getDbClient();

    try
    {
        orm::Result result = db->execSqlSync("update users set language=$1 where user_id=$2", language, user_id);
        if (result.affectedRows() > 0)
            SendOk(callback);
        else
            SendError(k401Unauthorized, "Login or password are incorrect", session_id, callback);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), session_id, callback);
    }
}

void AuthController::GetLanguage(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");

    GetLogger(session_id)->Debug("Get language request: user_id={}", user_id);
    orm::DbClientPtr db = app().getDbClient();

    try
    {
        ClearDbTurnOff t; //skip the clear db circles for a while

        orm::Result result = db->execSqlSync("select language from users where user_id=$1", user_id);
        if (result.size() == 0)
        {
            SendError(k401Unauthorized, "Login or password are incorrect", session_id, callback);
            return;
        }

        auto row = result[0];
        auto language = row["language"].as<std::string>();

        Json::Value v(Json::objectValue);
        v["language"] = language;
        SendJson(callback, v);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), session_id, callback);
    }
}

#ifdef TEST
void AuthController::SetParams(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    access_token_expires = req->getOptionalParameter<int>("access_token_expires").value();
    refresh_token_expires = req->getOptionalParameter<int>("refresh_token_expires").value();
    SendOk(callback);
}
#endif

void AuthController::UpdateSessionTime(const std::string& session_id)
{
    orm::DbClientPtr db = app().getDbClient();
    trantor::Date session_expires_date = trantor::Date::now().after(session_expires);
    db->execSqlSync("update user_sessions set expire_time=$1 where session_id=$2", session_expires_date.secondsSinceEpoch(), session_id);
}

size_t AuthController::EmailPayload(char *ptr, size_t size, size_t nmemb, void *userp)
{
    AuthController* context = (AuthController*)userp;
    upload_status& upload_context = context->upload_context;
    size_t room = size * nmemb;
 
    if ((size == 0) || (nmemb == 0) || (size * nmemb < 1))
        return 0;
 
    const char* t = context->email_message.c_str();
    const char* data = &t[upload_context.bytes_read];
    if (data)
    {
        size_t len = strlen(data);
        if (room < len)
            len = room;
        memcpy(ptr, data, len);
        upload_context.bytes_read += len;
        return len;
    }
 
    return 0;
}

CURLcode AuthController::SendEmail(const std::string& from, const std::string& to, const std::string& subject, const std::string& message)
{
    CURL* curl = curl_easy_init();
    if (!curl)
        return CURLE_FAILED_INIT;
    
    const Json::Value& v = app().getCustomConfig();
    std::string email_server = v.get("email_server", "").asString();

    CURLcode r = curl_easy_setopt(curl, CURLOPT_USERNAME, from.c_str());
    if (r == CURLE_OK)
        r = curl_easy_setopt(curl, CURLOPT_PASSWORD, EMAIL_PASSWORD);
    if (r == CURLE_OK)
        r = curl_easy_setopt(curl, CURLOPT_URL, email_server.c_str());
    if (r == CURLE_OK)
        r = curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
    if (r == CURLE_OK)
        r = curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from.c_str());
    struct curl_slist* recipients = nullptr;
    recipients = curl_slist_append(recipients, to.c_str());
    if (r == CURLE_OK)
        r = curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    if (r == CURLE_OK)
        r = curl_easy_setopt(curl, CURLOPT_READFUNCTION, AuthController::EmailPayload);
    if (r != CURLE_OK)
    {
        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
        return r;
    }

    upload_context = { 0 };
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%a, %d %b %Y %H:%M:%S %z");
    auto date = oss.str();
    std::string message_id(boost::uuids::to_string(boost::uuids::random_generator()()));

    email_message = "Date: " + date + "\r\n"\
        "To: " + to + "\r\n"\
        "From: " + from + "\r\n"\
        "Message-ID: <" + message_id + ">\r\n"\
        "Subject: " + subject + "\r\n"\
        "\r\n" + 
        message + 
        "\r\n";

    r = curl_easy_setopt(curl, CURLOPT_READDATA, this);
    if (r == CURLE_OK)
        r = curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    if (r == CURLE_OK)
        r = curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5);
    if (r == CURLE_OK)
        r = curl_easy_perform(curl);
    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
    return r;
}

}
