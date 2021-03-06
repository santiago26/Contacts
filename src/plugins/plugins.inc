include(../config.inc)
include(../install.inc)

TEMPLATE            = lib
CONFIG             += plugin
QT                 += network xml
macx:QT            += webkit
LIBS               += -L../../libs
LIBS               += -l$$TARGET_UTILS
DEPENDPATH         += ../..
INCLUDEPATH        += ../..
win32 {
  DLLDESTDIR        = ..\\..\\..\\plugins
  QMAKE_DISTCLEAN  += $${DLLDESTDIR}\\$${TARGET}.dll
}
unix:DESTDIR        = ../../../plugins

#Windows resources
win32:RC_FILE       = plugin_resources.rc

#Translation
TRANS_SOURCE_ROOT   = ../..
include(../translations.inc)

#Install
target.path         = $$INSTALL_PLUGINS
INSTALLS           += target

#MaxOS Install
macx {
  name_tool.path    = $$INSTALL_PLUGINS
  name_tool.extra   = install_name_tool -change lib$${TARGET_UTILS}.1.dylib @executable_path/../Frameworks/lib$${TARGET_UTILS}.1.dylib $(INSTALL_ROOT)$$INSTALL_PLUGINS/lib$${TARGET}.dylib
  INSTALLS         += name_tool
}
