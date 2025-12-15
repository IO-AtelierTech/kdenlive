/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#include "effectshandler.h"
#include "../rpcnotifier.h"

#include "assets/keyframes/model/keyframemodellist.hpp"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "effects/effectsrepository.hpp"
#include "effects/effectstack/model/effectitemmodel.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "project/projectmanager.h"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"

#include <QDomDocument>

EffectsHandler::EffectsHandler(RpcNotifier *notifier, QObject *parent)
    : QObject(parent)
    , m_notifier(notifier)
{
}

EffectsHandler::~EffectsHandler() = default;

auto EffectsHandler::prefix() const -> QString
{
    return QStringLiteral("effect");
}

auto EffectsHandler::supportedMethods() const -> QStringList
{
    return QStringList{QStringLiteral("listAvailable"), QStringLiteral("getInfo"),        QStringLiteral("add"),
                       QStringLiteral("remove"),        QStringLiteral("getClipEffects"), QStringLiteral("getProperty"),
                       QStringLiteral("setProperty"),   QStringLiteral("enable"),         QStringLiteral("disable"),
                       QStringLiteral("reorder"),       QStringLiteral("copyToClips"),    QStringLiteral("getKeyframes"),
                       QStringLiteral("setKeyframe"),   QStringLiteral("deleteKeyframe"), QStringLiteral("deleteAllKeyframes")};
}

auto EffectsHandler::handle(const QString &method, const QJsonObject &params) -> QJsonObject
{
    if (method == QLatin1String("listAvailable")) {
        return handleListAvailable(params);
    }
    if (method == QLatin1String("getInfo")) {
        return handleGetInfo(params);
    }
    if (method == QLatin1String("add")) {
        return handleAdd(params);
    }
    if (method == QLatin1String("remove")) {
        return handleRemove(params);
    }
    if (method == QLatin1String("getClipEffects")) {
        return handleGetClipEffects(params);
    }
    if (method == QLatin1String("getProperty")) {
        return handleGetProperty(params);
    }
    if (method == QLatin1String("setProperty")) {
        return handleSetProperty(params);
    }
    if (method == QLatin1String("enable")) {
        return handleEnable(params);
    }
    if (method == QLatin1String("disable")) {
        return handleDisable(params);
    }
    if (method == QLatin1String("reorder")) {
        return handleReorder(params);
    }
    if (method == QLatin1String("copyToClips")) {
        return handleCopyToClips(params);
    }
    if (method == QLatin1String("getKeyframes")) {
        return handleGetKeyframes(params);
    }
    if (method == QLatin1String("setKeyframe")) {
        return handleSetKeyframe(params);
    }
    if (method == QLatin1String("deleteKeyframe")) {
        return handleDeleteKeyframe(params);
    }
    if (method == QLatin1String("deleteAllKeyframes")) {
        return handleDeleteAllKeyframes(params);
    }

    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::MethodNotFound},
                                                             {QStringLiteral("message"), QStringLiteral("Unknown method: effect.%1").arg(method)}}}};
}

auto EffectsHandler::makeProjectNotOpenError() -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ProjectNotOpen},
                                                             {QStringLiteral("message"), QStringLiteral("No project is currently open")}}}};
}

auto EffectsHandler::makeApplicationClosingError() -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ApplicationClosing},
                                                             {QStringLiteral("message"), QStringLiteral("Application is shutting down")}}}};
}

auto EffectsHandler::makeClipNotFoundError(int clipId) -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                             {QStringLiteral("message"), QStringLiteral("Clip not found: %1").arg(clipId)}}}};
}

auto EffectsHandler::makeEffectNotFoundError(const QString &effectId) -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::EffectNotFound},
                                                             {QStringLiteral("message"), QStringLiteral("Effect not found: %1").arg(effectId)}}}};
}

auto EffectsHandler::makeEffectIndexError(int effectIndex) -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::EffectNotFound},
                                                             {QStringLiteral("message"), QStringLiteral("Invalid effect index: %1").arg(effectIndex)}}}};
}

