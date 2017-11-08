#*******************************************************************************
# Copyright (C) Sierra Wireless Inc.
#*******************************************************************************

mkapp(assetDataTest.adef)

# This is a C test
add_dependencies(avc_tests_c assetDataTest)
