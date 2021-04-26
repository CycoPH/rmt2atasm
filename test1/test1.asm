;
; SFX
; example by raster/c.p.u., 2006,2009
;
;
	*=$2000
STEREOMODE	= 0				;0 => compile RMTplayer for mono 4 tracks
;								;1 => compile RMTplayer for stereo 8 tracks
;								;2 => compile RMTplayer for 4 tracks stereo L1 R2 R3 L4
;								;3 => compile RMTplayer for 4 tracks stereo L1 L2 R3 R4
;
;
	.include "../atasm/rmtplayr.asm"			;include RMT player routine
;
;
	*=$4000
	.include "tune.asm"
	.local
;
;
MODUL	= $4000				;address of RMT module
KEY		= $2fc				;keypressed code
RANDOM	= $d20a				;random value
;
	* = $6000
start
;
	ldx #<text
	ldy #>text
	jsr $c642					;print info text to screen
;
	lda #$f0					;initial value
	sta RMTSFXVOLUME			;sfx note volume * 16 (0,16,32,...,240)
;
	lda #$ff					;initial value
	sta sfx_effect
;
	ldx #<MODUL					;low byte of RMT module to X reg
	ldy #>MODUL					;hi byte of RMT module to Y reg
	lda #2						;starting song line 0-255 to A reg
	jsr RASTERMUSICTRACKER		;Init
;
	ldy #<vbi
	ldx #>vbi
	lda #$07
	jsr $e45c					;Start VBI routine
;
;
loop
	lda #255
	sta KEY						;no key pressed
;
waitkey
	lda KEY						;keycode
	cmp #255
	beq waitkey					;no key pressed
;
	and #63
	tay
	lda (121),y					;keycode -> ascii
;
	cmp #$31					; < key '1' ?
	bcc loop
	cmp #$39					; >= key '9' ?
	bcs loop
	and #$0f					;A=1..8
	pha							;sfx number
;
	lda RANDOM					;random number
	and #$07					;0..7
	ora #$08					;8..15
	asl
	asl
	asl
	asl							;*16
	sta RMTSFXVOLUME
;
	pla							;sfx number
	sta sfx_effect				;VBI routine is watching this variable
;
	jmp loop
;
;
;
vbi
;
	lda sfx_effect
	bmi lab2
	asl							; * 2
	tay							;Y = 2,4,..,16	instrument number * 2 (0,2,4,..,126)
	ldx #3						;X = 3			channel (0..3 or 0..7 for stereo module)
	lda #12						;A = 12			note (0..60)
	jsr RASTERMUSICTRACKER+15	;RMT_SFX start tone (It works only if FEAT_SFX is enabled !!!)
;
	lda #$ff
	sta sfx_effect				;reinit value
;
lab2
	jsr RASTERMUSICTRACKER+3	;1 play
;
	jmp $e462					;end vbi
;
;
;
sfx_effect .byte 0				;sfx number variable
;
text .byte "Press 1-8 for SFX (random volume)",$9b
;
;
;
	*=$2E0
	.word start					;run addr
;
;that's all... :-)