#*******************************************************************************
# Copyright (C) Sierra Wireless Inc.
#*******************************************************************************

mkapp(coapHandlerTest.adef)

# This is a C test
add_dependencies(avc_tests_c coapHandlerTest)