auto EffectsHandler::findEffectIndexById(const std::shared_ptr<EffectStackModel> &effectStack, const QString &effectId) -> int
{
    if (!effectStack) {
        return -1;
    }
    for (int i = 0; i < effectStack->rowCount(); i++) {
        auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectStack->getEffectStackRow(i));
        if (effect && effect->getAssetId() == effectId) {
            return i;
        }
    }
    return -1;
}

auto EffectsHandler::resolveEffectIndex(const QJsonObject &params, const std::shared_ptr<EffectStackModel> &effectStack, QJsonObject &errorOut) -> int
{
    // Check for effectIndex first (explicit index has priority)
    int effectIndex = params.value(QStringLiteral("effectIndex")).toInt(-1);

    // If not provided, try to find by effectId
    if (effectIndex < 0) {
        QString effectId = params.value(QStringLiteral("effectId")).toString();
        if (effectId.isEmpty()) {
            errorOut = QJsonObject{
                {QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                      {QStringLiteral("message"), QStringLiteral("Missing 'effectIndex' or 'effectId' parameter")}}}};
            return -1;
        }
        effectIndex = findEffectIndexById(effectStack, effectId);
        if (effectIndex < 0) {
            errorOut = makeEffectNotFoundError(effectId);
            return -1;
        }
    }

    // Validate index is in range
    if (effectIndex >= effectStack->rowCount()) {
        errorOut = makeEffectIndexError(effectIndex);
        return -1;
    }

    return effectIndex;
}

auto EffectsHandler::handleListAvailable(const QJsonObject & /*params*/) -> QJsonObject
{
    QJsonArray effects;
    auto effectNames = EffectsRepository::get()->getNames();

    for (const auto &effect : effectNames) {
        const QString &id = effect.first;
        const QString &name = effect.second;

        QJsonObject effectObj;
        effectObj[QStringLiteral("id")] = id;
        effectObj[QStringLiteral("name")] = name;
        effectObj[QStringLiteral("description")] = EffectsRepository::get()->getDescription(id);

        auto type = EffectsRepository::get()->getType(id);
        QString typeStr;
        switch (type) {
        case AssetListType::AssetType::Audio:
            typeStr = QStringLiteral("audio");
            break;
        case AssetListType::AssetType::Video:
            typeStr = QStringLiteral("video");
            break;
        case AssetListType::AssetType::Custom:
            typeStr = QStringLiteral("custom");
            break;
        case AssetListType::AssetType::CustomAudio:
            typeStr = QStringLiteral("customAudio");
            break;
        case AssetListType::AssetType::Favorites:
            typeStr = QStringLiteral("favorite");
            break;
        case AssetListType::AssetType::AudioComposition:
            typeStr = QStringLiteral("audioComposition");
            break;
        case AssetListType::AssetType::VideoShortComposition:
            typeStr = QStringLiteral("videoShortComposition");
            break;
        case AssetListType::AssetType::VideoComposition:
            typeStr = QStringLiteral("videoComposition");
            break;
        case AssetListType::AssetType::AudioTransition:
            typeStr = QStringLiteral("audioTransition");
            break;
        case AssetListType::AssetType::VideoTransition:
            typeStr = QStringLiteral("videoTransition");
            break;
        case AssetListType::AssetType::Text:
            typeStr = QStringLiteral("text");
            break;
        case AssetListType::AssetType::TemplateAudio:
            typeStr = QStringLiteral("templateAudio");
            break;
        case AssetListType::AssetType::Template:
            typeStr = QStringLiteral("template");
            break;
        case AssetListType::AssetType::TemplateCustom:
            typeStr = QStringLiteral("templateCustom");
            break;
        case AssetListType::AssetType::TemplateCustomAudio:
            typeStr = QStringLiteral("templateCustomAudio");
            break;
        case AssetListType::AssetType::Preferred:
            typeStr = QStringLiteral("preferred");
            break;
        default:
            typeStr = QStringLiteral("unknown");
        }
        effectObj[QStringLiteral("type")] = typeStr;

        effects.append(effectObj);
    }

    return QJsonObject{{QStringLiteral("result"), effects}};
}

