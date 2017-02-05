include $(TOPDIR)/rules.mk

# Name and release number of this package
PKG_NAME:=mplayer
PKG_RELEASE:=1.0-rc2

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

# Specify package information for this program.
# The variables defined here should be self explanatory.
# If you are running Kamikaze, delete the DESCRIPTION
# variable below and uncomment the Kamikaze define
# directive for the description below
define Package/mplayer
	SECTION:=utils
	CATEGORY:=Utilities
#        DEPENDS:=+libpthread +librt +libasound +libjpeg +liblzo2 +libmad +libncurses +libz +libc
        DEPENDS:=+libpthread +libjpeg +libncurses +libmad +libc
	TITLE:=mplayer with wma fixed-point decoder
endef


# Uncomment portion below for Kamikaze and delete DESCRIPTION variable above
define Package/mplayer/description
endef

 

# Specify what needs to be done to prepare for building the package.
# In our case, we need to copy the source files to the build directory.
# This is NOT the default.  The default uses the PKG_SOURCE_URL and the
# PKG_SOURCE which is not defined here to download the source from the web.
# In order to just build a simple program that we have just written, it is
# much easier to do it this way.
define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef


define Package/mplayer/install
	$(INSTALL_DIR) $(1)/bin
	mkdir -p $(1)/usr/lib
	$(INSTALL_BIN) /home/midas/openwrt_widora/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2/usr/lib/liblzo2.so.2.0.0 $(1)/usr/lib/
	ln -fs /usr/lib/liblzo2.so.2.0.0 $(1)/usr/lib/liblzo2.so.2
	ln -fs /usr/lib/liblzo2.so.2.0.0 $(1)/usr/lib/liblzo2.so.2.0
	mkdir  $(1)/etc
	$(INSTALL_BIN) /home/midas/openwrt_widora/package/mplayer/src/asound.conf $(1)/etc/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mplayer $(1)/bin/
endef


# This line executes the necessary commands to compile our program.
# The above define directives specify all the information needed, but this
# line calls BuildPackage which in turn actually uses this information to
# build a package.
$(eval $(call BuildPackage,mplayer))
