#include "MaintenanceWindow.h"
#include "Maintenance.h"

#include <qthread.h>
#include <qtimer.h>
#include <qserialport.h>
#include <qdatetime.h>
#include <qmessagebox.h>

#define BTN_MASK(BTN)(1 << BTN)


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
    connect(_ui.btnReadButton1, &QPushButton::clicked, this, &MaintenanceWindow::onBtnReadButton1);
    connect(_ui.btnReadButton3, &QPushButton::clicked, this, &MaintenanceWindow::onBtnReadButton3);
    connect(_ui.btnDefault, &QPushButton::clicked, this, &MaintenanceWindow::onBtnDefault);
    connect(_ui.btnWriteButtons, &QPushButton::clicked, this, &MaintenanceWindow::onBtnWriteButtons);


    QTimer* pollTimeout = new QTimer();
    pollTimeout->setTimerType(Qt::PreciseTimer);
    pollTimeout->setInterval(10);
    pollTimeout->setSingleShot(false);



    connect(pollTimeout, &QTimer::timeout, this, &MaintenanceWindow::onPollTimeout);
    pollTimeout->start();

	autoscanComPortsTimer->start(500);
}

void MaintenanceWindow::onPollTimeout()
{
    if (_serialPort)
    {
        maint_header_t headerTx;
        headerTx.Word = 0;
        headerTx.Bytes.byte_2.Bits.get_btn_state = _ui.checkPollButtons->isChecked();
        headerTx.Bytes.byte_2.Bits.get_analog2_in = _ui.checkPollAnalog2->isChecked();
        headerTx.Bytes.byte_2.Bits.get_analog5_in = _ui.checkPollAnalog5->isChecked();

        QByteArray qba;
        qba.push_back(SYNC_CHAR);
        qba.push_back(headerTx.Bytes.byte_1.Byte);
        qba.push_back(headerTx.Bytes.byte_2.Byte);

        if (headerTx.Word)
        {
            _serialPort->write(qba);
        }
    }
}


void MaintenanceWindow::onBtnReadButton1()
{
    if (_serialPort)
    {
        maint_header_t headerTx;
        headerTx.Word = 0;
        headerTx.Bytes.byte_2.Bits.get_btn_12_val = 1;

        QByteArray qba;
        qba.push_back(SYNC_CHAR);
        qba.push_back(headerTx.Bytes.byte_1.Byte);
        qba.push_back(headerTx.Bytes.byte_2.Byte);

        _serialPort->write(qba);
    }
}


void MaintenanceWindow::onBtnReadButton3()
{
    if (_serialPort)
    {
        maint_header_t headerTx;
        headerTx.Word = 0;
        headerTx.Bytes.byte_2.Bits.get_btn_345_val = 1;

        QByteArray qba;
        qba.push_back(SYNC_CHAR);
        qba.push_back(headerTx.Bytes.byte_1.Byte);
        qba.push_back(headerTx.Bytes.byte_2.Byte);

        _serialPort->write(qba);
    }
}


void MaintenanceWindow::onBtnReadMaxDelay()
{
	if (_serialPort)
	{
        maint_header_t headerTx;
        headerTx.Word = 0;
        headerTx.Bytes.byte_1.Bits.get_max_delay = 1;

		QByteArray qba;
		qba.push_back(SYNC_CHAR);
        qba.push_back(headerTx.Bytes.byte_1.Byte);
        qba.push_back(headerTx.Bytes.byte_2.Byte);

		_serialPort->write(qba);
	}
}


void MaintenanceWindow::onBtnReadMinDelay()
{
	if (_serialPort)
	{
        maint_header_t headerTx;
        headerTx.Word = 0;
        headerTx.Bytes.byte_1.Bits.get_min_delay = 1;

		QByteArray qba;
		qba.push_back(SYNC_CHAR);
        qba.push_back(headerTx.Bytes.byte_1.Byte);
        qba.push_back(headerTx.Bytes.byte_2.Byte);

		_serialPort->write(qba);
	}
}


