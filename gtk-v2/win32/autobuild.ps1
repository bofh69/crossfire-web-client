# A few assumptions made in this script:
# You're running Windows 10 x64
# You've installed MSYS2 x64 into C:\msys64
# You've installed CMake for Windows
# You've installed 7-zip
# You've installed NSIS
# You have a working git
# You've already 'git clone' the client and sounds repos, into the "sourcedir" and "sounddir" directories listed below
# We have write permission into the C:\autobuild folder
# Nothing major has changed since git e6dde587db37839cf89c63c3391efaebe418d3da. That's the last time this script was more or less debugged.
# In MSYS' MinGW, you've installed:
#     mingw-w64-x86_64-SDL_image
#     mingw-w64-x86_64-SDL_mixer
#     mingw-w64-x86_64-gcc
#     mingw-w64-x86_64-make
#     mingw-w64-x86_64-perl
#     mingw-w64-x86_64-pkg-config
#     mingw-w64-x86_64-vala
# (The oneliner for these is here:)
# pacman -S mingw-w64-x86_64-SDL_image mingw-w64-x86_64-SDL_mixer mingw-w64-x86_64-gcc mingw-w64-x86_64-make mingw-w64-x86_64-perl mingw-w64-x86_64-pkg-config mingw-w64-x86_64-vala


$sourcedir = "C:\autobuild\cfsource"
$releasedir = "C:\autobuild\cfrelease"
$builddir = "C:\autobuild\cfbuild"
$sounddir = "C:\autobuild\cfsounds"
$packagedir = "C:\autobuild\cfpackage"
$7zipbinary = "$env:ProgramFiles\7-Zip\7z.exe"
$nsisbinary = "$env:ProgramFiles (x86)\NSIS\makensis.exe"


# Check to make sure the source exists. If not, abort.
if (!(Test-Path $sourcedir\.git\config))
    {Exit}

# Update the source tree
pushd $sourcedir
git pull --quiet
$cfgitversion = (git rev-parse --short HEAD)
popd

# Clean the build dir
If (Test-Path $builddir) { Remove-Item $builddir -recurse}
mkdir $builddir

# run CMake. Turn off OpenGL because it's broken on Windows as of Aug 2021.
$cmakeoutput = (
    cmake.exe `
    -S C:\autobuild\cfsource -B C:\autobuild\cfbuild\ `
    -G "MinGW Makefiles" `
    -D OPENGL=OFF `
    2>&1
    )

# If CMake gives an error about pkg-config or PKG_CONFIG_EXCEUTABLE,
# make sure you don't have another pkg-config in your PATH, such as
# one supplied by a third--party PERL.


# Run Make
pushd $builddir
$makeoutput = (mingw32-make.exe -j 4 2>&1 ) # if you have more cores, turn up the number of jobs with the "j" flag
$makeinstalloutput = (mingw32-make.exe install 2>&1)
popd

# Get a rough count of compile warnings and errors
$makewarnings = ($makeoutput | select-string -pattern ": warning:").count
$makeerrors = ($makeoutput | select-string -pattern ": error:").count

# Clean the directory to prepare for assembling the release
If (Test-Path $releasedir) { Remove-Item $releasedir -recurse}
mkdir $releasedir

# Pull in some CF stuff
Copy-Item -Path C:\autobuild\cfbuild\share -Destination $releasedir -Recurse

# Need a few GTK things from MinGW
mkdir $releasedir\lib
Copy-Item -Path "C:\msys64\mingw64\lib\gtk-2.0" -Destination $releasedir\lib -Recurse
Copy-Item -Path "C:\msys64\mingw64\lib\gdk-pixbuf-2.0" -Destination $releasedir\lib -Recurse
Copy-Item -Path "C:\msys64\mingw64\share\themes" -Destination $releasedir\share -Recurse

# Make sure the sounds exist...
if (Test-Path $sounddir\.git\config) {
    
    # ...before updating them...
    pushd $sounddir
    git pull --quiet
    popd

    # ...and copying them into the release folder.
    mkdir "$releasedir\share\crossfire-client\sounds"
    Copy-Item -Path $sounddir\* -Destination $releasedir\share\crossfire-client\sounds\ -Recurse
    }

