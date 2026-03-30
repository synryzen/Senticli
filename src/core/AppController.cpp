#include "AppController.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

namespace {
QString expandPath(const QString &raw)
{
    QString path = raw.trimmed();
    if (path.startsWith("~/")) {
        path.replace(0, 1, QDir::homePath());
    }
    return QDir::cleanPath(path);
}
} // namespace

AppController::AppController(QObject *parent)
    : QObject(parent)
{
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

    connect(&m_ttsProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int, QProcess::ExitStatus) {
        appendAudit("TTS finished");
    });

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

QString AppController::personality() const
{
    return m_personality;
}

QStringList AppController::personalities() const
{
    return {"Helpful", "Professional", "Witty", "Teacher", "Hacker", "Calm"};
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
    return m_streamingActive || m_commandRunning;
}

bool AppController::pendingApproval() const
{
    return m_pendingApproval;
}

QString AppController::pendingApprovalText() const
{
    return m_pendingApprovalText;
}

void AppController::sendUserInput(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    if (m_micActive) {
        m_micActive = false;
        emit micActiveChanged();
    }

    m_messageModel.addMessage("user", trimmed, "user");
    setFaceState("thinking");
    setStatusText("Thinking...");

    if (trimmed.startsWith('/')) {
        QTimer::singleShot(120, this, [this, trimmed]() { handleInput(trimmed); });
        return;
    }

    if (shouldUseRemoteModel()) {
        requestRemoteCompletion(trimmed);
        return;
    }

    QTimer::singleShot(120, this, [this, trimmed]() { handleInput(trimmed); });
}

