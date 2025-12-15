/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#include "assetshandler.h"
#include "../rpcnotifier.h"

#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "effects/effectsrepository.hpp"
#include "kdenlivesettings.h"
#include "transitions/transitionsrepository.hpp"

#include <QDir>
#include <QStandardPaths>

AssetsHandler::AssetsHandler(RpcNotifier *notifier, QObject *parent)
    : QObject(parent)
    , m_notifier(notifier)
{
}

AssetsHandler::~AssetsHandler() = default;

auto AssetsHandler::prefix() const -> QString
{
    return QStringLiteral("asset");
}

auto AssetsHandler::supportedMethods() const -> QStringList
{
    return QStringList{QStringLiteral("listCategories"), QStringLiteral("search"),      QStringLiteral("getEffectsByCategory"),
                       QStringLiteral("getFavorites"),   QStringLiteral("addFavorite"), QStringLiteral("removeFavorite"),
                       QStringLiteral("getPresets"),     QStringLiteral("savePreset"),  QStringLiteral("deletePreset")};
}

auto AssetsHandler::handle(const QString &method, const QJsonObject &params) -> QJsonObject
{
    if (method == QLatin1String("listCategories")) {
        return handleListCategories(params);
    }
    if (method == QLatin1String("search")) {
        return handleSearch(params);
    }
    if (method == QLatin1String("getEffectsByCategory")) {
        return handleGetEffectsByCategory(params);
    }
    if (method == QLatin1String("getFavorites")) {
        return handleGetFavorites(params);
    }
    if (method == QLatin1String("addFavorite")) {
        return handleAddFavorite(params);
    }
    if (method == QLatin1String("removeFavorite")) {
        return handleRemoveFavorite(params);
    }
    if (method == QLatin1String("getPresets")) {
        return handleGetPresets(params);
    }
    if (method == QLatin1String("savePreset")) {
        return handleSavePreset(params);
    }
    if (method == QLatin1String("deletePreset")) {
        return handleDeletePreset(params);
    }

    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::MethodNotFound},
                                                             {QStringLiteral("message"), QStringLiteral("Unknown method: asset.%1").arg(method)}}}};
}

auto AssetsHandler::handleListCategories(const QJsonObject & /*params*/) -> QJsonObject
{
    QJsonArray categories;

    // Effect categories based on asset types
    QJsonObject audioEffects;
    audioEffects[QStringLiteral("id")] = QStringLiteral("audio");
    audioEffects[QStringLiteral("name")] = QStringLiteral("Audio Effects");
    audioEffects[QStringLiteral("type")] = QStringLiteral("effect");
    categories.append(audioEffects);

    QJsonObject videoEffects;
    videoEffects[QStringLiteral("id")] = QStringLiteral("video");
    videoEffects[QStringLiteral("name")] = QStringLiteral("Video Effects");
    videoEffects[QStringLiteral("type")] = QStringLiteral("effect");
    categories.append(videoEffects);

    QJsonObject customEffects;
    customEffects[QStringLiteral("id")] = QStringLiteral("custom");
    customEffects[QStringLiteral("name")] = QStringLiteral("Custom Effects");
    customEffects[QStringLiteral("type")] = QStringLiteral("effect");
    categories.append(customEffects);

    QJsonObject favorites;
    favorites[QStringLiteral("id")] = QStringLiteral("favorites");
    favorites[QStringLiteral("name")] = QStringLiteral("Favorites");
    favorites[QStringLiteral("type")] = QStringLiteral("effect");
    categories.append(favorites);

    // Transition categories
    QJsonObject audioTransitions;
    audioTransitions[QStringLiteral("id")] = QStringLiteral("audioTransition");
    audioTransitions[QStringLiteral("name")] = QStringLiteral("Audio Transitions");
    audioTransitions[QStringLiteral("type")] = QStringLiteral("transition");
    categories.append(audioTransitions);

    QJsonObject videoTransitions;
    videoTransitions[QStringLiteral("id")] = QStringLiteral("videoTransition");
    videoTransitions[QStringLiteral("name")] = QStringLiteral("Video Transitions");
    videoTransitions[QStringLiteral("type")] = QStringLiteral("transition");
    categories.append(videoTransitions);

    QJsonObject compositions;
    compositions[QStringLiteral("id")] = QStringLiteral("composition");
    compositions[QStringLiteral("name")] = QStringLiteral("Compositions");
    compositions[QStringLiteral("type")] = QStringLiteral("transition");
    categories.append(compositions);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("categories"), categories}}}};
}

