# sdmmc/spi
An interactive shell for SD/MMC access via Linux spidev devices.

## Command Reference
### ?
Display commands.
```
sdmmc/spi> ?
  ?                               Display commands
  session?                        Display session parameters
  verbose                         Be verbose (default)
  quiet                           Be quiet
  bye                             Leave sdmmc/spi
  
  clock FREQUENCY                 Set SPI clock frequency
  open FILENAME                   Open SPI device
  close                           Close SPI device
  
  cmd0                            Go to Idle State
  cmd1                            Send Operating Condition
  cmd6 FUNCTION                   Check/Switch Function
  cmd8 CONDITION                  Send Interface Condition
  cmd9                            Read CSD Register
  cmd10                           Read CID Register
  cmd16 LENGTH                    Set Block Length
  cmd17 ADDRESS                   Read Single Block
  cmd58                           Read Operating Condition
  acmd41 CONDITION                Send Operating Condition
  
  fault tolerant                  Pad and skip block on error
  fault intolerant                Abort on block error
  retry COUNT                     Set block retry count
  
  push FILE BLOCK                 Push blocks to card
  pull BLOCK COUNT FILE           Pull blocks from card
```

### session?
Display session parameters.
```
$ ./sdmmcspi
sdmmc/spi> session?
  Device                          (null)
  Clock Frequency                 16000000Hz
  Poll Interval                   1000ms
  Fault Tolerant?                 No
  Retry Count                     0x00
  High Capacity?                  No
```

### verbose
Dump received and transmitted data, with supplementary field annotation where available, to standard output.

### quiet
Display errors only.

### bye
Exit the shell.

### clock FREQUENCY
Set the maximum SPI transmission frequency.
```
sdmmc/spi> clock 800000
sdmmc/spi> session?
  Device                          /dev/spidev0.0
  Clock Frequency                 800000Hz
  Poll Interval                   1000ms
  Fault Tolerant?                 No
  Retry Count                     0x00
  High Capacity?                  No
```

### open FILENAME
Open an SPI device.
```
sdmmc/spi> open /dev/spidev0.0
sdmmc/spi> session?
  Device                          /dev/spidev0.0
  Clock Frequency                 16000000Hz
  Poll Interval                   1000ms
  Fault Tolerant?                 No
  Retry Count                     0x00
  High Capacity?                  No
```

### close
Close the SPI device.
```
sdmmc/spi> close
sdmmc/spi> session?
  Device                          (null)
  Clock Frequency                 16000000Hz
  Poll Interval                   1000ms
  Fault Tolerant?                 No
  Retry Count                     0x00
  High Capacity?                  No
```

### cmd0
Go to Idle State.
```
sdmmc/spi> cmd0
TX
  00000000: ff40 0000 0000 95                        .@.....

  Command Type                    0x00 (Go to Idle State)
  Command Data                    0x00000000
  Command Checksum                0x4a

RX
  00000000: 01                                       .

  Card State                      0x01 (Idle)
```

### cmd1
Send Operating Condition, until the card enters the Ready state.
```
sdmmc/spi> cmd1
TX
  00000000: ff41 0000 0000 f9                        .A.....

  Command Type                    0x01 (Send Operating Condition)
  Command Data                    0x00000000
  Command Checksum                0x7c

RX
  00000000: 01

  Card State                      0x01 (Idle)

TX
  00000000: ff41 0000 0000 f9                        .A.....

  Command Type                    0x01 (Send Operating Condition)
  Command Data                    0x00000000
  Command Checksum                0x7c

RX
  00000000: 00

  Card State                      0x00 (Ready)
```

### cmd6
Check or Switch Card Function.

Response decoding for cmd6 is not implemented yet.
```
sdmmc/spi> cmd6 0x00000000
TX
  00000000: ff46 0000 0000 ef                        .F.....

  Command Type                    0x06 (Check / Switch Card Function)
  Command Data                    0x00000000
  Command Checksum                0x77

RX
  00000000: 00                                       .

  Card State                      0x00 (Ready)

RX
  00000000: fe00 6480 0180 0180 0180 01c0 0180 0100  ..d.............
  00000010: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  00000020: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  00000030: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  00000040: 0002 5d                                  ..]

  Token                           0xfe (Block Start)
  
  Checksum (received)             0x025d
  Checksum (calculated)           0x025d
```

