/*
 * SampleApplication.cpp
 *
 * Copyright (c) 2017-2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *     http://aws.amazon.com/apache2.0/
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include "SampleApp/ConnectionObserver.h"
#include "SampleApp/GuiRenderer.h"
#include "SampleApp/SampleApplication.h"

#include <AVSCommon/AVS/Initialization/AlexaClientSDKInit.h>
#include <AVSCommon/Utils/Configuration/ConfigurationNode.h>
#include <AVSCommon/Utils/LibcurlUtils/HTTPContentFetcherFactory.h>
#include <AVSCommon/Utils/Logger/LoggerSinkManager.h>
#include <Alerts/Storage/SQLiteAlertStorage.h>
#include <Audio/AudioFactory.h>
#include <AuthDelegate/AuthDelegate.h>
#include <MediaPlayer/MediaPlayer.h>
#include <Notifications/SQLiteNotificationsStorage.h>
#include <Settings/SQLiteSettingStorage.h>

#include <algorithm>
#include <cctype>
#include <fstream>

// 2018/05/04 Bling Added
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

struct my_msgbuf {
    long mtype;
    char mtext[20];
};

FILE *saFile;
char saBuffer[20];

const std::string& logLevel = "NONE";
const std::string& pathToConfig = "/home/parallels/AlexaClientSDKConfig.json";

const char filePath[30] = "/home/parallels/corpus.txt";
// 2018/05/04 Bling Added

namespace alexaClientSDK {
namespace sampleApp {

/// The sample rate of microphone audio data.
static const unsigned int SAMPLE_RATE_HZ = 16000;

/// The number of audio channels.
static const unsigned int NUM_CHANNELS = 1;

/// The size of each word within the stream.
static const size_t WORD_SIZE = 2;

/// The maximum number of readers of the stream.
static const size_t MAX_READERS = 10;

/// The amount of audio data to keep in the ring buffer.
static const std::chrono::seconds AMOUNT_OF_AUDIO_DATA_IN_BUFFER = std::chrono::seconds(15);

/// The size of the ring buffer.
static const size_t BUFFER_SIZE_IN_SAMPLES = (SAMPLE_RATE_HZ)*AMOUNT_OF_AUDIO_DATA_IN_BUFFER.count();

/// Key for the root node value containing configuration values for SampleApp.
static const std::string SAMPLE_APP_CONFIG_KEY("sampleApp");

/// Key for the @c firmwareVersion value under the @c SAMPLE_APP_CONFIG_KEY configuration node.
static const std::string FIRMWARE_VERSION_KEY("firmwareVersion");

/// Key for the @c endpoint value under the @c SAMPLE_APP_CONFIG_KEY configuration node.
static const std::string ENDPOINT_KEY("endpoint");

/// Key for setting if display cards are supported or not under the @c SAMPLE_APP_CONFIG_KEY configuration node.
static const std::string DISPLAY_CARD_KEY("displayCardsSupported");

/// A set of all log levels.
static const std::set<alexaClientSDK::avsCommon::utils::logger::Level> allLevels = {
    alexaClientSDK::avsCommon::utils::logger::Level::DEBUG9,
    alexaClientSDK::avsCommon::utils::logger::Level::DEBUG8,
    alexaClientSDK::avsCommon::utils::logger::Level::DEBUG7,
    alexaClientSDK::avsCommon::utils::logger::Level::DEBUG6,
    alexaClientSDK::avsCommon::utils::logger::Level::DEBUG5,
    alexaClientSDK::avsCommon::utils::logger::Level::DEBUG4,
    alexaClientSDK::avsCommon::utils::logger::Level::DEBUG3,
    alexaClientSDK::avsCommon::utils::logger::Level::DEBUG2,
    alexaClientSDK::avsCommon::utils::logger::Level::DEBUG1,
    alexaClientSDK::avsCommon::utils::logger::Level::DEBUG0,
    alexaClientSDK::avsCommon::utils::logger::Level::INFO,
    alexaClientSDK::avsCommon::utils::logger::Level::WARN,
    alexaClientSDK::avsCommon::utils::logger::Level::ERROR,
    alexaClientSDK::avsCommon::utils::logger::Level::CRITICAL,
    alexaClientSDK::avsCommon::utils::logger::Level::NONE};

/**
 * Gets a log level consumable by the SDK based on the user input string for log level.
 *
 * @param userInputLogLevel The string to be parsed into a log level.
 * @return The log level. This will default to NONE if the input string is not properly parsable.
 */
