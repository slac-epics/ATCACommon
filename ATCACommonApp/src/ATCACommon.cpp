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


static const char *driverName = "ATCACommonAsynDriver";
static ELLLIST    *pDrvList   = (ELLLIST *) NULL;

ATCACommonAsynDriver::ATCACommonAsynDriver(const char *portName, const char *pathString)
    :asynPortDriver(portName,
                     1, /* number of elements of this device */
                     NUM_ATCACOMMON_DET_PARAMS, /* number of asyn params of be cleared for each device */
                     asynInt32Mask | asynFloat64Mask | asynOctetMask | asynDrvUserMask | asynInt16ArrayMask | asynInt32ArrayMask | asynFloat64ArrayMask, /* Interface mask */
                     asynInt32Mask | asynFloat64Mask | asynOctetMask | asynEnumMask    | asynInt16ArrayMask | asynInt32ArrayMask | asynFloat64ArrayMask,  /* Interrupt mask */
                     1, /* asynFlags.  This driver does block and it is not multi-device, so flag is 1 */
                     1, /* Autoconnect */
                     0, /* Default priority */
                     0) /* Default stack size*/

{
    Path p_root;
    Path p_atcaCommon;


    port = epicsStrDup(portName);
    path = epicsStrDup(pathString);

    try {
      p_root = cpswGetRoot();
      p_atcaCommon = p_root->findByName(pathString);
    } catch (CPSWError &e) {
        fprintf(stderr, "CPSW Error: %s, file %s, line %d\n", e.getInfo().c_str(), __FILE__, __LINE__);
    }

    atcaCommon = IATCACommonFw::create(p_atcaCommon);
    ParameterSetup();

    jesdCnt_reset = 0;
    for(int i = 0; i < NUM_JESD; i++) {
        for(int j = 0; j < MAX_JESD_CNT; j++) {
            jesd_cnt_t *p = &(jesdCnt[i][j]);
            p->ref_cnt = p->curr_cnt = p->inc_cnt = 0;
        }
    }

    char     bs[256];
    uint32_t v;

    atcaCommon->getBuildStamp((uint8_t *) bs); setStringParam(p_buildStamp, bs);
    atcaCommon->getFpgaVersion(&v);            setIntegerParam(p_fpgaVersion, v);

    callParamCallbacks();
}

ATCACommonAsynDriver::~ATCACommonAsynDriver() {}

asynStatus ATCACommonAsynDriver::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *functionName = "writeInt32";

    status = (asynStatus) setIntegerParam(function, value);

    if(function == p_jesdCnt_reset) jesdCnt_reset  = 1;
    else
    for(int i = 0; i < MAX_DAQMUX_CNT; i++) {
        if(function == (p_daqMux+i)->p_triggerDaq              && value) atcaCommon->triggerDaq(i);
        else if(function == (p_daqMux+i)->p_armHwTrigger       && value) atcaCommon->armHwTrigger(i);
        else if(function == (p_daqMux+i)->p_freezeBuffer       && value) atcaCommon->freezeBuffer(i);
        else if(function == (p_daqMux+i)->p_clearTriggerStatus && value) atcaCommon->clearTriggerStatus(i);
        else if(function == (p_daqMux+i)->p_cascadedTrigger)             atcaCommon->cascadedTrigger(value?1:0, i);
        else if(function == (p_daqMux+i)->p_hardwareAutoRearm)           atcaCommon->hardwareAutoRearm(value?1:0, i);
        else if(function == (p_daqMux+i)->p_daqMode)                     atcaCommon->daqMode(value?1:0, i);
        else if(function == (p_daqMux+i)->p_enablePacketHeader)          atcaCommon->enablePacketHeader(value?1:0, i);
        else if(function == (p_daqMux+i)->p_enableHardwareFreeze)        atcaCommon->enableHardwareFreeze(value?1:0, i);
        else if(function == (p_daqMux+i)->p_decimationRateDivisor)       atcaCommon->decimationRateDivisor(value, i);
        else if(function == (p_daqMux+i)->p_dataBufferSize)              atcaCommon->dataBufferSize(value, i);
        else
        for(int j = 0; j < MAX_DAQMUX_CHN_CNT; j++) {
            if(function == (p_daqMux+i)->p_inputMuxSelect[j])            atcaCommon->inputMuxSelect(value, i, j);
            else if(function == (p_daqMux+i)->p_formatSignWidth[j])      atcaCommon->formatSignWidth(value, i, j);
            else if(function == (p_daqMux+i)->p_formatDataWidth[j])      atcaCommon->formatDataWidth(value, i, j);
            else if(function == (p_daqMux+i)->p_enableFormatSign[j])     atcaCommon->enableFormatSign(value?1:0, i, j);
            else if(function == (p_daqMux+i)->p_enableDecimation[j])     atcaCommon->enableDecimation(value?1:0, 1, j);
        }
    }

    return status;
}


