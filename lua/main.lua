local backstack = require("backstack")
local main_menu = require("main_menu"):Create(backstack.root)
backstack:Push(main_menu)
