/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#pragma once

#include "../rpctypes.h"

#include <QObject>

class RpcNotifier;

/**
 * @brief Handler for transition and composition RPC methods
 *
 * Handles the transition.* and composition.* method namespaces:
 *
 * transition.*:
 * - transition.list: List available transition types
 * - transition.add: Add a transition between clips
 * - transition.remove: Remove a transition
 * - transition.getProperties: Get transition properties
 * - transition.setProperty: Set a transition property
 *
 * composition.*:
 * - composition.list: List compositions on timeline
 * - composition.add: Add a composition
 * - composition.remove: Remove a composition
 * - composition.getProperties: Get composition properties
 * - composition.setProperty: Set a composition property
 */
class TransitionHandler : public QObject, public IRpcHandler
{
    Q_OBJECT

public:
    explicit TransitionHandler(RpcNotifier *notifier, QObject *parent = nullptr);
    ~TransitionHandler() override;

    QJsonObject handle(const QString &method, const QJsonObject &params) override;
    QStringList supportedMethods() const override;
    QString prefix() const override;

private:
    // Transition methods
    QJsonObject handleTransitionList(const QJsonObject &params);
    QJsonObject handleTransitionAdd(const QJsonObject &params);
    QJsonObject handleTransitionRemove(const QJsonObject &params);
    QJsonObject handleTransitionGetProperties(const QJsonObject &params);
    QJsonObject handleTransitionSetProperty(const QJsonObject &params);

    // Composition methods
    QJsonObject handleCompositionList(const QJsonObject &params);
    QJsonObject handleCompositionAdd(const QJsonObject &params);
    QJsonObject handleCompositionRemove(const QJsonObject &params);
    QJsonObject handleCompositionGetProperties(const QJsonObject &params);
    QJsonObject handleCompositionSetProperty(const QJsonObject &params);

    QJsonObject makeProjectNotOpenError();
    QJsonObject makeNoTimelineError();

    RpcNotifier *m_notifier;
    bool m_isTransitionPrefix; // Set to true when handling transition.*, false for composition.*
};
