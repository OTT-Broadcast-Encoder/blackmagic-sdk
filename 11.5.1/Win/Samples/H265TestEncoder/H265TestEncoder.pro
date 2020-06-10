# -LICENSE-START-
# Copyright (c) 2015 Blackmagic Design
#
# Permission is hereby granted, free of charge, to any person or organization
# obtaining a copy of the software and accompanying documentation covered by
# this license (the "Software") to use, reproduce, display, distribute,
# execute, and transmit the Software, and to prepare derivative works of the
# Software, and to permit third-parties to whom the Software is furnished to
# do so, all subject to the following:
#
# The copyright notices in the Software and this entire statement, including
# the above license grant, this restriction and the following disclaimer,
# must be included in all copies of the Software, in whole or in part, and
# all derivative works of the Software, unless such copies or derivative
# works are solely in the form of machine-executable object code generated by
# a source language processor.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
# SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
# FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
# -LICENSE-END-

CONFIG += qt
CONFIG += embed_manifest_exe
CONFIG += c++11

QT += core gui widgets

INCLUDEPATH += .\
               src\
               ../../include

lessThan(QT_VERSION, 5.2) {
	error("Qt 5.2 or greater is required.")
}

QT += widgets

TARGET = H265TestEncoder
TEMPLATE = app

SOURCES += "src/main.cpp"\
           "src/MainWindow.cpp"\
           "src/ControllerImp.cpp"\
           "src/ControllerWidget.cpp"\
           "src/VideoWriter.cpp"\
           "src/DeckLinkDevice.cpp"\
           "src/CommonGui.cpp"\
           "src/ColourPalette.cpp"\
		   "src/CommonWidgets.cpp"

HEADERS += "src/MainWindow.h"\
           "src/ControllerImp.h"\
           "src/ControllerWidget.h"\
           "src/VideoWriter.h"\
           "src/DeckLinkDevice.h"\
           "src/CommonGui.h"\
           "src/ColourPalette.h"\
           "src/CommonGui.h"\
		   "src/CommonWidgets.h"

RC_FILE = H265TestEncoder.rc
ICON    = Encoder_icon.ico

RESOURCES += H265TestEncoder.qrc

DEFINES += NOMINMAX

MIDL_FILES = "../../include/DeckLinkAPI.idl"

MIDL.name = Compiling IDL
MIDL.input = MIDL_FILES
MIDL.output = ${QMAKE_FILE_BASE}.h
MIDL.variable_out = HEADERS
MIDL_CONFIG += no_link
contains(QMAKE_TARGET.arch, x86_64)	{
	MIDL.commands = midl.exe /env win64 /h ${QMAKE_FILE_BASE}.h /W1 /char signed /D "NDEBUG" /robust /nologo ${QMAKE_FILE_IN}
} else {
	MIDL.commands = midl.exe /env win32 /h ${QMAKE_FILE_BASE}.h /W1 /char signed /D "NDEBUG" /robust /nologo ${QMAKE_FILE_IN}
}
QMAKE_EXTRA_COMPILERS += MIDL
