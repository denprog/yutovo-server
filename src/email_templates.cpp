/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "email_templates.h"

namespace yutovo_server
{

namespace
{

struct VerificationStrings
{
    const char* subject;
    const char* greeting;
    const char* body1;
    const char* body2;
    const char* ignore;
    const char* sign;
};

enum VerificationKind
{
    kRegisterVerification,
    kPasswordVerification,
    kEmailVerification
};

const VerificationStrings& GetVerificationStrings(VerificationKind kind, const std::string& language)
{
    static const VerificationStrings strings[7][kEmailVerification + 1] =
    {
        //en
        {
            {"Register code", "Confirm your registration",
                "You have started the registration process on <a href=\"https://yutovo.com\">yutovo.com</a>.",
                "To complete your registration, enter the following code in the verification field:",
                "If you did not request this registration, please ignore this email.",
                "Best regards,<br>The yutovo.com team"},
            {"Changing password", "Confirm your password",
                "You requested a change of password on <a href=\"https://yutovo.com\">yutovo.com</a>.",
                "To complete this process, enter the following code in the verification field:",
                "If you did not request this action, please ignore this email.",
                "Best regards,<br>The yutovo.com team"},
            {"Changing e-mail", "Confirm your e-mail",
                "You requested a change of email on <a href=\"https://yutovo.com\">yutovo.com</a>.",
                "To complete this process, enter the following code in the verification field:",
                "If you did not request this action, please ignore this email.",
                "Best regards,<br>The yutovo.com team"}
        },
        //ru
        {
            {"Регистрация на Ютово", "Подтвердите свою регистрацию",
                "Вы начали регистрацию на сайте <a href=\"https://yutovo.com\">yutovo.com</a>.",
                "Для завершения регистрации введите этот код в поле проверки:",
                "Если вы не начинали регистрацию, проигнорируйте это письмо.",
                "С уважением,<br>Команда yutovo.com"},
            {"Смена пароля", "Подтвердите ваш пароль",
                "Вы запросили смену пароля на <a href=\"https://yutovo.com\">yutovo.com</a>.",
                "Чтобы завершить этот процесс, введите следующий код в поле проверки:",
                "Если вы не запрашивали это действие, просто проигнорируйте это письмо.",
                "С уважением,<br>Команда yutovo.com"},
            {"Смена почты", "Подтвердите вашу почту",
                "Вы запросили смену почты на <a href=\"https://yutovo.com\">yutovo.com</a>.",
                "Для завершения введите этот код в поле проверки:",
                "Если вы не запрашивали этого, проигнорируйте это письмо.",
                "С уважением,<br>Команда yutovo.com"}
        },
        //de
        {
            {"Registrierung bei Yutovo", "Bestätigen Sie Ihre Registrierung",
                "Sie haben die Registrierung auf <a href=\"https://yutovo.com\">yutovo.com</a> gestartet.",
                "Geben Sie diesen Code zur Bestätigung ein:",
                "Falls Sie die Registrierung nicht gestartet haben, ignorieren Sie diese E-Mail.",
                "Mit freundlichen Grüßen,<br>das Team von yutovo.com"},
            {"Passwort-Änderung", "Bestätigen Sie Ihr Passwort",
                "Sie haben eine Passwort-Änderung auf <a href=\"https://yutovo.com\">yutovo.com</a> angefordert.",
                "Geben Sie diesen Code zur Bestätigung ein:",
                "Falls Sie diese Aktion nicht angefordert haben, ignorieren Sie diese E-Mail einfach.",
                "Mit freundlichen Grüßen,<br>das Team von yutovo.com"},
            {"E-Mail-Änderung", "Bestätigen Sie Ihre E-Mail-Adresse",
                "Sie haben eine Änderung der E-Mail-Adresse auf <a href=\"https://yutovo.com\">yutovo.com</a> angefordert.",
                "Geben Sie diesen Code zur Bestätigung ein:",
                "Falls Sie dies nicht angefordert haben, ignorieren Sie diese E-Mail.",
                "Mit freundlichen Grüßen,<br>das Team von yutovo.com"}
        },
        //fr
        {
            {"Inscription sur Yutovo", "Confirmez votre inscription",
                "Vous avez commencé votre inscription sur le site <a href=\"https://yutovo.com\">yutovo.com</a>.",
                "Pour terminer l'inscription, saisissez ce code dans le champ de vérification :",
                "Si vous n'avez pas commencé d'inscription, ignorez cet e-mail.",
                "Cordialement,<br>L'équipe yutovo.com"},
            {"Changement de mot de passe", "Confirmez votre mot de passe",
                "Vous avez demandé un changement de mot de passe sur <a href=\"https://yutovo.com\">yutovo.com</a>.",
                "Pour terminer ce processus, saisissez le code suivant dans le champ de vérification :",
                "Si vous n'avez pas demandé cette action, ignorez simplement cet e-mail.",
                "Cordialement,<br>L'équipe yutovo.com"},
            {"Changement d'e-mail", "Confirmez votre e-mail",
                "Vous avez demandé un changement d'e-mail sur <a href=\"https://yutovo.com\">yutovo.com</a>.",
                "Pour terminer, saisissez ce code dans le champ de vérification :",
                "Si vous n'avez pas fait cette demande, ignorez cet e-mail.",
                "Cordialement,<br>L'équipe yutovo.com"}
        },
        //es
        {
            {"Registro en Yutovo", "Confirme su registro",
                "Ha iniciado el registro en <a href=\"https://yutovo.com\">yutovo.com</a>.",
                "Para completar el registro, introduzca este código en el campo de verificación:",
                "Si no inició el registro, ignore este correo electrónico.",
                "Con respecto,<br>El equipo de yutovo.com"},
            {"Cambio de contraseña", "Confirme su contraseña",
                "Ha solicitado un cambio de contraseña en <a href=\"https://yutovo.com\">yutovo.com</a>.",
                "Para completar este proceso, introduzca el siguiente código en el campo de verificación:",
                "Si no solicitó esta acción, ignore este correo electrónico.",
                "Con respecto,<br>El equipo de yutovo.com"},
            {"Cambio de correo electrónico", "Confirme su correo electrónico",
                "Ha solicitado un cambio de correo electrónico en <a href=\"https://yutovo.com\">yutovo.com</a>.",
                "Para completar el proceso, introduzca este código en el campo de verificación:",
                "Si no solicitó esto, ignore este correo electrónico.",
                "Con respecto,<br>El equipo de yutovo.com"}
        },
        //it
        {
            {"Registrazione su Yutovo", "Confermate la vostra registrazione",
                "Avete iniziato la registrazione sul sito <a href=\"https://yutovo.com\">yutovo.com</a>.",
                "Per completare la registrazione, inserite questo codice nel campo di verifica:",
                "Se non avete iniziato la registrazione, ignorate questa e-mail.",
                "Cordiali saluti,<br>Il team di yutovo.com"},
            {"Cambio password", "Confermate la vostra password",
                "Avete richiesto un cambio password su <a href=\"https://yutovo.com\">yutovo.com</a>.",
                "Per completare questo processo, inserite il seguente codice nel campo di verifica:",
                "Se non avete richiesto questa azione, ignorate semplicemente questa e-mail.",
                "Cordiali saluti,<br>Il team di yutovo.com"},
            {"Cambio dell'e-mail", "Confermate la vostra e-mail",
                "Avete richiesto un cambio di e-mail su <a href=\"https://yutovo.com\">yutovo.com</a>.",
                "Per completare, inserite questo codice nel campo di verifica:",
                "Se non avete fatto questa richiesta, ignorate questa e-mail.",
                "Cordiali saluti,<br>Il team di yutovo.com"}
        },
        //pt_BR
        {
            {"Cadastro no Yutovo", "Confirme seu cadastro",
                "Você iniciou o cadastro em <a href=\"https://yutovo.com\">yutovo.com</a>.",
                "Para concluir o cadastro, insira este código no campo de verificação:",
                "Se você não iniciou o cadastro, ignore este e-mail.",
                "Atenciosamente,<br>Equipe yutovo.com"},
            {"Alteração de senha", "Confirme sua senha",
                "Você solicitou a alteração de senha em <a href=\"https://yutovo.com\">yutovo.com</a>.",
                "Para concluir este processo, insira o seguinte código no campo de verificação:",
                "Se você não solicitou esta ação, ignore este e-mail.",
                "Atenciosamente,<br>Equipe yutovo.com"},
            {"Alteração de e-mail", "Confirme seu e-mail",
                "Você solicitou a alteração de e-mail em <a href=\"https://yutovo.com\">yutovo.com</a>.",
                "Para concluir este processo, insira este código no campo de verificação:",
                "Se você não solicitou isso, ignore este e-mail.",
                "Atenciosamente,<br>Equipe yutovo.com"}
        }
    };

    size_t lang = 0; //en
    if (language == "ru")
        lang = 1;
    else if (language == "de")
        lang = 2;
    else if (language == "fr")
        lang = 3;
    else if (language == "es")
        lang = 4;
    else if (language == "it")
        lang = 5;
    else if (language == "pt_BR")
        lang = 6;

    return strings[lang][kind];
}

}

std::string EmailTemplates::BuildVerificationHtml(const std::string& subject, const std::string& greeting, const std::string& body1,
    const std::string& body2, const std::string& ignore, const std::string& sign)
{
    std::string html = R"HTML(<!DOCTYPE html>
        <html lang="en">
        <head>
            <meta charset="utf-8">
            <title>{{subject}}</title>
        </head>
        <body style="font-family: Arial, sans-serif; background-color: #f4f4f4; margin: 0; padding: 0;">
            <table width="100%" bgcolor="#f4f4f4" cellpadding="0" cellspacing="0" border="0">
                <tr>
                    <td align="center">
                        <table width="600" cellpadding="0" cellspacing="0" border="0" style="margin: 40px 0;">
                            <tr>
                                <td bgcolor="#ffffff" style="padding: 40px 30px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1);">
                                    <h1 style="font-size: 24px; color: #333333; margin: 0 0 20px;">{{greeting}}</h1>

                                    <p style="font-size: 16px; line-height: 1.6; color: #555555;">{{body1}}</p>
                                    <p style="font-size: 16px; line-height: 1.6; color: #555555;">{{body2}}</p>

                                    <div style="text-align: center; margin: 40px 0;">
                                        <div style="display: inline-block; background-color: #f0f0f0; padding: 20px 40px; border-radius: 8px; "
                                            "font-size: 32px; font-weight: bold; letter-spacing: 8px; color: #333333;">
                                            EMAIL_CODE
                                        </div>
                                    </div>

                                    <p style="font-size: 16px; line-height: 1.6; color: #555555;">{{ignore}}</p>
                                    <p style="font-size: 16px; line-height: 1.6; color: #555555;">{{sign}}</p>
                                </td>
                            </tr>
                        </table>
                    </td>
                </tr>
            </table>
        </body>
        </html>)HTML";

    auto replace_all = [&html](const std::string& from, const std::string& to)
    {
        size_t p = html.find(from);
        while (p != std::string::npos)
        {
            html.replace(p, from.size(), to);
            p = html.find(from, p + to.size());
        }
    };

    replace_all("{{subject}}", subject);
    replace_all("{{greeting}}", greeting);
    replace_all("{{body1}}", body1);
    replace_all("{{body2}}", body2);
    replace_all("{{ignore}}", ignore);
    replace_all("{{sign}}", sign);
    return html;
}

EmailTemplate EmailTemplates::GetVerificationEmailTemplate(const std::string& action, const std::string& language)
{
    VerificationKind kind = kRegisterVerification;
    if (action == "recover_password" || action == "change_password")
        kind = kPasswordVerification;
    else if (action == "change_email")
        kind = kEmailVerification;

    const VerificationStrings& s = GetVerificationStrings(kind, language);
    EmailTemplate result;
    result.subject = s.subject;
    result.body_html = BuildVerificationHtml(s.subject, s.greeting, s.body1, s.body2, s.ignore, s.sign);
    return result;
}

}
