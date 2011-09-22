import os, sys, subprocess
import urllib2 as u
pwd=os.getcwd()
def system(cmd):
	return subprocess.Popen(cmd.split(),env=os.environ,stdin=sys.stdin,stdout=sys.stdout,stderr=sys.stderr).wait()
def AskInstall(Name):
	return Auto or 'Y' in raw_input('Install %s?' %Name).upper()
Auto='auto' in sys.argv

def FetchFile(URL, Filename):
	f=u.urlopen(URL)
	o=open(Filename,'wb')
	txt=f.read(1024*512)
	ctr=0
	while txt:
		if (ctr+1)/2*2==ctr+1:
			print ctr*1024*512+len(txt), 'bytes downloaded'
		o.write(txt)	
		txt=f.read()
		ctr+=1
	f.close()
	o.close()


#FetchFile('https://crossfire.svn.sourceforge.net/svnroot/crossfire/client/trunk/gtk-v2/win32/gtkclient.nsi', 'gtkclient.nsi')
system('cp %s/gtk-2/win32/gtkclient.nsi.in .' %CFSOURCE)

if not DB and AskInstall('Nsis'):
	FetchFile('http://downloads.sourceforge.net/project/nsis/NSIS%202/2.46/nsis-2.46-setup.exe?r=http%3A%2F%2Fnsis.sourceforge.net%2FDownload&ts=1316497068&use_mirror=superb-sea2', 'nsis.exe')
	system('nsis.exe')

NSIS=pwd+'\NSIS' if DB else raw_input('Path to nsis?') or pwd+'\NSIS'
NSIS+='\makensis.exe '
if 'CFSOURCE' not in dir():
	CFSOURCE=raw_input("Path to client.svn? ")
f=open('gtkclient.nsi.in')
txt=f.read()
f.close()
txt=txt %(CFSOURCE,CFSOURCE,CFSOURCE,CFSOURCE,CFSOURCE,CFSOURCE,CFSOURCE,CFSOURCE,CFSOURCE,CFSOURCE,CFSOURCE,CFSOURCE,CFSOURCE,CFSOURCE,CFSOURCE,CFSOURCE,CFSOURCE,CFSOURCE)
f=open('gtkclient.nsi','w')
f.write(txt)
f.close()

print NSIS

system(NSIS + 'gtkclient.nsi')

