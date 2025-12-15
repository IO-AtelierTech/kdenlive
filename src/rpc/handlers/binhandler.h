/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#pragma once

#include "../rpctypes.h"

#include <QObject>

class RpcNotifier;

/**
 * @brief Handler for bin-related RPC methods
 *
 * Handles the bin.* method namespace:
 * - bin.listClips: List all clips in the bin
 * - bin.listFolders: List all folders in the bin
 * - bin.getClipInfo: Get detailed clip information
 * - bin.importClip: Import a clip from URL
 * - bin.importClips: Import multiple clips
 * - bin.deleteClip: Delete a clip
 * - bin.deleteClips: Delete multiple clips
 * - bin.createFolder: Create a folder
 * - bin.deleteFolder: Delete a folder
 * - bin.renameItem: Rename an item
 * - bin.moveItem: Move an item to a folder
 * - bin.getClipMarkers: Get clip markers
 * - bin.addClipMarker: Add a clip marker
 * - bin.deleteClipMarker: Delete a clip marker
 */
class BinHandler : public QObject, public IRpcHandler
{
    Q_OBJECT

public:
    explicit BinHandler(RpcNotifier *notifier, QObject *parent = nullptr);
    ~BinHandler() override;

    QJsonObject handle(const QString &method, const QJsonObject &params) override;
    QStringList supportedMethods() const override;
    QString prefix() const override;

private:
    static QJsonObject handleListClips(const QJsonObject &params);
    static QJsonObject handleListFolders(const QJsonObject &params);
    static QJsonObject handleGetClipInfo(const QJsonObject &params);
    static QJsonObject handleImportClip(const QJsonObject &params);
    static QJsonObject handleImportClips(const QJsonObject &params);
    static QJsonObject handleDeleteClip(const QJsonObject &params);
    static QJsonObject handleDeleteClips(const QJsonObject &params);
    static QJsonObject handleCreateFolder(const QJsonObject &params);
    static QJsonObject handleDeleteFolder(const QJsonObject &params);
    static QJsonObject handleRenameItem(const QJsonObject &params);
    static QJsonObject handleMoveItem(const QJsonObject &params);
    static QJsonObject handleGetClipMarkers(const QJsonObject &params);
    static QJsonObject handleAddClipMarker(const QJsonObject &params);
    static QJsonObject handleDeleteClipMarker(const QJsonObject &params);

    static QJsonObject makeProjectNotOpenError();
    static QJsonObject makeApplicationClosingError();

    RpcNotifier *m_notifier;
};
