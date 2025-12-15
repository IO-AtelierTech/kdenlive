/*
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "test_utils.hpp"

#include "rpc/rpcdispatcher.h"
#include "rpc/rpcnotifier.h"
#include "rpc/rpctypes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

/**
 * @brief Mock RPC handler for testing
 */
class MockRpcHandler : public IRpcHandler
{
public:
    explicit MockRpcHandler(const QString &prefix)
        : m_prefix(prefix)
    {
    }

    QJsonObject handle(const QString &method, const QJsonObject &params) override
    {
        m_lastMethod = method;
        m_lastParams = params;
        m_callCount++;

        if (method == QStringLiteral("echo")) {
            // Return success result - dispatcher wraps this
            return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("echo"), params}}}};
        }
        if (method == QStringLiteral("add")) {
            int a = params[QStringLiteral("a")].toInt();
            int b = params[QStringLiteral("b")].toInt();
            return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("sum"), a + b}}}};
        }
        if (method == QStringLiteral("error")) {
            // Return error object (not full response - dispatcher adds jsonrpc/id)
            return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                     {QStringLiteral("message"), QStringLiteral("Intentional error")}}}};
        }
        return QJsonObject{{QStringLiteral("error"),
                            QJsonObject{{QStringLiteral("code"), RpcError::MethodNotFound}, {QStringLiteral("message"), QStringLiteral("Unknown method")}}}};
    }

    QStringList supportedMethods() const override { return {QStringLiteral("echo"), QStringLiteral("add"), QStringLiteral("error")}; }

    QString prefix() const override { return m_prefix; }

    // Test inspection
    QString lastMethod() const { return m_lastMethod; }
    QJsonObject lastParams() const { return m_lastParams; }
    int callCount() const { return m_callCount; }
    void resetCallCount() { m_callCount = 0; }

private:
    QString m_prefix;
    QString m_lastMethod;
    QJsonObject m_lastParams;
    int m_callCount = 0;
};

// =============================================================================
// RpcTypes Tests
// =============================================================================

TEST_CASE("RPC error codes are correct", "[rpc][types]")
{
    // JSON-RPC 2.0 standard error codes
    REQUIRE(RpcError::ParseError == -32700);
    REQUIRE(RpcError::InvalidRequest == -32600);
    REQUIRE(RpcError::MethodNotFound == -32601);
    REQUIRE(RpcError::InvalidParams == -32602);
    REQUIRE(RpcError::InternalError == -32603);

    // Application-specific error codes
    REQUIRE(RpcError::ProjectNotOpen == -32001);
    REQUIRE(RpcError::ClipNotFound == -32002);
    REQUIRE(RpcError::TrackNotFound == -32003);
    REQUIRE(RpcError::EffectNotFound == -32004);
    REQUIRE(RpcError::RenderInProgress == -32005);
    REQUIRE(RpcError::InvalidPath == -32006);
    REQUIRE(RpcError::OperationFailed == -32007);
    REQUIRE(RpcError::Unauthorized == -32008);
}

TEST_CASE("makeSuccessResponse creates valid JSON-RPC response", "[rpc][types]")
{
    SECTION("with integer result")
    {
        QJsonObject response = makeSuccessResponse(1, 42);

        REQUIRE(response[QStringLiteral("jsonrpc")].toString() == QStringLiteral("2.0"));
        REQUIRE(response[QStringLiteral("id")].toInt() == 1);
        REQUIRE(response[QStringLiteral("result")].toInt() == 42);
        REQUIRE(!response.contains(QStringLiteral("error")));
    }

    SECTION("with object result")
    {
        QJsonObject result{{QStringLiteral("name"), QStringLiteral("test")}, {QStringLiteral("value"), 123}};
        QJsonObject response = makeSuccessResponse(QStringLiteral("abc"), result);

        REQUIRE(response[QStringLiteral("jsonrpc")].toString() == QStringLiteral("2.0"));
        REQUIRE(response[QStringLiteral("id")].toString() == QStringLiteral("abc"));
        REQUIRE(response[QStringLiteral("result")].toObject()[QStringLiteral("name")].toString() == QStringLiteral("test"));
    }

    SECTION("with array result")
    {
        QJsonArray arr{1, 2, 3};
        QJsonObject response = makeSuccessResponse(1, arr);

        REQUIRE(response[QStringLiteral("result")].toArray().size() == 3);
    }

    SECTION("with null id")
    {
        QJsonObject response = makeSuccessResponse(QJsonValue::Null, true);

        REQUIRE(response[QStringLiteral("id")].isNull());
        REQUIRE(response[QStringLiteral("result")].toBool() == true);
    }
}

