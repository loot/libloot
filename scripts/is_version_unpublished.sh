#!/bin/bash
set -e

PACKAGE="$1"

CURRENT_VERSION="$(cargo metadata --format-version 1 | jq -r ".packages[] | select(.name == \"$PACKAGE\") | .version")"

curl -sSfL "https://index.crates.io/${PACKAGE:0:2}/${PACKAGE:2:2}/$PACKAGE" | jq -s "all(.vers != \"$CURRENT_VERSION\")"
