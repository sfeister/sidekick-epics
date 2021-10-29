exec O.$EPICS_HOST_ARCH/streamApp $0
dbLoadDatabase "O.Common/streamApp.dbd"
streamApp_registerRecordDeviceDriver


epicsEnvSet "STREAM_PROTOCOL_PATH","."

drvAsynSerialPortConfigure("led_ino","/dev/ttyUSB0")
asynSetOption("led_ino",0,"baud","9600")
asynSetOption("led_ino",0,"bits","8")
asynSetOption("led_ino",0,"parity","none")
asynSetOption("led_ino",0,"stop","1")
asynSetOption("led_ino",0,"clocal","Y")
asynSetOption("led_ino",0,"crtscts","N")

#drvAsynIPPortConfigure "LO","localhost:40000"
#vxi11Configure "LO","192.168.1.236",0,0.0,"gpin0"

dbLoadRecords "leds.db"

#log debug output to file
#streamSetLogfile StreamDebug.log

iocInit

#var streamDebug 1
