TOP=../..

include $(TOP)/configure/CONFIG

USR_CXXFLAGS += -DHAVE_YAML

#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================
INCLUDES += $(addprefix -I,$(BOOST_INCLUDE))
INCLUDES += $(addprefix -I,$(CPSW_FRAMEWORK_INCLUDE))
INCLUDES += $(addprefix -I,$(YAML_INCLUDE))
INCLUDES += $(addprefix -I,$(CPSW_DEVICE_LIBRARY_INCLUDE))
INCLUDES += $(addprefix -I,$(YAMLREADER_INCLUDE))
INCLUDES += $(addprefix -I,$(COMMONATCA_INCLUDE))

#=============================
# Build the IOC application

PROD_IOC = ATCACommonExample
# ATCACommonExample.dbd will be created and installed
DBD += ATCACommonExample.dbd

# ATCACommonExample.dbd will be made up from these files:
ATCACommonExample_DBD += base.dbd

# Include dbd files from all support applications:
ATCACommonExample_DBD += system.dbd
ATCACommonExample_DBD += asyn.dbd
ATCACommonExample_DBD += yamlLoader.dbd
ATCACommonExample_DBD += yamlDownloader.dbd
ATCACommonExample_DBD += ATCACommon.dbd

# Add all the support libraries needed by this IOC
ATCACommonExample_LIBS += yamlDownloader
ATCACommonExample_LIBS += yamlLoader
#ATCACommonExample_LIBS += crossbarControl
ATCACommonExample_LIBS += ATCACommon
ATCACommonExample_LIBS += asyn

yamlReader_DIR = $(YAMLREADER_LIB)
ATCACommonExample_LIBS += yamlReader

deviceLib_DIR = $(CPSW_DEVICE_LIBRARY_LIB)
ATCACommonExample_LIBS += deviceLib

cpsw_DIR = $(CPSW_FRAMEWORK_LIB)
ATCACommonExample_LIBS += cpsw

yaml-cpp_DIR  += $(YAML_LIB)
ATCACommonExample_LIBS += yaml-cpp

commonATCA_DIR = $(COMMONATCA_LIB)
ATCACommonExample_LIBS += commonATCA

# ATCACommonExample_registerRecordDeviceDriver.cpp derives from ATCACommonExample.dbd
ATCACommonExample_SRCS += ATCACommonExample_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
ATCACommonExample_SRCS_DEFAULT += ATCACommonExampleMain.cpp
ATCACommonExample_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#ATCACommonExample_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
ATCACommonExample_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

