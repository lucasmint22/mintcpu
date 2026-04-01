; MintCPU framebuffer fill
; uses only MOV ADD STORE JMP

        MOV R0,0xB0000      ; framebuffer pointer
        MOV R1,0x00FF8800   ; orange color
        MOV R2,4            ; pixel size

loop:

        STORE R1,[R0]

        ADD R0,R2

        JMP loop
