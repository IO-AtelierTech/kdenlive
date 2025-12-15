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

auto ConnectionHandler::prefix() const -> QString
{
    return QStringLiteral("rpc");
}

auto ConnectionHandler::supportedMethods() const -> QStringList
{
    return QStringList{QStringLiteral("ping"), QStringLiteral("getVersion"), QStringLiteral("getCapabilities"), QStringLiteral("subscribe"),
                       QStringLiteral("unsubscribe")};
}

auto ConnectionHandler::handle(const QString &method, const QJsonObject &params) -> QJsonObject
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

auto ConnectionHandler::handlePing(const QJsonObject & /*params*/) -> QJsonObject
{
    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("pong"), true}}}};
}

auto ConnectionHandler::handleGetVersion(const QJsonObject & /*params*/) -> QJsonObject
{
    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("kdenlive"), QStringLiteral(KDENLIVE_VERSION)},
                                                              {QStringLiteral("rpc"), QString::fromLatin1(RPC_VERSION)}}}};
}

auto ConnectionHandler::handleGetCapabilities(const QJsonObject & /*params*/) -> QJsonObject
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

auto ConnectionHandler::handleSubscribe(const QJsonObject &params) -> QJsonObject
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

auto ConnectionHandler::handleUnsubscribe(const QJsonObject &params) -> QJsonObject
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