TEST_CASE("makeErrorResponse creates valid JSON-RPC error", "[rpc][types]")
{
    SECTION("basic error")
    {
        QJsonObject response = makeErrorResponse(1, RpcError::MethodNotFound, QStringLiteral("Method not found"));

        REQUIRE(response[QStringLiteral("jsonrpc")].toString() == QStringLiteral("2.0"));
        REQUIRE(response[QStringLiteral("id")].toInt() == 1);
        REQUIRE(!response.contains(QStringLiteral("result")));

        QJsonObject error = response[QStringLiteral("error")].toObject();
        REQUIRE(error[QStringLiteral("code")].toInt() == RpcError::MethodNotFound);
        REQUIRE(error[QStringLiteral("message")].toString() == QStringLiteral("Method not found"));
        REQUIRE(!error.contains(QStringLiteral("data")));
    }

    SECTION("error with data")
    {
        QJsonObject data{{QStringLiteral("detail"), QStringLiteral("extra info")}};
        QJsonObject response = makeErrorResponse(1, RpcError::InvalidParams, QStringLiteral("Bad params"), data);

        QJsonObject error = response[QStringLiteral("error")].toObject();
        REQUIRE(error.contains(QStringLiteral("data")));
        REQUIRE(error[QStringLiteral("data")].toObject()[QStringLiteral("detail")].toString() == QStringLiteral("extra info"));
    }

    SECTION("parse error with null id")
    {
        QJsonObject response = makeErrorResponse(QJsonValue::Null, RpcError::ParseError, QStringLiteral("Parse error"));

        REQUIRE(response[QStringLiteral("id")].isNull());
        REQUIRE(response[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt() == RpcError::ParseError);
    }
}

TEST_CASE("makeNotification creates valid JSON-RPC notification", "[rpc][types]")
{
    QJsonObject params{{QStringLiteral("path"), QStringLiteral("/home/test.kdenlive")}};
    QJsonObject notification = makeNotification(QStringLiteral("project.opened"), params);

    REQUIRE(notification[QStringLiteral("jsonrpc")].toString() == QStringLiteral("2.0"));
    REQUIRE(notification[QStringLiteral("method")].toString() == QStringLiteral("project.opened"));
    REQUIRE(notification[QStringLiteral("params")].toObject()[QStringLiteral("path")].toString() == QStringLiteral("/home/test.kdenlive"));
    REQUIRE(!notification.contains(QStringLiteral("id"))); // Notifications have no id
}

TEST_CASE("RPC constants are correct", "[rpc][types]")
{
    REQUIRE(QString::fromLatin1(RPC_VERSION) == QStringLiteral("1.0"));
    REQUIRE(RPC_DEFAULT_PORT == 9876);
}

// =============================================================================
// RpcDispatcher Tests
// =============================================================================

TEST_CASE("RpcDispatcher handles handler registration", "[rpc][dispatcher]")
{
    RpcDispatcher dispatcher;

    SECTION("register single handler")
    {
        auto *handler = new MockRpcHandler(QStringLiteral("test"));
        dispatcher.registerHandler(handler);

        REQUIRE(dispatcher.handler(QStringLiteral("test")) == handler);
        REQUIRE(dispatcher.handler(QStringLiteral("unknown")) == nullptr);
    }

    SECTION("register multiple handlers")
    {
        dispatcher.registerHandler(new MockRpcHandler(QStringLiteral("foo")));
        dispatcher.registerHandler(new MockRpcHandler(QStringLiteral("bar")));

        REQUIRE(dispatcher.handler(QStringLiteral("foo")) != nullptr);
        REQUIRE(dispatcher.handler(QStringLiteral("bar")) != nullptr);
    }

    SECTION("unregister handler")
    {
        dispatcher.registerHandler(new MockRpcHandler(QStringLiteral("test")));
        REQUIRE(dispatcher.handler(QStringLiteral("test")) != nullptr);

        dispatcher.unregisterHandler(QStringLiteral("test"));
        REQUIRE(dispatcher.handler(QStringLiteral("test")) == nullptr);
    }

    SECTION("allMethods returns registered methods")
    {
        dispatcher.registerHandler(new MockRpcHandler(QStringLiteral("test")));

        QStringList methods = dispatcher.allMethods();
        REQUIRE(methods.contains(QStringLiteral("test.echo")));
        REQUIRE(methods.contains(QStringLiteral("test.add")));
        REQUIRE(methods.contains(QStringLiteral("test.error")));
    }
}

TEST_CASE("RpcDispatcher routes requests correctly", "[rpc][dispatcher]")
{
    RpcDispatcher dispatcher;
    auto *handler = new MockRpcHandler(QStringLiteral("test"));
    dispatcher.registerHandler(handler);

    SECTION("valid request is routed to handler")
    {
        QString request = QStringLiteral(R"({"jsonrpc":"2.0","method":"test.echo","params":{"msg":"hello"},"id":1})");
        QString response = dispatcher.dispatch(request);

        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
        QJsonObject obj = doc.object();

        REQUIRE(obj[QStringLiteral("jsonrpc")].toString() == QStringLiteral("2.0"));
        REQUIRE(obj[QStringLiteral("id")].toInt() == 1);
        REQUIRE(obj.contains(QStringLiteral("result")));

        // Verify handler received correct method
        REQUIRE(handler->lastMethod() == QStringLiteral("echo"));
        REQUIRE(handler->lastParams()[QStringLiteral("msg")].toString() == QStringLiteral("hello"));
    }

    SECTION("handler computes correct result")
    {
        QString request = QStringLiteral(R"({"jsonrpc":"2.0","method":"test.add","params":{"a":5,"b":3},"id":2})");
        QString response = dispatcher.dispatch(request);

        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
        QJsonObject obj = doc.object();

        // Result contains the nested object from handler
        QJsonObject result = obj[QStringLiteral("result")].toObject();
        REQUIRE(result[QStringLiteral("sum")].toInt() == 8);
    }
}

TEST_CASE("RpcDispatcher handles errors correctly", "[rpc][dispatcher]")
{
    RpcDispatcher dispatcher;
    dispatcher.registerHandler(new MockRpcHandler(QStringLiteral("test")));

    SECTION("invalid JSON returns parse error")
    {
        QString response = dispatcher.dispatch(QStringLiteral("not valid json"));

        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
        QJsonObject obj = doc.object();

        REQUIRE(obj.contains(QStringLiteral("error")));
        REQUIRE(obj[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt() == RpcError::ParseError);
    }

    SECTION("missing jsonrpc field returns invalid request")
    {
        QString request = QStringLiteral(R"({"method":"test.echo","id":1})");
        QString response = dispatcher.dispatch(request);

        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
        QJsonObject obj = doc.object();

        REQUIRE(obj[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt() == RpcError::InvalidRequest);
    }

    SECTION("wrong jsonrpc version returns invalid request")
    {
        QString request = QStringLiteral(R"({"jsonrpc":"1.0","method":"test.echo","id":1})");
        QString response = dispatcher.dispatch(request);

        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
        QJsonObject obj = doc.object();

        REQUIRE(obj[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt() == RpcError::InvalidRequest);
    }

    SECTION("missing method returns invalid request")
    {
        QString request = QStringLiteral(R"({"jsonrpc":"2.0","id":1})");
        QString response = dispatcher.dispatch(request);

        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
        QJsonObject obj = doc.object();

        REQUIRE(obj[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt() == RpcError::InvalidRequest);
    }

    SECTION("unknown handler prefix returns method not found")
    {
        QString request = QStringLiteral(R"({"jsonrpc":"2.0","method":"unknown.echo","id":1})");
        QString response = dispatcher.dispatch(request);

        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
        QJsonObject obj = doc.object();

        REQUIRE(obj[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt() == RpcError::MethodNotFound);
    }

    SECTION("method without prefix returns method not found")
    {
        QString request = QStringLiteral(R"({"jsonrpc":"2.0","method":"noDotInMethod","id":1})");
        QString response = dispatcher.dispatch(request);

        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
        QJsonObject obj = doc.object();

        REQUIRE(obj[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt() == RpcError::MethodNotFound);
    }
}

TEST_CASE("RpcDispatcher handles notifications", "[rpc][dispatcher]")
{
    RpcDispatcher dispatcher;
    auto *handler = new MockRpcHandler(QStringLiteral("test"));
    dispatcher.registerHandler(handler);

    SECTION("notification without id is processed")
    {
        QString request = QStringLiteral(R"({"jsonrpc":"2.0","method":"test.echo","params":{"msg":"notify"}})");
        QString response = dispatcher.dispatch(request);

        // Notifications should still be processed
        REQUIRE(handler->lastMethod() == QStringLiteral("echo"));

        // Response for notification should be empty or minimal
        // (behavior depends on implementation - some return nothing, some return result)
    }
}

TEST_CASE("RpcDispatcher preserves request id type", "[rpc][dispatcher]")
{
    RpcDispatcher dispatcher;
    dispatcher.registerHandler(new MockRpcHandler(QStringLiteral("test")));

    SECTION("integer id")
    {
        QString request = QStringLiteral(R"({"jsonrpc":"2.0","method":"test.echo","params":{},"id":42})");
        QString response = dispatcher.dispatch(request);

        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
        REQUIRE(doc.object()[QStringLiteral("id")].toInt() == 42);
    }

    SECTION("string id")
    {
        QString request = QStringLiteral(R"({"jsonrpc":"2.0","method":"test.echo","params":{},"id":"request-123"})");
        QString response = dispatcher.dispatch(request);

        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
        REQUIRE(doc.object()[QStringLiteral("id")].toString() == QStringLiteral("request-123"));
    }

    SECTION("null id")
    {
        QString request = QStringLiteral(R"({"jsonrpc":"2.0","method":"test.echo","params":{},"id":null})");
        QString response = dispatcher.dispatch(request);

        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
        REQUIRE(doc.object()[QStringLiteral("id")].isNull());
    }
}

// =============================================================================
// RpcNotifier Tests
// =============================================================================

TEST_CASE("RpcNotifier manages subscriptions", "[rpc][notifier]")
{
    RpcNotifier notifier;

    SECTION("subscribe to event")
    {
        REQUIRE(notifier.subscribe(QStringLiteral("project.opened")));
        REQUIRE(notifier.isSubscribed(QStringLiteral("project.opened")));
        REQUIRE(!notifier.isSubscribed(QStringLiteral("project.closed")));
    }

    SECTION("subscribe returns false if already subscribed")
    {
        REQUIRE(notifier.subscribe(QStringLiteral("project.opened")));
        REQUIRE(!notifier.subscribe(QStringLiteral("project.opened")));
    }

    SECTION("unsubscribe from event")
    {
        notifier.subscribe(QStringLiteral("project.opened"));
        REQUIRE(notifier.unsubscribe(QStringLiteral("project.opened")));
        REQUIRE(!notifier.isSubscribed(QStringLiteral("project.opened")));
    }

    SECTION("unsubscribe returns false if not subscribed")
    {
        REQUIRE(!notifier.unsubscribe(QStringLiteral("project.opened")));
    }

    SECTION("clear all subscriptions")
    {
        notifier.subscribe(QStringLiteral("project.opened"));
        notifier.subscribe(QStringLiteral("project.closed"));
        notifier.clearSubscriptions();

        REQUIRE(notifier.subscriptions().isEmpty());
    }

    SECTION("get subscriptions list")
    {
        notifier.subscribe(QStringLiteral("project.opened"));
        notifier.subscribe(QStringLiteral("timeline.changed"));

        QSet<QString> subs = notifier.subscriptions();
        REQUIRE(subs.size() == 2);
        REQUIRE(subs.contains(QStringLiteral("project.opened")));
        REQUIRE(subs.contains(QStringLiteral("timeline.changed")));
    }
}

TEST_CASE("RpcNotifier lists available event types", "[rpc][notifier]")
{
    QStringList events = RpcNotifier::availableEventTypes();

    // Check that common events are present
    REQUIRE(events.contains(QStringLiteral("project.opened")));
    REQUIRE(events.contains(QStringLiteral("project.closed")));
    REQUIRE(events.contains(QStringLiteral("project.saved")));
    REQUIRE(events.contains(QStringLiteral("project.modified")));
    REQUIRE(events.contains(QStringLiteral("timeline.changed")));
    REQUIRE(events.contains(QStringLiteral("render.started")));
    REQUIRE(events.contains(QStringLiteral("render.progress")));
    REQUIRE(events.contains(QStringLiteral("render.completed")));
    REQUIRE(events.contains(QStringLiteral("render.error")));
}