void ATCACommonAsynDriver::ParameterSetup(void)
{
    char param_name[80];

    sprintf(param_name, UPTIMECNT_STR);     createParam(param_name, asynParamInt32, &p_upTimeCnt);
    sprintf(param_name, BUILDSTAMP_STR);    createParam(param_name, asynParamOctet, &p_buildStamp);
    sprintf(param_name, FPGAVERSION_STR);   createParam(param_name, asynParamInt32, &p_fpgaVersion);
    sprintf(param_name, ETH_UPTIMECNT_STR); createParam(param_name, asynParamInt32, &p_EthUpTimeCnt);
    sprintf(param_name, JESDCNT_RESET_STR); createParam(param_name, asynParamInt32, &p_jesdCnt_reset);
    sprintf(param_name, JESDCNT_MODE_STR);  createParam(param_name, asynParamInt32, &p_jesdCnt_mode);

    for(int i = 0; i < NUM_JESD; i++) {
        for(int j = 0; j < MAX_JESD_CNT; j++) {
            sprintf(param_name, JESDCNT_STR, i, j); createParam(param_name, asynParamInt32, &(p_jesdCnt[i][j]));
        }
    }

    for(int i = 0; i < MAX_DAQMUX_CNT; i++) {
        sprintf(param_name, TRIGGERDAQ_STR,         i); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_triggerDaq);
        sprintf(param_name, ARMHWTRIGGER_STR,       i); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_armHwTrigger);
        sprintf(param_name, CLEARTRIGGERSTATUS_STR, i); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_clearTriggerStatus);
        sprintf(param_name, CASCADEDTRIGGER_STR,    i); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_cascadedTrigger);
        sprintf(param_name, HARDWAREAUTOREARM_STR,  i); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_hardwareAutoRearm);
        sprintf(param_name, DAQMODE_STR,            i); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_daqMode);
        sprintf(param_name, ENABLEPACKETHEADER_STR, i); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_enablePacketHeader);
        sprintf(param_name, ENABLEHARDWAREFREEZE_STR, i); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_enableHardwareFreeze);
        sprintf(param_name, DECIMATIONRATEDIVISOR_STR, i); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_decimationRateDivisor);
        sprintf(param_name, DATABUFFERSIZE_STR,        i); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_dataBufferSize);
        sprintf(param_name, TIMESTAMP_SEC_STR,         i); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_timestamp_sec);
        sprintf(param_name, TIMESTAMP_NSEC_STR,        i); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_timestamp_nsec);
        sprintf(param_name, TRIGGERCOUNT_STR,          i); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_triggerCount);
        sprintf(param_name, DBGINPUTVALID_STR,         i); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_dbgInputValid);
        sprintf(param_name, DBGLINKREADY_STR,          i); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_dbgLinkReady);

        for(int j = 0; j < MAX_DAQMUX_CHN_CNT; j++) {
            sprintf(param_name, INPUTMUXSELECT_STR, i, j); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_inputMuxSelect[j]);
            sprintf(param_name, STREAMPAUSE_STR,    i, j); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_streamPause[j]);
            sprintf(param_name, STREAMREADY_STR,    i, j); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_streamReady[j]);
            sprintf(param_name, STREAMOVERFLOW_STR, i, j); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_streamOverflow[j]);
            sprintf(param_name, STREAMERROR_STR,    i, j); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_streamError[j]);
            sprintf(param_name, INPUTDATAVALID_STR, i, j); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_inputDataValid[j]);
            sprintf(param_name, STREAMENABLED_STR,  i, j); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_streamEnabled[j]);
            sprintf(param_name, FRAMECOUNT_STR,     i, j); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_frameCount[j]);
            sprintf(param_name, FORMATSIGNWIDTH_STR, i, j); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_formatSignWidth[j]);
            sprintf(param_name, FORMATDATAWIDTH_STR, i, j); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_formatDataWidth[j]);
            sprintf(param_name, ENABLEFORMATSIGN_STR, i, j); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_enableFormatSign[j]);
            sprintf(param_name, ENABLEDECIMATION_STR, i, j); createParam(param_name, asynParamInt32, &(p_daqMux+i)->p_enableDecimation[j]);
        }
    }
}

