#include "pch.h"
#include "IALibrary.h"

#pragma once
#define _PT_64_



#include <PvDevice.h>
#include <PvBuffer.h>
#include <PvStream.h>
#include <PvStreamRaw.h>
#include <PvConfigurationWriter.h>
#include <PvConfigurationReader.h>
#include <PvGenTypes.h>
#include <PvSystem.h>
#include <PvBufferWriter.h>
#include <PvSerialPortIPEngine.h>
#include <PvPipeline.h>





#define BUFFER_COUNT ( 16 )
#define TEST_COUNT ( 16 )

#define M "00-11-1c-02-04-00"
#define confFilePath "/conf.pvxml"



#include <windows.h>
#include <conio.h>
#include <process.h>

#include <chrono>
#include <iostream>
#include <fstream>
#include <string>
using namespace std::chrono;
using namespace std;


 




static PvStream lStream;
static PvDevice lDevice;

static PvGenParameterArray* lDeviceParams;
static PvGenInteger* lTLLocked;
static PvGenInteger* lPayloadSize;
static PvGenCommand* lStart;
static PvGenCommand* lStop;
static PvGenCommand* lResetTimestamp;
static PvGenCommand* lResetStats;

static PvGenParameterArray* lStreamParams;
static PvGenInteger* lCount;
static PvGenFloat* lFrameRate;
static PvGenFloat* lBandwidth;
static PvGenInteger* lWidthCommand;
static PvGenInteger* lHeightCommand;


static int dataindex = 0;
static int nos = 1;
static double* times;
static double* times2;

static int enableDebug = 0;// If equals one, then debug is enabled, otherwise debug is disabled.
static ofstream logfile;

static PvUInt32 numOfSamples;



void resetTimeStamp()
{
    lResetTimestamp->Execute();
}

int connect()
{


    PvSystem lSystem;
    PvUInt32 lInterfaceCount, lDeviceCount;
    PvInterface* lInterface = NULL;
    lSystem.Find();
    lInterfaceCount = lSystem.GetInterfaceCount();
    PvDeviceInfo* tempDeviceInfo;
    PvString deviceString;
    PvUInt32 deviceIndex;
    bool isBreak = 0;
    PvDeviceInfo* lDeviceInfo = NULL;
    for (PvUInt32 x = 0; x < lInterfaceCount; x++)
    {
        lInterface = lSystem.GetInterface(x);
        lInterface->Find();
        lDeviceCount = lInterface->GetDeviceCount();
        for (PvUInt32 y = 0; y < lDeviceCount; y++)
        {
            tempDeviceInfo = lInterface->GetDeviceInfo(y);
            deviceString = tempDeviceInfo->GetMACAddress();
            if (deviceString == M)
            {
                deviceIndex = y;
                lDeviceInfo = tempDeviceInfo;
                isBreak = 1;
                break;
            }
        }

        if (isBreak)
        {
            break;
        }

    }


    // If no device is selected, abort
    if (lDeviceInfo == NULL)
    {
        return 1;
    }

    // Connect to the GEV Device
    if (!lDevice.Connect(lDeviceInfo).IsOK())
    {
        return 2;
    }
    PvConfigurationReader lReader;
    PvString lFilenameToSave(confFilePath);
    PvResult r = lReader.Load(lFilenameToSave);

    r = lReader.Restore(PvUInt32(deviceIndex), &lDevice);


    // Get device parameters need to control streaming
    lDeviceParams = lDevice.GetGenParameters();
    
    //lPayloadSize = dynamic_cast<PvGenInteger*>(lDeviceParams->Get("PayloadSize"));
    //lStart = dynamic_cast<PvGenCommand*>(lDeviceParams->Get("AcquisitionStart"));
    //lStop = dynamic_cast<PvGenCommand*>(lDeviceParams->Get("AcquisitionStop"));

    



    // Negotiate streaming packet size
    lDevice.NegotiatePacketSize();


    PvUInt32 lStreamCount = lReader.GetStreamCount();
    lReader.Restore(PvUInt32(0), lStream);

    // Open stream - have the PvDevice do it for us
    lStream.Open(lDeviceInfo->GetIPAddress());

    // Have to set the Device IP destination to the Stream
    lDevice.SetStreamDestination(lStream.GetLocalIPAddress(), lStream.GetLocalPort());
 

    // Get stream parameters/stats
    //lStreamParams = lStream.GetParameters();
    //lCount = dynamic_cast<PvGenInteger*>(lStreamParams->Get("ImagesCount"));
    //lFrameRate = dynamic_cast<PvGenFloat*>(lStreamParams->Get("AcquisitionRateAverage"));
    //lBandwidth = dynamic_cast<PvGenFloat*>(lStreamParams->Get("BandwidthAverage"));

    lResetTimestamp = dynamic_cast<PvGenCommand*>(lDeviceParams->Get("GevTimestampControlReset"));

    

    resetTimeStamp();

    return 0;
}

void lock()
{
    if (lTLLocked != NULL)
    {
        lTLLocked->SetValue(1);
    }
}

