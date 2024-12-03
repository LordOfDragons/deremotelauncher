#!/bin/bash
(cd launcher/desktop && scons -c) \
	&& (cd testing/src && scons -c) \
	&& scons build archive install -c
