/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#include "binhandler.h"
#include "../rpcnotifier.h"

#include "bin/bin.h"
#include "bin/model/markerlistmodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "project/projectmanager.h"
#include "undohelper.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QUrl>

BinHandler::BinHandler(RpcNotifier *notifier, QObject *parent)
    : QObject(parent)
    , m_notifier(notifier)
{
}

BinHandler::~BinHandler() = default;

auto BinHandler::prefix() const -> QString
{
    return QStringLiteral("bin");
}

auto BinHandler::supportedMethods() const -> QStringList
{
    return QStringList{QStringLiteral("listClips"),     QStringLiteral("listFolders"),     QStringLiteral("getClipInfo"), QStringLiteral("importClip"),
                       QStringLiteral("importClips"),   QStringLiteral("deleteClip"),      QStringLiteral("deleteClips"), QStringLiteral("createFolder"),
                       QStringLiteral("deleteFolder"),  QStringLiteral("renameItem"),      QStringLiteral("moveItem"),    QStringLiteral("getClipMarkers"),
                       QStringLiteral("addClipMarker"), QStringLiteral("deleteClipMarker")};
}

auto BinHandler::handle(const QString &method, const QJsonObject &params) -> QJsonObject
{
    if (method == QLatin1String("listClips")) {
        return handleListClips(params);
    }
    if (method == QLatin1String("listFolders")) {
        return handleListFolders(params);
    }
    if (method == QLatin1String("getClipInfo")) {
        return handleGetClipInfo(params);
    }
    if (method == QLatin1String("importClip")) {
        return handleImportClip(params);
    }
    if (method == QLatin1String("importClips")) {
        return handleImportClips(params);
    }
    if (method == QLatin1String("deleteClip")) {
        return handleDeleteClip(params);
    }
    if (method == QLatin1String("deleteClips")) {
        return handleDeleteClips(params);
    }
    if (method == QLatin1String("createFolder")) {
        return handleCreateFolder(params);
    }
    if (method == QLatin1String("deleteFolder")) {
        return handleDeleteFolder(params);
    }
    if (method == QLatin1String("renameItem")) {
        return handleRenameItem(params);
    }
    if (method == QLatin1String("moveItem")) {
        return handleMoveItem(params);
    }
    if (method == QLatin1String("getClipMarkers")) {
        return handleGetClipMarkers(params);
    }
    if (method == QLatin1String("addClipMarker")) {
        return handleAddClipMarker(params);
    }
    if (method == QLatin1String("deleteClipMarker")) {
        return handleDeleteClipMarker(params);
    }

    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::MethodNotFound},
                                                             {QStringLiteral("message"), QStringLiteral("Unknown method: bin.%1").arg(method)}}}};
}

auto BinHandler::makeProjectNotOpenError() -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ProjectNotOpen},
                                                             {QStringLiteral("message"), QStringLiteral("No project is currently open")}}}};
}

auto BinHandler::makeApplicationClosingError() -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ApplicationClosing},
                                                             {QStringLiteral("message"), QStringLiteral("Application is shutting down")}}}};
}

auto BinHandler::handleListClips(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    auto model = pCore->projectItemModel();
    if (!model) {
        return makeProjectNotOpenError();
    }

    // Optional folder filter
    QString folderId = params.value(QStringLiteral("folderId")).toString();

    QJsonArray clips;
    std::vector<QString> allClipIds = model->getAllClipIds();

    for (const QString &clipId : allClipIds) {
        auto clip = model->getClipByBinID(clipId);
        if (!clip) {
            continue;
        }

        // Filter by folder if specified
        if (!folderId.isEmpty()) {
            auto parent = std::static_pointer_cast<AbstractProjectItem>(clip)->parent();
            if (parent && parent->clipId() != folderId) {
                continue;
            }
        }

        QJsonObject clipInfo;
        clipInfo[QStringLiteral("id")] = clipId;
        clipInfo[QStringLiteral("name")] = clip->name();
        clipInfo[QStringLiteral("type")] = static_cast<int>(clip->clipType());
        clipInfo[QStringLiteral("duration")] = static_cast<int>(clip->frameDuration());
        clipInfo[QStringLiteral("url")] = clip->url();
        clipInfo[QStringLiteral("hasAudio")] = clip->hasAudio();
        clipInfo[QStringLiteral("hasVideo")] = clip->hasVideo();

        clips.append(clipInfo);
    }

    return QJsonObject{{QStringLiteral("result"), clips}};
}

