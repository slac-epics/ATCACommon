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
#include <stdlib.h>

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

DebugStreamAsynDriver::DebugStreamAsynDriver(const char *portName, const char *named_root, 
                                             const unsigned size, const bool header, 
                                             const char *stream0, const char *stream1, 
                                             const char *stream2, const char *stream3)
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


    this->size        = size;
    this->header      = header;
    this->scopeIndex = -1;

    parameterSetup();


    for(int i = 0; i < 4; i++) {
        this->rdCnt_perStream[i] = 0;
        this->timeoutCnt_perStream[i] = 0;
        this->rdLen[i] = 0;
        this->s_type[i] = uint32;
        /* At this point we do not know how many samples we are expecting. We just know that the minimum
         * size is 2 bytes / sample (uint16_t). That is sizeof(double)/sizeof(uint16_t) times smaller 
         * than double. */
        this->doubleBuff[i] = (double *) mallocMustSucceed(size*sizeof(double)/sizeof(uint16_t), "DebugStreamAsynDriverDoubleBuff");                
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
        sprintf(param_name, STREAMFLOAT64_STR,   i); createParam(param_name, asynParamFloat64Array, &p_streamFloat64[i]);
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
    int totalElements = 0;

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
                totalElements = (rdLen[i] - sizeof(timing_header_t) - sizeof(packet_header_t))/sizeof(epicsInt32);
                for (int elementIndex = 0; elementIndex < totalElements; elementIndex++)
                    doubleBuff[i][elementIndex] = (double)((&p->payload.int32)[elementIndex]);                   
                break;
            case uint16:
            case int16:
                totalElements = (rdLen[i] - sizeof(timing_header_t) - sizeof(packet_header_t))/sizeof(epicsInt16);
                for (int elementIndex = 0; elementIndex < totalElements; elementIndex++)
                    doubleBuff[i][elementIndex] = (double)((&p->payload.int16)[elementIndex]);                 
                break;
            case float32:
                totalElements = (rdLen[i] - sizeof(timing_header_t) - sizeof(packet_header_t))/sizeof(epicsFloat32);
                for (int elementIndex = 0; elementIndex < totalElements; elementIndex++)
                    doubleBuff[i][elementIndex] = (double)((&p->payload.float32)[elementIndex]);                  
                break;
            case float64:
                totalElements = (rdLen[i] - sizeof(timing_header_t) - sizeof(packet_header_t))/sizeof(epicsFloat64);
                for (int elementIndex = 0; elementIndex < totalElements; elementIndex++)
                    doubleBuff[i][elementIndex] = (double)((&p->payload.float64)[elementIndex]);   
                break;
        }

        doCallbacksFloat64Array(doubleBuff[i], totalElements, p_streamFloat64[i], 0);

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
                totalElements = (rdLen[i] - sizeof(packet_header_t))/sizeof(epicsInt32);
                for (int elementIndex = 0; elementIndex < totalElements; elementIndex++)
                    doubleBuff[i][elementIndex] = (double)((&p->payload.int32)[elementIndex]);                
                break;
            case uint16:
            case int16:
                totalElements = (rdLen[i] - sizeof(packet_header_t))/sizeof(epicsInt16);
                for (int elementIndex = 0; elementIndex < totalElements; elementIndex++)
                    doubleBuff[i][elementIndex] = (double)((&p->payload.int16)[elementIndex]);                 
                break;
            case float32:
                totalElements = (rdLen[i] - sizeof(packet_header_t))/sizeof(epicsFloat32);
                for (int elementIndex = 0; elementIndex < totalElements; elementIndex++)
                    doubleBuff[i][elementIndex] = (double)((&p->payload.float32)[elementIndex]);                 
                break;
            case float64:
                totalElements = (rdLen[i] - sizeof(packet_header_t))/sizeof(epicsFloat64);
                for (int elementIndex = 0; elementIndex < totalElements; elementIndex++)
                    doubleBuff[i][elementIndex] = (double)((&p->payload.float64)[elementIndex]);                 
                break;
        }
        doCallbacksFloat64Array(doubleBuff[i], totalElements, p_streamFloat64[i], 0);
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

void DebugStreamAsynDriver::setScopeIndex(int index)
{
    scopeIndex = index;
}

