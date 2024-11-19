#include "MaintenanceWindow.h"
#include "Maintenance.h"

typedef maint_rx_byte_t maint_tx_byte_t;

#include <qtimer.h>
#include <qserialport.h>
#include <qdatetime.h>
#include <qmessagebox.h>


MaintenanceWindow::MaintenanceWindow()
{
	_ui.setupUi(this);
	_progressUi.setupUi(&_autoscanProgressWindow);
	_autoscanProgressWindow.setVisible(false);

	_serialPort = nullptr;
	_rxStatus = RxStatus::WAIT_SYNC;
	_expectedPayloadSize = 0;
	_receivedPayloadSize = 0;
	memset(&_rxBuffer, 0x00, 32);

	QTimer* autoscanComPortsTimer = new QTimer();
	autoscanComPortsTimer->setSingleShot(true);
	autoscanComPortsTimer->setTimerType(Qt::PreciseTimer);

	connect(autoscanComPortsTimer, &QTimer::timeout, this, [this, autoscanComPortsTimer] { this->autoScanComPorts(); autoscanComPortsTimer->deleteLater(); });
	connect(_ui.btnOpenSerialPort, &QPushButton::clicked, this, &MaintenanceWindow::onBtnOpen);
	connect(_ui.btnRescanPorts, &QPushButton::clicked, this, &MaintenanceWindow::onBtnRescan);

	connect(_ui.btnReadMaxDelay, &QPushButton::clicked, this, &MaintenanceWindow::onBtnReadMaxDelay);
	connect(_ui.btnReadMinDelay, &QPushButton::clicked, this, &MaintenanceWindow::onBtnReadMinDelay);
	connect(_ui.btnWriteMaxDelay, &QPushButton::clicked, this, &MaintenanceWindow::onBtnWriteMaxDelay);
	connect(_ui.btnWriteMinDelay, &QPushButton::clicked, this, &MaintenanceWindow::onBtnWriteMinDelay);
	connect(_ui.btnInvert, &QPushButton::clicked, this, &MaintenanceWindow::onBtnInvert);
	connect(_ui.btnStartStopMotor, &QPushButton::clicked, this, &MaintenanceWindow::onBtnStartStop);
	connect(_ui.btnDefault, &QPushButton::clicked, this, &MaintenanceWindow::onBtnDefault);

	autoscanComPortsTimer->start(500);
}

void MaintenanceWindow::onBtnReadMaxDelay()
{
	if (_serialPort)
	{
		maint_tx_byte_t byteTx;
		byteTx.Byte = 0;
		byteTx.Bits.get_max_delay = 1;

		QByteArray qba;
		qba.push_back(SYNC_CHAR);
		qba.push_back(byteTx.Byte);

		_serialPort->write(qba);
	}
}


void MaintenanceWindow::onBtnReadMinDelay()
{
	if (_serialPort)
	{
		maint_tx_byte_t byteTx;
		byteTx.Byte = 0;
		byteTx.Bits.get_min_delay = 1;

		QByteArray qba;
		qba.push_back(SYNC_CHAR);
		qba.push_back(byteTx.Byte);

		_serialPort->write(qba);
	}
}


void MaintenanceWindow::onBtnWriteMaxDelay()
{
	if (_serialPort)
	{
		maint_tx_byte_t byteTx;
		byteTx.Byte = 0;
		byteTx.Bits.set_max_delay = 1;

		quint16 maxDelay = _ui.spinMaxDelay->value() & 0xFFFF;

		QByteArray qba;
		qba.push_back(SYNC_CHAR);
		qba.push_back(byteTx.Byte);
		qba.push_back((maxDelay & 0xFF00) >> 8);
		qba.push_back(maxDelay & 0xFF);

		_serialPort->write(qba);
	}
}


void MaintenanceWindow::onBtnWriteMinDelay()
{
	if (_serialPort)
	{
		maint_tx_byte_t byteTx;
		byteTx.Byte = 0;
		byteTx.Bits.set_min_delay = 1;

		quint16 minDelay = _ui.spinMinDelay->value() & 0xFFFF;

		QByteArray qba;
		qba.push_back(SYNC_CHAR);
		qba.push_back(byteTx.Byte);
		qba.push_back((minDelay & 0xFF00) >> 8);
		qba.push_back(minDelay & 0xFF);

		_serialPort->write(qba);
	}
}


void MaintenanceWindow::onBtnInvert()
{
	if (_serialPort)
	{
		maint_tx_byte_t byteTx;
		byteTx.Byte = 0;
		byteTx.Bits.invert_dir = 1;

		QByteArray qba;
		qba.push_back(SYNC_CHAR);
		qba.push_back(byteTx.Byte);

		_serialPort->write(qba);
	}
}


void MaintenanceWindow::onBtnStartStop()
{
	if (_serialPort)
	{
		maint_tx_byte_t byteTx;
		byteTx.Byte = 0;
		byteTx.Bits.motor_move = 1;

		QByteArray qba;
		qba.push_back(SYNC_CHAR);
		qba.push_back(byteTx.Byte);

		_serialPort->write(qba);
	}
}


void MaintenanceWindow::onBtnDefault()
{
	if (_serialPort)
	{
		maint_tx_byte_t byteTx;
		byteTx.Byte = 0;
		byteTx.Bits.set_default = 1;

		QByteArray qba;
		qba.push_back(SYNC_CHAR);
		qba.push_back(byteTx.Byte);

		_serialPort->write(qba);
	}
}


