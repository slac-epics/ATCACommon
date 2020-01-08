#ifndef ATCA_COMMON_ASYN_DRIVER_H
#define ATCA_COMMON_ASYN_DRIVER_H

#include <asynPortDriver.h>
#include <epicsEvent.h>
#include <epicsTypes.h>
#include <epicsTime.h>

#include <cpsw_api_user.h>
#include <atcaCommon.h>
#include <vector>
#include <string>
#include <dlfcn.h>

#include <stdio.h>
#include <sstream>
#include <fstream>

#define MAX_WAVEFORMENGINE_CNT 2
#define MAX_WAVEFORMENGINE_CHN_CNT 4
#define MAX_DBG_STREAM_CNT 8
#define MAX_JESD_CNT       8
#define NUM_JESD           2
#define MAX_DAQMUX_CNT     2
#define MAX_DAQMUX_CHN_CNT 4

typedef struct {
    uint32_t    ref_cnt;     // reference
    uint32_t    curr_cnt;    // current count value
    uint32_t    inc_cnt;     // incremental
} jesd_cnt_t;

class ATCACommonAsynDriver:asynPortDriver {
    public:
        ATCACommonAsynDriver(const char *portName, const char *pathString, const char *named_root = NULL);
        ~ATCACommonAsynDriver();
        asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);

        void report(int level);
        void poll(void);

    private:
        char *port;
        char *path;
        CString t;
        ATCACommonFw       atcaCommon;
        jesd_cnt_t  jesdCnt[NUM_JESD][MAX_JESD_CNT];
        uint32_t    jesdCnt_reset;
        void ParameterSetup(void);
        void getJesdCount(void);
        void getDaqMuxStatus(void);
        void getWaveformEngineStatus(void);

        const char *inputMuxSelString(int idx);

    protected:
#if (ASYN_VERSION <<8 | ASYN_REVISION) < (4<<8 | 32)
        int firstATCACommonParam;
#define FIRST_ATCACOMMON_PARAM   firstATCACommonParam
#endif /* ASYN VERSION CHECK under 4.32 */
        int p_upTimeCnt;
        int p_buildStamp;
        int p_fpgaVersion;
        int p_EthUpTimeCnt;
        int p_jesdCnt_reset;
        int p_jesdCnt_mode;
        int p_jesdCnt[NUM_JESD][MAX_JESD_CNT];

        struct {
            int p_triggerDaq;
            int p_armHwTrigger;
            int p_freezeBuffer;
            int p_clearTriggerStatus;
            int p_cascadedTrigger;
            int p_hardwareAutoRearm;
            int p_daqMode;
            int p_enablePacketHeader;
            int p_enableHardwareFreeze;
            int p_decimationRateDivisor;
            int p_dataBufferSize;
            int p_timestamp_sec;
            int p_timestamp_nsec;
            int p_triggerCount;
            int p_dbgInputValid;
            int p_dbgLinkReady;
            int p_inputMuxSelect[MAX_DAQMUX_CHN_CNT];
            int p_readInpMuxSel[MAX_DAQMUX_CHN_CNT];
            int p_streamPause[MAX_DAQMUX_CHN_CNT];
            int p_streamReady[MAX_DAQMUX_CHN_CNT];
            int p_streamOverflow[MAX_DAQMUX_CHN_CNT];
            int p_streamError[MAX_DAQMUX_CHN_CNT];
            int p_inputDataValid[MAX_DAQMUX_CHN_CNT];
            int p_streamEnabled[MAX_DAQMUX_CHN_CNT];
            int p_frameCount[MAX_DAQMUX_CHN_CNT];
            int p_formatSignWidth[MAX_DAQMUX_CHN_CNT];
            int p_formatDataWidth[MAX_DAQMUX_CHN_CNT];
            int p_enableFormatSign[MAX_DAQMUX_CHN_CNT];
            int p_enableDecimation[MAX_DAQMUX_CHN_CNT];
        } p_daqMux[MAX_DAQMUX_CNT];

        struct {
            int p_startAddr[MAX_WAVEFORMENGINE_CHN_CNT];
            int p_endAddr[MAX_WAVEFORMENGINE_CHN_CNT];
            int p_wrAddr[MAX_WAVEFORMENGINE_CHN_CNT];
            int p_enabled[MAX_WAVEFORMENGINE_CHN_CNT];
            int p_mode[MAX_WAVEFORMENGINE_CHN_CNT];
            int p_status[MAX_WAVEFORMENGINE_CHN_CNT];
            int p_msgDest[MAX_WAVEFORMENGINE_CHN_CNT];
            int p_framesAfterTrigger[MAX_WAVEFORMENGINE_CHN_CNT];
            int p_initialize;
        } p_waveformEngine[MAX_WAVEFORMENGINE_CNT];

        
        int p_dbgStream[MAX_DBG_STREAM_CNT];