void MaintenanceWindow::onBtnWriteMaxDelay()
{
	if (_serialPort)
	{
        maint_header_t headerTx;
        headerTx.Word = 0;
        headerTx.Bytes.byte_1.Bits.set_max_delay = 1;

		quint16 maxDelay = _ui.spinMaxDelay->value() & 0xFFFF;

		QByteArray qba;
		qba.push_back(SYNC_CHAR);
        qba.push_back(headerTx.Bytes.byte_1.Byte);
        qba.push_back(headerTx.Bytes.byte_2.Byte);
		qba.push_back((maxDelay & 0xFF00) >> 8);
		qba.push_back(maxDelay & 0xFF);

		_serialPort->write(qba);
	}
}


void MaintenanceWindow::onBtnWriteMinDelay()
{
	if (_serialPort)
	{
        maint_header_t headerTx;
        headerTx.Word = 0;
        headerTx.Bytes.byte_1.Bits.set_min_delay = 1;

		quint16 minDelay = _ui.spinMinDelay->value() & 0xFFFF;

		QByteArray qba;
		qba.push_back(SYNC_CHAR);
        qba.push_back(headerTx.Bytes.byte_1.Byte);
        qba.push_back(headerTx.Bytes.byte_2.Byte);
		qba.push_back((minDelay & 0xFF00) >> 8);
		qba.push_back(minDelay & 0xFF);

		_serialPort->write(qba);
	}
}


void MaintenanceWindow::onBtnInvert()
{
	if (_serialPort)
	{
        maint_header_t headerTx;
        headerTx.Word = 0;
        headerTx.Bytes.byte_1.Bits.invert_dir = 1;

		QByteArray qba;
		qba.push_back(SYNC_CHAR);
        qba.push_back(headerTx.Bytes.byte_1.Byte);
        qba.push_back(headerTx.Bytes.byte_2.Byte);

		_serialPort->write(qba);
	}
}


void MaintenanceWindow::onBtnStartStop()
{
	if (_serialPort)
	{
        maint_header_t headerTx;
        headerTx.Word = 0;
        headerTx.Bytes.byte_1.Bits.motor_move = 1;

		QByteArray qba;
		qba.push_back(SYNC_CHAR);
        qba.push_back(headerTx.Bytes.byte_1.Byte);
        qba.push_back(headerTx.Bytes.byte_2.Byte);

		_serialPort->write(qba);
	}
}


void MaintenanceWindow::onBtnWriteButtons()
{
    if (_serialPort)
    {
        maint_header_t headerTx;
        headerTx.Word = 0;
        headerTx.Bytes.byte_2.Bits.set_btn_val = 1;

        quint16 btn1 = _ui.spinButton1->value() & 0xFFFF;
        quint16 btn2 = _ui.spinButton2->value() & 0xFFFF;
        quint16 btn3 = _ui.spinButton3->value() & 0xFFFF;
        quint16 btn4 = _ui.spinButton4->value() & 0xFFFF;
        quint16 btn5 = _ui.spinButton5->value() & 0xFFFF;

        QByteArray qba;
        qba.push_back(SYNC_CHAR);
        qba.push_back(headerTx.Bytes.byte_1.Byte);
        qba.push_back(headerTx.Bytes.byte_2.Byte);
        qba.push_back((btn1 & 0xFF00) >> 8);
        qba.push_back(btn1 & 0xFF);
        qba.push_back((btn2 & 0xFF00) >> 8);
        qba.push_back(btn2 & 0xFF);
        qba.push_back((btn3 & 0xFF00) >> 8);
        qba.push_back(btn3 & 0xFF);
        qba.push_back((btn4 & 0xFF00) >> 8);
        qba.push_back(btn4 & 0xFF);
        qba.push_back((btn5 & 0xFF00) >> 8);
        qba.push_back(btn5 & 0xFF);

        _serialPort->write(qba);
    }
}


void MaintenanceWindow::onBtnDefault()
{
	if (_serialPort)
	{
        maint_header_t headerTx;
        headerTx.Word = 0;
        headerTx.Bytes.byte_1.Bits.set_default = 1;

		QByteArray qba;
		qba.push_back(SYNC_CHAR);
        qba.push_back(headerTx.Bytes.byte_1.Byte);
        qba.push_back(headerTx.Bytes.byte_2.Byte);

		_serialPort->write(qba);
	}
}