int DebugStreamAsynDriver::setChannelType(const char *type, int index)
{
    drvNode_t *pList;
    if (strcmp(type, "uint32") == 0)
        s_type[index] = uint32;
    else if (strcmp(type, "int32") == 0)
        s_type[index] = int32;
    else if (strcmp(type, "uint16") == 0)
        s_type[index] = uint16;
    else if (strcmp(type, "int16") == 0)
        s_type[index] = int16; 
    else if (strcmp(type, "float32") == 0)
        s_type[index] = float32;    
    else if (strcmp(type, "float64") == 0)
        s_type[index] = float64;               
    else 
    {
        printf("Type %s for channel %d not recognized. Must be uint32, int32, uint16 or int16\n", type, index);
        return -1;
    }

    pList = last_drvList_ATCACommon();
    if ( !(pList && pList->named_root) )
    {
        printf("setChannelType() could not locate ATCACommonDriver. Exiting.\n");
        return -1;
    }
  
    pList->pDrv->getAtcaCommonAPI()->enableFormatSign(0, scopeIndex, index);
    pList->pDrv->getAtcaCommonAPI()->formatDataWidth(0, scopeIndex, index);
    pList->pDrv->getAtcaCommonAPI()->formatSignWidth(0, scopeIndex, index); 
            /*
    switch(s_type[index])
    {
        case int32:
            pList->pDrv->getAtcaCommonAPI()->enableFormatSign(1, scopeIndex, index);
            pList->pDrv->getAtcaCommonAPI()->formatDataWidth(0, scopeIndex, index);
            pList->pDrv->getAtcaCommonAPI()->formatSignWidth(31, scopeIndex, index);
            break;                    
        case uint16:
            pList->pDrv->getAtcaCommonAPI()->enableFormatSign(0, scopeIndex, index);
            pList->pDrv->getAtcaCommonAPI()->formatDataWidth(1, scopeIndex, index);
            pList->pDrv->getAtcaCommonAPI()->formatSignWidth(0, scopeIndex, index);
            break;                    
        case int16:
            pList->pDrv->getAtcaCommonAPI()->enableFormatSign(1, scopeIndex, index);
            pList->pDrv->getAtcaCommonAPI()->formatDataWidth(1, scopeIndex, index);
            pList->pDrv->getAtcaCommonAPI()->formatSignWidth(15, scopeIndex, index);
            break;     
        case uint32:
        default:
            pList->pDrv->getAtcaCommonAPI()->enableFormatSign(0, scopeIndex, index);
            pList->pDrv->getAtcaCommonAPI()->formatDataWidth(0, scopeIndex, index);
            pList->pDrv->getAtcaCommonAPI()->formatSignWidth(0, scopeIndex, index);  
    }      */
    return 0;
}

