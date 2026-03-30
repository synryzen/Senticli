#include "AppController.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QtEndian>

namespace {
QString expandPath(const QString &raw)
{
    QString path = raw.trimmed();
    if (path.startsWith("~/")) {
        path.replace(0, 1, QDir::homePath());
    }
    return QDir::cleanPath(path);
}

QString shellQuote(const QString &value)
{
    QString escaped = value;
    escaped.replace("'", "'\"'\"'");
    return "'" + escaped + "'";
}
} // namespace

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    m_runtimeClock.start();

    m_streamFlushTimer.setInterval(flushIntervalForProfile());
    m_streamFlushTimer.setSingleShot(false);
    connect(&m_streamFlushTimer, &QTimer::timeout, this, &AppController::flushStreamChunk);

    m_shellTimeoutTimer.setSingleShot(true);
    connect(&m_shellTimeoutTimer, &QTimer::timeout, this, [this]() {
        appendAudit("Shell timeout: " + m_lastShellCommand);
        if (m_shellProcess.state() != QProcess::NotRunning) {
            m_shellProcess.kill();
        }
        setStatusText("Command timed out");
        setFaceState("warning");
    });

    connect(&m_shellProcess, &QProcess::readyReadStandardOutput, this, &AppController::handleShellOutput);
    connect(&m_shellProcess, &QProcess::readyReadStandardError, this, &AppController::handleShellOutput);
    connect(
        &m_shellProcess,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        &AppController::handleShellFinished);

    connect(&m_ttsProcess, &QProcess::started, this, [this]() {
        appendAudit("TTS started");
        m_ttsQueueRunning = true;
        m_echoSuppressUntilMs = m_runtimeClock.elapsed() + echoSuppressStartMs();
        const QString expression = m_speakingExpression;
        int startLeadMs = m_lipSyncDelayMs;
        if (!m_ttsBinary.endsWith("spd-say")) {
            startLeadMs = qMax(90, m_lipSyncDelayMs - 140);
        }
        QTimer::singleShot(startLeadMs, this, [this, expression]() {
            if (m_ttsProcess.state() == QProcess::NotRunning) {
                return;
            }
            setSpeakingActive(true);
            setFaceState(expression);
            setStatusText("Speaking...");
        });
    });

    connect(&m_ttsProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int, QProcess::ExitStatus) {
        appendAudit("TTS finished");
        m_ttsQueueRunning = false;
        m_echoSuppressUntilMs = m_runtimeClock.elapsed() + echoSuppressTailMs();
        if (!m_ttsQueue.isEmpty()) {
            startNextTtsChunk();
            return;
        }
        setSpeakingActive(false);
        if (m_waitingForTtsDrain) {
            m_waitingForTtsDrain = false;
            QTimer::singleShot(80, this, [this]() { finalizeAssistantState(); });
        } else {
            QTimer::singleShot(120, this, [this]() { finalizeAssistantState(); });
        }
    });

    m_voiceCaptureRestartTimer.setSingleShot(true);
    connect(&m_voiceCaptureRestartTimer, &QTimer::timeout, this, [this]() {
        if (m_micActive && m_duplexVoiceEnabled) {
            runVoiceCaptureChunk();
        }
    });

    m_voiceUtteranceTimer.setSingleShot(true);
    connect(&m_voiceUtteranceTimer, &QTimer::timeout, this, [this]() {
        const QString merged = m_voiceUtteranceBuffer.simplified();
        m_voiceUtteranceBuffer.clear();
        if (merged.isEmpty()) {
            return;
        }
        dispatchVoiceTranscript(merged);
    });

    connect(
        &m_voiceCaptureProcess,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        &AppController::handleVoiceCaptureFinished);

    loadSettings();
    m_streamFlushTimer.setInterval(flushIntervalForProfile());
    loadAuditEntries();
    loadProjectMemory();

    if (m_availableModels.isEmpty()) {
        m_availableModels = {"local-prototype"};
    }
    if (!m_availableModels.contains(m_selectedModel)) {
        m_availableModels.append(m_selectedModel);
    }

    m_folderScope = m_grantedFolders.isEmpty() ? "No folders granted" : m_grantedFolders.join(", ");

    m_messageModel.addMessage("system", "Senticli initialized.", "system");
    if (!m_setupComplete) {
        m_messageModel.addMessage(
            "assistant",
            "Welcome. Open AI Settings to choose provider, endpoint, and model.",
            "assistant");
    } else {
        m_messageModel.addMessage("assistant", "Ready. Type /help for commands.", "assistant");
    }

    if (m_duplexVoiceEnabled && m_micActive) {
        startVoiceCaptureLoop();
    }
}

MessageModel *AppController::messageModel()
{
    return &m_messageModel;
}

QString AppController::faceState() const
{
    return m_faceState;
}

QString AppController::statusText() const
{
    return m_statusText;
}

QString AppController::mode() const
{
    return m_mode;
}

bool AppController::cameraEnabled() const
{
    return m_cameraEnabled;
}

bool AppController::cameraAnalyzeRunning() const
{
    return m_cameraAnalyzeRunning;
}

QString AppController::endpoint() const
{
    return m_endpoint;
}

QString AppController::modelsEndpoint() const
{
    return m_modelsEndpoint;
}

QString AppController::apiKey() const
{
    return m_apiKey;
}

QString AppController::provider() const
{
    return m_provider;
}

QStringList AppController::connectionProfiles() const
{
    return m_connectionProfiles;
}

QString AppController::activeProfile() const
{
    return m_activeProfile;
}

QStringList AppController::providers() const
{
    return {"Custom", "LM Studio", "Ollama"};
}

QStringList AppController::availableModels() const
{
    return m_availableModels;
}

QString AppController::selectedModel() const
{
    return m_selectedModel;
}

QString AppController::modelStatus() const
{
    return m_modelStatus;
}

QString AppController::smoothingProfile() const
{
    return m_smoothingProfile;
}

QStringList AppController::smoothingProfiles() const
{
    return {"Instant", "Terminal", "Balanced", "Human", "Cinematic"};
}

int AppController::tokenRate() const
{
    return m_tokenRate;
}

int AppController::lipSyncDelayMs() const
{
    return m_lipSyncDelayMs;
}

QString AppController::assistantName() const
{
    return m_assistantName;
}

bool AppController::wakeEnabled() const
{
    return m_wakeEnabled;
}

QStringList AppController::wakeResponses() const
{
    return m_wakeResponses;
}

bool AppController::conversationalMode() const
{
    return m_conversationalMode;
}

bool AppController::duplexVoiceEnabled() const
{
    return m_duplexVoiceEnabled;
}

QString AppController::transcriptionEndpoint() const
{
    return m_transcriptionEndpoint;
}

QString AppController::transcriptionModel() const
{
    return m_transcriptionModel;
}

int AppController::vadSensitivity() const
{
    return m_vadSensitivity;
}

QString AppController::duplexSmoothness() const
{
    return m_duplexSmoothness;
}

QStringList AppController::duplexSmoothnessOptions() const
{
    return {"Responsive", "Balanced", "Natural", "Studio"};
}

QString AppController::personality() const
{
    return m_personality;
}

QStringList AppController::personalities() const
{
    return {"Helpful", "Professional", "Witty", "Teacher", "Hacker", "Calm"};
}

QString AppController::faceStyle() const
{
    return m_faceStyle;
}

QStringList AppController::faceStyles() const
{
    return {"Loona", "Terminal", "Orb"};
}

QString AppController::gender() const
{
    return m_gender;
}

QStringList AppController::genders() const
{
    return {"Neutral", "Male", "Female"};
}

QString AppController::voiceStyle() const
{
    return m_voiceStyle;
}

QStringList AppController::voiceStyles() const
{
    return {"Default", "Soft", "Bright", "Narrator"};
}

QString AppController::voiceEngine() const
{
    return m_voiceEngine;
}

QStringList AppController::voiceEngines() const
{
    return {"Auto", "Speech Dispatcher", "eSpeak", "Piper"};
}

QString AppController::piperModelPath() const
{
    return m_piperModelPath;
}

bool AppController::ttsEnabled() const
{
    return m_ttsEnabled;
}

bool AppController::memoryEnabled() const
{
    return m_memoryEnabled;
}

QStringList AppController::grantedFolders() const
{
    return m_grantedFolders;
}

QStringList AppController::auditEntries() const
{
    return m_auditEntries;
}

bool AppController::setupComplete() const
{
    return m_setupComplete;
}

QString AppController::modelName() const
{
    if (m_selectedModel == "local-prototype") {
        return "Local Prototype";
    }
    return m_selectedModel;
}

QString AppController::folderScope() const
{
    return m_folderScope;
}

bool AppController::micActive() const
{
    return m_micActive;
}

qreal AppController::voiceInputLevel() const
{
    return m_voiceInputLevel;
}

int AppController::mouthBeatMs() const
{
    return m_mouthBeatMs;
}

bool AppController::speakingActive() const
{
    return m_speakingActive;
}

bool AppController::streamingActive() const
{
    return m_streamingActive;
}

bool AppController::commandRunning() const
{
    return m_commandRunning;
}

bool AppController::actionRunning() const
{
    return m_streamingActive || m_commandRunning || m_cameraAnalyzeRunning;
}

bool AppController::pendingApproval() const
{
    return m_pendingApproval;
}

QString AppController::pendingApprovalText() const
{
    return m_pendingApprovalText;
}

void AppController::captureAndAnalyzeCameraFrame()
{
    if (!m_cameraEnabled) {
        postAssistant("Camera is disabled. Enable it in AI Settings first.", "warning");
        return;
    }

    if (m_cameraAnalyzeRunning || m_activeVisionReply || m_activeChatReply || m_cameraCaptureProcess) {
        postAssistant("Model is busy. Wait for the current response to finish.", "warning");
        return;
    }

    const QString ffmpeg = QStandardPaths::findExecutable("ffmpeg");
    if (ffmpeg.isEmpty()) {
        postAssistant("Camera capture requires ffmpeg. Install ffmpeg and try again.", "warning");
        return;
    }

    const QStringList cameraCandidates = {"/dev/video0", "/dev/video1", "/dev/video2"};
    QString selectedDevice;
    for (const QString &candidate : cameraCandidates) {
        if (QFileInfo::exists(candidate)) {
            selectedDevice = candidate;
            break;
        }
    }

    if (selectedDevice.isEmpty()) {
        postAssistant("No camera device was found (expected /dev/video0).", "warning");
        return;
    }

    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString framePath = tempDir + QString("/senticli_frame_%1.png")
                                            .arg(QDateTime::currentMSecsSinceEpoch());

    setFaceState("thinking");
    setStatusText("Capturing camera frame...");
    setModelStatus("Capturing frame...");
    appendAudit("Camera frame capture started");
    setCameraAnalyzeRunning(true);

    QProcess *capture = new QProcess(this);
    m_cameraCaptureProcess = capture;
    capture->setProgram(ffmpeg);
    capture->setArguments({
        "-hide_banner",
        "-loglevel",
        "error",
        "-y",
        "-f",
        "video4linux2",
        "-i",
        selectedDevice,
        "-frames:v",
        "1",
        "-vf",
        "scale=640:360",
        framePath,
    });

    QTimer::singleShot(6000, this, [this, capture]() {
        if (capture == m_cameraCaptureProcess && capture->state() != QProcess::NotRunning) {
            appendAudit("Camera capture timed out");
            capture->kill();
        }
    });

    connect(capture, &QProcess::errorOccurred, this, [this, capture](QProcess::ProcessError) {
        if (capture != m_cameraCaptureProcess) {
            return;
        }
        m_cameraCaptureProcess = nullptr;
        setCameraAnalyzeRunning(false);
        setFaceState("warning");
        setStatusText("Camera capture failed");
        setModelStatus("Camera capture failed");
        const QString detail = capture->errorString().trimmed();
        appendAudit("Camera capture failed: " + (detail.isEmpty() ? QString("unknown error") : detail));
        postAssistant(
            "Camera capture failed. "
                + (detail.isEmpty() ? QString("Unable to access camera device.") : detail),
            "warning");
        capture->deleteLater();
    });

    connect(
        capture,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        [this, capture, framePath](int exitCode, QProcess::ExitStatus exitStatus) {
            if (capture != m_cameraCaptureProcess) {
                capture->deleteLater();
                return;
            }

            m_cameraCaptureProcess = nullptr;
            const QString stderrText = QString::fromUtf8(capture->readAllStandardError()).trimmed();
            const bool ok = exitStatus == QProcess::NormalExit && exitCode == 0 && QFileInfo::exists(framePath);

            if (!ok) {
                setCameraAnalyzeRunning(false);
                setFaceState("warning");
                setStatusText("Camera capture failed");
                setModelStatus("Camera capture failed");
                const QString detail = stderrText.isEmpty() ? "unknown error" : stderrText;
                appendAudit("Camera capture failed: " + detail);
                postAssistant("Camera capture failed. " + detail, "warning");
                capture->deleteLater();
                return;
            }

            setCameraAnalyzeRunning(false);
            appendAudit("Camera frame capture complete");
            capture->deleteLater();
            analyzeCameraFrameFile(framePath);
        });

    capture->start();
}

void AppController::analyzeCameraFrameFile(const QString &filePath)
{
    QString path = filePath.trimmed();
    if (path.startsWith("file://")) {
        path = QUrl(path).toLocalFile();
    }
    if (path.isEmpty()) {
        postAssistant("Camera frame path was empty.", "warning");
        return;
    }

    if (!m_cameraEnabled) {
        postAssistant("Camera is disabled. Enable it in AI Settings first.", "warning");
        return;
    }

    if (m_activeVisionReply || m_activeChatReply) {
        postAssistant("Model is busy. Wait for the current response to finish.", "warning");
        return;
    }

    if (!shouldUseRemoteModel()) {
        postAssistant("Camera analysis requires a remote vision-capable model.", "warning");
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        postAssistant("Unable to read the captured camera frame.", "warning");
        return;
    }
    const QByteArray imageBytes = file.readAll();
    if (imageBytes.isEmpty()) {
        postAssistant("Captured frame was empty.", "warning");
        return;
    }

    const QString completionUrl = normalizedCompletionUrl(m_endpoint);
    if (completionUrl.isEmpty()) {
        postAssistant("The configured endpoint is invalid.", "warning");
        setModelStatus("Invalid endpoint");
        return;
    }

    m_messageModel.addMessage("user", "[camera] Analyze current frame", "user");
    setFaceState("thinking");
    setStatusText("Analyzing camera frame...");
    setModelStatus("Sending frame to vision model...");
    appendAudit("Camera frame analysis started");
    setCameraAnalyzeRunning(true);

    const QString dataUrl = "data:image/png;base64," + QString::fromLatin1(imageBytes.toBase64());

    QJsonArray messages;
    messages.append(QJsonObject{
        {"role", "system"},
        {"content", "You are a vision companion. Describe what is visible and highlight notable objects, posture, emotion, or gestures. Keep it concise and practical."},
    });

    QJsonArray content;
    content.append(QJsonObject{
        {"type", "text"},
        {"text", "Analyze this camera frame and respond briefly with what you see."},
    });
    content.append(QJsonObject{
        {"type", "image_url"},
        {"image_url", QJsonObject{{"url", dataUrl}}},
    });
    messages.append(QJsonObject{
        {"role", "user"},
        {"content", content},
    });

    QJsonObject payload;
    payload.insert("model", m_selectedModel);
    payload.insert("stream", false);
    payload.insert("max_tokens", 320);
    payload.insert("messages", messages);

    QNetworkRequest request{QUrl(completionUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    applyAuthHeader(request);

    m_activeVisionReply = m_network.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QNetworkReply *reply = m_activeVisionReply;

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply != m_activeVisionReply) {
            reply->deleteLater();
            return;
        }
        m_activeVisionReply = nullptr;
        setCameraAnalyzeRunning(false);

        if (reply->error() != QNetworkReply::NoError) {
            if (reply->error() == QNetworkReply::OperationCanceledError) {
                setModelStatus("Camera analysis canceled");
                appendAudit("Camera analysis canceled");
                finalizeAssistantState();
                reply->deleteLater();
                return;
            }
            postAssistant("Camera analysis failed: " + reply->errorString(), "warning");
            setModelStatus("Camera analysis failed");
            appendAudit("Camera analysis failed: " + reply->errorString());
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            postAssistant("Camera analysis returned an invalid response.", "warning");
            setModelStatus("Invalid vision response");
            appendAudit("Camera analysis invalid response");
            reply->deleteLater();
            return;
        }

        const QJsonArray choices = doc.object().value("choices").toArray();
        QString answer;
        if (!choices.isEmpty()) {
            answer = extractContentFromChoice(choices.first().toObject()).trimmed();
        }

        if (answer.isEmpty()) {
            postAssistant("I could not extract a visual description from the model.", "warning");
            setModelStatus("No vision text returned");
            appendAudit("Camera analysis empty response");
            reply->deleteLater();
            return;
        }

        postAssistant(answer);
        setModelStatus("Camera analysis complete");
        appendAudit("Camera analysis complete");
        reply->deleteLater();
    });
}

