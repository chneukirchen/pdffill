TEMPLATE = app
TARGET = pdffill
INCLUDEPATH += .

CONFIG += link_pkgconfig
PKGCONFIG += poppler-qt5
QT += core

# Input
SOURCES += pdffill.cc