auto AssetsHandler::handleSearch(const QJsonObject &params) -> QJsonObject
{
    QString query = params.value(QStringLiteral("query")).toString().toLower();
    QString typeFilter = params.value(QStringLiteral("type")).toString();

    if (query.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'query' parameter")}}}};
    }

    QJsonArray results;

    // Search effects
    if (typeFilter.isEmpty() || typeFilter == QLatin1String("effect")) {
        auto effectNames = EffectsRepository::get()->getNames();
        for (const auto &effect : effectNames) {
            const QString &id = effect.first;
            const QString &name = effect.second;
            QString description = EffectsRepository::get()->getDescription(id);

            if (name.toLower().contains(query) || id.toLower().contains(query) || description.toLower().contains(query)) {
                QJsonObject effectObj;
                effectObj[QStringLiteral("id")] = id;
                effectObj[QStringLiteral("name")] = name;
                effectObj[QStringLiteral("description")] = description;
                effectObj[QStringLiteral("type")] = QStringLiteral("effect");
                results.append(effectObj);
            }
        }
    }

    // Search transitions
    if (typeFilter.isEmpty() || typeFilter == QLatin1String("transition")) {
        auto transitionNames = TransitionsRepository::get()->getNames();
        for (const auto &transition : transitionNames) {
            const QString &id = transition.first;
            const QString &name = transition.second;
            QString description = TransitionsRepository::get()->getDescription(id);

            if (name.toLower().contains(query) || id.toLower().contains(query) || description.toLower().contains(query)) {
                QJsonObject transObj;
                transObj[QStringLiteral("id")] = id;
                transObj[QStringLiteral("name")] = name;
                transObj[QStringLiteral("description")] = description;
                transObj[QStringLiteral("type")] = QStringLiteral("transition");
                results.append(transObj);
            }
        }
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("results"), results}}}};
}

auto AssetsHandler::handleGetEffectsByCategory(const QJsonObject &params) -> QJsonObject
{
    QString categoryId = params.value(QStringLiteral("categoryId")).toString();

    if (categoryId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'categoryId' parameter")}}}};
    }

    QJsonArray effects;
    auto effectNames = EffectsRepository::get()->getNames();

    for (const auto &effect : effectNames) {
        const QString &id = effect.first;
        const QString &name = effect.second;
        auto type = EffectsRepository::get()->getType(id);

        bool include = false;

        if (categoryId == QLatin1String("audio") && type == AssetListType::AssetType::Audio) {
            include = true;
        } else if (categoryId == QLatin1String("video") && type == AssetListType::AssetType::Video) {
            include = true;
        } else if (categoryId == QLatin1String("custom") && (type == AssetListType::AssetType::Custom || type == AssetListType::AssetType::CustomAudio)) {
            include = true;
        } else if (categoryId == QLatin1String("favorites")) {
            QStringList favs = KdenliveSettings::favorite_effects();
            if (favs.contains(id)) {
                include = true;
            }
        }

        if (include) {
            QJsonObject effectObj;
            effectObj[QStringLiteral("id")] = id;
            effectObj[QStringLiteral("name")] = name;
            effectObj[QStringLiteral("description")] = EffectsRepository::get()->getDescription(id);
            effects.append(effectObj);
        }
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("effects"), effects}}}};
}

