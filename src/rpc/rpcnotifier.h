/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#pragma once

#include "rpctypes.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QWebSocket>

/**
 * @brief Notification sender for JSON-RPC events
 *
 * Manages subscriptions and sends JSON-RPC 2.0 notifications
 * to connected clients. Notifications are one-way messages
 * from server to client without an id field.
 *
 * Supported event types:
 * - project.opened, project.closed, project.saved, project.modified
 * - timeline.changed
 * - render.started, render.progress, render.completed, render.error
 */
class RpcNotifier : public QObject
{
    Q_OBJECT

public:
    explicit RpcNotifier(QObject *parent = nullptr);
    ~RpcNotifier() override;

    /**
     * @brief Set the client socket to send notifications to
     * @param client The WebSocket client (nullptr to clear)
     */
    void setClient(QWebSocket *client);

    /**
     * @brief Subscribe to an event type
     * @param eventType The event type to subscribe to
     * @return true if subscribed, false if already subscribed
     */
    bool subscribe(const QString &eventType);

    /**
     * @brief Unsubscribe from an event type
     * @param eventType The event type to unsubscribe from
     * @return true if unsubscribed, false if not subscribed
     */
    bool unsubscribe(const QString &eventType);

    /**
     * @brief Check if subscribed to an event type
     * @param eventType The event type to check
     * @return true if subscribed
     */
    bool isSubscribed(const QString &eventType) const;

    /**
     * @brief Get list of current subscriptions
     * @return Set of subscribed event types
     */
    QSet<QString> subscriptions() const;

    /**
     * @brief Clear all subscriptions
     */
    void clearSubscriptions();

    /**
     * @brief Get list of all available event types
     * @return List of event type names
     */
    static QStringList availableEventTypes();

public Q_SLOTS:
    /**
     * @brief Send a notification to the client
     * @param eventType The event type (method name)
     * @param data The notification data
     */
    void notify(const QString &eventType, const QJsonObject &data);

    // Convenience slots for common events
    void notifyProjectOpened(const QString &path);
    void notifyProjectClosed();
    void notifyProjectSaved(const QString &path);
    void notifyProjectModified(bool modified);
    void notifyTimelineChanged();

    // Render notification slots
    void notifyRenderStarted(const QString &jobId, const QString &outputPath);
    void notifyRenderProgress(const QString &jobId, int progress, int frame);
    void notifyRenderCompleted(const QString &jobId, const QString &outputPath);
    void notifyRenderError(const QString &jobId, const QString &error);

Q_SIGNALS:
    /**
     * @brief Emitted when a notification is sent
     * @param eventType The event type
     */
    void notificationSent(const QString &eventType);

private:
    void sendNotification(const QString &method, const QJsonObject &params);

    QWebSocket *m_client{nullptr};
    QSet<QString> m_subscriptions;
};