void MaintenanceWindow::update_fsm(quint8 byteIn)
{
	switch (_rxStatus)
	{
		case RxStatus::WAIT_SYNC:
			if (byteIn == SYNC_CHAR)
			{
				_expectedPayloadSize = 0;
				_receivedPayloadSize = 0;
				memset(&_rxBuffer, 0x00, 32);
				_rxStatus = RxStatus::WAIT_HEADER;
			}
		break;
		case RxStatus::WAIT_HEADER:
		{
			maint_rx_byte_t protocolByteIn;
			protocolByteIn.Byte = byteIn;

			if (protocolByteIn.Bits.zero == 0)
			{
				if (protocolByteIn.Bits.get_min_delay)
				{
					_expectedPayloadSize += 2;
				}
				if (protocolByteIn.Bits.get_max_delay)
				{
					_expectedPayloadSize += 2;
				}

				if (_expectedPayloadSize)
				{
					_receivedPayloadSize = 1;
					_expectedPayloadSize += 1;
					_rxBuffer[0] = byteIn;
					_rxStatus = RxStatus::WAIT_PAYLOAD;
				}
				else
				{
					_rxStatus = RxStatus::WAIT_SYNC;
				}
			}
			else
			{
				_rxStatus = RxStatus::WAIT_SYNC;
			}
		}
		break;
		case RxStatus::WAIT_PAYLOAD:
		{
			_rxBuffer[_receivedPayloadSize] = byteIn;
			_receivedPayloadSize += 1;

			if (_receivedPayloadSize == _expectedPayloadSize)
			{
				maint_rx_byte_t protocolByteIn;
				protocolByteIn.Byte = _rxBuffer[0];

				quint8 curOffset = 1;
				if (protocolByteIn.Bits.get_min_delay)
				{
					quint16 minDelayMSB = _rxBuffer[curOffset];
					quint16 minDelayLSB = _rxBuffer[curOffset + 1];
					quint16 minDelay = ((minDelayMSB << 8) | minDelayLSB);
					_ui.spinMinDelay->setValue(minDelay);

					curOffset += 2;
				}

				if (protocolByteIn.Bits.get_max_delay)
				{
					quint16 maxDelayMSB = _rxBuffer[curOffset];
					quint16 maxDelayLSB = _rxBuffer[curOffset + 1];
					quint16 maxDelay = ((maxDelayMSB << 8) | maxDelayLSB);
					_ui.spinMaxDelay->setValue(maxDelay);

					curOffset += 2;
				}

				_rxStatus = RxStatus::WAIT_SYNC;
			}

		}
		break;
	}
}


void MaintenanceWindow::onArduinoRx()
{
	QByteArray qba = _serialPort->readAll();

	for (auto& byte : qba)
	{
		//printf("%c", byte);
		update_fsm(byte);
	}
}


void MaintenanceWindow::onBtnOpen()
{
	if (_ui.btnOpenSerialPort->text().toUpper() == "OPEN")
	{
		if (_serialPort == nullptr)
		{
			_serialPort = new QSerialPort();
			_serialPort->setPortName(_ui.comboSelPort->currentText());
			_serialPort->setBaudRate(QSerialPort::Baud115200);
			_serialPort->setParity(QSerialPort::NoParity);
			_serialPort->setDataBits(QSerialPort::Data8);
			_serialPort->setStopBits(QSerialPort::OneStop);
			_serialPort->setFlowControl(QSerialPort::NoFlowControl);

			connect(_serialPort, SIGNAL(readyRead()), this, SLOT(onArduinoRx()));

			bool ret = _serialPort->open(QSerialPort::OpenModeFlag::ReadWrite);
			if (ret)
			{

				_ui.comboSelPort->setEnabled(false);
				_ui.btnRescanPorts->setEnabled(false);
				_ui.btnOpenSerialPort->setText("Close");
			}
			else
			{
				_serialPort->deleteLater();
				_serialPort = nullptr;
			}
		}
	}
	else
	{
		_serialPort->close();
		_serialPort->deleteLater();
		_serialPort = nullptr;

		_ui.comboSelPort->setEnabled(true);
		_ui.btnRescanPorts->setEnabled(true);

		_ui.btnOpenSerialPort->setText("Open");
	}
}

void MaintenanceWindow::onBtnRescan()
{
	_ui.comboSelPort->clear();
	autoScanComPorts();
}



void MaintenanceWindow::autoScanComPorts()
{
	_progressUi.autoscanStatusPrompt->setText("");

	_autoscanProgressWindow.setVisible(true);
	for (int i = 0; i < 100; i++)
	{
#ifdef __linux__
        QString portName = QString("/dev/ttyACM%1").arg(i);
#else
		QString portName = QString("COM%1").arg(i);
#endif
		_progressUi.autoscanStatusPrompt->append(QString("Testing %1").arg(portName));
		qApp->processEvents();

		QSerialPort* serialPort = new QSerialPort(portName);
		if (serialPort->open(QSerialPort::ReadWrite))
		{
			serialPort->close();
			serialPort->deleteLater();

			_ui.comboSelPort->addItem(portName);
			_progressUi.autoscanStatusPrompt->append(QString("OK"));
		}
		else
		{
			_progressUi.autoscanStatusPrompt->append(QString("FAIL"));
		}

		_progressUi.autoscanStatusProgress->setValue(i + 1);

		qApp->processEvents();
	}
	_autoscanProgressWindow.setVisible(false);
}