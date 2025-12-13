/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#include "rpcdispatcher.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

RpcDispatcher::RpcDispatcher(QObject *parent)
    : QObject(parent)
{
}

RpcDispatcher::~RpcDispatcher()
{
    qDeleteAll(m_handlers);
    m_handlers.clear();
}

void RpcDispatcher::registerHandler(IRpcHandler *handler)
{
    if (!handler) {
        return;
    }

    QString prefix = handler->prefix();
    if (m_handlers.contains(prefix)) {
        delete m_handlers.take(prefix);
    }

    m_handlers.insert(prefix, handler);
    qInfo() << "RpcDispatcher: Registered handler for prefix:" << prefix;
}

void RpcDispatcher::unregisterHandler(const QString &prefix)
{
    if (m_handlers.contains(prefix)) {
        delete m_handlers.take(prefix);
        qInfo() << "RpcDispatcher: Unregistered handler for prefix:" << prefix;
    }
}

QString RpcDispatcher::dispatch(const QString &json)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        QJsonObject response = makeErrorResponse(QJsonValue::Null, RpcError::ParseError, QStringLiteral("Parse error: %1").arg(parseError.errorString()));
        return QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact));
    }

    QJsonObject response;

    if (doc.isObject()) {
        response = dispatchObject(doc.object());
    } else if (doc.isArray()) {
        // Batch request
        QJsonArray responses;
        const QJsonArray requests = doc.array();
        for (const QJsonValue &req : requests) {
            if (req.isObject()) {
                QJsonObject result = dispatchObject(req.toObject());
                if (!result.isEmpty()) {
                    responses.append(result);
                }
            }
        }
        if (responses.isEmpty()) {
            return QString();
        }
        return QString::fromUtf8(QJsonDocument(responses).toJson(QJsonDocument::Compact));
    } else {
        response = makeErrorResponse(QJsonValue::Null, RpcError::InvalidRequest, QStringLiteral("Invalid request: expected object or array"));
    }

    return QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact));
}

QJsonObject RpcDispatcher::dispatchObject(const QJsonObject &request)
{
    // Validate JSON-RPC version
    if (request.value(QStringLiteral("jsonrpc")).toString() != QLatin1String("2.0")) {
        return makeErrorResponse(request.value(QStringLiteral("id")), RpcError::InvalidRequest, QStringLiteral("Invalid JSON-RPC version"));
    }

    // Get method
    QString method = request.value(QStringLiteral("method")).toString();
    if (method.isEmpty()) {
        return makeErrorResponse(request.value(QStringLiteral("id")), RpcError::InvalidRequest, QStringLiteral("Missing method"));
    }

    // Get params (default to empty object)
    QJsonObject params;
    QJsonValue paramsValue = request.value(QStringLiteral("params"));
    if (paramsValue.isObject()) {
        params = paramsValue.toObject();
    } else if (!paramsValue.isNull() && !paramsValue.isUndefined()) {
        // Params must be object or omitted
        return makeErrorResponse(request.value(QStringLiteral("id")), RpcError::InvalidParams, QStringLiteral("Params must be an object"));
    }

    // Get ID (null for notifications)
    QJsonValue id = request.value(QStringLiteral("id"));

    // Route to handler
    QJsonObject result = routeToHandler(method, params, id);

    // Notifications don't get responses
    if (id.isNull() || id.isUndefined()) {
        return QJsonObject();
    }

    return result;
}

QJsonObject RpcDispatcher::routeToHandler(const QString &method, const QJsonObject &params, const QJsonValue &id)
{
    // Split method into prefix and name
    int dotIndex = method.indexOf(QLatin1Char('.'));
    if (dotIndex <= 0) {
        return makeErrorResponse(id, RpcError::MethodNotFound, QStringLiteral("Method not found: %1").arg(method));
    }

    QString prefix = method.left(dotIndex);
    QString methodName = method.mid(dotIndex + 1);

    IRpcHandler *handler = m_handlers.value(prefix);
    if (!handler) {
        return makeErrorResponse(id, RpcError::MethodNotFound, QStringLiteral("Unknown method prefix: %1").arg(prefix));
    }

    // Check if method is supported
    if (!handler->supportedMethods().contains(methodName)) {
        return makeErrorResponse(id, RpcError::MethodNotFound, QStringLiteral("Method not found: %1").arg(method));
    }

    // Call handler
    QJsonObject handlerResult = handler->handle(methodName, params);

    // Check if handler returned an error
    if (handlerResult.contains(QStringLiteral("error"))) {
        QJsonObject error = handlerResult.value(QStringLiteral("error")).toObject();
        return makeErrorResponse(id, error.value(QStringLiteral("code")).toInt(), error.value(QStringLiteral("message")).toString(),
                                 error.value(QStringLiteral("data")));
    }

    // Return success response
    return makeSuccessResponse(id, handlerResult.value(QStringLiteral("result")));
}

QStringList RpcDispatcher::allMethods() const
{
    QStringList methods;
    for (auto it = m_handlers.constBegin(); it != m_handlers.constEnd(); ++it) {
        const QString prefix = it.key();
        const QStringList handlerMethods = it.value()->supportedMethods();
        for (const QString &method : handlerMethods) {
            methods.append(prefix + QLatin1Char('.') + method);
        }
    }
    return methods;
}

IRpcHandler *RpcDispatcher::handler(const QString &prefix) const
{
    return m_handlers.value(prefix);
}
