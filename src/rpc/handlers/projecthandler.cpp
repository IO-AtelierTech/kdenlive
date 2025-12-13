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

#include <QCoreApplication>
#include <QFile>
#include <QTimer>
#include <QUrl>

ProjectHandler::ProjectHandler(RpcNotifier *notifier, QObject *parent)
    : QObject(parent)
    , m_notifier(notifier)
{
}

ProjectHandler::~ProjectHandler() = default;

auto ProjectHandler::prefix() const -> QString
{
    return QStringLiteral("project");
}

auto ProjectHandler::supportedMethods() const -> QStringList
{
    return QStringList{QStringLiteral("getInfo"), QStringLiteral("open"), QStringLiteral("save"), QStringLiteral("close"),
                       QStringLiteral("new"),     QStringLiteral("undo"), QStringLiteral("redo")};
}

auto ProjectHandler::handle(const QString &method, const QJsonObject &params) -> QJsonObject
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

auto ProjectHandler::makeProjectNotOpenError() -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ProjectNotOpen},
                                                             {QStringLiteral("message"), QStringLiteral("No project is currently open")}}}};
}

auto ProjectHandler::makeApplicationClosingError() -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ApplicationClosing},
                                                             {QStringLiteral("message"), QStringLiteral("Application is shutting down")}}}};
}

auto ProjectHandler::handleGetInfo(const QJsonObject & /*params*/) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    QJsonObject info;
    info[QStringLiteral("path")] = doc->url().toLocalFile();
    info[QStringLiteral("name")] = doc->url().fileName();
    info[QStringLiteral("modified")] = doc->isModified();
    info[QStringLiteral("fps")] = pCore->getCurrentFps();
    info[QStringLiteral("width")] = doc->width();
    info[QStringLiteral("height")] = doc->height();

    return QJsonObject{{QStringLiteral("result"), info}};
}

auto ProjectHandler::handleOpen(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

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

    // Check if the file exists before attempting to open
    if (!QFile::exists(path)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidPath},
                                                                 {QStringLiteral("message"), QStringLiteral("File not found: %1").arg(path)}}}};
    }

    // Defer the open operation to run after response is sent
    // This prevents QML setup from disrupting the WebSocket response
    QTimer::singleShot(0, [url]() {
        pCore->projectManager()->doOpenFile(url, nullptr);
        QCoreApplication::processEvents();
    });

    // Return immediately - client should poll project.getInfo or wait for notification
    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("opened"), true}, {QStringLiteral("path"), path}}}};
}

auto ProjectHandler::handleSave(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    QString path = params.value(QStringLiteral("path")).toString();

    bool success = false;
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

auto ProjectHandler::handleClose(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    // For RPC, default to NOT prompting for save (automation should explicitly save first)
    // This avoids blocking modal dialogs that would cause timeouts
    bool saveChanges = params.value(QStringLiteral("saveChanges")).toBool(false);

    // If saveChanges requested and project is modified, save it first (no dialog)
    if (saveChanges && doc->isModified()) {
        if (!pCore->projectManager()->saveFile()) {
            return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                     {QStringLiteral("message"), QStringLiteral("Failed to save project before closing")}}}};
        }
    }

    // Defer the close operation to run after response is sent
    // This prevents QML cleanup from disrupting the WebSocket response
    QTimer::singleShot(0, []() {
        pCore->projectManager()->closeCurrentDocument(false);
        QCoreApplication::processEvents();
    });

    m_notifier->notifyProjectClosed();

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("closed"), true}}}};
}

auto ProjectHandler::handleNew(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    QString profile = params.value(QStringLiteral("profile")).toString();

    // Defer the new project operation to run after response is sent
    // This prevents QML setup from disrupting the WebSocket response
    QTimer::singleShot(0, [profile]() {
        if (profile.isEmpty()) {
            pCore->projectManager()->newFile(false);
        } else {
            pCore->projectManager()->newFile(profile, false);
        }
        QCoreApplication::processEvents();
    });

    // Return immediately - client should poll project.getInfo or wait for notification
    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("created"), true}}}};
}

auto ProjectHandler::handleUndo(const QJsonObject & /*params*/) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    auto undoStack = pCore->projectManager()->undoStack();
    if (!undoStack || !undoStack->canUndo()) {
        return QJsonObject{{QStringLiteral("error"),
                            QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed}, {QStringLiteral("message"), QStringLiteral("Nothing to undo")}}}};
    }

    QString actionText = undoStack->undoText();
    undoStack->undo();

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("undone"), true}, {QStringLiteral("action"), actionText}}}};
}

auto ProjectHandler::handleRedo(const QJsonObject & /*params*/) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    auto undoStack = pCore->projectManager()->undoStack();
    if (!undoStack || !undoStack->canRedo()) {
        return QJsonObject{{QStringLiteral("error"),
                            QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed}, {QStringLiteral("message"), QStringLiteral("Nothing to redo")}}}};
    }

    QString actionText = undoStack->redoText();
    undoStack->redo();

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("redone"), true}, {QStringLiteral("action"), actionText}}}};
}