void AppController::sendUserInput(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    if (m_micActive && !m_duplexVoiceEnabled) {
        m_micActive = false;
        emit micActiveChanged();
    }

    if (m_ttsProcess.state() != QProcess::NotRunning || !m_ttsQueue.isEmpty()) {
        appendAudit("Barge-in: user interrupted speech");
        clearTtsQueue();
        stopSpeaking();
    }

    QString routedText = trimmed;
    bool wakeOnly = false;
    if (parseWakeInput(trimmed, &routedText, &wakeOnly)) {
        if (wakeOnly) {
            m_messageModel.addMessage("user", trimmed, "user");
            setFaceState("listening");
            setStatusText("Listening...");
            if (m_wakeResponses.isEmpty()) {
                postAssistant("How can I help you?");
            } else {
                const int idx = QRandomGenerator::global()->bounded(m_wakeResponses.size());
                postAssistant(m_wakeResponses.at(idx));
            }
            return;
        }
    } else {
        routedText = trimmed;
    }

    m_messageModel.addMessage("user", trimmed, "user");
    setFaceState("thinking");
    setStatusText("Thinking...");

    if (routedText.startsWith('/')) {
        QTimer::singleShot(120, this, [this, routedText]() { handleInput(routedText); });
        return;
    }

    if (shouldUseRemoteModel()) {
        requestRemoteCompletion(routedText);
        return;
    }

    QTimer::singleShot(120, this, [this, routedText]() { handleInput(routedText); });
}

void AppController::toggleMic()
{
    m_micActive = !m_micActive;
    emit micActiveChanged();

    if (m_micActive) {
        setVoiceInputLevel(0.0);
        setFaceState("listening");
        setStatusText(m_duplexVoiceEnabled ? "Duplex listening..." : "Listening (STT integration pending)...");
        if (m_duplexVoiceEnabled) {
            startVoiceCaptureLoop();
        }
    } else if (m_pendingApproval) {
        stopVoiceCaptureLoop();
        setFaceState("warning");
        setStatusText("Waiting for approval");
    } else {
        stopVoiceCaptureLoop();
        setVoiceInputLevel(0.0);
        setFaceState("idle");
        setStatusText("Ready");
    }
}

void AppController::setMode(const QString &mode)
{
    static const QStringList allowed = {"Chat", "Assist", "Command"};
    if (!allowed.contains(mode) || m_mode == mode) {
        return;
    }

    m_mode = mode;
    emit modeChanged();
    m_messageModel.addMessage("system", QString("Mode set to %1").arg(mode), "system");
}

void AppController::setCameraEnabled(bool enabled)
{
    if (m_cameraEnabled == enabled) {
        return;
    }

    m_cameraEnabled = enabled;
    emit cameraEnabledChanged();
    setModelStatus(m_cameraEnabled ? "Camera enabled" : "Camera disabled");
    appendAudit(QString("Camera %1").arg(m_cameraEnabled ? "enabled" : "disabled"));
    saveSettings();
}

