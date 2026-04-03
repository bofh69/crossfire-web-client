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
# Nothing major has changed since git bae2a4a7ba2ad5454920ad6f7153a0b8063cc24c. That's the last time this script was more or less debugged.
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

# If you modify this script, and can no longer run it due to an invalid signature, try removing everything below this line, to remove the signature entirely.


# SIG # Begin signature block
# MIImqQYJKoZIhvcNAQcCoIImmjCCJpYCAQExCzAJBgUrDgMCGgUAMGkGCisGAQQB
# gjcCAQSgWzBZMDQGCisGAQQBgjcCAR4wJgIDAQAABBAfzDtgWUsITrck0sYpfvNR
# AgEAAgEAAgEAAgEAAgEAMCEwCQYFKw4DAhoFAAQUCVEIH86yt61oRy6EdCFlVCnv
# Bx6ggh+7MIIFbzCCBFegAwIBAgIQSPyTtGBVlI02p8mKidaUFjANBgkqhkiG9w0B
# AQwFADB7MQswCQYDVQQGEwJHQjEbMBkGA1UECAwSR3JlYXRlciBNYW5jaGVzdGVy
# MRAwDgYDVQQHDAdTYWxmb3JkMRowGAYDVQQKDBFDb21vZG8gQ0EgTGltaXRlZDEh
# MB8GA1UEAwwYQUFBIENlcnRpZmljYXRlIFNlcnZpY2VzMB4XDTIxMDUyNTAwMDAw
# MFoXDTI4MTIzMTIzNTk1OVowVjELMAkGA1UEBhMCR0IxGDAWBgNVBAoTD1NlY3Rp
# Z28gTGltaXRlZDEtMCsGA1UEAxMkU2VjdGlnbyBQdWJsaWMgQ29kZSBTaWduaW5n
# IFJvb3QgUjQ2MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAjeeUEiIE
# JHQu/xYjApKKtq42haxH1CORKz7cfeIxoFFvrISR41KKteKW3tCHYySJiv/vEpM7
# fbu2ir29BX8nm2tl06UMabG8STma8W1uquSggyfamg0rUOlLW7O4ZDakfko9qXGr
# YbNzszwLDO/bM1flvjQ345cbXf0fEj2CA3bm+z9m0pQxafptszSswXp43JJQ8mTH
# qi0Eq8Nq6uAvp6fcbtfo/9ohq0C/ue4NnsbZnpnvxt4fqQx2sycgoda6/YDnAdLv
# 64IplXCN/7sVz/7RDzaiLk8ykHRGa0c1E3cFM09jLrgt4b9lpwRrGNhx+swI8m2J
# mRCxrds+LOSqGLDGBwF1Z95t6WNjHjZ/aYm+qkU+blpfj6Fby50whjDoA7NAxg0P
# OM1nqFOI+rgwZfpvx+cdsYN0aT6sxGg7seZnM5q2COCABUhA7vaCZEao9XOwBpXy
# bGWfv1VbHJxXGsd4RnxwqpQbghesh+m2yQ6BHEDWFhcp/FycGCvqRfXvvdVnTyhe
# Be6QTHrnxvTQ/PrNPjJGEyA2igTqt6oHRpwNkzoJZplYXCmjuQymMDg80EY2NXyc
# uu7D1fkKdvp+BRtAypI16dV60bV/AK6pkKrFfwGcELEW/MxuGNxvYv6mUKe4e7id
# FT/+IAx1yCJaE5UZkADpGtXChvHjjuxf9OUCAwEAAaOCARIwggEOMB8GA1UdIwQY
# MBaAFKARCiM+lvEH7OKvKe+CpX/QMKS0MB0GA1UdDgQWBBQy65Ka/zWWSC8oQEJw
# IDaRXBeF5jAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zATBgNVHSUE
# DDAKBggrBgEFBQcDAzAbBgNVHSAEFDASMAYGBFUdIAAwCAYGZ4EMAQQBMEMGA1Ud
# HwQ8MDowOKA2oDSGMmh0dHA6Ly9jcmwuY29tb2RvY2EuY29tL0FBQUNlcnRpZmlj
# YXRlU2VydmljZXMuY3JsMDQGCCsGAQUFBwEBBCgwJjAkBggrBgEFBQcwAYYYaHR0
# cDovL29jc3AuY29tb2RvY2EuY29tMA0GCSqGSIb3DQEBDAUAA4IBAQASv6Hvi3Sa
# mES4aUa1qyQKDKSKZ7g6gb9Fin1SB6iNH04hhTmja14tIIa/ELiueTtTzbT72ES+
# BtlcY2fUQBaHRIZyKtYyFfUSg8L54V0RQGf2QidyxSPiAjgaTCDi2wH3zUZPJqJ8
# ZsBRNraJAlTH/Fj7bADu/pimLpWhDFMpH2/YGaZPnvesCepdgsaLr4CnvYFIUoQx
# 2jLsFeSmTD1sOXPUC4U5IOCFGmjhp0g4qdE2JXfBjRkWxYhMZn0vY86Y6GnfrDyo
# XZ3JHFuu2PMvdM+4fvbXg50RlmKarkUT2n/cR/vfw1Kf5gZV6Z2M8jpiUbzsJA8p
# 1FiAhORFe1rYMIIGGjCCBAKgAwIBAgIQYh1tDFIBnjuQeRUgiSEcCjANBgkqhkiG
# 9w0BAQwFADBWMQswCQYDVQQGEwJHQjEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVk
# MS0wKwYDVQQDEyRTZWN0aWdvIFB1YmxpYyBDb2RlIFNpZ25pbmcgUm9vdCBSNDYw
# HhcNMjEwMzIyMDAwMDAwWhcNMzYwMzIxMjM1OTU5WjBUMQswCQYDVQQGEwJHQjEY
# MBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMSswKQYDVQQDEyJTZWN0aWdvIFB1Ymxp
# YyBDb2RlIFNpZ25pbmcgQ0EgUjM2MIIBojANBgkqhkiG9w0BAQEFAAOCAY8AMIIB
# igKCAYEAmyudU/o1P45gBkNqwM/1f/bIU1MYyM7TbH78WAeVF3llMwsRHgBGRmxD
# eEDIArCS2VCoVk4Y/8j6stIkmYV5Gej4NgNjVQ4BYoDjGMwdjioXan1hlaGFt4Wk
# 9vT0k2oWJMJjL9G//N523hAm4jF4UjrW2pvv9+hdPX8tbbAfI3v0VdJiJPFy/7Xw
# iunD7mBxNtecM6ytIdUlh08T2z7mJEXZD9OWcJkZk5wDuf2q52PN43jc4T9OkoXZ
# 0arWZVeffvMr/iiIROSCzKoDmWABDRzV/UiQ5vqsaeFaqQdzFf4ed8peNWh1OaZX
# nYvZQgWx/SXiJDRSAolRzZEZquE6cbcH747FHncs/Kzcn0Ccv2jrOW+LPmnOyB+t
# AfiWu01TPhCr9VrkxsHC5qFNxaThTG5j4/Kc+ODD2dX/fmBECELcvzUHf9shoFvr
# n35XGf2RPaNTO2uSZ6n9otv7jElspkfK9qEATHZcodp+R4q2OIypxR//YEb3fkDn
# 3UayWW9bAgMBAAGjggFkMIIBYDAfBgNVHSMEGDAWgBQy65Ka/zWWSC8oQEJwIDaR
# XBeF5jAdBgNVHQ4EFgQUDyrLIIcouOxvSK4rVKYpqhekzQwwDgYDVR0PAQH/BAQD
# AgGGMBIGA1UdEwEB/wQIMAYBAf8CAQAwEwYDVR0lBAwwCgYIKwYBBQUHAwMwGwYD
# VR0gBBQwEjAGBgRVHSAAMAgGBmeBDAEEATBLBgNVHR8ERDBCMECgPqA8hjpodHRw
# Oi8vY3JsLnNlY3RpZ28uY29tL1NlY3RpZ29QdWJsaWNDb2RlU2lnbmluZ1Jvb3RS
# NDYuY3JsMHsGCCsGAQUFBwEBBG8wbTBGBggrBgEFBQcwAoY6aHR0cDovL2NydC5z
# ZWN0aWdvLmNvbS9TZWN0aWdvUHVibGljQ29kZVNpZ25pbmdSb290UjQ2LnA3YzAj
# BggrBgEFBQcwAYYXaHR0cDovL29jc3Auc2VjdGlnby5jb20wDQYJKoZIhvcNAQEM
# BQADggIBAAb/guF3YzZue6EVIJsT/wT+mHVEYcNWlXHRkT+FoetAQLHI1uBy/YXK
# ZDk8+Y1LoNqHrp22AKMGxQtgCivnDHFyAQ9GXTmlk7MjcgQbDCx6mn7yIawsppWk
# vfPkKaAQsiqaT9DnMWBHVNIabGqgQSGTrQWo43MOfsPynhbz2Hyxf5XWKZpRvr3d
# MapandPfYgoZ8iDL2OR3sYztgJrbG6VZ9DoTXFm1g0Rf97Aaen1l4c+w3DC+IkwF
# kvjFV3jS49ZSc4lShKK6BrPTJYs4NG1DGzmpToTnwoqZ8fAmi2XlZnuchC4NPSZa
# PATHvNIzt+z1PHo35D/f7j2pO1S8BCysQDHCbM5Mnomnq5aYcKCsdbh0czchOm8b
# kinLrYrKpii+Tk7pwL7TjRKLXkomm5D1Umds++pip8wH2cQpf93at3VDcOK4N7Ew
# oIJB0kak6pSzEu4I64U6gZs7tS/dGNSljf2OSSnRr7KWzq03zl8l75jy+hOds9TW
# SenLbjBQUGR96cFr6lEUfAIEHVC1L68Y1GGxx4/eRI82ut83axHMViw1+sVpbPxg
# 51Tbnio1lB93079WPFnYaOvfGAA0e0zcfF/M9gXr+korwQTh2Prqooq2bYNMvUoU
# KD85gnJ+t0smrWrb8dee2CvYZXD5laGtaAxOfy/VKNmwuWuAh9kcMIIGPTCCBKWg
# AwIBAgIQHTQSaFFD0/dtUy63brV68DANBgkqhkiG9w0BAQwFADBUMQswCQYDVQQG
# EwJHQjEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMSswKQYDVQQDEyJTZWN0aWdv
# IFB1YmxpYyBDb2RlIFNpZ25pbmcgQ0EgUjM2MB4XDTIyMDkwMTAwMDAwMFoXDTIz
# MDkwMTIzNTk1OVowVDELMAkGA1UEBhMCVVMxETAPBgNVBAgMCFZpcmdpbmlhMRgw
# FgYDVQQKDA9OYXRoYW5pZWwgS2lwcHMxGDAWBgNVBAMMD05hdGhhbmllbCBLaXBw
# czCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBANfdlvg7QFIfRd1c0nA5
# EjzQTls+2aEpgvDRpgl9zAljNLXYTgvQJfWT0/SJEAPKAewsIB9nVVjPSSe1qFak
# db47cWj8zgLfRWwa6pnfu+ng4G7fOW7/2RrUVNfU+eO2Z1Yo7Iy/W2RUsayUxH+2
# hVt65O93t7PZiQ3MAGTQF80yTitkW8Omd/IgRo+MpETSCwKsaQzJUtFsW0Onzy9s
# nkPJhs6bbg+YNoqVggkqvJZ8GlU2Ulv18jXqQMBruAJgaakoessmbL2eBgyYTtJf
# R/rLK+D5EWnp0ZmTK2rDs8JhuxDhoRK2/5wiCwr3GhmYk2iZP/l0z0LM/zd2TkPF
# 8TBsFczliBxd71AhjeZ9DtCrQR0ZlvxT+ycUENBPeLtroYf8kU2d/EWqWlLEw8C7
# B2rTJolV5Zb7CwMU3L99YZW9oRFrRrKLKCen2G2UEdCePMR4cGewnKxGOY2qHGIg
# lb42S/yLPgl9nV9Pzkg6GPOoIrXFOaJWhppTjJvqaaxafmV0iOvabdgM3lLEElWb
# PXubi36PNcZ8BTJDPvJ8clJIYrIqGO4OLoJ6vJVycZLJca+ClKSFFuC4aKU602Ge
# uG26qtKuyKr7wo0Aa5OhmI2yahp9kk5wfVmlXcc+CBGV5XOdognmp3zjUoj4MoLN
# ya6cB0BUZ8H5EdKp9JPnEMERAgMBAAGjggGJMIIBhTAfBgNVHSMEGDAWgBQPKssg
# hyi47G9IritUpimqF6TNDDAdBgNVHQ4EFgQUtNsk0/UyW7UozUSsTrgdHL8NOPQw
# DgYDVR0PAQH/BAQDAgeAMAwGA1UdEwEB/wQCMAAwEwYDVR0lBAwwCgYIKwYBBQUH
# AwMwSgYDVR0gBEMwQTA1BgwrBgEEAbIxAQIBAwIwJTAjBggrBgEFBQcCARYXaHR0
# cHM6Ly9zZWN0aWdvLmNvbS9DUFMwCAYGZ4EMAQQBMEkGA1UdHwRCMEAwPqA8oDqG
# OGh0dHA6Ly9jcmwuc2VjdGlnby5jb20vU2VjdGlnb1B1YmxpY0NvZGVTaWduaW5n
# Q0FSMzYuY3JsMHkGCCsGAQUFBwEBBG0wazBEBggrBgEFBQcwAoY4aHR0cDovL2Ny
# dC5zZWN0aWdvLmNvbS9TZWN0aWdvUHVibGljQ29kZVNpZ25pbmdDQVIzNi5jcnQw
# IwYIKwYBBQUHMAGGF2h0dHA6Ly9vY3NwLnNlY3RpZ28uY29tMA0GCSqGSIb3DQEB
# DAUAA4IBgQAuQ7bCCANjgqc+UXd1pquMpDuYP+f3G0TLei6KlwORGwAEZ5OcrCCI
# NEtyPQ//pB/EkrAywwuAJklQ1ov7M6zm2YwRhJ6NFjxeeaBhZ3J9Y6doPabzGiYB
# DuwYOGiUrXg/cI0yE66CtW9PmGK2pnZpG9L/0CnihBS2/aMxzHQP/JIVHIxTcXBX
# Xa+LJ78M3CrpcAAs0EF3BUrFboqVp8AT+OgAzAvAXB+mOxK1oYBsrmF/C2FEh9aY
# HCkk0lKjVauux4mVHNi3c3Tlj1Nq4RY0Z3GFybJCD5hUL7lW2xPPrwiv95NfccSd
# TgQbGMfj0QRgYAq5paZuULEAQrIrE1pJUn4vH8nLwUD0A8mSnR6uwFHEvmeGMCx2
# 6UeZ5l1BfLpwVhbQGPVusDfPn07pogjX7vMZtgxEyCJacof7YBpNaoqT8K0wAzhn
# WevGe06pGGF8VOoq2frqp4Ad2TUTQcX44MnTXL9VLsmJMoxhmJywHzPw5ss5CLl8
# Sh6YZAcyKiYwggbsMIIE1KADAgECAhAwD2+s3WaYdHypRjaneC25MA0GCSqGSIb3
# DQEBDAUAMIGIMQswCQYDVQQGEwJVUzETMBEGA1UECBMKTmV3IEplcnNleTEUMBIG
# A1UEBxMLSmVyc2V5IENpdHkxHjAcBgNVBAoTFVRoZSBVU0VSVFJVU1QgTmV0d29y
# azEuMCwGA1UEAxMlVVNFUlRydXN0IFJTQSBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0
# eTAeFw0xOTA1MDIwMDAwMDBaFw0zODAxMTgyMzU5NTlaMH0xCzAJBgNVBAYTAkdC
# MRswGQYDVQQIExJHcmVhdGVyIE1hbmNoZXN0ZXIxEDAOBgNVBAcTB1NhbGZvcmQx
# GDAWBgNVBAoTD1NlY3RpZ28gTGltaXRlZDElMCMGA1UEAxMcU2VjdGlnbyBSU0Eg
# VGltZSBTdGFtcGluZyBDQTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIB
# AMgbAa/ZLH6ImX0BmD8gkL2cgCFUk7nPoD5T77NawHbWGgSlzkeDtevEzEk0y/NF
# Zbn5p2QWJgn71TJSeS7JY8ITm7aGPwEFkmZvIavVcRB5h/RGKs3EWsnb111JTXJW
# D9zJ41OYOioe/M5YSdO/8zm7uaQjQqzQFcN/nqJc1zjxFrJw06PE37PFcqwuCnf8
# DZRSt/wflXMkPQEovA8NT7ORAY5unSd1VdEXOzQhe5cBlK9/gM/REQpXhMl/VuC9
# RpyCvpSdv7QgsGB+uE31DT/b0OqFjIpWcdEtlEzIjDzTFKKcvSb/01Mgx2Bpm1gK
# VPQF5/0xrPnIhRfHuCkZpCkvRuPd25Ffnz82Pg4wZytGtzWvlr7aTGDMqLufDRTU
# GMQwmHSCIc9iVrUhcxIe/arKCFiHd6QV6xlV/9A5VC0m7kUaOm/N14Tw1/AoxU9k
# gwLU++Le8bwCKPRt2ieKBtKWh97oaw7wW33pdmmTIBxKlyx3GSuTlZicl57rjsF4
# VsZEJd8GEpoGLZ8DXv2DolNnyrH6jaFkyYiSWcuoRsDJ8qb/fVfbEnb6ikEk1Bv8
# cqUUotStQxykSYtBORQDHin6G6UirqXDTYLQjdprt9v3GEBXc/Bxo/tKfUU2wfeN
# gvq5yQ1TgH36tjlYMu9vGFCJ10+dM70atZ2h3pVBeqeDAgMBAAGjggFaMIIBVjAf
# BgNVHSMEGDAWgBRTeb9aqitKz1SA4dibwJ3ysgNmyzAdBgNVHQ4EFgQUGqH4YRkg
# D8NBd0UojtE1XwYSBFUwDgYDVR0PAQH/BAQDAgGGMBIGA1UdEwEB/wQIMAYBAf8C
# AQAwEwYDVR0lBAwwCgYIKwYBBQUHAwgwEQYDVR0gBAowCDAGBgRVHSAAMFAGA1Ud
# HwRJMEcwRaBDoEGGP2h0dHA6Ly9jcmwudXNlcnRydXN0LmNvbS9VU0VSVHJ1c3RS
# U0FDZXJ0aWZpY2F0aW9uQXV0aG9yaXR5LmNybDB2BggrBgEFBQcBAQRqMGgwPwYI
# KwYBBQUHMAKGM2h0dHA6Ly9jcnQudXNlcnRydXN0LmNvbS9VU0VSVHJ1c3RSU0FB
# ZGRUcnVzdENBLmNydDAlBggrBgEFBQcwAYYZaHR0cDovL29jc3AudXNlcnRydXN0
# LmNvbTANBgkqhkiG9w0BAQwFAAOCAgEAbVSBpTNdFuG1U4GRdd8DejILLSWEEbKw
# 2yp9KgX1vDsn9FqguUlZkClsYcu1UNviffmfAO9Aw63T4uRW+VhBz/FC5RB9/7B0
# H4/GXAn5M17qoBwmWFzztBEP1dXD4rzVWHi/SHbhRGdtj7BDEA+N5Pk4Yr8TAcWF
# o0zFzLJTMJWk1vSWVgi4zVx/AZa+clJqO0I3fBZ4OZOTlJux3LJtQW1nzclvkD1/
# RXLBGyPWwlWEZuSzxWYG9vPWS16toytCiiGS/qhvWiVwYoFzY16gu9jc10rTPa+D
# BjgSHSSHLeT8AtY+dwS8BDa153fLnC6NIxi5o8JHHfBd1qFzVwVomqfJN2Udvuq8
# 2EKDQwWli6YJ/9GhlKZOqj0J9QVst9JkWtgqIsJLnfE5XkzeSD2bNJaaCV+O/fex
# UpHOP4n2HKG1qXUfcb9bQ11lPVCBbqvw0NP8srMftpmWJvQ8eYtcZMzN7iea5aDA
# DHKHwW5NWtMe6vBE5jJvHOsXTpTDeGUgOw9Bqh/poUGd/rG4oGUqNODeqPk85sEw
# u8CgYyz8XBYAqNDEf+oRnR4GxqZtMl20OAkrSQeq/eww2vGnL8+3/frQo4TZJ577
# AWZ3uVYQ4SBuxq6x+ba6yDVdM3aO8XwgDCp3rrWiAoa6Ke60WgCxjKvj+QrJVF3U
# uWp0nr1Irpgwggb1MIIE3aADAgECAhA5TCXhfKBtJ6hl4jvZHSLUMA0GCSqGSIb3
# DQEBDAUAMH0xCzAJBgNVBAYTAkdCMRswGQYDVQQIExJHcmVhdGVyIE1hbmNoZXN0
# ZXIxEDAOBgNVBAcTB1NhbGZvcmQxGDAWBgNVBAoTD1NlY3RpZ28gTGltaXRlZDEl
# MCMGA1UEAxMcU2VjdGlnbyBSU0EgVGltZSBTdGFtcGluZyBDQTAeFw0yMzA1MDMw
# MDAwMDBaFw0zNDA4MDIyMzU5NTlaMGoxCzAJBgNVBAYTAkdCMRMwEQYDVQQIEwpN
# YW5jaGVzdGVyMRgwFgYDVQQKEw9TZWN0aWdvIExpbWl0ZWQxLDAqBgNVBAMMI1Nl
# Y3RpZ28gUlNBIFRpbWUgU3RhbXBpbmcgU2lnbmVyICM0MIICIjANBgkqhkiG9w0B
# AQEFAAOCAg8AMIICCgKCAgEApJMoUkvPJ4d2pCkcmTjA5w7U0RzsaMsBZOSKzXew
# cWWCvJ/8i7u7lZj7JRGOWogJZhEUWLK6Ilvm9jLxXS3AeqIO4OBWZO2h5YEgciBk
# QWzHwwj6831d7yGawn7XLMO6EZge/NMgCEKzX79/iFgyqzCz2Ix6lkoZE1ys/Oer
# 6RwWLrCwOJVKz4VQq2cDJaG7OOkPb6lampEoEzW5H/M94STIa7GZ6A3vu03lPYxU
# A5HQ/C3PVTM4egkcB9Ei4GOGp7790oNzEhSbmkwJRr00vOFLUHty4Fv9GbsfPGoZ
# e267LUQqvjxMzKyKBJPGV4agczYrgZf6G5t+iIfYUnmJ/m53N9e7UJ/6GCVPE/Je
# fKmxIFopq6NCh3fg9EwCSN1YpVOmo6DtGZZlFSnF7TMwJeaWg4Ga9mBmkFgHgM1C
# daz7tJHQxd0BQGq2qBDu9o16t551r9OlSxihDJ9XsF4lR5F0zXUS0Zxv5F4Nm+x1
# Ju7+0/WSL1KF6NpEUSqizADKh2ZDoxsA76K1lp1irScL8htKycOUQjeIIISoh67D
# uiNye/hU7/hrJ7CF9adDhdgrOXTbWncC0aT69c2cPcwfrlHQe2zYHS0RQlNxdMLl
# NaotUhLZJc/w09CRQxLXMn2YbON3Qcj/HyRU726txj5Ve/Fchzpk8WBLBU/vuS/s
# CRMCAwEAAaOCAYIwggF+MB8GA1UdIwQYMBaAFBqh+GEZIA/DQXdFKI7RNV8GEgRV
# MB0GA1UdDgQWBBQDDzHIkSqTvWPz0V1NpDQP0pUBGDAOBgNVHQ8BAf8EBAMCBsAw
# DAYDVR0TAQH/BAIwADAWBgNVHSUBAf8EDDAKBggrBgEFBQcDCDBKBgNVHSAEQzBB
# MDUGDCsGAQQBsjEBAgEDCDAlMCMGCCsGAQUFBwIBFhdodHRwczovL3NlY3RpZ28u
# Y29tL0NQUzAIBgZngQwBBAIwRAYDVR0fBD0wOzA5oDegNYYzaHR0cDovL2NybC5z
# ZWN0aWdvLmNvbS9TZWN0aWdvUlNBVGltZVN0YW1waW5nQ0EuY3JsMHQGCCsGAQUF
# BwEBBGgwZjA/BggrBgEFBQcwAoYzaHR0cDovL2NydC5zZWN0aWdvLmNvbS9TZWN0
# aWdvUlNBVGltZVN0YW1waW5nQ0EuY3J0MCMGCCsGAQUFBzABhhdodHRwOi8vb2Nz
# cC5zZWN0aWdvLmNvbTANBgkqhkiG9w0BAQwFAAOCAgEATJtlWPrgec/vFcMybd4z
# ket3WOLrvctKPHXefpRtwyLHBJXfZWlhEwz2DJ71iSBewYfHAyTKx6XwJt/4+DFl
# DeDrbVFXpoyEUghGHCrC3vLaikXzvvf2LsR+7fjtaL96VkjpYeWaOXe8vrqRZIh1
# /12FFjQn0inL/+0t2v++kwzsbaINzMPxbr0hkRojAFKtl9RieCqEeajXPawhj3DD
# JHk6l/ENo6NbU9irALpY+zWAT18ocWwZXsKDcpCu4MbY8pn76rSSZXwHfDVEHa1Y
# GGti+95sxAqpbNMhRnDcL411TCPCQdB6ljvDS93NkiZ0dlw3oJoknk5fTtOPD+UT
# T1lEZUtDZM9I+GdnuU2/zA2xOjDQoT1IrXpl5Ozf4AHwsypKOazBpPmpfTXQMkCg
# sRkqGCGyyH0FcRpLJzaq4Jgcg3Xnx35LhEPNQ/uQl3YqEqxAwXBbmQpA+oBtlGF7
# yG65yGdnJFxQjQEg3gf3AdT4LhHNnYPl+MolHEQ9J+WwhkcqCxuEdn17aE+Nt/cT
# tO2gLe5zD9kQup2ZLHzXdR+PEMSU5n4k5ZVKiIwn1oVmHfmuZHaR6Ej+yFUK7SnD
# H944psAU+zI9+KmDYjbIw74Ahxyr+kpCHIkD3PVcfHDZXXhO7p9eIOYJanwrCKNI
# 9RX8BE/fzSEceuX1jhrUuUAxggZYMIIGVAIBATBoMFQxCzAJBgNVBAYTAkdCMRgw
# FgYDVQQKEw9TZWN0aWdvIExpbWl0ZWQxKzApBgNVBAMTIlNlY3RpZ28gUHVibGlj
# IENvZGUgU2lnbmluZyBDQSBSMzYCEB00EmhRQ9P3bVMut261evAwCQYFKw4DAhoF
# AKB4MBgGCisGAQQBgjcCAQwxCjAIoAKAAKECgAAwGQYJKoZIhvcNAQkDMQwGCisG
# AQQBgjcCAQQwHAYKKwYBBAGCNwIBCzEOMAwGCisGAQQBgjcCARUwIwYJKoZIhvcN
# AQkEMRYEFP9G24ZEM+XbJIdxDCXKVODhr0fUMA0GCSqGSIb3DQEBAQUABIICACrV
# 21fcI4CVKCB3eSyiAjADo3NM3H4HbsOieCfaXVGzqL+RL1UxM/7UrqIr4WsO+AVa
# WOOwdnfP8iOyp2Saus30CJ6ube6dQhiktsqdFZfU45H6TYZYngwlHeRj/LoXhJE1
# MmjPUximyVGaMXphmjaYOAQu+EvWr/T4TTXSc25pkHZmR1jY+kdD80+OhdPJrTr2
# dWGmuyZU1/4K8rTDyyRFUI0gUroDWya3nvvylU/LYj3McmzUHEbNUq5P1SPic9Jk
# CIep6ej2DvMDni+JKcDAq3jCa88dQV3xuwAtZ+Nb/jmFduGM+BTQCcJYWGMSA1o0
# N3I1wYU0TTe+4RmwRcVQkfx0zqPnEBW2AW/3GQ9eipwP7aZOi/g4qZd8Of2x4UDw
# 4NIp/DZJNoHX9sCps3AAHxaBcVVt2DHVVD4h4l9kcPspIoXIo/DZUlt0LOFYISzD
# NEi1dbjQNptkIV5Ev7Qcto1addOcOJTYL247tx8uoUn5i3v7hkTQ4ssdD69XSoQY
# kDMTmjJ3pO1qQT4wtvOoF29L8zyHlNHE2uhD1lqfvkw/QY6oxhQOTePW55cJNaUa
# rGmeQGYULeS8eqsAqjnqB2Egqjj+2RVE3npl2cpQmAHy35DOJMjohgr1ejWo/TEv
# 10aMo4t+MPeYttO+xSXzgQPfxwKJCkgG8GPUOgCVoYIDSzCCA0cGCSqGSIb3DQEJ
# BjGCAzgwggM0AgEBMIGRMH0xCzAJBgNVBAYTAkdCMRswGQYDVQQIExJHcmVhdGVy
# IE1hbmNoZXN0ZXIxEDAOBgNVBAcTB1NhbGZvcmQxGDAWBgNVBAoTD1NlY3RpZ28g
# TGltaXRlZDElMCMGA1UEAxMcU2VjdGlnbyBSU0EgVGltZSBTdGFtcGluZyBDQQIQ
# OUwl4XygbSeoZeI72R0i1DANBglghkgBZQMEAgIFAKB5MBgGCSqGSIb3DQEJAzEL
# BgkqhkiG9w0BBwEwHAYJKoZIhvcNAQkFMQ8XDTIzMDYxOTIxNDE0N1owPwYJKoZI
# hvcNAQkEMTIEMOQf41fIgghI2b73/kAw43xp4xw3igpiB9ogHNNuhDQzGHm1Qxng
# xyVgOFHQlINepzANBgkqhkiG9w0BAQEFAASCAgBsYrfMRDWMxQwXQwL8f9SiOD03
# zn0yZOBJL/fQvl8c/cjDbVNZud897UtrHxzMVLXNGOdS6w+oGtnuV0ASafvP2Zxh
# xLN2jp2hxBxwSutOOZAO0shVvqd8J3PK1kPTSeLwIStlz+gnAFIlHOAWcC/Sbk9e
# KgIkJ18mexZXOtCv7JTGPb7ICUZIj5ZpCBEkRiWOoUnj1TLGUceNzq/uRl9EWSxW
# 4lV0trTI6tt8KoP63pip2OiC1kdY+dyyAmzNllb6z/gW6lLekJ2/+qyHJUpoZ5EN
# jh8FFK+MZhN1V01AoX3sTMiTOr8H2vu35zGj5OZwXgieGXncRJFrVCWfhnbhu49x
# xuTC0rfdMI8YZ7r+mVpe5Eqoqr0X7BVeLD0ekM+wAaNrhCtR0e19fA09jqRs/2D/
# hI/fBixRV9m170fL2NGgMIIgXLLDADaizFFwA6GjSiiompJNt+Zqvy5akkNWMayp
# ceNeAKG9bxqBfPZmVIR6d5HZdX9Gu0W7Dr6zxt48IFg7mIkvAlOHNGKkalXLRYHV
# PhLNM11EQWmIdg7zIaIThGPoJo30XPHV0vNetk87llFUOLGCnAb2LsGoDre+UpLd
# WlwHct5UlCdps9f+aK3Ups2Xguemh5Xtlh4u60Yx8U5KdvKnnEQ1y1wqQcCFy/QU
# 6DR4O1+hUajAEJt8ag==
# SIG # End signature block
