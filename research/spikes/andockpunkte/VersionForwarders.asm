; Windows-x64-Tailcalls: integer-/XMM-Argumente und Stackparameter erhalten.
; Keine geratene C++-Signatur fuer GetFileVersionInfoByHandle.
EXTERN ResolveVersionExport:PROC
.const
n01 DB 'GetFileVersionInfoA',0
n02 DB 'GetFileVersionInfoByHandle',0
n03 DB 'GetFileVersionInfoExA',0
n04 DB 'GetFileVersionInfoExW',0
n05 DB 'GetFileVersionInfoSizeA',0
n06 DB 'GetFileVersionInfoSizeExA',0
n07 DB 'GetFileVersionInfoSizeExW',0
n08 DB 'GetFileVersionInfoSizeW',0
n09 DB 'GetFileVersionInfoW',0
n10 DB 'VerFindFileA',0
n11 DB 'VerFindFileW',0
n12 DB 'VerInstallFileA',0
n13 DB 'VerInstallFileW',0
n14 DB 'VerLanguageNameA',0
n15 DB 'VerLanguageNameW',0
n16 DB 'VerQueryValueA',0
n17 DB 'VerQueryValueW',0
.code
FORWARD MACRO Symbol, Text
PUBLIC Symbol
Symbol PROC FRAME
    sub rsp, 0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h], rcx
    mov [rsp+28h], rdx
    mov [rsp+30h], r8
    mov [rsp+38h], r9
    movdqu [rsp+40h], xmm0
    movdqu [rsp+50h], xmm1
    movdqu [rsp+60h], xmm2
    movdqu [rsp+70h], xmm3
    lea rcx, Text
    call ResolveVersionExport
    mov rcx, [rsp+20h]
    mov rdx, [rsp+28h]
    mov r8, [rsp+30h]
    mov r9, [rsp+38h]
    movdqu xmm0, [rsp+40h]
    movdqu xmm1, [rsp+50h]
    movdqu xmm2, [rsp+60h]
    movdqu xmm3, [rsp+70h]
    add rsp, 0A8h
    jmp rax
Symbol ENDP
ENDM
FORWARD GetFileVersionInfoA, n01
FORWARD GetFileVersionInfoByHandle, n02
FORWARD GetFileVersionInfoExA, n03
FORWARD GetFileVersionInfoExW, n04
FORWARD GetFileVersionInfoSizeA, n05
FORWARD GetFileVersionInfoSizeExA, n06
FORWARD GetFileVersionInfoSizeExW, n07
FORWARD GetFileVersionInfoSizeW, n08
FORWARD GetFileVersionInfoW, n09
FORWARD VerFindFileA, n10
FORWARD VerFindFileW, n11
FORWARD VerInstallFileA, n12
FORWARD VerInstallFileW, n13
FORWARD VerLanguageNameA, n14
FORWARD VerLanguageNameW, n15
FORWARD VerQueryValueA, n16
FORWARD VerQueryValueW, n17
END
