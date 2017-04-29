# filepacker
Packs multiple files into a single binary file for simplified distribution. Win32 only.

DESCRIPTION

The 'filepacker' command packs all files under a given directory into a single file. 
The folder hierarchy information is kept within the file, such that the folder structure can be restored from the file via the 'fileunpacker' command.
The additional filepackertest verifies that the contents and structure of the source directory match after the directory has been packed and unpacked.

BUILD

* Just call build.bat from a VisualStudio command prompt. Otherwise call shell.bat first from a CMD.exe to setup the build environment (adapt the path to your VS installation first).
* This creates a "build" directory, containing the three executables: filepacker, fileunpacker and filepackertest.

