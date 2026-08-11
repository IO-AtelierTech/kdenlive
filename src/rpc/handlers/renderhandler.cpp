/*
    SPDX-License-Identifier: GPL-3.0-only
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
*/

#include "renderhandler.h"
#include "../rpcnotifier.h"

#include "bin/model/markerlistmodel.hpp"
#include "core.h"
#include "dialogs/renderwidget.h"
#include "doc/kdenlivedoc.h"
#include "mainwindow.h"
#include "project/projectmanager.h"
#include "render/renderrequest.h"
#include "renderpresets/renderpresetmodel.hpp"
#include "renderpresets/renderpresetrepository.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>

#include "kdenlivesettings.h"

RenderHandler::RenderHandler(RpcNotifier *notifier, QObject *parent)
    : QObject(parent)
    , m_notifier(notifier)
{
}

RenderHandler::~RenderHandler() = default;

auto RenderHandler::prefix() const -> QString
{
    return QStringLiteral("render");
}

auto RenderHandler::supportedMethods() const -> QStringList
{
    return QStringList{QStringLiteral("getPresets"),   QStringLiteral("getPresetInfo"), QStringLiteral("start"),     QStringLiteral("startWithGuides"),
                       QStringLiteral("stop"),         QStringLiteral("stopAll"),       QStringLiteral("getStatus"), QStringLiteral("getJobs"),
                       QStringLiteral("getActiveJob"), QStringLiteral("setOutput")};
}

auto RenderHandler::handle(const QString &method, const QJsonObject &params) -> QJsonObject
{
    if (method == QLatin1String("getPresets")) {
        return handleGetPresets(params);
    }
    if (method == QLatin1String("getPresetInfo")) {
        return handleGetPresetInfo(params);
    }
    if (method == QLatin1String("start")) {
        return handleStart(params);
    }
    if (method == QLatin1String("startWithGuides")) {
        return handleStartWithGuides(params);
    }
    if (method == QLatin1String("stop")) {
        return handleStop(params);
    }
    if (method == QLatin1String("stopAll")) {
        return handleStopAll(params);
    }
    if (method == QLatin1String("getStatus")) {
        return handleGetStatus(params);
    }
    if (method == QLatin1String("getJobs")) {
        return handleGetJobs(params);
    }
    if (method == QLatin1String("getActiveJob")) {
        return handleGetActiveJob(params);
    }
    if (method == QLatin1String("setOutput")) {
        return handleSetOutput(params);
    }

    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::MethodNotFound},
                                                             {QStringLiteral("message"), QStringLiteral("Unknown method: render.%1").arg(method)}}}};
}

auto RenderHandler::makeProjectNotOpenError() -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ProjectNotOpen},
                                                             {QStringLiteral("message"), QStringLiteral("No project is currently open")}}}};
}

auto RenderHandler::makeApplicationClosingError() -> QJsonObject
{
    return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::ApplicationClosing},
                                                             {QStringLiteral("message"), QStringLiteral("Application is shutting down")}}}};
}

auto RenderHandler::handleGetPresets(const QJsonObject & /*params*/) -> QJsonObject
{
    QJsonArray presets;

    auto presetNames = RenderPresetRepository::get()->getAllPresets();
    for (const QString &name : presetNames) {
        if (!RenderPresetRepository::get()->presetExists(name)) {
            continue;
        }
        auto &preset = RenderPresetRepository::get()->getPreset(name);

        QJsonObject presetObj;
        presetObj[QStringLiteral("name")] = preset->name();
        presetObj[QStringLiteral("extension")] = preset->extension();
        presetObj[QStringLiteral("group")] = preset->groupId();

        presets.append(presetObj);
    }

    return QJsonObject{{QStringLiteral("result"), presets}};
}

