/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#include "projecthandler.h"
#include "../rpcnotifier.h"

#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "project/projectmanager.h"

#include <QUrl>

ProjectHandler::ProjectHandler(RpcNotifier *notifier, QObject *parent)
    : QObject(parent)
    , m_notifier(notifier)
{
}

ProjectHandler::~ProjectHandler() = default;

QString ProjectHandler::prefix() const
{
    return QStringLiteral("project");
}

QStringList ProjectHandler::supportedMethods() const
{
    return QStringList{QStringLiteral("getInfo"), QStringLiteral("open"), QStringLiteral("save"), QStringLiteral("close"),
                       QStringLiteral("new"),     QStringLiteral("undo"), QStringLiteral("redo")};
}

QJsonObject ProjectHandler::handle(const QString &method, const QJsonObject &params)
{
    if (method == QLatin1String("getInfo")) {
        return handleGetInfo(params);
    }
    if (method == QLatin1String("open")) {
        return handleOpen(params);
    }
    if (method == QLatin1String("save")) {
        return handleSave(params);
    }
    if (method == QLatin1String("close")) {
        return handleClose(params);
    }
    if (method == QLatin1String("new")) {
        return handleNew(params);
    }
    if (method == QLatin1String("undo")) {
        return handleUndo(params);
    }
    if (method == QLatin1String("redo")) {
        return handleRedo(params);
    }

    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::MethodNotFound},
                                                             {QStringLiteral("message"), QStringLiteral("Unknown method: project.%1").arg(method)}}}};
}

QJsonObject ProjectHandler::makeProjectNotOpenError()
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ProjectNotOpen},
                                                             {QStringLiteral("message"), QStringLiteral("No project is currently open")}}}};
}

QJsonObject ProjectHandler::handleGetInfo(const QJsonObject & /*params*/)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    QJsonObject info;
    info[QStringLiteral("path")] = doc->url().toLocalFile();
    info[QStringLiteral("name")] = doc->url().fileName();
    info[QStringLiteral("modified")] = doc->isModified();
    info[QStringLiteral("fps")] = doc->fps();
    info[QStringLiteral("width")] = doc->width();
    info[QStringLiteral("height")] = doc->height();

    return QJsonObject{{QStringLiteral("result"), info}};
}

QJsonObject ProjectHandler::handleOpen(const QJsonObject &params)
{
    QString path = params.value(QStringLiteral("path")).toString();
    if (path.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'path' parameter")}}}};
    }

    QUrl url = QUrl::fromLocalFile(path);
    if (!url.isValid()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidPath},
                                                                 {QStringLiteral("message"), QStringLiteral("Invalid path: %1").arg(path)}}}};
    }

    // Open the project file
    pCore->projectManager()->doOpenFile(url, nullptr);

    // Notify if subscribed
    m_notifier->notifyProjectOpened(path);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("opened"), true}, {QStringLiteral("path"), path}}}};
}

QJsonObject ProjectHandler::handleSave(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    QString path = params.value(QStringLiteral("path")).toString();

    bool success;
    if (path.isEmpty()) {
        // Save to current location
        success = pCore->projectManager()->saveFile();
    } else {
        // Save to new location
        success = pCore->projectManager()->saveFileAs(path);
    }

    if (!success) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to save project")}}}};
    }

    QString savedPath = doc->url().toLocalFile();
    m_notifier->notifyProjectSaved(savedPath);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("saved"), true}, {QStringLiteral("path"), savedPath}}}};
}

QJsonObject ProjectHandler::handleClose(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    bool saveChanges = params.value(QStringLiteral("saveChanges")).toBool(true);

    bool success = pCore->projectManager()->closeCurrentDocument(saveChanges);

    if (!success) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to close project")}}}};
    }

    m_notifier->notifyProjectClosed();

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("closed"), true}}}};
}

QJsonObject ProjectHandler::handleNew(const QJsonObject &params)
{
    QString profile = params.value(QStringLiteral("profile")).toString();

    // Create new project (don't show project settings dialog)
    if (profile.isEmpty()) {
        pCore->projectManager()->newFile(false);
    } else {
        pCore->projectManager()->newFile(profile, false);
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to create new project")}}}};
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("created"), true},
                                                              {QStringLiteral("fps"), doc->fps()},
                                                              {QStringLiteral("width"), doc->width()},
                                                              {QStringLiteral("height"), doc->height()}}}};
}

QJsonObject ProjectHandler::handleUndo(const QJsonObject & /*params*/)
{
    auto undoStack = pCore->projectManager()->undoStack();
    if (!undoStack || !undoStack->canUndo()) {
        return QJsonObject{{QStringLiteral("error"),
                            QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed}, {QStringLiteral("message"), QStringLiteral("Nothing to undo")}}}};
    }

    QString actionText = undoStack->undoText();
    undoStack->undo();

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("undone"), true}, {QStringLiteral("action"), actionText}}}};
}

QJsonObject ProjectHandler::handleRedo(const QJsonObject & /*params*/)
{
    auto undoStack = pCore->projectManager()->undoStack();
    if (!undoStack || !undoStack->canRedo()) {
        return QJsonObject{{QStringLiteral("error"),
                            QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed}, {QStringLiteral("message"), QStringLiteral("Nothing to redo")}}}};
    }

    QString actionText = undoStack->redoText();
    undoStack->redo();

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("redone"), true}, {QStringLiteral("action"), actionText}}}};
}