# Pull in the compiled binary. No need for a sound server binary anymore, since
# git 1c9ba67464bf845ac1cc8bf4d7fb80774756cece or SVN 21700
Copy-Item $builddir\bin\crossfire-client-gtk2.exe $releasedir

# call ldd to find DLLs recursively
C:\msys64\usr\bin\ldd.exe $releasedir\crossfire-client-gtk2.exe |
    
    # trim ldd's output to get the filename of each DLL. Roughly equivalent to: awk '{print $3}'
    ForEach-Object {$_.Split(' ')[2]; } |
    
    # use Regex to find DLLs that A) are being pulled from MinGW, and B) regex match on only the filename, not the path.
    Select-String -Pattern "(?<=\/mingw64\/bin\/).*" |
    
    # We can't use the full path because ldd returns Unix-style paths. Return only what the regex found, not the whole line.
    ForEach-Object {$_.Matches.Value} |
    
    # Now that we have a list of DLLs being pulled from MinGW, lets just dump them all in the release folder.
    ForEach-Object {Copy-Item C:\msys64\mingw64\bin\$_ $releasedir}

# SDL pulls a sneaky one, and doesn't load the ogg/vorbis DLLs until you actually feed it an OGG file. So, ldd doesn't know, and we gotta get them manually.

Copy-Item C:\msys64\mingw64\bin\libvorbis-0.dll $releasedir
Copy-Item C:\msys64\mingw64\bin\libvorbisfile-3.dll $releasedir
Copy-Item C:\msys64\mingw64\bin\libogg-0.dll $releasedir



# Whew, now we should have all DLLs. You should now be able to take $releasedir and run it on another system safely.

# Time to make a zip package

# Create the package directory if it doesn't exist
If (!(Test-Path $packagedir)) {mkdir $packagedir}

# Use 7-zip to make a zip archive
If (Test-Path $packagedir\crossfire-client-git-$cfgitversion-win32_amd64.zip) {
    Write-Warning -Message "A zipfile already exists for this version, we'll delete and recreate it."
    Remove-Item $packagedir\crossfire-client-git-$cfgitversion-win32_amd64.zip
    }

Set-Alias 7z $7zipbinary
7z a -mx9 -mmt1 -r -- $packagedir\crossfire-client-git-$cfgitversion-win32_amd64.zip $releasedir

# Take the SHA256 of the zip
If (Test-Path $packagedir\crossfire-client-git-$cfgitversion-win32_amd64.sha256) {
    Write-Warning -Message "A sha file already exists for this version, we'll delete and recreate it."
    Remove-Item $packagedir\crossfire-client-git-$cfgitversion-win32_amd64.sha256
    }

# SHA the zipfile
pushd $packagedir
C:\msys64\usr\bin\sha256sum.exe "crossfire-client-git-$cfgitversion-win32_amd64.zip" > $packagedir\crossfire-client-git-$cfgitversion-win32_amd64.sha256
popd

# If an NSIS package exists, delete it
If (Test-Path $packagedir\CrossfireClient-git-$cfgitversion.exe) {
    Write-Warning -Message "An NSIS installer file already exists for this version, we'll delete and recreate it."
    Remove-Item $packagedir\CrossfireClient-git-$cfgitversion.exe
    }

# Let's get the current version string from CMakeLists.txt
$version = (Get-Content $sourcedir\CMakeLists.txt |
    
    # Use Regex to find the current version string.
    Select-String -Pattern "(?<=set\(VERSION ).*(?=\))" |
    
    # Return only what the regex found, not the whole line.
    ForEach-Object {$_.Matches.Value})

# NSIS is very particular about how to format the version string. Must be numerical, and in the format x.x.x.x
$nsisversion = $version + ".0"

# Now build an NSIS package.
Set-Alias nsis $nsisbinary
$OUTDIR = $packagedir
nsis /NOCD /DVERSION=$nsisversion /DGITVERSION=$cfgitversion /DINPUTDIR=$releasedir /DSOURCELOCATION=$sourcedir /DOUTPUTDIR=$packagedir $sourcedir\gtk-v2\win32\client.nsi

# SHA the resulting file.
pushd $packagedir
C:\msys64\usr\bin\sha256sum.exe "CrossfireClient-git-$cfgitversion.exe" > $packagedir\CrossfireClient-git-$cfgitversion.sha256
popd