### cmd8 CONDITION
Send Interface Condition
```
dmmc/spi> cmd8 0x01cd
TX
  00000000: ff48 0000 01cd 55                        .H....U

  Command Type                    0x08 (Send Interface Condition)
  Command Data                    0x000001cd
  Command Checksum                0x2a

RX
  00000000: 01                                       .

  Card State                      0x01 (Idle)

RX
  00000000: 0000 01cd                                ....

  Voltage Accepted                0x01 (2.7V - 3.6V)
  Check Pattern                   0xcd
```

### cmd9
Read CSD Register

#### CSD 1.0 Example
```
sdmmc/spi> cmd9
TX
  00000000: ff49 0000 0000 af                        .I.....

  Command Type                    0x09 (Read CSD Register)
  Command Data                    0x00000000
  Command Checksum                0x57

RX
  00000000: 00                                       .

  Card State                      0x00 (Ready)

RX
  00000000: fe00 2600 325f 5a83 aefe fbcf ff92 8040  ..&.2_Z........@
  00000010: df9f c5                                  ...

  Token                           0xfe (Block Start)
  
  Checksum (received)             0x9fc5
  Checksum (calculated)           0x9fc5

  CSD Version                     1.0
  TAAC                            0x26
  NSAC                            0x00
  Maximum Transfer Rate           0x32
  Command Classes                 0x05f5
  Maximum Read Block Length       0x0a
  Partial Block Reads?            0x01 (Yes)
  Write Block Misalignment?       0x00 (No)
  Read Block Misalignment?        0x00 (No)
  DSR Implemented                 0x00
  Device Size                     0x0ebb
  Max Read Current @ min(Vdd)     0x07
  Max Read Current @ max(Vdd)     0x06
  Max Write Current @ min(Vdd)    0x07
  Max Write Current @ max(Vdd)    0x06
  Device Size Multiplier          0x07
  Erase Block Enabled?            0x01 (Yes)
  Erase Sector Size               0x1f
  Write Protect Group Size        0x7f
  Write Protect Group Enabled?    0x01 (Yes)
  Write Speed Factor              0x04
  Max Write Block Length          0x0a
  Partial Block Writes?           0x00 (No)
  File Format Group               0x00
  Copy?                           0x01 (Yes)
  Permanent Write Protection?     0x00 (No)
  Temporary Write Protection?     0x00 (No)
  File Format                     0x00
  CSD Checksum                    0x6f
```

#### CSD 2.0 Example
```
sdmmc/spi> cmd9
TX
  00000000: ff49 0000 0000 af                        .I.....

  Command Type                    0x09 (Read CSD Register)
  Command Data                    0x00000000
  Command Checksum                0x57

RX
  00000000: 00                                       .

  Card State                      0x00 (Ready)

RX
  00000000: fe40 0e00 325b 5900 0076 b27f 800a 4040  .@..2[Y..v....@@
  00000010: 1309 93                                  ...

  Token                           0xfe (Block Start)
  
  Checksum (received)             0x0993
  Checksum (calculated)           0x0993

  CSD Version                     2.0
  TAAC                            0x0e
  NSAC                            0x00
  Maximum Transfer Rate           0x32
  Command Classes                 0x05b5
  Maximum Read Block Length       0x09
  Partial Block Reads?            0x00 (No)
  Write Block Misalignment?       0x00 (No)
  Read Block Misalignment?        0x00 (No)
  DSR Implemented                 0x00
  Device Size (Block Count)       0x000076b2
  Erase Block Enabled?            0x01 (Yes)
  Erase Sector Size               0x7f
  Write Protect Group Size        0x00
  Write Protect Group Enabled?    0x00 (No)
  Write Speed Factor              0x02
  Max Write Block Length          0x09
  Partial Block Writes?           0x00 (No)
  File Format Group               0x00
  Copy?                           0x01 (Yes)
  Permanent Write Protection?     0x00 (No)
  Temporary Write Protection?     0x00 (No)
  File Format                     0x00
  CSD Checksum                    0x00
```

