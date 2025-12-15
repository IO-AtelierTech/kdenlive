/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#include "binhandler.h"
#include "../rpcnotifier.h"

#include "bin/bin.h"
#include "bin/clipcreator.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "definitions.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "jobs/taskmanager.h"
#include "project/projectmanager.h"
#include "undohelper.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QTimer>
#include <QUrl>

#include <cstdio> // for fprintf/fflush debug logging

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
    // Immediate stderr logging to debug freeze
    fprintf(stderr, "BinHandler::handle called with method: %s\n", method.toUtf8().constData());
    fflush(stderr);

    if (method == QLatin1String("listClips")) {
        fprintf(stderr, "BinHandler: calling handleListClips\n");
        fflush(stderr);
        auto result = handleListClips(params);
        fprintf(stderr, "BinHandler: handleListClips returned\n");
        fflush(stderr);
        return result;
    }
    if (method == QLatin1String("listFolders")) {
        return handleListFolders(params);
    }
    if (method == QLatin1String("getClipInfo")) {
        return handleGetClipInfo(params);
    }
    if (method == QLatin1String("importClip")) {
        fprintf(stderr, "BinHandler: calling handleImportClip\n");
        fflush(stderr);
        auto result = handleImportClip(params);
        fprintf(stderr, "BinHandler: handleImportClip returned\n");
        fflush(stderr);
        return result;
    }
    if (method == QLatin1String("importClips")) {
        fprintf(stderr, "BinHandler: calling handleImportClips\n");
        fflush(stderr);
        auto result = handleImportClips(params);
        fprintf(stderr, "BinHandler: handleImportClips returned\n");
        fflush(stderr);
        return result;
    }
    if (method == QLatin1String("deleteClip")) {
        fprintf(stderr, "BinHandler: calling handleDeleteClip\n");
        fflush(stderr);
        auto result = handleDeleteClip(params);
        fprintf(stderr, "BinHandler: handleDeleteClip returned\n");
        fflush(stderr);
        return result;
    }
    if (method == QLatin1String("deleteClips")) {
        fprintf(stderr, "BinHandler: calling handleDeleteClips\n");
        fflush(stderr);
        auto result = handleDeleteClips(params);
        fprintf(stderr, "BinHandler: handleDeleteClips returned\n");
        fflush(stderr);
        return result;
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
    fprintf(stderr, "BinHandler::handleListClips START\n");
    fflush(stderr);

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

    fprintf(stderr, "BinHandler::handleListClips: got model\n");
    fflush(stderr);

    // Optional folder filter
    QString folderId = params.value(QStringLiteral("folderId")).toString();

    QJsonArray clips;
    fprintf(stderr, "BinHandler::handleListClips: calling getAllClipIds\n");
    fflush(stderr);
    std::vector<QString> allClipIds = model->getAllClipIds();
    fprintf(stderr, "BinHandler::handleListClips: got %lld clipIds\n", static_cast<long long>(allClipIds.size()));
    fflush(stderr);

    for (const QString &clipId : allClipIds) {
        fprintf(stderr, "BinHandler::handleListClips: processing clipId=%s\n", clipId.toUtf8().constData());
        fflush(stderr);

        fprintf(stderr, "BinHandler::handleListClips: calling getClipByBinID\n");
        fflush(stderr);
        auto clip = model->getClipByBinID(clipId);
        fprintf(stderr, "BinHandler::handleListClips: getClipByBinID returned\n");
        fflush(stderr);

        if (!clip) {
            fprintf(stderr, "BinHandler::handleListClips: clip is null, skipping\n");
            fflush(stderr);
            continue;
        }

        // Filter by folder if specified
        if (!folderId.isEmpty()) {
            auto parent = std::static_pointer_cast<AbstractProjectItem>(clip)->parent();
            if (parent && parent->clipId() != folderId) {
                continue;
            }
        }

        fprintf(stderr, "BinHandler::handleListClips: building clipInfo\n");
        fflush(stderr);

        QJsonObject clipInfo;
        clipInfo[QStringLiteral("id")] = clipId;
        clipInfo[QStringLiteral("name")] = clip->name();
        clipInfo[QStringLiteral("type")] = static_cast<int>(clip->clipType());
        clipInfo[QStringLiteral("url")] = clip->url();

        // Check if clip is still loading - if so, skip producer-dependent properties
        // to avoid deadlock with ClipLoadTask's BlockingQueuedConnection
        FileStatus::ClipStatus status = clip->clipStatus();
        fprintf(stderr, "BinHandler::handleListClips: clipStatus=%d (Waiting=%d)\n", static_cast<int>(status), static_cast<int>(FileStatus::StatusWaiting));
        fflush(stderr);

        if (status == FileStatus::StatusWaiting) {
            // Clip is still loading, use placeholder values
            clipInfo[QStringLiteral("duration")] = 0;
            clipInfo[QStringLiteral("hasAudio")] = false;
            clipInfo[QStringLiteral("hasVideo")] = false;
            clipInfo[QStringLiteral("loading")] = true;
        } else {
            // Clip is ready, get actual values
            fprintf(stderr, "BinHandler::handleListClips: getting duration\n");
            fflush(stderr);
            clipInfo[QStringLiteral("duration")] = static_cast<int>(clip->frameDuration());

            fprintf(stderr, "BinHandler::handleListClips: getting hasAudio/hasVideo\n");
            fflush(stderr);
            clipInfo[QStringLiteral("hasAudio")] = clip->hasAudio();
            clipInfo[QStringLiteral("hasVideo")] = clip->hasVideo();
        }

        fprintf(stderr, "BinHandler::handleListClips: appending clip info\n");
        fflush(stderr);
        clips.append(clipInfo);
    }

    fprintf(stderr, "BinHandler::handleListClips: returning %lld clips\n", static_cast<long long>(clips.size()));
    fflush(stderr);
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
    fprintf(stderr, "BinHandler::handleGetClipInfo START\n");
    fflush(stderr);

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

    fprintf(stderr, "BinHandler::handleGetClipInfo: getting clip %s\n", clipId.toUtf8().constData());
    fflush(stderr);

    // Process events first to let any pending ClipLoadTask complete
    qApp->processEvents(QEventLoop::AllEvents, 50);

    auto clip = model->getClipByBinID(clipId);
    if (!clip) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Clip not found: %1").arg(clipId)}}}};
    }

    fprintf(stderr, "BinHandler::handleGetClipInfo: got clip, checking status\n");
    fflush(stderr);

    // Check status FIRST before accessing any other properties
    FileStatus::ClipStatus status = clip->clipStatus();
    fprintf(stderr, "BinHandler::handleGetClipInfo: status=%d\n", static_cast<int>(status));
    fflush(stderr);

    QJsonObject clipInfo;
    clipInfo[QStringLiteral("id")] = clipId;

    // Check if clip is still loading - if so, skip ALL producer-dependent properties
    // to avoid deadlock with ClipLoadTask's BlockingQueuedConnection
    if (status == FileStatus::StatusWaiting) {
        fprintf(stderr, "BinHandler::handleGetClipInfo: clip still loading, returning placeholder\n");
        fflush(stderr);
        // Clip is still loading, use placeholder values for everything
        clipInfo[QStringLiteral("name")] = QStringLiteral("");
        clipInfo[QStringLiteral("type")] = 0;
        clipInfo[QStringLiteral("url")] = QStringLiteral("");
        clipInfo[QStringLiteral("duration")] = 0;
        clipInfo[QStringLiteral("hasAudio")] = false;
        clipInfo[QStringLiteral("hasVideo")] = false;
        clipInfo[QStringLiteral("width")] = 0;
        clipInfo[QStringLiteral("height")] = 0;
        clipInfo[QStringLiteral("fps")] = 0.0;
        clipInfo[QStringLiteral("folderId")] = QStringLiteral("-1");
        clipInfo[QStringLiteral("loading")] = true;
    } else {
        fprintf(stderr, "BinHandler::handleGetClipInfo: clip ready, getting properties\n");
        fflush(stderr);
        // Clip is ready, get actual values
        clipInfo[QStringLiteral("name")] = clip->name();
        clipInfo[QStringLiteral("type")] = static_cast<int>(clip->clipType());
        clipInfo[QStringLiteral("url")] = clip->url();
        clipInfo[QStringLiteral("duration")] = static_cast<int>(clip->frameDuration());
        clipInfo[QStringLiteral("hasAudio")] = clip->hasAudio();
        clipInfo[QStringLiteral("hasVideo")] = clip->hasVideo();

        // Additional metadata
        QSize frameSize = clip->frameSize();
        clipInfo[QStringLiteral("width")] = frameSize.width();
        clipInfo[QStringLiteral("height")] = frameSize.height();
        clipInfo[QStringLiteral("fps")] = clip->getOriginalFps();
        clipInfo[QStringLiteral("loading")] = false;

        // Parent folder
        auto parent = std::static_pointer_cast<AbstractProjectItem>(clip)->parent();
        if (parent) {
            clipInfo[QStringLiteral("folderId")] = parent->clipId();
        }
    }

    fprintf(stderr, "BinHandler::handleGetClipInfo: returning result\n");
    fflush(stderr);
    return QJsonObject{{QStringLiteral("result"), clipInfo}};
}

