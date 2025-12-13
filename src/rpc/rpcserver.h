/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#pragma once

#include "rpctypes.h"

#include <QObject>
#include <QString>
#include <QWebSocket>
#include <QWebSocketServer>
#include <memory>

class RpcDispatcher;
class RpcNotifier;

/**
 * @brief WebSocket server for JSON-RPC 2.0 communication
 *
 * Provides a local WebSocket server for external clients to control
 * Kdenlive via JSON-RPC 2.0 protocol. Binds to localhost only for
 * security. Supports optional bearer token authentication.
 *
 * Single client model: only one client can be connected at a time.
 * New connections will disconnect any existing client.
 */
class RpcServer : public QObject
{
    Q_OBJECT

public:
    explicit RpcServer(QObject *parent = nullptr);
    ~RpcServer() override;

    /**
     * @brief Start the WebSocket server
     * @param port Port to listen on (default: 9876)
     * @param authToken Optional bearer token for authentication
     * @return true if server started successfully
     */
    bool start(quint16 port = RPC_DEFAULT_PORT, const QString &authToken = QString());

    /**
     * @brief Stop the WebSocket server
     */
    void stop();

    /**
     * @brief Check if server is running
     * @return true if server is listening
     */
    bool isRunning() const;

    /**
     * @brief Get the server port
     * @return Port number, or 0 if not running
     */
    quint16 port() const;

    /**
     * @brief Check if a client is connected
     * @return true if a client is connected
     */
    bool hasClient() const;

    /**
     * @brief Get the RPC dispatcher
     * @return Pointer to the dispatcher
     */
    RpcDispatcher *dispatcher() const;

    /**
     * @brief Get the RPC notifier
     * @return Pointer to the notifier
     */
    RpcNotifier *notifier() const;

Q_SIGNALS:
    /**
     * @brief Emitted when a client connects
     */
    void clientConnected();

    /**
     * @brief Emitted when the client disconnects
     */
    void clientDisconnected();

    /**
     * @brief Emitted when server starts
     * @param port The port the server is listening on
     */
    void serverStarted(quint16 port);

    /**
     * @brief Emitted when server stops
     */
    void serverStopped();

private Q_SLOTS:
    void onNewConnection();
    void onClientDisconnected();
    void onTextMessageReceived(const QString &message);
    void onError(QAbstractSocket::SocketError error);

private:
    void setupHandlers();
    bool authenticateClient(const QString &message);

    std::unique_ptr<QWebSocketServer> m_server;
    QWebSocket *m_client{nullptr};
    std::unique_ptr<RpcDispatcher> m_dispatcher;
    std::unique_ptr<RpcNotifier> m_notifier;
    QString m_authToken;
    bool m_authenticated{false};
};
