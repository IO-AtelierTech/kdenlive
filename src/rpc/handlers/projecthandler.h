/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#pragma once

#include "../rpctypes.h"

#include <QObject>

class RpcNotifier;

/**
 * @brief Handler for project-related RPC methods
 *
 * Handles the project.* method namespace:
 * - project.getInfo: Get current project information
 * - project.open: Open a project file
 * - project.save: Save the current project
 * - project.close: Close the current project
 * - project.new: Create a new project
 * - project.undo: Undo last action
 * - project.redo: Redo last action
 */
class ProjectHandler : public QObject, public IRpcHandler
{
    Q_OBJECT

public:
    explicit ProjectHandler(RpcNotifier *notifier, QObject *parent = nullptr);
    ~ProjectHandler() override;

    QJsonObject handle(const QString &method, const QJsonObject &params) override;
    QStringList supportedMethods() const override;
    QString prefix() const override;

private:
    static QJsonObject handleGetInfo(const QJsonObject &params);
    QJsonObject handleOpen(const QJsonObject &params);
    QJsonObject handleSave(const QJsonObject &params);
    QJsonObject handleClose(const QJsonObject &params);
    static QJsonObject handleNew(const QJsonObject &params);
    static QJsonObject handleUndo(const QJsonObject &params);
    static QJsonObject handleRedo(const QJsonObject &params);

    static QJsonObject makeProjectNotOpenError();
    static QJsonObject makeApplicationClosingError();

    RpcNotifier *m_notifier;
};