auto AssetsHandler::handleGetFavorites(const QJsonObject & /*params*/) -> QJsonObject
{
    QJsonArray favorites;

    // Get favorite effects
    QStringList favEffects = KdenliveSettings::favorite_effects();
    for (const QString &id : favEffects) {
        if (EffectsRepository::get()->exists(id)) {
            QJsonObject effectObj;
            effectObj[QStringLiteral("id")] = id;
            effectObj[QStringLiteral("name")] = EffectsRepository::get()->getName(id);
            effectObj[QStringLiteral("type")] = QStringLiteral("effect");
            favorites.append(effectObj);
        }
    }

    // Get favorite transitions
    QStringList favTransitions = KdenliveSettings::favorite_transitions();
    for (const QString &id : favTransitions) {
        if (TransitionsRepository::get()->exists(id)) {
            QJsonObject transObj;
            transObj[QStringLiteral("id")] = id;
            transObj[QStringLiteral("name")] = TransitionsRepository::get()->getName(id);
            transObj[QStringLiteral("type")] = QStringLiteral("transition");
            favorites.append(transObj);
        }
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("favorites"), favorites}}}};
}

auto AssetsHandler::handleAddFavorite(const QJsonObject &params) -> QJsonObject
{
    QString assetId = params.value(QStringLiteral("assetId")).toString();
    QString type = params.value(QStringLiteral("type")).toString(QStringLiteral("effect"));

    if (assetId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'assetId' parameter")}}}};
    }

    if (type == QLatin1String("effect")) {
        if (!EffectsRepository::get()->exists(assetId)) {
            return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::EffectNotFound},
                                                                     {QStringLiteral("message"), QStringLiteral("Effect not found: %1").arg(assetId)}}}};
        }
        QStringList favs = KdenliveSettings::favorite_effects();
        if (!favs.contains(assetId)) {
            favs << assetId;
            KdenliveSettings::setFavorite_effects(favs);
        }
    } else if (type == QLatin1String("transition")) {
        if (!TransitionsRepository::get()->exists(assetId)) {
            return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::EffectNotFound},
                                                                     {QStringLiteral("message"), QStringLiteral("Transition not found: %1").arg(assetId)}}}};
        }
        QStringList favs = KdenliveSettings::favorite_transitions();
        if (!favs.contains(assetId)) {
            favs << assetId;
            KdenliveSettings::setFavorite_transitions(favs);
        }
    } else {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Invalid type: %1").arg(type)}}}};
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("added"), true}, {QStringLiteral("assetId"), assetId}}}};
}

auto AssetsHandler::handleRemoveFavorite(const QJsonObject &params) -> QJsonObject
{
    QString assetId = params.value(QStringLiteral("assetId")).toString();
    QString type = params.value(QStringLiteral("type")).toString(QStringLiteral("effect"));

    if (assetId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'assetId' parameter")}}}};
    }

    if (type == QLatin1String("effect")) {
        QStringList favs = KdenliveSettings::favorite_effects();
        favs.removeAll(assetId);
        KdenliveSettings::setFavorite_effects(favs);
    } else if (type == QLatin1String("transition")) {
        QStringList favs = KdenliveSettings::favorite_transitions();
        favs.removeAll(assetId);
        KdenliveSettings::setFavorite_transitions(favs);
    } else {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Invalid type: %1").arg(type)}}}};
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("removed"), true}, {QStringLiteral("assetId"), assetId}}}};
}

