"""
echo off
echo "Please Install Python http://www.python.org/ftp/python/2.7.2/python-2.7.2.msi"
echo "If you choose a non default location for the install, please rerun this script with python CFInstall.bat"
pause
explorer http://www.python.org/ftp/python/2.7.2/python-2.7.2.msi
echo "Press a key when python has finished installing"
pause
echo on
c:\python27\python CFInstall.bat auto
exit
"""
import urllib2 as u
import os, sys, subprocess

subprocess.list2cmdline=(lambda x: ' '.join(x))
def system(cmd):
	return subprocess.Popen(cmd.split(),env=os.environ,stdin=sys.stdin,stdout=sys.stdout,stderr=sys.stderr).wait()

pwd=os.getcwd()
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

Auto='auto' in sys.argv
if Auto or 'Y' in raw_input('Install MinGW?').upper():
	FetchFile('http://downloads.sourceforge.net/project/mingw/Automated%20MinGW%20Installer/mingw-get-inst/mingw-get-inst-20110802/mingw-get-inst-20110802.exe?r=http%3A%2F%2Fsourceforge.net%2Fprojects%2Fmingw%2Ffiles%2FAutomated%2520MinGW%2520Installer%2Fmingw-get-inst%2Fmingw-get-inst-20110802%2F&ts=1316141182&use_mirror=superb-sea2', 'MinGWInstall.exe')
	system('MinGWInstall.exe')


MINGW=raw_input('Where is the MinGW install? ')
if MINGW=='':
	MINGW='C:\\MinGW'
MINGWBIN=MINGW+'\\bin'
MSYS=raw_input('Where is MSYS [%s\msys]? '%MINGW)
if MSYS=='':
	MSYS=MINGW+'\\msys'
MSYSBIN=MSYS+'\\1.0\\bin'
def addPath(path):
	os.environ['PATH']+=';'+path

addPath(MSYSBIN)
addPath(MINGWBIN)
addPath(sys.executable.strip('python.exe'))




def AskInstall(Name):
	return Auto or 'Y' in raw_input('Install %s?' %Name).upper()

if AskInstall('Unzip'):
	FetchFile('http://downloads.sourceforge.net/project/infozip/UnZip%206.x%20%28latest%29/UnZip%206.0/unzip60.tar.gz?r=http%3A%2F%2Fsourceforge.net%2Fprojects%2Finfozip%2Ffiles%2FUnZip%25206.x%2520%2528latest%2529%2FUnZip%25206.0%2F&ts=1316144927&use_mirror=superb-sea2', 'unzip.tar.gz')
	system('tar -zxf unzip.tar.gz')
	os.chdir('unzip60')
	system('make -f win32/Makefile.gcc')
	system('make -f win32/Makefile.gcc install')
	system('cp unzip.exe /bin')
	os.chdir(pwd)

if system('unzip -h'):
	UZ=raw_input("Full path to unzip executable? ")
else:
	UZ='unzip'
def unZip(File):
	system('unzip %s' %File)

if AskInstall('libcurl'):
	FetchFile('http://curl.haxx.se/download/curl-7.22.0.zip','curl-7.22.0.zip')
	unZip('curl-7.22.0.zip')
	os.chdir('curl-7.22.0')
	f=open('lib/setup.h')
	txt=f.read()
	f.close()
	txt='#define HTTP_ONLY\n'+txt
	f=open('lib/setup.h','w')
	f.write(txt)
	f.close()
	system('sh ./configure')
	system('make')
	system('make install')
	LIBCURL=pwd+r'\curl-7.22.0'	
else:
	LIBCURL=raw_input('path to libcurl? ')
	if LIBCURL=='':
		LIBCURL=pwd+r'\curl-7.22.0'

os.chdir(pwd)


if AskInstall('gtk2'):
	if system('dir gtk2bundle'):
		system('mkdir gtk2bundle')
	FetchFile('http://ftp.gnome.org/pub/gnome/binaries/win32/gtk+/2.22/gtk+-bundle_2.22.1-20101227_win32.zip', 'gtk2bundle/gtk2bundle.zip')
	os.chdir('gtk2bundle')
	unZip('gtk2bundle.zip')
	os.chdir(pwd)
	addPath(pwd+r'\gtk2bundle\bin')