int DebugStreamAsynDriver::getScopeIndex()
{
    return scopeIndex;
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
    if ( !(pList && pList->named_root) )
    {
        printf("Could not find ATCACommonAsynDriver. Exiting.\n");
        return -1;
    }
    debugStreamNode_t *pStream = (debugStreamNode_t *) mallocMustSucceed(sizeof(debugStreamNode_t), "Debugstream Driver");
    pStream->portName =  epicsStrDup(portName);
    pStream->streamNames[0] = (stream0 && strlen(stream0))? epicsStrDup(stream0): epicsStrDup("");
    pStream->streamNames[1] = (stream1 && strlen(stream1))? epicsStrDup(stream1): epicsStrDup("");
    pStream->streamNames[2] = (stream2 && strlen(stream2))? epicsStrDup(stream2): epicsStrDup("");
    pStream->streamNames[3] = (stream3 && strlen(stream3))? epicsStrDup(stream3): epicsStrDup("");
    pStream->pDrv = new DebugStreamAsynDriver(pStream->portName,
                                              pList->named_root,
                                              size,
                                              (!strcmp(header, HEADER_EN_STRING))?true:false,
                                              pStream->streamNames[0],
                                              pStream->streamNames[1],
                                              pStream->streamNames[2],
                                              pStream->streamNames[3]);
    pStream->sizeInBytes = size;        
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

int scopeAsynDriverConfigure(const char *scopePortName, 
                             unsigned scopeIndex, const char* channel0Type, const char* channel1Type, 
                             const char* channel2Type, const char* channel3Type, const char * numSamplesOverride,
                             scope_cfg_type_t scope_cfg_type)
{

    dram_region_size_t allocableRegionSize = autogb;
    drvNode_t *pList = last_drvList_ATCACommon();
    if (!pList) 
    {
        printf("Error: Could not find ATCACommonAsynDriver. Exiting.\n");
        return -1;
    }

    DebugStreamAsynDriver* pDebugStreamDrv = NULL;
    const char *daqMux0ChannelNames[4] = {"Stream0", "Stream1", "Stream2", "Stream3"};
    const char *daqMux1ChannelNames[4] = {"Stream4", "Stream5", "Stream6", "Stream7"};
    const char **daqMuxChannelNames;
    unsigned    sizeInBytes;
    switch (scopeIndex)
    {
        case 0: daqMuxChannelNames = daqMux0ChannelNames; break;
        case 1: daqMuxChannelNames = daqMux1ChannelNames; break;
        default:
            printf("Error: scopeIndex value not recognized. Must be 0 or 1. Exiting.\n");
            return -1;
    }

    /* Get number of samples */
    uint32_t sampleSize = 0;
    if ( (strcmp(channel0Type, "uint32") == 0) || (strcmp(channel0Type, "int32") == 0) )
        sampleSize = sizeof(uint32_t);
    else 
        sampleSize = sizeof(uint16_t);    

    if (numSamplesOverride != NULL)
    {
        if (scope_cfg_type == cfg_advanced)
            sizeInBytes = strtoull(numSamplesOverride, NULL, 10) * sizeof(uint16_t);
        else {
            sizeInBytes = strtoull(numSamplesOverride, NULL, 10) * sampleSize;
        }
    }
    else
        sizeInBytes = DAQMUX_SAMPLES * sampleSize;
    if (0 == sizeInBytes)
    {
        printf("Error: Incorrect number of samples provided. Must be a base decimal 32-bit number. Exiting.\n");
        return -1;
    }
    /* Handle anomaly if same scope index is passed twice */
    if ( ( (pList->pdbStream0 != NULL) && (pList->pdbStream0->scopeIndex == scopeIndex) ) || 
       ( (pList->pdbStream1 != NULL) && (pList->pdbStream1->scopeIndex == scopeIndex) )
       )
    {
        printf("Error: Scope of provided scope index (%u) was already initialized. Exiting.\n", scopeIndex);
        return -1;
    }
    /* Two scopes already instantiated */
    if(pList->pdbStream0 != NULL && pList->pdbStream1 != NULL) {
        /* exception, two stream sub-driver instances are already launched */
        printf("Error: Both scopes already initialized. No more available resources. Exiting.\n");
    } 

    /* Both scopes must have the same allocated region size. 
     * DRAM mapping already was decided based on first scope. 
     * Second scope needs to play along. */
    if(pList->pdbStream0 != NULL) // 
    {
        dram_region_size_t scope0AllocableRegionSize = pList->pDrv->getAtcaCommonAPI()->getAllocableSize(pList->pdbStream0->sizeInBytes);
        dram_region_size_t scope1AllocableRegionSize = pList->pDrv->getAtcaCommonAPI()->getAllocableSize(sizeInBytes);
        if (scope1AllocableRegionSize > scope0AllocableRegionSize)
        {
            switch (scope0AllocableRegionSize){
                case twogb:  printf("Error: Due to scope 0 configured number of samples, scope 1 number of samples must be less than 134217728 samples @ 16-bits. Exiting.\n"); return -1;
                case fourgb: printf("Error: Due to scope 0 configured number of samples, scope 1 number of samples must be less than 268435456 samples @ 16-bits. Exiting.\n"); return -1;
                default: break;
            }
            
        } else if (scope1AllocableRegionSize < scope0AllocableRegionSize)
        {
            allocableRegionSize = scope0AllocableRegionSize;
        }
    }     

    /* This function is maintained for backward compatibility */
    if (0 != cpswDebugStreamAsynDriverConfigure(scopePortName, 
                                                sizeInBytes,
                                                "header_enabled", 
                                                daqMuxChannelNames[0], 
                                                daqMuxChannelNames[1], 
                                                daqMuxChannelNames[2], 
                                                daqMuxChannelNames[3]) )
    {
        printf("Error: cpswDebugStreamAsynDriverConfigure failed. Exiting.\n");
        return -1;
    }
    printf("Reserved buffer size in bytes=%u\n", sizeInBytes);

    /* Make scope index assignment. Backward compatible. */
    if(pList->pdbStream1 != NULL) {
        pList->pdbStream1->scopeIndex = scopeIndex;
    } else {
        pList->pdbStream0->scopeIndex = scopeIndex;
    }

    /* This if should never be exercised. cpswDebugStreamAsynDriverConfigure should have exited with error */
    if (searchDebugStreamDriver(scopePortName, &pDebugStreamDrv)) {
        printf("Error: scopeAsynDriverConfigure could not locate DebugStreamDriver. Exiting.\n");
        return -1;
    } 

    /* Reset waveform engine to defaults */
    pList->pDrv->getAtcaCommonAPI()->dataBufferSize(sizeInBytes / sizeof(uint32_t), scopeIndex);
    pList->pDrv->getAtcaCommonAPI()->setupDaqMux(scopeIndex);

    /* DRAM warnings if more than 2GB is used */
    if (sizeInBytes > 0x10000000 && sizeInBytes < 0x20000000 ) // Allocated 4GB
     {
        printf("WARNING: Using upper 4GB of DRAM.\n");
     } else if (sizeInBytes > 0x20000000) // Allocated 8GB
     {
        printf("WARNING: Using all DRAM (8GB). If BSA is activated, this will generate conflict and anomalies. Reduce the number of samples.\n");
     }    
    if (-1 == pList->pDrv->getAtcaCommonAPI()->setupWaveformEngine(scopeIndex, sizeInBytes , allocableRegionSize))
    {
        printf("Error: \n");
        return -1;
    }



    /* Override stored scope index value */
    pDebugStreamDrv->setScopeIndex(scopeIndex);

    /* Override channel type */
    if (0 != pDebugStreamDrv->setChannelType(channel0Type, 0))
        return -1;
    if (0 != pDebugStreamDrv->setChannelType(channel1Type, 1))
        return -1;
    if (0 != pDebugStreamDrv->setChannelType(channel2Type, 2))
        return -1;
    if (0 != pDebugStreamDrv->setChannelType(channel3Type, 3))
        return -1;

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
static const iocshArg   initArg1 = {"buffer size (Bytes)", iocshArgInt};
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

static const iocshArg   scopeAdvancedInitArg0 = {"scopePortName", iocshArgString};
static const iocshArg   scopeAdvancedInitArg1 = {"Scope (DaqMux) number (1/2)",  iocshArgInt};
static const iocshArg   scopeAdvancedInitArg2 = {"Channel 0 type (uint32/int32/uint16/int16)", iocshArgString};
static const iocshArg   scopeAdvancedInitArg3 = {"Channel 1 type (uint32/int32/uint16/int16)", iocshArgString};
static const iocshArg   scopeAdvancedInitArg4 = {"Channel 2 type (uint32/int32/uint16/int16)", iocshArgString};
static const iocshArg   scopeAdvancedInitArg5 = {"Channel 3 type (uint32/int32/uint16/int16)", iocshArgString};
static const iocshArg   scopeAdvancedInitArg6 = {"Override default number of samples @16-bits (4096) (optional)", iocshArgString};


static const iocshArg   *const scopeAdvancedInitArg[] = { &scopeAdvancedInitArg0,
                                                          &scopeAdvancedInitArg1,
                                                          &scopeAdvancedInitArg2,
                                                          &scopeAdvancedInitArg3,
                                                          &scopeAdvancedInitArg4,
                                                          &scopeAdvancedInitArg5,
                                                          &scopeAdvancedInitArg6 };
                                             
static const iocshFuncDef scopeAdvancedInitFuncDef = {"scopeAdvancedAsynDriverConfigure", 7, scopeAdvancedInitArg};

static void scopeAdvancedAsynDriverConfigureFunc(const iocshArgBuf *args)
{
    scopeAsynDriverConfigure(args[0].sval, args[1].ival, args[2].sval, args[3].sval,
                             args[4].sval, args[5].sval, args[6].sval , cfg_advanced);
}

static const iocshArg   scopeDefaultInitArg0 = {"scopePortName", iocshArgString};
static const iocshArg   scopeDefaultInitArg1 = {"Scope (DaqMux) number (1/2)",  iocshArgInt};
static const iocshArg   scopeDefaultInitArg2 = {"Channels type (uint32/int32/uint16/int16)", iocshArgString};
static const iocshArg   scopeDefaultInitArg3 = {"Override default number of samples (4096) (optional)", iocshArgString};


static const iocshArg   *const scopeDefaultInitArg[] = {&scopeDefaultInitArg0,
                                                        &scopeDefaultInitArg1,
                                                        &scopeDefaultInitArg2,
                                                        &scopeDefaultInitArg3 };
                                             
static const iocshFuncDef scopeDefaultInitFuncDef = {"scopeAsynDriverConfigure", 4, scopeDefaultInitArg};

static void scopeDefaultAsynDriverConfigureFunc(const iocshArgBuf *args)
{
    scopeAsynDriverConfigure(          args[0].sval,
                                       args[1].ival,
                                       args[2].sval,
                                       args[2].sval,
                                       args[2].sval,
                                       args[2].sval,
                                       args[3].sval,
                                       cfg_default );
}


static void scopeAsynDriverRegister(void)
{
    iocshRegister(&scopeAdvancedInitFuncDef, scopeAdvancedAsynDriverConfigureFunc);
    iocshRegister(&scopeDefaultInitFuncDef, scopeDefaultAsynDriverConfigureFunc);
}

epicsExportRegistrar(scopeAsynDriverRegister);

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