auto BinHandler::handleImportClip(const QJsonObject &params) -> QJsonObject
{
    fprintf(stderr, "BinHandler::handleImportClip START\n");
    fflush(stderr);

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

    fprintf(stderr, "BinHandler::handleImportClip: url=%s, folderId=%s\n", urlStr.toUtf8().constData(), folderId.toUtf8().constData());
    fflush(stderr);

    // Disable profile check dialog for RPC imports
    pCore->bin()->shouldCheckProfile = false;

    // Use createClipFromFile directly - this avoids qApp->processEvents() calls
    // that cause event loop reentrancy and freezes when called from RPC handler
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };

    fprintf(stderr, "BinHandler::handleImportClip: calling createClipFromFile\n");
    fflush(stderr);

    QString clipId = ClipCreator::createClipFromFile(url.toLocalFile(), folderId, pCore->projectItemModel(), undo, redo);

    fprintf(stderr, "BinHandler::handleImportClip: createClipFromFile returned clipId=%s\n", clipId.toUtf8().constData());
    fflush(stderr);

    if (clipId.isEmpty() || clipId == QLatin1String("-1")) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to import clip")}}}};
    }

    // Return immediately - clip loading happens async in ClipLoadTask
    // We cannot wait here because ClipLoadTask uses BlockingQueuedConnection
    // to call setProducer(), which would deadlock with processEvents()
    // Client should poll getClipInfo() and check the 'loading' field
    fprintf(stderr, "BinHandler::handleImportClip: returning clipId=%s (loading async)\n", clipId.toUtf8().constData());
    fflush(stderr);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("clipId"), clipId}}}};
}