void MaintenanceWindow::dataIngest()
{
    maint_header_byte_1 protocolByteIn1;
    maint_header_byte_2 protocolByteIn2;
    protocolByteIn1.Byte = _rxBuffer[0];
    protocolByteIn2.Byte = _rxBuffer[1];

    quint8 curOffset = 2;
    if (protocolByteIn1.Bits.get_min_delay)
    {
        quint16 minDelayMSB = _rxBuffer[curOffset];
        quint16 minDelayLSB = _rxBuffer[curOffset + 1];
        quint16 minDelay = ((minDelayMSB << 8) | minDelayLSB);
        _ui.spinMinDelay->setValue(minDelay);

        curOffset += 2;
    }

    if (protocolByteIn1.Bits.get_max_delay)
    {
        quint16 maxDelayMSB = _rxBuffer[curOffset];
        quint16 maxDelayLSB = _rxBuffer[curOffset + 1];
        quint16 maxDelay = ((maxDelayMSB << 8) | maxDelayLSB);
        _ui.spinMaxDelay->setValue(maxDelay);

        curOffset += 2;
    }

    if (protocolByteIn2.Bits.get_btn_state)
    {
        uint8_t valueIn = _rxBuffer[curOffset];
        uint8_t btnMask1 = BTN_MASK(1);
        uint8_t btnMask2 = BTN_MASK(2);
        uint8_t btnMask3 = BTN_MASK(3);
        uint8_t btnMask4 = BTN_MASK(4);
        uint8_t btnMask5 = BTN_MASK(5);

        uint8_t isBtn1 = valueIn & btnMask1;
        uint8_t isBtn2 = valueIn & btnMask2;
        uint8_t isBtn3 = valueIn & btnMask3;
        uint8_t isBtn4 = valueIn & btnMask4;
        uint8_t isBtn5 = valueIn & btnMask5;

        _ui.checkButton1->setChecked(isBtn1);
        _ui.checkButton2->setChecked(isBtn2);
        _ui.checkButton3->setChecked(isBtn3);
        _ui.checkButton4->setChecked(isBtn4);
        _ui.checkButton5->setChecked(isBtn5);

        curOffset += 1;
    }
    if (protocolByteIn2.Bits.get_analog2_in)
    {
        quint16 aInMSB = _rxBuffer[curOffset];
        quint16 aInLSB = _rxBuffer[curOffset + 1];
        quint16 aIn = ((aInMSB << 8) | aInLSB);
        _ui.spinAnalogIn2->setValue(aIn);

        curOffset += 2;
    }
    if (protocolByteIn2.Bits.get_analog5_in)
    {
        quint16 aInMSB = _rxBuffer[curOffset];
        quint16 aInLSB = _rxBuffer[curOffset + 1];
        quint16 aIn = ((aInMSB << 8) | aInLSB);
        _ui.spinAnalogIn5->setValue(aIn);

        curOffset += 2;
    }
    if (protocolByteIn2.Bits.get_btn_12_val)
    {
        quint16 btn1MSB = _rxBuffer[curOffset];
        quint16 btn1LSB = _rxBuffer[curOffset + 1];
        quint16 btn2MSB = _rxBuffer[curOffset + 2];
        quint16 btn2LSB = _rxBuffer[curOffset + 3];
        quint16 btn1 = ((btn1MSB << 8) | btn1LSB);
        quint16 btn2 = ((btn2MSB << 8) | btn2LSB);
        _ui.spinButton1->setValue(btn1);
        _ui.spinButton2->setValue(btn2);

        curOffset += 4;
    }
    if (protocolByteIn2.Bits.get_btn_345_val)
    {
        quint16 btn3MSB = _rxBuffer[curOffset];
        quint16 btn3LSB = _rxBuffer[curOffset + 1];
        quint16 btn4MSB = _rxBuffer[curOffset + 2];
        quint16 btn4LSB = _rxBuffer[curOffset + 3];
        quint16 btn5MSB = _rxBuffer[curOffset + 4];
        quint16 btn5LSB = _rxBuffer[curOffset + 5];
        quint16 btn3 = ((btn3MSB << 8) | btn3LSB);
        quint16 btn4 = ((btn4MSB << 8) | btn4LSB);
        quint16 btn5 = ((btn5MSB << 8) | btn5LSB);
        _ui.spinButton3->setValue(btn3);
        _ui.spinButton4->setValue(btn4);
        _ui.spinButton5->setValue(btn5);

        curOffset += 6;
    }
    if (protocolByteIn2.Bits.get_sw_ver)
    {
        _ui.lineSwVers->setText(
            QString("%1.%2-%3 %4").arg(char(_rxBuffer[curOffset])).arg(char(_rxBuffer[curOffset + 1])).arg(char(_rxBuffer[curOffset + 2]))
            .arg(_rxBuffer[curOffset + 3] == 'M' ? "MINI" :
            _rxBuffer[curOffset + 3] == 'F' ? "FULL" : "ERR"));

        curOffset += 4;
    }
}


