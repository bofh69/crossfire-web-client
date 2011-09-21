import time, os

while 1:
	midnight=list(time.localtime())
	midnight[4]=midnight[5]=midnight[3]=0
	
	midnight=time.mktime(midnight)+60*60*24
	print midnight-time.time()
	time.sleep(midnight-time.time())
	if not os.system('c:\python27\python.exe CFInstall.bat db auto'):
		os.system('scp *.exe 192.168.0.43:/home/perkins/svn/Perkins/CFCompiler/')
	
