-- Simple Script to test most of the different Lua Functions for MelonDS
-- Written by NPO197

MelonClear()

MelonPrint("This text Should be cleared")

MelonClear()

MelonPrint("Running Test...")

u32Data = Readu32(0x00000000)

MelonPrint(string.format("DataZero: %x",u32Data))

NDSTapDown(0,0)

NDSTapUp()

--StateSave("SaveState_Auto")

--StateLoad("SaveState_Auto")

canvas = MakeCanvas(0,0,500,500)

SetCanvas(canvas)

ClearOverlay()

FillRect(0,0,55,11,0xffffffff)

Text(0,9,"Test Message")

Line(0,10,55,10,0x00ff00ff)

Rect(0,0,55,11,0xff00ff00)

DrawImage("Lua-Logo_128x128.png",0,60)

Text(0,200,"WASD to move \"lua Stylus\", Q to tap screen",0xffffff)

--ClearHash()

Flip()


-------- Main Loop ----------

typed = ""
keys = {}
joys = {}

--protected ints -> string
function pInts2Str(ints)
    str = ""
    for _,i in pairs(ints) do
        if pcall(string.char,i) then
            str = str..string.char(i)
        else
            MelonPrint("NonAscii:"..i)
            typed = ""
        end    
    end
    return str
end


textFunctions = {
    --MousePosText 
    [1] = function()
        mouse = GetMouse()
        return "MousePos:"..mouse.X..","..mouse.Y
    end,
    --MouseButtonText 
    [2] = function()
        mouse = GetMouse()
        str = ""
        for k,v in pairs(mouse) do
            if k~="X" and k~="Y" and v then
                str = str..k
            end
        end
        return "MouseBtn:"..str
    end,
    --KeysText
    [3] = function()
        keys = Keys()
        temp = pInts2Str(keys)
        typed = typed..temp
        return "Keys:"..typed
    end,
    --JoyText
    [4] = function()
        joys = GetJoy()
        str = ""
        for k,v in pairs(joys) do
            if v then
                str = str..k
            end
        end
        return "Joy:"..str
    end
}

function TextLoop()
    SetCanvas(textCanvas)
    ClearOverlay()
    y = 0
    for _,tfunct in ipairs(textFunctions) do
        y = y+10
        Text(0,y,tfunct(),0xffffff)
    end
    Flip()
end

Stylus = {
    x = 0,
    y = 0,
}

function Stylus:Loop()
    move = {
        --Key = {dx,dy}
        ["W"] = {0,-1},
        ["A"] = {-1,0},
        ["S"] = {0,1},
        ["D"] = {1,0}
    }
    mask = KeyboardMask()
    for tkey,dir in pairs(move) do
        if mask[string.byte(tkey)] then
            self.x=self.x+dir[1]
            self.y=self.y+dir[2]
        end
    end
    if mask[string.byte("Q")] then
        NDSTapDown(self.x,self.y)
    else
        NDSTapUp()
    end
    SetCanvas(vstylusCanvas)
    ClearOverlay()
    Ellipse(self.x-5,self.y-5,10,10,0xffffffff)
    Ellipse(self.x-2,self.y-2,4,4,0x00000000)    
    Flip()
end

textCanvas = MakeCanvas(0,12,500,100)
vstylusCanvas = MakeCanvas(0,0,256,192,1) -- bottom screen
function _Update()
    TextLoop()
    Stylus:Loop()
end
