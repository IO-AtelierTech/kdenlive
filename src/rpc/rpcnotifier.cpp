/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#include "rpcnotifier.h"

#include <QJsonDocument>

RpcNotifier::RpcNotifier(QObject *parent)
    : QObject(parent)
{
}

RpcNotifier::~RpcNotifier() = default;

void RpcNotifier::setClient(QWebSocket *client)
{
    m_client = client;
    if (!client) {
        clearSubscriptions();
    }
}

bool RpcNotifier::subscribe(const QString &eventType)
{
    if (m_subscriptions.contains(eventType)) {
        return false;
    }
    m_subscriptions.insert(eventType);
    return true;
}

bool RpcNotifier::unsubscribe(const QString &eventType)
{
    return m_subscriptions.remove(eventType);
}

bool RpcNotifier::isSubscribed(const QString &eventType) const
{
    return m_subscriptions.contains(eventType);
}

QSet<QString> RpcNotifier::subscriptions() const
{
    return m_subscriptions;
}

void RpcNotifier::clearSubscriptions()
{
    m_subscriptions.clear();
}

QStringList RpcNotifier::availableEventTypes()
{
    return QStringList{QStringLiteral("project.opened"),   QStringLiteral("project.closed"),   QStringLiteral("project.saved"),
                       QStringLiteral("project.modified"), QStringLiteral("timeline.changed"), QStringLiteral("render.started"),
                       QStringLiteral("render.progress"),  QStringLiteral("render.completed"), QStringLiteral("render.error")};
}

void RpcNotifier::notify(const QString &eventType, const QJsonObject &data)
{
    if (!m_client || !m_client->isValid()) {
        return;
    }

    if (!m_subscriptions.contains(eventType)) {
        return;
    }

    sendNotification(eventType, data);
}

void RpcNotifier::sendNotification(const QString &method, const QJsonObject &params)
{
    if (!m_client || !m_client->isValid()) {
        return;
    }

    QJsonObject notification = makeNotification(method, params);
    QString json = QString::fromUtf8(QJsonDocument(notification).toJson(QJsonDocument::Compact));
    m_client->sendTextMessage(json);
    Q_EMIT notificationSent(method);
}

void RpcNotifier::notifyProjectOpened(const QString &path)
{
    notify(QStringLiteral("project.opened"), QJsonObject{{QStringLiteral("path"), path}});
}

void RpcNotifier::notifyProjectClosed()
{
    notify(QStringLiteral("project.closed"), QJsonObject{});
}

void RpcNotifier::notifyProjectSaved(const QString &path)
{
    notify(QStringLiteral("project.saved"), QJsonObject{{QStringLiteral("path"), path}});
}

void RpcNotifier::notifyProjectModified(bool modified)
{
    notify(QStringLiteral("project.modified"), QJsonObject{{QStringLiteral("modified"), modified}});
}

void RpcNotifier::notifyTimelineChanged()
{
    notify(QStringLiteral("timeline.changed"), QJsonObject{});
}
