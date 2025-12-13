/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#pragma once

#include "../rpctypes.h"

#include <QObject>

class RpcNotifier;

/**
 * @brief Handler for asset-related RPC methods
 *
 * Handles the asset.* method namespace:
 * - asset.listCategories: List effect/transition categories
 * - asset.search: Search for effects/transitions
 * - asset.getEffectsByCategory: Get effects in a category
 * - asset.getFavorites: Get favorite assets
 * - asset.addFavorite: Add an asset to favorites
 * - asset.removeFavorite: Remove an asset from favorites
 * - asset.getPresets: Get presets for an effect
 * - asset.savePreset: Save a preset
 * - asset.deletePreset: Delete a preset
 */
class AssetsHandler : public QObject, public IRpcHandler
{
    Q_OBJECT

public:
    explicit AssetsHandler(RpcNotifier *notifier, QObject *parent = nullptr);
    ~AssetsHandler() override;

    QJsonObject handle(const QString &method, const QJsonObject &params) override;
    QStringList supportedMethods() const override;
    QString prefix() const override;

private:
    QJsonObject handleListCategories(const QJsonObject &params);
    QJsonObject handleSearch(const QJsonObject &params);
    QJsonObject handleGetEffectsByCategory(const QJsonObject &params);
    QJsonObject handleGetFavorites(const QJsonObject &params);
    QJsonObject handleAddFavorite(const QJsonObject &params);
    QJsonObject handleRemoveFavorite(const QJsonObject &params);
    QJsonObject handleGetPresets(const QJsonObject &params);
    QJsonObject handleSavePreset(const QJsonObject &params);
    QJsonObject handleDeletePreset(const QJsonObject &params);

    RpcNotifier *m_notifier;
};
