#!/bin/bash
(cd launcher/linux && scons -c) \
	&& (cd testing/src && scons -c) \
	&& scons build archive install -c