void ATCACommonAsynDriver::getJesdCount(void)
{
    if(jesdCnt_reset) {
        jesdCnt_reset = 0;

        for(int i = 0; i < NUM_JESD; i++) {
            for(int j = 0; j < MAX_JESD_CNT; j++) {
                jesd_cnt_t *p = &(jesdCnt[i][j]);
                p->ref_cnt = p->curr_cnt;
            }
        }
    }

    int mode;
    getIntegerParam(p_jesdCnt_mode, &mode);

    for(int i = 0; i < NUM_JESD; i++) {
        for(int j = 0; j < MAX_JESD_CNT; j++) {
            jesd_cnt_t *p = &(jesdCnt[i][j]);
            atcaCommon->getJesdCnt(&(p->curr_cnt), i, j);
            p->inc_cnt = p->curr_cnt - p->ref_cnt;

            setIntegerParam(p_jesdCnt[i][j], mode?p->inc_cnt:p->curr_cnt);
        }
    }

}

void ATCACommonAsynDriver::getDaqMuxStatus(void)
{
    for(int i = 0; i < MAX_DAQMUX_CNT; i++) {
      uint32_t val[MAX_DAQMUX_CHN_CNT];
      uint32_t v;

      atcaCommon->getFrameCount(val, i); 
      for(int j = 0; j < MAX_DAQMUX_CHN_CNT; j++) setIntegerParam((p_daqMux+i)->p_frameCount[j], val[j]);

      atcaCommon->getStreamEnabled(val, i);
      for(int j = 0; j < MAX_DAQMUX_CHN_CNT; j++) setIntegerParam((p_daqMux+i)->p_streamEnabled[j], val[j]?1:0);

      atcaCommon->getInputDataValid(val, i);
      for(int j = 0; j < MAX_DAQMUX_CHN_CNT; j++) setIntegerParam((p_daqMux+i)->p_inputDataValid[j], val[j]?1:0);

      atcaCommon->getStreamError(val, i);
      for(int j = 0; j < MAX_DAQMUX_CHN_CNT; j++) setIntegerParam((p_daqMux+i)->p_streamError[j], val[j]?1:0);

      atcaCommon->getStreamOverflow(val, i);
      for(int j = 0; j < MAX_DAQMUX_CHN_CNT; j++) setIntegerParam((p_daqMux+i)->p_streamOverflow[j], val[j]?1:0);

      atcaCommon->getStreamReady(val, i);
      for(int j = 0; j < MAX_DAQMUX_CHN_CNT; j++) setIntegerParam((p_daqMux+i)->p_streamReady[j], val[j]?1:0);

      atcaCommon->getStreamPause(val, i);
      for(int j = 0; j < MAX_DAQMUX_CHN_CNT; j++) setIntegerParam((p_daqMux+i)->p_streamPause[j], val[j]?1:0);

      atcaCommon->dbgLinkReady(&v, i);  setIntegerParam((p_daqMux+i)->p_dbgLinkReady, v);
      atcaCommon->dbgInputValid(&v, i); setIntegerParam((p_daqMux+i)->p_dbgInputValid, v);
      atcaCommon->getTriggerCount(&v, i); setIntegerParam((p_daqMux+i)->p_triggerCount, v);

      atcaCommon->getTimestamp(val+0, val+1, i); setIntegerParam((p_daqMux+i)->p_timestamp_sec, val[0]);
                                                 setIntegerParam((p_daqMux+i)->p_timestamp_nsec, val[1]);
  
    }
}

