#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

#include <string>
#include <sstream>
#include <fstream>

#include <sys/types.h>
#include <sys/stat.h>

#include <cantProceed.h>
#include <epicsTypes.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <epicsTimer.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <epicsPrint.h>
#include <epicsExit.h>
#include <ellLib.h>
#include <iocsh.h>

#include <yaml-cpp/yaml.h>
#include <yamlLoader.h>

#include <drvSup.h>
#include <epicsExport.h>
#include <registryFunction.h>

#include <asynPortDriver.h>
#include <asynOctetSyncIO.h>

#include "ATCACommon.h"
#include "debugStream.h"

static const char *driverName = "DebugStreamAsynDriver";
static const char *stream_type_str[] = {"uint32 -ULONG",
                                        "int32  -LONG",
                                        "unt16  -USHORT",
                                        "int16  -SHORT",
                                        "float32-FLOAT",
                                        "float64-DOUBLE" };

DebugStreamAsynDriver::DebugStreamAsynDriver(const char *portName, const char *named_root, const unsigned size, const bool header, const char *stream0, const char *stream1, const char *stream2, const char *stream3)
    : asynPortDriver(portName,
                     1,  /* number of elements of the device */
#if (ASYN_VERSION << 8 | ASYN_REVISION) < (4<<8 | 32)
                     NUM_DEBUGSTREAM_DET_PARAMS,   /* number of asyn params of be clear for each device */
#endif /* asyn version check, under 4.32 */
                     asynInt32Mask | asynFloat64Mask | asynOctetMask | asynDrvUserMask | asynInt16ArrayMask | asynInt32ArrayMask | asynFloat32ArrayMask | asynFloat64ArrayMask, /* Interface mask */
                     asynInt32Mask | asynFloat64Mask | asynOctetMask | asynEnumMask    | asynInt16ArrayMask | asynInt32ArrayMask | asynFloat32ArrayMask | asynFloat64ArrayMask,  /* Interrupt mask */
                     1, /* asynFlags.  This driver does block and it is not multi-device, so flag is 1 */
                     1, /* Autoconnect */
                     0, /* Default priority */
                     0) /* Default stack size*/
{
    Path p_root;

    this->port       = epicsStrDup(portName);
    this->named_root = epicsStrDup(named_root);
    this->timeoutCnt = 0;
    this->rdCnt      = 0;


    this->size      = size;
    this->header    = header;


    parameterSetup();


    for(int i = 0; i < 4; i++) {
        this->rdCnt_perStream[i] = 0;
        this->timeoutCnt_perStream[i] = 0;
        this->rdLen[i] = 0;
        this->s_type[i] = uint32;
        this->buff[i] = (uint8_t *) mallocMustSucceed(size, "DebugStreamAsynDriver");
        this->dumpStreamInfo[i].remainingPackets = 0;
    }

    // Initialize linked list of callback functions
    callback_list = (ELLLIST*) mallocMustSucceed(sizeof(ELLLIST*), "DebugStreamAsynDriver");
    ellInit(callback_list);

    try {
        p_root = (named_root && strlen(named_root))? cpswGetNamedRoot(named_root):cpswGetRoot();
    } catch (CPSWError &e) {
        fprintf(stderr, "CPSW Error: %s, file: %s, line: %d\n", e.getInfo().c_str(), __FILE__, __LINE__);
    }

    // ***************** Try to create _stream[0]
    try {
        _stream[0] = IStream::create(p_root->findByName(stream0));
    } 
    catch (InvalidArgError &e) {
        // Don't print error if the stream name is empty, as the user didn't
        // want to create this channel anyway.
    }
    catch (CPSWError &e) {
        fprintf(stderr, "CPSW Error: %s, file: %s, line: %d\n", e.getInfo().c_str(), __FILE__, __LINE__);
    }

    // ***************** Try to create _stream[1]
    try {
       _stream[1] = IStream::create(p_root->findByName(stream1));
    }
    catch (InvalidArgError &e) {
        // Don't print error if the stream name is empty, as the user didn't
        // want to create this channel anyway.
    }
    catch (CPSWError &e) {
        fprintf(stderr, "CPSW Error: %s, file: %s, line: %d\n", e.getInfo().c_str(), __FILE__, __LINE__);
    }

    // ***************** Try to create _stream[2]
    try {
       _stream[2] = IStream::create(p_root->findByName(stream2));
    } 
    catch (InvalidArgError &e) {
        // Don't print error if the stream name is empty, as the user didn't
        // want to create this channel anyway.
    }
    catch (CPSWError &e) {
        fprintf(stderr, "CPSW Error: %s, file: %s, line: %d\n", e.getInfo().c_str(), __FILE__, __LINE__);
    }

    // ***************** Try to create _stream[3]
    try{
       _stream[3] = IStream::create(p_root->findByName(stream3));
    }
    catch (InvalidArgError &e) {
        // Don't print error if the stream name is empty, as the user didn't
        // want to create this channel anyway.
    }
    catch (CPSWError &e) {
        fprintf(stderr, "CPSW Error: %s, file: %s, line: %d\n", e.getInfo().c_str(), __FILE__, __LINE__);
    }
}

