build_mingw32_SHA256SUM = shasum -a 256
build_mingw32_DOWNLOAD = wget --no-check-certificate --timeout=$(DOWNLOAD_CONNECT_TIMEOUT) --tries=$(DOWNLOAD_RETRIES) -nv -O
