#!/bin/bash
(cd testing/src && scons -c) \
	&& scons build archive install -c
