#ifndef CROSSBAR_CONTROL_H
#define CROSSBAR_CONTROL_H

#include <asynPortDriver.h>
#include <yamlLoader.h>

#include <crossbarControlYaml.hh>

namespace CrossbarControl {

    class CrossbarControlDriver {
        private:
            CrossbarControlYaml *pCrossbarApi;
            enum {_atca, _pcie} busType;
        public:
            CrossbarControlDriver(const char * _path_str, const char *named_root = NULL);
            void Report(void);
            void Control(const char *output_name, const char *source_name);
            CrossbarControlYaml * GetApi(void) { return pCrossbarApi; };
    };
    
    
    class CrossbarControlAsynDriver:asynPortDriver {
        private:
             CrossbarControlDriver  *pDrv;
        public:
            CrossbarControlAsynDriver(const char * portName, CrossbarControlDriver *pDrv);
            asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
            
        protected:
        //
        // parameter section for asynPortDriver
        //
#if (ASYN_VERSION <<8 | ASYN_REVISION) < (4<<8 | 32)
        int firstCrossbarControlParam;
#define FIRST_CROSSBAR_CONTROL_PARAM      firstCrossbarControlParam
#endif /* asyn version check, under 4.32 */
        int p_outputConfig[4];      /* asynInt32, rw */
#if (ASYN_VERSION <<8 | ASYN_REVISION) < (4<<8 | 32)
        int lastCrossbarControlParam;
#define LAST_CROSSBAR_CONTROL_PARAM      lastCrossbarControlParam
#endif /* asyn version check, under 4.32 */        
        
    };


};  /* namespace CrossbarControl */

#if (ASYN_VERSION <<8 | ASYN_REVISION) < (4<<8 | 32)
#define NUM_CROSSBAR_CONTROL_DET_PARAMS ((int)(&LAST_CROSSBAR_CONTROL_PARAM - &FIRST_CROSSBAR_CONTROL_PARAM -1))
#endif /* asyn version check, under 4.32 */

#define outputConfigString    "output%d"

#endif /* CROSSBAR_CONTROL_H */
