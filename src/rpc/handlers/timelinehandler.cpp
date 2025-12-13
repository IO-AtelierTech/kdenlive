/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#include "timelinehandler.h"
#include "../rpcnotifier.h"

#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "mainwindow.h"
#include "monitor/monitor.h"
#include "monitor/monitormanager.h"
#include "project/projectmanager.h"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"
#include "timeline2/view/timelinecontroller.h"
#include "timeline2/view/timelinewidget.h"

#include <QJsonArray>
#include <unordered_set>

TimelineHandler::TimelineHandler(RpcNotifier *notifier, QObject *parent)
    : QObject(parent)
    , m_notifier(notifier)
{
}

TimelineHandler::~TimelineHandler() = default;

QString TimelineHandler::prefix() const
{
    return QStringLiteral("timeline");
}

QStringList TimelineHandler::supportedMethods() const
{
    return QStringList{QStringLiteral("getInfo"),      QStringLiteral("getTracks"),   QStringLiteral("getClips"),
                       QStringLiteral("getClip"),      QStringLiteral("insertClip"),  QStringLiteral("moveClip"),
                       QStringLiteral("deleteClip"),   QStringLiteral("deleteClips"), QStringLiteral("resizeClip"),
                       QStringLiteral("splitClip"),    QStringLiteral("seek"),        QStringLiteral("getPosition"),
                       QStringLiteral("addTrack"),     QStringLiteral("deleteTrack"), QStringLiteral("setTrackProperty"),
                       QStringLiteral("getSelection"), QStringLiteral("setSelection")};
}

QJsonObject TimelineHandler::handle(const QString &method, const QJsonObject &params)
{
    if (method == QLatin1String("getInfo")) {
        return handleGetInfo(params);
    }
    if (method == QLatin1String("getTracks")) {
        return handleGetTracks(params);
    }
    if (method == QLatin1String("getClips")) {
        return handleGetClips(params);
    }
    if (method == QLatin1String("getClip")) {
        return handleGetClip(params);
    }
    if (method == QLatin1String("insertClip")) {
        return handleInsertClip(params);
    }
    if (method == QLatin1String("moveClip")) {
        return handleMoveClip(params);
    }
    if (method == QLatin1String("deleteClip")) {
        return handleDeleteClip(params);
    }
    if (method == QLatin1String("deleteClips")) {
        return handleDeleteClips(params);
    }
    if (method == QLatin1String("resizeClip")) {
        return handleResizeClip(params);
    }
    if (method == QLatin1String("splitClip")) {
        return handleSplitClip(params);
    }
    if (method == QLatin1String("seek")) {
        return handleSeek(params);
    }
    if (method == QLatin1String("getPosition")) {
        return handleGetPosition(params);
    }
    if (method == QLatin1String("addTrack")) {
        return handleAddTrack(params);
    }
    if (method == QLatin1String("deleteTrack")) {
        return handleDeleteTrack(params);
    }
    if (method == QLatin1String("setTrackProperty")) {
        return handleSetTrackProperty(params);
    }
    if (method == QLatin1String("getSelection")) {
        return handleGetSelection(params);
    }
    if (method == QLatin1String("setSelection")) {
        return handleSetSelection(params);
    }

    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::MethodNotFound},
                                                             {QStringLiteral("message"), QStringLiteral("Unknown method: timeline.%1").arg(method)}}}};
}

QJsonObject TimelineHandler::makeProjectNotOpenError()
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ProjectNotOpen},
                                                             {QStringLiteral("message"), QStringLiteral("No project is currently open")}}}};
}

QJsonObject TimelineHandler::makeNoTimelineError()
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                             {QStringLiteral("message"), QStringLiteral("No timeline is currently active")}}}};
}

QJsonObject TimelineHandler::handleGetInfo(const QJsonObject & /*params*/)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    auto *controller = pCore->window()->getCurrentTimeline()->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    QJsonObject info;
    info[QStringLiteral("duration")] = model->duration();
    info[QStringLiteral("trackCount")] = model->getTracksCount();
    info[QStringLiteral("fps")] = pCore->getCurrentFps();
    info[QStringLiteral("profile")] = doc->getDocumentProperty(QStringLiteral("kdenlive:docproperties.profile"));

    QPair<int, int> avTracks = model->getAVtracksCount();
    info[QStringLiteral("audioTrackCount")] = avTracks.first;
    info[QStringLiteral("videoTrackCount")] = avTracks.second;

    return QJsonObject{{QStringLiteral("result"), info}};
}

