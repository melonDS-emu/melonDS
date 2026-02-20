# Lua Script Overview
### Note
Lua scripting support is still a work in progress, lua features and functions are subject to change.

## Hello World Eample
Example of a Lua script that, when run, will display "Hello World", and a second counter on the top screen.

```Lua
screenID = 0 -- TopScreen = 0, BottomScreen = 1, OSD = 2
canvas = gui.MakeCanvas(0,0,100,100,screenID) 
gui.SetCanvas(canvas)
frameCount = 0
time_seconds=0
-- '_Update()' gets called once every frame
function _Update()
    frameCount = frameCount+1
    
    if frameCount>=60 then
        frameCount=0 
        time_seconds+=1
        gui.ClearOverlay()
        gui.drawText(0,10,"Hello World: "..time_seconds,0xffffff) --White text 
    end
    gui.checkupdate() --check if canvas has been updated, flip image buffer if so.
end
```

## Types and Notation

`uReturnedVal lib.FunctionName(uArg1,uArg2,[uOptionalArg1 = defaultVal],[uOptionalArg2 = defaultVal],...)` 
- `lib` the name of the library the function is apart of
- `FunctionName` the name of the function
- `uArg` Non optional argument expected of type "u" (lua types are n="number",s="string",b="boolean", t="table")
- `[uOptionalArg = defaultVal]` optional argument expected of type "u" if not provided defaults to `defaultVal`
- `uReturnedVal` Value returned by function (usually `nil`)

## Function List

`nil print(sPrintText)`
- Print `sPrintText` to the lua consol

`nil console.clear()`
- Clears the console in the lua dialog window.

`nil input.NDSTapDown(nX,nY)`
- Push down on the bottom touchscreen at pixel position X,Y.
- touchscreen dimensions are 256x192.

`nil input.NDSTapUp()`
- Releases the touch screen.

`tMouseState input.GetMouse()`
- Returns a lua table of the mouse X/Y coordinates and button states. 
- Table keys are: `X, Y, Left, Middle, Right, XButton1, XButton2`
- Example
    ```Lua
    if GetMouse().Left == true then 
        print("Click at:"..GetMouse().X..','..GetMouse().Y)
    end
    ```

`nAxisValue input.GetJoyStick(nAxisNum)`
- Returns the current value of the connected analoge joystick axis 
- nAxisNum is a 4 bit int, (0-15)

`tJoyState input.GetJoy()`
- Returns a lua table of the Joypad button states.
- Table keys are: `A,B,Select,Start,Right,Left,Up,Down,R,L,X,Y`
- Example
    ```Lua 
    function JoyText()
        joys = input.GetJoy()
        str = ""
        for k,v in pairs(joys) do
            if v then
                str = str..k
            end
        end
        return "Joy:"..str
    end
    ```

`tKeyMask input.HeldKeys()`
- Returns a lua table containing the value "true" for each held key and "nil" for any key not currently held. 
- check the Qt docs for a list of key codes: https://doc.qt.io/qt-6/qt.html#Key-enum
- Example
    ```Lua
    --Loop over all currently held keys
    for k,_ in pairs(input.HeldKeys()) do
        print("KeyPressed:"..k)
    end
    --Check if the "Q" key is currently pressed
    if input.HeldKeys()[string.byte("Q")] then
        print("\"Q\" Pressed!")
    end
    ```

`tKeyStrokes input.Keys()` 
- Returns a lua table of all keys pressed since the last time `input.Keys()` was called...
- DIFFERENT from `input.HeldKeys()`
- This function keeps proper track of the order of keys typed, and if the same key was pressed multiple times since the last check.
- Mostly used for cases that need faster typing/higher poll rate... otherwise use HeldKeys() for most use cases.
- Example 
    ```Lua
    typed = ""
    keys = {}
    function KeysText() 
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
    ```




`nCanvasID gui.MakeCanvas(nX, nY,nWidth,nHeight,[nScreenTarget = 2],[bIsActive = true])`
- Creates a new canvas and returns it's ID.
- You must set a canvas using `SetCanvas` before drawing anything to the screen.
- `nScreenTarget` options are 0 = TopScreen, 1 = BottomScreen, 2 = OSD / non-screen target (default)
- `bIsActive` must be true for the canvas to be drawn to the screen (true by default)
- `nCanvasID` is the ID of the newly created Canvas.

`nil gui.SetCanvas(nCanvasID)`
- Sets the current canvas to `nCanvasID`
- All future drawing functions will draw to the *currently set canvas*. 

`nil gui.ClearOverlay()`
- Clears the *currently set canvas* by setting all pixels to `0x00000000` transparent black

`nil gui.Flip()`
- Tells MelonDS that the *currently set canvas* has been updated and must be redrawn. 
- This may be handled automatically in the future...

`nil gui.checkupdate()`
- Checks if the *currently set canvas* has been updated, then redraws the canvas if so

`nil gui.drawText(nX,nY,sMessage, [nColor = 0x000000],[nFontSize = 9],[sFontFamily = 'Helvetica'])`
- Draws `sMessage` to the *currently set canvas* given the color and font specified.
- Qt will attempt to match `sFontFamily` to the closest font it can find available...

`nil gui.drawLine(nX1,nY1,nX2,nY2,nColor)`
- Draws a line on the *currently set canvas* fom (x1,y1) to (x2,y2) of the specified color.

`nil gui.Rect(nX,nY,nWidth,nHeight,ncolor)`
- Draws a 1p width Rectangle on the *currently set canvas* of the given size position and color.

`nil gui.FillRect(nX,nY,nWidth,nHeight,ncolor)`
- Draws a filled in Rectangle on the *currently set canvas* of the given size position and color.

`nil gui.Ellipse(nX,nY,nWidth,nHeight,ncolor)`
- Draws a 1p width Ellipse on the *currently set canvas* of the given size position and color.

`nil gui.DrawImage(sImagePath,nX,nY,[nSourceX = 0],[nSourceY = 0],[nSourceWidth = -1],[nSourceHeight = -1])`
- Draws the image saved at `sImagePath` to the *currently set canvas* at the given position.
- By default copies the entire image to the canvas.
- optional arguments Source-(X,Y,Width,Height) determain where in the source image to copy from.

`nil gui.ClearImageHash()`
- Clears the interal hash of all images drawn using DrawImage
- Only needed if RAM is being filled up with lots of different Image files.