auto RenderHandler::handleGetPresetInfo(const QJsonObject &params) -> QJsonObject
{
    QString presetName = params.value(QStringLiteral("presetName")).toString();

    if (presetName.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'presetName' parameter")}}}};
    }

    if (!RenderPresetRepository::get()->presetExists(presetName)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Preset not found: %1").arg(presetName)}}}};
    }

    auto &preset = RenderPresetRepository::get()->getPreset(presetName);

    QJsonObject result;
    result[QStringLiteral("name")] = preset->name();
    result[QStringLiteral("extension")] = preset->extension();
    result[QStringLiteral("group")] = preset->groupId();
    result[QStringLiteral("note")] = preset->note();
    result[QStringLiteral("standard")] = preset->standard();
    result[QStringLiteral("params")] = preset->params().toString();

    // Speed options
    QJsonArray speeds;
    for (const QString &speed : preset->speeds()) {
        speeds.append(speed);
    }
    result[QStringLiteral("speeds")] = speeds;
    result[QStringLiteral("defaultSpeedIndex")] = preset->defaultSpeedIndex();

    // Bitrate options
    QJsonArray videoBitrates;
    for (const QString &bitrate : preset->videoBitrates()) {
        videoBitrates.append(bitrate);
    }
    result[QStringLiteral("videoBitrates")] = videoBitrates;
    result[QStringLiteral("defaultVideoBitrate")] = preset->defaultVBitrate();

    QJsonArray audioBitrates;
    for (const QString &bitrate : preset->audioBitrates()) {
        audioBitrates.append(bitrate);
    }
    result[QStringLiteral("audioBitrates")] = audioBitrates;
    result[QStringLiteral("defaultAudioBitrate")] = preset->defaultABitrate();

    // Quality options
    QJsonArray videoQualities;
    for (const QString &quality : preset->videoQualities()) {
        videoQualities.append(quality);
    }
    result[QStringLiteral("videoQualities")] = videoQualities;
    result[QStringLiteral("defaultVideoQuality")] = preset->defaultVQuality();

    QJsonArray audioQualities;
    for (const QString &quality : preset->audioQualities()) {
        audioQualities.append(quality);
    }
    result[QStringLiteral("audioQualities")] = audioQualities;
    result[QStringLiteral("defaultAudioQuality")] = preset->defaultAQuality();

    return QJsonObject{{QStringLiteral("result"), result}};
}

auto RenderHandler::handleStart(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    QString outputPath = params.value(QStringLiteral("outputPath")).toString();
    QString presetName = params.value(QStringLiteral("preset")).toString();
    int inPoint = params.value(QStringLiteral("inPoint")).toInt(-1);
    int outPoint = params.value(QStringLiteral("outPoint")).toInt(-1);

    if (outputPath.isEmpty() || presetName.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'outputPath' or 'preset' parameter")}}}};
    }

    if (!RenderPresetRepository::get()->presetExists(presetName)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Preset not found: %1").arg(presetName)}}}};
    }

    // Check if render is already in progress
    MainWindow *mw = pCore->window();
    if (!mw) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Main window not available")}}}};
    }

    // Create a RenderRequest and start the render
    RenderRequest request;
    request.setOutputFile(outputPath);
    request.loadPresetParams(presetName);

    if (inPoint >= 0 && outPoint >= 0) {
        request.setBounds(inPoint, outPoint);
    }

    auto jobs = request.process();

    if (jobs.empty()) {
        QStringList errors = request.errorMessages();
        QString errorMsg = errors.isEmpty() ? QStringLiteral("Failed to create render job") : errors.join(QStringLiteral("; "));
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed}, {QStringLiteral("message"), errorMsg}}}};
    }

    // Use the output path as job ID
    QString jobId = outputPath;

    // Actually start the render process for each job
    for (const auto &job : jobs) {
        QStringList rendererArgs = RenderRequest::argsByJob(job, true); // true = add PID for callback

        QProcess proc;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        if (!KdenliveSettings::hwDecoding().isEmpty()) {
            env.insert(QLatin1String("MLT_AVFORMAT_HWACCEL"), KdenliveSettings::hwDecoding());
        }
        proc.setProgram(KdenliveSettings::kdenliverendererpath());
        proc.setProcessEnvironment(env);
        proc.setArguments(rendererArgs);

        if (!proc.startDetached()) {
            return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                     {QStringLiteral("message"), QStringLiteral("Failed to start render process")}}}};
        }
    }

    // Notify that render started
    m_notifier->notify(QStringLiteral("render.started"), QJsonObject{{QStringLiteral("jobId"), jobId}, {QStringLiteral("outputPath"), outputPath}});

    return QJsonObject{
        {QStringLiteral("result"),
         QJsonObject{{QStringLiteral("jobId"), jobId}, {QStringLiteral("outputPath"), outputPath}, {QStringLiteral("status"), QStringLiteral("rendering")}}}};
}

