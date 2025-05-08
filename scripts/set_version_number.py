#!/usr/bin/env python
# -*- coding: utf-8 -*-

import argparse
import os
import re

def replace_in_file(path, regex, replacement):
    regex = re.compile(regex)

    lines = []
    with open(path) as infile:
        for line in infile:
            lines.append(re.sub(regex, replacement, line))

    with open(path, 'w') as outfile:
        for line in lines:
            outfile.write(line)

def update_cpp_file(path, version):
    version_parts = version.split('.')

    replace_in_file(path, 'LIBLOOT_VERSION_MAJOR = \\d+;', 'LIBLOOT_VERSION_MAJOR = {};'.format(version_parts[0]))
    replace_in_file(path, 'LIBLOOT_VERSION_MINOR = \\d+;', 'LIBLOOT_VERSION_MINOR = {};'.format(version_parts[1]))
    replace_in_file(path, 'LIBLOOT_VERSION_PATCH = \\d+;', 'LIBLOOT_VERSION_PATCH = {};'.format(version_parts[2]))

def update_resource_file(path, version):
    comma_separated_version = version.replace('.', ', ')

    replace_in_file(path, 'VERSION \\d+, \\d+, \\d+', 'VERSION {}'.format(comma_separated_version))
    replace_in_file(path, 'Version", "\\d+\\.\\d+\\.\\d+"', 'Version", "{}"'.format(version))

def update_cmakelists(path, version):
    version_parts = version.split('.')

    replace_in_file(path, 'set\\(LIBLOOT_VERSION "\\d+\\.\\d+\\.\\d+"\\)', 'set(LIBLOOT_VERSION "{}")'.format(version))
    replace_in_file(path, 'TARGET loot PROPERTY SOVERSION \\d+', 'TARGET loot PROPERTY SOVERSION {}'.format(version_parts[0]))
    replace_in_file(path, 'INTERFACE_libloot_MAJOR_VERSION \\d+', 'INTERFACE_libloot_MAJOR_VERSION {}'.format(version_parts[0]))

def update_cargo_toml(path, version):
    replace_in_file(path, '^version = "\\d+\\.\\d+\\.\\d+"$', 'version = "{}"'.format(version))

def update_package_json(path, version):
    replace_in_file(path, '"version": "\\d+\\.\\d+\\.\\d+",', '"version": "{}",'.format(version))

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description = 'Set the libloot version number')
    parser.add_argument('version', nargs='+')

    arguments = parser.parse_args()

    if len(arguments.version) != 1:
        raise RuntimeError('Invalid number of arguments given. Only one argument (the new version number) is expected.')

    if len(arguments.version[0].split('.')) != 3:
        raise RuntimeError('The version number must be a three-part semantic version.')

    update_cpp_file(os.path.join('cpp', 'include', 'loot', 'loot_version.h'), arguments.version[0])
    update_resource_file(os.path.join('cpp', 'src', 'api', 'resource.rc'), arguments.version[0])
    update_cmakelists(os.path.join('cpp', 'CMakeLists.txt'), arguments.version[0])

    update_cargo_toml('Cargo.toml', arguments.version[0])
    update_cargo_toml(os.path.join('cpp', 'Cargo.toml'), arguments.version[0])
    update_cargo_toml(os.path.join('ffi-errors', 'Cargo.toml'), arguments.version[0])
    update_cargo_toml(os.path.join('nodejs', 'Cargo.toml'), arguments.version[0])
    update_cargo_toml(os.path.join('parameterized-test', 'Cargo.toml'), arguments.version[0])
    update_cargo_toml(os.path.join('python', 'Cargo.toml'), arguments.version[0])

    update_package_json(os.path.join('nodejs', 'package.json'), arguments.version[0])
    update_package_json(os.path.join('nodejs', 'npm', 'linux-x64-gnu', 'package.json'), arguments.version[0])
    update_package_json(os.path.join('nodejs', 'npm', 'win32-x64-msvc', 'package.json'), arguments.version[0])
