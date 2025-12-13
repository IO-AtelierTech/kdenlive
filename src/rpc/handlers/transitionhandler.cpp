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
#include "timeline2/model/compositionmodel.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"
#include "timeline2/view/timelinecontroller.h"
#include "timeline2/view/timelinewidget.h"
#include "transitions/transitionsrepository.hpp"

#include <QJsonArray>

TransitionHandler::TransitionHandler(RpcNotifier *notifier, QObject *parent)
    : QObject(parent)
    , m_notifier(notifier)
    , m_isTransitionPrefix(true)
{
}

TransitionHandler::~TransitionHandler() = default;

QString TransitionHandler::prefix() const
{
    // This handler handles both transition.* and composition.* - we return transition as the main prefix
    // The dispatcher will also register us for composition.* methods
    return QStringLiteral("transition");
}

QStringList TransitionHandler::supportedMethods() const
{
    return QStringList{// Transition methods
                       QStringLiteral("list"), QStringLiteral("add"), QStringLiteral("remove"), QStringLiteral("getProperties"), QStringLiteral("setProperty")};
}

QJsonObject TransitionHandler::handle(const QString &method, const QJsonObject &params)
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

QJsonObject TransitionHandler::makeProjectNotOpenError()
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ProjectNotOpen},
                                                             {QStringLiteral("message"), QStringLiteral("No project is currently open")}}}};
}

QJsonObject TransitionHandler::makeNoTimelineError()
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                             {QStringLiteral("message"), QStringLiteral("No timeline is currently active")}}}};
}

QJsonObject TransitionHandler::handleTransitionList(const QJsonObject & /*params*/)
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

QJsonObject TransitionHandler::handleTransitionAdd(const QJsonObject &params)
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

QJsonObject TransitionHandler::handleTransitionRemove(const QJsonObject &params)
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

QJsonObject TransitionHandler::handleTransitionGetProperties(const QJsonObject &params)
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

QJsonObject TransitionHandler::handleTransitionSetProperty(const QJsonObject &params)
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

QJsonObject TransitionHandler::handleCompositionList(const QJsonObject & /*params*/)
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

    QJsonArray compositions;

    // Iterate through all compositions in the model
    for (auto it = model->m_allCompositions.begin(); it != model->m_allCompositions.end(); ++it) {
        int compoId = it->first;
        auto compo = it->second;

        QJsonObject compoInfo;
        compoInfo[QStringLiteral("id")] = compoId;
        compoInfo[QStringLiteral("trackId")] = model->getCompositionTrackId(compoId);
        compoInfo[QStringLiteral("position")] = model->getCompositionPosition(compoId);
        compoInfo[QStringLiteral("duration")] = model->getCompositionPlaytime(compoId);
        compoInfo[QStringLiteral("aTrack")] = compo->getATrack();
        compoInfo[QStringLiteral("name")] = compo->displayName();

        compositions.append(compoInfo);
    }

    return QJsonObject{{QStringLiteral("result"), compositions}};
}

QJsonObject TransitionHandler::handleCompositionAdd(const QJsonObject &params)
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

    int position = params.value(QStringLiteral("position")).toInt(-1);
    if (position < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'position' parameter")}}}};
    }

    QString compositionId = params.value(QStringLiteral("compositionId")).toString(QStringLiteral("composite"));
    int duration = params.value(QStringLiteral("duration")).toInt(doc->fps()); // Default to 1 second

    int newCompoId = controller->insertComposition(trackId, position, compositionId, true);

    if (newCompoId < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to add composition")}}}};
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("compositionId"), newCompoId}}}};
}

QJsonObject TransitionHandler::handleCompositionRemove(const QJsonObject &params)
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

QJsonObject TransitionHandler::handleCompositionGetProperties(const QJsonObject &params)
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

    // Get composition pointer for additional info
    auto compo = model->getCompositionPtr(compositionId);
    if (compo) {
        properties[QStringLiteral("aTrack")] = compo->getATrack();
        properties[QStringLiteral("forcedTrack")] = compo->getForcedTrack();
        properties[QStringLiteral("name")] = compo->displayName();
    }

    return QJsonObject{{QStringLiteral("result"), properties}};
}

QJsonObject TransitionHandler::handleCompositionSetProperty(const QJsonObject &params)
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
        auto trackInfo = controller->getCompositionATrack(compositionId);
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
