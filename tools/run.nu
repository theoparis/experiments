#!/usr/bin/env nu

ln -sf (buck2 bxl prelude//cxx/tools/compilation_database.bxl:generate -- --targets "//src/...") $"($env.PWD)/compile_commands.json"
buck2 run //src:stage-manager
