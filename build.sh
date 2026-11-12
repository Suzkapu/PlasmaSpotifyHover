#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

commit=3a67a04b656bda45b2c175b5f12bb7e4222d5026
if (( $# > 1 )); then
    echo "Usage: $0 [path-to-KDE-plasma-desktop-checkout]" >&2
    exit 2
fi
if (( $# == 1 )); then
    upstream=$(realpath -- "$1")
else
    upstream="$PWD/build/upstream"
    if [[ ! -d "$upstream/.git" ]]; then
        git clone --depth 1 --branch v6.8.2 https://github.com/KDE/plasma-desktop.git "$upstream"
    fi
fi
if [[ $(git -C "$upstream" rev-parse HEAD) != "$commit" ]]; then
    echo "Expected KDE plasma-desktop v6.8.2 ($commit)." >&2
    exit 1
fi

source_dir="$PWD/build/source"
mkdir -p "$source_dir"
git -C "$upstream" archive "$commit" applets/taskmanager kcms/recentFiles | tar -x -C "$source_dir"
cp CMakeLists.txt "$source_dir/"
cp spotify.cpp "$source_dir/applets/taskmanager/"
patch --batch --forward -d "$source_dir" -p1 < taskmanager.patch
cmake -S "$source_dir" -B build/cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake --parallel "${BUILD_JOBS:-4}"
