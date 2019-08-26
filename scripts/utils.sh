#!/usr/bin/env bash

#
# Copyright (c) ProsiebenSat1. Digital GmbH 2019.
#

# Use colors, but only if connected to a terminal, and that terminal
# supports them. Travis doesn't support tput and will fail if it is called
tput=$(which tput)
ncolors=""
if [[ -n "$tput" ]] && [[ -n "$TERM" ]] && [[ "$TERM" != "dumb" ]]; then
    echo "ncolors"
    ncolors=$($tput colors)
fi

if [ -t 1 ] && [ -n "$ncolors" ] && [ "$ncolors" -ge 8 ]; then
	fg_blue="$(tput setaf 3)"
	bg_white="$(tput setab 4)"
	bold="$(tput bold)"
	underline="$(tput smul)"
	reset="$(tput sgr0)"
else
	fg_blue=""
	bg_white=""
	bold=""
	underline=""
	reset=""
fi

declare needPrintVerbose=false
function echo_colored {
    echo ""
    echo "${fg_blue}${bg_white}${bold}"$1"${reset}"
}

function get_script_path {

	mydir=$(dirname $0)
	if [ "${mydir}" = "." ]; then        # ./setup.sh
	    mydir=$(pwd)
	elif [ "${mydir:0:1}" = "/" ]; then  # /Jenkins/job/project/tools/setup.sh
	    mydir=$(dirname $0)
	elif [ "${mydir:0:1}" != "/" ]; then # ./tools/setup.sh or tools/setup.sh
	    mydir="$(pwd)/${mydir}"
	fi

	WORKSPACE_ROOT_DIR=${mydir}
	echo ${WORKSPACE_ROOT_DIR}
}

#sourcePath=$1, destPath=$2, moveOutput=$3 (copy by default)
function copy_output {
	print_verbose "$@"

	local sourcePath=${1:-}
	local destPath=${2:-}
	local moveOutput=${3:-}

	if [ ! -d "$destPath" ]; then
		mkdir -p "$destPath"
	fi

#	if [[ -d $source_path || -f $source_path ]]; then
		if [ "$moveOutput" == "true" ]; then
			mv "$sourcePath" "$destPath"
		else
			cp -r "$sourcePath" "$destPath"
		fi
	# else
	# 	echo "Report hasn't been generated, looks like a bug in script"
	# 	echo "Folder $source_path not exist"
	# 	print_error_and_exit 5
	# fi
}

function print_verbose {
	if [ ${needPrintVerbose} == true ]; then
		echo $@
	fi
}