auto EffectsHandler::handleGetInfo(const QJsonObject &params) -> QJsonObject
{
    QString effectId = params.value(QStringLiteral("effectId")).toString();
    if (effectId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'effectId' parameter")}}}};
    }

    if (!EffectsRepository::get()->exists(effectId)) {
        return makeEffectNotFoundError(effectId);
    }

    QJsonObject result;
    result[QStringLiteral("id")] = effectId;
    result[QStringLiteral("name")] = EffectsRepository::get()->getName(effectId);
    result[QStringLiteral("description")] = EffectsRepository::get()->getDescription(effectId);
    result[QStringLiteral("version")] = EffectsRepository::get()->getVersion(effectId);

    // Get the XML definition for parameters
    QDomElement xml = EffectsRepository::get()->getXml(effectId);
    QJsonArray parameters;

    QDomNodeList paramNodes = xml.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < paramNodes.count(); i++) {
        QDomElement paramEl = paramNodes.at(i).toElement();
        QJsonObject paramObj;
        paramObj[QStringLiteral("name")] = paramEl.attribute(QStringLiteral("name"));
        paramObj[QStringLiteral("type")] = paramEl.attribute(QStringLiteral("type"));
        paramObj[QStringLiteral("default")] = paramEl.attribute(QStringLiteral("default"));
        paramObj[QStringLiteral("min")] = paramEl.attribute(QStringLiteral("min"));
        paramObj[QStringLiteral("max")] = paramEl.attribute(QStringLiteral("max"));

        // Get comment/description from child element
        QDomElement commentEl = paramEl.firstChildElement(QStringLiteral("comment"));
        if (!commentEl.isNull()) {
            paramObj[QStringLiteral("comment")] = commentEl.text();
        }

        parameters.append(paramObj);
    }
    result[QStringLiteral("parameters")] = parameters;

    return QJsonObject{{QStringLiteral("result"), result}};
}

auto EffectsHandler::handleAdd(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    if (clipId < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing or invalid 'clipId' parameter")}}}};
    }

    QString effectId = params.value(QStringLiteral("effectId")).toString();
    if (effectId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'effectId' parameter")}}}};
    }

    if (!EffectsRepository::get()->exists(effectId)) {
        return makeEffectNotFoundError(effectId);
    }

    // Get the timeline model
    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    // Validate clip exists
    if (!timeline->isClip(clipId)) {
        return makeClipNotFoundError(clipId);
    }

    // Get the clip's effect stack
    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    // Get optional parameters
    QJsonObject effectParams = params.value(QStringLiteral("params")).toObject();
    stringMap paramsMap;
    for (auto it = effectParams.begin(); it != effectParams.end(); ++it) {
        paramsMap[it.key()] = it.value().toString();
    }

    // Add the effect
    bool success = effectStack->appendEffect(effectId, true, paramsMap);
    if (!success) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to add effect")}}}};
    }

    // Return the effect index (row count - 1)
    int effectIndex = effectStack->rowCount() - 1;

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("effectIndex"), effectIndex}}}};
}

auto EffectsHandler::handleRemove(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    if (clipId < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing or invalid 'clipId' parameter")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    if (!timeline->isClip(clipId)) {
        return makeClipNotFoundError(clipId);
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    // Resolve effect index from effectId or effectIndex parameter
    QJsonObject error;
    int effectIndex = resolveEffectIndex(params, effectStack, error);
    if (effectIndex < 0) {
        return error;
    }

    auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectStack->getEffectStackRow(effectIndex));
    if (!effect) {
        return makeEffectIndexError(effectIndex);
    }

    effectStack->removeEffect(effect);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("deleted"), true}}}};
}

auto EffectsHandler::handleGetClipEffects(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    if (clipId < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing or invalid 'clipId' parameter")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    if (!timeline->isClip(clipId)) {
        return makeClipNotFoundError(clipId);
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    QJsonArray effects;
    for (int i = 0; i < effectStack->rowCount(); i++) {
        auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectStack->getEffectStackRow(i));
        if (effect) {
            QJsonObject effectObj;
            effectObj[QStringLiteral("index")] = i;
            effectObj[QStringLiteral("id")] = effect->getAssetId();
            effectObj[QStringLiteral("name")] = EffectsRepository::get()->getName(effect->getAssetId());
            effectObj[QStringLiteral("enabled")] = effect->isAssetEnabled();
            effects.append(effectObj);
        }
    }

    return QJsonObject{{QStringLiteral("result"), effects}};
}

