# writeLog
minifilter ; windows driver write log with Zw and Flt

这是用minifilter实现的一个写日志的驱动，
监控"\\Device\\HarddiskVolume3\\"盘，将操作磁盘的
进程全路径记录下来，写到C:\1.log中