DebugStreamAsynDriver::~DebugStreamAsynDriver() {}

bool DebugStreamAsynDriver::isChannelValid(int ch)
{
    // Using only return(_stream[ch]) didn't work
    return (_stream[ch]? true : false);
}

void DebugStreamAsynDriver::parameterSetup(void)
{
    char param_name[40];

    for(int i = 0; i< 4; i++) {
        sprintf(param_name, STREAMINT16_STR,     i); createParam(param_name, asynParamInt16Array,   &p_streamInt16[i]);
        sprintf(param_name, STREAMINT32_STR,     i); createParam(param_name, asynParamInt32Array,   &p_streamInt32[i]);
        sprintf(param_name, STREAMFLOAT32_STR,   i); createParam(param_name, asynParamFloat32Array, &p_streamFloat32[i]);
        sprintf(param_name, STREAMTYPE_STR,      i); createParam(param_name, asynParamInt32,        &p_streamType[i]);
        sprintf(param_name, READCOUNT_STR,       i); createParam(param_name, asynParamInt32,        &p_rdCnt[i]);
    }
}

int  DebugStreamAsynDriver::registerCallback(const int ch, STREAM_CALLBACK_FUNCTION cb_func, const void *cb_usr)
{
    if ((ch < 0) || (ch > MAX_WAVEFORMENGINE_CHN_CNT) || (! cb_func)) {
        return -1;
    } else {
        callback_node_t* newFunction = 
            (callback_node_t *) mallocMustSucceed ( sizeof(callback_node_t), 
                                        "debugStream callback linked list");

        newFunction->cb_func[ch] = cb_func;
        newFunction->cb_usr[ch] = (void*) cb_usr;

        ellAdd(callback_list, (ELLNODE*) newFunction);
    }

    return 0;
}

/* Call every function that was registered to receive callbacks, given an
 * specific channel.
 */
int  DebugStreamAsynDriver::triggerCallbacks(int ch, void *pBuf, unsigned size,  epicsTimeStamp time, int timeslot) 
{
    if ((ch < 0) || (ch > MAX_WAVEFORMENGINE_CHN_CNT)) {
        return -1;
    } else {
        callback_node_t* callback_node;
        STREAM_CALLBACK_FUNCTION function;
        ELLNODE* node = ellFirst(callback_list);

        while (node) {
            callback_node = (callback_node_t*) node;
            function = *(callback_node->cb_func[ch]);
            function(pBuf, size, time, timeslot, callback_node->cb_usr[ch]);

            node = ellNext(node);
        }
    }

    return 0;
}

