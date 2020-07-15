#ifndef   _DEBUG_STREAM_INTERFACE_H
#define   _DEBUG_STREAM_INTERFACE_H

#include <epicsTime.h>
#include <epicsTypes.h>

typedef  void (*STREAM_CALLBACK_FUNCTION)(void *pBuf, unsigned size,  epicsTimeStamp time, void *usr);


#endif   /* _DEBUG_STREAM_INTERFACE_H */
