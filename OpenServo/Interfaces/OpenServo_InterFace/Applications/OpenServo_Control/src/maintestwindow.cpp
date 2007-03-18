/***************************************************************************
 *   Copyright (C) 2007 by Barry Carter   *
 *   barry.carter@gmail.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "maintestwindow.h"
#include "registers.h"
#include "aboutbox.h"

#include <dlfcn.h>

#include <qstring.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qlistview.h>
#include <qpushbutton.h>
#include <qfiledialog.h>
#include <qtooltip.h>
#include <qtimer.h>
#include <qcheckbox.h>
#include <qspinbox.h>
#include <qregexp.h>
#include <qtextedit.h>

mainTestWindow::mainTestWindow(QWidget *parent, const char *name)
    :testMainWin(parent, name)
{
	bool ok;
	servoPWMenabled = true;
	logPrint("Welcome to OpenServo test application v0.2");
	//initialise the variables
	setPosOut = setPos->text().toInt(&ok, 0);
	setupPOut = setupP->text().toInt(&ok, 0);
	setupIOut = setupI->text().toInt(&ok, 0);
	setupDOut = setupD->text().toInt(&ok, 0);

	//initialise backup variables for later comparison of changes
	bckSetPosOut = setPosOut;
	bckSetupPOut = setupPOut;
	bckSetupIOut = setupIOut;
	bckSetupDOut = setupDOut;

	void * libhandle; // handle to the shared lib when opened

	libhandle = dlopen ( "libOSIFlib.so.1", RTLD_LAZY ); // open the shared lib

	// if the open failed, NULL was returned.  Print the error code
	if ( libhandle == NULL ) 
	{
		fprintf ( stderr, "fail 1: %s\n", dlerror() );
		//return
	} 

	/*GetProcAddress*/
	OSIF_init    = (OSIF_initfunc)dlsym(libhandle, "OSIF_init");
	OSIF_deinit  = (OSIF_deinitfunc)dlsym(libhandle, "OSIF_deinit");
	OSIF_write   = (OSIF_writefunc)dlsym(libhandle, "OSIF_write");
	OSIF_read    = (OSIF_readfunc)dlsym(libhandle, "OSIF_read");
	OSIF_reflash = (OSIF_reflashfunc)dlsym(libhandle, "OSIF_reflash");
	OSIF_scan    = (OSIF_scanfunc)dlsym(libhandle, "OSIF_scan");
	OSIF_probe   = (OSIF_probefunc)dlsym(libhandle, "OSIF_probe");
	OSIF_command = (OSIF_commandfunc)dlsym(libhandle, "OSIF_command");
	OSIF_get_adapter_name  = (OSIF_get_adapter_namefunc)dlsym(libhandle, "OSIF_get_adapter_name");
	OSIF_get_adapter_count = (OSIF_get_adapter_countfunc)dlsym(libhandle, "OSIF_get_adapter_count");

	// if bar is NULL, print() wasn't found in the lib, print error message	
	if ( OSIF_init == NULL ) 
	{
		char buf[255];
		sprintf(buf,"fail 2: %s", dlerror() );
		logPrint( buf );
	} 
	if (OSIF_init() < 0)
	{
		logPrint("Error initialising USB");
		//do some more stuff disable buttons 
	}
	else { logPrint("OSIF initiased! Now run a bus scan...");OSIFinit = true; }

	readTimer = new QTimer( this );
	connect( readTimer, SIGNAL(timeout()), SLOT(readServo()) );
	readInterval = timerIntervalBox->text().toInt();

	servo = 0x10;

	adapterCount = -1;
	adapterList->setSorting(-1);

}

mainTestWindow::~mainTestWindow()
{
	if (OSIFinit)
	{
		OSIF_deinit();
	}
}