auto BinHandler::handleImportClips(const QJsonObject &params) -> QJsonObject
{
    fprintf(stderr, "BinHandler::handleImportClips START\n");
    fflush(stderr);

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
    if (folderId.isEmpty()) {
        // Use root folder
        folderId = pCore->projectItemModel()->getRootFolder()->clipId();
    }

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

    fprintf(stderr, "BinHandler::handleImportClips: importing %lld files to folder %s\n", urlList.size(), folderId.toUtf8().constData());
    fflush(stderr);

    // Import each clip - no waiting, same as handleImportClip
    QJsonArray clipIds;

    for (const QUrl &url : urlList) {
        fprintf(stderr, "BinHandler::handleImportClips: importing %s\n", url.toLocalFile().toUtf8().constData());
        fflush(stderr);

        pCore->bin()->shouldCheckProfile = false;

        Fun undo = []() { return true; };
        Fun redo = []() { return true; };

        QString clipId = ClipCreator::createClipFromFile(url.toLocalFile(), folderId, pCore->projectItemModel(), undo, redo);

        fprintf(stderr, "BinHandler::handleImportClips: createClipFromFile returned clipId=%s\n", clipId.toUtf8().constData());
        fflush(stderr);

        if (clipId.isEmpty() || clipId == QLatin1String("-1")) {
            continue;
        }

        clipIds.append(clipId);
    }

    // Return immediately - clip loading happens async in ClipLoadTask
    // Client should poll getClipInfo() for each clip if needed
    fprintf(stderr, "BinHandler::handleImportClips: returning %lld clipIds (loading async)\n", clipIds.size());
    fflush(stderr);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("clipIds"), clipIds}}}};
}

