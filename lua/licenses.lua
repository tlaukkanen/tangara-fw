local backstack = require("backstack")
local widgets = require("widgets")
local font = require("font")
local theme = require("theme")

local function show_license(text)
  backstack.push(function()
    local screen = widgets.MenuScreen {
      show_back = true,
      title = "Licenses",
    }
    screen.root:Label {
      w = lvgl.PCT(100),
      h = lvgl.SIZE_CONTENT,
      text_font = font.fusion_10,
      text = text,
    }
  end)
end

local function gpl(copyright)
  show_license(copyright .. [[

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.]])
end

local function bsd(copyright)
  show_license(copyright .. [[

Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.
THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.]])
end

local function xiphbsd(copyright)
  show_license(copyright .. [[

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
- Neither the name of the Xiph.org Foundation nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.]])
end

local function apache(copyright)
  show_license(copyright .. [[

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at
   http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.]])
end

local function mit(copyright)
  show_license(copyright .. [[

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.]])
end

local function boost(copyright)
  show_license(copyright .. [[

Boost Software License - Version 1.0 - August 17th, 2003
Permission is hereby granted, free of charge, to any person or organization obtaining a copy of the software and accompanying documentation covered by this license (the "Software") to use, reproduce, display, distribute, execute, and transmit the Software, and to prepare derivative works of the Software, and to permit third-parties to whom the Software is furnished to do so, all subject to the following:
The copyright notices in the Software and this entire statement, including the above license grant, this restriction and the following disclaimer, must be included in all copies of the Software, in whole or in part, and all derivative works of the Software, unless such copies or derivative works are solely in the form of machine-executable object code generated by a source language processor.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.]])
end


return function()
  local menu = widgets.MenuScreen({
    show_back = true,
    title = "Licenses"
  })

  local container = menu.root:Object {
    flex = {
      flex_direction = "column",
      flex_wrap = "nowrap",
      justify_content = "flex-start",
      align_items = "flex-start",
      align_content = "flex-start",
    },
    w = lvgl.PCT(100),
    h = lvgl.SIZE_CONTENT,
  }

  local function library(name, license, show_fn)
    local row = container:Object {
      flex = {
        flex_direction = "row",
        justify_content = "flex-start",
        align_items = "flex-start",
        align_content = "flex-start",
      },
      w = lvgl.PCT(100),
      h = lvgl.SIZE_CONTENT,
    }
    row:add_style(theme.list_item)
    row:Label { text = name, flex_grow = 1 }
    local button = row:Button {}
    button:Label { text = license, text_font = font.fusion_10 }
    button:onClicked(show_fn)
  end

  library("ESP-IDF", "Apache 2.0", function()
    apache("2015-2024 Espressif Systems (Shanghai) CO LTD")
  end)
  library("esp-idf-lua", "MIT", function()
    mit("Copyright (C) 2019 Ruslan V. Uss")
  end)
  library("FatFs", "BSD", function()
    bsd("Copyright (C) 2022, ChaN, all right reserved.")
  end)
  library("komihash", "MIT", function()
    mit("Copyright (c) 2021-2022 Aleksey Vaneev")
  end)
  library("LevelDB", "BSD", function()
    bsd("Copyright (c) 2011 The LevelDB Authors. All rights reserved.")
  end)
  library("libcppbor", "Apache 2.0", function()
    apache("Copyright 2019 Google LLC")
  end)
  library("libmad", "GPL", function()
    gpl("Copyright (C) 2000-2004 Underbit Technologies, Inc.")
  end)
  library("libtags", "MIT", function()
    mit("Copyright © 2013-2020 Sigrid Solveig Haflínudóttir")
  end)
  library("Lua", "MIT", function()
    mit("Copyright (C) 1994-2018 Lua.org, PUC-Rio")
  end)
  library("lua-linenoise", "MIT", function()
    mit("Copyright (c) 2011-2015 Rob Hoelz <rob@hoelz.ro>")
  end)
  library("lua-term", "MIT", function()
    mit("Copyright (c) 2009 Rob Hoelz <rob@hoelzro.net>")
  end)
  library("luavgl", "MIT", function()
    mit("Copyright (c) 2022 Neo Xu")
  end)
  library("LVGL", "MIT", function()
    mit("Copyright (c) 2021 LVGL Kft")
  end)
  library("MillerShuffle", "Apache 2.0", function()
    apache("Copyright 2022 Ronald Ross Miller")
  end)
  library("ogg", "BSD", function()
    xiphbsd("Copyright (c) 2002, Xiph.org Foundation")
  end)
  library("Opus", "BSD", function()
    xiphbsd(
      "Copyright 2001-2011 Xiph.Org, Skype Limited, Octasic, Jean-Marc Valin, Timothy B. Terriberry, CSIRO, Gregory Maxwell, Mark Borgerding, Erik de Castro Lopo")
  end)
  library("Opusfile", "BSD", function()
    xiphbsd("Copyright (c) 1994-2013 Xiph.Org Foundation and contributors")
  end)
  library("result", "MIT", function()
    mit("Copyright (c) 2017-2021 Matthew Rodusek")
  end)
  library("span", "Boost", function()
    boost("Copyright Tristan Brindle 2018")
  end)
  library("speexdsp", "bsd", function()
    xiphbsd(
      "Copyright 2002-2008 Xiph.org Foundation, Copyright 2002-2008 Jean-Marc Valin, Copyright 2005-2007 Analog Devices Inc., Copyright 2005-2008	Commonwealth Scientific and Industrial Research, Organisation (CSIRO), Copyright 1993, 2002, 2006 David Rowe, Copyright 2003 EpicGames, Copyright 1992-1994	Jutta Degener, Carsten Bormann")
  end)
  library("tinyfsm", "MIT", function()
    mit("Copyright (c) 2012-2022 Axel Burri")
  end)
  library("tremor", "bsd", function()
    xiphbsd("Copyright (c) 2002, Xiph.org Foundation")
  end)
end
