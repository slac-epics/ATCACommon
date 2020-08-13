#ifndef   _DEBUG_STREAM_INTERFACE_H
#define   _DEBUG_STREAM_INTERFACE_H

#include <epicsTime.h>
#include <epicsTypes.h>

typedef  void (*STREAM_CALLBACK_FUNCTION)(void *pBuf, unsigned size,  epicsTimeStamp time, int timeslot, void *usr);
typedef struct {
    ELLNODE                     node;
    STREAM_CALLBACK_FUNCTION    cb_func [ MAX_WAVEFORMENGINE_CHN_CNT ];
    void*                       cb_usr  [ MAX_WAVEFORMENGINE_CHN_CNT ];
} callback_node_t;

#ifdef __cplusplus
extern "C" {
#endif

int registerStreamCallback(const char *portName, const int stream_channel, STREAM_CALLBACK_FUNCTION cb_func, void *cb_usr);

#ifdef __cplusplus
}
#endif

#endif   /* _DEBUG_STREAM_INTERFACE_H */