auto RenderHandler::handleStartWithGuides(const QJsonObject &params) -> QJsonObject
{
    // Check if application is shutting down
    if (pCore->closing) {
        return makeApplicationClosingError();
    }

    KdenliveDoc *doc = pCore->currentDoc();
    if (!doc || doc->closing) {
        return makeProjectNotOpenError();
    }

    // Accept both 'outputDir' (client sends this) and 'outputPath' (legacy)
    QString outputPath = params.value(QStringLiteral("outputDir")).toString();
    if (outputPath.isEmpty()) {
        outputPath = params.value(QStringLiteral("outputPath")).toString();
    }
    QString presetName = params.value(QStringLiteral("preset")).toString();
    int guideCategory = params.value(QStringLiteral("guideCategory")).toInt(-1);

    if (outputPath.isEmpty() || presetName.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'outputDir' or 'preset' parameter")}}}};
    }

    if (!RenderPresetRepository::get()->presetExists(presetName)) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Preset not found: %1").arg(presetName)}}}};
    }

    // Get guides model from timeline
    auto timeline = doc->getTimeline(pCore->currentTimelineId());
    if (!timeline) {
        return makeProjectNotOpenError();
    }

    RenderRequest request;
    request.setOutputFile(outputPath);
    request.loadPresetParams(presetName);

    // Enable multi-export with guides
    std::weak_ptr<MarkerListModel> guidesModel = timeline->getGuideModel();
    request.setGuideParams(guidesModel, true, guideCategory);

    auto jobs = request.process();

    if (jobs.empty()) {
        QStringList errors = request.errorMessages();
        QString errorMsg = errors.isEmpty() ? QStringLiteral("Failed to create render jobs") : errors.join(QStringLiteral("; "));
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed}, {QStringLiteral("message"), errorMsg}}}};
    }

    QJsonArray jobArray;
    for (const auto &job : jobs) {
        // Actually start the render process
        QStringList rendererArgs = RenderRequest::argsByJob(job, true); // true = add PID for callback

        QProcess proc;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        if (!KdenliveSettings::hwDecoding().isEmpty()) {
            env.insert(QLatin1String("MLT_AVFORMAT_HWACCEL"), KdenliveSettings::hwDecoding());
        }
        proc.setProgram(KdenliveSettings::kdenliverendererpath());
        proc.setProcessEnvironment(env);
        proc.setArguments(rendererArgs);

        if (!proc.startDetached()) {
            return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                     {QStringLiteral("message"), QStringLiteral("Failed to start render process")}}}};
        }

        QJsonObject jobObj;
        jobObj[QStringLiteral("jobId")] = job.outputPath;
        jobObj[QStringLiteral("outputPath")] = job.outputPath;
        jobArray.append(jobObj);

        // Notify for each job
        m_notifier->notify(QStringLiteral("render.started"),
                           QJsonObject{{QStringLiteral("jobId"), job.outputPath}, {QStringLiteral("outputPath"), job.outputPath}});
    }

    // Return list of job objects directly (client expects list, not wrapped)
    return QJsonObject{{QStringLiteral("result"), jobArray}};
}

auto RenderHandler::handleStop(const QJsonObject &params) -> QJsonObject
{
    QString jobId = params.value(QStringLiteral("jobId")).toString();

    if (jobId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'jobId' parameter")}}}};
    }

    MainWindow *mw = pCore->window();
    if (!mw) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Main window not available")}}}};
    }

    // Call MainWindow's abort slot directly
    mw->abortRenderJob(jobId);

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("stopped"), true}, {QStringLiteral("jobId"), jobId}}}};
}

