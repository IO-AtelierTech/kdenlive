/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#include "compositionhandler.h"
#include "../rpcnotifier.h"

#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "mainwindow.h"
#include "project/projectmanager.h"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"
#include "timeline2/view/timelinecontroller.h"
#include "timeline2/view/timelinewidget.h"
#include "transitions/transitionsrepository.hpp"

#include <QJsonArray>
#include <unordered_set>

CompositionHandler::CompositionHandler(RpcNotifier *notifier, QObject *parent)
    : QObject(parent)
    , m_notifier(notifier)
{
}

CompositionHandler::~CompositionHandler() = default;

auto CompositionHandler::prefix() const -> QString
{
    return QStringLiteral("composition");
}

auto CompositionHandler::supportedMethods() const -> QStringList
{
    return QStringList{QStringLiteral("list"), QStringLiteral("add"), QStringLiteral("remove"), QStringLiteral("getProperties"), QStringLiteral("setProperty")};
}

auto CompositionHandler::handle(const QString &method, const QJsonObject &params) -> QJsonObject
{
    if (method == QLatin1String("list")) {
        return handleList(params);
    }
    if (method == QLatin1String("add")) {
        return handleAdd(params);
    }
    if (method == QLatin1String("remove")) {
        return handleRemove(params);
    }
    if (method == QLatin1String("getProperties")) {
        return handleGetProperties(params);
    }
    if (method == QLatin1String("setProperty")) {
        return handleSetProperty(params);
    }

    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::MethodNotFound},
                                                             {QStringLiteral("message"), QStringLiteral("Unknown method: composition.%1").arg(method)}}}};
}

auto CompositionHandler::makeProjectNotOpenError() -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ProjectNotOpen},
                                                             {QStringLiteral("message"), QStringLiteral("No project is currently open")}}}};
}

auto CompositionHandler::makeNoTimelineError() -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::TimelineNotReady},
                                                             {QStringLiteral("message"), QStringLiteral("No timeline is currently active")}}}};
}

auto CompositionHandler::makeApplicationClosingError() -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ApplicationClosing},
                                                             {QStringLiteral("message"), QStringLiteral("Application is shutting down")}}}};
}

auto CompositionHandler::makeWindowNotAvailableError() -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::WindowNotAvailable},
                                                             {QStringLiteral("message"), QStringLiteral("Main window not available")}}}};
}

auto CompositionHandler::handleList(const QJsonObject & /*params*/) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    // Check window availability before accessing timeline
    if (!pCore->window()) {
        return makeWindowNotAvailableError();
    }

    auto *timeline = pCore->window()->getCurrentTimeline();
    if (!timeline) {
        return makeNoTimelineError();
    }
    auto *controller = timeline->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    QJsonArray compositions;

    // Get all items in the timeline and filter for compositions
    std::unordered_set<int> allItems = model->getItemsInRange(-1, 0, -1, true);
    for (int itemId : allItems) {
        if (!model->isComposition(itemId)) {
            continue;
        }

        QJsonObject compoInfo;
        compoInfo[QStringLiteral("id")] = itemId;
        compoInfo[QStringLiteral("trackId")] = model->getCompositionTrackId(itemId);
        compoInfo[QStringLiteral("position")] = model->getCompositionPosition(itemId);
        compoInfo[QStringLiteral("duration")] = model->getCompositionPlaytime(itemId);

        // Get aTrack via controller's public method
        QPair<int, int> aTrackInfo = controller->getCompositionATrack(itemId);
        compoInfo[QStringLiteral("aTrack")] = aTrackInfo.first;

        // Get composition name via parameter model
        auto paramModel = model->getCompositionParameterModel(itemId);
        if (paramModel) {
            QString assetId = paramModel->getAssetId();
            compoInfo[QStringLiteral("name")] = TransitionsRepository::get()->getName(assetId);
        }

        compositions.append(compoInfo);
    }

    return QJsonObject{{QStringLiteral("result"), compositions}};
}

auto CompositionHandler::handleAdd(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    // Check window availability before accessing timeline
    if (!pCore->window()) {
        return makeWindowNotAvailableError();
    }

    auto *timeline = pCore->window()->getCurrentTimeline();
    if (!timeline) {
        return makeNoTimelineError();
    }
    auto *controller = timeline->controller();
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

    int position = params.value(QStringLiteral("position")).toInt(-1);
    if (position < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'position' parameter")}}}};
    }

    QString compositionId = params.value(QStringLiteral("compositionId")).toString(QStringLiteral("composite"));
    int duration = params.value(QStringLiteral("duration")).toInt(static_cast<int>(pCore->getCurrentFps())); // Default to 1 second

    int newCompoId = controller->insertComposition(trackId, position, compositionId, true);

    if (newCompoId < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to add composition")}}}};
    }

    // Resize if a specific duration was requested
    if (duration > 0 && params.contains(QStringLiteral("duration"))) {
        model->requestItemResize(newCompoId, duration, true, true);
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("compositionId"), newCompoId}}}};
}

