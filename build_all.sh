#!/bin/bash
scons build archive install \
	&& (cd testing/src && scons)
