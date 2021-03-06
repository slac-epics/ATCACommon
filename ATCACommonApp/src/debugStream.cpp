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

DebugStreamAsynDriver::DebugStreamAsynDriver(const char *portName, const char *named_root, const unsigned size, const bool header, const char *stream0, const char *stream1, const char *stream2, const char *stream3)
    : asynPortDriver(portName,
                     1,  /* number of elements of the device */
#if (ASYN_VERSION << 8 | ASYN_REVISION) < (4<<8 | 32)
                     NUM_DEBUGSTREAM_DET_PARAMS,   /* number of asyn params of be clear for each device */
#endif /* asyn version check, under 4.32 */
                     asynInt32Mask | asynFloat64Mask | asynOctetMask | asynDrvUserMask | asynInt16ArrayMask | asynInt32ArrayMask | asynFloat64ArrayMask, /* Interface mask */
                     asynInt32Mask | asynFloat64Mask | asynOctetMask | asynEnumMask    | asynInt16ArrayMask | asynInt32ArrayMask | asynFloat64ArrayMask,  /* Interrupt mask */
                     1, /* asynFlags.  This driver does block and it is not multi-device, so flag is 1 */
                     1, /* Autoconnect */
                     0, /* Default priority */
                     0) /* Default stack size*/
{
    Path p_root;

    this->port       = epicsStrDup(portName);
    this->named_root = epicsStrDup(named_root);
    this->rdCnt      = 0;


    this->size      = size;
    this->header    = header;

    parameterSetup();


    for(int i = 0; i < 4; i++) {
        this->rdCnt_perStream[i] = 0;
        this->rdLen[i] = 0;
        this->buff[i] = (uint8_t *) mallocMustSucceed(size, "DebugStreamAsynDriver");
    }

    try {
        p_root = (named_root && strlen(named_root))? cpswGetNamedRoot(named_root):cpswGetRoot();
        _stream[0] = IStream::create(p_root->findByName(stream0));
        _stream[1] = IStream::create(p_root->findByName(stream1));
        _stream[2] = IStream::create(p_root->findByName(stream2));
        _stream[3] = IStream::create(p_root->findByName(stream3));

    } catch (CPSWError &e) {
        fprintf(stderr, "CPSW Error: %s, file: %s, line: %d\n", e.getInfo().c_str(), __FILE__, __LINE__);
    }
}

DebugStreamAsynDriver::~DebugStreamAsynDriver() {}

void DebugStreamAsynDriver::parameterSetup(void)
{
    char param_name[40];

    for(int i = 0; i< 4; i++) {
        sprintf(param_name, STREAM_STR,     i); createParam(param_name, asynParamInt32Array, &p_stream[i]);
        sprintf(param_name, READCOUNT_STR,  i); createParam(param_name, asynParamInt32,      &p_rdCnt[i]);
    }
}

void DebugStreamAsynDriver::streamPoll(void)
{
    for(int i = 0; i < 4; i++) {
        rdLen[i] = _stream[i]->read(buff[i], size, CTimeout());

    }

    rdCnt++;

    for(int i = 0; i < 4; i++) {
        if(rdLen[i] == 0) continue;
        if(header) {
            stream_with_header_t *p = (stream_with_header_t *) buff[i];
            this->time.nsec         = p->header.time.secPastEpoch;
            this->time.secPastEpoch = p->header.time.nsec;

            setTimeStamp(&this->time);
            doCallbacksInt32Array(&p->payload, (rdLen[i] - sizeof(timing_header_t) - sizeof(packet_header_t))/sizeof(epicsInt32), p_stream[i], 0);
        } else {
            stream_without_header_t *p = (stream_without_header_t *) buff[i];
            epicsTimeGetCurrent(&this->time);
            setTimeStamp(&this->time);
            doCallbacksInt32Array(&p->payload, (rdLen[i] - sizeof(packet_header_t))/sizeof(epicsInt32), p_stream[i], 0);
        }
    }
}

void DebugStreamAsynDriver::streamPoll(const int i)
{
    epicsTimeStamp time;

    rdLen[i] = _stream[i]->read(buff[i], size, CTimeout());
    rdCnt++; rdCnt_perStream[i]++;

    if(rdLen[i] == 0) return;
    if(header) { 
        stream_with_header_t *p = (stream_with_header_t *) buff[i];
        time.nsec               = p->header.time.secPastEpoch;
        time.secPastEpoch       = p->header.time.nsec;
        setTimeStamp(&time);
        doCallbacksInt32Array(&p->payload, (rdLen[i] - sizeof(timing_header_t) - sizeof(packet_header_t))/sizeof(epicsInt32), p_stream[i], 0);
    } else {
        stream_without_header_t *p = (stream_without_header_t *) buff[i];
        epicsTimeGetCurrent(&time);
        setTimeStamp(&time);
        doCallbacksInt32Array(&p->payload, (rdLen[i] - sizeof(packet_header_t))/sizeof(epicsInt32), p_stream[i], 0);
    }

}

