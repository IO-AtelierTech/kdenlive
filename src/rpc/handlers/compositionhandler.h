/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#pragma once

#include "../rpctypes.h"

#include <QObject>

class RpcNotifier;

/**
 * @brief Handler for composition RPC methods
 *
 * Handles the composition.* method namespace:
 * - composition.list: List compositions on timeline
 * - composition.add: Add a composition
 * - composition.remove: Remove a composition
 * - composition.getProperties: Get composition properties
 * - composition.setProperty: Set a composition property
 */
class CompositionHandler : public QObject, public IRpcHandler
{
    Q_OBJECT

public:
    explicit CompositionHandler(RpcNotifier *notifier, QObject *parent = nullptr);
    ~CompositionHandler() override;

    QJsonObject handle(const QString &method, const QJsonObject &params) override;
    QStringList supportedMethods() const override;
    QString prefix() const override;

private:
    static QJsonObject handleList(const QJsonObject &params);
    static QJsonObject handleAdd(const QJsonObject &params);
    static QJsonObject handleRemove(const QJsonObject &params);
    static QJsonObject handleGetProperties(const QJsonObject &params);
    static QJsonObject handleSetProperty(const QJsonObject &params);

    static QJsonObject makeProjectNotOpenError();
    static QJsonObject makeNoTimelineError();
    static QJsonObject makeApplicationClosingError();
    static QJsonObject makeWindowNotAvailableError();

    RpcNotifier *m_notifier;
};
