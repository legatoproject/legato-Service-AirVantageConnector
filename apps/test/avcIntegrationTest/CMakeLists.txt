#*******************************************************************************
# Copyright (C) Sierra Wireless Inc.
#*******************************************************************************

mkapp(avcTest.adef)

# This is a C test
add_dependencies(avc_tests_c avcTest)
