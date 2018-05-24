/*
 * UIManager.cpp
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

#include <sstream>
#include "SampleApp/UIManager.h"
#include <AVSCommon/SDKInterfaces/DialogUXStateObserverInterface.h>
#include "SampleApp/ConsolePrinter.h"

// 2018/05/23 Bling Added
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

struct my_msgbuf buf;
int msqid;
key_t key;
// 2018/05/23 Bling Added

namespace alexaClientSDK {
namespace sampleApp {

using namespace avsCommon::sdkInterfaces;

void UIManager::onDialogUXStateChanged(DialogUXState state) {
    m_executor.submit([this, state]() {
        if (state == m_dialogState) {
            return;
        }
        m_dialogState = state;
        printState();
    });
}

void UIManager::onConnectionStatusChanged(const Status status, const ChangedReason reason) {
    m_executor.submit([this, status]() {
        if (m_connectionStatus == status) {
            return;
        }
        m_connectionStatus = status;
        printState();
    });
}

void UIManager::onSettingChanged(const std::string& key, const std::string& value) {
    m_executor.submit([key, value]() {
        std::string msg = key + " set to " + value;
        ConsolePrinter::prettyPrint(msg);
    });
}

void UIManager::onSpeakerSettingsChanged(
    const SpeakerManagerObserverInterface::Source& source,
    const SpeakerInterface::Type& type,
    const SpeakerInterface::SpeakerSettings& settings) {
    m_executor.submit([source, type, settings]() {
        std::ostringstream oss;
        oss << "SOURCE:" << source << " TYPE:" << type << " VOLUME:" << static_cast<int>(settings.volume)
            << " MUTE:" << settings.mute;
        ConsolePrinter::prettyPrint(oss.str());
    });
}

void UIManager::onSetIndicator(avsCommon::avs::IndicatorState state) {
    m_executor.submit([state]() {
        std::ostringstream oss;
        oss << "NOTIFICATION INDICATOR STATE: " << state;
        ConsolePrinter::prettyPrint(oss.str());
    });
}

void UIManager::printState() {
    if (m_connectionStatus == avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::Status::DISCONNECTED) {
        ConsolePrinter::prettyPrint("Client not connected!");
    } else if (m_connectionStatus == avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::Status::PENDING) {
        ConsolePrinter::prettyPrint("Connecting...");
        // 2018/05/23 Bling Added
        if ((key = ftok("/home/parallels/a113d.txt", 66)) == -1) {
            perror("ftok");
            exit(1);
        }

        if ((msqid = msgget(key, 0644 | IPC_CREAT)) == -1) {
            perror("msgget");
            exit(1);
        }

        buf.mtype = 1;
        // 2018/05/23 Bling Added
    } else if (m_connectionStatus == avsCommon::sdkInterfaces::ConnectionStatusObserverInterface::Status::CONNECTED) {
        switch (m_dialogState) {
            case DialogUXState::IDLE:
                // 2018/05/23 Bling Added
                strcpy(buf.mtext, "OK");
                if (msgsnd(msqid, &buf, sizeof(buf), 0) == -1) {
                    perror("msgsnd");
                    exit(1);
                }
                // 2018/05/23 Bling Added
                ConsolePrinter::prettyPrint("Alexa is currently idle!");                
                return;

            case DialogUXState::LISTENING:
                ConsolePrinter::prettyPrint("Listening...");
                // 2018/05/23 Bling Added
                soundControl = 1;
                // 2018/05/23 Bling Added
                return;

            case DialogUXState::THINKING:
                ConsolePrinter::prettyPrint("Thinking...");                
                return;

            case DialogUXState::SPEAKING:
                ConsolePrinter::prettyPrint("Speaking...");
                return;

            /*
             * This is an intermediate state after a SPEAK directive is completed. In the case of a speech burst the
             * next SPEAK could kick in or if its the last SPEAK directive ALEXA moves to the IDLE state. So we do
             * nothing for this state.
             */
            case DialogUXState::FINISHED:
                return;
        }
    }
}

}  // namespace sampleApp
}  // namespace alexaClientSDK
