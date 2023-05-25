# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: CC0-1.0

set -l repo_dir (cd (dirname $_) && pwd)
set -gx PROJ_PATH $repo_dir
set -gx IDF_PATH $repo_dir/lib/esp-idf
. $IDF_PATH/export.fish

