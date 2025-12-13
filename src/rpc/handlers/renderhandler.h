/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#pragma once

#include "../rpctypes.h"

#include <QObject>

class RpcNotifier;

/**
 * @brief Handler for render-related RPC methods
 *
 * Handles the render.* method namespace:
 * - render.getPresets: Get available render presets
 * - render.getPresetInfo: Get full preset details
 * - render.start: Start a render job
 * - render.startWithGuides: Start render jobs based on guide markers
 * - render.stop: Stop a render job
 * - render.stopAll: Stop all render jobs
 * - render.getStatus: Get render job status
 * - render.getJobs: Get all render jobs
 * - render.getActiveJob: Get currently active render job
 * - render.setOutput: Set default output path
 */
class RenderHandler : public QObject, public IRpcHandler
{
    Q_OBJECT

public:
    explicit RenderHandler(RpcNotifier *notifier, QObject *parent = nullptr);
    ~RenderHandler() override;

    QJsonObject handle(const QString &method, const QJsonObject &params) override;
    QStringList supportedMethods() const override;
    QString prefix() const override;

private:
    QJsonObject handleGetPresets(const QJsonObject &params);
    QJsonObject handleGetPresetInfo(const QJsonObject &params);
    QJsonObject handleStart(const QJsonObject &params);
    QJsonObject handleStartWithGuides(const QJsonObject &params);
    QJsonObject handleStop(const QJsonObject &params);
    QJsonObject handleStopAll(const QJsonObject &params);
    QJsonObject handleGetStatus(const QJsonObject &params);
    QJsonObject handleGetJobs(const QJsonObject &params);
    QJsonObject handleGetActiveJob(const QJsonObject &params);
    QJsonObject handleSetOutput(const QJsonObject &params);

    QJsonObject makeProjectNotOpenError();

    RpcNotifier *m_notifier;
};
