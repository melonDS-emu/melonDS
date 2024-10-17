-- Simple Script to test most of the different Lua Functions for MelonDS
-- Written by NPO197

--TODO: re-write to make more readable

MelonClear()
MelonPrint("This text Should be cleared")
MelonClear()

MelonPrint("Running Test...")

u32Data = Readu32(0x00000000)
MelonPrint(string.format("DataZero: %x",u32Data))

canvas = MakeCanvas(0,0,500,500)
SetCanvas(canvas)
ClearOverlay()

FillRect(0,0,55,11,0xffffffff)
Text(0,9,"Test Message")
Line(0,10,55,10,0x00ff00ff)
Rect(0,0,55,11,0xff00ff00)

DrawImage("Lua-Logo_128x128.png",0,60)

Text(0,200,"WASD to move \"lua Stylus\", Q to tap screen.",0xffffff)
Text(0,210,"T to create savestate, R to load savestate.",0xffffff)

Flip()


-------- Main Loop ----------

textFunctions = {} --Declare the Table

Stylus = {
    x = 0,
    y = 0,
}

Btns = {
    Tup = true,
    Rup = true
}
--string -> ASCII Byte (only first char)
function BYTE(str)
    if #str~=1 then
        error("BYTE expects string of length 1")
    end
    return string.byte(str)    
end

textCanvas = MakeCanvas(0,12,500,100)
vstylusCanvas = MakeCanvas(0,0,256,192,1) -- bottom screen

--Gets Called once every frame
function _Update()
    
    --Text Functions
    SetCanvas(textCanvas)
    ClearOverlay()
    local y = 0
    for _,tfunct in ipairs(textFunctions) do
        y = y+10
        Text(0,y,tfunct(),0xffffff)
    end
    Flip()

    mask = KeyboardMask()

    --Save State
    if mask[BYTE("T")] then
        if Btns.Tup then
            StateSave("SaveState_Auto.mln")
            Btns.Tup = false
        end
    else
        Btns.Tup = true
    end
    if mask[BYTE("R")] then
        if Btns.Rup then
            StateLoad("SaveState_Auto.mln")
            Btns.Rup = false
        end
    else
        Btns.Rup = true
    end

    --StylusLoop
    for key,dir in pairs({
        --Key = {dx,dy}
        ["W"] = { 0,-1},
        ["A"] = {-1, 0},
        ["S"] = { 0, 1},
        ["D"] = { 1, 0}
    }) do
        if mask[BYTE(key)] then
            Stylus.x=Stylus.x+dir[1]
            Stylus.y=Stylus.y+dir[2]
        end
    end
    if mask[BYTE("Q")] then
        NDSTapDown(Stylus.x,Stylus.y)
    else
        NDSTapUp()
    end
    DrawStylus(mask)
end





--textFunctions

function MousePosText()
    mouse = GetMouse()
    return "MousePos:"..mouse.X..","..mouse.Y
end

function MouseButtonText()
    mouse = GetMouse()
    str = ""
    for k,v in pairs(mouse) do
        if k~="X" and k~="Y" and v then
            str = str..k
        end
    end
    return "MouseBtn:"..str
end

typed = ""
keys = {}

function KeysText() 
    keys = Keys()
    str = ""
    for _,i in pairs(keys) do
        if pcall(string.char,i) then
            str = str..string.char(i)
        else
            MelonPrint("NonAscii:"..i)
            typed = ""
        end    
    end
    typed = typed..str
    return "Keys:"..typed
end

joys = {}

function JoyText()
    joys = GetJoy()
    str = ""
    for k,v in pairs(joys) do
        if v then
            str = str..k
        end
    end
    return "Joy:"..str
end

textFunctions = {MousePosText,MouseButtonText,KeysText,JoyText} --Initalize Table

--Draw Stylus

function DrawStylus()
    SetCanvas(vstylusCanvas)
    ClearOverlay()
    Ellipse(Stylus.x-5,Stylus.y-5,10,10,0xffffffff)
    Ellipse(Stylus.x-2,Stylus.y-2,4,4,0x00000000)    
    Flip()
end