auto EffectsHandler::handleGetProperty(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    QString property = params.value(QStringLiteral("property")).toString();

    if (clipId < 0 || property.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing required parameters")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    if (!timeline->isClip(clipId)) {
        return makeClipNotFoundError(clipId);
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    // Resolve effect index from effectId or effectIndex parameter
    QJsonObject error;
    int effectIndex = resolveEffectIndex(params, effectStack, error);
    if (effectIndex < 0) {
        return error;
    }

    auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectStack->getEffectStackRow(effectIndex));
    if (!effect) {
        return makeEffectIndexError(effectIndex);
    }

    QString value = effect->getParam(property);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("value"), value}}}};
}

auto EffectsHandler::handleSetProperty(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    QString property = params.value(QStringLiteral("property")).toString();
    QString value = params.value(QStringLiteral("value")).toString();

    if (clipId < 0 || property.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing required parameters")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    if (!timeline->isClip(clipId)) {
        return makeClipNotFoundError(clipId);
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    // Resolve effect index from effectId or effectIndex parameter
    QJsonObject error;
    int effectIndex = resolveEffectIndex(params, effectStack, error);
    if (effectIndex < 0) {
        return error;
    }

    auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectStack->getEffectStackRow(effectIndex));
    if (!effect) {
        return makeEffectIndexError(effectIndex);
    }

    effect->setParameter(property, value);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("updated"), true}}}};
}

auto EffectsHandler::handleEnable(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    if (clipId < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'clipId' parameter")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    if (!timeline->isClip(clipId)) {
        return makeClipNotFoundError(clipId);
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    // Resolve effect index from effectId or effectIndex parameter
    QJsonObject error;
    int effectIndex = resolveEffectIndex(params, effectStack, error);
    if (effectIndex < 0) {
        return error;
    }

    auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectStack->getEffectStackRow(effectIndex));
    if (!effect) {
        return makeEffectIndexError(effectIndex);
    }

    effect->setAssetEnabled(true);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("enabled"), true}}}};
}

auto EffectsHandler::handleDisable(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    if (clipId < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'clipId' parameter")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    if (!timeline->isClip(clipId)) {
        return makeClipNotFoundError(clipId);
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    // Resolve effect index from effectId or effectIndex parameter
    QJsonObject error;
    int effectIndex = resolveEffectIndex(params, effectStack, error);
    if (effectIndex < 0) {
        return error;
    }

    auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectStack->getEffectStackRow(effectIndex));
    if (!effect) {
        return makeEffectIndexError(effectIndex);
    }

    effect->setAssetEnabled(false);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("enabled"), false}}}};
}

auto EffectsHandler::handleReorder(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    if (clipId < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'clipId' parameter")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    if (!timeline->isClip(clipId)) {
        return makeClipNotFoundError(clipId);
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    // Support two calling conventions:
    // 1. New: effectId/effectIndex + newIndex (client sends this)
    // 2. Legacy: fromIndex + toIndex
    int fromIndex = -1;
    int toIndex = -1;

    // Check for newIndex (new convention)
    int newIndex = params.value(QStringLiteral("newIndex")).toInt(-1);
    if (newIndex >= 0) {
        // Resolve source effect index from effectId or effectIndex
        QJsonObject error;
        fromIndex = resolveEffectIndex(params, effectStack, error);
        if (fromIndex < 0) {
            return error;
        }
        toIndex = newIndex;
    } else {
        // Try legacy fromIndex + toIndex
        fromIndex = params.value(QStringLiteral("fromIndex")).toInt(-1);
        toIndex = params.value(QStringLiteral("toIndex")).toInt(-1);

        if (fromIndex < 0 || toIndex < 0) {
            return QJsonObject{{QStringLiteral("error"),
                                QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                            {QStringLiteral("message"),
                                             QStringLiteral("Missing parameters: need (effectId/effectIndex + newIndex) or (fromIndex + toIndex)")}}}};
        }
    }

    if (fromIndex >= effectStack->rowCount() || toIndex >= effectStack->rowCount()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Invalid effect index")}}}};
    }

    auto effect = effectStack->getEffectStackRow(fromIndex);
    if (!effect) {
        return makeEffectIndexError(fromIndex);
    }

    effectStack->moveEffect(toIndex, effect);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("reordered"), true}}}};
}

