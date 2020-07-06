/*
* Mobile Adapter for mGBA QT
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include "MobileAdapterView.h"

#include "CoreController.h"
#include "ConfigController.h"
#include "Window.h"

#include <QHostAddress>

using namespace QGBA;

MobileAdapterView::MobileAdapterView(std::shared_ptr<CoreController> controller, Window* window, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint),
	m_controller(controller),
	m_window(window)
{
    m_ui.setupUi(this);

	connect(m_controller.get(), &CoreController::mobileUpdate, this, &MobileAdapterView::frameCallback);

	m_controller->attachMobileAdapter();

    Reset();

    struct mMobileAdapter* mobile = m_controller->getMobileAdapter();
	m_ui.p2pPortEdit->setValue(mobile->mobile.config.p2p_port);
	m_ui.adapterTypeCombo->setCurrentIndex(mobile->mobile.config.device - 8);

	m_ui.serverDomainEdit->setText(QHostAddress(mobile->serverip).toString());
}

MobileAdapterView::~MobileAdapterView() {
    disconnect(m_controller.get(), &CoreController::mobileUpdate, this, &MobileAdapterView::frameCallback);
	m_controller->detachMobileAdapter();
}

void MobileAdapterView::frameCallback() {
    mobile_action action = m_controller->getMobileAction();
    if (action == MOBILE_ACTION_NONE) {
        return;
    }
    else if (action == MOBILE_ACTION_RESET_SERIAL) {
        Reset();
        return;
    }

    Update(&m_controller->getMobileAdapter()->mobile);
}

// UI updates
void MobileAdapterView::Reset() {
    m_ui.adapterStatusLabel->setText(tr("Disconnected"));
    m_ui.sessionBegunBox->setChecked(false);
    m_ui.mode32Box->setChecked(false);
}


void MobileAdapterView::Update(const struct mobile_adapter* adapter) {
    m_ui.mode32Box->setChecked(adapter->serial.mode_32bit);
    m_ui.sessionBegunBox->setChecked(adapter->commands.session_begun);
    if (adapter->commands.state == MOBILE_CONNECTION_DISCONNECTED)
        m_ui.adapterStatusLabel->setText(tr("Disconnected"));
    else if (adapter->commands.state == MOBILE_CONNECTION_CALL)
        m_ui.adapterStatusLabel->setText(tr("Connected (P2P): %1").arg("¡×todo"));
	else {
        uint32_t connections = 0;
        for (int i = 0; i < MOBILE_MAX_CONNECTIONS; i++) {
		    if (adapter->commands.connections[i])
                connections++;
        }

        m_ui.adapterStatusLabel->setText(tr("Connected (Internet): %1 connections").arg(QString::number(connections)));
    }
}

// Qt Actions
void MobileAdapterView::closeButtonClick() { 
    close();
}

void MobileAdapterView::p2pPortChanged(int value) {
	m_controller->getMobileAdapter()->mobile.config.p2p_port = value;
}

void MobileAdapterView::serverDomainChanged(QString str) {
	m_controller->getMobileAdapter()->serverip = QHostAddress(str).toIPv4Address();
}

void MobileAdapterView::adapterTypeChanged(int value) {
	if (value < 0 || value > 3)
		value = 0;

	m_controller->getMobileAdapter()->mobile.config.device = (enum mobile_adapter_device)(value + 8);
}

void MobileAdapterView::usernameChanged(QString str) {

}

void MobileAdapterView::passwordChanged(QString str) {

}

void MobileAdapterView::dnsChanged(QString str) { 
    
}
