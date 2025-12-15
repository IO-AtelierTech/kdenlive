/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

/**
 * @brief JSON-RPC 2.0 error codes
 *
 * Standard JSON-RPC 2.0 error codes (-32700 to -32600) and
 * application-specific error codes (-32099 to -32000).
 */
namespace RpcError {
// JSON-RPC 2.0 standard errors
constexpr int ParseError = -32700;     ///< Invalid JSON
constexpr int InvalidRequest = -32600; ///< Not a valid Request object
constexpr int MethodNotFound = -32601; ///< Method does not exist
constexpr int InvalidParams = -32602;  ///< Invalid method parameters
constexpr int InternalError = -32603;  ///< Internal JSON-RPC error

// Application-specific errors (-32099 to -32000)
constexpr int ProjectNotOpen = -32001;     ///< No project is currently open
constexpr int ClipNotFound = -32002;       ///< Referenced clip not found
constexpr int TrackNotFound = -32003;      ///< Referenced track not found
constexpr int EffectNotFound = -32004;     ///< Referenced effect not found
constexpr int RenderInProgress = -32005;   ///< Render job already running
constexpr int InvalidPath = -32006;        ///< Invalid file path
constexpr int OperationFailed = -32007;    ///< Generic operation failure
constexpr int Unauthorized = -32008;       ///< Authentication required/failed
constexpr int ApplicationClosing = -32009; ///< Application is shutting down
constexpr int TimelineNotReady = -32010;   ///< Timeline not initialized or transitioning
constexpr int WindowNotAvailable = -32011; ///< Main window not available
} // namespace RpcError

/**
 * @brief Interface for RPC method handlers
 *
 * All RPC handlers must implement this interface to be registered
 * with the RpcDispatcher. Each handler is responsible for a group
 * of methods with a common prefix (e.g., "project", "timeline").
 */
class IRpcHandler
{
public:
    virtual ~IRpcHandler() = default;

    /**
     * @brief Handle an RPC method call
     * @param method The method name (without prefix, e.g., "open" for "project.open")
     * @param params The JSON parameters object
     * @return JSON result object (success result or error)
     */
    virtual QJsonObject handle(const QString &method, const QJsonObject &params) = 0;

    /**
     * @brief Get list of supported method names (without prefix)
     * @return List of method names this handler supports
     */
    virtual QStringList supportedMethods() const = 0;

    /**
     * @brief Get the method prefix for this handler
     * @return Prefix string (e.g., "project", "timeline", "rpc")
     */
    virtual QString prefix() const = 0;
};

/**
 * @brief Create a JSON-RPC 2.0 success response
 * @param id The request ID
 * @param result The result value
 * @return Complete JSON-RPC response object
 */
inline QJsonObject makeSuccessResponse(const QJsonValue &id, const QJsonValue &result)
{
    return QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("result"), result}, {QStringLiteral("id"), id}};
}

/**
 * @brief Create a JSON-RPC 2.0 error response
 * @param id The request ID (can be null for parse errors)
 * @param code The error code
 * @param message The error message
 * @param data Optional additional error data
 * @return Complete JSON-RPC error response object
 */
inline QJsonObject makeErrorResponse(const QJsonValue &id, int code, const QString &message, const QJsonValue &data = QJsonValue())
{
    QJsonObject error{{QStringLiteral("code"), code}, {QStringLiteral("message"), message}};
    if (!data.isNull() && !data.isUndefined()) {
        error[QStringLiteral("data")] = data;
    }
    return QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("error"), error}, {QStringLiteral("id"), id}};
}

/**
 * @brief Create a JSON-RPC 2.0 notification (server -> client)
 * @param method The notification method name
 * @param params The notification parameters
 * @return Complete JSON-RPC notification object (no id field)
 */
inline QJsonObject makeNotification(const QString &method, const QJsonObject &params)
{
    return QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("method"), method}, {QStringLiteral("params"), params}};
}

/**
 * @brief RPC protocol version
 */
constexpr const char *RPC_VERSION = "1.0";

/**
 * @brief Default WebSocket server port
 */
constexpr quint16 RPC_DEFAULT_PORT = 9876;