auto BinHandler::handleDeleteClip(const QJsonObject &params) -> QJsonObject
{
    fprintf(stderr, "BinHandler::handleDeleteClip START\n");
    fflush(stderr);

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

    fprintf(stderr, "BinHandler::handleDeleteClip: deleting clipId=%s\n", clipId.toUtf8().constData());
    fflush(stderr);

    auto clip = model->getClipByBinID(clipId);
    if (!clip) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Clip not found: %1").arg(clipId)}}}};
    }

    // Cancel any pending jobs for this clip before deletion to avoid deadlock
    FileStatus::ClipStatus status = clip->clipStatus();
    fprintf(stderr, "BinHandler::handleDeleteClip: clipStatus=%d\n", static_cast<int>(status));
    fflush(stderr);

    if (status == FileStatus::StatusWaiting) {
        fprintf(stderr, "BinHandler::handleDeleteClip: clip still loading, canceling jobs...\n");
        fflush(stderr);

        // Cancel all pending jobs for this clip
        pCore->taskManager.discardJobs(ObjectId(KdenliveObjectType::BinClip, clipId.toInt(), QUuid()), AbstractTask::NOJOBTYPE, true);

        // Process events to allow job cancellation to complete
        qApp->processEvents(QEventLoop::AllEvents, 100);

        fprintf(stderr, "BinHandler::handleDeleteClip: jobs canceled, proceeding with deletion\n");
        fflush(stderr);
    }

    Fun undo = []() { return true; };
    Fun redo = []() { return true; };

    fprintf(stderr, "BinHandler::handleDeleteClip: calling requestBinClipDeletion\n");
    fflush(stderr);

    if (!model->requestBinClipDeletion(clip, undo, redo)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to delete clip")}}}};
    }

    fprintf(stderr, "BinHandler::handleDeleteClip: deleted successfully\n");
    fflush(stderr);

    pCore->pushUndo(undo, redo, i18n("Delete bin clip"));

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("deleted"), true}}}};
}