QJsonObject TimelineHandler::handleGetTracks(const QJsonObject & /*params*/)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    auto *controller = pCore->window()->getCurrentTimeline()->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    QJsonArray tracks;
    auto trackIds = model->getAllTracksIds();

    for (int trackId : trackIds) {
        QJsonObject trackInfo;
        trackInfo[QStringLiteral("id")] = trackId;

        // Get track name
        QModelIndex trackIndex = model->makeTrackIndexFromID(trackId);
        trackInfo[QStringLiteral("name")] = model->data(trackIndex, TimelineModel::NameRole).toString();

        // Get track type
        bool isAudio = model->isAudioTrack(trackId);
        trackInfo[QStringLiteral("type")] = isAudio ? QStringLiteral("audio") : QStringLiteral("video");

        // Get track properties
        trackInfo[QStringLiteral("locked")] = model->data(trackIndex, TimelineModel::IsLockedRole).toBool();
        trackInfo[QStringLiteral("muted")] = model->data(trackIndex, TimelineModel::IsDisabledRole).toBool();
        trackInfo[QStringLiteral("hidden")] = !model->data(trackIndex, TimelineModel::IsCompositeRole).toBool();
        trackInfo[QStringLiteral("position")] = model->getTrackPosition(trackId);

        tracks.append(trackInfo);
    }

    return QJsonObject{{QStringLiteral("result"), tracks}};
}

QJsonObject TimelineHandler::handleGetClips(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    auto *controller = pCore->window()->getCurrentTimeline()->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    // Optional track filter
    int filterTrackId = params.value(QStringLiteral("trackId")).toInt(-1);

    QJsonArray clips;
    auto trackIds = model->getAllTracksIds();

    for (int trackId : trackIds) {
        if (filterTrackId >= 0 && trackId != filterTrackId) {
            continue;
        }

        // Get all items in this track and filter for clips
        std::unordered_set<int> trackItems = model->getItemsInRange(trackId, 0, -1, false);
        for (int itemId : trackItems) {
            if (!model->isClip(itemId)) {
                continue;
            }

            QJsonObject clipInfo;
            clipInfo[QStringLiteral("id")] = itemId;
            clipInfo[QStringLiteral("binId")] = model->getClipBinId(itemId);
            clipInfo[QStringLiteral("trackId")] = trackId;
            clipInfo[QStringLiteral("position")] = model->getClipPosition(itemId);
            clipInfo[QStringLiteral("duration")] = model->getClipPlaytime(itemId);

            auto inOut = model->getClipInOut(itemId);
            clipInfo[QStringLiteral("in")] = inOut.first;
            clipInfo[QStringLiteral("out")] = inOut.second;

            clips.append(clipInfo);
        }
    }

    return QJsonObject{{QStringLiteral("result"), clips}};
}

QJsonObject TimelineHandler::handleGetClip(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    auto *controller = pCore->window()->getCurrentTimeline()->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    if (clipId < 0 || !model->isClip(clipId)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Clip not found: %1").arg(clipId)}}}};
    }

    QJsonObject clipInfo;
    clipInfo[QStringLiteral("id")] = clipId;
    clipInfo[QStringLiteral("binId")] = model->getClipBinId(clipId);
    clipInfo[QStringLiteral("trackId")] = model->getClipTrackId(clipId);
    clipInfo[QStringLiteral("position")] = model->getClipPosition(clipId);
    clipInfo[QStringLiteral("duration")] = model->getClipPlaytime(clipId);

    auto inOut = model->getClipInOut(clipId);
    clipInfo[QStringLiteral("in")] = inOut.first;
    clipInfo[QStringLiteral("out")] = inOut.second;

    clipInfo[QStringLiteral("speed")] = model->getClipSpeed(clipId);
    clipInfo[QStringLiteral("name")] = model->getClipName(clipId);

    return QJsonObject{{QStringLiteral("result"), clipInfo}};
}