void DebugStreamAsynDriver::streamPoll(const int i)
{
    // First check if the user created the channel
    if(! _stream[i]) {
        return;
    }

    epicsTimeStamp time;
    int timeslot;

    try {
        rdLen[i] = _stream[i]->read(buff[i], size, CTimeout(2000000));
    }
    catch (IOError &e) {
        // A timeout happened
        timeoutCnt ++;
        timeoutCnt_perStream[i] ++;
    }
    catch (CPSWError &e) {
        // Don't print, as we are inside a polling. Wait for the next try, as
        // rdLen[i] will be zero.
    }

    if(rdLen[i] == 0 ) {
        return;
    }

    rdCnt++; rdCnt_perStream[i]++;

    if (dumpStreamInfo[i].remainingPackets) {
        // Counter for loop
        std::size_t aaa; // To get rid of compiler warning messages
        // How many bytes to print during dump
        std::size_t byteQty = (std::size_t) dumpStreamInfo[i].wordQty * 8 > rdLen[i] ? rdLen[i] : dumpStreamInfo[i].wordQty * 8;

        printf("\nATCA Common stream dump - %d packets remaining\n", dumpStreamInfo[i].remainingPackets-1);
        printf("Message size: %d bytes\n", rdLen[i]);
        for (aaa=0; aaa<byteQty; ++aaa) {
            // Print word number on first column
            if (!(aaa % 8)) {
                printf("\n%lu   ", aaa/8);
            }

            printf ("%02X ", buff[i][aaa]);
        }

        printf("\n");
    }

    if(header) { 
        stream_with_header_t *p = (stream_with_header_t *) buff[i];
        time.nsec               = p->header.time.secPastEpoch;
        time.secPastEpoch       = p->header.time.nsec;
        timeslot = ((p->header.mod[3] >> 29) & 0x00000007);  // extract timeslot information from the timing pattern modifier
        setTimeStamp(&time);
        switch(s_type[i]) {
            case uint32:
            case int32:
                doCallbacksInt32Array(&p->payload.int32, (rdLen[i] - sizeof(timing_header_t) - sizeof(packet_header_t))/sizeof(epicsInt32), p_streamInt32[i], 0);
                break;
            case uint16:
            case int16:
                doCallbacksInt16Array(&p->payload.int16, (rdLen[i] - sizeof(timing_header_t) - sizeof(packet_header_t))/sizeof(epicsInt16), p_streamInt16[i], 0);
                break;
            case float32:
                doCallbacksFloat32Array(&p->payload.float32, (rdLen[i] - sizeof(timing_header_t) - sizeof(packet_header_t))/sizeof(epicsFloat32), p_streamFloat32[i], 0);
                break;
            case float64:
                doCallbacksFloat64Array(&p->payload.float64, (rdLen[i] - sizeof(timing_header_t) - sizeof(packet_header_t))/sizeof(epicsFloat64), p_streamFloat64[i], 0);
                break;
        }

        // Print data decoded from stream dump
        if (dumpStreamInfo[i].remainingPackets > 0) {
            printf("\nATCA Common mapped data:\n");
            printf("time.sec = %u\n", p->header.time.secPastEpoch);
            printf("time.nsec = %u\n", p->header.time.nsec);
            printf("\n");
        }

        triggerCallbacks(i, &(p->payload), rdLen[i] - sizeof(timing_header_t) - sizeof(packet_header_t), time, timeslot);

    } else {
        stream_without_header_t *p = (stream_without_header_t *) buff[i];
        epicsTimeGetCurrent(&time);
        timeslot = -1;     // no timeslot information
        setTimeStamp(&time);
        switch(s_type[i]) {
            case uint32:
            case int32:
                doCallbacksInt32Array(&p->payload.int32, (rdLen[i] - sizeof(packet_header_t))/sizeof(epicsInt32), p_streamInt32[i], 0);
                break;
            case uint16:
            case int16:
                doCallbacksInt16Array(&p->payload.int16, (rdLen[i] - sizeof(packet_header_t))/sizeof(epicsInt16), p_streamInt16[i], 0);
                break;
            case float32:
                doCallbacksFloat32Array(&p->payload.float32, (rdLen[i] - sizeof(packet_header_t))/sizeof(epicsFloat32), p_streamFloat32[i], 0);
                break;
            case float64:
                doCallbacksFloat64Array(&p->payload.float64, (rdLen[i] - sizeof(packet_header_t))/sizeof(epicsFloat64), p_streamFloat64[i], 0);
                break;
        }

        triggerCallbacks(i, &(p->payload), rdLen[i] - sizeof(packet_header_t), time, timeslot);
    }

    // Decrement dump packets counter in the end
    if (dumpStreamInfo[i].remainingPackets > 0) {
        --dumpStreamInfo[i].remainingPackets;
    }

}

