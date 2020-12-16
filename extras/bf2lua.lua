#!/usr/bin/env lua
--[[
This is a self-contained BF interpreter for lua or luajit.

Tape cells are set to eight bits and the interpreter uses run length encoding
and re-encoding of [-] to a single token. The lua interpreter has a limit on
the size of while loops, this means that a plain translation fails for very
large programs; the method used by this interpreter works with all known BF
programs.

]]
local code = assert(io.open(assert(({...})[1],"filename argument needed"),"r")):read("*all")
local ft = {}

-- Head of the script we will run
ft[#ft+1] = [[
-- Brainfuck code
local p = 0
local m = setmetatable({},{__index=function() return 0 end})

function o() io.write(string.char(m[p]%256)) io.flush() end
function i() local c=io.read(1) if c then m[p] = string.byte(c) end end
]]

local wt = {}
local wtc = {}
local sp = 0
local fno = 1
local tt = {}
local ttc = 0

-- Filter out non-command characters.
code = code:gsub('[^%<%>%+%-%.%,%[%]]','')

-- foreach substring ...
local l = string.len( code )
local i = 0
while i < l do
    i = i + 1
    local c = string.sub( code, i, i )
    local m = 1
    while c == string.sub( code, i+m, i+m ) do m=m+1 end
    if ( c == "+" ) then
	tt[#tt+1] = "m[p]=(m[p]+"..m..")%256"
	i = i + m-1
    elseif ( c == "-" ) then
	tt[#tt+1] = "m[p]=(m[p]-"..m..")%256"
	i = i + m-1
    elseif ( c == ">" ) then
	tt[#tt+1] = "p=p+"..m
	i = i + m-1
    elseif ( c == "<" ) then
	tt[#tt+1] = "p=p-"..m
	i = i + m-1
    elseif ( c == "." ) then tt[#tt+1]="o()"
    elseif ( c == "," ) then tt[#tt+1]="i()"
    elseif ( c == "[" ) then
	sp = sp + 1
	wt[sp] = table.concat(tt,"\n")
	wtc[sp] = ttc + #tt
	tt = {}
	ttc = 0
    elseif ( c == "]" ) then
	local body = table.concat(tt,"\n")
	local bodyc = #tt + ttc
	tt = {}
	ttc = 0
	if wt[sp] ~= "" then
	    tt[#tt+1]= wt[sp]
	    ttc = ttc + wtc[sp]
	end
	if body == "m[p]=(m[p]-1)%256" or body == "m[p]=(m[p]+1)%256" then
	    -- Tiny optimisation; [-] is set to zero.
	    tt[#tt+1]= "m[p]=0"
	elseif bodyc < 5120 then
	    -- Not too large loops are done directly.
	    tt[#tt+1]= "while m[p]~=0 do"
	    tt[#tt+1]= body
	    ttc = ttc + bodyc
	    tt[#tt+1]= "end"
	else
	    -- When the BF code gets larger we run into lua limitations.
	    -- Specifically the body of a while loop has a maximum size,
	    -- fortunatly the same limit does not apply to the body of a
	    -- function.
	    tt[#tt+1]= "while m[p]~=0 do"
	    tt[#tt+1]= "f"..fno.."()"
	    tt[#tt+1]= "end"

	    ft[#ft+1]= "function f"..fno.."()"
	    ft[#ft+1]= body
	    ft[#ft+1]= "end"

	    fno = fno + 1
	end
	wt[sp] = nil
	sp = sp -1
    else
        -- Comment char
    end
end

local ccode = table.concat(ft,"\n").."\n--\n"..table.concat(tt,"\n")
assert(loadstring(ccode))()
