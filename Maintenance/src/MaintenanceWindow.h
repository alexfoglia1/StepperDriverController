#ifndef MAINT_WIN_H
#define MAINT_WIN_H
#include <qmainwindow.h>
#include <qserialport.h>

#include "ui_MaintenanceGui.h"
#include "ui_AutoscanComPortsGui.h"


class MaintenanceWindow : public QMainWindow
{
	Q_OBJECT
public:
	MaintenanceWindow();

private:
	Ui_MaintenanceGui _ui;
	Ui_AutoscanComPortsGui _progressUi;

	QMainWindow _autoscanProgressWindow;
	QSerialPort* _serialPort;

	enum class RxStatus
	{
		WAIT_SYNC = 0,
        WAIT_HEADER_1,
        WAIT_HEADER_2,
		WAIT_PAYLOAD
	};


	RxStatus _rxStatus;
	quint8 _expectedPayloadSize;
	quint8 _receivedPayloadSize;
	quint8 _rxBuffer[32];

    void autoScanComPorts();
    void dataIngest();
	void update_fsm(quint8 byteIn);

private slots:
	void onBtnOpen();
	void onBtnRescan();
	void onArduinoRx();
	void onBtnReadMaxDelay();
	void onBtnReadMinDelay();
    void onBtnWriteMaxDelay();
	void onBtnWriteMinDelay();
	void onBtnInvert();
	void onBtnStartStop();
	void onBtnDefault();
    void onBtnWriteButtons();
    void onPollTimeout();
    void onBtnReadButtons();
	void onSetButtonsState();
	void onDelayChanged(int newDelay);
};

#endif
