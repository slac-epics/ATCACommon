#!../../bin/linuxRT-x86_64/sioc-b084-atcaCommon01

#- You may have to change sioc-b084-atcaCommon01 to something else
#- everywhere it appears in this file

#< envPaths

## Register all support components
dbLoadDatabase("../../dbd/sioc-b084-atcaCommon01.dbd",0,0)
sioc_b084_atcaCommon01_registerRecordDeviceDriver(pdbbase) 

## Load record instances
dbLoadRecords("../../db/sioc-b084-atcaCommon01.db","user=marcio")

iocInit()

## Start any sequence programs
#seq sncsioc-b084-atcaCommon01,"user=marcio"