auto BinHandler::handleListFolders(const QJsonObject & /*params*/) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    auto model = pCore->projectItemModel();
    if (!model) {
        return makeProjectNotOpenError();
    }

    QJsonArray folders;
    auto allFolders = model->getFolders();

    for (const auto &folder : allFolders) {
        QJsonObject folderInfo;
        folderInfo[QStringLiteral("id")] = folder->clipId();
        folderInfo[QStringLiteral("name")] = folder->name();

        auto parent = std::static_pointer_cast<AbstractProjectItem>(folder)->parent();
        if (parent) {
            folderInfo[QStringLiteral("parentId")] = parent->clipId();
        } else {
            folderInfo[QStringLiteral("parentId")] = QJsonValue::Null;
        }

        folders.append(folderInfo);
    }

    return QJsonObject{{QStringLiteral("result"), folders}};
}

auto BinHandler::handleGetClipInfo(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    auto model = pCore->projectItemModel();
    if (!model) {
        return makeProjectNotOpenError();
    }

    QString clipId = params.value(QStringLiteral("clipId")).toString();
    if (clipId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'clipId' parameter")}}}};
    }

    auto clip = model->getClipByBinID(clipId);
    if (!clip) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Clip not found: %1").arg(clipId)}}}};
    }

    QJsonObject clipInfo;
    clipInfo[QStringLiteral("id")] = clipId;
    clipInfo[QStringLiteral("name")] = clip->name();
    clipInfo[QStringLiteral("type")] = static_cast<int>(clip->clipType());
    clipInfo[QStringLiteral("duration")] = static_cast<int>(clip->frameDuration());
    clipInfo[QStringLiteral("url")] = clip->url();
    clipInfo[QStringLiteral("hasAudio")] = clip->hasAudio();
    clipInfo[QStringLiteral("hasVideo")] = clip->hasVideo();

    // Additional metadata
    QSize frameSize = clip->frameSize();
    clipInfo[QStringLiteral("width")] = frameSize.width();
    clipInfo[QStringLiteral("height")] = frameSize.height();
    clipInfo[QStringLiteral("fps")] = clip->getOriginalFps();

    // Parent folder
    auto parent = std::static_pointer_cast<AbstractProjectItem>(clip)->parent();
    if (parent) {
        clipInfo[QStringLiteral("folderId")] = parent->clipId();
    }

    return QJsonObject{{QStringLiteral("result"), clipInfo}};
}

auto BinHandler::handleImportClip(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    QString urlStr = params.value(QStringLiteral("url")).toString();
    if (urlStr.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'url' parameter")}}}};
    }

    QString folderId = params.value(QStringLiteral("folderId")).toString();
    if (folderId.isEmpty()) {
        // Use root folder
        folderId = pCore->projectItemModel()->getRootFolder()->clipId();
    }

    QUrl url = QUrl::fromLocalFile(urlStr);
    if (!url.isValid()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidPath},
                                                                 {QStringLiteral("message"), QStringLiteral("Invalid URL: %1").arg(urlStr)}}}};
    }

    // Disable profile check dialog for RPC imports
    pCore->bin()->shouldCheckProfile = false;

    // Import synchronously - this returns the bin clip ID
    QString clipId = pCore->bin()->slotAddClipToProject(url);

    // Let UI settle
    QCoreApplication::processEvents();

    if (clipId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to import clip")}}}};
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("clipId"), clipId}, {QStringLiteral("url"), urlStr}}}};
}

