#!/usr/bin/env python
# -*- coding: utf-8 -*-

import datetime
import json
import os
import re
import sys
import subprocess

from urllib.request import urlopen

SKIPPABLE_NOTICE_LICENSES = [
    # CC0 and Unlicense licenses don't require license redistribution.
    'CC0-1.0',
    'Unlicense',
    # Zlib and MPL licenses don't need to be included in binary distributions.
    'Zlib',
    'MPL-2.0'
]
SKIPPABLE_LICENSE_FILES = SKIPPABLE_NOTICE_LICENSES + [
    # Skip GPL v3 because it's already present as libloot's own license.
    'GPL-3.0',
    'GPL-3.0-only',
    'GPL-3.0-or-later'
]
# Don't include notices for libloot's own crates.
SKIPPABLE_CRATES = [
        'libloot',
        'libloot-ffi-errors',
]
# Notice extraction is not flawless and may fail to find notices, so I've
# verified that these have no notices.
NO_NOTICES_WHITELIST = [
    ('allocator-api2',  '0.2.21'),
    ('cxx',             '1.0.161'),
    ('cxxbridge-macro', '1.0.161'),
    ('delegate',        '0.13.4'),
    ('link-cplusplus',  '1.0.10'),
    ('once_cell',       '1.21.3'),
    ('option-ext',      '0.2.0'),
    ('pelite-macros',   '0.1.1'),
    ('pest',            '2.8.0'),
    ('proc-macro2',     '1.0.101'),
    ('quote',           '1.0.40'),
    ('rustc-hash',      '2.1.1'),
    ('rustversion',     '1.0.20'),
    ('serde',           '1.0.219'),
    ('serde_derive',    '1.0.219'),
    ('syn',             '2.0.106'),
    ('thiserror',       '2.0.12'),
    ('thiserror-impl',  '2.0.12'),
]

YEAR_REGEX = re.compile(r'\d{4}')

def recurse_dependencies(packages, current_package, dependency_names):
    for dependency in current_package['dependencies']:
        name = dependency['name']
        if name in dependency_names or dependency['kind'] in ['dev', 'build'] or dependency['target'] == 'cfg(target_os = \"redox\")':
            continue

        dependency_names.add(name)
        for package in packages:
            if package['name'] == name:
                recurse_dependencies(packages, package, dependency_names)

def get_package_dependencies(cargo_toml_path, package_name):
    result = subprocess.run(
        [
            'cargo',
            'metadata',
            '--manifest-path',
            cargo_toml_path,
            '--filter-platform',
            'x86_64-pc-windows-msvc',
            '--filter-platform',
            'x86_64-unknown-linux-gnu',
            '--format-version',
            '1'
        ],
        capture_output=True,
        text=True,
        check=True
    )

    json_output = json.loads(result.stdout[:-1])
    packages = json_output['packages']

    dependency_names = set()
    for package in packages:
        if package['name'] == package_name:
            recurse_dependencies(packages, package, dependency_names)

    dependencies = []
    for package in packages:
        if package['name'] in dependency_names:
            dependencies.append({
                'name': package['name'],
                'version': package['version'],
                'license': package['license'],
                'manifest_path': package['manifest_path'],
            })

    return dependencies

def get_url(dependency):
    return f'https://crates.io/crates/{dependency['name']}/{dependency['version']}'

def parse_licenses(dependency):
    # This is not robust, but it's good enough with a couple of specific workarounds.

    if 'license' not in dependency:
        print(f'Found dependency with no license: {dependency['name']}', file=sys.stderr)
        exit(1)

    # Fix non-SPDX expression
    if dependency['license'] == 'MIT/Apache-2.0':
        expression = 'MIT OR Apache-2.0'

    # Preselect a license to simplify a couple of expressions.
    elif dependency['license'] == '(Apache-2.0 OR MIT) AND BSD-3-Clause':
        expression = 'MIT AND BSD-3-Clause'

    elif dependency['license'] == '(MIT OR Apache-2.0) AND Unicode-3.0':
        expression = 'MIT AND Unicode-3.0'

    else:
        expression = dependency['license']

    licenses_any = list(map(str.strip, expression.split('OR')))

    # If it's possible to pick a license that doesn't need a notice, pick it.
    for license in licenses_any:
        licenses_all = list(map(str.strip, license.split('AND')))
        if all(license in SKIPPABLE_NOTICE_LICENSES for license in licenses_all):
            return licenses_all

    # MIT OR Apache-2.0 is a common combination, MIT also appears on its own.
    if 'MIT' in licenses_any:
        return ['MIT']

    return list(map(str.strip, licenses_any[0].split('AND')))

