/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#pragma once

#include "../rpctypes.h"

#include <QObject>

#include <memory>

class EffectStackModel;

class RpcNotifier;

/**
 * @brief Handler for effect-related RPC methods
 *
 * Handles the effect.* method namespace:
 * - effect.listAvailable: List all available effects
 * - effect.getInfo: Get full effect metadata with parameters
 * - effect.add: Add an effect to a clip
 * - effect.remove: Remove an effect from a clip
 * - effect.getClipEffects: Get all effects on a clip
 * - effect.getProperty: Get an effect property value
 * - effect.setProperty: Set an effect property value
 * - effect.enable: Enable an effect
 * - effect.disable: Disable an effect
 * - effect.reorder: Reorder effects on a clip
 * - effect.copyToClips: Copy effect to multiple clips
 * - effect.getKeyframes: Get keyframes for an effect property
 * - effect.setKeyframe: Set a keyframe
 * - effect.deleteKeyframe: Delete a keyframe
 * - effect.deleteAllKeyframes: Delete all keyframes for a property
 */
class EffectsHandler : public QObject, public IRpcHandler
{
    Q_OBJECT

public:
    explicit EffectsHandler(RpcNotifier *notifier, QObject *parent = nullptr);
    ~EffectsHandler() override;

    QJsonObject handle(const QString &method, const QJsonObject &params) override;
    QStringList supportedMethods() const override;
    QString prefix() const override;

private:
    static QJsonObject handleListAvailable(const QJsonObject &params);
    static QJsonObject handleGetInfo(const QJsonObject &params);
    static QJsonObject handleAdd(const QJsonObject &params);
    static QJsonObject handleRemove(const QJsonObject &params);
    static QJsonObject handleGetClipEffects(const QJsonObject &params);
    static QJsonObject handleGetProperty(const QJsonObject &params);
    static QJsonObject handleSetProperty(const QJsonObject &params);
    static QJsonObject handleEnable(const QJsonObject &params);
    static QJsonObject handleDisable(const QJsonObject &params);
    static QJsonObject handleReorder(const QJsonObject &params);
    static QJsonObject handleCopyToClips(const QJsonObject &params);
    static QJsonObject handleGetKeyframes(const QJsonObject &params);
    static QJsonObject handleSetKeyframe(const QJsonObject &params);
    static QJsonObject handleDeleteKeyframe(const QJsonObject &params);
    static QJsonObject handleDeleteAllKeyframes(const QJsonObject &params);

    static QJsonObject makeProjectNotOpenError();
    static QJsonObject makeApplicationClosingError();
    static QJsonObject makeClipNotFoundError(int clipId);
    static QJsonObject makeEffectNotFoundError(const QString &effectId);
    static QJsonObject makeEffectIndexError(int effectIndex);

    /**
     * @brief Find effect index by its asset ID
     * @param effectStack The effect stack to search
     * @param effectId The effect asset ID (e.g., "brightness")
     * @return The effect index or -1 if not found
     */
    static int findEffectIndexById(const std::shared_ptr<EffectStackModel> &effectStack, const QString &effectId);

    /**
     * @brief Resolve effect index from params (accepts both effectId and effectIndex)
     * @param params The RPC params
     * @param effectStack The effect stack to search
     * @param errorOut Output error object if resolution fails
     * @return The resolved effect index or -1 on error
     */
    static int resolveEffectIndex(const QJsonObject &params, const std::shared_ptr<EffectStackModel> &effectStack, QJsonObject &errorOut);

    RpcNotifier *m_notifier;
};