auto BinHandler::handleDeleteClips(const QJsonObject &params) -> QJsonObject
{
    fprintf(stderr, "BinHandler::handleDeleteClips START\n");
    fflush(stderr);

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

    fprintf(stderr, "BinHandler::handleDeleteClips: deleting %lld clips\n", static_cast<long long>(clipIds.size()));
    fflush(stderr);

    // First, collect all clips and wait for any that are still loading
    QList<std::shared_ptr<AbstractProjectItem>> clipsToDelete;
    QList<std::shared_ptr<ProjectClip>> loadingClips;

    for (const QJsonValue &val : clipIds) {
        QString clipId = val.toString();
        fprintf(stderr, "BinHandler::handleDeleteClips: checking clipId=%s\n", clipId.toUtf8().constData());
        fflush(stderr);

        auto clip = model->getClipByBinID(clipId);
        if (clip) {
            FileStatus::ClipStatus status = clip->clipStatus();
            fprintf(stderr, "BinHandler::handleDeleteClips: clipStatus=%d\n", static_cast<int>(status));
            fflush(stderr);

            if (status == FileStatus::StatusWaiting) {
                loadingClips.append(clip);
            }
            clipsToDelete.append(clip);
        }
    }

    // Cancel pending jobs for any loading clips to avoid deadlock during deletion
    if (!loadingClips.isEmpty()) {
        fprintf(stderr, "BinHandler::handleDeleteClips: canceling jobs for %lld loading clips...\n", static_cast<long long>(loadingClips.size()));
        fflush(stderr);

        for (const auto &clip : loadingClips) {
            QString clipIdStr = clip->clipId();
            fprintf(stderr, "BinHandler::handleDeleteClips: canceling jobs for clip %s\n", clipIdStr.toUtf8().constData());
            fflush(stderr);

            pCore->taskManager.discardJobs(ObjectId(KdenliveObjectType::BinClip, clipIdStr.toInt(), QUuid()), AbstractTask::NOJOBTYPE, true);

            // Process events after each cancellation to prevent blocking
            qApp->processEvents(QEventLoop::AllEvents, 100);
        }

        fprintf(stderr, "BinHandler::handleDeleteClips: jobs canceled, proceeding with deletion\n");
        fflush(stderr);
    }

    fprintf(stderr, "BinHandler::handleDeleteClips: %lld clips to delete\n", static_cast<long long>(clipsToDelete.size()));
    fflush(stderr);

    if (clipsToDelete.isEmpty()) {
        fprintf(stderr, "BinHandler::handleDeleteClips: returning early (no clips found)\n");
        fflush(stderr);
        return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("count"), 0}}}};
    }

    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    int deletedCount = 0;

    for (const auto &clip : clipsToDelete) {
        fprintf(stderr, "BinHandler::handleDeleteClips: calling requestBinClipDeletion\n");
        fflush(stderr);

        if (model->requestBinClipDeletion(clip, undo, redo)) {
            deletedCount++;
            fprintf(stderr, "BinHandler::handleDeleteClips: deleted successfully\n");
            fflush(stderr);
        }
    }

    fprintf(stderr, "BinHandler::handleDeleteClips: deleted %d clips\n", deletedCount);
    fflush(stderr);

    if (deletedCount > 0) {
        pCore->pushUndo(undo, redo, i18n("Delete bin clips"));
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("count"), deletedCount}}}};
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

    // Check if item is still loading - if so, return error to avoid deadlock
    if (item->clipStatus() == FileStatus::StatusWaiting) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Item is still loading, try again later")}}}};
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

    // Check if item is still loading - if so, return error to avoid deadlock
    if (item->clipStatus() == FileStatus::StatusWaiting) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Item is still loading, try again later")}}}};
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

    // Check if clip is still loading - if so, return empty markers to avoid deadlock
    if (clip->clipStatus() == FileStatus::StatusWaiting) {
        return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("markers"), QJsonArray()}, {QStringLiteral("loading"), true}}}};
    }

    auto markerModel = clip->markerModel();
    if (!markerModel) {
        return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("markers"), QJsonArray()}}}};
    }

    QJsonArray markers;
    QList<CommentedTime> allMarkers = markerModel->getAllMarkers();

    for (const CommentedTime &marker : allMarkers) {
        QJsonObject markerInfo;
        int position = marker.time().frames(pCore->getCurrentFps());
        markerInfo[QStringLiteral("id")] = position; // Use position as ID (markers are identified by position)
        markerInfo[QStringLiteral("position")] = position;
        markerInfo[QStringLiteral("comment")] = marker.comment();
        markerInfo[QStringLiteral("type")] = marker.markerType();
        markers.append(markerInfo);
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("markers"), markers}}}};
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

    auto clip = model->getClipByBinID(clipId);
    if (!clip) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Clip not found: %1").arg(clipId)}}}};
    }

    // Check if clip is still loading - if so, return error to avoid deadlock
    if (clip->clipStatus() == FileStatus::StatusWaiting) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Clip is still loading, try again later")}}}};
    }

    auto markerModel = clip->markerModel();
    if (!markerModel) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("No marker model for clip")}}}};
    }

    QString comment = params.value(QStringLiteral("comment")).toString();
    if (comment.isEmpty()) {
        comment = i18n("Marker");
    }
    int type = params.value(QStringLiteral("type")).toInt(-1);

    // Convert position to GenTime and add marker directly
    GenTime pos(position, pCore->getCurrentFps());
    bool success = markerModel->addMarker(pos, comment, type);

    if (!success) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to add marker")}}}};
    }

    // Return position as markerId (markers are identified by position in Kdenlive)
    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("markerId"), position}}}};
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

    // Accept either 'markerId' or 'position' (markerId is preferred, position for backwards compat)
    int position = params.value(QStringLiteral("markerId")).toInt(-1);
    if (position < 0) {
        position = params.value(QStringLiteral("position")).toInt(-1);
    }
    if (position < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'markerId' parameter")}}}};
    }

    auto clip = model->getClipByBinID(clipId);
    if (!clip) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Clip not found: %1").arg(clipId)}}}};
    }

    // Check if clip is still loading - if so, return error to avoid deadlock
    if (clip->clipStatus() == FileStatus::StatusWaiting) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Clip is still loading, try again later")}}}};
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
