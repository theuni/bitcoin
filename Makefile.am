ACLOCAL_AMFLAGS = -I src/m4
SUBDIRS = src
.PHONY: windows-deploy osx-deploy FORCE

GZIP_ENV="-9n"

BITCOIND_BIN=$(top_builddir)/src/bitcoind$(EXEEXT)
BITCOIN_QT_BIN=$(top_builddir)/src/qt/bitcoin-qt$(EXEEXT)
BITCOIN_WIN_INSTALLER=$(PACKAGE)-$(PACKAGE_VERSION)-win32-setup$(EXEEXT)

WINDOWS_PACKAGING = $(top_srcdir)/share/pixmaps/bitcoin.ico \
  $(top_srcdir)/share/pixmaps/nsis-header.bmp \
  $(top_srcdir)/share/pixmaps/nsis-wizard.bmp \
  $(top_srcdir)/doc/README_windows.txt

dist-hook:
	-$(MAKE) -C $(top_distdir)/src/leveldb clean
	-$(GIT) archive --format=tar HEAD -- src/version.cpp | $(AMTAR) -C $(top_distdir) -xf -

distcheck-hook:
	$(MKDIR_P) $(top_distdir)/_build/src/leveldb
	cp -rf $(top_srcdir)/src/leveldb/ $(top_distdir)/_build/src/
	-$(MAKE) -C $(top_distdir)/_build/src/leveldb clean

distcleancheck:
	@:

windows-deploy: $(BITCOIN_WIN_INSTALLER)

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


EXTRA_DIST = $(top_srcdir)/share/genbuild.sh $(WINDOWS_PACKAGING)