auto RenderHandler::handleStopAll(const QJsonObject & /*params*/) -> QJsonObject
{
    MainWindow *mw = pCore->window();
    if (!mw) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Main window not available")}}}};
    }

    RenderWidget *rw = mw->renderWidget();
    if (rw) {
        rw->slotAbortCurrentJob();
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("stopped"), true}}}};
}

auto RenderHandler::handleGetStatus(const QJsonObject &params) -> QJsonObject
{
    QString jobId = params.value(QStringLiteral("jobId")).toString();

    if (jobId.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'jobId' parameter")}}}};
    }

    MainWindow *mw = pCore->window();
    if (!mw) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Main window not available")}}}};
    }

    // Return a basic status - the RenderWidget manages detailed job status internally
    QJsonObject result;
    result[QStringLiteral("jobId")] = jobId;

    RenderWidget *rw = mw->renderWidget();
    if (rw && rw->isRendering()) {
        result[QStringLiteral("status")] = QStringLiteral("rendering");
    } else {
        result[QStringLiteral("status")] = QStringLiteral("idle");
    }

    return QJsonObject{{QStringLiteral("result"), result}};
}

auto RenderHandler::handleGetJobs(const QJsonObject & /*params*/) -> QJsonObject
{
    MainWindow *mw = pCore->window();
    if (!mw) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Main window not available")}}}};
    }

    QJsonArray jobs;

    RenderWidget *rw = mw->renderWidget();
    if (rw) {
        int waiting = rw->waitingJobsCount();
        int running = rw->runningJobsCount();

        QJsonObject statusObj;
        statusObj[QStringLiteral("waitingJobs")] = waiting;
        statusObj[QStringLiteral("runningJobs")] = running;
        statusObj[QStringLiteral("isRendering")] = rw->isRendering();
        jobs.append(statusObj);
    }

    return QJsonObject{{QStringLiteral("result"), jobs}};
}

auto RenderHandler::handleGetActiveJob(const QJsonObject & /*params*/) -> QJsonObject
{
    MainWindow *mw = pCore->window();
    if (!mw) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Main window not available")}}}};
    }

    RenderWidget *rw = mw->renderWidget();
    if (!rw || !rw->isRendering()) {
        return QJsonObject{{QStringLiteral("result"), QJsonValue::Null}};
    }

    QJsonObject result;
    result[QStringLiteral("isRendering")] = true;
    result[QStringLiteral("waitingJobs")] = rw->waitingJobsCount();
    result[QStringLiteral("runningJobs")] = rw->runningJobsCount();

    return QJsonObject{{QStringLiteral("result"), result}};
}

auto RenderHandler::handleSetOutput(const QJsonObject &params) -> QJsonObject
{
    // Accept both 'defaultPath' (client sends this) and 'path' (legacy)
    QString path = params.value(QStringLiteral("defaultPath")).toString();
    if (path.isEmpty()) {
        path = params.value(QStringLiteral("path")).toString();
    }

    if (path.isEmpty()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidParams},
                                                                 {QStringLiteral("message"), QStringLiteral("Missing 'defaultPath' or 'path' parameter")}}}};
    }

    // Validate path
    QFileInfo fileInfo(path);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::InvalidPath},
                                                                 {QStringLiteral("message"), QStringLiteral("Output directory does not exist")}}}};
    }

    MainWindow *mw = pCore->window();
    if (!mw) {
        return QJsonObject{{QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), RpcError::OperationFailed},
                                                                 {QStringLiteral("message"), QStringLiteral("Main window not available")}}}};
    }

    RenderWidget *rw = mw->renderWidget();
    if (rw) {
        rw->resetRenderPath(path);
    }

    return QJsonObject{{QStringLiteral("result"), QJsonObject{{QStringLiteral("set"), true}}}};
}