#if (ASYN_VERSION <<8 | ASYN_REVISION) < (4<<8 | 32)      
        int lastATCACommonParam;
#define LAST_ATCACOMMON_PARAM   lastATCACommonParam
#endif /* asyn version check, under 4.32 */
};

#if (ASYN_VERSION <<8 | ASYN_REVISION) < (4<<8 | 32)
#define NUM_ATCACOMMON_DET_PARAMS ((int)(&LAST_ATCACOMMON_PARAM - &FIRST_ATCACOMMON_PARAM-1))
#endif /* asyn version check, under 4.32 */

// ATCA Common
#define UPTIMECNT_STR              "upTimeCnt"
#define BUILDSTAMP_STR             "buildStamp"
#define FPGAVERSION_STR            "fpgaVersion"
#define ETH_UPTIMECNT_STR          "EthUpTimeCnt"
// JESD
#define JESDCNT_RESET_STR          "jesdCntReset"
#define JESDCNT_MODE_STR           "jesdCntMode"
#define JESDCNT_STR                "jesdCnt_%d_%d"
// DAQMUX
#define TRIGGERDAQ_STR             "triggerDaq_%d"
#define ARMHWTRIGGER_STR           "armHwTrigger_%d"
#define FREEZEBUFFER_STR           "freezeBuffer_%d"
#define CLEARTRIGGERSTATUS_STR     "clearTriggerStatus_%d"
#define CASCADEDTRIGGER_STR        "cascadedTrigger_%d"
#define HARDWAREAUTOREARM_STR      "hardwareAutoRearm_%d"
#define DAQMODE_STR                "daqMode_%d"
#define ENABLEPACKETHEADER_STR     "enablePacketHeader_%d"
#define ENABLEHARDWAREFREEZE_STR   "enableHardwareFreeze_%d"
#define DECIMATIONRATEDIVISOR_STR  "decimateRateDivisor_%d"
#define DATABUFFERSIZE_STR         "dataBufferSize_%d"
#define TIMESTAMP_SEC_STR          "timestamp_sec_%d"
#define TIMESTAMP_NSEC_STR         "timestamp_nsec_%d"
#define TRIGGERCOUNT_STR           "triggerCount_%d"
#define DBGINPUTVALID_STR          "dbgInputValid_%d"
#define DBGLINKREADY_STR           "dbgLinkReady_%d"
#define INPUTMUXSELECT_STR         "inputMuxSelect_%d_%d"
#define READINPMUXSEL_STR          "readInpMuxSel_%d_%d"
#define STREAMPAUSE_STR            "streamPause_%d_%d"
#define STREAMREADY_STR            "streamReady_%d_%d"
#define STREAMOVERFLOW_STR         "streamOverflow_%d_%d"
#define STREAMERROR_STR            "streamError_%d_%d"
#define INPUTDATAVALID_STR         "inputDataValid_%d_%d"
#define STREAMENABLED_STR          "streamEnabled_%d_%d"
#define FRAMECOUNT_STR             "frameCount_%d_%d"
#define FORMATSIGNWIDTH_STR        "formatSignWidth_%d_%d"
#define FORMATDATAWIDTH_STR        "formatDataWidth_%d_%d"
#define ENABLEFORMATSIGN_STR       "enableFormatSign_%d_%d"
#define ENABLEDECIMATION_STR       "enableDecimation_%d_%d"

#define WFBUFSTARTADDR_STR         "wfBuffStartAddr_%d_%d"
#define WFBUFENDADDR_STR           "wfBuffEndAddr_%d_%d"
#define WFBUFWRADDR_STR            "wfBuffWrAddr_%d_%d"
#define WFBUFENABLE_STR            "wfBuffEnable_%d_%d"
#define WFBUFMODE_STR              "wfBuffMode_%d_%d"
#define WFBUFSTATUS_STR            "wfBuffStatus_%d_%d"
#define WFBUFMSGDEST_STR           "wfBuffMsgDest_%d_%d"
#define WFBUFFRAFTTRG_STR          "wfBuffFramesAfterTrigger_%d_%d"
#define WFBUFINIT_STR              "wfBuffInit_%d"

// Debug Stream
#define DBGSTREAM_STR              "dbg_stream_%d"




typedef struct {
    ELLNODE                 node;
    char                    *named_root;
    char                    *portName;
    char                    *pathName;
    ATCACommonAsynDriver    *pDrv;
}  drvNode_t;

drvNode_t * last_drvList_ATCACommon(void);

#endif /* ATCA_COMMON_ASYN_DRIVER_H */

