# SPDX-License-Identifier: GPL-2.0+

from setuptools import setup
setup(name='u_boot_pylib',
      version='1.0',
      license='GPL-2.0+',
      dependencies = ['u_boot_pylib'],
      packages=['u_boot_pylib'],
      package_dir={'u_boot_pylib': ''},
      package_data={'u_boot_pylib': ['README.rst']},
      classifiers=['Topic :: Software Development'])