### cmd10
Read CID Register
```
sdmmc/spi> cmd10
TX
  00000000: ff4a 0000 0000 1b                        .J.....

  Command Type                    0x0a (Read CID Register)
  Command Data                    0x00000000
  Command Checksum                0x0d

RX
  00000000: 00                                       .

  Card State                      0x00 (Ready)

RX
  00000000: fe03 5344 5342 3136 4780 db0d d8a8 014b  ..SDSB16G......K
  00000010: b144 c7                                  .D.

  Token                           0xfe (Block Start)
  
  Checksum (received)             0x44c7
  Checksum (calculated)           0x44c7

  Manufacturer                    0x03
  OEM/Application                 SD
  Product                         SB16G
  Revision                        8.0
  Serial Number                   0xdb0dd8a8
  Reserved                        0x00
  Manufactured                    2020/11
  Checksum                        0x58
```

### cmd16 LENGTH
Set Block Length
```
sdmmc/spi> cmd16 512
TX
  00000000: ff50 0000 0200 15                        .P.....

  Command Type                    0x10 (Set Block Length)
  Command Data                    0x00000200
  Command Checksum                0x0a

RX
  00000000: 00                                       .

  Card State                      0x00 (Ready)
```

### cmd17 ADDRESS
Read Single Block.

For standard capacity cards, address is a byte-offset and is typically provided as a multiple of block length. For high capacity cards, address is expected to be provided as a block-offset.
```
sdmmc/spi> cmd17 0
TX
  00000000: ff51 0000 0000 55                        .Q....U

  Command Type                    0x11 (Read Single Block)
  Command Data                    0x00000000
  Command Checksum                0x2a

RX
  00000000: 00                                       .

  Card State                      0x00 (Ready)

RX
  00000000: feeb 6390 0000 0000 0000 0000 0000 0000  ..c.............
  00000010: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  00000020: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  00000030: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  00000040: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  00000050: 0000 0000 0000 0000 0000 0000 8001 0000  ................
  00000060: 0000 0000 00ff fa90 90f6 c280 7405 f6c2  ............t...
  00000070: 7074 02b2 80ea 797c 0000 31c0 8ed8 8ed0  pt....y|..1.....
  00000080: bc00 20fb a064 7c3c ff74 0288 c252 be80  .. ..d|<.t...R..
  00000090: 7de8 1701 be05 7cb4 41bb aa55 cd13 5a52  }.....|.A..U..ZR
  000000a0: 723d 81fb 55aa 7537 83e1 0174 3231 c089  r=..U.u7...t21..
  000000b0: 4404 4088 44ff 8944 02c7 0410 0066 8b1e  D.@.D..D.....f..
  000000c0: 5c7c 6689 5c08 668b 1e60 7c66 895c 0cc7  \|f.\.f..`|f.\..
  000000d0: 4406 0070 b442 cd13 7205 bb00 70eb 76b4  D..p.B..r...p.v.
  000000e0: 08cd 1373 0d5a 84d2 0f83 d800 be8b 7de9  ...s.Z........}.
  000000f0: 8200 660f b6c6 8864 ff40 6689 4404 0fb6  ..f....d.@f.D...
  00000100: d1c1 e202 88e8 88f4 4089 4408 0fb6 c2c0  ........@.D.....
  00000110: e802 6689 0466 a160 7c66 09c0 754e 66a1  ..f..f.`|f..uNf.
  00000120: 5c7c 6631 d266 f734 88d1 31d2 66f7 7404  \|f1.f.4..1.f.t.
  00000130: 3b44 087d 37fe c188 c530 c0c1 e802 08c1  ;D.}7....0......
  00000140: 88d0 5a88 c6bb 0070 8ec3 31db b801 02cd  ..Z....p..1.....
  00000150: 1372 1e8c c360 1eb9 0001 8edb 31f6 bf00  .r...`......1...
  00000160: 808e c6fc f3a5 1f61 ff26 5a7c be86 7deb  .......a.&Z|..}.
  00000170: 03be 957d e834 00be 9a7d e82e 00cd 18eb  ...}.4...}......
  00000180: fe47 5255 4220 0047 656f 6d00 4861 7264  .GRUB .Geom.Hard
  00000190: 2044 6973 6b00 5265 6164 0020 4572 726f   Disk.Read. Erro
  000001a0: 720d 0a00 bb01 00b4 0ecd 10ac 3c00 75f4  r...........<.u.
  000001b0: c300 0000 0000 0000 0000 0000 0000 0080  ................
  000001c0: 2021 0083 2edf 4f00 0800 0000 a8ca 0100   !....O.........
  000001d0: 0081 c005 03d0 2f00 b0ca 0100 1c10 0000  ....../.........
  000001e0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
  000001f0: 0000 0000 0000 0000 0000 0000 0000 0055  ...............U
  00000200: aad6 0c                                  ...

  Token                           0xfe (Block Start)
  
  Checksum (received)             0xd60c
  Checksum (calculated)           0xd60c
