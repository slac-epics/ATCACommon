TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

#----------------------------------------------------
# Create and install (or just install)
# databases, templates, substitutions like this
#
# ATCACommon
DB += ATCACommon.db
DB += crossbarCtrl.db
DB += StreamLong.db
DB += StreamUlong.db
DB += StreamShort.db
DB += StreamUshort.db
DB += StreamFloat.db
DB += StreamDouble.db
DB += StreamType.db
# Waveform manipulation
DB += StreamSize.db
DB += StreamEGU.db
DB += StreamGainControl.db

# =======================================================================

#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_TEMPLATE = <templatename>

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

