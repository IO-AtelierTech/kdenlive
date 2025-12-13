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
#include "timeline2/model/timelinemodel.hpp"

#include <QDomDocument>

EffectsHandler::EffectsHandler(RpcNotifier *notifier, QObject *parent)
    : QObject(parent)
    , m_notifier(notifier)
{
}

EffectsHandler::~EffectsHandler() = default;

QString EffectsHandler::prefix() const
{
    return QStringLiteral("effect");
}

QStringList EffectsHandler::supportedMethods() const
{
    return QStringList{QStringLiteral("listAvailable"), QStringLiteral("getInfo"),        QStringLiteral("add"),
                       QStringLiteral("remove"),        QStringLiteral("getClipEffects"), QStringLiteral("getProperty"),
                       QStringLiteral("setProperty"),   QStringLiteral("enable"),         QStringLiteral("disable"),
                       QStringLiteral("reorder"),       QStringLiteral("copyToClips"),    QStringLiteral("getKeyframes"),
                       QStringLiteral("setKeyframe"),   QStringLiteral("deleteKeyframe"), QStringLiteral("deleteAllKeyframes")};
}

QJsonObject EffectsHandler::handle(const QString &method, const QJsonObject &params)
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

QJsonObject EffectsHandler::makeProjectNotOpenError()
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ProjectNotOpen},
                                                             {QStringLiteral("message"), QStringLiteral("No project is currently open")}}}};
}

QJsonObject EffectsHandler::makeClipNotFoundError(int clipId)
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ClipNotFound},
                                                             {QStringLiteral("message"), QStringLiteral("Clip not found: %1").arg(clipId)}}}};
}

QJsonObject EffectsHandler::makeEffectNotFoundError(const QString &effectId)
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::EffectNotFound},
                                                             {QStringLiteral("message"), QStringLiteral("Effect not found: %1").arg(effectId)}}}};
}

QJsonObject EffectsHandler::makeEffectIndexError(int effectIndex)
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::EffectNotFound},
                                                             {QStringLiteral("message"), QStringLiteral("Invalid effect index: %1").arg(effectIndex)}}}};
}

QJsonObject EffectsHandler::handleListAvailable(const QJsonObject & /*params*/)
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
        case AssetListType::AssetType::TemplateVideo:
            typeStr = QStringLiteral("templateVideo");
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

QJsonObject EffectsHandler::handleGetInfo(const QJsonObject &params)
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

QJsonObject EffectsHandler::handleAdd(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc) {
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

QJsonObject EffectsHandler::handleRemove(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    if (clipId < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing or invalid 'clipId' parameter")}}}};
    }

    int effectIndex = params.value(QStringLiteral("effectIndex")).toInt(-1);
    if (effectIndex < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing or invalid 'effectIndex' parameter")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    if (effectIndex >= effectStack->rowCount()) {
        return makeEffectIndexError(effectIndex);
    }

    auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectStack->getEffectStackRow(effectIndex));
    if (!effect) {
        return makeEffectIndexError(effectIndex);
    }

    effectStack->removeEffect(effect);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("removed"), true}}}};
}

QJsonObject EffectsHandler::handleGetClipEffects(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc) {
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
            effectObj[QStringLiteral("enabled")] = effect->isEnabled();
            effects.append(effectObj);
        }
    }

    return QJsonObject{{QStringLiteral("result"), effects}};
}

QJsonObject EffectsHandler::handleGetProperty(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    int effectIndex = params.value(QStringLiteral("effectIndex")).toInt(-1);
    QString property = params.value(QStringLiteral("property")).toString();

    if (clipId < 0 || effectIndex < 0 || property.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing required parameters")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    if (effectIndex >= effectStack->rowCount()) {
        return makeEffectIndexError(effectIndex);
    }

    auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectStack->getEffectStackRow(effectIndex));
    if (!effect) {
        return makeEffectIndexError(effectIndex);
    }

    QString value = effect->getParam(property);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("property"), property}, {QStringLiteral("value"), value}}}};
}

QJsonObject EffectsHandler::handleSetProperty(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    int effectIndex = params.value(QStringLiteral("effectIndex")).toInt(-1);
    QString property = params.value(QStringLiteral("property")).toString();
    QString value = params.value(QStringLiteral("value")).toString();

    if (clipId < 0 || effectIndex < 0 || property.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing required parameters")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    if (effectIndex >= effectStack->rowCount()) {
        return makeEffectIndexError(effectIndex);
    }

    auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectStack->getEffectStackRow(effectIndex));
    if (!effect) {
        return makeEffectIndexError(effectIndex);
    }

    effect->setParameter(property, value);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("property"), property}, {QStringLiteral("value"), value}}}};
}

QJsonObject EffectsHandler::handleEnable(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    int effectIndex = params.value(QStringLiteral("effectIndex")).toInt(-1);

    if (clipId < 0 || effectIndex < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing required parameters")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    if (effectIndex >= effectStack->rowCount()) {
        return makeEffectIndexError(effectIndex);
    }

    auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectStack->getEffectStackRow(effectIndex));
    if (!effect) {
        return makeEffectIndexError(effectIndex);
    }

    effect->setEnabled(true);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("enabled"), true}}}};
}