```

### cmd58
Read Operating Condition.

The **High Capacity?** session flag is set according to the CCS bit in the OCR register.
```
sdmmc/spi> cmd58
TX
  00000000: ff7a 0000 0000 fd                        .z.....

  Command Type                    0x3a (Read Operating Condition)
  Command Data                    0x00000000
  Command Checksum                0x7e

RX
  00000000: 00                                       .

  Card State                      0x00 (Ready)

RX
  00000000: c0ff 8000                                ....

  OCR                             0x80000000 (Busy)
                                  0x40000000 (High Capacity)
                                  0x00800000 (3.5V - 3.6V OK)
                                  0x00400000 (3.4V - 3.5V OK)
                                  0x00200000 (3.3V - 3.4V OK)
                                  0x00100000 (3.2V - 3.3V OK)
                                  0x00080000 (3.1V - 3.2V OK)
                                  0x00040000 (3.0V - 3.1V OK)
                                  0x00020000 (2.9V - 3.0V OK)
                                  0x00010000 (2.8V - 2.9V OK)
                                  0x00008000 (2.7V - 2.8V OK)

sdmmc/spi> session?
  Device                          /dev/spidev0.0
  Clock Frequency                 16000000Hz
  Poll Interval                   1000ms
  Fault Tolerant?                 No
  Retry Count                     0x05
  High Capacity?                  Yes
```

### acmd41 CONDITION
Send Operating Condition, until card enters Ready state.
```
sdmmc/spi> acmd41 0x40000000
TX
  00000000: ff77 0000 0000 65                        .w....e

  Command Type                    0x37 (Begin Application Specific Command)
  Command Data                    0x00000000
  Command Checksum                0x32

RX
  00000000: c1                                       .

  Card State                      0xc1 (Unknown)

TX
  00000000: ff69 4000 0000 77                        .i@...w

  Command Type                    0x29 (Send Operating Condition)
  Command Data                    0x40000000
  Command Checksum                0x3b

RX
  00000000: 05                                       .

  Card State                      0x05 (Unknown)

TX
  00000000: ff77 0000 0000 65                        .w....e

  Command Type                    0x37 (Begin Application Specific Command)
  Command Data                    0x00000000
  Command Checksum                0x32

RX
  00000000: 01                                       .

  Card State                      0x01 (Idle)

TX
  00000000: ff69 4000 0000 77                        .i@...w

  Command Type                    0x29 (Send Operating Condition)
  Command Data                    0x40000000
  Command Checksum                0x3b

RX
  00000000: 01                                       .

  Card State                      0x01 (Idle)
  
TX
  00000000: ff77 0000 0000 65                        .w....e

  Command Type                    0x37 (Begin Application Specific Command)
  Command Data                    0x00000000
  Command Checksum                0x32

RX
  00000000: 01                                       .

  Card State                      0x01 (Idle)

TX
  00000000: ff69 4000 0000 77                        .i@...w

  Command Type                    0x29 (Send Operating Condition)
  Command Data                    0x40000000
  Command Checksum                0x3b

RX
  00000000: 00                                       .

  Card State                      0x00 (Ready)
```

### fault tolerant
Pad, with a block of NUL bytes, and skip block on push/pull error.
```
sdmmc/spi> fault tolerant 
sdmmc/spi> session?
  Device                          /dev/spidev0.0
  Clock Frequency                 16000000Hz
  Poll Interval                   1000ms
  Fault Tolerant?                 Yes
  Retry Count                     0x00
  High Capacity?                  Yes
