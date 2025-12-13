/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#include "rpcserver.h"
#include "handlers/assetshandler.h"
#include "handlers/binhandler.h"
#include "handlers/compositionhandler.h"
#include "handlers/connectionhandler.h"
#include "handlers/effectshandler.h"
#include "handlers/projecthandler.h"
#include "handlers/renderhandler.h"
#include "handlers/timelinehandler.h"
#include "handlers/transitionhandler.h"
#include "rpcdispatcher.h"
#include "rpcnotifier.h"

#include "kdenlivesettings.h"

#include <QHostAddress>
#include <QJsonDocument>

RpcServer::RpcServer(QObject *parent)
    : QObject(parent)
    , m_dispatcher(std::make_unique<RpcDispatcher>(this))
    , m_notifier(std::make_unique<RpcNotifier>(this))
{
    setupHandlers();
}

RpcServer::~RpcServer()
{
    stop();
}

void RpcServer::setupHandlers()
{
    // Register built-in handlers
    m_dispatcher->registerHandler(new ConnectionHandler(m_notifier.get(), m_dispatcher.get(), this));
    m_dispatcher->registerHandler(new ProjectHandler(m_notifier.get(), this));

    // Register timeline, bin, and transition handlers
    m_dispatcher->registerHandler(new TimelineHandler(m_notifier.get(), this));
    m_dispatcher->registerHandler(new BinHandler(m_notifier.get(), this));
    m_dispatcher->registerHandler(new TransitionHandler(m_notifier.get(), this));
    m_dispatcher->registerHandler(new CompositionHandler(m_notifier.get(), this));

    // Register effects, assets, and render handlers
    m_dispatcher->registerHandler(new EffectsHandler(m_notifier.get(), this));
    m_dispatcher->registerHandler(new AssetsHandler(m_notifier.get(), this));
    m_dispatcher->registerHandler(new RenderHandler(m_notifier.get(), this));
}

auto RpcServer::start(quint16 port, const QString &authToken) -> bool
{
    // Check if RPC is enabled in settings
    if (!KdenliveSettings::rpcEnabled()) {
        qInfo() << "RpcServer: RPC server disabled in settings";
        return false;
    }

    if (m_server && m_server->isListening()) {
        return true; // Already running
    }

    // Use settings if default values provided
    quint16 actualPort = (port == RPC_DEFAULT_PORT) ? static_cast<quint16>(KdenliveSettings::rpcPort()) : port;
    QString actualToken = authToken.isEmpty() ? KdenliveSettings::rpcAuthToken() : authToken;

    m_authToken = actualToken;
    m_authenticated = m_authToken.isEmpty(); // No auth needed if no token set

    m_server = std::make_unique<QWebSocketServer>(QStringLiteral("Kdenlive RPC"), QWebSocketServer::NonSecureMode, this);

    if (!m_server->listen(QHostAddress::LocalHost, actualPort)) {
        qWarning() << "RpcServer: Failed to start on port" << actualPort << "-" << m_server->errorString();
        m_server.reset();
        return false;
    }

    connect(m_server.get(), &QWebSocketServer::newConnection, this, &RpcServer::onNewConnection);

    qInfo() << "RpcServer: Listening on ws://127.0.0.1:" << m_server->serverPort();
    Q_EMIT serverStarted(m_server->serverPort());
    return true;
}

void RpcServer::stop()
{
    if (m_client) {
        QWebSocket *oldClient = m_client;
        m_client = nullptr;
        m_notifier->setClient(nullptr);
        disconnect(oldClient, nullptr, this, nullptr);
        oldClient->close();
        oldClient->deleteLater();
    }

    if (m_server) {
        m_server->close();
        m_server.reset();
        Q_EMIT serverStopped();
        qInfo() << "RpcServer: Stopped";
    }
}

auto RpcServer::isRunning() const -> bool
{
    return m_server && m_server->isListening();
}

auto RpcServer::port() const -> quint16
{
    return m_server ? m_server->serverPort() : 0;
}

auto RpcServer::hasClient() const -> bool
{
    return m_client != nullptr && m_client->isValid();
}

auto RpcServer::dispatcher() const -> RpcDispatcher *
{
    return m_dispatcher.get();
}

auto RpcServer::notifier() const -> RpcNotifier *
{
    return m_notifier.get();
}

void RpcServer::onNewConnection()
{
    QWebSocket *socket = m_server->nextPendingConnection();
    if (!socket) {
        return;
    }

    // Single client model: disconnect existing client
    if (m_client) {
        qInfo() << "RpcServer: Disconnecting existing client for new connection";
        QWebSocket *oldClient = m_client;
        m_client = nullptr;

        // Clear notifier reference first to prevent stale pointer access
        m_notifier->setClient(nullptr);

        // Now safely disconnect and cleanup the old socket
        disconnect(oldClient, nullptr, this, nullptr);
        oldClient->close();
        oldClient->deleteLater();
    }

    m_client = socket;
    m_authenticated = m_authToken.isEmpty();
    m_notifier->setClient(m_client);

    connect(m_client, &QWebSocket::textMessageReceived, this, &RpcServer::onTextMessageReceived);
    connect(m_client, &QWebSocket::disconnected, this, &RpcServer::onClientDisconnected);
    connect(m_client, &QWebSocket::errorOccurred, this, &RpcServer::onError);

    qInfo() << "RpcServer: Client connected from" << socket->peerAddress().toString();

    if (m_authenticated) {
        Q_EMIT clientConnected();
    }
}

void RpcServer::onClientDisconnected()
{
    auto *disconnectedSocket = qobject_cast<QWebSocket *>(sender());

    // Only handle if it's our current client (not an old one being replaced)
    if (m_client && m_client == disconnectedSocket) {
        qInfo() << "RpcServer: Client disconnected";
        m_client->deleteLater();
        m_client = nullptr;
        m_authenticated = m_authToken.isEmpty();
        m_notifier->setClient(nullptr);
        Q_EMIT clientDisconnected();
    } else if (disconnectedSocket) {
        // Old client being replaced - just clean it up
        disconnectedSocket->deleteLater();
    }
}

void RpcServer::onTextMessageReceived(const QString &message)
{
    if (!m_authenticated) {
        if (authenticateClient(message)) {
            m_authenticated = true;
            Q_EMIT clientConnected();
            // Send success response for auth
            QJsonObject response = makeSuccessResponse(QJsonValue::Null, QJsonObject{{QStringLiteral("authenticated"), true}});
            m_client->sendTextMessage(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)));
        } else {
            QJsonObject response = makeErrorResponse(QJsonValue::Null, RpcError::Unauthorized, QStringLiteral("Invalid authentication token"));
            m_client->sendTextMessage(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)));
            m_client->close();
        }
        return;
    }

    // Dispatch the message and send response
    QString response = m_dispatcher->dispatch(message);
    if (!response.isEmpty()) {
        m_client->sendTextMessage(response);
    }
}

auto RpcServer::authenticateClient(const QString &message) -> bool
{
    // Expected format: {"jsonrpc": "2.0", "method": "rpc.auth", "params": {"token": "..."}, "id": ...}
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        return false;
    }

    QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("method")).toString() != QLatin1String("rpc.auth")) {
        return false;
    }

    QJsonObject params = obj.value(QStringLiteral("params")).toObject();
    QString token = params.value(QStringLiteral("token")).toString();

    return token == m_authToken;
}

void RpcServer::onError(QAbstractSocket::SocketError error)
{
    qWarning() << "RpcServer: Socket error:" << error;
    if (m_client) {
        qWarning() << "RpcServer: Client error:" << m_client->errorString();
    }
}
