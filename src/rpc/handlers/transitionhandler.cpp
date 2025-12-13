/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#include "transitionhandler.h"
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

TransitionHandler::TransitionHandler(RpcNotifier *notifier, QObject *parent)
    : QObject(parent)
    , m_notifier(notifier)
    , m_isTransitionPrefix(true)
{
}

TransitionHandler::~TransitionHandler() = default;

auto TransitionHandler::prefix() const -> QString
{
    // This handler handles both transition.* and composition.* - we return transition as the main prefix
    // The dispatcher will also register us for composition.* methods
    return QStringLiteral("transition");
}

auto TransitionHandler::supportedMethods() const -> QStringList
{
    return QStringList{// Transition methods
                       QStringLiteral("list"), QStringLiteral("add"), QStringLiteral("remove"), QStringLiteral("getProperties"), QStringLiteral("setProperty")};
}

auto TransitionHandler::handle(const QString &method, const QJsonObject &params) -> QJsonObject
{
    // Handle transition.* methods
    if (method == QLatin1String("list")) {
        return handleTransitionList(params);
    }
    if (method == QLatin1String("add")) {
        return handleTransitionAdd(params);
    }
    if (method == QLatin1String("remove")) {
        return handleTransitionRemove(params);
    }
    if (method == QLatin1String("getProperties")) {
        return handleTransitionGetProperties(params);
    }
    if (method == QLatin1String("setProperty")) {
        return handleTransitionSetProperty(params);
    }

    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::MethodNotFound},
                                                             {QStringLiteral("message"), QStringLiteral("Unknown method: transition.%1").arg(method)}}}};
}

auto TransitionHandler::makeProjectNotOpenError() -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ProjectNotOpen},
                                                             {QStringLiteral("message"), QStringLiteral("No project is currently open")}}}};
}

auto TransitionHandler::makeNoTimelineError() -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::TimelineNotReady},
                                                             {QStringLiteral("message"), QStringLiteral("No timeline is currently active")}}}};
}

auto TransitionHandler::makeApplicationClosingError() -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ApplicationClosing},
                                                             {QStringLiteral("message"), QStringLiteral("Application is shutting down")}}}};
}

auto TransitionHandler::makeWindowNotAvailableError() -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::WindowNotAvailable},
                                                             {QStringLiteral("message"), QStringLiteral("Main window not available")}}}};
}

auto TransitionHandler::handleTransitionList(const QJsonObject & /*params*/) -> QJsonObject
{
    // Get all available transitions from the repository
    auto allTransitions = TransitionsRepository::get()->getNames();

    QJsonArray transitions;
    for (const auto &transition : allTransitions) {
        QJsonObject transitionInfo;
        transitionInfo[QStringLiteral("id")] = transition.first;
        transitionInfo[QStringLiteral("name")] = transition.second;

        // Check if it's a composition (vs same-track transition/mix)
        bool isComposition = TransitionsRepository::get()->isComposition(transition.first);
        transitionInfo[QStringLiteral("isComposition")] = isComposition;

        // Check if it's audio
        bool isAudio = TransitionsRepository::get()->isAudio(transition.first);
        transitionInfo[QStringLiteral("isAudio")] = isAudio;

        transitions.append(transitionInfo);
    }

    return QJsonObject{{QStringLiteral("result"), transitions}};
}

auto TransitionHandler::handleTransitionAdd(const QJsonObject &params) -> QJsonObject
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

    // Get required parameters
    int clipId1 = params.value(QStringLiteral("clipId1")).toInt(-1);
    int clipId2 = params.value(QStringLiteral("clipId2")).toInt(-1);
    QString transitionId = params.value(QStringLiteral("transitionId")).toString(QStringLiteral("luma"));

    // If we have two clips, create a mix between them
    if (clipId1 >= 0 && clipId2 >= 0) {
        // Use mix functionality
        bool success = model->mixClip(clipId1, transitionId, 0);
        if (!success) {
            return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                     {QStringLiteral("message"), QStringLiteral("Failed to add transition")}}}};
        }

        return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("added"), true}}}};
    }

    // Single clip - add mix with adjacent clip
    if (clipId1 >= 0) {
        int delta = params.value(QStringLiteral("delta")).toInt(0); // -1 = previous, 1 = next, 0 = auto
        bool success = model->mixClip(clipId1, transitionId, delta);

        if (!success) {
            return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                     {QStringLiteral("message"), QStringLiteral("Failed to add transition")}}}};
        }

        return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("added"), true}}}};
    }

    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                             {QStringLiteral("message"), QStringLiteral("Missing clipId1 parameter")}}}};
}

