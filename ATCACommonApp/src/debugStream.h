#ifndef DEBUG_STREAM_ASYN_DRIVER_H
#define DEBUG_STREAM_ASYN_DRIVER_H

#include <asynPortDriver.h>
#include <epicsEvent.h>
#include <epicsTypes.h>
#include <epicsTime.h>

#include <cpsw_api_user.h>

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

#define DAQMUX_SAMPLES     4096UL
#define DAQMUX_HEADER      14UL

#define HEADER_EN_STRING   "header_enabled"

#include "debugStreamInterface.h"

typedef enum {
   uint32 = 0,
   int32,
   uint16,
   int16,
   float32,
   float64,
   undefined
} stream_type_t;

typedef enum {
   cfg_default = 0,
   cfg_advanced
} scope_cfg_type_t;

typedef enum {
   scope_d32 = 0,
   scope_d16 = 1
} scope_dwidth_t;

typedef enum {
   scope_dunsigned = 0,
   scope_dsigned = 1
} scope_dsign_t;

struct dumpStreamInfo_t {
    int remainingPackets;
    int wordQty;
};

class DebugStreamAsynDriver: public asynPortDriver {
    public:
        DebugStreamAsynDriver(const char *portName,
                              const char *named_root,
                              const unsigned size,
                              const bool header,
                              const char *stream0 = NULL,
                              const char *stream1 = NULL,
                              const char *stream2 = NULL,
                              const char *stream3 = NULL);
        ~DebugStreamAsynDriver();
        int  registerCallback(const int ch, STREAM_CALLBACK_FUNCTION cb_func, const void *cb_usr);
        void streamPoll(const int ch);
        void report(int interest);
        asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
        bool isChannelValid(int ch);
        void dumpStreamContents(int ch, int wordQty, int packQty);
        bool hasHeader();
        void setScopeIndex(int);
        int  getScopeIndex(void);
        stream_type_t getChannelTypeEnum(const char *type);
        int  setChannelType(stream_type_t type, int index);        
    private:
        char *named_root;
        char *port;
        void parameterSetup(void);
        ELLLIST* callback_list;
        int8_t scopeIndex;

    protected:
        unsigned rdLen[4];
        uint8_t *buff[4];
        double  *doubleBuff[4];
        unsigned size;
        unsigned timeoutCnt;
        unsigned timeoutCnt_perStream[4];
        unsigned rdCnt;
        unsigned rdCnt_perStream[4];
        bool     header;
        epicsTimeStamp time;

        // Call all registered functions
        int triggerCallbacks(int ch, void *pBuf, unsigned size,  epicsTimeStamp time, int timeslot);
        //STREAM_CALLBACK_FUNCTION  cb_func[4];
        //void *cb_usr[4];

        Stream _stream[4];
        stream_type_t  s_type[4];
        dumpStreamInfo_t dumpStreamInfo[MAX_WAVEFORMENGINE_CHN_CNT];
#if (ASYN_VERSION <<8 | ASYN_REVISION) < (4<<8 | 32)
        int firstDebugStreamParam;
#define FIRST_DEBUGSTREAM_PARAM   firstDebugStreamParam
#endif /* ASYN VERSION CHECK, under 4.32 */

        int p_streamFloat64[4];
        int p_streamType[4];
        int p_rdCnt[4];


        int lastDebugStreamParam;
#if (ASYN_VERSION <<8 | ASYN_REVISION) < (4<<8 | 32)
#define LAST_DEBUGSTREAM_PARAM    lastDebugStreamParam
#endif /* ASYN VERSION CHECK, under 4.32 */
};

#if (ASYN_VERSION <<8 | ASYN_REVISION) < (4<<8 | 32)
#define NUM_DEBUGSTREAM_DET_PARAMS ((int)(&LAST_DEBUGSTREAM_PARAM - &FIRST_DEBUGSTREAM_PARAM-1))
#endif /* asyn version check, under 4.32 */

typedef struct {
    char *portName;
    char *streamNames[4];
    DebugStreamAsynDriver *pDrv;
    unsigned sizeInBytes;
    unsigned scopeIndex;
} debugStreamNode_t;

typedef struct {
    char          *name;
    int            ch;
    bool           stopLoop;
    epicsEventId   shutdownEvent;
    DebugStreamAsynDriver *pDrv;
} usrPvt_t;

void streamStop(void *u);
int createStreamThread(int ch, const char *prefix_name, void *p, int (*streamThreadFunc)(void *));
int createStreamThreads(debugStreamNode_t *p, int (*streamThreadFunc)(void *));
int searchDebugStreamDriver(const char* streamPortName, DebugStreamAsynDriver** pDrv);


extern "C" {
int debugStreamAsynDriver_createStreamThreads(void);
int debugStreamAsynDriver_report(debugStreamNode_t *p, int interest);
} /* extern C */


#pragma pack(push)
#pragma pack(1)

typedef struct {
    epicsUInt32    mod[6];
    epicsTimeStamp time;
    epicsUInt32    edefAvgDoneMask;
    epicsUInt32    edefMinorMask;
    epicsUInt32    edefMajorMask;
    epicsUInt32    edefInitMask;
} timing_header_t;

typedef struct {
    epicsUInt32    packet_size;
    epicsUInt32    trg_conf;
} packet_header_t;

typedef struct {
    timing_header_t  header;
    packet_header_t  packet;
    union {
        epicsInt16       int16;
        epicsInt32       int32;
        epicsFloat32     float32;
        epicsFloat64     float64;
    } payload;    
}  stream_with_header_t;

typedef struct {
    union {
        epicsInt16       int16;
        epicsInt32       int32;
        epicsFloat32     float32;
        epicsFloat64     float64;
    } payload;
}  stream_without_header_t;

#pragma pack(pop)

#define   STREAMINT16_STR     "streamInt16_%d"
#define   STREAMINT32_STR     "streamInt32_%d"
#define   STREAMFLOAT32_STR   "streamFloat32_%d"
#define   STREAMFLOAT64_STR   "streamFloat64_%d"
#define   STREAMTYPE_STR      "streamType_%d"
#define   READCOUNT_STR       "readCount_%d"

#endif /* DEBUG_STREAM_ASYN_DRIVER_H */
