#!/usr/bin/env bash

#
# Copyright (c) ProsiebenSat1. Digital GmbH 2019.
#

set -x # outputs every line of script, uncomment for debugging

# fail fast
# Travis does not work well with this flag, disabling for now.
set -o errexit # or set -e

# all variables must be set
set -o nounset # or set -u

export TARGET_RELEASE="release"
export TARGET_DEBUG="debug"
export MASTER="master"

declare PROJECT_PATH
declare SCRIPTS_PATH
declare ARTIFACTS_PATH
declare BUILD_TYPE

export needPrintVerbose=false
declare needClean=false

declare needQualityCheck=true
declare needRunUnitTests=true
declare needRunIntegrationTests=false
declare needInstallToMavenLocal=false

declare needGenerateJavadoc=true
declare needZipArtifacts=false

function main {
    MODULE_NAME="jsbridge"
    TESTS_MODULE_NAME="jsbridge"

	# get current script dir
	SCRIPTS_PATH="$(cd "$(dirname "$BASH_SOURCE")" && pwd)"

	source ${SCRIPTS_PATH}/utils.sh

	PROJECT_PATH=$(cd "${SCRIPTS_PATH}/.."; pwd)
	ARTIFACTS_PATH="$PROJECT_PATH/artifacts"

	pushd ${PROJECT_PATH} > /dev/null

	parseParams "$@"
	validateEnvironment

	if [[ ${needClean} == true ]]; then
		performClean
	fi

	performBuild ${BUILD_TYPE}

	if [[ ${needQualityCheck} == true ]]; then
		performQualityCheck
	fi

	if [[ ${needRunUnitTests} == true ]]; then
		runUnitTests
	fi

	if [[ ${needRunIntegrationTests} == true ]]; then
		runIntegrationTests
	fi

	if [[ ${needGenerateJavadoc} == true ]]; then
		generateJavaDoc ${BUILD_TYPE} "$ARTIFACTS_PATH"
	fi

	if [[ ${needZipArtifacts} == true ]]; then
		zipArtifacts "$ARTIFACTS_PATH"
	fi

	if [[ ${needInstallToMavenLocal} == true ]]; then
		installToMavenLocal
	fi

	publishArtifacts

	popd > /dev/null

	exit 0
}

# parameters: $1 build type
function performBuild {
	local type=$1
	echo_colored "Performing ${type} build"

	pushd ${PROJECT_PATH} > /dev/null
	local assembleTask

	if [[ ${type} == ${TARGET_RELEASE} ]]; then
		assembleTask="assembleRelease"
	elif [[ ${type} == ${TARGET_DEBUG} ]]; then
		assembleTask="assembleDebug"
	else
		echo "Build type is not specified. Default is DEBUG"
		#set build type to TARGET_DEBUG by default
		assembleTask="assembleDebug"
	fi

	# "-p MODULE_NAME" builds MODULE_NAME only
	./gradlew ${assembleTask}
	#./gradlew ${assembleTask} -p ${MODULE_NAME}

#	local output_path="${PROJECT_PATH}/${MODULE_NAME}/build/outputs/aar/."
#	local dest_path="$ARTIFACTS_PATH/"
#	cp -r "$output_path" "$dest_path"

	popd > /dev/null
}

function performQualityCheck {
	echo_colored "Performing quality check"
	pushd ${PROJECT_PATH} > /dev/null

	# `check` task includes `lint` and `findBugs`
	# see "quality.gradle" file for more details
	./gradlew check

	#popd > /dev/null
	#local source_path="${PROJECT_PATH}/build/reports/."
	#local dest_path="$ARTIFACTS_PATH"

	#copy_output "$source_path" "$dest_path"
}

function runUnitTests {
	echo_colored "Running unit tests"
	pushd ${PROJECT_PATH} > /dev/null

    ./gradlew test

	popd > /dev/null
}

function runIntegrationTests {
	echo_colored "Running integration tests. Make sure real device or emulator is connected."
	pushd ${PROJECT_PATH} > /dev/null

	./gradlew :${TESTS_MODULE_NAME}:connectedAndroidTest

	popd > /dev/null
}

# $1 - input files
function zipArtifacts {
	echo_colored "Zipping artifacts"
	pushd ${ARTIFACTS_PATH} > /dev/null

	if [[ -d "$1" ]]; then
		zip -r "artifacts.zip" ./
		echo "artifacts.zip archive created"
	fi

	popd > /dev/null
}

