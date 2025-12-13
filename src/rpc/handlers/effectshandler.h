/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#pragma once

#include "../rpctypes.h"

#include <QObject>

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
    QJsonObject handleListAvailable(const QJsonObject &params);
    QJsonObject handleGetInfo(const QJsonObject &params);
    QJsonObject handleAdd(const QJsonObject &params);
    QJsonObject handleRemove(const QJsonObject &params);
    QJsonObject handleGetClipEffects(const QJsonObject &params);
    QJsonObject handleGetProperty(const QJsonObject &params);
    QJsonObject handleSetProperty(const QJsonObject &params);
    QJsonObject handleEnable(const QJsonObject &params);
    QJsonObject handleDisable(const QJsonObject &params);
    QJsonObject handleReorder(const QJsonObject &params);
    QJsonObject handleCopyToClips(const QJsonObject &params);
    QJsonObject handleGetKeyframes(const QJsonObject &params);
    QJsonObject handleSetKeyframe(const QJsonObject &params);
    QJsonObject handleDeleteKeyframe(const QJsonObject &params);
    QJsonObject handleDeleteAllKeyframes(const QJsonObject &params);

    QJsonObject makeProjectNotOpenError();
    QJsonObject makeClipNotFoundError(int clipId);
    QJsonObject makeEffectNotFoundError(const QString &effectId);
    QJsonObject makeEffectIndexError(int effectIndex);

    RpcNotifier *m_notifier;
};
