/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#include "rpcnotifier.h"

#include <QJsonDocument>
#include <QMutexLocker>

RpcNotifier::RpcNotifier(QObject *parent)
    : QObject(parent)
{
}

RpcNotifier::~RpcNotifier() = default;

void RpcNotifier::setClient(QWebSocket *client)
{
    QMutexLocker locker(&m_mutex);
    m_client = client;
    if (!client) {
        m_subscriptions.clear();
    }
}

auto RpcNotifier::subscribe(const QString &eventType) -> bool
{
    QMutexLocker locker(&m_mutex);
    if (m_subscriptions.contains(eventType)) {
        return false;
    }
    m_subscriptions.insert(eventType);
    return true;
}

auto RpcNotifier::unsubscribe(const QString &eventType) -> bool
{
    QMutexLocker locker(&m_mutex);
    return m_subscriptions.remove(eventType);
}

auto RpcNotifier::isSubscribed(const QString &eventType) const -> bool
{
    QMutexLocker locker(&m_mutex);
    return m_subscriptions.contains(eventType);
}

auto RpcNotifier::subscriptions() const -> QSet<QString>
{
    QMutexLocker locker(&m_mutex);
    return m_subscriptions;
}

void RpcNotifier::clearSubscriptions()
{
    QMutexLocker locker(&m_mutex);
    m_subscriptions.clear();
}

auto RpcNotifier::availableEventTypes() -> QStringList
{
    return QStringList{QStringLiteral("project.opened"),   QStringLiteral("project.closed"),   QStringLiteral("project.saved"),
                       QStringLiteral("project.modified"), QStringLiteral("timeline.changed"), QStringLiteral("render.started"),
                       QStringLiteral("render.progress"),  QStringLiteral("render.completed"), QStringLiteral("render.error")};
}

void RpcNotifier::notify(const QString &eventType, const QJsonObject &data)
{
    QMutexLocker locker(&m_mutex);
    if (!m_client || !m_client->isValid()) {
        return;
    }

    if (!m_subscriptions.contains(eventType)) {
        return;
    }

    // Unlock before sending to avoid holding lock during I/O
    QWebSocket *client = m_client;
    locker.unlock();

    QJsonObject notification = makeNotification(eventType, data);
    QString json = QString::fromUtf8(QJsonDocument(notification).toJson(QJsonDocument::Compact));
    client->sendTextMessage(json);
    Q_EMIT notificationSent(eventType);
}

void RpcNotifier::sendNotification(const QString &method, const QJsonObject &params)
{
    QMutexLocker locker(&m_mutex);
    if (!m_client || !m_client->isValid()) {
        return;
    }

    // Unlock before sending to avoid holding lock during I/O
    QWebSocket *client = m_client;
    locker.unlock();

    QJsonObject notification = makeNotification(method, params);
    QString json = QString::fromUtf8(QJsonDocument(notification).toJson(QJsonDocument::Compact));
    client->sendTextMessage(json);
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

void RpcNotifier::notifyRenderStarted(const QString &jobId, const QString &outputPath)
{
    notify(QStringLiteral("render.started"), QJsonObject{{QStringLiteral("jobId"), jobId}, {QStringLiteral("outputPath"), outputPath}});
}

void RpcNotifier::notifyRenderProgress(const QString &jobId, int progress, int frame)
{
    notify(QStringLiteral("render.progress"),
           QJsonObject{{QStringLiteral("jobId"), jobId}, {QStringLiteral("progress"), progress}, {QStringLiteral("frame"), frame}});
}

void RpcNotifier::notifyRenderCompleted(const QString &jobId, const QString &outputPath)
{
    notify(QStringLiteral("render.completed"), QJsonObject{{QStringLiteral("jobId"), jobId}, {QStringLiteral("outputPath"), outputPath}});
}

void RpcNotifier::notifyRenderError(const QString &jobId, const QString &error)
{
    notify(QStringLiteral("render.error"), QJsonObject{{QStringLiteral("jobId"), jobId}, {QStringLiteral("error"), error}});
}