void DebugStreamAsynDriver::report(int interest)
{
    printf("\ttiming header     : %s\n", header?"Enabled":"Disabled");
    printf("\ttimeout counter   :  %u %u %u %u\n", this->timeoutCnt_perStream[0],
                                                   this->timeoutCnt_perStream[1],
                                                   this->timeoutCnt_perStream[2],
                                                   this->timeoutCnt_perStream[3]);
    printf("\tread counter      :  %u %u %u %u\n", this->rdCnt_perStream[0], 
                                                   this->rdCnt_perStream[1], 
                                                   this->rdCnt_perStream[2], 
                                                   this->rdCnt_perStream[3]);
    printf("\tstream types      : %16s %16s %16s %16s\n", stream_type_str[(int)(this->s_type[0])],
                                                          stream_type_str[(int)(this->s_type[1])],
                                                          stream_type_str[(int)(this->s_type[2])],
                                                          stream_type_str[(int)(this->s_type[3])]);
    printf("\tread buffer length: %u %u %u %u\n", this->rdLen[0], this->rdLen[1], this->rdLen[2], this->rdLen[3]);

    if(interest > 4) {
        callback_node_t* callback_node;
           
        for(int i = 0; i < 4; i++) {
            printf("\tCallbacks for channel %d:\n", i);
            
            ELLNODE* node = ellFirst(callback_list);
            while (node) {
                callback_node = (callback_node_t*) node;
                if (callback_node->cb_func[i]) {
                    printf("\t\t function (%p), usr pvt (%p)\n", 
                        (void *) callback_node->cb_func[i], 
                        (void *) callback_node->cb_usr[i]);
                }
                node = ellNext(node);
            }   
        } // for(int i = 0; i < 4; i++)

    } // if(interest > 4)
}


asynStatus DebugStreamAsynDriver::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *functionName = "writeInt32";

    /* set the parameter in the parameter library */
    status = (asynStatus) setIntegerParam(function, value);

    switch(function) {
        default:
            break;
    }

    for(int i = 0; i< 4; i++) {
        if(function == p_streamType[i]) {
            s_type[i] = (stream_type_t) value;
            break;
        }
    }

    return status;
}

/*
 * During the polling of channel zero, the stream content will be dump to
 * screen with wordQty 64-bit words. packQty packets will be dump.
 */
void DebugStreamAsynDriver::dumpStreamContents(int ch, int wordQty, int packQty)
{
    if ((ch > MAX_WAVEFORMENGINE_CHN_CNT - 1) ||
        (ch < 0) ||
        (wordQty <= 0) || 
        (packQty <= 0))
    {
        return;
    }

    dumpStreamInfo[ch].wordQty = wordQty;
    dumpStreamInfo[ch].remainingPackets = packQty;
}

/* Return if the stream packets contain a header or not */
bool DebugStreamAsynDriver::hasHeader()
{
    return header;
}

static int streamThread(void *u)
{
    usrPvt_t *usrPvt =  (usrPvt_t*) u;
    DebugStreamAsynDriver *p = (DebugStreamAsynDriver *) usrPvt->pDrv;
    while(!usrPvt->stopLoop) {
        p->streamPoll(((usrPvt_t *)usrPvt)->ch);
    }

    epicsEventSignal(usrPvt->shutdownEvent);
    return 0;
}

void streamStop(void *u)
{
    usrPvt_t *usrPvt = (usrPvt_t*) u;
    usrPvt->stopLoop = true;
    epicsEventWait(usrPvt->shutdownEvent);
    epicsPrintf("debugStreamAsynDriver: Stop %s\n", usrPvt->name);
}