```

### fault intolerant
Abort push/pull on block error.
```
sdmmc/spi> fault intolerant
sdmmc/spi> session?
  Device                          /dev/spidev0.0
  Clock Frequency                 16000000Hz
  Poll Interval                   1000ms
  Fault Tolerant?                 No
  Retry Count                     0x00
  High Capacity?                  Yes

```

### retry COUNT
Set block retry count for push/pull errors.
```
sdmmc/spi> retry 5
sdmmc/spi> session?
  Device                          /dev/spidev0.0
  Clock Frequency                 16000000Hz
  Poll Interval                   1000ms
  Fault Tolerant?                 No
  Retry Count                     0x05
  High Capacity?                  Yes
```

### push FILE BLOCK
Push blocks to card.

#### Test Data
```
$ for i in $(seq 1 512); do echo -n "A"; done > /tmp/blocks
$ for i in $(seq 1 512); do echo -n "B"; done >> /tmp/blocks
```

#### Quiet Example
```
sdmmc/spi> quiet
sdmmc/spi> push /tmp/blocks 1024
Pushed 2 of 2 block(s) in +-1s
```

#### Verbose Example
```
sdmmc/spi> verbose
sdmmc/spi> push /tmp/blocks 1024                                        
TX                                     
  00000000: ff58 0000 0400 37                        .X....7

  Command Type                    0x18 (Unknown)
  Command Data                    0x00000400
  Command Checksum                0x1b                                         

RX                                     
  00000000: 00                                       .

  Card State                      0x00 (Ready)

TX                                     
  00000000: fe41 4141 4141 4141 4141 4141 4141 4141  .AAAAAAAAAAAAAAA
  00000010: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000020: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000030: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000040: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000050: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000060: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000070: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000080: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000090: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000000a0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000000b0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000000c0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000000d0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000000e0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000000f0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000100: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000110: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000120: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000130: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000140: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000150: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000160: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000170: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000180: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000190: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000001a0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000001b0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000001c0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000001d0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000001e0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000001f0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000200: 41                                       A

  Token                           0xfe (Block Start)

RX                                     
  00000000: e5                                       .

  Write Status                    0x02 (Accepted)

RX                                     
  00000000: 00                                       .

RX                                                                             
  00000000: 03                                       .               
                                                                               
TX                                                                             
  00000000: ff58 0000 0401 25                        .X....%         
                                                                               
  Command Type                    0x18 (Unknown)                               
  Command Data                    0x00000401          
  Command Checksum                0x12
                                                                               
RX                                                                             
  00000000: 00                                       .                         
                                       
  Card State                      0x00 (Ready)        

TX                                     
  00000000: fe42 4242 4242 4242 4242 4242 4242 4242  .BBBBBBBBBBBBBBB
  00000010: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000020: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000030: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000040: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000050: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000060: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000070: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000080: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000090: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000000a0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000000b0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000000c0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000000d0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000000e0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000000f0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000100: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000110: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000120: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000130: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000140: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000150: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000160: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000170: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000180: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000190: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000001a0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000001b0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000001c0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000001d0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000001e0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000001f0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000200: 42                                       B

  Token                           0xfe (Block Start)
  
RX                                     
  00000000: e5                                       .

  Write Status                    0x02 (Accepted)

RX                                     
  00000000: 00                                       .

RX                                     
  00000000: 03                                       .

Pushed 2 of 2 block(s) in +-2s             
```

### pull BLOCK COUNT FILE
Pull blocks from card.

#### Quiet Example
```
sdmmc/spi> quiet
sdmmc/spi> pull 0 128 /tmp/blocks
Pulled 128 of 128 block(s) in +-2s
```

#### Verbose Example
```
sdmmc/spi> pull 0 2 /tmp/blocks                                                 
TX                                                                              
  00000000: ff51 0000 0000 55                        .Q....U                    
                                                                                
  Command Type                    0x11 (Read Single Block)                      
  Command Data                    0x00000000                         
  Command Checksum                0x2a
                                                                                
