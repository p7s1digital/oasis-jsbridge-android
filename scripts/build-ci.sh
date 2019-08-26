#!/bin/sh

#
# Copyright (c) ProsiebenSat1. Digital GmbH 2019.
#

source $HOME/.profile

setTagVariableIfOnTag() {
    unset GIT_TAG
    # `--tags` allows to detect lightweight tags (vs annotated tags by default)
    tag="$(git describe --exact-match --tags HEAD 2>/dev/null)"
    export GIT_TAG=${tag}
}

# TRAVIS_PULL_REQUEST - PR number if the current job is a PR, “false” if it’s not a PR.
# TRAVIS_BRANCH - for builds not triggered by a PR this is the name of the branch currently being built;
# whereas for builds triggered by a PR this is the name of the branch targeted by the PR.
setTagVariableIfOnTag
echo "GIT tag is ${GIT_TAG}"
echo "CI ref is ${CI_COMMIT_REF_NAME}"

if [[ ${CI_COMMIT_REF_NAME} == ${GIT_TAG} ]]; then
	# This is a tag => create a release build
	echo "build in release mode"

	projectVersion="$(cat ./version | xargs echo -n)" # stripped
	echo "Project version is ${projectVersion}"

	if [[ ${projectVersion} != ${GIT_TAG} ]]; then
		echo "The framework version does not match the tag!"
		exit 1
	fi

    export S3_BUCKET=${AWS_S3_NATIVE_PLAYER_BUCKET}
    export S3_PATH="${AWS_S3_NATIVE_PLAYER_COMPONENT_PATH}/oasis-native-android"
	export S3_ACCESS_KEY=${AWS_ACCESS_KEY_ID}
	export S3_SECRET_KEY=${AWS_SECRET_ACCESS_KEY}

	if [ -z ${S3_ACCESS_KEY} ]; then
	    echo "S3_ACCESS_KEY envvar is empty. Exiting the script."
	    exit 1
	fi

	if [ -z ${S3_SECRET_KEY} ]; then
	    echo "S3_SECRET_KEY envvar is empty. Exiting the script."
	    exit 1
	fi

  ./scripts/makeProject.sh --release #--clean
else
  # This is a branch => create a debug build
  echo "build in debug mode"

  # There is no deploy to S3 in debug mode. AAR artifact is stored in Gitlab CI pipeline

  ./scripts/makeProject.sh --debug #--clean
fi
