/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#pragma once

#include "../rpctypes.h"

#include <QObject>

class RpcNotifier;

/**
 * @brief Handler for timeline-related RPC methods
 *
 * Handles the timeline.* method namespace:
 * - timeline.getInfo: Get timeline information
 * - timeline.getTracks: Get all tracks
 * - timeline.getClips: Get all clips
 * - timeline.getClip: Get single clip info
 * - timeline.insertClip: Insert a clip
 * - timeline.moveClip: Move a clip
 * - timeline.deleteClip: Delete a clip
 * - timeline.deleteClips: Delete multiple clips
 * - timeline.resizeClip: Resize a clip
 * - timeline.splitClip: Split a clip
 * - timeline.seek: Seek to position
 * - timeline.getPosition: Get current position
 * - timeline.addTrack: Add a track
 * - timeline.deleteTrack: Delete a track
 * - timeline.setTrackProperty: Set track property
 * - timeline.getSelection: Get current selection
 * - timeline.setSelection: Set selection
 */
class TimelineHandler : public QObject, public IRpcHandler
{
    Q_OBJECT

public:
    explicit TimelineHandler(RpcNotifier *notifier, QObject *parent = nullptr);
    ~TimelineHandler() override;

    QJsonObject handle(const QString &method, const QJsonObject &params) override;
    QStringList supportedMethods() const override;
    QString prefix() const override;

private:
    QJsonObject handleGetInfo(const QJsonObject &params);
    QJsonObject handleGetTracks(const QJsonObject &params);
    QJsonObject handleGetClips(const QJsonObject &params);
    QJsonObject handleGetClip(const QJsonObject &params);
    QJsonObject handleInsertClip(const QJsonObject &params);
    QJsonObject handleMoveClip(const QJsonObject &params);
    QJsonObject handleDeleteClip(const QJsonObject &params);
    QJsonObject handleDeleteClips(const QJsonObject &params);
    QJsonObject handleResizeClip(const QJsonObject &params);
    QJsonObject handleSplitClip(const QJsonObject &params);
    QJsonObject handleSeek(const QJsonObject &params);
    QJsonObject handleGetPosition(const QJsonObject &params);
    QJsonObject handleAddTrack(const QJsonObject &params);
    QJsonObject handleDeleteTrack(const QJsonObject &params);
    QJsonObject handleSetTrackProperty(const QJsonObject &params);
    QJsonObject handleGetSelection(const QJsonObject &params);
    QJsonObject handleSetSelection(const QJsonObject &params);

    static QJsonObject makeProjectNotOpenError();
    static QJsonObject makeNoTimelineError();
    static QJsonObject makeApplicationClosingError();
    static QJsonObject makeWindowNotAvailableError();

    RpcNotifier *m_notifier;
};