void mainTestWindow::scanBus()
{
	int n;
	char name[255];
	QListViewItem *listItem;
	char logbuf[255];

	//check to see if the bus is initialised. If so deinitialise and rescan all busses.
	//thisis the only way to detect for new adapters on the bus.
	//if (OSIFinit)
	{
		OSIF_deinit();
		if (OSIF_init()<0)
		{
			logPrint("Error: No compatible adapters found");
			OSIFinit =false;
		}
		else
		{ 
			OSIFinit =true;
		}
	}
	adapterList->clear();
	servoList->clear();

	if (OSIFinit == false )
	{
		//error no adapters
		return;
	}
	//get list of adapters
	adapterCount = OSIF_get_adapter_count();

	if (adapterCount >=0)
	{
		for (n=0;n<=adapterCount;n++)
		{
			OSIF_get_adapter_name(n, &name[0]);
			sprintf( logbuf, "Found adapter %s", name);
			logPrint(logbuf);
			listItem = new QListViewItem( adapterList, QString(name) );
		}
		//select last one in the list
		adapterList->setSelected(listItem, true);
	}
	else
	{
		return;
	}

	OSIF_scan( adapter, &devices[0], &devCount );


	//stop the timer to make sure we dont trounce data.

	liveData->setChecked( false );

	for( n = 0; n< devCount; n++)
	{
		sprintf( logbuf, "device at 0x%02x", devices[n]);
		logPrint( logbuf );
        	listItem = new QListViewItem( servoList, QString().sprintf("0x%-2x", devices[n]) );
	}
	if (n>0)
	{
		//highlight that last item in the list
		servoList->setSelected(listItem, true);
		readBtn->setEnabled(true);
		writeBtn->setEnabled(true);
		liveData->setEnabled(true);
		timerIntervalBox->setEnabled(true);

	}
	else
	{
		readBtn->setEnabled(false);
		writeBtn->setEnabled(false);
		liveData->setEnabled(false);
		timerIntervalBox->setEnabled(false);
	}
}


void mainTestWindow::writeServo()
{
	bool ok;

	setPosOut = setPos->text().toInt(&ok, 0);
	//write the position regardless
	writeData(adapter, servo, REG_SEEK_POSITION_HI, (char*)setPos->text().ascii(), 2);


	//send P
	setupPOut = setupP->text().toInt(&ok, 0);
	//check to see if the variables have changed since that last click
	if (setupPOut != bckSetupPOut )
	{
		if (writeData(adapter, servo, REG_PID_PGAIN_HI, (char*)setupP->text().ascii(), 2) <0)
		{ logPrint("I2C send error"); }
		bckSetupPOut = setupPOut;
	}
	
	setupIOut = setupI->text().toInt(&ok, 0);
	if (setupIOut != bckSetupIOut )
	{
		writeData(adapter, servo, REG_PID_IGAIN_HI, (char*)setupI->text().ascii(), 2);
		bckSetupIOut = setupIOut;
	}
	
	setupDOut = setupD->text().toInt(&ok, 0);
	if ( setupDOut != bckSetupDOut )
	{
		writeData(adapter, servo, REG_PID_DGAIN_HI, (char*)setupD->text().ascii(), 2);
		bckSetupDOut = setupDOut;
	}

	setupSMinOut = setupSMin->text().toInt(&ok, 0);
	if ( setupSMinOut != bckSetupSMinOut )
	{
		writeData(adapter, servo, REG_MIN_SEEK_HI, (char*)setupSMin->text().ascii(), 2);
		bckSetupSMinOut = setupSMinOut;
	}

	setupSMaxOut = setupSMax->text().toInt(&ok, 0);
	if ( setupSMaxOut != bckSetupSMaxOut )
	{
		writeData(adapter, servo, REG_MAX_SEEK_HI, (char*)setupSMax->text().ascii(), 2);
		bckSetupSMaxOut = setupSMaxOut;
	}

	if (!readTimer->isActive())
	{
		readServo();	
	}
}