void unlock()
{
    if (lTLLocked != NULL)
    {
        printf("Resetting TLParamsLocked to 0\n");
        lTLLocked->SetValue(0);
    }
}



int acquireIm(unsigned char* bp)
{

    if (enableDebug)
    {
        // Open log file for debug
        logfile.open(".\IALibraryLogFile.txt");
    }

    lResetStats = lStream.GetParameters()->GetCommand("Reset");
    lTLLocked = dynamic_cast<PvGenInteger*>(lDeviceParams->Get("TLParamsLocked"));
    lPayloadSize = dynamic_cast<PvGenInteger*>(lDevice.GetGenParameters()->Get("PayloadSize"));
    lWidthCommand = dynamic_cast<PvGenInteger*>(lDevice.GetGenParameters()->Get("Width"));
    lHeightCommand = dynamic_cast<PvGenInteger*>(lDevice.GetGenParameters()->Get("Height"));
    lStart = dynamic_cast<PvGenCommand*>(lDevice.GetGenParameters()->Get("AcquisitionStart"));
    lStop = dynamic_cast<PvGenCommand*>(lDevice.GetGenParameters()->Get("AcquisitionStop"));
    PvGenEnum* lPixelFormat = lDevice.GetGenParameters()->GetEnum("PixelFormat");

    // Create the PvPipeline object
    PvPipeline lPipeline(&lStream);

    PvInt64 lSize = 0;
    //lDeviceParams->GetIntegerValue("PayloadSize", lSize);
    // Get payload size
    PvInt64 lPayloadSizeValue = 0;
    lPayloadSize->GetValue(lPayloadSizeValue);

    // Compute poor man's payload size - for devices not maintaning PayloadSize properly
    PvInt64 lPoorMansPayloadSize = 0;
    PvInt64 lWidthValue, lHeightValue;
    if ((lWidthCommand != NULL) && (lHeightCommand != NULL) && (lPixelFormat != NULL))
    {
        
        lWidthCommand->GetValue(lWidthValue);
        lHeightCommand->GetValue(lHeightValue);

        PvInt64 lPixelFormatValue;
        lPixelFormat->GetValue(lPixelFormatValue);
        PvInt64 lPixelSizeInBits = PvGetPixelBitCount(static_cast<PvPixelType>(lPixelFormatValue));

        lPoorMansPayloadSize = (lWidthValue * lHeightValue * lPixelSizeInBits) / 8;
    }

    // Take max, let pipeline know what the payload size is
    PvInt64 lBestPayloadSize = max(lPayloadSizeValue, lPoorMansPayloadSize);
    if ((lBestPayloadSize > 0) && (lBestPayloadSize < UINT_MAX))
    {
        lSize =  lBestPayloadSize;
    }
    
    lPipeline.SetBufferSize(static_cast<PvUInt32>(lSize));
    
    lPipeline.SetBufferCount(BUFFER_COUNT); // Increase for high frame rate without missing block IDs

    lPipeline.Start();

    auto startTotAcq = high_resolution_clock::now();
    (PvUInt8*)bp;

    // TLParamsLocked is optional but when present, it MUST be set to 1
    // before sending the AcquisitionStart command
    lResetStats->Execute();
    lock();

    PvResult lResult = lStart->Execute(); 
    
    unsigned long long lWidth = 0, lHeight = 0;
    PvImage* lImage = NULL;
    int nextpos = 0;

    
    if (enableDebug) logfile << "New Acq:\n";
    
    PvBuffer* lBuffer = NULL;
    PvResult lOperationResult; 	
    lResult = NULL;

    for (int i = 0; i < nos; i++)
    {           
        auto start = high_resolution_clock::now();
        // Retrieve next buffer
        lResult = lPipeline.RetrieveNextBuffer(&lBuffer, 1000, &lOperationResult);
        auto stop = high_resolution_clock::now();
        // If debug is enabled, print time measurment to the lo file
        if(enableDebug) logfile << "loop index: " << i << ", Retrieve duration: " << duration_cast<microseconds>(stop - start).count() << ", Result Code: " << lOperationResult.GetCodeString() << ", result description: " << lOperationResult.GetDescription();

        if (lResult.IsOK())
        {
            if (lOperationResult.IsOK())
            {
                
                if (lBuffer->GetPayloadType() == PvPayloadTypeImage)
                {
                    // Get image specific buffer interface
                    lImage = lBuffer->GetImage();                   
                    lWidth = (unsigned long long)lImage->GetWidth();
                    lHeight = (unsigned long long)lImage->GetHeight();

                    start = high_resolution_clock::now();
                    memcpy(bp, lImage->GetDataPointer(), lWidth * lHeight * 2 * sizeof(PvUInt8));
                    stop = high_resolution_clock::now();
                    bp = bp + lWidth * lHeight * 2 * sizeof(PvUInt8);
                    // If debug is enabled, print time measurment to the log file
                    if (enableDebug) logfile << " memcpy: " << duration_cast<microseconds>(stop - start).count() << " , ";
                }

            }

            start = high_resolution_clock::now();
            //lStream.QueueBuffer(lBuffer);
            lPipeline.ReleaseBuffer(lBuffer);
            stop = high_resolution_clock::now();
            if (enableDebug) logfile << " queue buffer: " << duration_cast<microseconds>(stop - start).count() << "ms";
        }
        if (enableDebug) logfile << "\n";
    }


    lStop->Execute();
    lPipeline.Stop();

   

    unlock();

    auto stopTotAcq = high_resolution_clock::now();
    if (enableDebug) logfile << "Total Acuisition of Frames: " << duration_cast<microseconds>(stopTotAcq - startTotAcq).count() << "; \n\n ";
    logfile.close();

    return 0;
}