function performClean {
	echo_colored "Cleaning project"
	pushd ${PROJECT_PATH} > /dev/null

	# clean project
	./gradlew clean

	# remove output folders
	rm -rf "$ARTIFACTS_PATH"
	rm -rf "$PROJECT_PATH/${MODULE_NAME}/build"

	popd > /dev/null
}

function generateJavaDoc {
	echo_colored "Generating javadoc"
	local buildType=$1
	local docOutPath=$2

	pushd ${PROJECT_PATH} > /dev/null

	if [[ ${buildType} == ${TARGET_RELEASE} ]]; then
		./gradlew generateDuktapeReleaseJavadocJar
	elif [[ ${buildType} == ${TARGET_DEBUG} ]]; then
		./gradlew generateDuktapeDebugJavadocJar
	else
		echo "Build type is missing"
	fi

	popd > /dev/null
}

function installToMavenLocal {
	echo_colored "Installing to Maven local repository"

	./gradlew publishToMavenLocal
}

function publishArtifacts {
	set +u

	# Trigger artifacts publish only when building for TAG, which means we want to create a release,
	# otherwise, when we open PR from develop into master, it will build old version and publish it.

	if [[ -z ${CI} ]]; then
		echo "No CI environment detected -> not publishing. Comment me out in 'makeProject.sh' if you need to publish from local machine."
		return 0
	fi
	if [[ ! ${CI_COMMIT_REF_NAME} == ${GIT_TAG} ]]; then
		echo "Not building from a tag => not publishing to S3"
		return 0
 	fi
 
	set -u

	echo_colored "Publishing artifacts to S3"

	./gradlew publish
}

function parseParams {
	local input_arguments="$@"

	# About ${1:-} see this - http://stackoverflow.com/questions/7832080/test-if-a-variable-is-set-in-bash-when-using-set-o-nounset#comment9608281_7832158
	while [[ ${1:-} ]]; do
		case "$1" in
			help|-h|--help)
				print_usage
				exit 0
				;;
			-r|--release|release)
				BUILD_TYPE=${TARGET_RELEASE}
				;;
			-d|--debug|debug)
				BUILD_TYPE=${TARGET_DEBUG}
				;;
			-c|--clean|clean)
				needClean=true
				;;
			-v|--verbose)
				needPrintVerbose=true
				;;
			-t|--test-integration)
				needRunIntegrationTests=true
				;;
			-i|--install)
				needInstallToMavenLocal=true
				;;
			*)
				echo "Unknown argument $1"
				print_usage
				;;
		esac
		shift
	done

	print_verbose "Input arguments : ${input_arguments}"
	print_verbose "Project path: ${PROJECT_PATH}"

	if [[ -z ${BUILD_TYPE} ]]; then
		echo "build type required"
		exit 3
	fi
}

function print_usage {
	echo "
	Args:
	-r|--release|release)
		create Release build

	-d|--debug|debug)
		create Debug build

	-c|--clean)
		clean project

	-t|--test-integration)
		run Integration tests (on real device or Genymotion emulator)

	-i|--install)
		install artifact to maven local repository

	-v|--verbose)
		enable verbose logging

	-h|--help|help)
		print this help
	"
}

function validateEnvironment {

	function checkAndroidSdkInstalled {
	    local needInstallTools
#		local DEFAULT_SDK_HOME="~/Library/Android/sdk"
#		local tryDefault=false

		# a way to go with possibly unset envvars
		export ANDROID_SDK_HOME=${ANDROID_SDK_HOME:-}
		export ANDROID_HOME=${ANDROID_HOME:-}

		local sdkRoot=""

		if [[ ! -z ${ANDROID_SDK_HOME} ]]; then
			sdkRoot=${ANDROID_SDK_HOME}
			print_verbose "Android SDK found at: ${sdkRoot}"
		elif [[ ! -z ${ANDROID_HOME} ]]; then
			sdkRoot=${ANDROID_HOME}
			print_verbose "Android SDK found at: ${sdkRoot}"
		else
			echo "Android SDK path not found. Please set ANDROID_SDK_HOME or ANDROID_HOME env var to the root of the Android SDK." >&2
#			tryDefault=true
#			sdkRoot=${DEFAULT_SDK_HOME}
#			print_verbose "Android SDK not found in envvars ANDROID_SDK_HOME or ANDROID_HOME. Trying default location: ${sdkRoot}"
		fi

		# resolve ~ in relative home path
		# NOTE: we can't do this with `sdkRoot="$(cd "$sdkRoot" && pwd)"`
		# because directory might not yet exist
		if [[ ${sdkRoot:0:1} == "~" ]];then
			sdkRoot="${HOME}${sdkRoot:1}"
		fi
	}

	checkAndroidSdkInstalled
}

main $@
set +ex