void DebugStreamAsynDriver::report(int interest)
{
    printf("\ttiming header     : %s\n", header?"Enabled":"Disabled");
/*    printf("\tread counter (total)  : %u\n", this->rdCnt); */
    printf("\tread counter      :  %u %u %u %u\n", this->rdCnt_perStream[0], this->rdCnt_perStream[1], this->rdCnt_perStream[2], this->rdCnt_perStream[3]);
    printf("\tread buffer length: %u %u %u %u\n", this->rdLen[0], this->rdLen[1], this->rdLen[2], this->rdLen[3]);
}


typedef struct {
    int ch;
    DebugStreamAsynDriver *pDrv;
} usrPvt_t;


static int streamThread(void *usrPvt)
{
    DebugStreamAsynDriver *p = (DebugStreamAsynDriver *) ((usrPvt_t *) usrPvt)->pDrv;
    while(1) {
        p->streamPoll(((usrPvt_t *)usrPvt)->ch);
    }
    return 0;
}


static int createStreamThread(int ch, const char *prefix_name, void *p)
{
    usrPvt_t *usrPvt = (usrPvt_t *) mallocMustSucceed(sizeof(usrPvt_t), "createStreamThread");

    char name[80];

    sprintf(name, "strm_%s%d", prefix_name, ch);

    usrPvt->ch   = ch;
    usrPvt->pDrv = (DebugStreamAsynDriver *) p;
    epicsThreadCreate(name, epicsThreadPriorityHigh,
                      epicsThreadGetStackSize(epicsThreadStackMedium),
                      (EPICSTHREADFUNC) streamThread, (void *) usrPvt);

    return 0;
}


static int createStreamThreads(void)
{
    drvNode_t *p = last_drvList_ATCACommon();
    while(p) {
        if(p->pdbStream0) {
            debugStreamNode_t *ps = p->pdbStream0;
            for (int i = 0; i < 4; i++) { createStreamThread(i, (const char*) ps->portName, (void *) ps->pDrv); }
        }
        if(p->pdbStream1) {
            debugStreamNode_t *ps = p->pdbStream1;
            for(int i = 0; i < 4; i++) { createStreamThread(i, (const char*) ps->portName, (void *) ps->pDrv); }
        }
        p = (drvNode_t *)ellPrevious(&p->node);
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

int debugStreamAsynDriver_createStreamThreads(void)
{
    return createStreamThreads();
}

int debugStreamAsynDriver_report(debugStreamNode_t *p, int interest)
{
    return report(p, interest);
}

int cpswDebugStreamAsynDriverConfigure(const char *portName, unsigned size, const char *header, const char *stream0, const char *stream1, const char *stream2, const char *stream3)
{
    drvNode_t *pList = last_drvList_ATCACommon();
    debugStreamNode_t *pStream = (debugStreamNode_t *) mallocMustSucceed(sizeof(debugStreamNode_t), "Debugstream Driver");
    pStream->portName =  epicsStrDup(portName);
    pStream->streamNames[0] = (stream0 && strlen(stream0))? epicsStrDup(stream0): NULL;
    pStream->streamNames[1] = (stream1 && strlen(stream1))? epicsStrDup(stream1): NULL;
    pStream->streamNames[2] = (stream2 && strlen(stream2))? epicsStrDup(stream2): NULL;
    pStream->streamNames[3] = (stream3 && strlen(stream3))? epicsStrDup(stream3): NULL;
    pStream->pDrv = new DebugStreamAsynDriver(pStream->portName,
                                              (pList && pList->named_root)?pList->named_root: NULL,
                                              size,
                                              (!strcmp(header, "header_enabled"))?true:false,
                                              pStream->streamNames[0],
                                              pStream->streamNames[1],
                                              pStream->streamNames[2],
                                              pStream->streamNames[3]);
    if(!pList->pdbStream0)        pList->pdbStream0 = pStream;
    else if (!pList->pdbStream1)   pList->pdbStream1 = pStream;
    else {   /* exception, two stream sub-driver instances are already launched */
    }

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


static void cpswDebugStreamAsynDriverRegister(void)
{
    iocshRegister(&initFuncDef, initCallFunc);
}


epicsExportRegistrar(cpswDebugStreamAsynDriverRegister);