auto BinHandler::handleImportClips(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    QJsonArray urls = params.value(QStringLiteral("urls")).toArray();
    if (urls.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'urls' parameter")}}}};
    }

    QString folderId = params.value(QStringLiteral("folderId")).toString();

    QList<QUrl> urlList;
    for (const QJsonValue &val : urls) {
        QString urlStr = val.toString();
        QUrl url = QUrl::fromLocalFile(urlStr);
        if (url.isValid()) {
            urlList.append(url);
        }
    }

    if (urlList.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("No valid URLs provided")}}}};
    }

    // Disable profile check dialog for RPC imports
    pCore->bin()->shouldCheckProfile = false;

    // Import each clip synchronously and collect IDs
    QJsonArray importedClips;
    QJsonArray failedUrls;

    for (const QUrl &url : urlList) {
        QString clipId = pCore->bin()->slotAddClipToProject(url);
        if (!clipId.isEmpty()) {
            QJsonObject clipInfo;
            clipInfo[QStringLiteral("clipId")] = clipId;
            clipInfo[QStringLiteral("url")] = url.toLocalFile();
            importedClips.append(clipInfo);
        } else {
            failedUrls.append(url.toLocalFile());
        }
    }

    // Let UI settle
    QCoreApplication::processEvents();

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("imported"), importedClips}, {QStringLiteral("failed"), failedUrls}}}};
}

auto BinHandler::handleDeleteClip(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    auto model = pCore->projectItemModel();
    if (!model) {
        return makeProjectNotOpenError();
    }

    QString clipId = params.value(QStringLiteral("clipId")).toString();
    if (clipId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'clipId' parameter")}}}};
    }

    auto clip = model->getClipByBinID(clipId);
    if (!clip) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Clip not found: %1").arg(clipId)}}}};
    }

    Fun undo = []() { return true; };
    Fun redo = []() { return true; };

    bool success = model->requestBinClipDeletion(clip, undo, redo);

    if (!success) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to delete clip")}}}};
    }

    pCore->projectManager()->undoStack()->push(new FunctionalUndoCommand(undo, redo, QStringLiteral("Delete bin clip")));

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("deleted"), true}}}};
}

auto BinHandler::handleDeleteClips(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    auto model = pCore->projectItemModel();
    if (!model) {
        return makeProjectNotOpenError();
    }

    QJsonArray clipIds = params.value(QStringLiteral("clipIds")).toArray();
    if (clipIds.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'clipIds' parameter")}}}};
    }

    Fun undo = []() { return true; };
    Fun redo = []() { return true; };

    QJsonArray deleted;
    QJsonArray failed;

    for (const QJsonValue &val : clipIds) {
        QString clipId = val.toString();
        auto clip = model->getClipByBinID(clipId);
        if (!clip) {
            failed.append(clipId);
            continue;
        }

        bool success = model->requestBinClipDeletion(clip, undo, redo);
        if (success) {
            deleted.append(clipId);
        } else {
            failed.append(clipId);
        }
    }

    if (!deleted.isEmpty()) {
        pCore->projectManager()->undoStack()->push(new FunctionalUndoCommand(undo, redo, QStringLiteral("Delete bin clips")));
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("deleted"), deleted}, {QStringLiteral("failed"), failed}}}};
}

auto BinHandler::handleCreateFolder(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    auto model = pCore->projectItemModel();
    if (!model) {
        return makeProjectNotOpenError();
    }

    QString name = params.value(QStringLiteral("name")).toString();
    if (name.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'name' parameter")}}}};
    }

    QString parentId = params.value(QStringLiteral("parentId")).toString();
    if (parentId.isEmpty()) {
        parentId = model->getRootFolder()->clipId();
    }

    Fun undo = []() { return true; };
    Fun redo = []() { return true; };

    QString folderId;
    bool success = model->requestAddFolder(folderId, name, parentId, undo, redo);

    if (!success) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to create folder")}}}};
    }

    pCore->projectManager()->undoStack()->push(new FunctionalUndoCommand(undo, redo, QStringLiteral("Create folder")));

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("folderId"), folderId}}}};
}

auto BinHandler::handleDeleteFolder(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    auto model = pCore->projectItemModel();
    if (!model) {
        return makeProjectNotOpenError();
    }

    QString folderId = params.value(QStringLiteral("folderId")).toString();
    if (folderId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'folderId' parameter")}}}};
    }

    auto folder = model->getFolderByBinId(folderId);
    if (!folder) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Folder not found: %1").arg(folderId)}}}};
    }

    Fun undo = []() { return true; };
    Fun redo = []() { return true; };

    bool success = model->requestBinClipDeletion(folder, undo, redo);

    if (!success) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to delete folder")}}}};
    }

    pCore->projectManager()->undoStack()->push(new FunctionalUndoCommand(undo, redo, QStringLiteral("Delete folder")));

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("deleted"), true}}}};
}

