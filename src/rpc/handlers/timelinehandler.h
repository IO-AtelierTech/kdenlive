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
    static QJsonObject handleGetInfo(const QJsonObject &params);
    static QJsonObject handleGetTracks(const QJsonObject &params);
    static QJsonObject handleGetClips(const QJsonObject &params);
    static QJsonObject handleGetClip(const QJsonObject &params);
    static QJsonObject handleInsertClip(const QJsonObject &params);
    static QJsonObject handleMoveClip(const QJsonObject &params);
    static QJsonObject handleDeleteClip(const QJsonObject &params);
    static QJsonObject handleDeleteClips(const QJsonObject &params);
    static QJsonObject handleResizeClip(const QJsonObject &params);
    static QJsonObject handleSplitClip(const QJsonObject &params);
    static QJsonObject handleSeek(const QJsonObject &params);
    static QJsonObject handleGetPosition(const QJsonObject &params);
    static QJsonObject handleAddTrack(const QJsonObject &params);
    static QJsonObject handleDeleteTrack(const QJsonObject &params);
    static QJsonObject handleSetTrackProperty(const QJsonObject &params);
    static QJsonObject handleGetSelection(const QJsonObject &params);
    static QJsonObject handleSetSelection(const QJsonObject &params);

    static QJsonObject makeProjectNotOpenError();
    static QJsonObject makeNoTimelineError();
    static QJsonObject makeApplicationClosingError();
    static QJsonObject makeWindowNotAvailableError();

    RpcNotifier *m_notifier;
};