QJsonObject TimelineHandler::handleInsertClip(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    auto *controller = pCore->window()->getCurrentTimeline()->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    QString binId = params.value(QStringLiteral("binId")).toString();
    if (binId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'binId' parameter")}}}};
    }

    int trackId = params.value(QStringLiteral("trackId")).toInt(-1);
    if (trackId < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing or invalid 'trackId' parameter")}}}};
    }

    int position = params.value(QStringLiteral("position")).toInt(0);

    // Get the bin clip and create XML for insertion
    auto binClip = pCore->projectItemModel()->getClipByBinID(binId);
    if (!binClip) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Bin clip not found: %1").arg(binId)}}}};
    }

    QDomDocument doc2;
    QDomElement xml = binClip->toXml(doc2, false, true);

    int clipId = controller->insertClip(trackId, position, QString::fromUtf8(doc2.toByteArray()), true, true, false);

    if (clipId == -1) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to insert clip")}}}};
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("clipId"), clipId}}}};
}

QJsonObject TimelineHandler::handleMoveClip(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    auto *controller = pCore->window()->getCurrentTimeline()->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    if (clipId < 0 || !model->isClip(clipId)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Clip not found: %1").arg(clipId)}}}};
    }

    int trackId = params.value(QStringLiteral("trackId")).toInt(model->getClipTrackId(clipId));
    int position = params.value(QStringLiteral("position")).toInt(-1);

    if (position < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'position' parameter")}}}};
    }

    bool success = model->requestClipMove(clipId, trackId, position, true, true, true, false);

    if (!success) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to move clip")}}}};
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("moved"), true}}}};
}

QJsonObject TimelineHandler::handleDeleteClip(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    auto *controller = pCore->window()->getCurrentTimeline()->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    if (clipId < 0 || !model->isClip(clipId)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Clip not found: %1").arg(clipId)}}}};
    }

    bool success = model->requestItemDeletion(clipId, true);

    if (!success) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to delete clip")}}}};
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("deleted"), true}}}};
}

QJsonObject TimelineHandler::handleDeleteClips(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    auto *controller = pCore->window()->getCurrentTimeline()->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    QJsonArray clipIds = params.value(QStringLiteral("clipIds")).toArray();
    if (clipIds.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'clipIds' parameter")}}}};
    }

    QJsonArray deleted;
    QJsonArray failed;

    for (const QJsonValue &val : clipIds) {
        int clipId = val.toInt(-1);
        if (clipId < 0 || !model->isClip(clipId)) {
            failed.append(clipId);
            continue;
        }

        bool success = model->requestItemDeletion(clipId, true);
        if (success) {
            deleted.append(clipId);
        } else {
            failed.append(clipId);
        }
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("deleted"), deleted}, {QStringLiteral("failed"), failed}}}};
}

QJsonObject TimelineHandler::handleResizeClip(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    auto *controller = pCore->window()->getCurrentTimeline()->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    if (clipId < 0 || !model->isClip(clipId)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Clip not found: %1").arg(clipId)}}}};
    }

    // Get current in/out
    auto currentInOut = model->getClipInOut(clipId);
    int currentIn = currentInOut.first;
    int currentOut = currentInOut.second;

    // Check for new in/out values
    bool hasIn = params.contains(QStringLiteral("in"));
    bool hasOut = params.contains(QStringLiteral("out"));

    if (!hasIn && !hasOut) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'in' or 'out' parameter")}}}};
    }

    int newIn = hasIn ? params.value(QStringLiteral("in")).toInt() : currentIn;
    int newOut = hasOut ? params.value(QStringLiteral("out")).toInt() : currentOut;
    int newDuration = newOut - newIn + 1;

    // Resize the clip
    int result = model->requestItemResize(clipId, newDuration, !hasIn, true);

    if (result == -1) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to resize clip")}}}};
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("resized"), true}, {QStringLiteral("newDuration"), result}}}};
}