void ATCACommonAsynDriver::report(int level)
{
    printf("Driver Class: %s, (port, %s, path: %s)\n", driverName, port, path);
}

void ATCACommonAsynDriver::poll(void)
{
    uint32_t val;


    atcaCommon->getUpTimeCnt(&val);    setIntegerParam(p_upTimeCnt, val);
    atcaCommon->getEthUpTimeCnt(&val); setIntegerParam(p_EthUpTimeCnt, val);

    getJesdCount();
    getDaqMuxStatus();

    callParamCallbacks();
}



static void init_drvList(void)
{
    if(!pDrvList) {
        pDrvList = (ELLLIST *) mallocMustSucceed(sizeof(ELLLIST), "atcaCommonAsynDriver linked list");
        ellInit(pDrvList);
    }
}

static void add_drvList(drvNode_t *p)
{
    if(!pDrvList) init_drvList();
    ellAdd(pDrvList, (ELLNODE *) p);
}


static long atcaCommonAsynDriverReport(int interest)
{
    drvNode_t *p = NULL;

    if(!pDrvList) init_drvList();
    if(!ellCount(pDrvList)) return 0;

    p = (drvNode_t *) ellFirst(pDrvList);
    while(p && p->pDrv) {
        p->pDrv->report(interest);
        p = (drvNode_t *) ellNext(&p->node);
    }

    return 0;
}


static long atcaCommonAsynDriverPoll(void)
{
    drvNode_t *p = NULL;

    if(!pDrvList) init_drvList();
    if(!ellCount(pDrvList)) return 0;

    p = (drvNode_t *) ellFirst(pDrvList);
    while(p && p->pDrv) {
        p->pDrv->poll();
        p = (drvNode_t *) ellNext(&p->node);
    }

    return 0;
}


static long atcaCommonAsynDriverPollThread(void *p)
{
    while(1) {
        atcaCommonAsynDriverPoll();
        epicsThreadSleep(.5);
    }

    return 0;
}


static long atcaCommonAsynDriverInitialize(void)
{

    if(!(pDrvList  && ellCount(pDrvList))) return 0;


    char name[64];

    sprintf(name, "mon_%s", driverName);
    epicsThreadCreate(name, epicsThreadPriorityHigh - 10,
                      epicsThreadGetStackSize(epicsThreadStackMedium),
                      (EPICSTHREADFUNC) atcaCommonAsynDriverPollThread, (void *) NULL);

    return 0;
}



static long atcaCommonAsynDriverReport(int interest);
static long atcaCommonAsynDriverInitialize(void);

drvet atcaCommonAsynDriver = {
    2,
    (DRVSUPFUN) atcaCommonAsynDriverReport,
    (DRVSUPFUN) atcaCommonAsynDriverInitialize
};

epicsExportAddress(drvet, atcaCommonAsynDriver);


extern "C" {
/* consideration for Cexp */
int cpswATCACommonAsynDriverConfigure(const char *portName, const char *pathName)
{
    drvNode_t *p = (drvNode_t *) mallocMustSucceed(sizeof(drvNode_t), "ATCACommon Drvier");

    p->portName = epicsStrDup(portName);
    p->pathName = epicsStrDup(pathName);

    p->pDrv = new ATCACommonAsynDriver((const char *) p->portName, (const char *) p->pathName);

    add_drvList(p);

    return 0;
}


} /* end of extern C */


/* EPICS ioc shell command */

static const iocshArg    initArg0 = {"port name", iocshArgString};
static const iocshArg    initArg1 = {"path name", iocshArgString};
static const iocshArg    * const initArgs[] = {&initArg0, &initArg1};
static const iocshFuncDef initFuncDef = {"cpswATCACommonAsynDriverConfigure", 2, initArgs};
static void  initCallFunc(const iocshArgBuf *args)
{
    cpswATCACommonAsynDriverConfigure(args[0].sval, args[1].sval);
}

void cpswATCACommonAsynDriverRegister(void)
{
    iocshRegister(&initFuncDef, initCallFunc);
}

epicsExportRegistrar(cpswATCACommonAsynDriverRegister);


