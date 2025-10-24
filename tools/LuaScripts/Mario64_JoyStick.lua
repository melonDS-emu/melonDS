
--shocoman's hack uses RTCOM_DATA_OUTPUT register to send analog stick information from the 3ds
LeftStickAddress = 0x0C7FFDF0 

function _Update()
    local bytes = getPad(LeftStick,true)
    print(string.format("Stick:%x",bytes))
    Writeu32(bytes,LeftStickAddress)
    
    bytes=0
    if GetJoyStick(Triggers.y)>0 then bytes = 0x8000 end
    if GetJoyStick(Triggers.x)>0 then bytes = bytes|0x7f00 end
    Writeu32(bytes,LeftStickAddress+4) --turn camera left and right
end

-- Joystick Axis for XBOX controller joystick sticks...
LeftStick = {x=0,y=1}
RightSick = {x=2,y=3}
Triggers = {x=4,y=5}

-- Get axis values and pack into a single 16bit number
function getPad(stick,flipy)
    
    --joy_x/y are in the range -32,768 to 32,767  (16bit percision)
    local joy_x = GetJoyStick(stick.x)
    local joy_y = GetJoyStick(stick.y) 
    
    --flip y (adjust by 1 to prevent overflow!)
    if flipy then joy_y = -joy_y - 1 end

    --Deadzone of +/- 5%
    local MAX = 32767
    if (math.abs(joy_x)<(MAX*0.05)) then joy_x = 0 end
    if (math.abs(joy_y)<(MAX*0.05)) then joy_y = 0 end

    --Shift 16 bit values down to 8 bit value
    local x = ((math.abs(joy_x)>>8))
    local y = ((math.abs(joy_y)>>8))

    --convert back to signed 8bit values
    if(joy_x<0) then  x = 0x100-x end
    if(joy_y<0) then  y = 0x100-y end

    --shift 'y' byte into upper 8 bits of packed value
    local pad = (y<<8)|(x)

    return pad
end



--[[ Cheat code Source: https://github.com/shocoman/Analog-Controls-for-NDS-Games-on-3DS
    
    !!NOTE!! 
    I had to remove the arm 7 payload part of the shocoman's code since we are not running this on a 3ds
    (everything up to the first `D2000000 00000000` in the original code was removed) 

    
ActionReplay Cheat code USA v1.0 (for melonDS): 
5202B324 E7D01108
E2004B00 000001E0
E92D400F E59F11C4
E59F01CC E5810000
E8BD400F E12FFF1E
E92D423F E59F9198
E5990004 E1A03E80
E1A03F23 E59F0180
E58F317C E0232000
E0022003 E1866602
E1877603 E3A03000
E5990004 E1A01800
E1A01C41 E351000C
C3833002 E371000C
B3833001 E1A03403
E59F915C E1D900B4
E1800003 E1C900B4
E59F913C E1D940B0
E1A00004 EB00002C
E3540000 08BD423F
07D01108 059F0138
012FFF10 E1A05804
E1A04C04 E1A04C44
E1A05C45 E2655000
E0C10494 E0E10595
E59F9104 E12FFF39
E1B00600 E59F10EC
E59F90F0 E12FFF39
E3500A01 C3A00A01
E59F90EC E1C900B8
E1B00004 E1B01005
E59F90D8 E12FFF39
E59F90D4 E1C900BE
E1A00604 E59F10B4
E59F90B8 E12FFF39
E59F90BC E1C900BA
E1A00605 E59F109C
E59F90A0 E12FFF39
E59F90A4 E1C900BC
E3A00001 E5C90014
E8BD423F E59F009C
E12FFF10 E92D40FF
E59F5064 E59F605C
E59F4054 E5942000
E3500000 1A000006
E1520005 1A00000F
E59F2048 E2522001
E58F2040 D5846000
EA00000A E3A07014
E58F7030 E1520006
1A000006 E15400BC
E2400004 E7943000
E5D31000 E3C11004
E5C31000 E5845000
E8BD80FF 020F0DAC
E3811004 E1A08008
00000000 00000000
00000069 027FFDF0
01FFABE4 0203B898
02039684 02097594
0202B324 0202B328
0202B634 EAFF65FB
0202B324 EAFF65FB
D2000000 00000000
]]

