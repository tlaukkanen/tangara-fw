$repo_dir="$(pwd)".replace("`\", "/")
$env:PROJ_PATH="$repo_dir"
$env:IDF_PATH="$repo_dir/lib/esp-idf"
. "$($env:IDF_PATH)/export.ps1"
