#!/bin/bash

set -xe

tool=$1
name=${2:-1}

echo "Building dist package for tool ${tool} with name ${name}"

if [ -z "${TWINE_PASSWORD}" ]; then
	echo "Please set TWINE_PASSWORD to your password and retry"
	exit 1
fi

dir=$(mktemp -d)
cp -v tools/${tool}/pyproject.toml ${dir}
cp -v Licenses/gpl-2.0.txt ${dir}/LICENSE
readme="tools/${tool}/README.*"

cat ${readme} | sed -E 's/:(doc|ref):`.*`//; /sectionauthor/d; /toctree::/d' \
	> ${dir}/$(basename ${readme})

dest=${dir}/src/${name}
mkdir -p ${dest}
cp -v tools/$tool/{*.py,*.rst} ${dest}
pushd tools/${tool}
for subdir in $(find . -maxdepth 1 -type d | grep -vE "(__pycache__|home|usr|scratch|\.$|pyproject)"); do
	pathname="${dest}/${subdir}"
	echo "Copy ${pathname}"
	cp -a ${subdir} ${pathname}
done
popd
find ${dest} -name __pycache__ -type f -exec rm {} \;
find ${dest} -depth -name __pycache__ -exec rmdir {} \;
mkdir ${dir}/tests
cd ${dir}

python3 -m pip install --upgrade build
python3 -m pip install --upgrade twine
python3 -m build

echo "Uploading from ${dir}"
python3 -m twine upload --repository testpypi -u __token__ dist/*

echo "Completed build and upload of ${tool}"

# remove tmpdir
