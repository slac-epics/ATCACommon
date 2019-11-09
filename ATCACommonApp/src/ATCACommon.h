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

#define MAX_JESD_CNT       8
#define NUM_JESD           2
typedef struct {
    uint32_t    ref_cnt;     // reference
    uint32_t    curr_cnt;    // current count value
    uint32_t    inc_cnt;     // incremental
} jesd_cnt_t;

class ATCACommonAsynDriver:asynPortDriver {
    public:
        ATCACommonAsynDriver(const char *portName, const char *pathString);
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

    protected:
        int firstATCACommonParam;
#define FIRST_ATCACOMMON_PARAM   firstATCACommonParam
        int p_upTimeCnt;
        int p_buildStamp;
        int p_fpgaVersion;
        int p_EthUpTimeCnt;
        int p_jesdCnt_reset;
        int p_jesdCnt_mode;
        int p_jesdCnt[NUM_JESD][MAX_JESD_CNT];
        int lastATCACommonParam;
#define LAST_ATCACOMMON_PARAM   lastATCACommonParam
};

#define NUM_ATCACOMMON_DET_PARAMS ((int)(&LAST_ATCACOMMON_PARAM - &FIRST_ATCACOMMON_PARAM-1))

#define UPTIMECNT_STR              "upTimeCnt"
#define BUILDSTAMP_STR             "buildStamp"
#define FPGAVERSION_STR            "fpgaVersion"
#define ETH_UPTIMECNT_STR          "EthUpTimeCnt"
#define JESDCNT_RESET_STR          "jesdCntReset"
#define JESDCNT_MODE_STR           "jesdCntMode"
#define JESDCNT_STR                "jesdCnt_%d_%d"




typedef struct {
    ELLNODE                 node;
    char                    *portName;
    char                    *pathName;
    ATCACommonAsynDriver    *pDrv;
}  drvNode_t;


#endif /* ATCA_COMMON_ASYN_DRIVER_H */