void MaintenanceWindow::update_fsm(quint8 byteIn)
{
    //printf("rxByteIn(%d)\n", byteIn);
    //return;
	switch (_rxStatus)
	{
		case RxStatus::WAIT_SYNC:
			if (byteIn == SYNC_CHAR)
			{
				_expectedPayloadSize = 0;
				_receivedPayloadSize = 0;
				memset(&_rxBuffer, 0x00, 32);
                _rxStatus = RxStatus::WAIT_HEADER_1;
			}
		break;
        case RxStatus::WAIT_HEADER_1:
		{
            maint_header_byte_1 protocolByteIn;
			protocolByteIn.Byte = byteIn;

			if (protocolByteIn.Bits.zero == 0)
			{
                _rxBuffer[0] = byteIn;
                _rxStatus = RxStatus::WAIT_HEADER_2;
			}
			else
			{
				_rxStatus = RxStatus::WAIT_SYNC;
			}
		}
		break;
        case RxStatus::WAIT_HEADER_2:
        {
            maint_header_byte_2 protocolByteIn;
            protocolByteIn.Byte = byteIn;

            if (protocolByteIn.Bits.zero == 0)
            {
                _rxBuffer[1] = byteIn;

                maint_header_t* hdr = reinterpret_cast<maint_header_t*>(&_rxBuffer[0]);

                if (hdr->Bytes.byte_1.Bits.get_min_delay)
                {
                    _expectedPayloadSize += 2;
                }
                if (hdr->Bytes.byte_1.Bits.get_max_delay)
                {
                    _expectedPayloadSize += 2;
                }
                if (hdr->Bytes.byte_2.Bits.get_btn_state)
                {
                    _expectedPayloadSize += 1;
                }
                if (hdr->Bytes.byte_2.Bits.get_analog2_in)
                {
                    _expectedPayloadSize += 2;
                }
                if (hdr->Bytes.byte_2.Bits.get_analog5_in)
                {
                    _expectedPayloadSize += 2;
                }
                if (hdr->Bytes.byte_2.Bits.get_btn_12_val)
                {
                    _expectedPayloadSize += 4;
                }
                if (hdr->Bytes.byte_2.Bits.get_btn_345_val)
                {
                    _expectedPayloadSize += 6;
                }
                if (hdr->Bytes.byte_2.Bits.get_sw_ver)
                {
                    _expectedPayloadSize += 4;
                }

                if (_expectedPayloadSize)
                {
                    _receivedPayloadSize = 0;
                    _rxStatus = RxStatus::WAIT_PAYLOAD;
                }
                else
                {
                    dataIngest();
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
            _rxBuffer[sizeof(maint_header_t) + _receivedPayloadSize] = byteIn;
			_receivedPayloadSize += 1;

			if (_receivedPayloadSize == _expectedPayloadSize)
			{
                dataIngest();

				_rxStatus = RxStatus::WAIT_SYNC;
			}

		}
		break;
	}
}


void MaintenanceWindow::onArduinoRx()
{
	QByteArray qba = _serialPort->readAll();

    //printf("*****\n");
	for (auto& byte : qba)
	{
        //printf("rx: %hhu\n", quint8(byte));
		//printf("%c", byte);
		update_fsm(byte);
    }
    //printf("**********\n\n");
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

                // get sw vers
                maint_header_t headerTx;
                headerTx.Word = 0;
                headerTx.Bytes.byte_2.Bits.get_sw_ver = 1;

                QByteArray qba;
                qba.push_back(SYNC_CHAR);
                qba.push_back(headerTx.Bytes.byte_1.Byte);
                qba.push_back(headerTx.Bytes.byte_2.Byte);

                _serialPort->write(qba);
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
        QString portName = QString("/dev/ttyUSB%1").arg(i);
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