void mainTestWindow::readServo()
{
	int addr = 0x08;
	unsigned char buf[255];

	//Read from I2C
	if (readData(adapter,servo,addr,buf,14)>0)
	{
		//print what we got
		int n;
		char tmpbuf[255];
		char newbuf[255];
		newbuf[0] ='\0';
		for (n=0;n<14;n++)
		{
			sprintf( tmpbuf, "0x%02x ", buf[n]);
			strcat(newbuf, tmpbuf);
		}
		logPrint( newbuf );
	} 
	//parse into form
	posLbl->setText( QString("%1").arg(hexarrToInt( &buf[0] ) ) );
	spdLbl->setText( QString("%1").arg(hexarrToInt( &buf[2] ) ) );
	curLbl->setText( QString("%1").arg(hexarrToInt( &buf[4] ) ) );
	pwmLbl->setText( QString("%1").arg(hexarrToInt( &buf[6] ) ) );
	voltLbl->setText( QString("%1").arg(hexarrToInt( &buf[12] ) ) );
	//update log view with raw data
}

void mainTestWindow::readPids()
{
	int addr = REG_PID_PGAIN_HI;
	unsigned char buf[255];

	//Read from I2C
	if (readData(adapter,servo,addr,buf,10)>0)
	{
		//print what we got
		int n;
		char tmpbuf[255];
		char newbuf[255];
		newbuf[0] ='\0';
		for (n=0;n<10;n++)
		{
			sprintf( tmpbuf, "0x%02x ", buf[n]);
			strcat(newbuf, tmpbuf);
		}
		logPrint( newbuf );
	} 
	//parse into form
	setupP->setText( QString().sprintf("0x%02x%02x",buf[0],buf[1] ));
	setupD->setText( QString().sprintf("0x%02x%02x",buf[2],buf[3] ));
	setupI->setText( QString().sprintf("0x%02x%02x",buf[4],buf[5] ));

	setupSMin->setText( QString().sprintf("0x%02x%02x",buf[6],buf[7] ));
	setupSMax->setText( QString().sprintf("0x%02x%02x",buf[8],buf[9] ));
	//update log view with raw data
}


int mainTestWindow::writeData( int adapter, int servo, int addr, char *val, size_t len )
{
	int byteData;
	//check validity of options
	byteData = parseOption(val);
	unsigned char outData[2];
	char logbuf[255];

	outData[0] = (byteData >>8)&0x00FF;
	outData[1] = (byteData)&0x00FF;

	sprintf(logbuf, "Hex Out: 0x%02x  0x%02x", outData[0], outData[1]);
	logPrint( logbuf );

	//write
	if (OSIF_write(adapter,servo,addr,outData,len) < 0)
	{
		logPrint("Write failed");
		return -1;
	}
	logPrint("Wrote data OK");
	return 1;
}

int mainTestWindow::readData(int adapter, int servo, int addr, unsigned char *buf, size_t len)
{

	if (OSIF_read(adapter,servo,addr,buf,len) < 0)
	{
		logPrint("I2C read failed");
		return -1;
	}
	logPrint("Read data OK");
	return 1;
}

int mainTestWindow::hexarrToInt(unsigned char *ncurrent) 
{
  return (ncurrent[0]<<8)|ncurrent[1];
}

//gets all of the command line data and puts into a character array.
int mainTestWindow::parseData( char *argv[], int count, int len, char *data )
{
	int n;
	for (n = 0; n< len; n++)
	{
		*data = parseOption(argv[count]);
		data++;
		count++;
	}
	
	return 1;
}

//Read char string as either hex or integer and convert to int
int mainTestWindow::parseOption( char *arg )
{
        int byte;
	bool ok;

	QString qarg = QString(arg);
	//automatically convert the type from string int/hex to int
	byte = qarg.toInt(&ok, 0);

        return byte;
}

void mainTestWindow::commandReboot()
{
	unsigned char msg[2];

	msg[0] = 0x00;
	//write
	if (OSIF_write(adapter,servo,TWI_CMD_RESET,msg,1) < 0)
	{
		logPrint("I2C Write reboot failed");
		return;
	}
	logPrint("Wrote reboot command OK");
}