auto TransitionHandler::handleTransitionRemove(const QJsonObject &params) -> QJsonObject
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

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    if (clipId < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing clipId parameter")}}}};
    }

    // Remove mix from clip
    bool success = model->removeMix(clipId);

    if (!success) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to remove transition")}}}};
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("removed"), true}}}};
}

auto TransitionHandler::handleTransitionGetProperties(const QJsonObject &params) -> QJsonObject
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

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    if (clipId < 0 || !model->isClip(clipId)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Clip not found: %1").arg(clipId)}}}};
    }

    // Get the mix stack model for this clip
    auto mixStack = model->getClipMixStackModel(clipId);
    if (!mixStack) {
        return QJsonObject{{QStringLiteral("result"), QJsonObject{}}}; // No mix on this clip
    }

    // Get mix properties
    QJsonObject properties;
    auto mixInfo = model->getMixInOut(clipId);
    properties[QStringLiteral("mixIn")] = mixInfo.first;
    properties[QStringLiteral("mixOut")] = mixInfo.second;
    properties[QStringLiteral("duration")] = model->getMixDuration(clipId);

    return QJsonObject{{QStringLiteral("result"), properties}};
}

auto TransitionHandler::handleTransitionSetProperty(const QJsonObject &params) -> QJsonObject
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

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    if (clipId < 0 || !model->isClip(clipId)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Clip not found: %1").arg(clipId)}}}};
    }

    QString property = params.value(QStringLiteral("property")).toString();
    if (property.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'property' parameter")}}}};
    }

    // Handle duration resize
    if (property == QLatin1String("duration")) {
        int duration = params.value(QStringLiteral("value")).toInt(-1);
        if (duration <= 0) {
            return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                     {QStringLiteral("message"), QStringLiteral("Invalid duration value")}}}};
        }

        // Get mix alignment
        MixAlignment align = model->getMixAlign(clipId);
        model->requestResizeMix(clipId, duration, align);

        return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("set"), true}}}};
    }

    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                             {QStringLiteral("message"), QStringLiteral("Unknown property: %1").arg(property)}}}};
}

auto TransitionHandler::handleCompositionList(const QJsonObject & /*params*/) -> QJsonObject
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

auto TransitionHandler::handleCompositionAdd(const QJsonObject &params) -> QJsonObject
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

    int newCompoId = controller->insertComposition(trackId, position, compositionId, true);

    if (newCompoId < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to add composition")}}}};
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("compositionId"), newCompoId}}}};
}

auto TransitionHandler::handleCompositionRemove(const QJsonObject &params) -> QJsonObject
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

auto TransitionHandler::handleCompositionGetProperties(const QJsonObject &params) -> QJsonObject
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

    auto paramModel = model->getCompositionParameterModel(compositionId);
    if (!paramModel) {
        return QJsonObject{{QStringLiteral("result"), QJsonObject{}}};
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
    if (paramModel) {
        QString assetId = paramModel->getAssetId();
        properties[QStringLiteral("name")] = TransitionsRepository::get()->getName(assetId);
    }

    return QJsonObject{{QStringLiteral("result"), properties}};
}

auto TransitionHandler::handleCompositionSetProperty(const QJsonObject &params) -> QJsonObject
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

    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                             {QStringLiteral("message"), QStringLiteral("Unknown property: %1").arg(property)}}}};
}