/*************
 * Returns a pointer to the DebugStreamAsynDriver with name equals to
 * streamPortName. The pointer is returned in the pDrv parameter. The function
 * returns zero only if it can find the driver with the asked name.
 *************/
int searchDebugStreamDriver(const char* streamPortName, DebugStreamAsynDriver** pDrv)
{
    // Prevents seg fault if the pointer to char is NULL
    if (! streamPortName) return -1;

    drvNode_t *pList = last_drvList_ATCACommon();
    debugStreamNode_t *pStream;
    while(pList) {
        pStream = pList->pdbStream0;
        if(pStream && !strcmp(pStream->portName, streamPortName)) break;  // found matched port

        pStream = pList->pdbStream1;
        if(pStream && !strcmp(pStream->portName, streamPortName)) break;  // found matched port

        pList = (drvNode_t *) ellPrevious(&pList->node);
    }

    if(pList && pStream && pStream->pDrv) {
        *pDrv = pStream->pDrv;
        return 0;
    }

    return -1;
}

int createStreamThread(int ch, const char *prefix_name, void *p, int (*streamThreadFunc)(void *))
{
    DebugStreamAsynDriver* tmpPDrv = (DebugStreamAsynDriver*) p;

    // Create thread only if the channel was created correctly.
    // If any error prevented the channel to be created, we should not
    // have a thread running.
    if (! tmpPDrv->isChannelValid(ch)) {
        return 0;
    }

    usrPvt_t *usrPvt = (usrPvt_t *) mallocMustSucceed(sizeof(usrPvt_t), "createStreamThread");
    
    char name[80];

    sprintf(name, "strm_%s%d", prefix_name, ch);
    usrPvt->name = epicsStrDup(name);

    usrPvt->ch   = ch;
    usrPvt->stopLoop = false;
    usrPvt->shutdownEvent = epicsEventMustCreate(epicsEventEmpty);
    usrPvt->pDrv = tmpPDrv;
    epicsThreadCreate(name, epicsThreadPriorityHigh,
                      epicsThreadGetStackSize(epicsThreadStackMedium),
                      (EPICSTHREADFUNC) streamThreadFunc, (void *) usrPvt);
    epicsAtExit3((epicsExitFunc) streamStop, (void*) usrPvt, usrPvt->name);

    return 0;
}


int createStreamThreads(debugStreamNode_t *p, int (*streamThreadFunc)(void *))
{
    for (int i = 0; i < 4; i++) { 
        createStreamThread(i, (const char*) p->portName, (void *) p->pDrv, streamThreadFunc); 
    }

    return 0;
}


static int report(debugStreamNode_t *p, int interest)
{
    printf("\tdebug stream driver (sub-driver %p), port %s, (class) %p\n", p, p->portName, p->pDrv);
    printf("\tstreams: %s, %s, %s, %s\n", p->streamNames[0], p->streamNames[1], p->streamNames[2], p->streamNames[3]);
    p->pDrv->report(interest);

    return 0;
}


