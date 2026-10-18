#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
#
# Print the minimum supported version of the given tool.

# When you raise the minimum version, please update
# Documentation/process/changes.rst as well.

binutils_min=2.23
gcc_min=4.9
icc_min=16.0.3
llvm_min=10.0.1

case "$1" in
binutils)
	echo $binutils_min
	;;
gcc)
	echo $gcc_min
	;;
icc)
	echo $icc_min
	;;
llvm)
	echo $llvm_min
	;;
esac
