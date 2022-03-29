exec O.$EPICS_HOST_ARCH/streamApp $0
dbLoadDatabase "O.Common/streamApp.dbd"
streamApp_registerRecordDeviceDriver


epicsEnvSet "STREAM_PROTOCOL_PATH","."

drvAsynSerialPortConfigure("photo_ino","/dev/ttyUSB1")
asynSetOption("photo_ino",0,"baud","9600")
asynSetOption("photo_ino",0,"bits","8")
asynSetOption("photo_ino",0,"parity","none")
asynSetOption("photo_ino",0,"stop","1")
asynSetOption("photo_ino",0,"clocal","Y")
asynSetOption("photo_ino",0,"crtscts","N")

#drvAsynIPPortConfigure "LO","localhost:40000"
#vxi11Configure "LO","192.168.1.236",0,0.0,"gpin0"

dbLoadRecords "photo.db"

#log debug output to file
#streamSetLogfile StreamDebug.log

iocInit

#var streamDebug 1