static alexaClientSDK::avsCommon::utils::logger::Level getLogLevelFromUserInput(std::string userInputLogLevel) {
    std::transform(userInputLogLevel.begin(), userInputLogLevel.end(), userInputLogLevel.begin(), ::toupper);
    return alexaClientSDK::avsCommon::utils::logger::convertNameToLevel(userInputLogLevel);
}

std::unique_ptr<SampleApplication> SampleApplication::create() {
    auto clientApplication = std::unique_ptr<SampleApplication>(new SampleApplication);

    if (!clientApplication->initialize()) {
        ConsolePrinter::simplePrint("Failed to initialize SampleApplication");
        return nullptr;
    }
    return clientApplication;
}

void SampleApplication::run() {
    // 2018/05/04 Bling Added
    if (!interactionManager) {
        return;
    }
    
    struct my_msgbuf buf;
    int msqid;
    key_t key;

    if ((key = ftok(filePath, 66)) == -1) {
        perror("ftok");
        return;
    }

    if ((msqid = msgget(key, 0644 | IPC_CREAT)) == -1) {
        perror("msgget");
        return;
    }

    if ((saFile = fopen(filePath, "r")) == NULL) {
        perror("fopen");
        return;
    }

    while (fgets(saBuffer, 20, saFile) != NULL) {
    }
    fclose(saFile);

    while (true) {
        if (msgrcv(msqid, &buf, sizeof(buf), 0, 0) == -1) {
            perror("msgrcv");
            return;
        }

        if (!strncmp(buf.mtext, saBuffer, strlen(saBuffer) - 1)) {
            printf("%s\n", buf.mtext);
            // reset shared_ptr
            reSampleApplication();
            // get device microphone to use
            if (paMicrophone()) {
                interactionManager->tap();
            }
        }
    }
    // 2018/05/04 Bling Added
}

void SampleApplication::reSampleApplication() {
    sharedDataStream.reset();
    micWrapper.reset();
    interactionManager.reset();
}