else:
	GTK=raw_input('path to gtk library? ')
	if GTK:
		addPath(GTK)
	else:
		addPath(pwd+r'\gtk2bundle\bin')
		GTK=pwd+r'\gtk2bundle\bin'


if AskInstall('libxml'):
	FetchFile('ftp://xmlsoft.org/libxml2/libxml2-git-snapshot.tar.gz', 'libxml2.tar.gz')
	system('tar -zxf libxml2.tar.gz')
	os.chdir('libxml2-2.7.8')
	system('sh ./configure')
	system('make')
	system('make install')
	system('cp libxml.h %s\include'%MINGW)
	LIBXML=pwd+r'\libxml2-2.7.8'

else:
	LIBXML=raw_input('path to libxml library? ')
	if LIBXML =='':
		LIBXML=pwd+r'\libxml2-2.7.8'
os.environ.update({'PKG_CONFIG_PATH':LIBXML})

os.chdir(pwd)


if AskInstall('libglade'):
	FetchFile('ftp://ftp.gnome.org/mirror/gnome.org/sources/libglade/2.6/libglade-2.6.4.tar.gz','libglade.tar.gz')
	system('tar -zxf libglade.tar.gz')
	os.chdir('libglade-2.6.4')
	system('sh configure')
	system('make')
	system('make install')
	LIBGLADE=(pwd+r'\libglade-2.6.4').replace('\\','/')
else:
	LIBGLADE=raw_input('path to libglade library? ')
	if LIBGLADE=='':
		LIBGLADE=pwd+r'\libglade-2.6.4'
LIBGLADE=LIBGLADE.replace('\\','/')
os.environ['PKG_CONFIG_PATH']+=';'+LIBGLADE
#os.environ['LIBGLADE_CFLAGS']=LIBGLADE
#os.environ['LIBGLADE_LIBS']=LIBGLADE+r'/glade'
os.chdir(pwd)

if AskInstall('zlib'):
	FetchFile('http://zlib.net/zlib125.zip','zlib.zip')
	unZip('zlib.zip')
	os.chdir('zlib-1.2.5')
	system('make -f win32/Makefile.gcc')
	system('cp zlib1.dll %s'%MINGWBIN)
	system('cp zconf.h %s'%MINGW+'\\include')
	system('cp zlib.h %s' %MINGW+'\\include')
	system('cp libz.a %s'%MINGW+r'\lib')
	system('cp libzdll.a %s'%MINGW+r'\lib\libz.dll.a')
	ZLIB=pwd+'/zlib-1.2.5'
else:
	ZLIB=raw_input("Path to zlib? ") or pwd+'/zlib-1.2.5'

os.chdir(pwd)
LIBPNG=''
if AskInstall('libpng'):
	FetchFile('http://downloads.sourceforge.net/project/libpng/libpng14/1.4.8/lpng148.zip?r=http%3A%2F%2Fsourceforge.net%2Fprojects%2Flibpng%2Ffiles%2Flibpng14%2F1.4.8%2F&ts=1316306163&use_mirror=superb-sea2','libpng.zip')
#	system('tar -zxf libpng.tar.gz')
	unZip('libpng.zip')
	os.chdir('lpng148')

#	system('sh configure')
	system('make -f scripts/makefile.gcc')
	system('make -f scripts/makefile.gcc install')
	LIBPNG=pwd+r'\lpng148'
if not LIBPNG:
	LIBPNG=raw_input("location of libpng?")

if not LIBPNG:
	LIBPNG=pwd+r'\lpng148'

os.chdir(pwd)

