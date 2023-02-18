#!/bin/bash

set -xe

tool=$1

echo "Building dist package for tool ${tool}"

if [ -z "${TWINE_PASSWORD}" ]; then
	echo "Please set TWINE_PASSWORD to your password and retry"
	exit 1
fi

dir=$(mktemp -d)
cp -v tools/${tool}/{LICENSE,pyproject.toml,README.md} ${dir}

mkdir -p ${dir}/src/${tool}
cp -v tools/$tool/*.py ${dir}/src/${tool}
mkdir ${dir}/tests
cd ${dir}

python3 -m pip install --upgrade build
python3 -m pip install --upgrade twine
python3 -m build
python3 -m twine upload --repository testpypi -u __token__ dist/*

echo "Output in ${dir}"
