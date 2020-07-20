/*
* Mobile Adapter for mGBA QT
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#pragma once

#include <QDialog>
#include <QDateTime>
#include <QMutex>
#include <QByteArray>

#include <libmobile/mobile.h>

#include "ui_MobileAdapterView.h"

namespace QGBA {

class CoreController;
class ConfigController;
class Window;

class MobileAdapterView : public QDialog {
Q_OBJECT

public:
	MobileAdapterView(std::shared_ptr<CoreController> controller, Window* window, QWidget* parent = nullptr);
	~MobileAdapterView();

	void frameCallback();

private:
	std::shared_ptr<CoreController> m_controller;
	Window* m_window;

	Ui::MobileAdapterView m_ui;

	void Reset();
	void Update(const struct mobile_adapter* adapter);

private slots:
	void closeButtonClick();
	void p2pPortChanged(int value);
	void serverDomainChanged(QString str);
	void serverIpChanged(QString str);
	void adapterTypeChanged(int value);
	void usernameChanged(QString str);
	void passwordChanged(QString str);
	void dnsChanged(QString str);
};

}

