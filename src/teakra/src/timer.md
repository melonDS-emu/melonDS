# Timer

## MMIO Layout

The following MMIO definition is extracted from Lauterbach's Teak debugger. Some of them are untested.
```
Timer 0
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0020    |   TM  | GP| CS| BP|RES| MU| PC| CT| TP|   |     CM    |   TS  |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0022    |                                                           | EW|
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0024    |                         START_COUNT_L                         |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0026    |                         START_COUNT_H                         |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x0028    |                           COUNTER_L                           |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x002A    |                           COUNTER_H                           |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x002C    |                         PWM_COUNTER_L                         |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
|+0x002E    |                         PWM_COUNTER_H                         |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#

TS: time scale
 - 0: /1
 - 1: /2
 - 2: /4
 - 3: /16
CM: count mode
 - 0: single count
 - 1: auto restart
 - 2: free running
 - 3: event count
 - 4: watchdog mode 1
 - 5: watchdog mode 2
 - 6: watchdog mode 3
TP: output signal polarity
CT: 1 to clear output signal
PC: 1 to pause the counter
MU: 1 to enable COUNTER_L/_H register update
RES: 1 to restart the counter
BP: 1 to enable breakpoint requests
CS: clock source
 - 0: internal clock
 - 1: external clock
GP: ?
TM: output signal clearing method
 - 0: by setting CT manually
 - 1: after two cycles
 - 2: after four cycles
 - 3: after eight cycles

EW: 1 to decrement counter in event count mode / to reload watchdog in watch dog mode
START_COUNT_L, START_COUNT_H: the restart value for counter. Loaded to the counter on restarting
COUNTER_L, COUNTER_H: the value of the counter
PWM_COUNTER_L, PWM_COUNTER_H: the restart value for PWM counter. Loaded to the PWM counter on restarting

Timer 1
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
| +0x0030   | Same layout as Timer 0                                        |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
| ...                                                                       |
+-----------#---+---+---+---#---+---+---+---#---+---+---+---#---+---+---+---#
```
