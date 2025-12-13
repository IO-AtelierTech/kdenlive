/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#pragma once

#include "../rpctypes.h"

#include <QObject>

class RpcNotifier;
class RpcDispatcher;

/**
 * @brief Handler for RPC connection/system methods
 *
 * Handles the rpc.* method namespace:
 * - rpc.ping: Health check
 * - rpc.getVersion: Get Kdenlive and RPC versions
 * - rpc.getCapabilities: List all available methods
 * - rpc.subscribe: Subscribe to notifications
 * - rpc.unsubscribe: Unsubscribe from notifications
 */
class ConnectionHandler : public QObject, public IRpcHandler
{
    Q_OBJECT

public:
    explicit ConnectionHandler(RpcNotifier *notifier, RpcDispatcher *dispatcher, QObject *parent = nullptr);
    ~ConnectionHandler() override;

    QJsonObject handle(const QString &method, const QJsonObject &params) override;
    QStringList supportedMethods() const override;
    QString prefix() const override;

private:
    static QJsonObject handlePing(const QJsonObject &params);
    static QJsonObject handleGetVersion(const QJsonObject &params);
    QJsonObject handleGetCapabilities(const QJsonObject &params);
    QJsonObject handleSubscribe(const QJsonObject &params);
    QJsonObject handleUnsubscribe(const QJsonObject &params);

    RpcNotifier *m_notifier;
    RpcDispatcher *m_dispatcher;
};