void mainTestWindow::commandFlash()
{
	int servo;
	bool wasActive = false;

	if ( readTimer->isActive() )
	{
		wasActive = true;
		readTimer->stop();
	}

	if (OSIF_reflash(0, servo, 0x7F, (char*)fileToFlash.ascii())<0)
	{
		logPrint("Flash failed!");
		return;
	}
	else
	{
		logPrint("Flash completed OK");
	}
	if (wasActive)
	{
		readTimer->start(readInterval);
	}
}


void mainTestWindow::commandDefault()
{

	unsigned char msg[2];

	msg[0] = 0x00;

	//write
	if (OSIF_write(adapter,servo,TWI_CMD_REGISTERS_DEFAULT,msg,1) < 0)
	{
		logPrint("I2C write default failed");
		return;
	}
	logPrint("Wrote default register command OK");

}


void mainTestWindow::commandRestore()
{
	unsigned char msg[2];

	msg[0] = 0x00;

	//write
	if (OSIF_write(adapter,servo,TWI_CMD_REGISTERS_RESTORE,msg,1) < 0)
	{
		logPrint("I2C write restore failed");
		return;
	}
	logPrint("Wrote restore register command OK");

}


void mainTestWindow::commandSave()
{
	unsigned char msg[2];

	msg[0] = 0x00;

	//write
	if (OSIF_write(adapter,servo,TWI_CMD_REGISTERS_SAVE,msg,1) < 0)
	{
		logPrint("I2C write save failed");
		return;

	}
	logPrint("Wrote save register command OK");
}


void mainTestWindow::commandPWM()
{
	unsigned char msg[2];

	msg[0] = 0x00;

	if (servoPWMenabled == false)
	{
		//write
		if (OSIF_write(adapter,servo,TWI_CMD_PWM_ENABLE,msg,1) < 0)
		{
			logPrint("I2C write PWM failed");
			return;
		}
		logPrint("Wrote PWM off command OK");

		servoPWMenabled = true;
		//change the text on the button
		cmdPWM->setText("PWM off");
	} 
	else 
	{
		//write
		if (OSIF_write(adapter,servo,TWI_CMD_PWM_DISABLE,msg,1) < 0)
		{
			logPrint("I2C write PWM failed");
			return;
		}
		logPrint("Wrote PWM on command OK");

		//change the text on the button
		cmdPWM->setText("PWM on");
		servoPWMenabled = false;
	}

}

void mainTestWindow::flashFileLoad()
{
	QString workingDirectory = QString("c:\\");
	QFileDialog *dlg = new QFileDialog( workingDirectory,
	QString("Intel Hex (*.hex)"), 0, 0, TRUE );
	dlg->setCaption( QFileDialog::tr( "Open" ) );
	dlg->setMode( QFileDialog::ExistingFile );
	QString result;
	if ( dlg->exec() == QDialog::Accepted ) {
		result = dlg->selectedFile();
		workingDirectory = dlg->url();
		fileToFlash = result;
		cmdFlash->setEnabled(true);
		QToolTip::add(cmdFlash, result);
	}
	delete dlg;
}

void mainTestWindow::requestVoltage()
{
	unsigned char msg[2];

	msg[0] = 0x00;

	//write
	if (OSIF_write(adapter,servo,TWI_CMD_VOLTAGE_READ,msg,1) < 0)
	{
		logPrint("I2C write Voltage request failed");
		return;
	}
	logPrint("Wrote voltage request command OK");
	//after the voltage is sampled, read back the new sample
	readServo();
}

void mainTestWindow::liveDataClick()
{
	if (liveData->isChecked())
	{
		readTimer->start(readInterval);
		readBtn->setEnabled(false);
	}
	else
	{
		readTimer->stop();
		readBtn->setEnabled(true);
	}
}