auto CompositionHandler::handleRemove(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    // Check window availability before accessing timeline
    if (!pCore->window()) {
        return makeWindowNotAvailableError();
    }

    auto *timeline = pCore->window()->getCurrentTimeline();
    if (!timeline) {
        return makeNoTimelineError();
    }
    auto *controller = timeline->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    int compositionId = params.value(QStringLiteral("compositionId")).toInt(-1);
    if (compositionId < 0 || !model->isComposition(compositionId)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Composition not found: %1").arg(compositionId)}}}};
    }

    bool success = model->requestItemDeletion(compositionId, true);

    if (!success) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to remove composition")}}}};
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("removed"), true}}}};
}

auto CompositionHandler::handleGetProperties(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    // Check window availability before accessing timeline
    if (!pCore->window()) {
        return makeWindowNotAvailableError();
    }

    auto *timeline = pCore->window()->getCurrentTimeline();
    if (!timeline) {
        return makeNoTimelineError();
    }
    auto *controller = timeline->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    int compositionId = params.value(QStringLiteral("compositionId")).toInt(-1);
    if (compositionId < 0 || !model->isComposition(compositionId)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Composition not found: %1").arg(compositionId)}}}};
    }

    QJsonObject properties;

    // Get basic composition info
    properties[QStringLiteral("position")] = model->getCompositionPosition(compositionId);
    properties[QStringLiteral("duration")] = model->getCompositionPlaytime(compositionId);
    properties[QStringLiteral("trackId")] = model->getCompositionTrackId(compositionId);

    // Get aTrack info via controller's public method
    QPair<int, int> aTrackInfo = controller->getCompositionATrack(compositionId);
    properties[QStringLiteral("aTrack")] = aTrackInfo.first;

    // Check if composition has auto track (forcedTrack == -1 means auto)
    properties[QStringLiteral("autoTrack")] = controller->compositionAutoTrack(compositionId);

    // Get composition name via parameter model
    auto paramModel = model->getCompositionParameterModel(compositionId);
    if (paramModel) {
        QString assetId = paramModel->getAssetId();
        properties[QStringLiteral("name")] = TransitionsRepository::get()->getName(assetId);
    }

    return QJsonObject{{QStringLiteral("result"), properties}};
}

auto CompositionHandler::handleSetProperty(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->projectManager()->current();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    // Check window availability before accessing timeline
    if (!pCore->window()) {
        return makeWindowNotAvailableError();
    }

    auto *timeline = pCore->window()->getCurrentTimeline();
    if (!timeline) {
        return makeNoTimelineError();
    }
    auto *controller = timeline->controller();
    if (!controller) {
        return makeNoTimelineError();
    }

    auto model = controller->getModel();
    if (!model) {
        return makeNoTimelineError();
    }

    int compositionId = params.value(QStringLiteral("compositionId")).toInt(-1);
    if (compositionId < 0 || !model->isComposition(compositionId)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Composition not found: %1").arg(compositionId)}}}};
    }

    QString property = params.value(QStringLiteral("property")).toString();
    if (property.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'property' parameter")}}}};
    }

    QJsonValue value = params.value(QStringLiteral("value"));

    if (property == QLatin1String("aTrack")) {
        int aTrack = value.toInt(-1);
        controller->setCompositionATrack(compositionId, aTrack);
        return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("set"), true}}}};
    }

    if (property == QLatin1String("duration")) {
        int duration = value.toInt(-1);
        if (duration <= 0) {
            return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                     {QStringLiteral("message"), QStringLiteral("Invalid duration value")}}}};
        }

        int result = model->requestItemResize(compositionId, duration, true, true);
        if (result < 0) {
            return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                     {QStringLiteral("message"), QStringLiteral("Failed to resize composition")}}}};
        }
        return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("set"), true}, {QStringLiteral("newDuration"), result}}}};
    }

    if (property == QLatin1String("position")) {
        int position = value.toInt(-1);
        if (position < 0) {
            return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                     {QStringLiteral("message"), QStringLiteral("Invalid position value")}}}};
        }

        int trackId = model->getCompositionTrackId(compositionId);
        bool success = model->requestCompositionMove(compositionId, trackId, position, true, true);
        if (!success) {
            return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                     {QStringLiteral("message"), QStringLiteral("Failed to move composition")}}}};
        }
        return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("set"), true}}}};
    }

    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                             {QStringLiteral("message"), QStringLiteral("Unknown property: %1").arg(property)}}}};
}
