#!/bin/bash

mkdir tmp
cd tmp
git clone https://github.com/bwalter/oasis-jni-helpers
cd oasis-jni-helpers
git checkout $1
rm -rf .git
cp src/* ..
cd ../..
rm -r tmp

rm VERSION
echo $1 > VERSION
