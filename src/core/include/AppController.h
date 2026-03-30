#pragma once

#include <QNetworkAccessManager>
#include <QPointer>
#include <QObject>
#include <QStringList>
#include <QByteArray>
#include <QTimer>
#include <QProcess>
#include <QHash>

#include "models/MessageModel.h"

class QNetworkReply;
class QJsonObject;

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(MessageModel *messageModel READ messageModel CONSTANT)
    Q_PROPERTY(QString faceState READ faceState NOTIFY faceStateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(QString endpoint READ endpoint WRITE setEndpoint NOTIFY endpointChanged)
    Q_PROPERTY(QString modelsEndpoint READ modelsEndpoint WRITE setModelsEndpoint NOTIFY modelsEndpointChanged)
    Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY apiKeyChanged)
    Q_PROPERTY(QString provider READ provider WRITE setProvider NOTIFY providerChanged)
    Q_PROPERTY(QStringList providers READ providers CONSTANT)
    Q_PROPERTY(QStringList connectionProfiles READ connectionProfiles NOTIFY connectionProfilesChanged)
    Q_PROPERTY(QString activeProfile READ activeProfile NOTIFY activeProfileChanged)
    Q_PROPERTY(QStringList availableModels READ availableModels NOTIFY availableModelsChanged)
    Q_PROPERTY(QString selectedModel READ selectedModel WRITE setSelectedModel NOTIFY selectedModelChanged)
    Q_PROPERTY(QString modelStatus READ modelStatus NOTIFY modelStatusChanged)
    Q_PROPERTY(QString smoothingProfile READ smoothingProfile WRITE setSmoothingProfile NOTIFY smoothingProfileChanged)
    Q_PROPERTY(QStringList smoothingProfiles READ smoothingProfiles CONSTANT)
    Q_PROPERTY(int tokenRate READ tokenRate WRITE setTokenRate NOTIFY tokenRateChanged)
    Q_PROPERTY(QString personality READ personality WRITE setPersonality NOTIFY personalityChanged)
    Q_PROPERTY(QStringList personalities READ personalities CONSTANT)
    Q_PROPERTY(QString gender READ gender WRITE setGender NOTIFY genderChanged)
    Q_PROPERTY(QStringList genders READ genders CONSTANT)
    Q_PROPERTY(QString voiceStyle READ voiceStyle WRITE setVoiceStyle NOTIFY voiceStyleChanged)
    Q_PROPERTY(QStringList voiceStyles READ voiceStyles CONSTANT)
    Q_PROPERTY(bool ttsEnabled READ ttsEnabled WRITE setTtsEnabled NOTIFY ttsEnabledChanged)
    Q_PROPERTY(bool memoryEnabled READ memoryEnabled WRITE setMemoryEnabled NOTIFY memoryEnabledChanged)
    Q_PROPERTY(QStringList grantedFolders READ grantedFolders NOTIFY grantedFoldersChanged)
    Q_PROPERTY(QStringList auditEntries READ auditEntries NOTIFY auditEntriesChanged)
    Q_PROPERTY(bool setupComplete READ setupComplete NOTIFY setupCompleteChanged)
    Q_PROPERTY(QString modelName READ modelName NOTIFY modelNameChanged)
    Q_PROPERTY(QString folderScope READ folderScope NOTIFY folderScopeChanged)
    Q_PROPERTY(bool micActive READ micActive NOTIFY micActiveChanged)
    Q_PROPERTY(bool speakingActive READ speakingActive NOTIFY speakingActiveChanged)
    Q_PROPERTY(bool streamingActive READ streamingActive NOTIFY streamingActiveChanged)
    Q_PROPERTY(bool commandRunning READ commandRunning NOTIFY commandRunningChanged)
    Q_PROPERTY(bool actionRunning READ actionRunning NOTIFY actionRunningChanged)
    Q_PROPERTY(bool pendingApproval READ pendingApproval NOTIFY pendingApprovalChanged)
    Q_PROPERTY(QString pendingApprovalText READ pendingApprovalText NOTIFY pendingApprovalChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    MessageModel *messageModel();
    QString faceState() const;
    QString statusText() const;
    QString mode() const;
    QString endpoint() const;
    QString modelsEndpoint() const;
    QString apiKey() const;
    QString provider() const;
    QStringList connectionProfiles() const;
    QString activeProfile() const;
    QStringList providers() const;
    QStringList availableModels() const;
    QString selectedModel() const;
    QString modelStatus() const;
    QString smoothingProfile() const;
    QStringList smoothingProfiles() const;
    int tokenRate() const;
    QString personality() const;
    QStringList personalities() const;
    QString gender() const;
    QStringList genders() const;
    QString voiceStyle() const;
    QStringList voiceStyles() const;
    bool ttsEnabled() const;
    bool memoryEnabled() const;
    QStringList grantedFolders() const;
    QStringList auditEntries() const;
    bool setupComplete() const;
    QString modelName() const;
    QString folderScope() const;
    bool micActive() const;
    bool speakingActive() const;
    bool streamingActive() const;
    bool commandRunning() const;
    bool actionRunning() const;
    bool pendingApproval() const;
    QString pendingApprovalText() const;

    Q_INVOKABLE void sendUserInput(const QString &text);
    Q_INVOKABLE void toggleMic();
    Q_INVOKABLE void setMode(const QString &mode);
    Q_INVOKABLE void setEndpoint(const QString &endpoint);
    Q_INVOKABLE void setModelsEndpoint(const QString &modelsEndpoint);
    Q_INVOKABLE void setApiKey(const QString &apiKey);
    Q_INVOKABLE void setProvider(const QString &provider);
    Q_INVOKABLE void setActiveProfile(const QString &profileName);
    Q_INVOKABLE void saveCurrentProfile(const QString &profileName);
    Q_INVOKABLE void deleteProfile(const QString &profileName);
    Q_INVOKABLE void setSelectedModel(const QString &model);
    Q_INVOKABLE void setSmoothingProfile(const QString &profile);
    Q_INVOKABLE void setTokenRate(int rate);
    Q_INVOKABLE void setPersonality(const QString &personality);
    Q_INVOKABLE void setGender(const QString &gender);
    Q_INVOKABLE void setVoiceStyle(const QString &voiceStyle);
    Q_INVOKABLE void setTtsEnabled(bool enabled);
    Q_INVOKABLE void setMemoryEnabled(bool enabled);
    Q_INVOKABLE void refreshModels();
    Q_INVOKABLE void testProviderConnection();
    Q_INVOKABLE void addGrantedFolder(const QString &folder);
    Q_INVOKABLE void removeGrantedFolder(const QString &folder);
    Q_INVOKABLE void clearAuditEntries();
    Q_INVOKABLE void completeSetup();
    Q_INVOKABLE void cancelActiveRequest();
    Q_INVOKABLE void approvePendingAction(bool approved);

signals:
    void faceStateChanged();
    void statusTextChanged();
    void modeChanged();
    void endpointChanged();
    void modelsEndpointChanged();
    void apiKeyChanged();
    void providerChanged();
    void connectionProfilesChanged();
    void activeProfileChanged();
    void availableModelsChanged();
    void selectedModelChanged();
    void modelStatusChanged();
    void smoothingProfileChanged();
    void tokenRateChanged();
    void personalityChanged();
    void genderChanged();
    void voiceStyleChanged();
    void ttsEnabledChanged();
    void memoryEnabledChanged();
    void grantedFoldersChanged();
    void auditEntriesChanged();
    void setupCompleteChanged();
    void modelNameChanged();
    void folderScopeChanged();
    void micActiveChanged();
    void speakingActiveChanged();
    void streamingActiveChanged();
    void commandRunningChanged();
    void actionRunningChanged();
    void pendingApprovalChanged();

private:
    void setFaceState(const QString &state);
    void setStatusText(const QString &text);
    void setModelStatus(const QString &text);
    void setStreamingActive(bool active);
    void updateStreamingMessageDisplay(bool showCursor);
    void queueStreamText(const QString &text);
    void flushStreamChunk();
    void maybeFinalizeSuccessfulStream();
    void clearStreamingState();
    void setCommandRunning(bool running);
    void startShellCommand(const QString &command);
    void cancelShellCommand();
    void handleShellOutput();
    void handleShellFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void speakText(const QString &text);
    void stopSpeaking();
    QString currentProjectKey() const;
    QString appDataDir() const;
    QString auditLogPath() const;
    QString memoryPath() const;
    void loadSettings();
    void saveSettings() const;
    void appendAudit(const QString &entry);
    void loadAuditEntries();
    void saveAuditEntries() const;
    void loadProjectMemory();
    void saveProjectMemory() const;
    QString projectMemoryContext() const;
    QString findTtsBinary() const;
    void handleInput(const QString &text);
    void finalizeAssistantState();
    void requestRemoteCompletion(const QString &text);
    QString extractContentFromChoice(const QJsonObject &choiceObject) const;
    QString systemPersonaPrompt() const;
    void postAssistant(const QString &text, const QString &kind = "assistant");
    void requestApproval(const QString &prompt, const QString &actionKind, const QString &payload);
    void runShellCommandPreview(const QString &command, bool approved);
    QString normalizedCompletionUrl(const QString &endpoint) const;
    QString normalizedModelsOverrideUrl(const QString &endpoint) const;
    QString modelsUrlFromEndpoint(const QString &endpoint) const;
    void applyAuthHeader(QNetworkRequest &request) const;
    void loadProfile(const QString &profileName);
    void saveProfileToSettings(const QString &profileName) const;
    int chunkSizeForBacklog(int backlog) const;
    int flushIntervalForProfile() const;
    bool shouldUseRemoteModel() const;
    bool isRiskyCommand(const QString &command) const;

    MessageModel m_messageModel;
    QString m_faceState = "idle";
    QString m_statusText = "Ready";
    QString m_mode = "Assist";
    QString m_provider = "Custom";
    QString m_endpoint = "http://127.0.0.1:1234/v1/chat/completions";
    QString m_modelsEndpoint;
    QString m_apiKey;
    QStringList m_connectionProfiles;
    QString m_activeProfile = "Default";
    QStringList m_availableModels = {"local-prototype"};
    QString m_selectedModel = "local-prototype";
    QString m_modelStatus = "Prototype mode (local rules)";
    QString m_smoothingProfile = "Terminal";
    int m_tokenRate = 180;
    QString m_personality = "Helpful";
    QString m_gender = "Neutral";
    QString m_voiceStyle = "Default";
    bool m_ttsEnabled = false;
    bool m_memoryEnabled = true;
    bool m_setupComplete = false;
    QStringList m_grantedFolders;
    QStringList m_auditEntries;
    QString m_folderScope = "~/Documents, ~/Projects";
    bool m_micActive = false;
    bool m_speakingActive = false;
    bool m_streamingActive = false;
    bool m_commandRunning = false;
    bool m_pendingApproval = false;
    QString m_pendingApprovalText;
    QString m_pendingActionKind;
    QString m_pendingPayload;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_activeChatReply;
    QPointer<QNetworkReply> m_activeModelsReply;
    QPointer<QNetworkReply> m_activeHealthReply;
    QByteArray m_chatStreamBuffer;
    QByteArray m_chatRawBuffer;
    QString m_streamAccumulatedText;
    QString m_streamPendingText;
    int m_streamingMessageRow = -1;
    bool m_streamSawToken = false;
    bool m_cancelRequested = false;
    bool m_streamFinalizePending = false;
    QTimer m_streamFlushTimer;
    QTimer m_shellTimeoutTimer;
    int m_shellOutputRow = -1;
    QProcess m_shellProcess;
    QString m_lastShellCommand;
    QProcess m_ttsProcess;
    QString m_ttsBinary;
    QHash<QString, QStringList> m_projectMemory;
};