auto AssetsHandler::handleGetPresets(const QJsonObject &params) -> QJsonObject
{
    QString effectId = params.value(QStringLiteral("effectId")).toString();

    if (effectId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'effectId' parameter")}}}};
    }

    if (!EffectsRepository::get()->exists(effectId)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::EffectNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Effect not found: %1").arg(effectId)}}}};
    }

    // Get the presets directory
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/presets/"));
    QString presetFile = dir.absoluteFilePath(QStringLiteral("%1.json").arg(effectId));

    QJsonArray presets;

    QFile loadFile(presetFile);
    if (loadFile.open(QIODevice::ReadOnly)) {
        QByteArray data = loadFile.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            QJsonArray array = doc.array();
            for (const auto &preset : array) {
                QJsonObject presetObj = preset.toObject();
                QJsonObject resultPreset;
                resultPreset[QStringLiteral("name")] = presetObj.value(QStringLiteral("name"));
                resultPreset[QStringLiteral("params")] = presetObj.value(QStringLiteral("params"));
                presets.append(resultPreset);
            }
        }
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("presets"), presets}}}};
}

auto AssetsHandler::handleSavePreset(const QJsonObject &params) -> QJsonObject
{
    QString effectId = params.value(QStringLiteral("effectId")).toString();
    // Accept both 'name' and 'presetName' for compatibility
    QString name =
        params.contains(QStringLiteral("presetName")) ? params.value(QStringLiteral("presetName")).toString() : params.value(QStringLiteral("name")).toString();
    QJsonObject presetParams = params.value(QStringLiteral("params")).toObject();

    if (effectId.isEmpty() || name.isEmpty()) {
        return QJsonObject{
            {QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                  {QStringLiteral("message"), QStringLiteral("Missing 'effectId' or 'name'/'presetName' parameter")}}}};
    }

    if (!EffectsRepository::get()->exists(effectId)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::EffectNotFound},
                                                                 {QStringLiteral("message"), QStringLiteral("Effect not found: %1").arg(effectId)}}}};
    }

    // Get the presets directory
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/presets/"));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    QString presetFile = dir.absoluteFilePath(QStringLiteral("%1.json").arg(effectId));

    // Load existing presets
    QJsonArray array;
    QFile loadFile(presetFile);
    if (loadFile.open(QIODevice::ReadOnly)) {
        QByteArray data = loadFile.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            array = doc.array();
            // Remove existing preset with same name
            for (int i = 0; i < array.count(); i++) {
                if (array.at(i).toObject().value(QStringLiteral("name")).toString() == name) {
                    array.removeAt(i);
                    break;
                }
            }
        }
        loadFile.close();
    }

    // Add new preset
    QJsonObject newPreset;
    newPreset[QStringLiteral("name")] = name;
    newPreset[QStringLiteral("params")] = presetParams;
    array.append(newPreset);

    // Save file
    QFile saveFile(presetFile);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to save preset file")}}}};
    }

    saveFile.write(QJsonDocument(array).toJson());
    saveFile.close();

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("saved"), true}, {QStringLiteral("name"), name}}}};
}

auto AssetsHandler::handleDeletePreset(const QJsonObject &params) -> QJsonObject
{
    QString effectId = params.value(QStringLiteral("effectId")).toString();
    QString presetName = params.value(QStringLiteral("presetName")).toString();

    if (effectId.isEmpty() || presetName.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'effectId' or 'presetName' parameter")}}}};
    }

    // Get the presets directory
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/presets/"));
    QString presetFile = dir.absoluteFilePath(QStringLiteral("%1.json").arg(effectId));

    // Load existing presets
    QFile loadFile(presetFile);
    if (!loadFile.open(QIODevice::ReadOnly)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Preset file not found")}}}};
    }

    QByteArray data = loadFile.readAll();
    loadFile.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Invalid preset file format")}}}};
    }

    QJsonArray array = doc.array();
    bool found = false;
    for (int i = 0; i < array.count(); i++) {
        if (array.at(i).toObject().value(QStringLiteral("name")).toString() == presetName) {
            array.removeAt(i);
            found = true;
            break;
        }
    }

    if (!found) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Preset not found: %1").arg(presetName)}}}};
    }

    // Save file
    QFile saveFile(presetFile);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Failed to save preset file")}}}};
    }

    saveFile.write(QJsonDocument(array).toJson());
    saveFile.close();

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("deleted"), true}, {QStringLiteral("presetName"), presetName}}}};
}
