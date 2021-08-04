#include <stdio.h>

#include <iocsh.h>
#include <epicsExport.h>

extern "C" {

/* IOC shell function.
 * Inputs:
 *      1 - Action to execute when firmware is wrong:
 *          a - Warning message on screen
 *          b - Warning message plus quit the IOC
 *      2 - List of firmware versions to check against
 */

static const iocshArg atcaFirmVersionArg0 = { "Hang IOC? Y/N", iocshArgString };
static const iocshArg atcaFirmVersionArg1 = { "List of accepted firmware versions (strings)", iocshArgArgv };
static const iocshArg* const atcaFirmVersionArgs [] = { &atcaFirmVersionArg0,
                                                        &atcaFirmVersionArg1 };
static const iocshFuncDef atcaFirmVersionFuncDef = {"atcaCheckFirmwareVersion", 2, atcaFirmVersionArgs};
static void  atcaFirmVersionCallFunc(const iocshArgBuf *args)
{

printf("atcaFirmVersionCallFunc");
    //crossbarControl((const char *) args[0].sval, (const char *) args[1].sval,
   //                 (const char *)(args[2].sval && strlen(args[2].sval))?args[2].sval: NULL);
} 

void versionComparatorDriverRegister(void)
{
    iocshRegister(&atcaFirmVersionFuncDef, atcaFirmVersionCallFunc);
}

epicsExportRegistrar(versionComparatorDriverRegister);


} /*extern C */