RX                                                                              
  00000000: 00                                       .               
                                                                                
  Card State                      0x00 (Ready)                       
                                                                                
RX                                                                              
  00000000: fe41 4141 4141 4141 4141 4141 4141 4141  .AAAAAAAAAAAAAAA
  00000010: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000020: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000030: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000040: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA           
  00000050: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000060: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA    
  00000070: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000080: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000090: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000000a0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000000b0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000000c0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000000d0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000000e0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000000f0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000100: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000110: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000120: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000130: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000140: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000150: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000160: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000170: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000180: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000190: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000001a0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000001b0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000001c0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000001d0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000001e0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  000001f0: 4141 4141 4141 4141 4141 4141 4141 4141  AAAAAAAAAAAAAAAA
  00000200: 41bf 75                                  A.u

  Token                           0xfe (Block Start)

  Checksum (received)             0xbf75                             
  Checksum (calculated)           0xbf75                             
                                                                                
TX                                                                              
  00000000: ff51 0000 0001 47                        .Q....G         
                                                                                
  Command Type                    0x11 (Read Single Block)           
  Command Data                    0x00000001                         
  Command Checksum                0x23                                          
                                                                                
RX                                                                              
  00000000: 00                                       .               
                                                                                
  Card State                      0x00 (Ready)                       
                                                                                
RX                                                                              
  00000000: fe42 4242 4242 4242 4242 4242 4242 4242  .BBBBBBBBBBBBBBB
  00000010: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000020: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000030: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000040: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000050: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000060: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000070: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000080: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000090: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000000a0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000000b0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000000c0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000000d0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000000e0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000000f0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000100: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000110: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000120: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000130: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000140: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000150: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000160: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000170: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000180: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000190: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000001a0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000001b0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000001c0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000001d0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000001e0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  000001f0: 4242 4242 4242 4242 4242 4242 4242 4242  BBBBBBBBBBBBBBBB
  00000200: 428b a6                                  B..

  Token                           0xfe (Block Start)

  Checksum (received)             0x8ba6 
  Checksum (calculated)           0x8ba6 

Pulled 2 of 2 block(s) in +-1s
``` 

#### Fault Tolerant Example
```
sdmmc/spi> fault tolerant
sdmmc/spi> retry 5
sdmmc/spi> quiet
sdmmc/spi> pull 0 6160 /tmp/blocks
Bad Block: 6144
Bad Block: 6145
Bad Block: 6146
Bad Block: 6147
Bad Block: 6148
Bad Block: 6149
Bad Block: 6150
Bad Block: 6151
Bad Block: 6152
Bad Block: 6153
Bad Block: 6154
Bad Block: 6155
Bad Block: 6156
Bad Block: 6157
Bad Block: 6158
Bad Block: 6159
Pulled 6160 of 6160 block(s) in +-33s
```

#### Fault Intolerant Example
```
sdmmc/spi> fault intolerant
sdmmc/spi> retry 5
sdmmc/spi> quiet
sdmmc/spi> pull 0 6160 /tmp/blocks
Bad Block: 6144
Pulled 6143 of 6160 block(s) in +-33s
```


## Card Initialisation
### SD
```
sdmmc/spi> cmd0
sdmmc/spi> cmd8 0x01cd
sdmmc/spi> cmd58
sdmmc/spi> acmd41 0x40000000
sdmmc/spi> cmd58
```

### MMC
```
sdmmc/spi> cmd0
sdmmc/spi> cmd8 0x1cd
sdmmc/spi> cmd58
sdmmc/spi> cmd1
sdmmc/spi> cmd58
```


## Remarks
- `cmd0` may need to be invoked multiple times before the card enters the *Ready* state.

- *Poll Interval*, displayed in the response of the `session?` command, is an approximate delay period between each request generated by the `cmd1` and `acmd41` commands until the card enters the *Ready* state.

- The card capacity indicated in the `cmd58` response affects how blocks are addressed for `push` and `pull` commands. Keep this in mind if you are working with both Standard and High Capacity cards in the same session.

- SIGINT (*Ctrl+C*) can be used to interrupt the `push` and `pull` commands. This will terminate the command once the current block en transit has been processed.


## Recommended Reading
- SD Physical Layer Specification Version 2.00
