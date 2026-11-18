--[[
    lua testing script for melonDS.
    written by NPO
--]]

--Check first if any NDS rom has been loaded yet, 
if gameinfo.getromhash() == "NULL" then 
    print("No ROM is loaded. Please load a ROM first.\n")
    return--skip loading the rest of the script.
end

local GameTitleAddress = 0x027ffe00 --https://www.problemkaputt.de/gbatek-ds-cartridge-header.htm

local topScreen   = gui.MakeCanvas( 0, 0,256,192,0) -- 0 : top screen
local lowerScreen = gui.MakeCanvas( 0, 0,256,192,1) -- 1 : lower screen
local text_canvas = gui.MakeCanvas( 0,12,500,100)   -- 2 : OSD canvas (default / non-DS screen target)

function StartUpTest()
    console.clear()
    print("This text will be cleared...")
    console.clear()
    print("Running Test...")
    local data = memory.read_bytes_as_array(GameTitleAddress,12,"ARM9 System Bus")
    print("Game Title:")
    print(string.char(table.unpack(data)))
    print("WASD to move \"lua Stylus\", Q to tap screen.")
    print("T to create savestate, R to load savestate.")

    gui.SetCanvas(topScreen)
    gui.ClearOverlay()

    gui.FillRect(0,0,75,11,0xffffffff)
    gui.Rect(0,0,75,11,0xff00ff00)
    gui.drawText(0,9,"Hello World!")
    gui.drawLine(0,10,75,10,0x00ff00ff)
    
    --Note: `Lua-Logo_128x128.png` is assumed to be in the same folder as this script.
    gui.drawImage("Lua-Logo_128x128.png",256-32,0,32,32) 

    gui.Flip()
end
StartUpTest()

local color={
    white=0xffffff,
    black=0x000000
}

local Stylus = {x=0,y=0}

function Stylus.draw()
    gui.SetCanvas(lowerScreen)
    gui.ClearOverlay()
    gui.Ellipse(Stylus.x-5,Stylus.y-5,10,10,color.white)
    gui.Ellipse(Stylus.x-2,Stylus.y-2,4,4,color.black)    
    gui.Flip()
end

local saved,loaded = false,false

--Note: Gets called automaticially once every frame by melonDS
function _Update()
    local heldKeys = input.HeldKeys()
    local function KeyHeld(keyStr)
        return heldKeys[string.byte(keyStr)]
    end

    local saveKeyHeld = KeyHeld('T')
    if (saveKeyHeld and not(saved)) then
        savestate.save("SaveState_Auto.mln")
    end
    saved = saveKeyHeld --wait for key release

    local loadKeyHeld = KeyHeld('R')
    if (loadKeyHeld and not(loaded)) then
        savestate.load("SaveState_Auto.mln")
    end
    loaded = loadKeyHeld --wait for key release

    --Stylus Controls
    for key,dir in pairs({
        --Key = {dx,dy}
        ["W"] = { 0,-1},
        ["A"] = {-1, 0},
        ["S"] = { 0, 1},
        ["D"] = { 1, 0}
    }) do
        if KeyHeld(key) then
            Stylus.x=Stylus.x+dir[1]
            Stylus.y=Stylus.y+dir[2]
        end
    end
    Stylus.draw()

    if KeyHeld("Q") then
        input.NDSTapDown(Stylus.x,Stylus.y)
    else
        input.NDSTapUp()
    end

    Test_TextFunctions()
end

local TextFunctions = {} -- list of functions to test each frame

function Test_TextFunctions()
    gui.SetCanvas(text_canvas)
    gui.ClearOverlay()
    local y = 0
    for _,textFunc in pairs(TextFunctions) do
        y = y + 10
        gui.drawText(0,y,textFunc(),color.white)
    end
    gui.Flip()
end
function TextFunctions.MousePosText()
    local mouse = input.getmouse()
    return "MousePos:"..mouse.X..","..mouse.Y.."\tWheel:"..mouse.Wheel
end

function TextFunctions.JoyStickText()
    local x = input.getjoystick(0)
    local y = input.getjoystick(1)
    return "JoyStick:"..x..","..y
end

function TextFunctions.MouseButtonText()
    local str = ""
    for k,v in pairs(input.getmouse()) do
        if k~="X" and k~="Y" and k~="Wheel" and v then
            str = str..k
        end
    end
    return "MouseBtn:"..str
end

local typed_string = ""
function TextFunctions.KeysText() 
    local keys = input.Keys()
    local str = ""
    for _,i in pairs(keys) do
        if (0<=i) and (i<=255) then
            str = str..string.char(i)
        else
            print(string.format("NonASCII keypress: 0x%x",i))
            typed_string = ""
        end
    end
    typed_string = typed_string..str
    return "Keys:"..typed_string
end

function TextFunctions.JoyText()
    local joys = input.getjoy()
    local str = ""
    for k,v in pairs(joys) do
        if v then
            str = str..k
        end
    end
    return "Joy:"..str
end