QJsonObject TimelineHandler::handleSplitClip(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    auto *controller = pCore->window()->getCurrentTimeline()->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    if (clipId < 0 || !model->isClip(clipId)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Clip not found: %1").arg(clipId)}}}};
    }

    int position = params.value(QStringLiteral("position")).toInt(-1);
    if (position < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'position' parameter")}}}};
    }

    // Get the track ID for the clip
    int trackId = model->getClipTrackId(clipId);

    // Use the controller to cut the clip
    controller->requestClipCut(clipId, position);

    // Find the new clip that was created after the cut
    // The new clip should be at position after the cut on the same track
    int newClipId = model->getClipByStartPosition(trackId, position);

    return QJsonObject{
        {QStringLiteral("result"), QJsonObject{{QStringLiteral("split"), true}, {QStringLiteral("newClipId"), newClipId >= 0 ? newClipId : QJsonValue()}}}};
}

QJsonObject TimelineHandler::handleSeek(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    auto *controller = pCore->window()->getCurrentTimeline()->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    int position = params.value(QStringLiteral("position")).toInt(-1);
    if (position < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'position' parameter")}}}};
    }

    controller->setPosition(position);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("position"), position}}}};
}

QJsonObject TimelineHandler::handleGetPosition(const QJsonObject & /*params*/)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    auto *controller = pCore->window()->getCurrentTimeline()->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    int position = pCore->monitorManager()->projectMonitor()->position();
    int duration = model->duration();

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("position"), position}, {QStringLiteral("duration"), duration}}}};
}

QJsonObject TimelineHandler::handleAddTrack(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    auto *controller = pCore->window()->getCurrentTimeline()->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    QString typeStr = params.value(QStringLiteral("type")).toString(QStringLiteral("video"));
    bool isAudio = (typeStr.toLower() == QLatin1String("audio"));

    int position = params.value(QStringLiteral("position")).toInt(-1);

    int newTrackId = -1;
    bool success = model->requestTrackInsertion(position, newTrackId, QString(), isAudio);

    if (!success || newTrackId < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to add track")}}}};
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("trackId"), newTrackId}}}};
}

QJsonObject TimelineHandler::handleDeleteTrack(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    auto *controller = pCore->window()->getCurrentTimeline()->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    int trackId = params.value(QStringLiteral("trackId")).toInt(-1);
    if (trackId < 0 || !model->isTrack(trackId)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::TrackNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Track not found: %1").arg(trackId)}}}};
    }

    bool success = model->requestTrackDeletion(trackId);

    if (!success) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to delete track")}}}};
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("deleted"), true}}}};
}

QJsonObject TimelineHandler::handleSetTrackProperty(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    auto *controller = pCore->window()->getCurrentTimeline()->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    int trackId = params.value(QStringLiteral("trackId")).toInt(-1);
    if (trackId < 0 || !model->isTrack(trackId)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::TrackNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Track not found: %1").arg(trackId)}}}};
    }

    QString property = params.value(QStringLiteral("property")).toString();
    if (property.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'property' parameter")}}}};
    }

    QJsonValue value = params.value(QStringLiteral("value"));

    if (property == QLatin1String("locked")) {
        model->setTrackLockedState(trackId, value.toBool());
    } else if (property == QLatin1String("muted") || property == QLatin1String("disabled")) {
        // Use hide track for muting
        controller->hideTrack(trackId, value.toBool());
    } else {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Unknown property: %1").arg(property)}}}};
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("set"), true}}}};
}

QJsonObject TimelineHandler::handleGetSelection(const QJsonObject & /*params*/)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    auto *controller = pCore->window()->getCurrentTimeline()->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    QList<int> selection = controller->selection();

    QJsonArray clipIds;
    QJsonArray compositionIds;

    for (int id : selection) {
        if (model->isClip(id)) {
            clipIds.append(id);
        } else if (model->isComposition(id)) {
            compositionIds.append(id);
        }
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("clipIds"), clipIds}, {QStringLiteral("compositionIds"), compositionIds}}}};
}

QJsonObject TimelineHandler::handleSetSelection(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    auto *controller = pCore->window()->getCurrentTimeline()->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    QJsonArray clipIds = params.value(QStringLiteral("clipIds")).toArray();

    // Convert to QList<int>
    QList<int> ids;
    for (const QJsonValue &val : clipIds) {
        int id = val.toInt(-1);
        if (id >= 0 && model->isClip(id)) {
            ids.append(id);
        }
    }

    // Clear selection first
    model->requestClearSelection();

    // Set new selection
    controller->selectItems(ids);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("selected"), ids.count()}}}};
}