bool SampleApplication::initialize() {
    /*
     * Set up the SDK logging system to write to the SampleApp's ConsolePrinter.  Also adjust the logging level
     * if requested.
     */
    std::shared_ptr<alexaClientSDK::avsCommon::utils::logger::Logger> consolePrinter =
        std::make_shared<alexaClientSDK::sampleApp::ConsolePrinter>();

    auto logLevelValue = getLogLevelFromUserInput(logLevel);
    if (alexaClientSDK::avsCommon::utils::logger::Level::UNKNOWN == logLevelValue) {
        alexaClientSDK::sampleApp::ConsolePrinter::simplePrint("Unknown log level input!");
        alexaClientSDK::sampleApp::ConsolePrinter::simplePrint("Possible log level options are: ");
        for (auto it = allLevels.begin(); it != allLevels.end(); ++it) {
                alexaClientSDK::sampleApp::ConsolePrinter::simplePrint(
                alexaClientSDK::avsCommon::utils::logger::convertLevelToName(*it));
        }
        return false;
    }

    alexaClientSDK::sampleApp::ConsolePrinter::simplePrint("Running app with log level: " + alexaClientSDK::avsCommon::utils::logger::convertLevelToName(logLevelValue));
    consolePrinter->setLevel(logLevelValue);

    alexaClientSDK::avsCommon::utils::logger::LoggerSinkManager::instance().initialize(consolePrinter);

    /*
     * This is a required step upon startup of the SDK before any modules are created. For that reason, it is being
     * called here, before creating the MediaPlayer, audio streams, DefaultClient, etc.
     */
    std::ifstream configInfile(pathToConfig);
    if (!configInfile.good()) {
        alexaClientSDK::sampleApp::ConsolePrinter::simplePrint("Failed to read config file!");
        return false;
    }
    if (!avsCommon::avs::initialization::AlexaClientSDKInit::initialize({&configInfile})) {
        alexaClientSDK::sampleApp::ConsolePrinter::simplePrint("Failed to initialize SDK!");
        return false;
    }

    auto config = alexaClientSDK::avsCommon::utils::configuration::ConfigurationNode::getRoot();
    auto sampleAppConfig = config[SAMPLE_APP_CONFIG_KEY];

    auto httpContentFetcherFactory = std::make_shared<avsCommon::utils::libcurlUtils::HTTPContentFetcherFactory>();

    /*
     * Creating the media players. Here, the default GStreamer based MediaPlayer is being created. However, any
     * MediaPlayer that follows the specified MediaPlayerInterface can work.
     */
    m_speakMediaPlayer = alexaClientSDK::mediaPlayer::MediaPlayer::create(httpContentFetcherFactory, avsCommon::sdkInterfaces::SpeakerInterface::Type::AVS_SYNCED, "SpeakMediaPlayer");
    if (!m_speakMediaPlayer) {
        alexaClientSDK::sampleApp::ConsolePrinter::simplePrint("Failed to create media player for speech!");
        return false;
    }

    m_audioMediaPlayer = alexaClientSDK::mediaPlayer::MediaPlayer::create(httpContentFetcherFactory, avsCommon::sdkInterfaces::SpeakerInterface::Type::AVS_SYNCED, "AudioMediaPlayer");
    if (!m_audioMediaPlayer) {
        alexaClientSDK::sampleApp::ConsolePrinter::simplePrint("Failed to create media player for content!");
        return false;
    }

    m_notificationsMediaPlayer = alexaClientSDK::mediaPlayer::MediaPlayer::create(httpContentFetcherFactory, avsCommon::sdkInterfaces::SpeakerInterface::Type::AVS_SYNCED, "NotificationsMediaPlayer");
    if (!m_notificationsMediaPlayer) {
        alexaClientSDK::sampleApp::ConsolePrinter::simplePrint("Failed to create media player for notifications!");
        return false;
    }

    /*
     * The ALERTS speaker type will cause volume control to be independent and localized. By assigning this type,
     * Alerts volume/mute changes will not be in sync with AVS. No directives or events will be associated with volume
     * control.
     */
    m_alertsMediaPlayer = alexaClientSDK::mediaPlayer::MediaPlayer::create(httpContentFetcherFactory, avsCommon::sdkInterfaces::SpeakerInterface::Type::LOCAL, "AlertsMediaPlayer");
    if (!m_alertsMediaPlayer) {
        alexaClientSDK::sampleApp::ConsolePrinter::simplePrint("Failed to create media player for alerts!");
        return false;
    }

    /*
     * Create Speaker interfaces to control the volume. For the SDK, the MediaPlayer happens to also provide
     * volume control functionality, but this does not have to be case.
     */
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::SpeakerInterface> speakSpeaker = std::static_pointer_cast<alexaClientSDK::avsCommon::sdkInterfaces::SpeakerInterface>(m_speakMediaPlayer);
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::SpeakerInterface> audioSpeaker = std::static_pointer_cast<alexaClientSDK::avsCommon::sdkInterfaces::SpeakerInterface>(m_audioMediaPlayer);
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::SpeakerInterface> alertsSpeaker = std::static_pointer_cast<alexaClientSDK::avsCommon::sdkInterfaces::SpeakerInterface>(m_alertsMediaPlayer);
    std::shared_ptr<alexaClientSDK::avsCommon::sdkInterfaces::SpeakerInterface> notificationsSpeaker = std::static_pointer_cast<alexaClientSDK::avsCommon::sdkInterfaces::SpeakerInterface>(m_notificationsMediaPlayer);

    auto audioFactory = std::make_shared<alexaClientSDK::applicationUtilities::resources::audio::AudioFactory>();

    // Creating the alert storage object to be used for rendering and storing alerts.
    auto alertStorage = std::make_shared<alexaClientSDK::capabilityAgents::alerts::storage::SQLiteAlertStorage>(audioFactory->alerts());

    /*
     * Creating notifications storage object to be used for storing notification indicators.
     */
    auto notificationsStorage = std::make_shared<alexaClientSDK::capabilityAgents::notifications::SQLiteNotificationsStorage>();

    /*
     * Creating settings storage object to be used for storing <key, value> pairs of AVS Settings.
     */
    auto settingsStorage = std::make_shared<alexaClientSDK::capabilityAgents::settings::SQLiteSettingStorage>();

    /*
     * Creating the UI component that observes various components and prints to the console accordingly.
     */
    //auto userInterfaceManager = std::make_shared<alexaClientSDK::sampleApp::UIManager>();
    userInterfaceManager = std::make_shared<alexaClientSDK::sampleApp::UIManager>();

    /*
     * Setting up a connection observer to wait for connection and authorization prior to accepting user input at
     * startup.
     */
    auto connectionObserver = std::make_shared<alexaClientSDK::sampleApp::ConnectionObserver>();

    /*
     * Creating the AuthDelegate - this component takes care of LWA and authorization of the client. At the moment,
     * this must be done and authorization must be achieved prior to making the call to connect().
     */
    std::shared_ptr<alexaClientSDK::authDelegate::AuthDelegate> authDelegate = alexaClientSDK::authDelegate::AuthDelegate::create();

    authDelegate->addAuthObserver(connectionObserver);

    // INVALID_FIRMWARE_VERSION is passed to @c getInt() as a default in case FIRMWARE_VERSION_KEY is not found.
    int firmwareVersion = static_cast<int>(avsCommon::sdkInterfaces::softwareInfo::INVALID_FIRMWARE_VERSION);
    sampleAppConfig.getInt(FIRMWARE_VERSION_KEY, &firmwareVersion, firmwareVersion);

    /*
     * Creating the DefaultClient - this component serves as an out-of-box default object that instantiates and "glues"
     * together all the modules.
     */
    client =
        alexaClientSDK::defaultClient::DefaultClient::create(
            m_speakMediaPlayer,
            m_audioMediaPlayer,
            m_alertsMediaPlayer,
            m_notificationsMediaPlayer,
            speakSpeaker,
            audioSpeaker,
            alertsSpeaker,
            notificationsSpeaker,
            audioFactory,
            authDelegate,
            alertStorage,
            notificationsStorage,
            settingsStorage,
            {userInterfaceManager},
            {connectionObserver, userInterfaceManager},
            firmwareVersion,
            true,
            nullptr);

    if (!client) {
        alexaClientSDK::sampleApp::ConsolePrinter::simplePrint("Failed to create default SDK client!");
        return false;
    }

    /*
     * TODO: ACSDK-384 Remove the requirement of clients having to wait for authorization before making the connect()
     * call.
     */
    if (!connectionObserver->waitFor(
            alexaClientSDK::avsCommon::sdkInterfaces::AuthObserverInterface::State::REFRESHED)) {
        alexaClientSDK::sampleApp::ConsolePrinter::simplePrint("Failed to authorize SDK client!");
        return false;
    }

    std::string endpoint;
    sampleAppConfig.getString(ENDPOINT_KEY, &endpoint);

    client->connect(endpoint);

    if (!connectionObserver->waitFor(avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::Status::CONNECTED)) {
        alexaClientSDK::sampleApp::ConsolePrinter::simplePrint("Failed to connect to AVS!");
        return false;
    }
    // Add userInterfaceManager as observer of locale setting.
    client->addSettingObserver("locale", userInterfaceManager);
    // Send default settings set by the user to AVS.
    client->sendDefaultSettings();

    client->addSpeakerManagerObserver(userInterfaceManager);

    client->addNotificationsObserver(userInterfaceManager);

    /*
     * Add GUI Renderer as an observer if display cards are supported.  The default is supported unless specified
     * otherwise in the configuration.
     */
    bool displayCardsSupported;
    config[SAMPLE_APP_CONFIG_KEY].getBool(DISPLAY_CARD_KEY, &displayCardsSupported, true);
    if (displayCardsSupported) {
        auto guiRenderer = std::make_shared<GuiRenderer>();
        client->addTemplateRuntimeObserver(guiRenderer);
    }

    compatibleAudioFormat.sampleRateHz = SAMPLE_RATE_HZ;
    compatibleAudioFormat.sampleSizeInBits = WORD_SIZE * CHAR_BIT;
    compatibleAudioFormat.numChannels = NUM_CHANNELS;
    compatibleAudioFormat.endianness = alexaClientSDK::avsCommon::utils::AudioFormat::Endianness::LITTLE;
    compatibleAudioFormat.encoding = alexaClientSDK::avsCommon::utils::AudioFormat::Encoding::LPCM;

    if (!paMicrophone()) {
        return false;
    }

    return true;
}

