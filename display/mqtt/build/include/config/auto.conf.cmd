deps_config := \
	/home/conor/esp/esp-idf/components/app_trace/Kconfig \
	/home/conor/esp/esp-idf/components/aws_iot/Kconfig \
	/home/conor/esp/esp-idf/components/bt/Kconfig \
	/home/conor/esp/esp-idf/components/driver/Kconfig \
	/home/conor/esp/esp-idf/components/esp32/Kconfig \
	/home/conor/esp/esp-idf/components/esp_adc_cal/Kconfig \
	/home/conor/esp/esp-idf/components/esp_event/Kconfig \
	/home/conor/esp/esp-idf/components/esp_http_client/Kconfig \
	/home/conor/esp/esp-idf/components/esp_http_server/Kconfig \
	/home/conor/esp/esp-idf/components/esp_https_ota/Kconfig \
	/home/conor/esp/esp-idf/components/ethernet/Kconfig \
	/home/conor/esp/esp-idf/components/fatfs/Kconfig \
	/home/conor/esp/esp-idf/components/freemodbus/Kconfig \
	/home/conor/esp/esp-idf/components/freertos/Kconfig \
	/home/conor/esp/esp-idf/components/heap/Kconfig \
	/home/conor/esp/esp-idf/components/libsodium/Kconfig \
	/home/conor/esp/esp-idf/components/log/Kconfig \
	/home/conor/esp/esp-idf/components/lwip/Kconfig \
	/home/conor/esp/esp-idf/components/mbedtls/Kconfig \
	/home/conor/esp/esp-idf/components/mdns/Kconfig \
	/home/conor/esp/esp-idf/components/mqtt/Kconfig \
	/home/conor/esp/esp-idf/components/nvs_flash/Kconfig \
	/home/conor/esp/esp-idf/components/openssl/Kconfig \
	/home/conor/esp/esp-idf/components/pthread/Kconfig \
	/home/conor/esp/esp-idf/components/spi_flash/Kconfig \
	/home/conor/esp/esp-idf/components/spiffs/Kconfig \
	/home/conor/esp/esp-idf/components/tcpip_adapter/Kconfig \
	/home/conor/esp/esp-idf/components/unity/Kconfig \
	/home/conor/esp/esp-idf/components/vfs/Kconfig \
	/home/conor/esp/esp-idf/components/wear_levelling/Kconfig \
	/home/conor/esp/esp-idf/components/app_update/Kconfig.projbuild \
	/home/conor/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/home/conor/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/home/conor/esp/i2c_self_test/main/Kconfig.projbuild \
	/home/conor/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/home/conor/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(IDF_TARGET)" "esp32"
include/config/auto.conf: FORCE
endif
ifneq "$(IDF_CMAKE)" "n"
include/config/auto.conf: FORCE
endif

$(deps_config): ;
