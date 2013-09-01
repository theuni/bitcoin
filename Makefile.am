ACLOCAL_AMFLAGS = -I src/m4
SUBDIRS = src
.PHONY: deploy

GZIP_ENV="-9n"

BITCOIND_BIN=$(top_builddir)/src/bitcoind$(EXEEXT)
BITCOIN_QT_BIN=$(top_builddir)/src/qt/bitcoin-qt$(EXEEXT)
BITCOIN_WIN_INSTALLER=$(PACKAGE)-$(PACKAGE_VERSION)-win32-setup$(EXEEXT)

OSX_APP=Bitcoin-Qt.app
OSX_DMG=Bitcoin-Qt.dmg
OSX_DEPLOY_SCRIPT=$(top_srcdir)/contrib/macdeploy/macdeployqtplus
OSX_FANCY_PLIST=$(top_srcdir)/contrib/macdeploy/fancy.plist
OSX_INSTALLER_ICONS=$(top_srcdir)/src/qt/res/icons/bitcoin.icns
OSX_PLIST=$(top_srcdir)/share/qt/Info.plist #not installed

WINDOWS_PACKAGING = $(top_srcdir)/share/pixmaps/bitcoin.ico \
  $(top_srcdir)/share/pixmaps/nsis-header.bmp \
  $(top_srcdir)/share/pixmaps/nsis-wizard.bmp \
  $(top_srcdir)/doc/README_windows.txt

OSX_PACKAGING = $(OSX_DEPLOY_SCRIPT) $(OSX_FANCY_PLIST) $(OSX_INSTALLER_ICONS) \
  $(top_srcdir)/contrib/macdeploy/background.png

dist-hook:
	-$(MAKE) -C $(top_distdir)/src/leveldb clean
	-$(GIT) archive --format=tar HEAD -- src/version.cpp | $(AMTAR) -C $(top_distdir) -xf -

distcheck-hook:
	$(MKDIR_P) $(top_distdir)/_build/src/leveldb
	cp -rf $(top_srcdir)/src/leveldb/* $(top_distdir)/_build/src/leveldb/
	-$(MAKE) -C $(top_distdir)/_build/src/leveldb clean

distcleancheck:
	@:

$(BITCOIN_WIN_INSTALLER): $(BITCOIND_BIN) $(BITCOIN_QT_BIN)
	$(MAKE) distdir
	$(MKDIR_P) $(top_builddir)/release
	$(INSTALL_STRIP_PROGRAM) $(top_builddir)/src/bitcoind$(EXEEXT) $(top_builddir)/release
	$(INSTALL_STRIP_PROGRAM) $(top_builddir)/src/qt/bitcoin-qt$(EXEEXT) $(top_builddir)/release
	@test -f $(MAKENSIS) && $(MAKENSIS) $(top_builddir)/share/setup.nsi || \
	  echo error: could not build $@

$(BITCOIND_BIN):
	make -C $(top_srcdir)/src bitcoind$(EXEEXT)

$(BITCOIN_QT_BIN):
	make -C $(top_srcdir)/src/qt bitcoin-qt$(EXEEXT)


$(OSX_APP)/Contents/PkgInfo:
	$(MKDIR_P) $(@D)
	@echo "APPL????" > $@

$(OSX_APP)/Contents/Resources/empty.lproj:
	$(MKDIR_P) $(@D)
	@touch $@

$(OSX_APP)/Contents/Info.plist: $(OSX_PLIST)
	$(MKDIR_P) $(@D)
	$(INSTALL) $< $@

$(OSX_APP)/Contents/Resources/bitcoin.icns: $(OSX_INSTALLER_ICONS)
	$(MKDIR_P) $(@D)
	$(INSTALL) $< $@

$(OSX_APP)/Contents/MacOS/Bitcoin-Qt: $(BITCOIN_QT_BIN)
	$(MKDIR_P) $(@D)
	$(INSTALL_STRIP_PROGRAM)  $< $@

OSX_APP_BUILT=$(OSX_APP)/Contents/PkgInfo $(OSX_APP)/Contents/Resources/empty.lproj $(OSX_APP)/Contents/Info.plist $(OSX_APP)/Contents/MacOS/Bitcoin-Qt

$(OSX_DMG): $(OSX_APP_BUILT) $(OSX_PACKAGING)
	$(OSX_DEPLOY_SCRIPT) $(OSX_APP) -add-qt-tr da,de,es,hu,ru,uk,zh_CN,zh_TW -dmg -fancy $(OSX_FANCY_PLIST) -verbose 2
	rm -rf $(OSX_APP)

.INTERMEDIATE: $(OSX_APP_BUILT)

if TARGET_DARWIN
deploy: $(OSX_DMG)
endif
if TARGET_WINDOWS
deploy: $(BITCOIN_WIN_INSTALLER)
endif

EXTRA_DIST = $(top_srcdir)/share/genbuild.sh $(WINDOWS_PACKAGING) $(OSX_PACKAGING)

CLEAN_FILES = $(OSX_DMG) $(BITCOIN_WIN_INSTALLER)