int conf(int p, int v)
{
    lDeviceParams->SetEnumValue("Uart0BaudRate", "Baud9600");
    lDeviceParams->SetEnumValue("Uart0NumOfStopBits", "One");
    lDeviceParams->SetEnumValue("Uart0Parity", "None");

    // For this test to work without attached serial hardware we enable the port loopback
    lDeviceParams->SetBooleanValue("Uart0Loopback", true);

    // Open serial port
    PvSerialPortIPEngine lPort;


    PvResult lResult = lPort.Open(&lDevice, PvIPEngineSerial0);

    lPort.SetRxBufferSize(2 << TEST_COUNT);
 
    PvUInt32 lSize = 6;

    PvUInt8* lInBuffer = new PvUInt8[lSize];
    PvUInt8* lOutBuffer = new PvUInt8[lSize];

    lInBuffer[0] = 0; lInBuffer[1] = 0; lInBuffer[2] = 0; lInBuffer[3] = 0; lInBuffer[4] = 0; lInBuffer[5] = 0;
    
    switch (p)
    {
    case 1:
        lInBuffer[1] = 1;
        if (v != 0 && v != 1 && v != 2)
        {
            return -1;
        }
        lInBuffer[2] = (PvUInt8)v;
        break;
    case 2:
        lInBuffer[1] = 2;
        if (v < 0 || v > 199)
        {
            return -1;
        }
        lInBuffer[2] = (PvUInt8)v;
        break;
    case 3:
        lInBuffer[1] = 3;
        if (v < 0 || v > 128)
        {
            return -1;
        }
        lInBuffer[2] = (PvUInt8)v;
        break;
    case 5:
        lInBuffer[1] = 5;
        break;
    case 6:
        lInBuffer[1] = 6;
        break;
    case 7:
        lInBuffer[1] = 7;
        if (v != 0 && v != 1)
        {
            return -1;
        }
        lInBuffer[2] = (PvUInt8)v;
        break;
    case 8:
        lInBuffer[1] = 8;
        if (v != 0 && v != 1)
        {
            return -1;
        }
        lInBuffer[2] = (PvUInt8)v;
        break;
    case 9:
        lInBuffer[1] = 9;
        if (v != 0 && v != 1)
        {
            return -1;
        }
        lInBuffer[2] = (PvUInt8)v;
        break;
    default:
        lPort.Close();
        return -1;
    }


    // Send the buffer content on the serial port
    PvUInt32 lBytesWritten = 0;
    lResult = lPort.Write(lInBuffer, lSize, lBytesWritten);
    if (!lResult.IsOK())
    {
        // Unable to send data over serial port!
        return -1;
    }

    // Close serial port
    lPort.Close();
    return 0;
}

int SetSamplingRate(int i)
{
    if (i != 0 && i != 1)
    {
        return -1;
    }
    conf(9, i);
    return 0;
}

int SetDelay(int v)
{
    if (v < 0 || v > 199) {
        return -1;
    }
    return conf(2, v);
}

int SetCoupling(int i)
{
    if (i != 0 && i != 1)
    {
        return -1;
    }
    conf(8, i);
    return 0;
}

int SetNumOfFrames(int n)
{
    nos = n;
    return 0;
}

int SetSamplelength(int i) {
    switch (i)
    {
        case 0:
            numOfSamples = 2032;
            break;
        case 1:
            numOfSamples = 1008;
            break;
        case 2:
            numOfSamples = 496;
            break;
        default:
            return -1;
    }
    return conf(1, i);
}

int SetADCGain(int v)
{
    if (v < 0 || v > 128)
    {
        return -1;
    }
    conf(3, v);
    return 0;
}

int SetFilterBandwidth(int i)
{
    if (i != 0 && i != 1)
    {
        return -1;
    }
    conf(7, i);
    return 0;
}

int disconnect()
{
    lStream.AbortQueuedBuffers();

    

    lStream.Close();
    lDevice.Disconnect();

    return lDevice.IsConnected();
}

int isDeviceConnected()
{
    return lDevice.IsConnected();
}

int setTimesArray(double* t, double*t2)
{
    times = t;
    times2 = t2;
    return 0;
}

int RequestDebug()
{
    enableDebug = 1;
    return 0;
}

int AbortDebug()
{
    enableDebug = 0;
    return 0;
}
