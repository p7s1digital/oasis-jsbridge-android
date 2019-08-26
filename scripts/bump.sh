#!/usr/bin/env bash

#
# Copyright (c) ProsiebenSat1. Digital GmbH 2019.
#

#set -x # outputs every line of script, uncomment for debugging

# fail fast
set -o errexit # or set -e
# all variables must be set
set -o nounset # or set -u

declare SCRIPTS_PATH="$(cd "$(dirname "$BASH_SOURCE")" && pwd)"
declare PROJECT_PATH=$(cd "${SCRIPTS_PATH}/.."; pwd)
declare VERSION_FILE_NAME="${PROJECT_PATH}/version"
declare -a VERSION_PARAMS=[]
declare CHANGELOG_FILE_NAME="${PROJECT_PATH}/CHANGELOG.md"
declare PREV_VERSION=$(head -n 1 ${VERSION_FILE_NAME})
declare UPDATED_VERSION

function main {
    initVersionParams
    parseParams "$@"

    printPrevVersion
    # Final preparations
    updateVersionFile

    # TODO introduce Changelog
#    updateChangeLogFile
#    updateChangeLogTimestamp
    commitToGit

    setGitTag
    finishScript
}

function printUsage {
    echo "
 Bash script that encapsulates actions related to bumping of semantic version

 Usage:
 --major|-M)
   increments major version parameter, e.g X.*.*
 --minor|-m)
   increments minor version parameter, e.g *.X.*
 --patch|-p)
   increments patch version parameter, e.g *.*.X
 --version|-v)
   bump to specified version
 --current|-c)
   prints current version

 For more information please refer to
 https://confluence.glomex.com/display/PLYR/How+to+bump+up+Semantic+Version+for+Native+apps
   "
}

function parseParams {
    set +u
    local input_arguments="$@"
    local regex=\[0-9]+\.\[0-9]+\.\[0-9]+
    while [[ ${1:-} ]]; do
        case "$1" in
            help|-h|--help)
                printUsage
                exit 0
                ;;
            --major|-M)
                ((VERSION_PARAMS[0]++))
                VERSION_PARAMS[1]=0;
                VERSION_PARAMS[2]=0;
                setNewVersionFromParams
                ;;
            --minor|-m)
                ((VERSION_PARAMS[1]++))
                VERSION_PARAMS[2]=0;
                setNewVersionFromParams
                ;;
            --patch|-p)
                ((VERSION_PARAMS[2]++))
                setNewVersionFromParams
                ;;
            --version|-v)
                shift
                UPDATED_VERSION=$1
                ;;
            --current|-c)
                printPrevVersion
                exit 0
                ;;
            *)
            echo $1
                if [[ $1 =~ $regex ]]
                    then
                        UPDATED_VERSION=$1
                    else
                        echo "Unknown argument $1"
                        printUsage
                        exit 1
                fi
                ;;
        esac
        shift
    done
    set -u
}

function printPrevVersion {
    echo "Previous version: ${PREV_VERSION}"
}

function updateVersionFile {
    echo "Updating to version: ${UPDATED_VERSION}"
    echo -n ${UPDATED_VERSION} > ${VERSION_FILE_NAME}
}

function setNewVersionFromParams {
    UPDATED_VERSION="${VERSION_PARAMS[0]}.${VERSION_PARAMS[1]}.${VERSION_PARAMS[2]}"
}

function initVersionParams {
    VERSION_PARAMS=( ${PREV_VERSION//./ } )
}

function updateChangeLogFile {
    # Currently date format in CHANGELOG.md is "2000-01-01 12:00:00"
    # If it changes this regexp must be updated accordingly
    dateRegexp="[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}"
    while read line
    do
        # look for top changelog update date
        if [[ ${line} =~ $dateRegexp ]]; then
            lastChangelogUpdateDate="${BASH_REMATCH[0]}"
            lastChangelogUpdateDate="${lastChangelogUpdateDate/ /T}"    # replace space with "T"
            break
        fi

    done < ${CHANGELOG_FILE_NAME}

    if [[ ${lastChangelogUpdateDate} != "" ]]; then
        sh ${SCRIPTS_PATH}/updateChangelog.sh -d ${lastChangelogUpdateDate}
    else
        echo "CHANGELOG was not updated. See errors above. Please debug bump.sh script"
    fi
}

function updateChangeLogTimestamp {
    local now="$(date +'%Y-%m-%d %T')"
    local update="### v${UPDATED_VERSION} / ${now}"

    # prepend Changelog.md with bumped version
    echo -e "${update}" | cat - ${CHANGELOG_FILE_NAME} > /tmp/out &&
    mv /tmp/out ${CHANGELOG_FILE_NAME}
}

function commitToGit {
    local commitMessage="Upgrade to version ${UPDATED_VERSION}"
    git commit version -m "${commitMessage}"
}

function setGitTag {
    local tagName=${UPDATED_VERSION}
    git tag -a ${tagName} -m "Upgrade version to ${UPDATED_VERSION}"
}

function finishScript {
    echo "Bump script finished successfully. Please check if commits and tag is ok and do 'git push --follow-tags'"
}
main $@
