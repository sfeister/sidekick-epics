exec O.$EPICS_HOST_ARCH/streamApp $0
dbLoadDatabase "O.Common/streamApp.dbd"
streamApp_registerRecordDeviceDriver


epicsEnvSet "STREAM_PROTOCOL_PATH","."

drvAsynSerialPortConfigure("shutter_ino","/dev/ttyUSB0")
asynSetOption("shutter_ino",0,"baud","9600")
asynSetOption("shutter_ino",0,"bits","8")
asynSetOption("shutter_ino",0,"parity","none")
asynSetOption("shutter_ino",0,"stop","1")
asynSetOption("shutter_ino",0,"clocal","Y")
asynSetOption("shutter_ino",0,"crtscts","N")

#drvAsynIPPortConfigure "LO","localhost:40000"
#vxi11Configure "LO","192.168.1.236",0,0.0,"gpin0"

dbLoadRecords "shutter.db"

#log debug output to file
#streamSetLogfile StreamDebug.log

iocInit

#var streamDebug 1
