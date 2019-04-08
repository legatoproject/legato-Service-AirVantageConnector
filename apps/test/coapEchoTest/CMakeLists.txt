#*******************************************************************************
# Copyright (C) Sierra Wireless Inc.
#*******************************************************************************

mkapp(coapEchoTest.adef)

# This is a C test
add_dependencies(avc_tests_c coapEchoTest)
