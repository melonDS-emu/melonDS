# Lua Script Overview
### Disclaimer 
Note this feature is still a work in progress. All Lua functions and features are subject to change.
## Hello World Example
Quick example of a Lua script that when run, will display "Hello World!" on screen.

```Lua
canvas = MakeCanvas(0,0,100,100)
SetCanvas(canvas)
function _Update()
    ClearOverlay()
    Text(0,10,"Hello World!",0xffffff,9,"Franklin Gothic Medium")
    Flip()
end
```
First we create a canvas at 0,0 (the top left of the screen), with width and height of 100 pixels.

`canvas` contains the index of the canvas we just created. `SetCanvas(canvas)` sets the current canvas so all drawing functions know which canvas to draw to.

`function _Update()` is a special function that if defined in a Lua script will be called once every frame, we don't need it in this example, but if you need to run some code, or update what is drawn to the screen every frame, this is the way to do it.

`ClearOverlay()` clears the canvas

`Text()` draws the text to the selected canvas at a x,y (y is the bottom of the text not the top unlike in Bizhawk) with RGB color font size and font family, qt5 uses a font matching algorithm to find the font family that matches the font provided. 

`Flip()` flips between a display buffer and an image buffer, this is to prevent MelonDS from displaying a frame that the Lua script hasn't finished drawing yet, **you must call flip after making any changes to the image buffer to be seen.**

### Other Functions

`GetMouse()` similar to Bizhawk's [`input.getmouse()`](https://tasvideos.org/Bizhawk/LuaFunctions)  
Returns a table with the x,y pos and mouse buttons.
<details><summary><b>Buttons:</b></summary><p>

```
    "X,"Y","Left","Middle","Right",
    "XButton1","XButton2"
```
</p>
</details>

`StateSave()` / `StateLoad()` creates / loads a save state from the given file path.

`Readu8(int address)` / `Readu16` / `Readu32` read unsigned data from RAM in format specified.
`Reads8(int address)`/ `Reads16` / `Reads32` read signed data from RAM in format specified

`MelonPrint` print plain text to console

`MelonClear` clear console 

`Rect(int x, int y, int width, int height, u32 color)` Draw rectangle of given width height and color at x,y

`FillRect(int x, int y, int width, int height, u32 color)` Draw a filled rectangle at x,y

`Line(int x1, int y1, int x2, int y2, u32 color)` Draw a line from x1,y1 to x2,y2

`DrawImage(string path, int x, int y, int sourceWidth, int sourceHeight)` Draw an image of width an height stored at `path` to the canvas at x,y.

`ClearHash()` Clear the image hash table. (Images Drawn with DrawImage are automatically stored in a hash table for faster redrawing)

`GetJoy()` Returns a table of buttons and if they are currently held down.  
<details><summary><b>Buttons:</b></summary><p>

```
    "A","B","Select","Start",
    "Right","Left","Up","Down",
    "R","L","X","Y"
```
</p>
</details>



## Other Examples



TODO: add more examples.

Report Mouse pos.
```Lua
SetCanvas(MakeCanvas(0,0,100,100))
function _Update()
    ClearOverlay()
    Pos=""..GetMouse().X..","..GetMouse().Y
    Text(0,10,"Pos:"..Pos,0xffffff,9,"Franklin Gothic Medium")
    Flip()
end

```

Example template to implement `emu.frameadvance` structured code using the `_Update()` function structure.
```Lua

emu ={
    frameadvance = function() coroutine.yield() end
}

function BlockingFunction()
    --InitCode
    while true do
        --LoopCode
        emu.frameadvance()
    end
end
MainLoop = coroutine.create(BlockingFunction)

function _Update()
	_,err = coroutine.resume(MainLoop)
	if err then --Handel Errors
		error(err)
	end
end
```