CFSOURCE=''
SvnType=raw_input('Please select svn client [S]vn, [p]ysvn, [t]ortoiseSVN, [m]anual checkout:')
if not 'M' in SvnType.upper():
	if AskInstall('svn client'):
		if 'S' in SvnType:
			FetchFile('http://downloads.sourceforge.net/project/win32svn/1.6.17/Setup-Subversion-1.6.17.msi?r=http%3A%2F%2Fsourceforge.net%2Fprojects%2Fwin32svn%2F&ts=1316154613&use_mirror=superb-sea2','Subversion.msi')
			system('Subversion.msi')
		elif 'p' in SvnType:
			FetchFile('http://pysvn.tigris.org/files/documents/1233/48847/py27-pysvn-svn1615-1.7.5-1360.exe','pysvn.exe')
			system('pysvn.exe')
		elif 't' in SvnType:
			FetchFile('http://downloads.sourceforge.net/tortoisesvn/TortoiseSVN-1.6.16.21511-win32-svn-1.6.17.msi?download','tsvn.msi')
			system('tsvn.msi')


	if AskInstall('cf client gtk2 code'):
		if 'S' in SvnType:
			system('svn co https://crossfire.svn.sourceforge.net/svnroot/crossfire/client/trunk client.svn')
		elif 'p' in SvnType:
			import pysvn
			pysvn.Client().checkout('http://crossfire.svn.sourceforge.net/svnroot/crossfire/client/trunk', 'client.svn')
		elif 't' in SvnType:
			print 'Please check out http://crossfire.svn.sourceforge.net/svnroot/crossfire/client/trunk into client.svn'
		CFSOURCE=pwd+r'\client.svn'
	
if not CFSOURCE:
	CFSOURCE=raw_input("Path to client.svn?") or os.path.join(pwd,'client.svn')
os.chdir(CFSOURCE or 'client.svn')
#
if 'configure' not in os.listdir('.'):
	system('sh autogen.sh')

t=subprocess.Popen('pkg-config --cflags gtk+-2.0',stdout=subprocess.PIPE)
t.wait()
GTKCF=t.stdout.read()
GTKCF=GTKCF.replace('C:','/C').replace('\\','/')
t=subprocess.Popen('pkg-config --cflags libglade-2.0',stdout=subprocess.PIPE)
t.wait()
GLADECF=t.stdout.read()
GLADECF=GLADECF.replace('C:','/C').replace('\\','/').replace('(top_builddir/)//','')
t=subprocess.Popen('pkg-config --libs libglade-2.0',stdout=subprocess.PIPE)
t.wait()
GLADELD=t.stdout.read()
GLADELD=GLADECF.replace('C:','/C').replace('\\','/').replace('(top_builddir/)//','').replace(r'(top_builddir)\c;c:','/c/').replace(r'glade\libglade-2.0.1a','glade/.libs').replace('\\','/').replace('//','/')

GLADELD=''
print '''sh -c './configure  --with-includes="%s %s -I %s" --with-ldflags="-L %s -L %s %s -L %s"' '''%(GLADECF, GTKCF, ('/'+LIBCURL+r'\include').replace('\\','/').replace(':',''),LIBCURL.replace('\\','/').replace('c:/','/c/').replace('//','/')+'lib/.libs', ('/'+LIBPNG).replace('\\','/').replace(':',''), GLADELD, LIBXML.replace('\\','/').replace(':','')+'/.libs')
if raw_input('') or Auto:

  if not system('''sh -c './configure  --with-includes="%s %s -I %s" --with-ldflags="-L %s -L %s %s -L %s -L %s"' '''%(GLADECF, GTKCF, ('/'+LIBCURL+r'\include').replace('\\','/').replace(':',''),LIBCURL.replace('\\','/').replace('c:/','/c/').replace('//','/')+'lib/.libs', ('/'+LIBPNG).replace('\\','/').replace(':',''), GLADELD, '/'+LIBXML.replace('\\','/').replace(':','')+'/.libs', ('/'+LIBCURL+r'/lib/.libs').replace('\\','/').replace(':',''))):
	f=open('gtk-v2/src/Makefile')
	txt=f.readlines()
	t=''
	for i in txt:
		if i.startswith('crossfire_client_gtk2_LDADD '):
			i=i.replace(r'\$\(top_builddir\)/C:','/c').replace('/glade/libglade-2.0.la','/glade/.libs/libglade-2.0-0.dll -l curl')
		t+=i
	f.close()
	f=open('gtk-v2/src/Makefile','w')
	f.write(t)
	f.close()
	f=open('common/config.h')
	txt=f.read()
	f.close()
	txt='#define MINGW\n'+txt
	f=open('common/config.h','w')
	f.write(txt)
	f.close()
	if not system('make'):
		system('make install')




if Auto or 'Y' in raw_input('Prepare distro? ').upper():
	os.chdir(pwd)
	execfile('Collect.py')
os.chdir(pwd)
if Auto or 'Y' in raw_input('Prepare installer? ').upper():
	execfile('Bundle.py')

if system('sh'):
	print MSYSBIN
	system(MSYSBIN+'\sh.exe')
