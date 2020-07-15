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

#include "debugStreamInterface.h"

typedef enum {
   uint32 = 0,
   int32,
   uint16,
   int16,
   float32,
   float64
} stream_type_t;


class DebugStreamAsynDriver:asynPortDriver {
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
        int  registerCallback(const int ch, const void *cb_func, const void *cb_usr);
        void streamPoll(const int ch);
        void report(int interest);
        asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    private:
        void parameterSetup(void);

        char *named_root;
        char *port;
        unsigned timeoutCnt;
        unsigned timeoutCnt_perStream[4];
        unsigned rdCnt;
        unsigned rdCnt_perStream[4];
        unsigned rdLen[4];
        unsigned size;
        bool     header;
        epicsTimeStamp time;

        STREAM_CALLBACK_FUNCTION  cb_func[4];
        void *cb_usr[4];

        Stream _stream[4];
        stream_type_t  s_type[4];

        uint8_t *buff[4];

    protected:
#if (ASYN_VERSION <<8 | ASYN_REVISION) < (4<<8 | 32)
        int firstDebugStreamParam;
#define FIRST_DEBUGSTREAM_PARAM   firstDebugStreamParam
#endif /* ASYN VERSION CHECK, under 4.32 */

        int p_streamInt16[4];
        int p_streamInt32[4];
        int p_streamFloat32[4];
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
} debugStreamNode_t;

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
