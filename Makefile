#*******************************************************************************
# Copyright (C) Sierra Wireless Inc.
#*******************************************************************************

TARGETS := ar759x wp85

TINYDTLS_DIR = ../../../3rdParty/Lwm2mCore/tinydtls

TINYDTLS_H_GENERATED = ${TINYDTLS_DIR}/tinydtls.h
DTLS_CONFIG_H_GENERATED = ${TINYDTLS_DIR}/dtls_config.h

.PHONY: all $(TARGETS)
all: $(TARGETS)

$(TARGETS): ${TINYDTLS_H_GENERATED} ${DTLS_CONFIG_H_GENERATED}

${DTLS_CONFIG_H_GENERATED}: ${TINYDTLS_H_GENERATED}
	cd ${TINYDTLS_DIR};    \
	ac_cv_func_malloc_0_nonnull=yes ./configure --host="$(shell uname -m)-linux"; \
	cd -;

${TINYDTLS_H_GENERATED}:
	cd ${TINYDTLS_DIR}; \
	autoreconf -i; \
	cd -;

clean:
	rm -rf _build_* *.update

