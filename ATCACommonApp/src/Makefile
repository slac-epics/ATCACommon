TOP=../..

include $(TOP)/configure/CONFIG

USR_CXXFLAGS += -DHAVE_YAML

#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#========================================

INCLUDES += $(addprefix -I,$(YAML_INCLUDE))
INCLUDES += $(addprefix -I,$(BOOST_INCLUDE))

## this part is just for yaml loader driver
INCLUDES += $(addprefix -I,$(CPSW_FRAMEWORK_INCLUDE))

INCLUDES += $(addprefix -I,$(COMMONATCA_INCLUDE))

INC += ATCACommon.h

DBD += ATCACommon.dbd
DBD += crossbarControl.dbd


LIBRARY_IOC += ATCACommon

ATCACommon_DBD  += ATCACommon.dbd
ATCACommon_DBD  += crossbarControl.dbd
ATCACommon_SRCS += ATCACommon.cpp
ATCACommon_SRCS += debugStream.cpp
ATCACommon_SRCS += crossbarControl.cpp


cpsw_DIR = $(CPSW_FRAMEWORK_LIB)
commonATCA_DIR = $(COMMONATCA_LIB)

ATCACommon_LIBS += cpsw
ATCACommon_LIBS += commonATCA
ATCACommon_LIBS += asyn
ATCACommon_LIBS += yamlLoader
ATCACommon_LIBS += $(EPICS_BASE_IOC_LIBS)


#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
#
