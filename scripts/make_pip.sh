#!/bin/bash

set -xe

tool=$1

echo "Building dist package for tool ${tool}"

if [ -z "${TWINE_PASSWORD}" ]; then
	echo "Please set TWINE_PASSWORD to your password and retry"
	exit 1
fi

dir=$(mktemp -d)
cp -v tools/${tool}/pyproject.toml ${dir}
cp -v Licenses/gpl-2.0.txt ${dir}/LICENSE
readme="tools/${tool}/README.*"

cat ${readme} | sed 's/:doc:`.*`//; /sectionauthor/d' > ${dir}/$(basename ${readme})

mkdir -p ${dir}/src/${tool}
cp -v tools/$tool/*.py ${dir}/src/${tool}
for subdir in $(find tools/${tool} -type d -maxdepth 1 | grep -v __pycache__); do
	echo "copy ${subdir}"
done
mkdir ${dir}/tests
cd ${dir}

python3 -m pip install --upgrade build
python3 -m pip install --upgrade twine
python3 -m build
python3 -m twine upload --repository testpypi -u __token__ dist/*

echo "Completed build and upload of ${tool}"
echo