auto EffectsHandler::handleCopyToClips(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    int sourceClipId = params.value(QStringLiteral("sourceClipId")).toInt(-1);
    QJsonArray targetClipIds = params.value(QStringLiteral("targetClipIds")).toArray();

    if (sourceClipId < 0 || targetClipIds.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing required parameters")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    if (!timeline->isClip(sourceClipId)) {
        return makeClipNotFoundError(sourceClipId);
    }

    auto sourceStack = timeline->getClipEffectStack(sourceClipId);
    if (!sourceStack) {
        return makeClipNotFoundError(sourceClipId);
    }

    // Resolve which effect to copy (effectId or effectIndex)
    QJsonObject error;
    int effectIndex = resolveEffectIndex(params, sourceStack, error);
    if (effectIndex < 0) {
        return error;
    }

    auto sourceEffect = std::dynamic_pointer_cast<EffectItemModel>(sourceStack->getEffectStackRow(effectIndex));
    if (!sourceEffect) {
        return makeEffectIndexError(effectIndex);
    }

    QString effectId = sourceEffect->getAssetId();

    int copiedCount = 0;
    for (const auto &targetIdVal : targetClipIds) {
        int targetId = targetIdVal.toInt(-1);
        if (targetId < 0 || !timeline->isClip(targetId)) {
            continue;
        }

        auto targetStack = timeline->getClipEffectStack(targetId);
        if (!targetStack) {
            continue;
        }

        // Copy the effect by adding it with its parameters
        if (targetStack->copyEffect(sourceEffect, PlaylistState::Disabled, false)) {
            copiedCount++;
        }
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("count"), copiedCount}}}};
}

auto EffectsHandler::handleGetKeyframes(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    QString property = params.value(QStringLiteral("property")).toString();

    if (clipId < 0 || property.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing required parameters")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    if (!timeline->isClip(clipId)) {
        return makeClipNotFoundError(clipId);
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    // Resolve effect index from effectId or effectIndex parameter
    QJsonObject error;
    int effectIndex = resolveEffectIndex(params, effectStack, error);
    if (effectIndex < 0) {
        return error;
    }

    auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectStack->getEffectStackRow(effectIndex));
    if (!effect) {
        return makeEffectIndexError(effectIndex);
    }

    auto keyframes = effect->getKeyframeModel();
    if (!keyframes) {
        return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("keyframes"), QJsonArray()}}}};
    }

    QJsonArray keyframeArray;
    // Note: getKeyModel() returns the first keyframeable parameter's model
    // The 'property' parameter is kept for API consistency but all keyframes are returned
    Q_UNUSED(property)
    auto *kfModel = keyframes->getKeyModel();
    if (kfModel) {
        for (auto &it : *kfModel) {
            QJsonObject kfObj;
            // Client expects "position", not "frame"
            kfObj[QStringLiteral("position")] = it.first.frames(pCore->getCurrentFps());
            kfObj[QStringLiteral("value")] = it.second.second.toString();

            QString typeStr;
            switch (it.second.first) {
            case KeyframeType::Linear:
                typeStr = QStringLiteral("linear");
                break;
            case KeyframeType::Discrete:
                typeStr = QStringLiteral("discrete");
                break;
            case KeyframeType::Curve:
            case KeyframeType::CurveSmooth:
                typeStr = QStringLiteral("smooth");
                break;
            default:
                typeStr = QStringLiteral("linear");
            }
            kfObj[QStringLiteral("type")] = typeStr;
            keyframeArray.append(kfObj);
        }
    }

    // Client expects {"keyframes": [...]} wrapper
    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("keyframes"), keyframeArray}}}};
}