void mainTestWindow::timerIntervalChange(int timerVal)
{
	//check the value is not too low minimum 20
	if ( timerVal < 20 )
		timerVal = 20;
	//check it is not too high. A reasnable cap is 2 seconds
	if ( timerVal > 2000 )
		timerVal = 2000;
	//set the new timer value.
	readInterval = timerVal;
}

void mainTestWindow::genericReadData()
{
	unsigned char buf[255];
	QString data;
	bool ok;
	//Read from I2C
	if (readData(adapter,genericDevice->text().toInt(&ok, 0),genericRegister->text().toInt(&ok, 0),buf,genericLen->text().toInt(&ok, 0))>0)
	{
		//print what we got
		int n;
		char tmpbuf[255];
		char newbuf[255];
		newbuf[0] ='\0';

		for (n=0;n<genericLen->text().toInt(&ok, 0);n++)
		{
			sprintf( tmpbuf, "0x%02x ", buf[n]);
			strcat(newbuf, tmpbuf);
			data.append(QString(tmpbuf));

		}
		logPrint( newbuf );

	} 
	genericDataRead->setText(data);
	//parse into form
	//genericReadData->setText( QString("%1").arg(hexarrToInt( &buf[16] ) ) );
	//update log view with raw data
}


void mainTestWindow::genericWriteData()
{

	bool ok;
	unsigned char outData[255];
	int byteData;
	int i,n,j;
	//make sure we are initialised
	if ( OSIFinit == false )
	{
		logPrint("Error, no adpapters found");
		return;
	}

	// make sure it is not 0
	if (genericLen->text().toInt(&ok, 0) == 0)
	{ // error 
		return; 
	}
	QString boxString(genericData->text());

	outData[0] = '\0';
	QString t;
	j=0; i=0;
	for( n=0;n<genericLen->text().toInt(&ok, 0);n++)
	{
		// pull one string off the list
		if (n>0)
		{
			i = boxString.find( QRegExp(" "), i+1 );
			t= boxString.mid(i+1,4);
		}
		else
			t= boxString.mid(0,4);

		byteData = t.toInt(&ok, 0);
		
		//write those hex bytes to array
	
//		outData[j] = (byteData >>8)&0x00FF;
		outData[j] = (byteData)&0x00FF;
		j++;

	}
	OSIF_write(adapter, genericDevice->text().toInt(&ok, 0), genericRegister->text().toInt(&ok, 0),outData, genericLen->text().toInt(&ok, 0));

}


void mainTestWindow::servoSelectChange(QListViewItem *selItem)
{
	char logbuf[255];
	servo = parseOption((char *)selItem->text(0).ascii());
	sprintf(logbuf, "Selected servo 0x%02x", servo);
	logPrint(logbuf);
	//do a read
	readServo();
	//get pids
	readPids();
}

void mainTestWindow::adapterSelectChange(QListViewItem *listItem)
{
	QListViewItem *listViewItem;
	int row;
	QListViewItemIterator it(adapterList);
	char logbuf[255];

	while ((listViewItem = it.current())) 
	{
		if (listViewItem->isSelected())
			break;
		++it;
		++row;
	}
	adapter = row;
	sprintf( logbuf, "Selected Adapter %s", listItem->text(0).ascii());
	logPrint(logbuf);
}

void mainTestWindow::logPrint( char *logData)
{
	loggingData.append(QString(logData));
	loggingData.append(QString("\n"));
	logBox->setText(loggingData);
	printf("%s\n", logData);
}

void mainTestWindow::logBoxClear(int,int)
{
	logBox->clear();
}

void mainTestWindow::aboutClicked()
{

	//theAboutWin = new(AboutClass);
	AboutClass dlg( this );
	//theAboutWin->show();

 
	// Show it and wait for Ok or Cancel
	if( dlg.exec() == QDialog::Accepted )
	{
		; //just wait for close.
	}
}






