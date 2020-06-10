/* -LICENSE-START-
** Copyright (c) 2019 Blackmagic Design
**
** Permission is hereby granted, free of charge, to any person or organization
** obtaining a copy of the software and accompanying documentation covered by
** this license (the "Software") to use, reproduce, display, distribute,
** execute, and transmit the Software, and to prepare derivative works of the
** Software, and to permit third-parties to whom the Software is furnished to
** do so, all subject to the following:
**
** The copyright notices in the Software and this entire statement, including
** the above license grant, this restriction and the following disclaimer,
** must be included in all copies of the Software, in whole or in part, and
** all derivative works of the Software, unless such copies or derivative
** works are solely in the form of machine-executable object code generated by
** a source language processor.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
** -LICENSE-END-
*/

#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QGridLayout>
#include <array>
#include <map>
#include <memory>

#include "DeckLinkDeviceDiscovery.h"
#include "DeckLinkInputPage.h"
#include "ProfileCallback.h"

#include "DeckLinkAPI.h"

static const int kPreviewDevicesCount = 4;

namespace Ui {
class QuadPreview;
}

class QuadPreview : public QDialog
{
	enum class DeviceState { Inactive, Available, Selected };

	Q_OBJECT

public:
	explicit QuadPreview(QWidget *parent = 0);
	~QuadPreview();

	void customEvent(QEvent* event) override;
	void closeEvent(QCloseEvent *event) override;

	void setup();

	void startCapture(int deviceIndex);
	void refreshDisplayModeMenu(int deviceIndex);
	void refreshInputConnectionMenu(int deviceIndex);
	void addDevice(com_ptr<IDeckLink>& deckLink);
	void removeDevice(com_ptr<IDeckLink>& deckLink);
	void haltStreams(com_ptr<IDeckLinkProfile> profile);
	void updateProfile(com_ptr<IDeckLinkProfile>& newProfile);
	bool isDeviceAvailable(com_ptr<IDeckLink>& device);

public slots:
	void requestDevice(DeckLinkInputPage* page, com_ptr<IDeckLink>& deckLink);
	void requestDeviceIfAvailable(DeckLinkInputPage* page, com_ptr<IDeckLink>& deckLink);
	void relinquishDevice(com_ptr<IDeckLink>& device);

private slots:
	void deviceLabelEnableChanged(bool enabled);
	void timecodeEnableChanged(bool enabled);

private:
	std::unique_ptr<Ui::QuadPreview>					m_ui;
	QGridLayout*										m_previewLayout;

	com_ptr<DeckLinkDeviceDiscovery>					m_deckLinkDiscovery;
	ProfileCallback*									m_profileCallback;

	std::array<DeckLinkInputPage*, kPreviewDevicesCount> m_devicePages;

	std::map<com_ptr<IDeckLink>, DeviceState>			m_inputDevices;
};