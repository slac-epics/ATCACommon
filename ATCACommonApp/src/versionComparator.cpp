#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <iocsh.h>
#include <epicsExport.h>
#include <epicsExit.h>

#include <yaml-cpp/yaml.h>
#include <yamlLoader.h>

#include "ATCACommon.h"
#include <cpsw_api_user.h>
#include <atcaCommon.h>

extern "C" {

/* 
 * IOC shell function.
 * Inputs:
 *      1 - y/Y or n/N for question "Stop IOC?". If the firmware is wrong and
 *          Stop IOC = y/Y, then the IOC is stopped.
 *      2 - List of firmware versions to check against gitHash and/or fpgaVersion
 */
static const iocshArg atcaFirmVersionArg0 = { "Stop IOC? Y/N", iocshArgString };
static const iocshArg atcaFirmVersionArg1 = { "List of accepted firmware versions (strings)", iocshArgArgv };
static const iocshArg* const atcaFirmVersionArgs [] = { &atcaFirmVersionArg0,
                                                        &atcaFirmVersionArg1 };
static const iocshFuncDef atcaFirmVersionFuncDef = {"atcaCheckFirmwareVersion", 2, atcaFirmVersionArgs};
static int atcaFirmVersionGuts(const iocshArgBuf *args)
{
    int c, iii;
    bool stopIOC;
    if (args[0].sval != NULL) {
        // Get only the first character of the string, that must be Y, y, N, or n.
        c =  (int) (* args[0].sval);
    } else {
        // This will make the switch statement below to print a nice message
        // when the user calls the function without a parameter
        c = 0;
    }

    // Check for Y/y or N/n to the question "Stop IOC"?
    switch (c) {
        case 89:
        case 121:
            stopIOC = true;
            break;
        case 78:
        case 110:
            stopIOC = false;
            break;
        default:
            printf ("atcaCheckFirmwareVersion. Please, use Y or N with the first parameter -> Stop IOC?\n");
            return -1;
    }


    char gitHash[41];
    uint32_t fpgaVersion;

    // Commands to attach to the FPGA register map in order to access the atcaCommon API
    Path p_root;
    Path p_atcaCommon;
    char* named_root = NULL;
    ATCACommonFw atcaCommon;

    p_root = (named_root && strlen(named_root))? cpswGetNamedRoot(named_root): cpswGetRoot();
    if (! p_root) {
        printf ("atcaCheckFirmwareVersion depends on the command cpswLoadYamlFile. Please, run cpswLoadYamlFile first. atcaCheckFirmwareVersion will exit now without any action.\n");
        return -1;
    }

    try {
        p_atcaCommon = p_root->findByName("mmio");
    }
    catch(const CPSWError& e) {
        printf("atcaCheckFirmwareVersion: error while checking version: %s\n", e.getInfo().c_str());
        return -1;
    }

    atcaCommon = IATCACommonFw::create(p_atcaCommon);
    atcaCommon = IATCACommonFw::create(p_atcaCommon);

    try {
        atcaCommon->getGitHash((uint8_t *) gitHash);
        atcaCommon->getFpgaVersion(&fpgaVersion);
    }
    catch(const CPSWError& e) {
        printf("atcaCheckFirmwareVersion: error while getting Git hash and FPGA version: %s\n", e.getInfo().c_str());
        return -1;
    }

    bool versionMatch = false;

    // Check if any of the firmware version strings informed matches gitHash or fpgaVersion.
    // Having one of them matching is sufficient for a success status.
    for (iii = 1; iii < args[1].aval.ac; ++iii) {
        versionMatch = (! strcmp(args[1].aval.av[iii], gitHash)) || versionMatch;

        // Check for both decimal and hexadecimal versions of the fpgaVersion parameter
        versionMatch = (strtoul(args[1].aval.av[iii], NULL, 0) == fpgaVersion) || versionMatch;
    }

    printf("\nThis is the current firmware version information:\n");
    printf("gitHash = %s\n", gitHash);
    printf("fpgaVersion in decimal = %d, fpgaVersion in hexa = 0x%x\n\n", fpgaVersion, fpgaVersion);

    if (! versionMatch) {
        printf("The firmware running in the ATCA carrier board is not compatible with the provided list of versions.\n\n");
        if (stopIOC) {
            printf("The IOC is going to be stopped. Please install the right version of the firmware.\n");
            epicsExit(0);
        }
    } else {
        printf("The firmware is compatible with the provided list of versions.\\n");
    }

    return 0;
}

static void atcaFirmVersionCallFunc(const iocshArgBuf* args)
{
    iocshSetError(atcaFirmVersionGuts(args));
}

void versionComparatorDriverRegister(void)
{
    iocshRegister(&atcaFirmVersionFuncDef, atcaFirmVersionCallFunc);
}

epicsExportRegistrar(versionComparatorDriverRegister);


} /*extern C */
