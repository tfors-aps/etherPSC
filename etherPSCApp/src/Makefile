TOP=../..

include $(TOP)/configure/CONFIG

#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

DBD += etherPSC.dbd
etherPSC_DBD += base.dbd
etherPSC_DBD += psControl.dbd
etherPSC_DBD += subPS.dbd
etherPSC_DBD += devEtherPSC.dbd

PROD = etherPSC

etherPSC_SRCS += etherPSC_registerRecordDeviceDriver.cpp
etherPSC_SRCS_DEFAULT += etherPSCMain.c

etherPSC_LIBS += etherPSC
etherPSC_LIBS += seq
etherPSC_LIBS += pv
etherPSC_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

