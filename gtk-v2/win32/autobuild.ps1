# A few assumptions made in this script:
# You're running Windows 10 x64
# You've installed MSYS2 x64 into C:\msys64. MSYS2 should install MinGW64 into C:\msys64\mingw64. Use of MSYS2's installer is recommended, instead of installing from a third-party repo or tool.
# You've installed CMake for Windows
# You've installed 7-zip
# You've installed NSIS
# The MINGW "bin" folder must be in your PATH. Typically this is C:\msys64\mingw64\bin
# The CMake "bin" folder must be in your PATH. Typically this is C:\Program Files\CMake\bin
# You have a working git
# You've already 'git clone' the client and sounds repos, into the "sourcedir" and "sounddir" directories listed below
# We have write permission into the C:\autobuild folder
# Nothing major has changed since git e460f2ab73e8c7900f428b8c066c6b1a11d63622. That's the last time this script was more or less debugged.
# In MSYS' MinGW, you've installed:
#     mingw-w64-x86_64-SDL2_mixer
#     mingw-w64-x86_64-gcc
#     mingw-w64-x86_64-make
#     mingw-w64-x86_64-perl
#     mingw-w64-x86_64-pkg-config
#     mingw-w64-x86_64-vala
# (The oneliner for these is here:)
# pacman -S mingw-w64-x86_64-SDL2_mixer mingw-w64-x86_64-gcc mingw-w64-x86_64-make mingw-w64-x86_64-perl mingw-w64-x86_64-pkg-config mingw-w64-x86_64-vala


$sourcedir = "C:\autobuild\cfsource"
$releasedir = "C:\autobuild\cfrelease"
$builddir = "C:\autobuild\cfbuild"
$sounddir = "C:\autobuild\cfsounds"
$packagedir = "C:\autobuild\cfpackage"
$7zipbinary = "$env:ProgramFiles\7-Zip\7z.exe"
$nsisbinary = "$env:ProgramFiles (x86)\NSIS\makensis.exe"

# A handy function to see if something is a command, or is in our PATH. Returns a simple $true/$false.
# If we want to know *where* the executable was, then we'd use "Get-Command" by itself, which is similar to the Unix "which" command.
Function Test-CommandExists
{
 Param ($command)
 $oldPreference = $ErrorActionPreference
 $ErrorActionPreference = ‘stop’
 try {if(Get-Command $command){RETURN $true}}
 Catch {Write-Host “$command does not exist”; RETURN $false}
 Finally {$ErrorActionPreference=$oldPreference}
} #end function test-CommandExists

Write-Progress -Activity "Building Crossfire Client" -Status "Checking prereqs" -PercentComplete 0

# Before we even get started, lets run a bunch of sanity checks. Note that several items must be in your PATH.
if (!(Test-Path $sourcedir\.git\config))
    {throw "Can't confirm the source dir is a git dir."}

if (!(Test-CommandExists cmake.exe))
    {throw "cmake.exe is not in your PATH."}

if (!(Test-CommandExists mingw32-make.exe))
    {throw "mingw32-make.exe is not in your PATH."}

if (!(Test-CommandExists pkg-config.exe))
    {throw "pkg-config.exe is not in your PATH."}