void AppController::toggleMic()
{
    m_micActive = !m_micActive;
    emit micActiveChanged();

    if (m_micActive) {
        setFaceState("listening");
        setStatusText("Listening (STT integration pending)...");
    } else if (m_pendingApproval) {
        setFaceState("warning");
        setStatusText("Waiting for approval");
    } else {
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

    if (m_ttsProcess.state() != QProcess::NotRunning) {
        stopSpeaking();
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
    updateStreamingMessageDisplay(false);
    setModelStatus(QString("Connected: %1").arg(m_selectedModel));
    appendAudit("Completion received");
    clearStreamingState();
    if (m_ttsEnabled) {
        speakText(finalText);
    }
    QTimer::singleShot(200, this, [this]() { finalizeAssistantState(); });
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

    m_shellProcess.start("/bin/bash", {"-lc", command});
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
    const QString merged = out + err;

    if (!merged.isEmpty()) {
        m_messageModel.appendTextAt(m_shellOutputRow, merged);
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

void AppController::speakText(const QString &text)
{
    if (!m_ttsEnabled) {
        return;
    }

    QString toSpeak = text.trimmed();
    if (toSpeak.isEmpty()) {
        return;
    }

    if (toSpeak.size() > 700) {
        toSpeak = toSpeak.left(700);
    }

    if (m_ttsBinary.isEmpty()) {
        m_ttsBinary = findTtsBinary();
    }

    if (m_ttsBinary.isEmpty()) {
        appendAudit("No TTS backend found (needs spd-say or espeak)");
        return;
    }

    stopSpeaking();

    QStringList args;
    if (m_ttsBinary.endsWith("spd-say")) {
        QString voiceToken = "neutral";
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

        args << "-t" << voiceToken << "-r" << QString::number(rate) << "-p" << QString::number(pitch);
        args << toSpeak;
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
    }

    m_ttsProcess.start(m_ttsBinary, args);
    appendAudit("TTS started");
}

void AppController::stopSpeaking()
{
    if (m_ttsProcess.state() == QProcess::NotRunning) {
        return;
    }
    m_ttsProcess.kill();
    m_ttsProcess.waitForFinished(300);
    appendAudit("TTS stopped");
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
    m_personality = settings.value("ai/personality", m_personality).toString();
    if (!personalities().contains(m_personality)) {
        m_personality = "Helpful";
    }
    m_gender = settings.value("ai/gender", m_gender).toString();
    if (!genders().contains(m_gender)) {
        m_gender = "Neutral";
    }
    m_voiceStyle = settings.value("voice/style", m_voiceStyle).toString();
    if (!voiceStyles().contains(m_voiceStyle)) {
        m_voiceStyle = "Default";
    }
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
    settings.setValue("ai/endpoint", m_endpoint);
    settings.setValue("ai/modelsEndpoint", m_modelsEndpoint);
    settings.setValue("ai/apiKey", m_apiKey);
    settings.setValue("ai/selectedModel", m_selectedModel);
    settings.setValue("ai/availableModels", m_availableModels);
    settings.setValue("ai/smoothingProfile", m_smoothingProfile);
    settings.setValue("ai/tokenRate", m_tokenRate);
    settings.setValue("ai/personality", m_personality);
    settings.setValue("ai/gender", m_gender);
    settings.setValue("voice/ttsEnabled", m_ttsEnabled);
    settings.setValue("voice/style", m_voiceStyle);
    settings.setValue("memory/enabled", m_memoryEnabled);
    settings.setValue("permissions/grantedFolders", m_grantedFolders);
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
    QString bin = QStandardPaths::findExecutable("spd-say");
    if (!bin.isEmpty()) {
        return bin;
    }

    bin = QStandardPaths::findExecutable("espeak");
    return bin;
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
            "/models\n"
            "/model <id>\n"
            "/speed <Instant|Terminal|Balanced|Human|Cinematic>\n"
            "/personality <Helpful|Professional|Witty|Teacher|Hacker|Calm>\n"
            "/gender <Neutral|Male|Female>\n"
            "/voice-style <Default|Soft|Bright|Narrator>\n"
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
            QString("Voice options:\nGender: %1\nStyle: %2")
                .arg(genders().join(", "), voiceStyles().join(", ")));
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

    if (lowered.startsWith("/model ")) {
        setSelectedModel(text.mid(7).trimmed());
        return;
    }

    if (lowered.startsWith("/speed ")) {
        setSmoothingProfile(text.mid(7).trimmed());
        return;
    }

    if (lowered.startsWith("/personality ")) {
        setPersonality(text.mid(13).trimmed());
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
    if (m_speakingActive) {
        m_speakingActive = false;
        emit speakingActiveChanged();
    }

    if (m_pendingApproval) {
        setFaceState("warning");
        setStatusText("Waiting for approval");
    } else if (m_micActive) {
        setFaceState("listening");
        setStatusText("Listening...");
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
    m_streamSawToken = false;
    m_cancelRequested = false;
    m_streamFinalizePending = false;
    m_streamingMessageRow = m_messageModel.addMessageWithRow("assistant", "", "assistant");
    m_streamFlushTimer.setInterval(flushIntervalForProfile());
    setStreamingActive(true);
    updateStreamingMessageDisplay(true);

    m_speakingActive = true;
    emit speakingActiveChanged();
    setFaceState("speaking");
    setStatusText("Streaming response...");
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
            if (m_speakingActive) {
                m_speakingActive = false;
                emit speakingActiveChanged();
            }
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
            if (m_speakingActive) {
                m_speakingActive = false;
                emit speakingActiveChanged();
            }
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

    return QString(
               "You are Senticli, a local Linux terminal companion. "
               "Keep replies %1. "
               "Current presentation preferences: personality=%2, voice-gender=%3, voice-style=%4. "
               "When suggesting actions, be explicit and safe.")
        .arg(tone, m_personality, m_gender, m_voiceStyle);
}

void AppController::postAssistant(const QString &text, const QString &kind)
{
    m_messageModel.addMessage("assistant", text, kind);

    if (kind == "warning") {
        setFaceState("warning");
        setStatusText("Attention required");
        return;
    }

    m_speakingActive = true;
    emit speakingActiveChanged();
    setFaceState("speaking");
    setStatusText("Speaking...");

    if (m_ttsEnabled) {
        speakText(text);
    }

    QTimer::singleShot(820, this, [this]() { finalizeAssistantState(); });
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
    const QString profileApiKey = settings.value(base + "apiKey", m_apiKey).toString();
    const QString profileModel = settings.value(base + "selectedModel", m_selectedModel).toString().trimmed();
    const QString profileSmoothing = settings.value(base + "smoothingProfile", m_smoothingProfile).toString();
    const int profileTokenRate = qBound(20, settings.value(base + "tokenRate", m_tokenRate).toInt(), 600);
    const QString profilePersonality = settings.value(base + "personality", m_personality).toString();
    const QString profileGender = settings.value(base + "gender", m_gender).toString();
    const QString profileVoiceStyle = settings.value(base + "voiceStyle", m_voiceStyle).toString();

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

    if (personalities().contains(profilePersonality) && m_personality != profilePersonality) {
        m_personality = profilePersonality;
        emit personalityChanged();
    }

    if (genders().contains(profileGender) && m_gender != profileGender) {
        m_gender = profileGender;
        emit genderChanged();
    }

    if (voiceStyles().contains(profileVoiceStyle) && m_voiceStyle != profileVoiceStyle) {
        m_voiceStyle = profileVoiceStyle;
        emit voiceStyleChanged();
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
    settings.setValue(base + "endpoint", m_endpoint);
    settings.setValue(base + "modelsEndpoint", m_modelsEndpoint);
    settings.setValue(base + "apiKey", m_apiKey);
    settings.setValue(base + "selectedModel", m_selectedModel);
    settings.setValue(base + "smoothingProfile", m_smoothingProfile);
    settings.setValue(base + "tokenRate", m_tokenRate);
    settings.setValue(base + "personality", m_personality);
    settings.setValue(base + "gender", m_gender);
    settings.setValue(base + "voiceStyle", m_voiceStyle);
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
