#!/usr/bin/env sh
set -e

ln -sf $(buck2 bxl prelude//cxx/tools/compilation_database.bxl:generate -- --targets //src/...) $PWD/compile_commands.json
buck2 run //src:stage_manager
