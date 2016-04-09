#!../../bin/linux-x86_64/etherPSC

# iocpsgendev linux startup file 
< envPaths
cd ${TOP}

dbLoadDatabase("dbd/etherPSC.dbd")
etherPSC_registerRecordDeviceDriver(pdbbase)

dbLoadRecords("db/etherPSCDev.db")

# Load IOC CPU monitoring
# -----------------------
#dbLoadRecords("db/iocAdminSoft.db", "IOC=SOFT-IOCPS")

iocInit()

# Start sequences
seq(&psControl,"ps=118-PSD5")
epicsThreadSleep(1)
seq(&psControl,"ps=118-PSD6")

