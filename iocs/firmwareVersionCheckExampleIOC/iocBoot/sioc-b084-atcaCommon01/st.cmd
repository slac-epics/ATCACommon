#!../../bin/linuxRT-x86_64/ATCACommonExample

< envPaths

epicsEnvSet("FPGA_IP","10.0.1.105")

# YAML directory
epicsEnvSet("YAML_DIR","${IOC_DATA}/${IOC}/yaml")
epicsEnvSet("TOP_YAML","${YAML_DIR}/000TopLevel.yaml")

## Register all support components
dbLoadDatabase("../../dbd/ATCACommonExample.dbd",0,0)
ATCACommonExample_registerRecordDeviceDriver(pdbbase) 

## Yaml Downloader
DownloadYamlFile("${FPGA_IP}", "${YAML_DIR}")

cpswLoadYamlFile("${TOP_YAML}", "NetIODev", "", "${FPGA_IP}")

iocInit()

# atcaCheckFirmwareVersion [Stop IOC? Y/N] [any number of desired gitHash or fpgaVersion, each item between quotes]
# If using the fpgaVersion in hexadecimal format, 0x is required in the beginning of the number. In the example
# below, "50528256" decimal is equal to "0x3030000", so the same result would be obtained with either one or the other.
atcaCheckFirmwareVersion "Y" "879c14487f9f78a7f369190b260f2ea295f106d6" "50528256" "0x3030000"
