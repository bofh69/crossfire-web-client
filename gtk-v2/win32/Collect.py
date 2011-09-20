#for i in [LIBCURL,GTK,LIBXML,LIBGLADE,LIBPNG]:
#	print i
LIBCURL=LIBCURL.replace('C:\\','/c/').replace('\\','/').replace('//','/')
GTK=GTK.replace('C:\\','/c/').replace('\\','/').replace('//','/')
LIBXML=LIBXML.replace('C:\\','/c/').replace('\\','/').replace('//','/')
LIBGLADE=LIBGLADE.replace('C:\\','/c/').replace('\\','/').replace('//','/')
LIBPNG=LIBPNG.replace('C:\\','/c/').replace('\\','/').replace('//','/')
ZLIB=ZLIB.replace('C:\\','/c/').replace('\\','/').replace('//','/')
MINGWBIN=MINGWBIN.replace('C:\\','/c/').replace('\\','/').replace('//','/')
MINGW=MINGW.replace('C:\\','/c/').replace('\\','/').replace('//','/')



DIST=pwd+'/Dist'
system('mkdir Dist')
os.chdir('client.svn/gtk-v2/src')

system('cp crossfire-client-gtk2.exe %s/Dist' %pwd)
system('cp -r ../glade %s/glade-gtk2' %DIST)
system('cp %s/lib/.libs/libcurl-4.dll %s' %(LIBCURL, DIST))
system('cp %s/zlib1.dll %s' %(ZLIB, DIST))
system('cp %s/glade/.libs/libglade-2.0-0.dll %s' %(LIBGLADE,DIST))
system('cp %s/libpthread-2.dll %s' %(MINGWBIN, DIST))
system('cp %s/libiconv-2.dll %s' %(MINGWBIN, DIST))
system('cp %s/.libs/libxml2-2.dll %s' %(LIBXML, DIST))

for i in ['libgdk-win32-2.0-0.dll', 'libpng14-14.dll', 'libcairo-2.dll', 'libfontconfig-1.dll', 'libexpat-1.dll', 'freetype6.dll', 'libgdk_pixbuf-2.0-0.dll', 'intl.dll', 'libgio-2.0-0.dll', 'libglib-2.0-0.dll','libgmodule-2.0-0.dll','libgobject-2.0-0.dll','libgthread-2.0-0.dll', 'libpango-1.0-0.dll','libpangocairo-1.0-0.dll', 'libpangoft2-1.0-0.dll', 'libpangowin32-1.0-0.dll', 'libgtk-win32-2.0-0.dll', 'libatk-1.0-0.dll']:
	system('cp %s/%s %s' %(GTK, i, DIST))

system('rm -rf glade-gtk2/.svn')