def download_licenses(dependencies, output_directory):
    unique_licenses = set()
    for dependency in dependencies:
        licenses = parse_licenses(dependency)
        unique_licenses.update(licenses)

    # These three all have the same license text.
    if 'GPL-3.0-or-later' in unique_licenses:
        if 'GPL-3.0' in unique_licenses:
            unique_licenses.remove('GPL-3.0')

        if 'GPL-3.0-only' in unique_licenses:
            unique_licenses.remove('GPL-3.0-only')

    for license in unique_licenses:
        if license in SKIPPABLE_LICENSE_FILES:
            continue

        license_url = f'https://raw.githubusercontent.com/spdx/license-list-data/refs/heads/main/text/{license}.txt'

        print(f'Downloading {license} from {license_url}...')

        response = urlopen(license_url)

        if response.status != 200:
            print(f'Error fetching license: {response.status} {response.reason}, {response.read()}', file=sys.stderr)
            exit(1)

        with open(os.path.join(output_directory, f'{license}'), 'wb') as outfile:
            outfile.write(response.read())

def read_notices_from_file(file_path):
    notices = []
    with open(file_path, 'r', encoding='utf-8') as file:
        for line in file:
            lowercase_line = line.lower()
            if 'copyright' in lowercase_line and ('(c)' in lowercase_line or '©' in line or re.search(YEAR_REGEX, line)):
                notices.append(line.strip())

    return notices

def read_notices(dependency):
    # First hardcode a couple of notices that get mangled by the code below.
    if dependency['name'] == 'hashlink':
        return ['Copyright (c) 2015 The Rust Project Developers']
    elif dependency['name'] == 'libloadorder':
        return ['Copyright (C) 2017 Oliver Hamlet']
    elif dependency['name'] == 'esplugin':
        return ['Copyright (C) 2017 Oliver Hamlet']

    crate_path = os.path.dirname(dependency['manifest_path'])

    notices = []

    for entry in os.scandir(crate_path):
        lowercase_filename = entry.name.lower()
        if 'license' in lowercase_filename or 'copying' in lowercase_filename:
            if entry.is_file():
                notices.extend(read_notices_from_file(entry.path))
            elif entry.is_dir():
                for entry in os.scandir(entry.path):
                    if entry.is_file():
                        notices.extend(read_notices_from_file(entry.path))

    notices = list(set(notices))
    notices.sort()

    return notices

if __name__ == "__main__":
    target_package_name = 'libloot-cpp'
    cargo_toml_path = os.path.join('..', 'cpp', 'Cargo.toml')
    output_dir = os.path.join('api', 'copyright')

    package_dependencies = get_package_dependencies(cargo_toml_path, target_package_name)

    download_licenses(package_dependencies, os.path.join(output_dir, 'licenses'))

    notices_rst = f'.. This file was generated by scripts/licenses.py at {datetime.datetime.now().isoformat()}.\n\n'

    heading = 'Dependency Copyright Notices'
    notices_rst += heading
    notices_rst += '\n'
    notices_rst += '=' * len(heading)
    notices_rst += '\n\n'

    for dependency in package_dependencies:
        if dependency['name'] in SKIPPABLE_CRATES:
            continue

        if all(license in SKIPPABLE_NOTICE_LICENSES for license in parse_licenses(dependency)):
            print(f'Skipping {dependency['name']} because its license allows it: {dependency['license']}')
            continue

        notices = read_notices(dependency)

        if not notices:
            if (dependency['name'], dependency['version']) not in NO_NOTICES_WHITELIST:
                print(f'Found dependency with no notices: {dependency['name']} v{dependency['version']}', file=sys.stderr)
                exit(1)
            else:
                print(f'Skipping dependency with no notices: {dependency['name']}')
                continue

        link_rst = f'`{dependency['name']} v{dependency['version']} <{get_url(dependency)}>`_'
        underline_rst = '-' * len(link_rst)
        dep_notices_rst = '\n    '.join(notices)

        section_rst = f'{link_rst}\n{underline_rst}\n\n::\n\n    {dep_notices_rst}\n\n'

        notices_rst += section_rst

    notices_rst_path = os.path.join(output_dir, 'dependency-notices.rst')
    with open(notices_rst_path, 'w', encoding="utf-8") as outfile:
        outfile.write(notices_rst)
