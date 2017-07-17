--- qtbase/src/plugins/platforms/cocoa/qprintengine_mac_p.h	2017-06-28 05:54:29.000000000 -0400
+++ qtbase/src/plugins/platforms/cocoa/qprintengine_mac_p.h	2017-07-16 23:44:19.604644664 -0400
@@ -52,7 +52,7 @@
 //
 
 #include <QtCore/qglobal.h>
-
+#include <QtPrintSupport/qtprintsupport-config.h>
 #ifndef QT_NO_PRINTER
 
 #include <QtPrintSupport/qprinter.h>
--- qtbase/src/plugins/printsupport/cocoa/main.cpp	2017-07-17 00:05:05.068600356 -0400
+++ qtbase/src/plugins/printsupport/cocoa/main.cpp	2017-07-17 00:06:23.220597575 -0400
@@ -42,6 +42,7 @@
 #include <qpa/qplatformnativeinterface.h>
 #include <qpa/qplatformprintplugin.h>
 
+#ifndef QT_NO_PRINTER
 QT_BEGIN_NAMESPACE
 
 class QCocoaPrinterSupportPlugin : public QPlatformPrinterSupportPlugin
@@ -74,3 +75,4 @@
 QT_END_NAMESPACE
 
 #include "main.moc"
+#endif
