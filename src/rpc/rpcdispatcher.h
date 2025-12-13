/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#pragma once

#include "rpctypes.h"

#include <QHash>
#include <QObject>
#include <QString>

/**
 * @brief JSON-RPC 2.0 method dispatcher
 *
 * Routes incoming JSON-RPC requests to the appropriate handler
 * based on method prefix. Handles JSON parsing and error responses.
 */
class RpcDispatcher : public QObject
{
    Q_OBJECT

public:
    explicit RpcDispatcher(QObject *parent = nullptr);
    ~RpcDispatcher() override;

    /**
     * @brief Register a handler for a method prefix
     * @param handler The handler to register (takes ownership)
     */
    void registerHandler(IRpcHandler *handler);

    /**
     * @brief Unregister a handler
     * @param prefix The prefix to unregister
     */
    void unregisterHandler(const QString &prefix);

    /**
     * @brief Dispatch a JSON-RPC request
     * @param json The raw JSON request string
     * @return JSON response string
     */
    QString dispatch(const QString &json);

    /**
     * @brief Get all registered method names
     * @return List of fully qualified method names (prefix.method)
     */
    QStringList allMethods() const;

    /**
     * @brief Get handler by prefix
     * @param prefix The handler prefix
     * @return Handler pointer or nullptr
     */
    IRpcHandler *handler(const QString &prefix) const;

private:
    QJsonObject dispatchObject(const QJsonObject &request);
    QJsonObject routeToHandler(const QString &method, const QJsonObject &params, const QJsonValue &id);

    QHash<QString, IRpcHandler *> m_handlers;
};