auto BinHandler::handleRenameItem(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    auto model = pCore->projectItemModel();
    if (!model) {
        return makeProjectNotOpenError();
    }

    QString itemId = params.value(QStringLiteral("itemId")).toString();
    if (itemId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'itemId' parameter")}}}};
    }

    QString name = params.value(QStringLiteral("name")).toString();
    if (name.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'name' parameter")}}}};
    }

    auto item = model->getItemByBinId(itemId);
    if (!item) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Item not found: %1").arg(itemId)}}}};
    }

    bool success = item->rename(name, 0);

    if (!success) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to rename item")}}}};
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("renamed"), true}}}};
}

auto BinHandler::handleMoveItem(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    QString itemId = params.value(QStringLiteral("itemId")).toString();
    if (itemId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'itemId' parameter")}}}};
    }

    QString folderId = params.value(QStringLiteral("folderId")).toString();
    if (folderId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'folderId' parameter")}}}};
    }

    // Get current parent
    auto model = pCore->projectItemModel();
    auto item = model->getItemByBinId(itemId);
    if (!item) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Item not found: %1").arg(itemId)}}}};
    }

    auto currentParent = item->parent();
    QString oldParentId = currentParent ? currentParent->clipId() : QString();

    // Move the item
    QMap<QString, std::pair<QString, QString>> ids;
    ids.insert(itemId, {folderId, oldParentId});
    pCore->bin()->doMoveClips(ids, true, false);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("moved"), true}}}};
}

auto BinHandler::handleGetClipMarkers(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    auto model = pCore->projectItemModel();
    if (!model) {
        return makeProjectNotOpenError();
    }

    QString clipId = params.value(QStringLiteral("clipId")).toString();
    if (clipId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'clipId' parameter")}}}};
    }

    auto clip = model->getClipByBinID(clipId);
    if (!clip) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Clip not found: %1").arg(clipId)}}}};
    }

    auto markerModel = clip->markerModel();
    if (!markerModel) {
        return QJsonObject{{QStringLiteral("result"), QJsonArray()}};
    }

    QJsonArray markers;
    QList<CommentedTime> allMarkers = markerModel->getAllMarkers();

    for (const CommentedTime &marker : allMarkers) {
        QJsonObject markerInfo;
        markerInfo[QStringLiteral("position")] = marker.time().frames(pCore->getCurrentFps());
        markerInfo[QStringLiteral("comment")] = marker.comment();
        markerInfo[QStringLiteral("type")] = marker.markerType();
        markers.append(markerInfo);
    }

    return QJsonObject{{QStringLiteral("result"), markers}};
}

auto BinHandler::handleAddClipMarker(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    auto model = pCore->projectItemModel();
    if (!model) {
        return makeProjectNotOpenError();
    }

    QString clipId = params.value(QStringLiteral("clipId")).toString();
    if (clipId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'clipId' parameter")}}}};
    }

    int position = params.value(QStringLiteral("position")).toInt(-1);
    if (position < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'position' parameter")}}}};
    }

    QString comment = params.value(QStringLiteral("comment")).toString();
    int type = params.value(QStringLiteral("type")).toInt(-1);

    QMap<int, QString> markersData;
    markersData.insert(position, comment);

    pCore->bin()->addClipMarker(clipId, markersData, type);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("added"), true}}}};
}

auto BinHandler::handleDeleteClipMarker(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    auto model = pCore->projectItemModel();
    if (!model) {
        return makeProjectNotOpenError();
    }

    QString clipId = params.value(QStringLiteral("clipId")).toString();
    if (clipId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'clipId' parameter")}}}};
    }

    int position = params.value(QStringLiteral("position")).toInt(-1);
    if (position < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'position' parameter")}}}};
    }

    auto clip = model->getClipByBinID(clipId);
    if (!clip) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Clip not found: %1").arg(clipId)}}}};
    }

    auto markerModel = clip->markerModel();
    if (!markerModel) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("No marker model for clip")}}}};
    }

    GenTime pos(position, pCore->getCurrentFps());
    bool success = markerModel->removeMarker(pos);

    if (!success) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to delete marker")}}}};
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("deleted"), true}}}};
}