extern "C" {

int debugStreamAsynDriver_report(debugStreamNode_t *p, int interest)
{
    return report(p, interest);
}


int registerStreamCallback(const char *portName, const int stream_channel, STREAM_CALLBACK_FUNCTION cb_func, void *cb_usr)
{
    DebugStreamAsynDriver* pDrv = NULL;

    // Look for the driver with the port name. Return if it can't find it.
    if (searchDebugStreamDriver(portName, &pDrv)) {
        return -1;
    } 
    
    pDrv->registerCallback(stream_channel, cb_func, cb_usr);
    return 0;
}

int cpswDebugStreamAsynDriverConfigure(const char *portName, unsigned size, const char *header, const char *stream0, const char *stream1, const char *stream2, const char *stream3)
{
    drvNode_t *pList = last_drvList_ATCACommon();
    debugStreamNode_t *pStream = (debugStreamNode_t *) mallocMustSucceed(sizeof(debugStreamNode_t), "Debugstream Driver");
    pStream->portName =  epicsStrDup(portName);
    pStream->streamNames[0] = (stream0 && strlen(stream0))? epicsStrDup(stream0): epicsStrDup("");
    pStream->streamNames[1] = (stream1 && strlen(stream1))? epicsStrDup(stream1): epicsStrDup("");
    pStream->streamNames[2] = (stream2 && strlen(stream2))? epicsStrDup(stream2): epicsStrDup("");
    pStream->streamNames[3] = (stream3 && strlen(stream3))? epicsStrDup(stream3): epicsStrDup("");
    pStream->pDrv = new DebugStreamAsynDriver(pStream->portName,
                                              (pList && pList->named_root)?pList->named_root: NULL,
                                              size,
                                              (!strcmp(header, "header_enabled"))?true:false,
                                              pStream->streamNames[0],
                                              pStream->streamNames[1],
                                              pStream->streamNames[2],
                                              pStream->streamNames[3]);
    if(!pList->pdbStream0) {
        pList->pdbStream0 = pStream;
        createStreamThreads(pList->pdbStream0, streamThread);
    }
    else if (!pList->pdbStream1) { 
        pList->pdbStream1 = pStream;
        createStreamThreads(pList->pdbStream1, streamThread);
    }
    else {   /* exception, two stream sub-driver instances are already launched */
    }

    return 0;
}

int cpswDebugStreamDump (const char* stream_portName, int ch, int wordQty, int packQty)
{
    DebugStreamAsynDriver* pDrv = NULL;

    // Look for the driver with the port name. Return if it can't find it.
    if (searchDebugStreamDriver(stream_portName, &pDrv)) {
        printf("No driver was found with this name\n");        
        return -1;
    }

    pDrv->dumpStreamContents(ch, wordQty, packQty);

    return 0;
}

} /* extern C */

/* epics ioc shell command */

static const iocshArg   initArg0 = {"portName", iocshArgString};
static const iocshArg   initArg1 = {"buffer size (samples)", iocshArgInt};
static const iocshArg   initArg2 = {"header (header_enable/header_disable)", iocshArgString};
static const iocshArg   initArg3 = {"stream0",  iocshArgString};
static const iocshArg   initArg4 = {"stream1",  iocshArgString};
static const iocshArg   initArg5 = {"stream2",  iocshArgString};
static const iocshArg   initArg6 = {"stream3",  iocshArgString};
static const iocshArg   *const initArgs[] = {&initArg0,
                                             &initArg1,
                                             &initArg2,
                                             &initArg3,
                                             &initArg4,
                                             &initArg5,
                                             &initArg6 };
static const iocshFuncDef initFuncDef = {"cpswDebugStreamAsynDriverConfigure", 7, initArgs};

static void initCallFunc(const iocshArgBuf *args)
{
    cpswDebugStreamAsynDriverConfigure(args[0].sval,
                                       args[1].ival,
                                       args[2].sval,
                                       args[3].sval,
                                       args[4].sval,
                                       args[5].sval,
                                       args[6].sval );
}



/* ioc shell command to dump stream contents on screen */

static const iocshArg   streamDumpArg0 = {"Stream port name", iocshArgString};
static const iocshArg   streamDumpArg1 = {"Channel number", iocshArgInt};
static const iocshArg   streamDumpArg2 = {"Number of 64-bit words to dump", iocshArgInt};
static const iocshArg   streamDumpArg3 = {"Number of sequential packets",  iocshArgInt};
static const iocshArg   *const streamDumpArgs[] = { &streamDumpArg0,
                                                    &streamDumpArg1,
                                                    &streamDumpArg2,
                                                    &streamDumpArg3 };
static const iocshFuncDef streamDumpFuncDef = {"cpswDebugStreamDump", 4, streamDumpArgs};

static void streamDumpCallFunc(const iocshArgBuf *args)
{
    cpswDebugStreamDump(args[0].sval,
                        args[1].ival,
                        args[2].ival,
                        args[3].ival);
}
static void cpswDebugStreamAsynDriverRegister(void)
{
    iocshRegister(&initFuncDef, initCallFunc);
    iocshRegister(&streamDumpFuncDef, streamDumpCallFunc);
}


epicsExportRegistrar(cpswDebugStreamAsynDriverRegister);