void AppController::setEndpoint(const QString &endpoint)
{
    const QString normalized = normalizedCompletionUrl(endpoint);
    if (normalized.isEmpty() || m_endpoint == normalized) {
        return;
    }

    m_endpoint = normalized;
    emit endpointChanged();
    setModelStatus("Endpoint updated");
    m_messageModel.addMessage("system", QString("Endpoint: %1").arg(m_endpoint), "system");
    appendAudit("Endpoint changed: " + m_endpoint);
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setModelsEndpoint(const QString &modelsEndpoint)
{
    const QString trimmed = modelsEndpoint.trimmed();
    const QString normalized = normalizedModelsOverrideUrl(modelsEndpoint);
    if (!trimmed.isEmpty() && normalized.isEmpty()) {
        setModelStatus("Invalid models endpoint");
        return;
    }

    if (m_modelsEndpoint == normalized) {
        return;
    }

    m_modelsEndpoint = normalized;
    emit modelsEndpointChanged();
    setModelStatus(
        m_modelsEndpoint.isEmpty() ? "Models endpoint reset to auto (/v1/models)"
                                   : QString("Models endpoint: %1").arg(m_modelsEndpoint));
    appendAudit(
        m_modelsEndpoint.isEmpty() ? "Models endpoint reset to auto"
                                   : QString("Models endpoint changed: %1").arg(m_modelsEndpoint));
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setApiKey(const QString &apiKey)
{
    const QString trimmed = apiKey.trimmed();
    if (m_apiKey == trimmed) {
        return;
    }

    m_apiKey = trimmed;
    emit apiKeyChanged();
    setModelStatus(m_apiKey.isEmpty() ? "API key cleared" : "API key updated");
    appendAudit(m_apiKey.isEmpty() ? "API key cleared" : "API key updated");
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setProvider(const QString &provider)
{
    const QString trimmed = provider.trimmed();
    if (!providers().contains(trimmed) || m_provider == trimmed) {
        return;
    }

    m_provider = trimmed;
    emit providerChanged();

    if (m_provider == "LM Studio") {
        m_endpoint = "http://127.0.0.1:1234/v1/chat/completions";
        emit endpointChanged();
    } else if (m_provider == "Ollama") {
        m_endpoint = "http://127.0.0.1:11434/v1/chat/completions";
        emit endpointChanged();
    }

    setModelStatus(QString("Provider set to %1").arg(m_provider));
    m_messageModel.addMessage("system", QString("Provider: %1").arg(m_provider), "system");
    appendAudit("Provider changed: " + m_provider);
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setSelectedModel(const QString &model)
{
    const QString trimmed = model.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    if (!m_availableModels.contains(trimmed)) {
        m_availableModels.append(trimmed);
        emit availableModelsChanged();
    }

    if (m_selectedModel == trimmed) {
        return;
    }

    m_selectedModel = trimmed;
    emit selectedModelChanged();
    emit modelNameChanged();
    setModelStatus(QString("Active model: %1").arg(m_selectedModel));
    appendAudit("Model selected: " + m_selectedModel);
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setActiveProfile(const QString &profileName)
{
    const QString trimmed = profileName.trimmed();
    if (trimmed.isEmpty() || !m_connectionProfiles.contains(trimmed)) {
        return;
    }

    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }

    if (m_activeProfile != trimmed) {
        m_activeProfile = trimmed;
        emit activeProfileChanged();
    }

    loadProfile(trimmed);
    if (m_micActive) {
        if (m_duplexVoiceEnabled) {
            startVoiceCaptureLoop();
        } else {
            stopVoiceCaptureLoop();
        }
    }
    saveSettings();
    setModelStatus(QString("Loaded profile: %1").arg(trimmed));
    appendAudit(QString("Profile loaded: %1").arg(trimmed));
    m_messageModel.addMessage("system", QString("Profile loaded: %1").arg(trimmed), "system");
}

void AppController::saveCurrentProfile(const QString &profileName)
{
    const QString trimmed = profileName.trimmed();
    if (trimmed.isEmpty()) {
        setModelStatus("Profile name is required");
        return;
    }

    if (!m_connectionProfiles.contains(trimmed)) {
        m_connectionProfiles.append(trimmed);
        emit connectionProfilesChanged();
    }

    if (m_activeProfile != trimmed) {
        m_activeProfile = trimmed;
        emit activeProfileChanged();
    }

    saveProfileToSettings(trimmed);
    saveSettings();
    setModelStatus(QString("Profile saved: %1").arg(trimmed));
    appendAudit(QString("Profile saved: %1").arg(trimmed));
    m_messageModel.addMessage("system", QString("Profile saved: %1").arg(trimmed), "system");
}

void AppController::deleteProfile(const QString &profileName)
{
    const QString trimmed = profileName.trimmed();
    if (trimmed.isEmpty() || !m_connectionProfiles.contains(trimmed)) {
        return;
    }

    QSettings settings;
    settings.remove(QString("profiles/%1").arg(trimmed));

    m_connectionProfiles.removeAll(trimmed);
    if (m_connectionProfiles.isEmpty()) {
        m_connectionProfiles = {"Default"};
        saveProfileToSettings("Default");
    }
    emit connectionProfilesChanged();

    if (m_activeProfile == trimmed) {
        m_activeProfile = m_connectionProfiles.first();
        emit activeProfileChanged();
        loadProfile(m_activeProfile);
    }

    saveSettings();
    setModelStatus(QString("Profile deleted: %1").arg(trimmed));
    appendAudit(QString("Profile deleted: %1").arg(trimmed));
}

void AppController::setSmoothingProfile(const QString &profile)
{
    const QString trimmed = profile.trimmed();
    QString resolved;
    for (const QString &candidate : smoothingProfiles()) {
        if (candidate.compare(trimmed, Qt::CaseInsensitive) == 0) {
            resolved = candidate;
            break;
        }
    }
    if (resolved.isEmpty() || m_smoothingProfile == resolved) {
        return;
    }

    m_smoothingProfile = resolved;
    m_streamFlushTimer.setInterval(flushIntervalForProfile());
    emit smoothingProfileChanged();
    setModelStatus(QString("Smoothing: %1").arg(m_smoothingProfile));
    appendAudit("Smoothing profile: " + m_smoothingProfile);
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setTokenRate(int rate)
{
    const int clamped = qBound(20, rate, 600);
    if (m_tokenRate == clamped) {
        return;
    }

    m_tokenRate = clamped;
    m_streamFlushTimer.setInterval(flushIntervalForProfile());
    emit tokenRateChanged();
    setModelStatus(QString("Token rate: %1%").arg(m_tokenRate));
    appendAudit(QString("Token rate set: %1").arg(m_tokenRate));
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setLipSyncDelayMs(int delayMs)
{
    const int clamped = qBound(0, delayMs, 1100);
    if (m_lipSyncDelayMs == clamped) {
        return;
    }

    m_lipSyncDelayMs = clamped;
    emit lipSyncDelayMsChanged();
    setModelStatus(QString("Lip sync delay: %1 ms").arg(m_lipSyncDelayMs));
    appendAudit(QString("Lip sync delay set: %1 ms").arg(m_lipSyncDelayMs));
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setAssistantName(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || m_assistantName == trimmed) {
        return;
    }

    m_assistantName = trimmed;
    emit assistantNameChanged();
    setModelStatus(QString("Assistant name: %1").arg(m_assistantName));
    appendAudit("Assistant renamed: " + m_assistantName);
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setWakeEnabled(bool enabled)
{
    if (m_wakeEnabled == enabled) {
        return;
    }

    m_wakeEnabled = enabled;
    emit wakeEnabledChanged();
    setModelStatus(m_wakeEnabled ? "Wake phrase enabled" : "Wake phrase disabled");
    appendAudit(QString("Wake phrase %1").arg(m_wakeEnabled ? "enabled" : "disabled"));
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setWakeResponses(const QStringList &responses)
{
    QStringList cleaned;
    for (const QString &item : responses) {
        const QString trimmed = item.trimmed();
        if (!trimmed.isEmpty() && !cleaned.contains(trimmed)) {
            cleaned.append(trimmed);
        }
    }

    if (cleaned.isEmpty()) {
        cleaned = {"How can I help you?",
                   "I am here. What do you need?",
                   "Ready when you are.",
                   "Yep, talk to me.",
                   "What would you like me to do?"};
    }

    if (m_wakeResponses == cleaned) {
        return;
    }

    m_wakeResponses = cleaned;
    emit wakeResponsesChanged();
    setModelStatus(QString("Wake responses updated (%1)").arg(m_wakeResponses.size()));
    appendAudit("Wake responses updated");
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setConversationalMode(bool enabled)
{
    if (m_conversationalMode == enabled) {
        return;
    }

    m_conversationalMode = enabled;
    emit conversationalModeChanged();
    setModelStatus(m_conversationalMode ? "Conversational mode enabled" : "Conversational mode disabled");
    appendAudit(QString("Conversational mode %1").arg(m_conversationalMode ? "enabled" : "disabled"));
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setDuplexVoiceEnabled(bool enabled)
{
    if (m_duplexVoiceEnabled == enabled) {
        return;
    }

    m_duplexVoiceEnabled = enabled;
    emit duplexVoiceEnabledChanged();
    setModelStatus(m_duplexVoiceEnabled ? "Duplex voice beta enabled" : "Duplex voice beta disabled");
    appendAudit(QString("Duplex voice %1").arg(m_duplexVoiceEnabled ? "enabled" : "disabled"));
    if (m_micActive) {
        if (m_duplexVoiceEnabled) {
            startVoiceCaptureLoop();
        } else {
            stopVoiceCaptureLoop();
        }
    }
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setTranscriptionEndpoint(const QString &endpoint)
{
    const QString normalized = normalizedTranscriptionUrl(endpoint);
    if (m_transcriptionEndpoint == normalized) {
        return;
    }

    m_transcriptionEndpoint = normalized;
    emit transcriptionEndpointChanged();
    setModelStatus(m_transcriptionEndpoint.isEmpty() ? "Transcription endpoint: auto"
                                                     : "Transcription endpoint updated");
    appendAudit(m_transcriptionEndpoint.isEmpty() ? "Transcription endpoint auto"
                                                  : "Transcription endpoint changed: " + m_transcriptionEndpoint);
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setTranscriptionModel(const QString &model)
{
    const QString trimmed = model.trimmed();
    if (trimmed.isEmpty() || m_transcriptionModel == trimmed) {
        return;
    }

    m_transcriptionModel = trimmed;
    emit transcriptionModelChanged();
    setModelStatus("Transcription model: " + m_transcriptionModel);
    appendAudit("Transcription model changed: " + m_transcriptionModel);
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setVadSensitivity(int value)
{
    const int clamped = qBound(1, value, 100);
    if (m_vadSensitivity == clamped) {
        return;
    }

    m_vadSensitivity = clamped;
    emit vadSensitivityChanged();
    setModelStatus(QString("VAD sensitivity: %1").arg(m_vadSensitivity));
    appendAudit(QString("VAD sensitivity set: %1").arg(m_vadSensitivity));
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setDuplexSmoothness(const QString &value)
{
    const QString trimmed = value.trimmed();
    QString resolved;
    for (const QString &candidate : duplexSmoothnessOptions()) {
        if (candidate.compare(trimmed, Qt::CaseInsensitive) == 0) {
            resolved = candidate;
            break;
        }
    }

    if (resolved.isEmpty() || m_duplexSmoothness == resolved) {
        return;
    }

    m_duplexSmoothness = resolved;
    emit duplexSmoothnessChanged();
    setModelStatus("Duplex smoothness: " + m_duplexSmoothness);
    appendAudit("Duplex smoothness set: " + m_duplexSmoothness);
    if (m_micActive && m_duplexVoiceEnabled) {
        stopVoiceCaptureLoop();
        startVoiceCaptureLoop();
    }
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

bool AppController::applyDuplexPreset(const QString &presetRaw)
{
    const QString preset = presetRaw.trimmed().toLower();
    if (preset.isEmpty()) {
        return false;
    }

    QString smoothness;
    int sensitivity = m_vadSensitivity;
    QString display;

    if (preset == "fast" || preset == "responsive") {
        smoothness = "Responsive";
        sensitivity = 58;
        display = "Fast";
    } else if (preset == "balanced") {
        smoothness = "Balanced";
        sensitivity = 50;
        display = "Balanced";
    } else if (preset == "human" || preset == "natural") {
        smoothness = "Natural";
        sensitivity = 64;
        display = "Human";
    } else if (preset == "studio") {
        smoothness = "Studio";
        sensitivity = 42;
        display = "Studio";
    } else {
        return false;
    }

    setDuplexSmoothness(smoothness);
    setVadSensitivity(sensitivity);
    setModelStatus(QString("Duplex preset: %1 (%2, VAD %3)")
                       .arg(display, smoothness)
                       .arg(sensitivity));
    appendAudit(QString("Duplex preset applied: %1").arg(display));
    return true;
}

void AppController::setPersonality(const QString &personality)
{
    const QString trimmed = personality.trimmed();
    QString resolved;
    for (const QString &candidate : personalities()) {
        if (candidate.compare(trimmed, Qt::CaseInsensitive) == 0) {
            resolved = candidate;
            break;
        }
    }
    if (resolved.isEmpty() || m_personality == resolved) {
        return;
    }

    m_personality = resolved;
    emit personalityChanged();
    setModelStatus(QString("Personality: %1").arg(m_personality));
    appendAudit("Personality set: " + m_personality);
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setFaceStyle(const QString &faceStyle)
{
    const QString trimmed = faceStyle.trimmed();
    QString resolved;
    for (const QString &candidate : faceStyles()) {
        if (candidate.compare(trimmed, Qt::CaseInsensitive) == 0) {
            resolved = candidate;
            break;
        }
    }

    if (resolved.isEmpty() || m_faceStyle == resolved) {
        return;
    }

    m_faceStyle = resolved;
    emit faceStyleChanged();
    setModelStatus("Face style: " + m_faceStyle);
    appendAudit("Face style set: " + m_faceStyle);
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setGender(const QString &gender)
{
    const QString trimmed = gender.trimmed();
    QString resolved;
    for (const QString &candidate : genders()) {
        if (candidate.compare(trimmed, Qt::CaseInsensitive) == 0) {
            resolved = candidate;
            break;
        }
    }
    if (resolved.isEmpty() || m_gender == resolved) {
        return;
    }

    m_gender = resolved;
    emit genderChanged();
    setModelStatus(QString("Gender voice target: %1").arg(m_gender));
    appendAudit("Gender set: " + m_gender);
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setVoiceStyle(const QString &voiceStyle)
{
    const QString trimmed = voiceStyle.trimmed();
    QString resolved;
    for (const QString &candidate : voiceStyles()) {
        if (candidate.compare(trimmed, Qt::CaseInsensitive) == 0) {
            resolved = candidate;
            break;
        }
    }
    if (resolved.isEmpty() || m_voiceStyle == resolved) {
        return;
    }

    m_voiceStyle = resolved;
    emit voiceStyleChanged();
    setModelStatus(QString("Voice style: %1").arg(m_voiceStyle));
    appendAudit("Voice style set: " + m_voiceStyle);
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setVoiceEngine(const QString &voiceEngine)
{
    const QString trimmed = voiceEngine.trimmed();
    QString resolved;
    for (const QString &candidate : voiceEngines()) {
        if (candidate.compare(trimmed, Qt::CaseInsensitive) == 0) {
            resolved = candidate;
            break;
        }
    }
    if (resolved.isEmpty() || m_voiceEngine == resolved) {
        return;
    }

    m_voiceEngine = resolved;
    m_ttsBinary.clear();
    emit voiceEngineChanged();
    setModelStatus(QString("Voice engine: %1").arg(m_voiceEngine));
    appendAudit("Voice engine set: " + m_voiceEngine);
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setPiperModelPath(const QString &path)
{
    const QString normalized = expandPath(path);
    if (m_piperModelPath == normalized) {
        return;
    }

    m_piperModelPath = normalized;
    emit piperModelPathChanged();
    if (m_piperModelPath.isEmpty()) {
        setModelStatus("Piper model path cleared");
        appendAudit("Piper model path cleared");
    } else {
        setModelStatus("Piper model: " + m_piperModelPath);
        appendAudit("Piper model path set: " + m_piperModelPath);
    }
    if (!m_activeProfile.isEmpty()) {
        saveProfileToSettings(m_activeProfile);
    }
    saveSettings();
}

void AppController::setTtsEnabled(bool enabled)
{
    if (m_ttsEnabled == enabled) {
        return;
    }

    m_ttsEnabled = enabled;
    emit ttsEnabledChanged();
    setModelStatus(m_ttsEnabled ? "Voice output enabled" : "Voice output disabled");
    appendAudit(QString("TTS %1").arg(m_ttsEnabled ? "enabled" : "disabled"));
    saveSettings();
}

void AppController::setMemoryEnabled(bool enabled)
{
    if (m_memoryEnabled == enabled) {
        return;
    }

    m_memoryEnabled = enabled;
    emit memoryEnabledChanged();
    setModelStatus(m_memoryEnabled ? "Project memory enabled" : "Project memory disabled");
    appendAudit(QString("Memory %1").arg(m_memoryEnabled ? "enabled" : "disabled"));
    saveSettings();
}

void AppController::refreshModels()
{
    const QString modelsUrl = m_modelsEndpoint.isEmpty() ? modelsUrlFromEndpoint(m_endpoint)
                                                          : m_modelsEndpoint;
    if (modelsUrl.isEmpty()) {
        setModelStatus("Invalid endpoint");
        return;
    }

    if (m_activeModelsReply) {
        m_activeModelsReply->abort();
        m_activeModelsReply->deleteLater();
        m_activeModelsReply = nullptr;
    }

    QNetworkRequest request{QUrl(modelsUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    applyAuthHeader(request);

    setModelStatus("Loading models...");
    m_activeModelsReply = m_network.get(request);
    QNetworkReply *reply = m_activeModelsReply;

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply != m_activeModelsReply) {
            reply->deleteLater();
            return;
        }

        m_activeModelsReply = nullptr;

        if (reply->error() != QNetworkReply::NoError) {
            setModelStatus(QString("Model fetch failed: %1").arg(reply->errorString()));
            appendAudit("Model refresh failed: " + reply->errorString());
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            setModelStatus("Invalid /v1/models response");
            reply->deleteLater();
            return;
        }

        QStringList newModels = {"local-prototype"};
        const QJsonObject root = doc.object();

        QJsonArray data = root.value("data").toArray();
        if (data.isEmpty() && root.value("models").isArray()) {
            data = root.value("models").toArray();
        }

        for (const QJsonValue &value : data) {
            QString id;
            if (value.isObject()) {
                id = value.toObject().value("id").toString().trimmed();
                if (id.isEmpty()) {
                    id = value.toObject().value("name").toString().trimmed();
                }
            } else if (value.isString()) {
                id = value.toString().trimmed();
            }

            if (!id.isEmpty() && !newModels.contains(id)) {
                newModels.append(id);
            }
        }

        if (m_availableModels != newModels) {
            m_availableModels = newModels;
            emit availableModelsChanged();
        }

        if (!m_availableModels.contains(m_selectedModel)) {
            m_selectedModel = m_availableModels.first();
            emit selectedModelChanged();
            emit modelNameChanged();
        }

        setModelStatus(QString("Loaded %1 model(s)").arg(m_availableModels.size() - 1));
        appendAudit("Model refresh succeeded");
        saveSettings();
        reply->deleteLater();
    });
}

void AppController::testProviderConnection()
{
    const QString modelsUrl = m_modelsEndpoint.isEmpty() ? modelsUrlFromEndpoint(m_endpoint)
                                                          : m_modelsEndpoint;
    if (modelsUrl.isEmpty()) {
        setModelStatus("Invalid endpoint");
        return;
    }

    if (m_activeHealthReply) {
        m_activeHealthReply->abort();
        m_activeHealthReply->deleteLater();
        m_activeHealthReply = nullptr;
    }

    QNetworkRequest request{QUrl(modelsUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    applyAuthHeader(request);

    setModelStatus("Testing connection...");
    m_activeHealthReply = m_network.get(request);
    QNetworkReply *reply = m_activeHealthReply;

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply != m_activeHealthReply) {
            reply->deleteLater();
            return;
        }

        m_activeHealthReply = nullptr;

        if (reply->error() == QNetworkReply::NoError) {
            setModelStatus("Connection OK");
            m_messageModel.addMessage("system", "Provider health check passed.", "system");
            appendAudit("Health check OK");
        } else {
            setModelStatus(QString("Connection failed: %1").arg(reply->errorString()));
            m_messageModel.addMessage("system", "Provider health check failed.", "system");
            appendAudit("Health check failed: " + reply->errorString());
        }

        reply->deleteLater();
    });
}

void AppController::addGrantedFolder(const QString &folder)
{
    const QString normalized = expandPath(folder);
    if (normalized.isEmpty() || m_grantedFolders.contains(normalized)) {
        return;
    }

    m_grantedFolders.append(normalized);
    emit grantedFoldersChanged();
    m_folderScope = m_grantedFolders.join(", ");
    emit folderScopeChanged();

    appendAudit("Folder granted: " + normalized);
    saveSettings();
}

void AppController::removeGrantedFolder(const QString &folder)
{
    const QString normalized = expandPath(folder);
    if (!m_grantedFolders.removeOne(normalized)) {
        return;
    }

    emit grantedFoldersChanged();
    m_folderScope = m_grantedFolders.isEmpty() ? "No folders granted" : m_grantedFolders.join(", ");
    emit folderScopeChanged();

    appendAudit("Folder revoked: " + normalized);
    saveSettings();
}

void AppController::clearAuditEntries()
{
    if (m_auditEntries.isEmpty()) {
        return;
    }

    m_auditEntries.clear();
    emit auditEntriesChanged();
    saveAuditEntries();
    setModelStatus("Audit log cleared");
}

void AppController::completeSetup()
{
    if (m_setupComplete) {
        return;
    }

    m_setupComplete = true;
    emit setupCompleteChanged();
    appendAudit("Setup completed");
    saveSettings();
}

void AppController::cancelActiveRequest()
{
    bool canceledSomething = false;

    if (m_activeChatReply) {
        m_cancelRequested = true;
        m_activeChatReply->abort();
        canceledSomething = true;
    }

    if (m_commandRunning) {
        cancelShellCommand();
        canceledSomething = true;
    }

    if (m_ttsProcess.state() != QProcess::NotRunning || !m_ttsQueue.isEmpty()) {
        clearTtsQueue();
        stopSpeaking();
        canceledSomething = true;
    }

    if (m_activeTranscriptionReply) {
        m_activeTranscriptionReply->abort();
        canceledSomething = true;
    }

    if (m_activeVisionReply) {
        m_activeVisionReply->abort();
        canceledSomething = true;
    }

    if (m_cameraCaptureProcess && m_cameraCaptureProcess->state() != QProcess::NotRunning) {
        m_cameraCaptureProcess->kill();
        canceledSomething = true;
    }

    if (!canceledSomething) {
        setModelStatus("Nothing to cancel");
    } else {
        setStatusText("Canceling active operation...");
        setModelStatus("Cancel requested");
    }
}

void AppController::approvePendingAction(bool approved)
{
    if (!m_pendingApproval) {
        return;
    }

    const QString actionKind = m_pendingActionKind;
    const QString payload = m_pendingPayload;

    m_pendingApproval = false;
    m_pendingApprovalText.clear();
    m_pendingActionKind.clear();
    m_pendingPayload.clear();
    emit pendingApprovalChanged();

    if (!approved) {
        m_messageModel.addMessage("system", "Action canceled.", "system");
        appendAudit("Action canceled by user");
        setFaceState("confused");
        setStatusText("Action canceled");
        QTimer::singleShot(700, this, [this]() { finalizeAssistantState(); });
        return;
    }

    if (actionKind == "open") {
        const bool ok = QProcess::startDetached(payload, {});
        m_messageModel.addMessage(
            "system",
            ok ? QString("Launched %1").arg(payload) : QString("Failed to launch %1").arg(payload),
            "system");
        appendAudit(QString("Open app %1: %2").arg(payload, ok ? "ok" : "failed"));
        setFaceState(ok ? "happy" : "warning");
        setStatusText(ok ? "App launched" : "Launch failed");
    } else if (actionKind == "run") {
        runShellCommandPreview(payload, true);
    } else {
        m_messageModel.addMessage("system", "Approved. Action queued.", "system");
    }
}

void AppController::setFaceState(const QString &state)
{
    if (m_faceState == state) {
        return;
    }
    m_faceState = state;
    emit faceStateChanged();
}

void AppController::setStatusText(const QString &text)
{
    if (m_statusText == text) {
        return;
    }
    m_statusText = text;
    emit statusTextChanged();
}

void AppController::setModelStatus(const QString &text)
{
    if (m_modelStatus == text) {
        return;
    }
    m_modelStatus = text;
    emit modelStatusChanged();
}

void AppController::setVoiceInputLevel(qreal level)
{
    const qreal clamped = qBound<qreal>(0.0, level, 1.0);
    if (qAbs(m_voiceInputLevel - clamped) < 0.01) {
        return;
    }
    m_voiceInputLevel = clamped;
    emit voiceInputLevelChanged();
}

void AppController::setMouthBeatMs(int beatMs)
{
    const int clamped = qBound(110, beatMs, 420);
    if (m_mouthBeatMs == clamped) {
        return;
    }
    m_mouthBeatMs = clamped;
    emit mouthBeatMsChanged();
}

void AppController::setCameraAnalyzeRunning(bool running)
{
    if (m_cameraAnalyzeRunning == running) {
        return;
    }
    const bool beforeAction = actionRunning();
    m_cameraAnalyzeRunning = running;
    emit cameraAnalyzeRunningChanged();
    if (beforeAction != actionRunning()) {
        emit actionRunningChanged();
    }
}

void AppController::setStreamingActive(bool active)
{
    if (m_streamingActive == active) {
        return;
    }
    const bool beforeAction = actionRunning();
    m_streamingActive = active;
    emit streamingActiveChanged();
    if (beforeAction != actionRunning()) {
        emit actionRunningChanged();
    }
}

void AppController::setCommandRunning(bool running)
{
    if (m_commandRunning == running) {
        return;
    }
    const bool beforeAction = actionRunning();
    m_commandRunning = running;
    emit commandRunningChanged();
    if (beforeAction != actionRunning()) {
        emit actionRunningChanged();
    }
}

void AppController::setSpeakingActive(bool active)
{
    if (m_speakingActive == active) {
        return;
    }
    m_speakingActive = active;
    emit speakingActiveChanged();
}

void AppController::updateStreamingMessageDisplay(bool showCursor)
{
    if (m_streamingMessageRow < 0) {
        return;
    }

    QString text = m_streamAccumulatedText;
    if (showCursor) {
        text += QStringLiteral("▌");
    }
    m_messageModel.setTextAt(m_streamingMessageRow, text);
}

void AppController::queueStreamText(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    if (m_smoothingProfile == "Instant") {
        m_streamAccumulatedText += text;
        updateStreamingMessageDisplay(true);
        return;
    }

    m_streamPendingText += text;
    if (!m_streamFlushTimer.isActive()) {
        m_streamFlushTimer.start();
    }
}

void AppController::flushStreamChunk()
{
    if (!m_streamingActive) {
        m_streamFlushTimer.stop();
        return;
    }

    if (!m_streamPendingText.isEmpty()) {
        int chunk = chunkSizeForBacklog(m_streamPendingText.size());
        if (chunk > m_streamPendingText.size()) {
            chunk = m_streamPendingText.size();
        }

        m_streamAccumulatedText += m_streamPendingText.left(chunk);
        m_streamPendingText.remove(0, chunk);
        updateStreamingMessageDisplay(true);
    }

    if (m_streamPendingText.isEmpty()) {
        m_streamFlushTimer.stop();
        maybeFinalizeSuccessfulStream();
    }
}

void AppController::maybeFinalizeSuccessfulStream()
{
    if (!m_streamFinalizePending || !m_streamPendingText.isEmpty()) {
        return;
    }

    const QString finalText = m_streamAccumulatedText;
    m_speakingExpression = expressionFromAssistantText(finalText);
    updateStreamingMessageDisplay(false);
    setModelStatus(QString("Connected: %1").arg(m_selectedModel));
    appendAudit("Completion received");
    clearStreamingState();
    if (m_ttsEnabled) {
        processTtsSentenceBuffer(true);
        if (m_ttsQueue.isEmpty() && m_ttsProcess.state() == QProcess::NotRunning) {
            enqueueTtsChunk(finalText);
        }
        if (m_ttsQueue.isEmpty() && m_ttsProcess.state() == QProcess::NotRunning) {
            QTimer::singleShot(80, this, [this]() { finalizeAssistantState(); });
            return;
        }
        m_waitingForTtsDrain = true;
        startNextTtsChunk();
        return;
    }
    QTimer::singleShot(180, this, [this]() { finalizeAssistantState(); });
}

void AppController::clearStreamingState()
{
    m_streamFlushTimer.stop();
    setStreamingActive(false);
    m_streamingMessageRow = -1;
    m_chatRawBuffer.clear();
    m_chatStreamBuffer.clear();
    m_streamAccumulatedText.clear();
    m_streamPendingText.clear();
    m_streamSawToken = false;
    m_cancelRequested = false;
    m_streamFinalizePending = false;
    m_waitingForTtsDrain = false;
}

void AppController::startShellCommand(const QString &command)
{
    if (m_commandRunning) {
        m_messageModel.addMessage("system", "A shell command is already running.", "system");
        return;
    }

    m_lastShellCommand = command;
    m_shellOutputRow = m_messageModel.addMessageWithRow("system", QString("$ %1\n").arg(command), "system");
    appendAudit("Shell start: " + command);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!env.contains("TERM")) {
        env.insert("TERM", "xterm-256color");
    }
    env.insert("CLICOLOR_FORCE", "1");
    env.insert("FORCE_COLOR", "1");
    m_shellProcess.setProcessEnvironment(env);

    const QString scriptBin = QStandardPaths::findExecutable("script");
    if (!scriptBin.isEmpty()) {
        m_shellProcess.start(scriptBin, {"-qefc", command, "/dev/null"});
    } else {
        m_shellProcess.start("/bin/bash", {"-lc", command});
    }
    if (!m_shellProcess.waitForStarted(1200)) {
        m_messageModel.appendTextAt(m_shellOutputRow, "Failed to start shell command.");
        appendAudit("Shell failed to start");
        m_shellOutputRow = -1;
        setFaceState("warning");
        setStatusText("Command failed to start");
        return;
    }

    setCommandRunning(true);
    setStatusText("Running shell command...");
    setFaceState("thinking");
    m_shellTimeoutTimer.start(45000);
}

void AppController::cancelShellCommand()
{
    if (!m_commandRunning) {
        return;
    }

    appendAudit("Shell cancel requested: " + m_lastShellCommand);
    m_shellProcess.terminate();
    if (!m_shellProcess.waitForFinished(700)) {
        m_shellProcess.kill();
    }
    setStatusText("Shell command canceled");
}

void AppController::handleShellOutput()
{
    if (m_shellOutputRow < 0) {
        m_shellOutputRow = m_messageModel.addMessageWithRow("system", "", "system");
    }

    const QString out = QString::fromLocal8Bit(m_shellProcess.readAllStandardOutput());
    const QString err = QString::fromLocal8Bit(m_shellProcess.readAllStandardError());

    if (!out.isEmpty()) {
        m_messageModel.appendTextAt(m_shellOutputRow, out);
    }
    if (!err.isEmpty()) {
        m_messageModel.appendTextAt(m_shellOutputRow, "\u001b[31m" + err + "\u001b[0m");
    }
}

void AppController::handleShellFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!m_commandRunning) {
        return;
    }

    m_shellTimeoutTimer.stop();
    setCommandRunning(false);

    const bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);
    if (success) {
        appendAudit("Shell success: " + m_lastShellCommand);
        setFaceState("happy");
        setStatusText("Command completed");
        m_messageModel.addMessage("assistant", "Command completed successfully.", "assistant");
    } else {
        appendAudit(QString("Shell failure (%1): %2").arg(exitCode).arg(m_lastShellCommand));
        setFaceState("warning");
        setStatusText("Command failed");
        m_messageModel.addMessage(
            "assistant",
            QString("Command ended with exit code %1.").arg(exitCode),
            "warning");
    }

    m_shellOutputRow = -1;
    QTimer::singleShot(350, this, [this]() { finalizeAssistantState(); });
}

bool AppController::speakText(const QString &text)
{
    if (!m_ttsEnabled) {
        return false;
    }

    QString toSpeak = text.trimmed();
    if (toSpeak.isEmpty()) {
        return false;
    }

    if (toSpeak.size() > 700) {
        toSpeak = toSpeak.left(700);
    }

    const QStringList words = toSpeak.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    const int wordCount = qMax(1, words.size());
    int targetWordMs = 210;
    if (m_voiceStyle == "Soft") {
        targetWordMs = 245;
    } else if (m_voiceStyle == "Bright") {
        targetWordMs = 175;
    } else if (m_voiceStyle == "Narrator") {
        targetWordMs = 275;
    }
    const int estimatedMs = qMax(wordCount * targetWordMs, toSpeak.size() * 26);
    setMouthBeatMs(qMax(110, estimatedMs / wordCount));

    if (m_ttsBinary.isEmpty()) {
        m_ttsBinary = findTtsBinary();
    }

    if (m_ttsBinary.isEmpty()) {
        appendAudit("No TTS backend found (needs spd-say, espeak, or piper)");
        return false;
    }

    stopSpeaking();

    if (m_ttsBinary.endsWith("piper")) {
        if (m_piperModelPath.trimmed().isEmpty()) {
            appendAudit("Piper selected but no model path configured");
            return false;
        }
        const QString modelPath = expandPath(m_piperModelPath);
        if (!QFileInfo::exists(modelPath)) {
            appendAudit("Piper model file not found: " + modelPath);
            return false;
        }

        const QString aplay = QStandardPaths::findExecutable("aplay");
        if (aplay.isEmpty()) {
            appendAudit("Piper playback requires aplay");
            return false;
        }

        qreal lengthScale = 1.0;
        if (m_voiceStyle == "Soft") {
            lengthScale = 1.12;
        } else if (m_voiceStyle == "Bright") {
            lengthScale = 0.92;
        } else if (m_voiceStyle == "Narrator") {
            lengthScale = 1.24;
        }

        const QString outWav = appDataDir() + "/piper_tts.wav";
        const QString command = QString("printf %%s %1 | %2 --model %3 --length_scale %4 --output_file %5 && %6 -q %5")
                                    .arg(shellQuote(toSpeak),
                                         shellQuote(m_ttsBinary),
                                         shellQuote(modelPath),
                                         QString::number(lengthScale, 'f', 2),
                                         shellQuote(outWav),
                                         shellQuote(aplay));

        m_ttsProcess.start("/bin/bash", {"-lc", command});
        if (m_ttsProcess.waitForStarted(650)) {
            return true;
        }

        appendAudit("Piper TTS failed: " + m_ttsProcess.errorString());
        return false;
    }

    QStringList args;
    QStringList fallbackArgs;
    if (m_ttsBinary.endsWith("spd-say")) {
        QString voiceToken;
        if (m_gender == "Male") {
            voiceToken = "male1";
        } else if (m_gender == "Female") {
            voiceToken = "female1";
        }

        int rate = 0;
        int pitch = 0;
        if (m_voiceStyle == "Soft") {
            rate = -35;
            pitch = -25;
        } else if (m_voiceStyle == "Bright") {
            rate = 20;
            pitch = 20;
        } else if (m_voiceStyle == "Narrator") {
            rate = -25;
            pitch = -8;
        }

        args << "-w" << "-r" << QString::number(rate) << "-p" << QString::number(pitch);
        if (!voiceToken.isEmpty()) {
            args << "-t" << voiceToken;
        }
        args << toSpeak;
        fallbackArgs << "-w" << toSpeak;
    } else {
        QString voiceToken = "en";
        if (m_gender == "Male") {
            voiceToken = "en+m3";
        } else if (m_gender == "Female") {
            voiceToken = "en+f3";
        }

        int speed = 175;
        int pitch = 50;
        if (m_voiceStyle == "Soft") {
            speed = 155;
            pitch = 36;
        } else if (m_voiceStyle == "Bright") {
            speed = 195;
            pitch = 68;
        } else if (m_voiceStyle == "Narrator") {
            speed = 145;
            pitch = 44;
        }

        args << "-v" << voiceToken << "-s" << QString::number(speed) << "-p" << QString::number(pitch);
        args << toSpeak;
        fallbackArgs << toSpeak;
    }

    m_ttsProcess.start(m_ttsBinary, args);
    if (m_ttsProcess.waitForStarted(450)) {
        return true;
    }

    appendAudit("TTS start failed with styled args, retrying fallback");
    m_ttsProcess.start(m_ttsBinary, fallbackArgs);
    if (m_ttsProcess.waitForStarted(450)) {
        return true;
    }

    appendAudit("TTS failed: " + m_ttsProcess.errorString());
    return false;
}

void AppController::enqueueTtsChunk(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    m_ttsQueue.append(trimmed);
}

void AppController::processTtsSentenceBuffer(bool flushRemainder)
{
    if (!m_ttsEnabled) {
        return;
    }

    while (true) {
        const int boundary = findSentenceBoundary(m_ttsSentenceBuffer);
        if (boundary <= 0) {
            break;
        }
        const QString part = m_ttsSentenceBuffer.left(boundary).trimmed();
        m_ttsSentenceBuffer.remove(0, boundary);
        enqueueTtsChunk(part);
    }

    if (flushRemainder) {
        enqueueTtsChunk(m_ttsSentenceBuffer.trimmed());
        m_ttsSentenceBuffer.clear();
    }
}

void AppController::startNextTtsChunk()
{
    if (!m_ttsEnabled || m_ttsProcess.state() != QProcess::NotRunning || m_ttsQueue.isEmpty()) {
        return;
    }

    const QString next = m_ttsQueue.takeFirst();
    const bool started = speakText(next);
    if (!started) {
        if (!m_ttsQueue.isEmpty()) {
            QTimer::singleShot(0, this, [this]() { startNextTtsChunk(); });
            return;
        }
        setSpeakingActive(false);
        if (m_waitingForTtsDrain) {
            m_waitingForTtsDrain = false;
            QTimer::singleShot(80, this, [this]() { finalizeAssistantState(); });
        }
    }
}

void AppController::clearTtsQueue()
{
    m_ttsQueue.clear();
    m_ttsSentenceBuffer.clear();
    m_waitingForTtsDrain = false;
    m_ttsQueueRunning = false;
    setSpeakingActive(false);
}

int AppController::findSentenceBoundary(const QString &text) const
{
    if (text.isEmpty()) {
        return -1;
    }

    static const QString boundaries = QStringLiteral(".!?;:\n。！？；：");
    for (int i = 0; i < text.size(); ++i) {
        if (boundaries.contains(text.at(i))) {
            return i + 1;
        }
    }

    if (text.size() > 140) {
        int split = text.lastIndexOf(' ', 120);
        if (split < 32) {
            split = 120;
        }
        return split + 1;
    }

    return -1;
}

void AppController::stopSpeaking()
{
    if (m_ttsProcess.state() == QProcess::NotRunning) {
        setSpeakingActive(false);
        return;
    }
    m_ttsProcess.kill();
    m_ttsProcess.waitForFinished(300);
    appendAudit("TTS stopped");
    m_ttsQueueRunning = false;
    m_echoSuppressUntilMs = m_runtimeClock.elapsed() + echoSuppressTailMs();
    setSpeakingActive(false);
}

QString AppController::currentProjectKey() const
{
    return QDir::cleanPath(QDir::currentPath());
}

QString AppController::appDataDir() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath() + "/.senticli";
    }
    QDir().mkpath(dir);
    return dir;
}

QString AppController::auditLogPath() const
{
    return appDataDir() + "/audit.json";
}

QString AppController::memoryPath() const
{
    return appDataDir() + "/memory.json";
}

void AppController::loadSettings()
{
    QSettings settings;

    m_connectionProfiles = settings.value("profiles/names", QStringList{"Default"}).toStringList();
    if (m_connectionProfiles.isEmpty()) {
        m_connectionProfiles = {"Default"};
    }
    m_activeProfile = settings.value("profiles/active", m_connectionProfiles.first()).toString();
    if (m_activeProfile.isEmpty() || !m_connectionProfiles.contains(m_activeProfile)) {
        m_activeProfile = m_connectionProfiles.first();
    }

    m_provider = settings.value("ai/provider", m_provider).toString();
    if (!providers().contains(m_provider)) {
        m_provider = "Custom";
    }
    m_cameraEnabled = settings.value("camera/enabled", m_cameraEnabled).toBool();

    m_endpoint = settings.value("ai/endpoint", m_endpoint).toString();
    m_modelsEndpoint = normalizedModelsOverrideUrl(
        settings.value("ai/modelsEndpoint", m_modelsEndpoint).toString());
    m_apiKey = settings.value("ai/apiKey", m_apiKey).toString();
    m_selectedModel = settings.value("ai/selectedModel", m_selectedModel).toString();
    m_availableModels = settings.value("ai/availableModels", QStringList{"local-prototype"}).toStringList();
    if (m_availableModels.isEmpty()) {
        m_availableModels = {"local-prototype"};
    }

    m_smoothingProfile = settings.value("ai/smoothingProfile", m_smoothingProfile).toString();
    if (!smoothingProfiles().contains(m_smoothingProfile)) {
        m_smoothingProfile = "Terminal";
    }

    m_tokenRate = qBound(20, settings.value("ai/tokenRate", m_tokenRate).toInt(), 600);
    m_lipSyncDelayMs = qBound(0, settings.value("voice/lipSyncDelayMs", m_lipSyncDelayMs).toInt(), 1100);
    m_assistantName = settings.value("ai/assistantName", m_assistantName).toString().trimmed();
    if (m_assistantName.isEmpty()) {
        m_assistantName = "Senticli";
    }
    m_wakeEnabled = settings.value("ai/wakeEnabled", m_wakeEnabled).toBool();
    m_wakeResponses = settings.value("ai/wakeResponses", m_wakeResponses).toStringList();
    if (m_wakeResponses.isEmpty()) {
        m_wakeResponses = {"How can I help you?",
                           "I am here. What do you need?",
                           "Ready when you are.",
                           "Yep, talk to me.",
                           "What would you like me to do?"};
    }
    m_conversationalMode = settings.value("ai/conversationalMode", m_conversationalMode).toBool();
    m_duplexVoiceEnabled = settings.value("voice/duplexEnabled", m_duplexVoiceEnabled).toBool();
    m_transcriptionEndpoint = normalizedTranscriptionUrl(
        settings.value("voice/transcriptionEndpoint", m_transcriptionEndpoint).toString());
    m_transcriptionModel = settings.value("voice/transcriptionModel", m_transcriptionModel).toString().trimmed();
    if (m_transcriptionModel.isEmpty()) {
        m_transcriptionModel = "whisper-1";
    }
    m_vadSensitivity = qBound(1, settings.value("voice/vadSensitivity", m_vadSensitivity).toInt(), 100);
    m_duplexSmoothness = settings.value("voice/duplexSmoothness", m_duplexSmoothness).toString();
    if (!duplexSmoothnessOptions().contains(m_duplexSmoothness)) {
        m_duplexSmoothness = "Balanced";
    }
    m_personality = settings.value("ai/personality", m_personality).toString();
    if (!personalities().contains(m_personality)) {
        m_personality = "Helpful";
    }
    m_faceStyle = settings.value("ui/faceStyle", m_faceStyle).toString();
    if (!faceStyles().contains(m_faceStyle)) {
        m_faceStyle = "Loona";
    }
    m_gender = settings.value("ai/gender", m_gender).toString();
    if (!genders().contains(m_gender)) {
        m_gender = "Neutral";
    }
    m_voiceStyle = settings.value("voice/style", m_voiceStyle).toString();
    if (!voiceStyles().contains(m_voiceStyle)) {
        m_voiceStyle = "Default";
    }
    m_voiceEngine = settings.value("voice/engine", m_voiceEngine).toString();
    if (!voiceEngines().contains(m_voiceEngine)) {
        m_voiceEngine = "Auto";
    }
    m_piperModelPath = expandPath(settings.value("voice/piperModelPath", m_piperModelPath).toString());
    m_ttsEnabled = settings.value("voice/ttsEnabled", m_ttsEnabled).toBool();
    m_memoryEnabled = settings.value("memory/enabled", m_memoryEnabled).toBool();
    m_setupComplete = settings.value("ui/setupComplete", false).toBool();

    m_grantedFolders = settings.value("permissions/grantedFolders", QStringList{}).toStringList();
    if (m_grantedFolders.isEmpty()) {
        m_grantedFolders = {
            QDir::cleanPath(QDir::homePath() + "/Documents"),
            QDir::cleanPath(QDir::homePath() + "/Projects"),
        };
    }

    if (settings.contains(QString("profiles/%1/endpoint").arg(m_activeProfile))) {
        loadProfile(m_activeProfile);
    } else {
        saveProfileToSettings(m_activeProfile);
    }
}

void AppController::saveSettings() const
{
    QSettings settings;
    settings.setValue("profiles/names", m_connectionProfiles);
    settings.setValue("profiles/active", m_activeProfile);
    settings.setValue("ai/provider", m_provider);
    settings.setValue("camera/enabled", m_cameraEnabled);
    settings.setValue("ai/endpoint", m_endpoint);
    settings.setValue("ai/modelsEndpoint", m_modelsEndpoint);
    settings.setValue("ai/apiKey", m_apiKey);
    settings.setValue("ai/selectedModel", m_selectedModel);
    settings.setValue("ai/availableModels", m_availableModels);
    settings.setValue("ai/smoothingProfile", m_smoothingProfile);
    settings.setValue("ai/tokenRate", m_tokenRate);
    settings.setValue("voice/lipSyncDelayMs", m_lipSyncDelayMs);
    settings.setValue("ai/assistantName", m_assistantName);
    settings.setValue("ai/wakeEnabled", m_wakeEnabled);
    settings.setValue("ai/wakeResponses", m_wakeResponses);
    settings.setValue("ai/conversationalMode", m_conversationalMode);
    settings.setValue("ai/personality", m_personality);
    settings.setValue("ai/gender", m_gender);
    settings.setValue("voice/ttsEnabled", m_ttsEnabled);
    settings.setValue("voice/style", m_voiceStyle);
    settings.setValue("voice/engine", m_voiceEngine);
    settings.setValue("voice/piperModelPath", m_piperModelPath);
    settings.setValue("voice/duplexEnabled", m_duplexVoiceEnabled);
    settings.setValue("voice/transcriptionEndpoint", m_transcriptionEndpoint);
    settings.setValue("voice/transcriptionModel", m_transcriptionModel);
    settings.setValue("voice/vadSensitivity", m_vadSensitivity);
    settings.setValue("voice/duplexSmoothness", m_duplexSmoothness);
    settings.setValue("memory/enabled", m_memoryEnabled);
    settings.setValue("permissions/grantedFolders", m_grantedFolders);
    settings.setValue("ui/faceStyle", m_faceStyle);
    settings.setValue("ui/setupComplete", m_setupComplete);
}

void AppController::appendAudit(const QString &entry)
{
    const QString stamped = QString("[%1] %2")
                                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"), entry);
    m_auditEntries.append(stamped);
    while (m_auditEntries.size() > 500) {
        m_auditEntries.removeFirst();
    }
    emit auditEntriesChanged();
    saveAuditEntries();
}

void AppController::loadAuditEntries()
{
    QFile file(auditLogPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        return;
    }

    m_auditEntries.clear();
    const QJsonArray arr = doc.array();
    for (const QJsonValue &v : arr) {
        const QString text = v.toString();
        if (!text.isEmpty()) {
            m_auditEntries.append(text);
        }
    }
}

void AppController::saveAuditEntries() const
{
    QFile file(auditLogPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }

    QJsonArray arr;
    for (const QString &entry : m_auditEntries) {
        arr.append(entry);
    }

    file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void AppController::loadProjectMemory()
{
    QFile file(memoryPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    m_projectMemory.clear();
    const QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        QStringList notes;
        const QJsonArray arr = it.value().toArray();
        for (const QJsonValue &v : arr) {
            const QString note = v.toString();
            if (!note.isEmpty()) {
                notes.append(note);
            }
        }
        if (!notes.isEmpty()) {
            m_projectMemory.insert(it.key(), notes);
        }
    }
}

void AppController::saveProjectMemory() const
{
    QJsonObject obj;
    for (auto it = m_projectMemory.begin(); it != m_projectMemory.end(); ++it) {
        QJsonArray arr;
        for (const QString &note : it.value()) {
            arr.append(note);
        }
        obj.insert(it.key(), arr);
    }

    QFile file(memoryPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

QString AppController::projectMemoryContext() const
{
    if (!m_memoryEnabled) {
        return {};
    }

    const QString key = currentProjectKey();
    const QStringList notes = m_projectMemory.value(key);
    if (notes.isEmpty()) {
        return {};
    }

    const int start = qMax(0, notes.size() - 8);
    QStringList subset;
    for (int i = start; i < notes.size(); ++i) {
        subset.append("- " + notes.at(i));
    }
    return subset.join("\n");
}

QString AppController::findTtsBinary() const
{
    const QString engine = m_voiceEngine.trimmed();

    if (engine == "Speech Dispatcher") {
        return QStandardPaths::findExecutable("spd-say");
    }
    if (engine == "eSpeak") {
        return QStandardPaths::findExecutable("espeak");
    }
    if (engine == "Piper") {
        return QStandardPaths::findExecutable("piper");
    }

    QString bin = QStandardPaths::findExecutable("spd-say");
    if (!bin.isEmpty()) {
        return bin;
    }

    bin = QStandardPaths::findExecutable("espeak");
    if (!bin.isEmpty()) {
        return bin;
    }

    return QStandardPaths::findExecutable("piper");
}

void AppController::handleInput(const QString &text)
{
    const QString lowered = text.toLower();

    if (lowered == "/help" || lowered == "help") {
        postAssistant(
            "Commands:\n"
            "/profiles\n"
            "/profile <name>\n"
            "/profile-save <name>\n"
            "/profile-delete <name>\n"
            "/provider <Custom|LM Studio|Ollama>\n"
            "/endpoint <url>\n"
            "/models-endpoint <url> (optional override)\n"
            "/apikey <token> (or /apikey clear)\n"
            "/camera on|off|snap\n"
            "/models\n"
            "/model <id>\n"
            "/speed <Instant|Terminal|Balanced|Human|Cinematic>\n"
            "/lip-sync <0-1100>\n"
            "/name <assistant-name>\n"
            "/wake on|off\n"
            "/conversation on|off\n"
            "/duplex on|off\n"
            "/duplex-smooth <Responsive|Balanced|Natural|Studio>\n"
            "/duplex-preset <fast|balanced|human|studio>\n"
            "/vad <1-100>\n"
            "/stt-endpoint <url>\n"
            "/stt-model <id>\n"
            "/personality <Helpful|Professional|Witty|Teacher|Hacker|Calm>\n"
            "/face-style <Loona|Terminal|Orb>\n"
            "/gender <Neutral|Male|Female>\n"
            "/voice-style <Default|Soft|Bright|Narrator>\n"
            "/voice-engine <Auto|Speech Dispatcher|eSpeak|Piper>\n"
            "/piper-model <path|clear>\n"
            "/voices\n"
            "/test\n"
            "/run <command>\n"
            "/cancel\n"
            "/grant <folder>\n"
            "/revoke <folder>\n"
            "/remember <note>\n"
            "/recall\n"
            "/forget\n"
            "/voice on|off|stop");
        return;
    }

    if (lowered == "/profiles") {
        postAssistant("Profiles:\n- " + m_connectionProfiles.join("\n- ")
                      + QString("\nActive: %1").arg(m_activeProfile));
        return;
    }

    if (lowered == "/models") {
        refreshModels();
        return;
    }

    if (lowered == "/voices") {
        postAssistant(
            QString("Voice options:\nGender: %1\nStyle: %2\nEngine: %3\nCurrent engine: %4\nPiper model: %5")
                .arg(genders().join(", "),
                     voiceStyles().join(", "),
                     voiceEngines().join(", "),
                     m_voiceEngine,
                     m_piperModelPath.isEmpty() ? "(not set)" : m_piperModelPath));
        return;
    }

    if (lowered == "/test") {
        testProviderConnection();
        return;
    }

    if (lowered == "/cancel") {
        cancelActiveRequest();
        return;
    }

    if (lowered == "/recall") {
        const QString context = projectMemoryContext();
        postAssistant(context.isEmpty() ? "No notes stored for this project." : "Project memory:\n" + context);
        return;
    }

    if (lowered == "/forget") {
        m_projectMemory.remove(currentProjectKey());
        saveProjectMemory();
        postAssistant("Cleared project memory notes.");
        return;
    }

    if (lowered.startsWith("/remember ")) {
        const QString note = text.mid(10).trimmed();
        if (note.isEmpty()) {
            postAssistant("Usage: /remember <note>", "warning");
            return;
        }
        QStringList notes = m_projectMemory.value(currentProjectKey());
        notes.append(note);
        while (notes.size() > 40) {
            notes.removeFirst();
        }
        m_projectMemory.insert(currentProjectKey(), notes);
        saveProjectMemory();
        postAssistant("Saved to project memory.");
        return;
    }

    if (lowered.startsWith("/provider ")) {
        setProvider(text.mid(10).trimmed());
        return;
    }

    if (lowered.startsWith("/profile-save ")) {
        saveCurrentProfile(text.mid(14).trimmed());
        return;
    }

    if (lowered.startsWith("/profile-delete ")) {
        deleteProfile(text.mid(16).trimmed());
        return;
    }

    if (lowered.startsWith("/profile ")) {
        setActiveProfile(text.mid(9).trimmed());
        return;
    }

    if (lowered.startsWith("/endpoint ")) {
        setEndpoint(text.mid(10).trimmed());
        return;
    }

    if (lowered.startsWith("/models-endpoint ")) {
        const QString value = text.mid(17).trimmed();
        if (value.toLower() == "auto" || value.toLower() == "clear") {
            setModelsEndpoint("");
            postAssistant("Models endpoint override cleared (auto mode).");
            return;
        }
        setModelsEndpoint(value);
        postAssistant("Models endpoint override saved.");
        return;
    }

    if (lowered.startsWith("/apikey ")) {
        const QString value = text.mid(8).trimmed();
        if (value.toLower() == "clear") {
            setApiKey("");
            postAssistant("API key cleared.");
            return;
        }
        setApiKey(value);
        postAssistant("API key saved.");
        return;
    }

    if (lowered.startsWith("/camera ")) {
        const QString value = lowered.mid(8).trimmed();
        if (value == "on") {
            setCameraEnabled(true);
            return;
        }
        if (value == "off") {
            setCameraEnabled(false);
            return;
        }
        if (value == "snap" || value == "analyze") {
            captureAndAnalyzeCameraFrame();
            return;
        }
        postAssistant("Usage: /camera on|off|snap", "warning");
        return;
    }

    if (lowered.startsWith("/model ")) {
        setSelectedModel(text.mid(7).trimmed());
        return;
    }

    if (lowered.startsWith("/name ")) {
        setAssistantName(text.mid(6).trimmed());
        return;
    }

    if (lowered.startsWith("/wake ")) {
        const QString value = lowered.mid(6).trimmed();
        if (value == "on") {
            setWakeEnabled(true);
            return;
        }
        if (value == "off") {
            setWakeEnabled(false);
            return;
        }
        postAssistant("Usage: /wake on|off", "warning");
        return;
    }

    if (lowered.startsWith("/conversation ")) {
        const QString value = lowered.mid(14).trimmed();
        if (value == "on") {
            setConversationalMode(true);
            return;
        }
        if (value == "off") {
            setConversationalMode(false);
            return;
        }
        postAssistant("Usage: /conversation on|off", "warning");
        return;
    }

    if (lowered.startsWith("/duplex ")) {
        const QString value = lowered.mid(8).trimmed();
        if (value == "on") {
            setDuplexVoiceEnabled(true);
            return;
        }
        if (value == "off") {
            setDuplexVoiceEnabled(false);
            return;
        }
        postAssistant("Usage: /duplex on|off", "warning");
        return;
    }

    if (lowered.startsWith("/duplex-smooth ")) {
        const QString value = text.mid(15).trimmed();
        bool valid = false;
        for (const QString &candidate : duplexSmoothnessOptions()) {
            if (candidate.compare(value, Qt::CaseInsensitive) == 0) {
                valid = true;
                break;
            }
        }
        if (!valid) {
            postAssistant("Usage: /duplex-smooth <Responsive|Balanced|Natural|Studio>", "warning");
            return;
        }
        setDuplexSmoothness(value);
        return;
    }

    if (lowered.startsWith("/duplex-preset ")) {
        const QString value = text.mid(15).trimmed();
        if (!applyDuplexPreset(value)) {
            postAssistant("Usage: /duplex-preset <fast|balanced|human|studio>", "warning");
        }
        return;
    }

    if (lowered.startsWith("/vad ")) {
        bool ok = false;
        const int value = text.mid(5).trimmed().toInt(&ok);
        if (!ok) {
            postAssistant("Usage: /vad <1-100>", "warning");
            return;
        }
        setVadSensitivity(value);
        return;
    }

    if (lowered.startsWith("/stt-endpoint ")) {
        const QString value = text.mid(14).trimmed();
        if (value.toLower() == "auto" || value.toLower() == "clear") {
            setTranscriptionEndpoint("");
            return;
        }
        setTranscriptionEndpoint(value);
        return;
    }

    if (lowered.startsWith("/stt-model ")) {
        setTranscriptionModel(text.mid(11).trimmed());
        return;
    }

    if (lowered.startsWith("/speed ")) {
        setSmoothingProfile(text.mid(7).trimmed());
        return;
    }

    if (lowered.startsWith("/lip-sync ")) {
        bool ok = false;
        const int value = text.mid(10).trimmed().toInt(&ok);
        if (!ok) {
            postAssistant("Usage: /lip-sync <0-1100>", "warning");
            return;
        }
        setLipSyncDelayMs(value);
        return;
    }

    if (lowered.startsWith("/personality ")) {
        setPersonality(text.mid(13).trimmed());
        return;
    }

    if (lowered.startsWith("/face-style ")) {
        const QString value = text.mid(12).trimmed();
        bool valid = false;
        for (const QString &candidate : faceStyles()) {
            if (candidate.compare(value, Qt::CaseInsensitive) == 0) {
                valid = true;
                break;
            }
        }
        if (!valid) {
            postAssistant("Usage: /face-style <Loona|Terminal|Orb>", "warning");
            return;
        }
        setFaceStyle(value);
        return;
    }

    if (lowered.startsWith("/gender ")) {
        setGender(text.mid(8).trimmed());
        return;
    }

    if (lowered.startsWith("/voice-style ")) {
        setVoiceStyle(text.mid(13).trimmed());
        return;
    }

    if (lowered.startsWith("/voice-engine ")) {
        const QString value = text.mid(14).trimmed();
        bool valid = false;
        for (const QString &candidate : voiceEngines()) {
            if (candidate.compare(value, Qt::CaseInsensitive) == 0) {
                valid = true;
                break;
            }
        }
        if (!valid) {
            postAssistant("Usage: /voice-engine <Auto|Speech Dispatcher|eSpeak|Piper>", "warning");
            return;
        }
        setVoiceEngine(value);
        return;
    }

    if (lowered.startsWith("/piper-model ")) {
        const QString value = text.mid(13).trimmed();
        if (value.toLower() == "clear" || value.toLower() == "none") {
            setPiperModelPath("");
            return;
        }
        setPiperModelPath(value);
        return;
    }

    if (lowered.startsWith("/grant ")) {
        addGrantedFolder(text.mid(7).trimmed());
        return;
    }

    if (lowered.startsWith("/revoke ")) {
        removeGrantedFolder(text.mid(8).trimmed());
        return;
    }

    if (lowered.startsWith("/voice ")) {
        const QString cmd = lowered.mid(7).trimmed();
        if (cmd == "on") {
            setTtsEnabled(true);
            return;
        }
        if (cmd == "off") {
            setTtsEnabled(false);
            return;
        }
        if (cmd == "stop") {
            stopSpeaking();
            return;
        }
        postAssistant("Usage: /voice on|off|stop", "warning");
        return;
    }

    if (lowered.startsWith("/open ")) {
        const QString appName = text.mid(6).trimmed();
        if (appName.isEmpty()) {
            postAssistant("Usage: /open <app-name>", "warning");
            return;
        }
        requestApproval(QString("Allow Senticli to open %1?").arg(appName), "open", appName);
        return;
    }

    if (lowered.startsWith("/find ")) {
        const QString query = text.mid(6).trimmed();
        if (m_grantedFolders.isEmpty()) {
            postAssistant("No folders are granted. Add one in AI Settings > Permissions.", "warning");
            return;
        }

        QStringList matches;
        const QString needle = query.toLower();
        for (const QString &folder : m_grantedFolders) {
            QDir dir(folder);
            if (!dir.exists()) {
                continue;
            }
            const QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
            for (const QString &file : files) {
                if (file.toLower().contains(needle) || needle == "*") {
                    matches.append(dir.filePath(file));
                }
                if (matches.size() >= 20) {
                    break;
                }
            }
            if (matches.size() >= 20) {
                break;
            }
        }

        if (matches.isEmpty()) {
            postAssistant(QString("No files found matching '%1'.").arg(query));
        } else {
            postAssistant("Matches:\n" + matches.join("\n"));
        }
        return;
    }

    if (lowered.startsWith("/run ")) {
        const QString command = text.mid(5).trimmed();
        if (command.isEmpty()) {
            postAssistant("Usage: /run <command>", "warning");
            return;
        }

        if (isRiskyCommand(command)) {
            requestApproval(
                QString("This command may be destructive: `%1`\nApprove execution?").arg(command),
                "run",
                command);
            return;
        }

        runShellCommandPreview(command, false);
        return;
    }

    postAssistant(
        "I can run shell commands, manage provider settings, remember project notes, and assist with Linux tasks. Type /help.");
}

void AppController::finalizeAssistantState()
{
    setSpeakingActive(false);

    if (m_pendingApproval) {
        setFaceState("warning");
        setStatusText("Waiting for approval");
    } else if (m_micActive) {
        setFaceState("listening");
        setStatusText(m_duplexVoiceEnabled ? "Duplex listening..." : "Listening...");
    } else {
        setFaceState("idle");
        setStatusText("Ready");
    }
}

void AppController::requestRemoteCompletion(const QString &text)
{
    if (m_activeChatReply) {
        m_messageModel.addMessage("system", "A model response is already in progress.", "system");
        setFaceState("warning");
        setStatusText("Model busy");
        return;
    }

    const QString completionUrl = normalizedCompletionUrl(m_endpoint);
    if (completionUrl.isEmpty()) {
        postAssistant("The configured endpoint is invalid.", "warning");
        setModelStatus("Invalid endpoint");
        return;
    }

    QNetworkRequest request{QUrl(completionUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "text/event-stream");
    applyAuthHeader(request);

    QJsonArray messages;
    messages.append(QJsonObject{
        {"role", "system"},
        {"content", systemPersonaPrompt()},
    });

    const QString memoryContext = projectMemoryContext();
    if (!memoryContext.isEmpty()) {
        messages.append(QJsonObject{
            {"role", "system"},
            {"content", QString("Project memory context:\n%1").arg(memoryContext)},
        });
    }

    messages.append(QJsonObject{{"role", "user"}, {"content", text}});

    QJsonObject payload;
    payload.insert("model", m_selectedModel);
    payload.insert("stream", true);
    payload.insert("messages", messages);

    m_chatStreamBuffer.clear();
    m_chatRawBuffer.clear();
    m_streamAccumulatedText.clear();
    m_streamPendingText.clear();
    m_ttsSentenceBuffer.clear();
    clearTtsQueue();
    m_streamSawToken = false;
    m_cancelRequested = false;
    m_streamFinalizePending = false;
    m_streamingMessageRow = m_messageModel.addMessageWithRow("assistant", "", "assistant");
    m_streamFlushTimer.setInterval(flushIntervalForProfile());
    setStreamingActive(true);
    updateStreamingMessageDisplay(true);

    setSpeakingActive(false);
    setFaceState("thinking");
    setStatusText("Waiting for first token...");
    setModelStatus(QString("Querying %1").arg(m_selectedModel));
    appendAudit("Completion request started");

    m_activeChatReply = m_network.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QNetworkReply *reply = m_activeChatReply;

    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        if (reply != m_activeChatReply) {
            return;
        }

        const QByteArray chunk = reply->readAll();
        if (chunk.isEmpty()) {
            return;
        }

        m_chatRawBuffer += chunk;
        m_chatStreamBuffer += chunk;

        while (true) {
            const int lineBreak = m_chatStreamBuffer.indexOf('\n');
            if (lineBreak < 0) {
                break;
            }

            QByteArray line = m_chatStreamBuffer.left(lineBreak);
            m_chatStreamBuffer.remove(0, lineBreak + 1);
            line = line.trimmed();

            if (line.isEmpty() || !line.startsWith("data:")) {
                continue;
            }

            QByteArray payloadLine = line.mid(5).trimmed();
            if (payloadLine == "[DONE]") {
                continue;
            }

            QJsonParseError parseError;
            const QJsonDocument eventDoc = QJsonDocument::fromJson(payloadLine, &parseError);
            if (parseError.error != QJsonParseError::NoError || !eventDoc.isObject()) {
                continue;
            }

            const QJsonArray choices = eventDoc.object().value("choices").toArray();
            if (choices.isEmpty()) {
                continue;
            }

            const QString token = extractContentFromChoice(choices.first().toObject());
            if (token.isEmpty()) {
                continue;
            }

            m_streamSawToken = true;
            if (!m_ttsEnabled) {
                setSpeakingActive(true);
                setFaceState("speaking");
                setStatusText("Streaming response...");
            } else {
                if (!isAssistantAudible()) {
                    setFaceState("thinking");
                    setStatusText("Receiving response...");
                } else {
                    setFaceState(m_speakingExpression);
                    setStatusText("Speaking...");
                }
                m_ttsSentenceBuffer += token;
                processTtsSentenceBuffer(false);
                startNextTtsChunk();
            }
            queueStreamText(token);
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply != m_activeChatReply) {
            reply->deleteLater();
            return;
        }

        m_activeChatReply = nullptr;

        if (reply->error() != QNetworkReply::NoError) {
            if (reply->error() == QNetworkReply::OperationCanceledError && m_cancelRequested) {
                setModelStatus("Streaming canceled");
                const QString partial = m_streamAccumulatedText + m_streamPendingText;
                m_messageModel.setTextAt(
                    m_streamingMessageRow,
                    partial.isEmpty() ? "Response canceled." : partial + " [stopped]");
                appendAudit("Completion canceled");
                clearStreamingState();
                finalizeAssistantState();
                reply->deleteLater();
                return;
            }

            setModelStatus(QString("Request failed: %1").arg(reply->errorString()));
            const QString partial = m_streamAccumulatedText + m_streamPendingText;
            if (m_streamingMessageRow >= 0) {
                m_messageModel.setTextAt(
                    m_streamingMessageRow,
                    partial.isEmpty() ? "Model request failed." : partial + " [connection lost]");
            }
            appendAudit("Completion failed: " + reply->errorString());
            clearStreamingState();
            setFaceState("warning");
            setStatusText("Model request failed");
            setSpeakingActive(false);
            reply->deleteLater();
            return;
        }

        if (!m_streamSawToken) {
            QJsonParseError parseError;
            const QJsonDocument fallbackDoc = QJsonDocument::fromJson(m_chatRawBuffer, &parseError);
            if (parseError.error == QJsonParseError::NoError && fallbackDoc.isObject()) {
                const QJsonArray choices = fallbackDoc.object().value("choices").toArray();
                if (!choices.isEmpty()) {
                    const QString content = extractContentFromChoice(choices.first().toObject()).trimmed();
                    if (!content.isEmpty()) {
                        m_streamSawToken = true;
                        m_speakingExpression = expressionFromAssistantText(content);
                        if (!m_ttsEnabled) {
                            setSpeakingActive(true);
                            setFaceState(m_speakingExpression);
                            setStatusText("Responding...");
                        } else {
                            m_ttsSentenceBuffer += content;
                            processTtsSentenceBuffer(false);
                            startNextTtsChunk();
                        }
                        queueStreamText(content);
                    }
                }
            }
        }

        if (!m_streamSawToken) {
            setModelStatus("Completion response had no text");
            if (m_streamingMessageRow >= 0) {
                m_messageModel.setTextAt(m_streamingMessageRow, "The model responded, but no text content was returned.");
            }
            appendAudit("Completion empty response");
            clearStreamingState();
            setFaceState("warning");
            setStatusText("No response text");
            setSpeakingActive(false);
            reply->deleteLater();
            return;
        }

        m_streamFinalizePending = true;
        maybeFinalizeSuccessfulStream();
        reply->deleteLater();
    });
}

QString AppController::extractContentFromChoice(const QJsonObject &choiceObject) const
{
    const QJsonObject delta = choiceObject.value("delta").toObject();
    if (!delta.isEmpty()) {
        if (delta.value("content").isString()) {
            return delta.value("content").toString();
        }

        if (delta.value("content").isArray()) {
            QString collected;
            const QJsonArray parts = delta.value("content").toArray();
            for (const QJsonValue &part : parts) {
                const QJsonObject partObject = part.toObject();
                if (partObject.value("text").isString()) {
                    collected += partObject.value("text").toString();
                }
            }
            if (!collected.isEmpty()) {
                return collected;
            }
        }
    }

    const QJsonObject message = choiceObject.value("message").toObject();
    if (message.value("content").isString()) {
        return message.value("content").toString();
    }

    if (message.value("content").isArray()) {
        QString collected;
        const QJsonArray parts = message.value("content").toArray();
        for (const QJsonValue &part : parts) {
            const QJsonObject partObject = part.toObject();
            if (partObject.value("text").isString()) {
                collected += partObject.value("text").toString();
            } else if (partObject.value("type").toString() == "text"
                       && partObject.value("content").isString()) {
                collected += partObject.value("content").toString();
            }
        }
        if (!collected.isEmpty()) {
            return collected;
        }
    }

    if (choiceObject.value("text").isString()) {
        return choiceObject.value("text").toString();
    }

    return {};
}

QString AppController::systemPersonaPrompt() const
{
    QString tone = "practical, concise, and helpful";
    if (m_personality == "Professional") {
        tone = "professional, precise, and concise";
    } else if (m_personality == "Witty") {
        tone = "smart, lightly witty, and concise";
    } else if (m_personality == "Teacher") {
        tone = "clear, step-by-step, and encouraging";
    } else if (m_personality == "Hacker") {
        tone = "technical, direct, and terminal-native";
    } else if (m_personality == "Calm") {
        tone = "calm, reassuring, and concise";
    }

    const QString conversationInstruction = m_conversationalMode
        ? "Use natural dialogue, ask brief clarifying follow-ups when useful, and keep interaction human-like without fluff."
        : "Keep interaction efficient and command-focused.";

    return QString(
               "You are %1, a local Linux terminal companion. "
               "Keep replies %2. %3 "
               "Current presentation preferences: personality=%4, face-style=%5, voice-gender=%6, voice-style=%7, voice-engine=%8. "
               "When suggesting actions, be explicit and safe.")
        .arg(m_assistantName, tone, conversationInstruction, m_personality, m_faceStyle, m_gender, m_voiceStyle, m_voiceEngine);
}

void AppController::postAssistant(const QString &text, const QString &kind)
{
    m_messageModel.addMessage("assistant", text, kind);

    if (kind == "warning") {
        setSpeakingActive(false);
        setFaceState("warning");
        setStatusText("Attention required");
        return;
    }

    m_speakingExpression = expressionFromAssistantText(text, kind);

    if (m_ttsEnabled) {
        clearTtsQueue();
        enqueueTtsChunk(text);
        m_waitingForTtsDrain = true;
        startNextTtsChunk();
        if (m_ttsProcess.state() != QProcess::NotRunning || !m_ttsQueue.isEmpty()) {
            return;
        }
    }

    setSpeakingActive(true);
    setFaceState(m_speakingExpression);
    setStatusText("Responding...");
    const int ms = qBound(220, text.size() * 16, 1500);
    QTimer::singleShot(ms, this, [this]() { finalizeAssistantState(); });
}

void AppController::requestApproval(const QString &prompt, const QString &actionKind, const QString &payload)
{
    m_pendingApproval = true;
    m_pendingApprovalText = prompt;
    m_pendingActionKind = actionKind;
    m_pendingPayload = payload;
    emit pendingApprovalChanged();

    m_messageModel.addMessage("assistant", prompt, "warning");
    setFaceState("warning");
    setStatusText("Waiting for approval");
}

void AppController::runShellCommandPreview(const QString &command, bool approved)
{
    if (approved) {
        m_messageModel.addMessage("system", "Approval received.", "system");
    }
    startShellCommand(command);
}

void AppController::startVoiceCaptureLoop()
{
    if (!m_duplexVoiceEnabled || !m_micActive) {
        return;
    }

    if (m_voiceCaptureTempPath.isEmpty()) {
        m_voiceCaptureTempPath = appDataDir() + "/voice_chunk.wav";
    }
    runVoiceCaptureChunk();
}

void AppController::stopVoiceCaptureLoop()
{
    m_voiceCaptureRestartTimer.stop();
    m_voiceUtteranceTimer.stop();
    m_voiceUtteranceBuffer.clear();
    if (m_voiceCaptureProcess.state() != QProcess::NotRunning) {
        m_voiceCaptureProcess.kill();
        m_voiceCaptureProcess.waitForFinished(250);
    }
    m_voiceCaptureRunning = false;
    setVoiceInputLevel(0.0);
}

void AppController::runVoiceCaptureChunk()
{
    if (!m_duplexVoiceEnabled || !m_micActive || m_voiceCaptureRunning) {
        return;
    }

    const QString arecordBin = QStandardPaths::findExecutable("arecord");
    if (arecordBin.isEmpty()) {
        setModelStatus("arecord not found for duplex voice");
        return;
    }

    if (m_voiceCaptureTempPath.isEmpty()) {
        m_voiceCaptureTempPath = appDataDir() + "/voice_chunk.wav";
    }
    QFile::remove(m_voiceCaptureTempPath);

    m_voiceCaptureRunning = true;
    const QString chunkSeconds = QString::number(voiceChunkSeconds());
    m_voiceCaptureProcess.start(
        arecordBin,
        {"-q", "-f", "S16_LE", "-r", "16000", "-c", "1", "-d", chunkSeconds, "-t", "wav", m_voiceCaptureTempPath});
    if (!m_voiceCaptureProcess.waitForStarted(150)) {
        m_voiceCaptureRunning = false;
        setModelStatus("Unable to start arecord for duplex voice");
        if (m_duplexVoiceEnabled && m_micActive) {
            m_voiceCaptureRestartTimer.start(qMax(180, voiceRestartDelayMs() * 3));
        }
    }
}

void AppController::handleVoiceCaptureFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);

    m_voiceCaptureRunning = false;
    if (!m_duplexVoiceEnabled || !m_micActive) {
        return;
    }

    QFile file(m_voiceCaptureTempPath);
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray wavData = file.readAll();
        qreal voiceLevel = 0.0;
        const bool speechDetected = wavHasSpeech(wavData, &voiceLevel);
        // Decay slightly so the meter stays readable between capture chunks.
        setVoiceInputLevel(qMax(voiceLevel, m_voiceInputLevel * 0.45));
        if (speechDetected) {
            const qint64 nowMs = m_runtimeClock.elapsed();
            if (nowMs < m_echoSuppressUntilMs || isAssistantAudible()) {
                appendAudit("Voice chunk ignored (assistant speaking)");
            } else {
                if (actionRunning() && nowMs - m_lastBargeInMs > voiceBargeDebounceMs()) {
                    m_lastBargeInMs = nowMs;
                    appendAudit("Voice barge-in detected");
                    cancelActiveRequest();
                }
                requestTranscription(wavData);
            }
        }
    }

    if (m_duplexVoiceEnabled && m_micActive) {
        m_voiceCaptureRestartTimer.start(voiceRestartDelayMs());
    }
}

bool AppController::wavHasSpeech(const QByteArray &wavData, qreal *normalizedLevel) const
{
    if (wavData.size() <= 44) {
        if (normalizedLevel) {
            *normalizedLevel = 0.0;
        }
        return false;
    }

    const char *data = wavData.constData() + 44;
    const int bytes = wavData.size() - 44;
    if (bytes < 3200) {
        if (normalizedLevel) {
            *normalizedLevel = 0.0;
        }
        return false;
    }

    qint64 sumAbs = 0;
    int samples = 0;
    for (int i = 0; i + 1 < bytes; i += 2) {
        const qint16 sample = qFromLittleEndian<qint16>(reinterpret_cast<const uchar *>(data + i));
        sumAbs += qAbs(sample);
        ++samples;
    }
    if (samples == 0) {
        if (normalizedLevel) {
            *normalizedLevel = 0.0;
        }
        return false;
    }

    const qint64 avg = sumAbs / samples;
    if (normalizedLevel) {
        const qreal normalized = qBound<qreal>(0.0, static_cast<qreal>(avg) / 2600.0, 1.0);
        *normalizedLevel = normalized;
    }
    return avg > vadAmplitudeThreshold();
}

void AppController::requestTranscription(const QByteArray &wavData)
{
    if (wavData.isEmpty()) {
        return;
    }

    const QString url = normalizedTranscriptionUrl(m_transcriptionEndpoint.isEmpty() ? m_endpoint : m_transcriptionEndpoint);
    if (url.isEmpty()) {
        return;
    }

    if (m_activeTranscriptionReply) {
        return;
    }

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart modelPart;
    modelPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"model\""));
    modelPart.setBody(m_transcriptionModel.toUtf8());
    multiPart->append(modelPart);

    QHttpPart filePart;
    filePart.setHeader(
        QNetworkRequest::ContentDispositionHeader,
        QVariant("form-data; name=\"file\"; filename=\"speech.wav\""));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("audio/wav"));
    filePart.setBody(wavData);
    multiPart->append(filePart);

    QNetworkRequest request{QUrl(url)};
    applyAuthHeader(request);

    m_activeTranscriptionReply = m_network.post(request, multiPart);
    QNetworkReply *reply = m_activeTranscriptionReply;
    multiPart->setParent(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply != m_activeTranscriptionReply) {
            reply->deleteLater();
            return;
        }
        m_activeTranscriptionReply = nullptr;

        if (reply->error() != QNetworkReply::NoError) {
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            reply->deleteLater();
            return;
        }

        QString text = doc.object().value("text").toString().trimmed();
        if (text.isEmpty()) {
            const QJsonArray choices = doc.object().value("choices").toArray();
            if (!choices.isEmpty()) {
                text = choices.first().toObject().value("text").toString().trimmed();
            }
        }

        if (!text.isEmpty()) {
            routeVoiceTranscript(text);
        }
        reply->deleteLater();
    });
}

void AppController::routeVoiceTranscript(const QString &text)
{
    const QString transcript = text.trimmed();
    if (transcript.isEmpty()) {
        return;
    }

    appendAudit("Voice fragment: " + transcript);

    if (m_duplexVoiceEnabled && m_micActive) {
        const QString fragment = transcript.simplified();
        if (!fragment.isEmpty()) {
            if (!m_voiceUtteranceBuffer.isEmpty()) {
                const QString existingLower = m_voiceUtteranceBuffer.toLower();
                const QString fragmentLower = fragment.toLower();
                // Avoid repeating near-identical partial hypotheses from STT.
                if (!existingLower.endsWith(fragmentLower)) {
                    m_voiceUtteranceBuffer += " " + fragment;
                }
            } else {
                m_voiceUtteranceBuffer = fragment;
            }
            m_voiceUtteranceTimer.start(voiceUtteranceHoldMs());
            setFaceState("listening");
            setStatusText("Listening...");
        }
        return;
    }

    dispatchVoiceTranscript(transcript);
}

void AppController::dispatchVoiceTranscript(const QString &transcript)
{
    const QString clean = transcript.trimmed();
    if (clean.isEmpty()) {
        return;
    }

    appendAudit("Voice transcript: " + clean);
    QString routed = clean;
    bool wakeOnly = false;
    parseWakeInput(clean, &routed, &wakeOnly);
    if (wakeOnly) {
        if (!m_wakeResponses.isEmpty()) {
            const int idx = QRandomGenerator::global()->bounded(m_wakeResponses.size());
            postAssistant(m_wakeResponses.at(idx));
        }
        return;
    }

    if (routed.isEmpty()) {
        return;
    }

    m_messageModel.addMessage("user", routed, "user");
    setFaceState("thinking");
    setStatusText("Thinking...");

    if (routed.startsWith('/')) {
        QTimer::singleShot(80, this, [this, routed]() { handleInput(routed); });
        return;
    }

    if (shouldUseRemoteModel()) {
        requestRemoteCompletion(routed);
        return;
    }

    QTimer::singleShot(80, this, [this, routed]() { handleInput(routed); });
}

bool AppController::isAssistantAudible() const
{
    return m_ttsProcess.state() != QProcess::NotRunning
        || m_ttsQueueRunning
        || !m_ttsQueue.isEmpty();
}

QString AppController::expressionFromAssistantText(const QString &text, const QString &kind) const
{
    if (kind == "warning") {
        return "angry";
    }

    const QString lowered = text.toLower();
    if (lowered.contains("error")
        || lowered.contains("failed")
        || lowered.contains("cannot")
        || lowered.contains("can't")
        || lowered.contains("permission denied")
        || lowered.contains("not found")) {
        return "angry";
    }

    if (lowered.contains("i'm not sure")
        || lowered.contains("i am not sure")
        || lowered.contains("unclear")
        || lowered.contains("do you mean")
        || lowered.contains("could you clarify")) {
        return "confused";
    }

    if (lowered.contains("sorry")
        || lowered.contains("i apologize")
        || lowered.contains("unfortunately")
        || lowered.contains("wish i could")) {
        return "sad";
    }

    if (lowered.contains("done")
        || lowered.contains("completed")
        || lowered.contains("success")
        || lowered.contains("great")
        || lowered.contains("nice")
        || lowered.contains("ready")) {
        return "happy";
    }

    return "speaking";
}

int AppController::voiceChunkSeconds() const
{
    if (m_duplexSmoothness == "Responsive") {
        return 1;
    }
    if (m_duplexSmoothness == "Natural") {
        return 3;
    }
    if (m_duplexSmoothness == "Studio") {
        return 4;
    }
    return 2;
}

int AppController::voiceRestartDelayMs() const
{
    if (m_duplexSmoothness == "Responsive") {
        return 30;
    }
    if (m_duplexSmoothness == "Natural") {
        return 90;
    }
    if (m_duplexSmoothness == "Studio") {
        return 120;
    }
    return 60;
}

int AppController::voiceBargeDebounceMs() const
{
    if (m_duplexSmoothness == "Responsive") {
        return 320;
    }
    if (m_duplexSmoothness == "Natural") {
        return 700;
    }
    if (m_duplexSmoothness == "Studio") {
        return 900;
    }
    return 550;
}

int AppController::voiceUtteranceHoldMs() const
{
    if (m_duplexSmoothness == "Responsive") {
        return 380;
    }
    if (m_duplexSmoothness == "Natural") {
        return 900;
    }
    if (m_duplexSmoothness == "Studio") {
        return 1200;
    }
    return 650;
}

int AppController::echoSuppressStartMs() const
{
    if (m_duplexSmoothness == "Responsive") {
        return 420;
    }
    if (m_duplexSmoothness == "Natural") {
        return 860;
    }
    if (m_duplexSmoothness == "Studio") {
        return 980;
    }
    return 720;
}

int AppController::echoSuppressTailMs() const
{
    if (m_duplexSmoothness == "Responsive") {
        return 140;
    }
    if (m_duplexSmoothness == "Natural") {
        return 320;
    }
    if (m_duplexSmoothness == "Studio") {
        return 380;
    }
    return 260;
}

int AppController::vadAmplitudeThreshold() const
{
    const int sensitivity = qBound(1, m_vadSensitivity, 100);
    const int threshold = 1500 - (sensitivity * 13);
    return qBound(180, threshold, 1400);
}

QString AppController::normalizedTranscriptionUrl(const QString &endpoint) const
{
    QString raw = endpoint.trimmed();
    if (raw.isEmpty()) {
        return {};
    }

    if (!raw.startsWith("http://", Qt::CaseInsensitive)
        && !raw.startsWith("https://", Qt::CaseInsensitive)) {
        raw.prepend("http://");
    }

    QUrl url(raw);
    if (!url.isValid()) {
        return {};
    }

    QString path = url.path().trimmed();
    path.remove(QRegularExpression("/+$"));

    if (path.isEmpty()) {
        path = "/v1/audio/transcriptions";
    } else if (path.endsWith("/chat/completions")) {
        path.chop(QString("/chat/completions").size());
        path += "/audio/transcriptions";
    } else if (path.endsWith("/models")) {
        path.chop(QString("/models").size());
        path += "/audio/transcriptions";
    } else if (path.endsWith("/v1")) {
        path += "/audio/transcriptions";
    } else if (!path.endsWith("/audio/transcriptions")) {
        path += "/audio/transcriptions";
    }

    url.setPath(path);
    url.setQuery(QString());
    url.setFragment(QString());
    return url.toString();
}

QString AppController::normalizedCompletionUrl(const QString &endpoint) const
{
    QString raw = endpoint.trimmed();
    if (raw.isEmpty()) {
        return {};
    }

    if (!raw.startsWith("http://", Qt::CaseInsensitive)
        && !raw.startsWith("https://", Qt::CaseInsensitive)) {
        raw.prepend("http://");
    }

    QUrl url(raw);
    if (!url.isValid()) {
        return {};
    }

    QString path = url.path().trimmed();
    path.remove(QRegularExpression("/+$"));

    if (path.isEmpty()) {
        path = "/v1/chat/completions";
    } else if (path.endsWith("/v1/models")) {
        path.chop(QString("/models").size());
        path += "/chat/completions";
    } else if (path.endsWith("/v1")) {
        path += "/chat/completions";
    } else if (path.endsWith("/chat/completions")) {
        // already normalized
    } else if (path.endsWith("/models")) {
        path.chop(QString("/models").size());
        path += "/chat/completions";
    } else {
        path += "/chat/completions";
    }

    url.setPath(path);
    url.setQuery(QString());
    url.setFragment(QString());
    return url.toString();
}

QString AppController::normalizedModelsOverrideUrl(const QString &endpoint) const
{
    QString raw = endpoint.trimmed();
    if (raw.isEmpty()) {
        return {};
    }

    if (!raw.startsWith("http://", Qt::CaseInsensitive)
        && !raw.startsWith("https://", Qt::CaseInsensitive)) {
        raw.prepend("http://");
    }

    QUrl url(raw);
    if (!url.isValid()) {
        return {};
    }

    QString path = url.path().trimmed();
    path.remove(QRegularExpression("/+$"));

    if (path.isEmpty()) {
        path = "/v1/models";
    } else if (path.endsWith("/chat/completions")) {
        path.chop(QString("/chat/completions").size());
        path += "/models";
    }

    url.setPath(path);
    url.setQuery(QString());
    url.setFragment(QString());
    return url.toString();
}

QString AppController::modelsUrlFromEndpoint(const QString &endpoint) const
{
    const QString completionUrl = normalizedCompletionUrl(endpoint);
    if (completionUrl.isEmpty()) {
        return {};
    }

    QUrl url(completionUrl);
    QString path = url.path();
    if (path.endsWith("/chat/completions")) {
        path.chop(QString("/chat/completions").size());
    }
    path += "/models";

    url.setPath(path);
    url.setQuery(QString());
    url.setFragment(QString());
    return url.toString();
}

void AppController::applyAuthHeader(QNetworkRequest &request) const
{
    if (m_apiKey.trimmed().isEmpty()) {
        return;
    }

    const QByteArray bearer = "Bearer " + m_apiKey.trimmed().toUtf8();
    request.setRawHeader("Authorization", bearer);
}

bool AppController::parseWakeInput(const QString &raw, QString *trimmedMessage, bool *wakeOnly) const
{
    if (trimmedMessage) {
        *trimmedMessage = raw.trimmed();
    }
    if (wakeOnly) {
        *wakeOnly = false;
    }

    if (!m_wakeEnabled) {
        return false;
    }

    const QString name = m_assistantName.trimmed();
    if (name.isEmpty()) {
        return false;
    }

    const QString input = raw.trimmed();
    if (input.isEmpty()) {
        return false;
    }

    const QString lowered = input.toLower();
    const QString escapedName = QRegularExpression::escape(name.toLower());

    QRegularExpression prefixedPattern(
        QString("^\\s*(hey|hi|hello)\\s+%1([\\s,!.?:;-]*)").arg(escapedName),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpression directPattern(
        QString("^\\s*%1([\\s,!.?:;-]*)").arg(escapedName),
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch match = prefixedPattern.match(lowered);
    if (!match.hasMatch()) {
        match = directPattern.match(lowered);
    }
    if (!match.hasMatch()) {
        return false;
    }

    const int consumed = match.capturedLength(0);
    const QString remainder = input.mid(consumed).trimmed();
    if (trimmedMessage) {
        *trimmedMessage = remainder;
    }
    if (wakeOnly) {
        *wakeOnly = remainder.isEmpty();
    }
    return true;
}

void AppController::loadProfile(const QString &profileName)
{
    const QString trimmed = profileName.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    QSettings settings;
    const QString base = QString("profiles/%1/").arg(trimmed);

    const QString profileProvider = settings.value(base + "provider", m_provider).toString();
    const QString profileEndpoint = normalizedCompletionUrl(
        settings.value(base + "endpoint", m_endpoint).toString());
    const QString profileModelsEndpoint = normalizedModelsOverrideUrl(
        settings.value(base + "modelsEndpoint", m_modelsEndpoint).toString());
    const bool profileCameraEnabled = settings.value(base + "cameraEnabled", m_cameraEnabled).toBool();
    const QString profileApiKey = settings.value(base + "apiKey", m_apiKey).toString();
    const QString profileModel = settings.value(base + "selectedModel", m_selectedModel).toString().trimmed();
    const QString profileSmoothing = settings.value(base + "smoothingProfile", m_smoothingProfile).toString();
    const int profileTokenRate = qBound(20, settings.value(base + "tokenRate", m_tokenRate).toInt(), 600);
    const int profileLipSyncDelayMs = qBound(0, settings.value(base + "lipSyncDelayMs", m_lipSyncDelayMs).toInt(), 1100);
    const QString profileAssistantName = settings.value(base + "assistantName", m_assistantName).toString().trimmed();
    const bool profileWakeEnabled = settings.value(base + "wakeEnabled", m_wakeEnabled).toBool();
    QStringList profileWakeResponses = settings.value(base + "wakeResponses", m_wakeResponses).toStringList();
    const bool profileConversationalMode = settings.value(base + "conversationalMode", m_conversationalMode).toBool();
    const QString profilePersonality = settings.value(base + "personality", m_personality).toString();
    const QString profileFaceStyle = settings.value(base + "faceStyle", m_faceStyle).toString();
    const QString profileGender = settings.value(base + "gender", m_gender).toString();
    const QString profileVoiceStyle = settings.value(base + "voiceStyle", m_voiceStyle).toString();
    const QString profileVoiceEngine = settings.value(base + "voiceEngine", m_voiceEngine).toString();
    const QString profilePiperModelPath = expandPath(
        settings.value(base + "piperModelPath", m_piperModelPath).toString());
    const bool profileDuplexVoiceEnabled = settings.value(base + "duplexVoiceEnabled", m_duplexVoiceEnabled).toBool();
    const QString profileTranscriptionEndpoint = normalizedTranscriptionUrl(
        settings.value(base + "transcriptionEndpoint", m_transcriptionEndpoint).toString());
    QString profileTranscriptionModel = settings.value(base + "transcriptionModel", m_transcriptionModel).toString().trimmed();
    const int profileVadSensitivity = qBound(1, settings.value(base + "vadSensitivity", m_vadSensitivity).toInt(), 100);
    const QString profileDuplexSmoothness = settings.value(base + "duplexSmoothness", m_duplexSmoothness).toString();
    if (profileTranscriptionModel.isEmpty()) {
        profileTranscriptionModel = m_transcriptionModel;
    }

    if (providers().contains(profileProvider) && m_provider != profileProvider) {
        m_provider = profileProvider;
        emit providerChanged();
    }

    if (!profileEndpoint.isEmpty() && m_endpoint != profileEndpoint) {
        m_endpoint = profileEndpoint;
        emit endpointChanged();
    }

    if (m_modelsEndpoint != profileModelsEndpoint) {
        m_modelsEndpoint = profileModelsEndpoint;
        emit modelsEndpointChanged();
    }

    if (m_cameraEnabled != profileCameraEnabled) {
        m_cameraEnabled = profileCameraEnabled;
        emit cameraEnabledChanged();
    }

    if (m_apiKey != profileApiKey) {
        m_apiKey = profileApiKey;
        emit apiKeyChanged();
    }

    if (!profileModel.isEmpty() && m_selectedModel != profileModel) {
        if (!m_availableModels.contains(profileModel)) {
            m_availableModels.append(profileModel);
            emit availableModelsChanged();
        }
        m_selectedModel = profileModel;
        emit selectedModelChanged();
        emit modelNameChanged();
    }

    if (smoothingProfiles().contains(profileSmoothing) && m_smoothingProfile != profileSmoothing) {
        m_smoothingProfile = profileSmoothing;
        m_streamFlushTimer.setInterval(flushIntervalForProfile());
        emit smoothingProfileChanged();
    }

    if (m_tokenRate != profileTokenRate) {
        m_tokenRate = profileTokenRate;
        m_streamFlushTimer.setInterval(flushIntervalForProfile());
        emit tokenRateChanged();
    }

    if (m_lipSyncDelayMs != profileLipSyncDelayMs) {
        m_lipSyncDelayMs = profileLipSyncDelayMs;
        emit lipSyncDelayMsChanged();
    }

    if (!profileAssistantName.isEmpty() && m_assistantName != profileAssistantName) {
        m_assistantName = profileAssistantName;
        emit assistantNameChanged();
    }

    if (m_wakeEnabled != profileWakeEnabled) {
        m_wakeEnabled = profileWakeEnabled;
        emit wakeEnabledChanged();
    }

    if (profileWakeResponses.isEmpty()) {
        profileWakeResponses = m_wakeResponses;
    }
    if (m_wakeResponses != profileWakeResponses) {
        m_wakeResponses = profileWakeResponses;
        emit wakeResponsesChanged();
    }

    if (m_conversationalMode != profileConversationalMode) {
        m_conversationalMode = profileConversationalMode;
        emit conversationalModeChanged();
    }

    if (personalities().contains(profilePersonality) && m_personality != profilePersonality) {
        m_personality = profilePersonality;
        emit personalityChanged();
    }

    if (faceStyles().contains(profileFaceStyle) && m_faceStyle != profileFaceStyle) {
        m_faceStyle = profileFaceStyle;
        emit faceStyleChanged();
    }

    if (genders().contains(profileGender) && m_gender != profileGender) {
        m_gender = profileGender;
        emit genderChanged();
    }

    if (voiceStyles().contains(profileVoiceStyle) && m_voiceStyle != profileVoiceStyle) {
        m_voiceStyle = profileVoiceStyle;
        emit voiceStyleChanged();
    }

    if (voiceEngines().contains(profileVoiceEngine) && m_voiceEngine != profileVoiceEngine) {
        m_voiceEngine = profileVoiceEngine;
        m_ttsBinary.clear();
        emit voiceEngineChanged();
    }

    if (m_piperModelPath != profilePiperModelPath) {
        m_piperModelPath = profilePiperModelPath;
        emit piperModelPathChanged();
    }

    if (m_duplexVoiceEnabled != profileDuplexVoiceEnabled) {
        m_duplexVoiceEnabled = profileDuplexVoiceEnabled;
        emit duplexVoiceEnabledChanged();
    }

    if (m_transcriptionEndpoint != profileTranscriptionEndpoint) {
        m_transcriptionEndpoint = profileTranscriptionEndpoint;
        emit transcriptionEndpointChanged();
    }

    if (!profileTranscriptionModel.isEmpty() && m_transcriptionModel != profileTranscriptionModel) {
        m_transcriptionModel = profileTranscriptionModel;
        emit transcriptionModelChanged();
    }

    if (m_vadSensitivity != profileVadSensitivity) {
        m_vadSensitivity = profileVadSensitivity;
        emit vadSensitivityChanged();
    }

    if (duplexSmoothnessOptions().contains(profileDuplexSmoothness)
        && m_duplexSmoothness != profileDuplexSmoothness) {
        m_duplexSmoothness = profileDuplexSmoothness;
        emit duplexSmoothnessChanged();
    }
}

void AppController::saveProfileToSettings(const QString &profileName) const
{
    const QString trimmed = profileName.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    QSettings settings;
    const QString base = QString("profiles/%1/").arg(trimmed);
    settings.setValue(base + "provider", m_provider);
    settings.setValue(base + "cameraEnabled", m_cameraEnabled);
    settings.setValue(base + "endpoint", m_endpoint);
    settings.setValue(base + "modelsEndpoint", m_modelsEndpoint);
    settings.setValue(base + "apiKey", m_apiKey);
    settings.setValue(base + "selectedModel", m_selectedModel);
    settings.setValue(base + "smoothingProfile", m_smoothingProfile);
    settings.setValue(base + "tokenRate", m_tokenRate);
    settings.setValue(base + "lipSyncDelayMs", m_lipSyncDelayMs);
    settings.setValue(base + "assistantName", m_assistantName);
    settings.setValue(base + "wakeEnabled", m_wakeEnabled);
    settings.setValue(base + "wakeResponses", m_wakeResponses);
    settings.setValue(base + "conversationalMode", m_conversationalMode);
    settings.setValue(base + "personality", m_personality);
    settings.setValue(base + "faceStyle", m_faceStyle);
    settings.setValue(base + "gender", m_gender);
    settings.setValue(base + "voiceStyle", m_voiceStyle);
    settings.setValue(base + "voiceEngine", m_voiceEngine);
    settings.setValue(base + "piperModelPath", m_piperModelPath);
    settings.setValue(base + "duplexVoiceEnabled", m_duplexVoiceEnabled);
    settings.setValue(base + "transcriptionEndpoint", m_transcriptionEndpoint);
    settings.setValue(base + "transcriptionModel", m_transcriptionModel);
    settings.setValue(base + "vadSensitivity", m_vadSensitivity);
    settings.setValue(base + "duplexSmoothness", m_duplexSmoothness);
}

int AppController::chunkSizeForBacklog(int backlog) const
{
    if (m_smoothingProfile == "Instant") {
        return qMax(1, backlog);
    }

    int base = 2;

    if (m_smoothingProfile == "Cinematic") {
        base = backlog > 260 ? 3 : (backlog > 160 ? 2 : 1);
    } else if (m_smoothingProfile == "Human") {
        if (backlog > 180) {
            base = 6;
        } else if (backlog > 90) {
            base = 4;
        } else if (backlog > 30) {
            base = 3;
        } else {
            base = 1;
        }
    } else if (m_smoothingProfile == "Terminal") {
        if (backlog > 180) {
            base = 20;
        } else if (backlog > 120) {
            base = 14;
        } else if (backlog > 70) {
            base = 9;
        } else if (backlog > 30) {
            base = 6;
        } else if (backlog > 14) {
            base = 4;
        } else {
            base = 2;
        }
    } else {
        if (backlog > 180) {
            base = 14;
        } else if (backlog > 120) {
            base = 10;
        } else if (backlog > 70) {
            base = 7;
        } else if (backlog > 30) {
            base = 5;
        } else if (backlog > 14) {
            base = 3;
        } else {
            base = 2;
        }
    }

    const int scaled = qMax(1, (base * m_tokenRate) / 100);
    return scaled;
}

int AppController::flushIntervalForProfile() const
{
    if (m_smoothingProfile == "Instant") {
        return 2;
    }

    int base = 24;
    if (m_smoothingProfile == "Cinematic") {
        base = 54;
    } else if (m_smoothingProfile == "Human") {
        base = 38;
    } else if (m_smoothingProfile == "Terminal") {
        base = 14;
    }

    const int scaled = qMax(8, (base * 100) / qMax(20, m_tokenRate));
    return scaled;
}

bool AppController::shouldUseRemoteModel() const
{
    return m_selectedModel != "local-prototype" && !normalizedCompletionUrl(m_endpoint).isEmpty();
}

bool AppController::isRiskyCommand(const QString &command) const
{
    static const QRegularExpression riskyPattern(
        "(^|\\s)(rm|mv|chmod|chown|dd|mkfs|sudo|shutdown|reboot)(\\s|$)",
        QRegularExpression::CaseInsensitiveOption);

    if (riskyPattern.match(command).hasMatch()) {
        return true;
    }

    return command.contains('>') || command.contains(">>");
}