QJsonObject EffectsHandler::handleDisable(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    int effectIndex = params.value(QStringLiteral("effectIndex")).toInt(-1);

    if (clipId < 0 || effectIndex < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing required parameters")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    if (effectIndex >= effectStack->rowCount()) {
        return makeEffectIndexError(effectIndex);
    }

    auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectStack->getEffectStackRow(effectIndex));
    if (!effect) {
        return makeEffectIndexError(effectIndex);
    }

    effect->setEnabled(false);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("enabled"), false}}}};
}

QJsonObject EffectsHandler::handleReorder(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    int fromIndex = params.value(QStringLiteral("fromIndex")).toInt(-1);
    int toIndex = params.value(QStringLiteral("toIndex")).toInt(-1);

    if (clipId < 0 || fromIndex < 0 || toIndex < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing required parameters")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
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

    return QJsonObject{{QStringLiteral("result"),
                        QJsonObject{{QStringLiteral("moved"), true}, {QStringLiteral("fromIndex"), fromIndex}, {QStringLiteral("toIndex"), toIndex}}}};
}

QJsonObject EffectsHandler::handleCopyToClips(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc) {
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

    auto sourceStack = timeline->getClipEffectStack(sourceClipId);
    if (!sourceStack) {
        return makeClipNotFoundError(sourceClipId);
    }

    int copiedCount = 0;
    for (const auto &targetIdVal : targetClipIds) {
        int targetId = targetIdVal.toInt(-1);
        if (targetId < 0) continue;

        auto targetStack = timeline->getClipEffectStack(targetId);
        if (!targetStack) continue;

        if (targetStack->importEffects(sourceStack, PlaylistState::Disabled)) {
            copiedCount++;
        }
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("copiedToClips"), copiedCount}}}};
}

QJsonObject EffectsHandler::handleGetKeyframes(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    int effectIndex = params.value(QStringLiteral("effectIndex")).toInt(-1);
    QString property = params.value(QStringLiteral("property")).toString();

    if (clipId < 0 || effectIndex < 0 || property.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing required parameters")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    if (effectIndex >= effectStack->rowCount()) {
        return makeEffectIndexError(effectIndex);
    }

    auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectStack->getEffectStackRow(effectIndex));
    if (!effect) {
        return makeEffectIndexError(effectIndex);
    }

    auto keyframes = effect->getKeyframeModel();
    if (!keyframes) {
        return QJsonObject{{QStringLiteral("result"), QJsonArray()}};
    }

    QJsonArray keyframeArray;
    auto kfModel = keyframes->getKeyModel(property);
    if (kfModel) {
        for (auto it = kfModel->begin(); it != kfModel->end(); ++it) {
            QJsonObject kfObj;
            kfObj[QStringLiteral("frame")] = it->first.frames(doc->fps());
            kfObj[QStringLiteral("value")] = it->second.second.toString();

            QString typeStr;
            switch (it->second.first) {
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

    return QJsonObject{{QStringLiteral("result"), keyframeArray}};
}

QJsonObject EffectsHandler::handleSetKeyframe(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    int effectIndex = params.value(QStringLiteral("effectIndex")).toInt(-1);
    QString property = params.value(QStringLiteral("property")).toString();
    int frame = params.value(QStringLiteral("frame")).toInt(-1);
    QString value = params.value(QStringLiteral("value")).toString();
    QString typeStr = params.value(QStringLiteral("type")).toString(QStringLiteral("linear"));

    if (clipId < 0 || effectIndex < 0 || property.isEmpty() || frame < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing required parameters")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    if (effectIndex >= effectStack->rowCount()) {
        return makeEffectIndexError(effectIndex);
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

    GenTime pos(frame, doc->fps());
    bool success = keyframes->addKeyframe(pos, kfType);

    if (success) {
        keyframes->updateKeyframe(pos, QVariant(value));
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("success"), success}, {QStringLiteral("frame"), frame}}}};
}

QJsonObject EffectsHandler::handleDeleteKeyframe(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    int effectIndex = params.value(QStringLiteral("effectIndex")).toInt(-1);
    QString property = params.value(QStringLiteral("property")).toString();
    int frame = params.value(QStringLiteral("frame")).toInt(-1);

    if (clipId < 0 || effectIndex < 0 || property.isEmpty() || frame < 0) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing required parameters")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    if (effectIndex >= effectStack->rowCount()) {
        return makeEffectIndexError(effectIndex);
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

    GenTime pos(frame, doc->fps());
    bool success = keyframes->removeKeyframe(pos);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("success"), success}, {QStringLiteral("frame"), frame}}}};
}

QJsonObject EffectsHandler::handleDeleteAllKeyframes(const QJsonObject &params)
{
    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc) {
        return makeProjectNotOpenError();
    }

    int clipId = params.value(QStringLiteral("clipId")).toInt(-1);
    int effectIndex = params.value(QStringLiteral("effectIndex")).toInt(-1);
    QString property = params.value(QStringLiteral("property")).toString();

    if (clipId < 0 || effectIndex < 0 || property.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing required parameters")}}}};
    }

    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    auto effectStack = timeline->getClipEffectStack(clipId);
    if (!effectStack) {
        return makeClipNotFoundError(clipId);
    }

    if (effectIndex >= effectStack->rowCount()) {
        return makeEffectIndexError(effectIndex);
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

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("success"), success}}}};
}