bool SampleApplication::paMicrophone() {
    /*
     * Creating the buffer (Shared Data Stream) that will hold user audio data. This is the main input into the SDK.
     */
    size_t bufferSize = alexaClientSDK::avsCommon::avs::AudioInputStream::calculateBufferSize(BUFFER_SIZE_IN_SAMPLES, WORD_SIZE, MAX_READERS);
    auto buffer = std::make_shared<alexaClientSDK::avsCommon::avs::AudioInputStream::Buffer>(bufferSize);

    sharedDataStream = alexaClientSDK::avsCommon::avs::AudioInputStream::create(buffer, WORD_SIZE, MAX_READERS);
    if (!sharedDataStream) {
        alexaClientSDK::sampleApp::ConsolePrinter::simplePrint("Failed to create shared data stream!");
        return false;
    }

    /*
     * Creating each of the audio providers. An audio provider is a simple package of data consisting of the stream
     * of audio data, as well as metadata about the stream. For each of the three audio providers created here, the same
     * stream is used since this sample application will only have one microphone.
     */

    // Creating tap to talk audio provider
    bool tapAlwaysReadable = true;
    bool tapCanOverride = true;
    bool tapCanBeOverridden = true;

    alexaClientSDK::capabilityAgents::aip::AudioProvider tapToTalkAudioProvider(
        sharedDataStream,
        compatibleAudioFormat,
        alexaClientSDK::capabilityAgents::aip::ASRProfile::NEAR_FIELD,
        tapAlwaysReadable,
        tapCanOverride,
        tapCanBeOverridden);

    // Creating hold to talk audio provider
    bool holdAlwaysReadable = false;
    bool holdCanOverride = true;
    bool holdCanBeOverridden = false;

    alexaClientSDK::capabilityAgents::aip::AudioProvider holdToTalkAudioProvider(
        sharedDataStream,
        compatibleAudioFormat,
        alexaClientSDK::capabilityAgents::aip::ASRProfile::CLOSE_TALK,
        holdAlwaysReadable,
        holdCanOverride,
        holdCanBeOverridden);

    micWrapper = alexaClientSDK::sampleApp::PortAudioMicrophoneWrapper::create(sharedDataStream);
    if (!micWrapper) {
        alexaClientSDK::sampleApp::ConsolePrinter::simplePrint("Failed to create PortAudioMicrophoneWrapper!");
        return false;
    }

    interactionManager = std::make_shared<alexaClientSDK::sampleApp::InteractionManager>(client, micWrapper, userInterfaceManager, holdToTalkAudioProvider, tapToTalkAudioProvider);
    client->addAlexaDialogStateObserver(interactionManager);

    return true;
}

}  // namespace sampleApp
}  // namespace alexaClientSDK
