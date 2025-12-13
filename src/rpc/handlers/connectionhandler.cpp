/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#include "connectionhandler.h"
#include "../rpcdispatcher.h"
#include "../rpcnotifier.h"

#include <config-kdenlive.h>

#include <QJsonArray>

ConnectionHandler::ConnectionHandler(RpcNotifier *notifier, RpcDispatcher *dispatcher, QObject *parent)
    : QObject(parent)
    , m_notifier(notifier)
    , m_dispatcher(dispatcher)
{
}

ConnectionHandler::~ConnectionHandler() = default;

QString ConnectionHandler::prefix() const
{
    return QStringLiteral("rpc");
}

QStringList ConnectionHandler::supportedMethods() const
{
    return QStringList{QStringLiteral("ping"), QStringLiteral("getVersion"), QStringLiteral("getCapabilities"), QStringLiteral("subscribe"),
                       QStringLiteral("unsubscribe")};
}

QJsonObject ConnectionHandler::handle(const QString &method, const QJsonObject &params)
{
    if (method == QLatin1String("ping")) {
        return handlePing(params);
    }
    if (method == QLatin1String("getVersion")) {
        return handleGetVersion(params);
    }
    if (method == QLatin1String("getCapabilities")) {
        return handleGetCapabilities(params);
    }
    if (method == QLatin1String("subscribe")) {
        return handleSubscribe(params);
    }
    if (method == QLatin1String("unsubscribe")) {
        return handleUnsubscribe(params);
    }

    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::MethodNotFound},
                                                             {QStringLiteral("message"), QStringLiteral("Unknown method: rpc.%1").arg(method)}}}};
}

QJsonObject ConnectionHandler::handlePing(const QJsonObject & /*params*/)
{
    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("pong"), true}}}};
}

QJsonObject ConnectionHandler::handleGetVersion(const QJsonObject & /*params*/)
{
    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("kdenlive"), QStringLiteral(KDENLIVE_VERSION)},
                                                              {QStringLiteral("rpc"), QString::fromLatin1(RPC_VERSION)}}}};
}

QJsonObject ConnectionHandler::handleGetCapabilities(const QJsonObject & /*params*/)
{
    QJsonArray methods;
    const QStringList allMethods = m_dispatcher->allMethods();
    for (const QString &method : allMethods) {
        methods.append(method);
    }

    QJsonArray events;
    const QStringList eventTypes = RpcNotifier::availableEventTypes();
    for (const QString &event : eventTypes) {
        events.append(event);
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("methods"), methods}, {QStringLiteral("events"), events}}}};
}

QJsonObject ConnectionHandler::handleSubscribe(const QJsonObject &params)
{
    QStringList events;

    // Accept either a single event or an array of events
    QJsonValue eventsValue = params.value(QStringLiteral("events"));
    if (eventsValue.isString()) {
        events.append(eventsValue.toString());
    } else if (eventsValue.isArray()) {
        const QJsonArray arr = eventsValue.toArray();
        for (const QJsonValue &v : arr) {
            if (v.isString()) {
                events.append(v.toString());
            }
        }
    }

    if (events.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'events' parameter")}}}};
    }

    QJsonArray subscribed;
    const QStringList available = RpcNotifier::availableEventTypes();

    for (const QString &event : std::as_const(events)) {
        if (available.contains(event)) {
            m_notifier->subscribe(event);
            subscribed.append(event);
        }
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("subscribed"), subscribed}}}};
}

QJsonObject ConnectionHandler::handleUnsubscribe(const QJsonObject &params)
{
    QStringList events;

    QJsonValue eventsValue = params.value(QStringLiteral("events"));
    if (eventsValue.isString()) {
        events.append(eventsValue.toString());
    } else if (eventsValue.isArray()) {
        const QJsonArray arr = eventsValue.toArray();
        for (const QJsonValue &v : arr) {
            if (v.isString()) {
                events.append(v.toString());
            }
        }
    }

    if (events.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'events' parameter")}}}};
    }

    QJsonArray unsubscribed;
    for (const QString &event : std::as_const(events)) {
        if (m_notifier->unsubscribe(event)) {
            unsubscribed.append(event);
        }
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("unsubscribed"), unsubscribed}}}};
}
