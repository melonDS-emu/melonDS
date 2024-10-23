# Lua Script Overview
### Note
Lua scripting support is still a work in progress, lua features and functions are subject to change.

## Hello World Eample
Example of a Lua script that, when run, will display "Hello World", and a frame counter on the top screen.

```Lua
screenID = 0 -- TopScreen = 0, BottomScreen = 1, OSD = 2
canvas = MakeCanvas(0,0,100,100,screenID) 
SetCanvas(canvas)
frameCount = 0

-- '_Update()' gets called once every frame
function _Update()
    frameCount = frameCount+1
    ClearOverlay()
    Text(0,10,"Hello World: "..frameCount,0xffffff) --White text
    Flip()
end
```

## Types and Notation

`uReturnedVal FunctionName(uArg1,uArg2,[uOptionalArg1 = defaultVal],[uOptionalArg2 = defaultVal],...)` 
- `FunctionName` the name of the function
- `uArg` Non optional argument expected of type "u" (lua types are n="number",s="string",b="boolean", t="table")
- `[uOptionalArg = defaultVal]` optional argument expected of type "u" if not provided defaults to `defaultVal`
- `uReturnedVal` Value returned by function (usually `nil`)

## Function List

`nil MelonPrint(sPrintText)`
- Print `sPrintText` to the lua consol
- for now use this instead of `print()`

`nil MelonClear()`
- Clearse the lua consol

`n_u8Value Readu8(nAddress)`
- Read Data unisigned one Byte from u32 address `nAddress`
- `n_u16Value Readu16(nAddress)` Read Data unisigned two Bytes from u32 address `nAddress`
- `n_u32Value Readu32(nAddress)` Read Data unisigned four Bytes from u32 address `nAddress`

`n_s8Value Readu8(nAddress)`
- Read Data signed one Byte from u32 address `nAddress`
- `n_s16Value Readu16(nAddress)` Read Data signed two Bytes from u32 address `nAddress`
- `n_s32Value Readu32(nAddress)` Read Data signed four Bytes from u32 address `nAddress`

`nil NDSTapDown(nX,nY)`
- Push down on the bottom touchscreen at pixel position X,Y.
- touchscreen dimensions are 256x192.

`nil NDSTapUp()`
- Releases the touch screen.

`nil StateSave(sFileName)`
- Creates a savestate and saves it as `sFileName`.

`nil StateLoad(sFileName)`
- Attempts to load a savestate with the filename `sFileName`.

`tMouseState GetMouse()`
- Returns a lua table of the mouse X/Y coordinates and button states. 
- Table keys are: `X, Y, Left, Middle, Right, XButton1, XButton2`
- Example
    ```Lua
    if GetMouse().Left == true then 
        MelonPrint("Click at:"..GetMouse().X..','..GetMouse().Y)
    end
    ```

`tJoyState GetJoy()`
- Returns a lua table of the Joypad button states.
- Table keys are: `A,B,Select,Start,Right,Left,Up,Down,R,L,X,Y`
- Example
    ```Lua 
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
    ```

`tKeyMask KeyboardMask()`
- Returns a lua table of 256 bool values representing the current state of the keyboard.
- Most Keys are ascii
- TODO: Improve support for non-US Keyboards
- Example
    ```Lua
    --Print id of key being pressed
    for k,v in pairs(KeyboardMask()) do
        if v then 
            MelonPrint("KeyPressed:"..k)
        end
    end
    --Check if the "Q" key is currently pressed
    if KeyboardMask()[string.byte("Q")] then
        MelonPrint("\"Q\" Pressed!")
    end
    ```

`tKeyStrokes Keys()` 
- Returns a lua table of all keys pressed since the last time `Keys()` was called...
- DIFFERENT from `KeyboardMask()`
- Mostly used for detecting special non-US Keyboard keys, or for faster typing/higher poll rate
- Example 
    ```Lua
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
    ```




`nCanvasID MakeCanvas(nX, nY,nWidth,nHeight,[nScreenTarget = 2],[bIsActive = true])`
- Creates a new canvas and returns it's ID.
- You must set a canvas using `SetCanvas` before drawing anything to the screen.
- `nScreenTarget` options are 0 = TopScreen, 1 = BottomScreen, 2 = OSD / non-screen target (default)
- `bIsActive` must be true for the canvas to be drawn to the screen (true by default)
- `nCanvasID` is the ID of the newly created Canvas.

`nil SetCanvas(nCanvasID)`
- Sets the current canvas to `nCanvasID`
- All future drawing functions will draw to the *currently set canvas*. 

`nil ClearOverlay()`
- Clears the *currently set canvas* by setting all pixels to `0x00000000` transparent black

`nil Flip()`
- Tells MelonDS that the *currently set canvas* has been updated and must be redrawn. 
- This may be handled automatically in the future...

`nil Text(nX,nY,sMessage, [nColor = 0x000000],[nFontSize = 9],[sFontFamily = 'Helvetica'])`
- Draws `sMessage` to the *currently set canvas* given the color and font specified.
- Qt will attempt to match `sFontFamily` to the closest font it can find available...

`nil Line(nX1,nY1,nX2,nY2,nColor)`
- Draws a line on the *currently set canvas* fom (x1,y1) to (x2,y2) of the specified color.

`nil Rect(nX,nY,nWidth,nHeight,ncolor)`
- Draws a 1p width Rectangle on the *currently set canvas* of the given size position and color.

`nil FillRect(nX,nY,nWidth,nHeight,ncolor)`
- Draws a filled in Rectangle on the *currently set canvas* of the given size position and color.

`nil Ellipse(nX,nY,nWidth,nHeight,ncolor)`
- Draws a 1p width Ellipse on the *currently set canvas* of the given size position and color.

`nil DrawImage(sImagePath,nX,nY,[nSourceX = 0],[nSourceY = 0],[nSourceWidth = -1],[nSourceHeight = -1])`
- Draws the image saved at `sImagePath` to the *currently set canvas* at the given position.
- By default copies the entire image to the canvas.
- optional arguments Source-(X,Y,Width,Height) determain where in the source image to copy from.

`nil ClearImageHash()`
- Clears the interal hash of all images drawn using DrawImage
- Only needed if RAM is being filled up with lots of different Image files.





