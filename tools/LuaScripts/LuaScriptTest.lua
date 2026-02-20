-- Simple Script to test most of the different Lua Functions for MelonDS
-- Written by NPO197
-- TODO: re-write to be more readable... 

console.clear()
print("This text Should be cleared")
console.clear()

print("Running Test...")

u32Data = memory.read_u32_le(0x00000000)
print(string.format("DataZero: %x",u32Data))

canvas = gui.MakeCanvas(0,0,500,500)
gui.SetCanvas(canvas)
gui.ClearOverlay()

gui.FillRect(0,0,55,11,0xffffffff)
gui.drawText(0,9,"Test Message")
gui.drawLine(0,10,55,10,0x00ff00ff)
gui.Rect(0,0,55,11,0xff00ff00)

gui.drawImage("Lua-Logo_128x128.png",0,60)

gui.drawText(0,200,"WASD to move \"lua Stylus\", Q to tap screen.",0xffffff)
gui.drawText(0,210,"T to create savestate, R to load savestate.",0xffffff)

gui.Flip()


-------- Main Loop ----------

updateFlags = {}
Stylus = {x=0,y=0,}
textCanvas = gui.MakeCanvas(0,12,500,100)
vstylusCanvas = gui.MakeCanvas(0,0,256,192,1) -- bottom screen

--Gets Called once every frame
function _Update()
    updateFlags = {} -- Reset updateFlags at start of each frame

    --Text Functions
    gui.SetCanvas(textCanvas)
    gui.ClearOverlay()
    local y = 0
    for _,tfunct in ipairs({
        MousePosText,
        MouseButtonText,
        KeysText,
        JoyText,
        JoyStickText,
    }) do
        y = y+10
        gui.drawText(0,y,tfunct(),0xffffff)
    end
    gui.checkupdate()

    --SaveState stuff
    if KeyPress("T") then
        savestate.save("SaveState_Auto.mln")
    elseif KeyPress("R") then
        savestate.load("SaveState_Auto.mln")
    end

    --StylusLoop
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

    if KeyHeld("Q") then
        input.NDSTapDown(Stylus.x,Stylus.y)
    else
        input.NDSTapUp()
    end
    
    DrawStylus()
end

--Text Functions

function MousePosdrawText()
    mouse = input.getmouse()
    return "MousePos:"..mouse.X..","..mouse.Y
end

function JoyStickdrawText()
    local x = input.getjoystick(0)
    local y = input.getjoystick(1)
    return "JoyStick:"..x..","..y
end

function MouseButtondrawText()
    str = ""
    for k,v in pairs(input.getmouse()) do
        if k~="X" and k~="Y" and v then
            str = str..k
        end
    end
    return "MouseBtn:"..str
end

typed = ""
keys = {}
function KeysdrawText() 
    keys = input.Keys()
    str = ""
    for _,i in pairs(keys) do
        if pcall(string.char,i) then
            str = str..string.char(i)
        else
            print("NonAscii:"..i)
            typed = ""
        end    
    end
    typed = typed..str
    return "Keys:"..typed
end

joys = {}
function JoydrawText()
    joys = input.getjoy()
    str = ""
    for k,v in pairs(joys) do
        if v then
            str = str..k
        end
    end
    return "Joy:"..str
end

function DrawStylus()
    gui.SetCanvas(vstylusCanvas)
    gui.ClearOverlay()
    gui.Ellipse(Stylus.x-5,Stylus.y-5,10,10,0xffffffff)
    gui.Ellipse(Stylus.x-2,Stylus.y-2,4,4,0x00000000)    
    gui.Flip()
end

function KeyHeld(keyStr)
    -- Only need to update mask once per frame
    if updateFlags.KeyboardCheck == nil then
        mask = input.HeldKeys()
        updateFlags.KeyboardCheck = true 
    end
    return mask[string.byte(keyStr:sub(1,1))]
end

Btns = {}
function KeyPress(keyStr)
    if KeyHeld(keyStr) and Btns[keyStr]  then
        Btns[keyStr] = false
        return true
    end
    Btns[keyStr] = true
    return false
end



