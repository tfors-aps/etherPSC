TOP=../..

include $(TOP)/configure/CONFIG

#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

DBD += devEtherPSC.dbd
DBD += psControl.dbd
DBD += subPS.dbd

PROD_HOST   = etherPSCTest
etherPSCTest_SRCS += etherPSCTest.c
etherPSCTest_SRCS += EtherTest.c

LIBRARY_IOC = etherPSC
etherPSC_SRCS += devEtherPSC.c
etherPSC_SRCS += drvEtherPSC.c
etherPSC_SRCS += psControl.st
etherPSC_SRCS += subPS.c

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

