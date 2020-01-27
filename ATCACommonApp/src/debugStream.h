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

typedef enum {
   uint32 = 0,
   int32,
   uint16,
   int16
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
        void streamPoll(void);
        void report(int interest);
    private:
        void parameterSetup(void);

        char *named_root;
        char *port;
        unsigned rdCnt;
        unsigned rdLen[4];
        unsigned size;
        bool     header;
        epicsTimeStamp time;

        Stream _stream[4];
        stream_type_t  s_type[4];

        uint8_t *buff[4];

    protected:
#if (ASYN_VERSION <<8 | ASYN_REVISION) < (4<<8 | 32)
        int firstDebugStreamParam;
#define FIRST_DEBUGSTREAM_PARAM   firstDebugStreamParam
#endif /* ASYN VERSION CHECK, under 4.32 */

        int p_stream[4];


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
    epicsInt32       payload;    
}  stream_with_header_t;

typedef struct {
    epicsInt32       payload;
}  stream_without_header_t;

#pragma pack(pop)

#define   STREAM_STR    "stream_%d"

#endif /* DEBUG_STREAM_ASYN_DRIVER_H */
