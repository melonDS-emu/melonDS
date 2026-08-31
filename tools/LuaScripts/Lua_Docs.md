# Lua Script Overview
### Note
Lua scripting support is still a work in progress, lua features and functions are subject to change.

## Hello World Eample
Example of a Lua script to display "Hello World", and a frame counter on the top screen.

```Lua
local screenID = 0 -- TopScreen = 0, BottomScreen = 1, OSD = 2
local canvas = gui.MakeCanvas(0,0,100,100,screenID) 
gui.SetCanvas(canvas)
local frameCount = 0
-- '_Update()' gets called once every frame
function _Update()
    gui.ClearOverlay() -- Clear previous frame
    frameCount = frameCount+1
    gui.drawText(0,10,"Hello World: "..frameCount,0xffffff) --White text 
    gui.Flip() -- Draw the new frame
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
    local mouse = input.GetMouse()
    if mouse.Left == true then 
        print("Click at:"..mouse.X..','..mouse.Y)
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
        local joys = input.GetJoy()
        local str = ""
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
    --poll currently held keys
    local tHeldKeys = input.HeldKeys()
    --Loop over all currently held keys
    for k,v in tHeldKeys do
        if v then 
            print("KeyPressed:"..k)
        end
    end
    --Check if the "Q" key is currently pressed
    if tHeldKeys[string.byte("Q")] then
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
    local typed_string = ""
    function KeysText() 
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
- Clears the *currently set canvas* by setting all pixels to `0x00000000` (transparent black)

`nil gui.Flip()`
- Tells MelonDS that the *currently set canvas* has been updated and must be redrawn.
- Note: Every canvas has an 'active' (on screen) and 'inactive' (internal) image buffer.
    - At any time only the 'active' buffer is shown on screen
    - internally all draw functions draw to the inactive image buffer
    - calling Flip will swap the active and inactive image buffers
    - by using a double buffer in this way we can prevent partialy finished drawings from being shown
- This may be handled automatically in the future...

`nil gui.checkupdate()` *for compatibilty use*
- same as `do Flip();ClearOverlay() end` but only actually gets called if the *currently set canvas* is in need of an update
- all `gui.draw___()` functions will set the *currently set canvas*'s 'updateNeeded' flag, regardless if anything is drawn.
- can be useful for compatability with bizhawk lua scripts in some cases.
- for now please use `Flip` and `ClearOverlay` instead of `checkupdate` whenever possible.

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

`nil gui.drawImage(sImagePath, nX, nY, [nWidth=-1],[nHeight=-1])`
- Draws the image saved at `sImagePath` to the *currently set canvas* at the given position.
- By default copies the entire image to the canvas.
- optional arguments nWidth, nHeight, will resize image to the given width and height before drawing.
- if nWidth or nHeight are negative will instead default to using the images original width / height.
- the file contents will be cached and re-used next time this function is called with the same sImagePath

`nil void gui.drawImageRegion(sImagePath, nSource_x, nSource_y, nSource_width, nSource_height, nDest_x, nDest_y, [nDest_width = -1], [nDest_height = -1])` 

- Draws part of the image saved at `sImagePath` to the *currently set canvas*
- Consult the following diagram to see its usage (renders embedded on the TASVideos Wiki): 
![](https://user-images.githubusercontent.com/13409956/198868522-55dc1e5f-ae67-4ebb-a75f-558656cb4468.png)
- the file contents will be cached and re-used next time this function is called with the same sImagePath

`nil gui.ClearImageHash()`
- Clears the interal hash of all images drawn using drawImage
- Only really needed if RAM is being filled up with lots of different Image files.