if (!(Test-Path C:\msys64\mingw64\lib))
    {throw "Can't find MinGW's lib folder. Make sure it's in C:\msys64\mingw64\lib\"}

if (!(Test-Path C:\msys64\mingw64\share))
    {throw "Can't find MinGW's share folder. Make sure it's in C:\msys64\mingw64\share\"}

Write-Progress -Activity "Building Crossfire Client" -Status "Fetching latest source" -PercentComplete 5

# Update the source tree
pushd $sourcedir
git pull --quiet
$cfgitversion = (git rev-parse --short HEAD)
popd

Write-Progress -Activity "Building Crossfire Client" -Status "Running CMake" -PercentComplete 10

# Clean the build dir
If (Test-Path $builddir) {Remove-Item $builddir -recurse}
New-Item -Path $builddir -ItemType Directory | Out-Null

# run CMake. Turn off OpenGL because it's broken on Windows as of Aug 2021.
$cmakeoutput = (
    cmake.exe `
    -S C:\autobuild\cfsource -B C:\autobuild\cfbuild\ `
    -G "MinGW Makefiles" `
    -D OPENGL=OFF `
    2>&1
    )
$cmakeoutput | Out-File -FilePath C:\autobuild\cmakeoutput.txt
if ($LASTEXITCODE -eq $true) {Write-Warning -Message "The CMake process may not have exited cleanly. Check C:\autobuild\cmakeoutput.txt for more info."}

# If CMake gives an error about pkg-config or PKG_CONFIG_EXCEUTABLE,
# make sure you don't have another pkg-config in your PATH, such as
# one supplied by a third-party PERL.

# Run Make
Write-Progress -Activity "Building Crossfire Client" -Status "Running make" -PercentComplete 15

pushd $builddir

$makeoutput = (mingw32-make.exe -j 4 2>&1 ) # if you have more cores, turn up the number of jobs with the "j" flag
$makeoutput | Out-File -FilePath C:\autobuild\makeoutput.txt
if ($LASTEXITCODE -eq $true) {Write-Warning -Message "The make process may not have exited cleanly. Check C:\autobuild\makeoutput.txt for more info."}

Write-Progress -Activity "Building Crossfire Client" -Status "Running make install" -PercentComplete 30

$makeinstalloutput = (mingw32-make.exe install 2>&1)
$makeinstalloutput | Out-File -FilePath C:\autobuild\makeinstalloutput.txt
if ($LASTEXITCODE -eq $true) {Write-Warning -Message "The make install process may not have exited cleanly. Check C:\autobuild\makeinstalloutput.txt for more info."}

popd

# Get a rough count of compile warnings and errors
$makewarnings = ($makeoutput | select-string -pattern ": warning:").count
$makeerrors = ($makeoutput | select-string -pattern ": error:").count

if ($makeerrors -gt 0)
    {Write-Warning -Message "The make process had errors. The build was probably not successful."}

# We're done with the build and need to assemble all it's components into one place.

Write-Progress -Activity "Building Crossfire Client" -Status "Assembling dependencies" -PercentComplete 35

# First, a few sanity checks.

if (!(Test-Path C:\msys64\mingw64\lib))
    {throw "Can't find MinGW's lib folder. Make sure it's in C:\msys64\mingw64\lib\"}

if (!(Test-Path C:\msys64\mingw64\share))
    {throw "Can't find MinGW's share folder. Make sure it's in C:\msys64\mingw64\share\"}

if (!(Test-Path C:\msys64\mingw64\bin))
    {throw "Can't find MinGW's bin folder. Make sure it's in C:\msys64\mingw64\bin\"}

if (!(Test-Path C:\autobuild\cfbuild\share))
    {throw "'make install' didn't create a 'share' directory in $builddir. Check the content of $makeoutput and $makeinstalloutput"}

if (!(Test-Path $builddir\bin\crossfire-client-gtk2.exe))
    {throw "Can't find the Crossfire binary. It should be in $builddir\bin\crossfire-client-gtk2.exe"}

if (!(Test-CommandExists C:\msys64\usr\bin\ldd.exe))
    {throw "Can't find ldd.exe. It should be in C:\msys64\usr\bin\"}



# Clean the directory to prepare for assembling the release
If (Test-Path $releasedir) { Remove-Item $releasedir -recurse}
New-Item -Path $releasedir -ItemType Directory | Out-Null

# Pull in some CF stuff
Copy-Item -Path C:\autobuild\cfbuild\share -Destination $releasedir -Recurse

# Need a few GTK things from MinGW
New-Item -Path $releasedir\lib -ItemType Directory | Out-Null
Copy-Item -Path "C:\msys64\mingw64\lib\gtk-2.0" -Destination $releasedir\lib -Recurse
Copy-Item -Path "C:\msys64\mingw64\lib\gdk-pixbuf-2.0" -Destination $releasedir\lib -Recurse
Copy-Item -Path "C:\msys64\mingw64\share\themes" -Destination $releasedir\share -Recurse

# Make sure the sounds exist...
Write-Progress -Activity "Building Crossfire Client" -Status "Assembling sounds" -PercentComplete 40
if (Test-Path $sounddir\.git\config) {
    
    # ...before updating them...
    pushd $sounddir
    git pull --quiet
    popd

    # ...and copying them into the release folder.
    New-Item -Path "$releasedir\share\crossfire-client\sounds" -ItemType Directory | Out-Null
    Copy-Item -Path $sounddir\* -Destination $releasedir\share\crossfire-client\sounds\ -Recurse
    }
else {Write-Warning -Message "Couldn't find the sounds, so we won't copy them into the build."}

# Pull in the compiled binary. No need for a sound server binary anymore, since
# git 1c9ba67464bf845ac1cc8bf4d7fb80774756cece or SVN 21700
Write-Progress -Activity "Building Crossfire Client" -Status "Assembling binary and DLLs" -PercentComplete 45

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

# We're done with assembling the release. You should now be able to take $releasedir and run it on another system safely.
# Let's do a few sanity checks before trying to make the package.

Write-Progress -Activity "Building Crossfire Client" -Status "Checking packaging prereqs" -PercentComplete 50

if (!(Test-CommandExists $7zipbinary))
    {throw "Can't find 7-zip."}

if (!(Test-CommandExists C:\msys64\usr\bin\sha256sum.exe))
    {throw "Can't find sha256sum.exe in the MSYS bin directory."}

if (!(Test-CommandExists $nsisbinary))
    {throw "Can't find NSIS."}

# Time to make a zip package

# Create the package directory if it doesn't exist
If (!(Test-Path $packagedir)) {mkdir $packagedir}

Write-Progress -Activity "Building Crossfire Client" -Status "Building zip archive" -PercentComplete 55

# Use 7-zip to make a zip archive
If (Test-Path $packagedir\crossfire-client-git-$cfgitversion-win32_amd64.zip) {
    Write-Warning -Message "A zipfile already exists for this version, we'll delete and recreate it."
    Remove-Item $packagedir\crossfire-client-git-$cfgitversion-win32_amd64.zip
    }

Set-Alias 7z $7zipbinary
$7zoutput = (7z a -mx9 -mmt1 -r -- $packagedir\crossfire-client-git-$cfgitversion-win32_amd64.zip $releasedir)
$7zoutput | Out-File -FilePath C:\autobuild\7zoutput.txt
if ($LASTEXITCODE -eq $true) {Write-Warning -Message "The 7-zip process may not have exited cleanly. Check C:\autobuild\7zoutput.txt for more info."}

# Take the SHA256 of the zip
If (Test-Path $packagedir\crossfire-client-git-$cfgitversion-win32_amd64.sha256) {
    Write-Warning -Message "A sha file already exists for this version's zipfile, we'll delete and recreate it."
    Remove-Item $packagedir\crossfire-client-git-$cfgitversion-win32_amd64.sha256
    }

pushd $packagedir
C:\msys64\usr\bin\sha256sum.exe "crossfire-client-git-$cfgitversion-win32_amd64.zip" > $packagedir\crossfire-client-git-$cfgitversion-win32_amd64.sha256
popd

Write-Progress -Activity "Building Crossfire Client" -Status "Building NSIS installer" -PercentComplete 80

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
$nsisoutput = (nsis /NOCD /DVERSION=$nsisversion /DGITVERSION=$cfgitversion /DINPUTDIR=$releasedir /DSOURCELOCATION=$sourcedir /DOUTPUTDIR=$packagedir $sourcedir\gtk-v2\win32\client.nsi)
$nsisoutput | Out-File -FilePath C:\autobuild\nsisoutput.txt
if ($LASTEXITCODE -eq $true) {Write-Warning -Message "The NSIS process may not have exited cleanly. Check C:\autobuild\nsisoutput.txt for more info."}

# Take the SHA256 of the NSIS file.
If (Test-Path $packagedir\CrossfireClient-git-$cfgitversion.sha256) {
    Write-Warning -Message "A sha file already exists for this version's zipfile, we'll delete and recreate it."
    Remove-Item $packagedir\CrossfireClient-git-$cfgitversion.sha256
    }

pushd $packagedir
C:\msys64\usr\bin\sha256sum.exe "CrossfireClient-git-$cfgitversion.exe" > $packagedir\CrossfireClient-git-$cfgitversion.sha256
popd

Write-Progress -Activity "Building Crossfire Client" -Status "Finished!" -PercentComplete 100
Start-Sleep -Seconds 1