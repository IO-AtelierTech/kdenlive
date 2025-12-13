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
    static QJsonObject handleGetPresets(const QJsonObject &params);
    static QJsonObject handleGetPresetInfo(const QJsonObject &params);
    QJsonObject handleStart(const QJsonObject &params);
    QJsonObject handleStartWithGuides(const QJsonObject &params);
    static QJsonObject handleStop(const QJsonObject &params);
    static QJsonObject handleStopAll(const QJsonObject &params);
    static QJsonObject handleGetStatus(const QJsonObject &params);
    static QJsonObject handleGetJobs(const QJsonObject &params);
    static QJsonObject handleGetActiveJob(const QJsonObject &params);
    static QJsonObject handleSetOutput(const QJsonObject &params);

    static QJsonObject makeProjectNotOpenError();
    static QJsonObject makeApplicationClosingError();

    RpcNotifier *m_notifier;
};