auto EffectsHandler::handleSetKeyframe(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    QString property = params.value(QStringLiteral("property")).toString();
    // Accept both "position" (client sends this) and "frame" (legacy)
    int frame = params.value(QStringLiteral("position")).toInt(-1);
    if (frame < 0) {
        frame = params.value(QStringLiteral("frame")).toInt(-1);
    }
    QString value = params.value(QStringLiteral("value")).toString();
    QString typeStr = params.value(QStringLiteral("type")).toString(QStringLiteral("linear"));

    if (clipId < 0 || property.isEmpty() || frame < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing required parameters")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    if (!timeline->isClip(clipId)) {
        return makeClipNotFoundError(clipId);
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    // Resolve effect index from effectId or effectIndex parameter
    QJsonObject error;
    int effectIndex = resolveEffectIndex(params, effectStack, error);
    if (effectIndex < 0) {
        return error;
    }

    auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectStack->getEffectStackRow(effectIndex));
    if (!effect) {
        return makeEffectIndexError(effectIndex);
    }

    auto keyframes = effect->getKeyframeModel();
    if (!keyframes) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Effect does not support keyframes")}}}};
    }

    KeyframeType::KeyframeEnum kfType = KeyframeType::Linear;
    if (typeStr == QLatin1String("discrete")) {
        kfType = KeyframeType::Discrete;
    } else if (typeStr == QLatin1String("smooth") || typeStr == QLatin1String("curve")) {
        kfType = KeyframeType::Curve;
    }

    Q_UNUSED(property)
    GenTime pos(frame, pCore->getCurrentFps());
    bool success = keyframes->addKeyframe(pos, kfType);

    if (success && !value.isEmpty()) {
        // Update keyframe value using the move-with-value signature (oldPos, newPos, value, logUndo)
        keyframes->updateKeyframe(pos, pos, QVariant(value), false);
    }

    // Client expects "set" not "success"
    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("set"), success}}}};
}

auto EffectsHandler::handleDeleteKeyframe(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    QString property = params.value(QStringLiteral("property")).toString();
    // Accept both "position" (client sends this) and "frame" (legacy)
    int frame = params.value(QStringLiteral("position")).toInt(-1);
    if (frame < 0) {
        frame = params.value(QStringLiteral("frame")).toInt(-1);
    }

    if (clipId < 0 || property.isEmpty() || frame < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing required parameters")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    if (!timeline->isClip(clipId)) {
        return makeClipNotFoundError(clipId);
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    // Resolve effect index from effectId or effectIndex parameter
    QJsonObject error;
    int effectIndex = resolveEffectIndex(params, effectStack, error);
    if (effectIndex < 0) {
        return error;
    }

    auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectStack->getEffectStackRow(effectIndex));
    if (!effect) {
        return makeEffectIndexError(effectIndex);
    }

    auto keyframes = effect->getKeyframeModel();
    if (!keyframes) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Effect does not support keyframes")}}}};
    }

    GenTime pos(frame, pCore->getCurrentFps());
    bool success = keyframes->removeKeyframe(pos);

    // Client expects "deleted" not "success"
    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("deleted"), success}}}};
}

auto EffectsHandler::handleDeleteAllKeyframes(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    QString property = params.value(QStringLiteral("property")).toString();

    if (clipId < 0 || property.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing required parameters")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    if (!timeline->isClip(clipId)) {
        return makeClipNotFoundError(clipId);
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    // Resolve effect index from effectId or effectIndex parameter
    QJsonObject error;
    int effectIndex = resolveEffectIndex(params, effectStack, error);
    if (effectIndex < 0) {
        return error;
    }

    auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectStack->getEffectStackRow(effectIndex));
    if (!effect) {
        return makeEffectIndexError(effectIndex);
    }

    auto keyframes = effect->getKeyframeModel();
    if (!keyframes) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Effect does not support keyframes")}}}};
    }

    bool success = keyframes->removeAllKeyframes();

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("deleted"), success}}}};
}
