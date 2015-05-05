#!/bin/bash

# This script is to package the FreeIMU_GUI package for Windows/Linux and OSx
# This script should run under Linux and OSx, as well as Windows with Cygwin.

#############################
# CONFIGURATION
#############################

if [ $# -eq 1 ]
then
  BUILD_TARGET=$1
else
  BUILD_TARGET=${1:-all}
fi

##Select the build target
#BUILD_TARGET=${1:-all}
#BUILD_TARGET=win32
#BUILD_TARGET=linux
#BUILD_TARGET=osx64

##Do we need to create the final archive
ARCHIVE_FOR_DISTRIBUTION=1
##Which version name are we appending to the final archive
BUILD_NAME=0.2
APP_NAME=FreeIMU_GUI
BUILD_DIR=dist
TARGET_DIR=${APP_NAME}-${BUILD_NAME}-${BUILD_TARGET}


##Which versions of external programs to use
PYPY_VERSION=1.9
WIN_PORTABLE_PY_VERSION=2.7.2.1

#############################
# Support functions
#############################
function checkTool
{
	if [ -z `which $1` ]; then
		echo "The $1 command must be somewhere in your \$PATH."
		echo "Fix your \$PATH or install $2"
		exit 1
	fi
}

function downloadURL
{
	filename=`basename "$1"`
	echo "Checking for $filename"
	if [ ! -f "$filename" ]; then
		echo "Downloading $1"
		curl -4 -L -O "$1"
		if [ $? != 0 ]; then
			echo "Failed to download $1"
			exit 1
		fi
	fi
}

function extract
{
	echo "Extracting $*"
	echo "7z x -y $*" >> log.txt
	7z x -y $* >> log.txt
}

#############################
# Actual build script
#############################
if [ "$BUILD_TARGET" = "all" ]; then
	$0 win32
	$0 linux
	$0 osx64
	exit
fi

# Change working directory to the directory the script is in
# http://stackoverflow.com/a/246128
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $SCRIPT_DIR

checkTool git "git: http://git-scm.com/"
checkTool curl "curl: http://curl.haxx.se/"
if [ $BUILD_TARGET = "win32" ]; then
	#Check if we have 7zip, needed to extract and packup a bunch of packages for windows.
	checkTool 7z "7zip: http://www.7-zip.org/"
fi
#For building under MacOS we need gnutar instead of tar
if [ -z `which gnutar` ]; then
	TAR=tar
else
	TAR=gnutar
fi


# change directory
cd $BUILD_DIR


#############################
# Download all needed files.
#############################


if [ $BUILD_TARGET = "win32" ]; then
	#Get portable python for windows and extract it. (Linux and Mac need to install python themselfs)
	downloadURL http://ftp.nluug.nl/languages/python/portablepython/v2.7/PortablePython_${WIN_PORTABLE_PY_VERSION}.exe
	downloadURL http://sourceforge.net/projects/pyserial/files/pyserial/2.5/pyserial-2.5.win32.exe
	downloadURL http://sourceforge.net/projects/pyopengl/files/PyOpenGL/3.0.1/PyOpenGL-3.0.1.win32.exe
  downloadURL http://sourceforge.net/projects/pyqt/files/PyQt4/PyQt-4.9.5/PyQt-Py2.7-x86-gpl-4.9.5-1.exe
  downloadURL http://www.pyqtgraph.org/downloads/pyqtgraph-dev-r223-32.exe
  downloadURL http://downloads.sourceforge.net/project/scipy/scipy/0.11.0/scipy-0.11.0-win32-superpack-python2.7.exe
	downloadURL http://sourceforge.net/projects/numpy/files/NumPy/1.6.2/numpy-1.6.2-win32-superpack-python2.7.exe
	downloadURL http://sourceforge.net/projects/comtypes/files/comtypes/0.6.2/comtypes-0.6.2.win32.exe
	#Get pypy
	downloadURL https://bitbucket.org/pypy/pypy/downloads/pypy-${PYPY_VERSION}-win32.zip
elif [ $BUILD_TARGET = "osx64" ]; then
	downloadURL https://bitbucket.org/pypy/pypy/downloads/pypy-${PYPY_VERSION}-${BUILD_TARGET}.tar.bz2
	downloadURL http://python.org/ftp/python/2.7.3/python-2.7.3-macosx10.6.dmg
	downloadURL http://sourceforge.net/projects/numpy/files/NumPy/1.6.2/numpy-1.6.2-py2.7-python.org-macosx10.3.dmg
	downloadURL http://pypi.python.org/packages/source/p/pyserial/pyserial-2.6.tar.gz
	downloadURL http://pypi.python.org/packages/source/P/PyOpenGL/PyOpenGL-3.0.2.tar.gz
	downloadURL http://downloads.sourceforge.net/wxpython/wxPython2.9-osx-2.9.4.0-cocoa-py2.7.dmg
else
	downloadURL https://bitbucket.org/pypy/pypy/downloads/pypy-${PYPY_VERSION}-${BUILD_TARGET}.tar.bz2
fi

#############################
# Build the packages
#############################
rm -rf ${TARGET_DIR}
mkdir -p ${TARGET_DIR}

rm -f log.txt
if [ $BUILD_TARGET = "win32" ]; then
	#For windows extract portable python to include it.
	extract PortablePython_${WIN_PORTABLE_PY_VERSION}.exe \$_OUTDIR/App
	extract PortablePython_${WIN_PORTABLE_PY_VERSION}.exe \$_OUTDIR/Lib/site-packages

  mkdir -p ${TARGET_DIR}/python
  mkdir -p ${TARGET_DIR}/${APP_NAME}/
  mv \$_OUTDIR/App/* ${TARGET_DIR}/python
  mv \$_OUTDIR/Lib/site-packages/wx* ${TARGET_DIR}/python/Lib/site-packages/
  rm -rf \$_OUTDIR

	extract pyserial-2.5.win32.exe PURELIB
	extract PyOpenGL-3.0.1.win32.exe PURELIB
  extract PyQt-Py2.7-x86-gpl-4.9.5-1.exe
  extract pyqtgraph-dev-r223-32.exe PURELIB
	extract numpy-1.6.2-win32-superpack-python2.7.exe numpy-1.6.2-sse2.exe
	extract numpy-1.6.2-sse2.exe PLATLIB
  extract scipy-0.11.0-win32-superpack-python2.7.exe scipy-0.11.0-sse2.exe
  extract scipy-0.11.0-sse2.exe PLATLIB
	extract comtypes-0.6.2.win32.exe
	
	mv Lib/site-packages/* ${TARGET_DIR}/python/Lib/site-packages
	mv \$_OUTDIR/* ${TARGET_DIR}/python/Lib/site-packages/PyQt4/
  mv PURELIB/serial ${TARGET_DIR}/python/Lib
	mv PURELIB/OpenGL ${TARGET_DIR}/python/Lib
  mv PURELIB/pyqtgraph ${TARGET_DIR}/python/Lib
	mv PURELIB/comtypes ${TARGET_DIR}/python/Lib
	mv PLATLIB/numpy ${TARGET_DIR}/python/Lib
  mv PLATLIB/scipy ${TARGET_DIR}/python/Lib
	#
	rm -rf PURELIB
	rm -rf PLATLIB
	rm -rf numpy-1.6.2-sse2.exe
	
	#Clean up portable python a bit, to keep the package size down.
	rm -rf ${TARGET_DIR}/python/PyScripter.*
	rm -rf ${TARGET_DIR}/python/Doc
	rm -rf ${TARGET_DIR}/python/locale
	rm -rf ${TARGET_DIR}/python/tcl
	rm -rf ${TARGET_DIR}/python/Lib/test
	rm -rf ${TARGET_DIR}/python/Lib/distutils
	rm -rf ${TARGET_DIR}/python/Lib/site-packages/wx-2.8-msw-unicode/wx/tools
	rm -rf ${TARGET_DIR}/python/Lib/site-packages/wx-2.8-msw-unicode/wx/locale
	#Remove the gle files because they require MSVCR71.dll, which is not included. We also don't need gle, so it's safe to remove it.
	rm -rf ${TARGET_DIR}/python/Lib/OpenGL/DLLS/gle*
fi

#Extract pypy
if [ $BUILD_TARGET = "win32" ]; then
	extract pypy-${PYPY_VERSION}-win32.zip -o${TARGET_DIR}
else
	cd ${TARGET_DIR}; $TAR -xjf ../pypy-${PYPY_VERSION}-${BUILD_TARGET}.tar.bz2; cd ..
fi
mv ${TARGET_DIR}/pypy-* ${TARGET_DIR}/pypy
#Cleanup pypy
rm -rf ${TARGET_DIR}/pypy/lib-python/2.7/test

#add FreeIMU_GUI
mkdir -p ${TARGET_DIR}/${APP_NAME}
cp -a ../FreeIMU_GUI/* ${TARGET_DIR}/${APP_NAME}
#Add version file
echo $BUILD_NAME > ${TARGET_DIR}/${APP_NAME}/version

#add script files
if [ $BUILD_TARGET = "win32" ]; then
    cp -a ../scripts/${BUILD_TARGET}/*.bat $TARGET_DIR/
else
    cp -a ../scripts/${BUILD_TARGET}/*.sh $TARGET_DIR/
fi

#package the result
if (( ${ARCHIVE_FOR_DISTRIBUTION} )); then
	if [ $BUILD_TARGET = "win32" ]; then
		#rm ${TARGET_DIR}.zip
		#cd ${TARGET_DIR}
		#7z a ../${TARGET_DIR}.zip *
		#cd ..
		
		if [ ! -z `which wine` ]; then
			#if we have wine, try to run our nsis script.
			rm -rf scripts/win32/dist
			ln -sf `pwd`/${TARGET_DIR} scripts/win32/dist
			wine ~/.wine/drive_c/Program\ Files/NSIS/makensis.exe /DVERSION=${BUILD_NAME} scripts/win32/installer.nsi 
			mv scripts/win32/Cura_${BUILD_NAME}.exe ./
		fi
		if [ -f '/cygdrive/c/Program Files/NSIS/makensis.exe' ]; then
			rm -rf scripts/win32/dist
			mv `pwd`/${TARGET_DIR} scripts/win32/dist
			'/c/Program Files/NSIS/makensis.exe' -DVERSION=${BUILD_NAME} 'scripts/win32/installer.nsi' >> log.txt
			mv scripts/win32/Cura_${BUILD_NAME}.exe ./
		fi
	elif [ $BUILD_TARGET = "osx64" ]; then
		echo "Building osx app"
		mkdir -p scripts/osx64/Cura.app/Contents/Resources
		mkdir -p scripts/osx64/Cura.app/Contents/Pkgs
		rm -rf scripts/osx64/Cura.app/Contents/Resources/Cura
		rm -rf scripts/osx64/Cura.app/Contents/Resources/pypy
		cp -a ${TARGET_DIR}/* scripts/osx64/Cura.app/Contents/Resources
		cp python-2.7.3-macosx10.6.dmg scripts/osx64/Cura.app/Contents/Pkgs
		cp numpy-1.6.2-py2.7-python.org-macosx10.3.dmg scripts/osx64/Cura.app/Contents/Pkgs
		cp pyserial-2.6.tar.gz scripts/osx64/Cura.app/Contents/Pkgs
		cp PyOpenGL-3.0.2.tar.gz scripts/osx64/Cura.app/Contents/Pkgs
		cp wxPython2.9-osx-2.9.4.0-cocoa-py2.7.dmg scripts/osx64/Cura.app/Contents/Pkgs
		cd scripts/osx64
		$TAR cfp - Cura.app | gzip --best -c > ../../${TARGET_DIR}.tar.gz
		hdiutil detach /Volumes/Cura\ -\ Ultimaker/
		rm -rf Cura.dmg.sparseimage
		hdiutil convert DmgTemplateCompressed.dmg -format UDSP -o Cura.dmg
		hdiutil resize -size 500m Cura.dmg.sparseimage
		hdiutil attach Cura.dmg.sparseimage
		cp -a Cura.app /Volumes/Cura\ -\ Ultimaker/Cura/
		hdiutil detach /Volumes/Cura\ -\ Ultimaker
		hdiutil convert Cura.dmg.sparseimage -format UDZO -imagekey zlib-level=9 -ov -o ../../${TARGET_DIR}.dmg
	else
		echo "Archiving to ${TARGET_DIR}.tar.gz"
		$TAR cfp - ${TARGET_DIR} | gzip --best -c > ${TARGET_DIR}.tar.gz
	fi
else
	echo "Installed into ${TARGET_DIR}"
fi
