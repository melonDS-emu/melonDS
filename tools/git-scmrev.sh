#!/bin/bash

REVISION=`git rev-parse HEAD`
if [ "$REVISION" == "" ] ; then
	REVISION="0"
fi

DESCRIBE=`git describe --always --long`
if [ "$DESCRIBE" == "" ] ; then
	DESCRIBE="0"
fi

BRANCH=`git rev-parse --abbrev-ref HEAD`
if [ "$BRANCH" == "" ] ; then
	BRANCH="unknown"
fi

ISSTABLE="0"
if [ "$BRANCH" = "master" ] || [ "$BRANCH" = "stable" ] ; then
	ISSTABLE="1"
fi

printf "// revision tracking\n\
// This file was generated automatically\n\
#define SOURCE_CONTROL_REVISION_STRING \"$REVISION\"\n\
#define SOURCE_CONTROL_DESC_STRING \"$DESCRIBE\"\n\
#define SOURCE_CONTROL_BRANCH_STRING \"$BRANCH\"\n\
#define SOURCE_CONTROL_IS_STABLE $ISSTABLE\n" > ./src/scmrev.h
