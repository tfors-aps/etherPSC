# Makefile

TOP=..

include $(TOP)/configure/CONFIG

# Set the following to NO to disable consistency checking of
# the support applications defined in $(TOP)/configure/RELEASE
# CHECK_RELEASE = YES
# T. Straumann / H. Rarback, 2002/4/1,
# checkRelease.pl sucks, i.e. doesn't reduce paths to
# canonical representation.
#CHECK_RELEASE = NO
#TS, 10/4/2002; reenabled CHECK_RELEASE - using absolute paths to define EPICS_BASE
CHECK_RELEASE=YES

TARGETS = $(CONFIG_TARGETS)
CONFIGS += $(subst ../,,$(wildcard $(CONFIG_INSTALLS)))

include $(TOP)/configure/RULES

