; CPU_TIME.INC
;******************************************************************************
;* Zeitmessmodul fuer TLCS 90                                                 *
;*                                                                            *
;* Originale fuer den TLCS 900 von Oli(ver Sellke)                            *
;* Implementierung TLCS 90 von Alfred Arnold, Dezember 1993                   *
;*                                                                            *
;*  Routine     Funktion                Eingabe Ausgabe  Stack    Laenge      *
;*                                                                            *
;* InitTimer    Timer initialisieren     ----    ----    0 Byte   6 Byte      *
;* StartTimer   Timer starten            ----    ----    0 Byte   7 Byte      *
;* StopTime     Timer stoppen & Ausgabe  ----    ----    6 Byte 117 Byte *)   *
;*                                                                            *
;*      *) bei hoher Aufloesung                                               *
;*                                                                            *
;* - Die Routinen benutzen Timer 4, dieser darf nicht von der Anwendung       *
;*   benutzt werden!!                                                         *
;* - Die Aufloesung kann ueber das Symbol BigTime umgeschaltet werden:        *
;*                                                                            *
;*       BigTime       Aufloesung    Maximalzeit                              *
;*       definiert      0,8 us       52,428 ms                                *
;*       undefiniert   12,8 us      838,848 ms                                *
;*                                                                            *
;* - Die Datei MON.INC wird fuer die Einspruenge des TDBTMP90-Monitors benoetigt.*
;* - MACROS.INC muss vorher eingebunden werden                                *
;*                                                                            *
;******************************************************************************

                section CPU_Time

;------------------------------------------------------------------------------
; Monitoreinsprungadressen laden

                include "mon.inc"

;------------------------------------------------------------------------------
; Timer vorinitialisieren

                proc    InitTimer
                ifdef   BigTime
                 ld      (T4MOD),00100010b ; nur Software-Capture, Phi16
                elseif
                 ld      (T4MOD),00100001b ; nur Software-Capture, Phi1
                endif
                set     5,(TRUN)        ; Vorteiler starten
                res     4,(TRUN)        ; Timer steht noch
                ret
                endp

;------------------------------------------------------------------------------
; Timer starten

                proc    StartTimer
                set     2,(T4MOD)       ; Timer loeschen
                res     2,(T4MOD)
                set     4,(TRUN)        ; Timer starten
                ret
                endp

;------------------------------------------------------------------------------
; Timer stoppen & Wert auf Konsole ausgeben

                subproc Div24

                ld      b,16            ; A-HL : B = HL und A
DivLoop:        sll     l               ; Divident hochschieben
                rl      h
                rla
                srl     h               ; fuer nachher
                rr      l
                sub     a,c             ; passt Divisor hinein ?
                jr      nc,DivOK
                add     a,c             ; nein, zuruecknehmen
                scf                     ; ins Ergebnis 0 einschieben
DivOK:          ccf                     ; neues Ergebnisbit
                rl      l               ; Ergebnis in HL einschieben
                rl      h
                djnz    DivLoop
                ret

                endp


                proc    StopTimer

                push    af              ; Register retten
                push    hl
                push    de
                push    bc

                res     5,(T4MOD)       ; Wert im CAP1 latchen
                res     4,(TRUN)        ; Timer wieder anhalten
                ld      hl,(CAP1L)      ; Wert auslesen

                ifdef   BigTime
                 ld     a,h             ; grosse Periode: x128...
                 ld     h,l
                 ld     l,0
                 srla
                 rr     h
                 rr     l
                 ld     c,100           ; ...Teilen durch 100
                elseif
                 ld     a,0             ; kleine Periode: x8...
                 rept   3
                  sll   l
                  rl    h
                  rla
                 endm
                 ld     c,10            ; ...Teilen durch 10
                endif
                call    Div24           ; Division durchfuehren
                ifdef   BigTime         ; Kommatrennung fuer grosse Periode
                 ld     a,0
                 call   Div24
                 ld     d,a             ; Rest merken
                endif
                ld      e,0             ; Zeichenzaehler nullen
                ld      c,10            ; Teilerfaktor konstant
InLoop:         ld      a,0             ; Erweiterung auf 24 Bit
                call    Div24           ; einmal teilen
                push    af              ; Zeichen auf LIFO merken
                inc     e               ; ein Zeichen mehr
                or      hl,hl           ; noch etwas uebrig ?
                jr      nz,InLoop
                ld      a,5             ; Leerzeichen fuer Ausrichtung
                sub     a,e             ; Zahl berechnen
                jr      z,OutErg        ; Nullschleife abfangen
                ld      b,a             ; ansonsten passende Zahl Leerzeichen
BlankLoop:      call    BLAUS           ; voranstellen
                djnz    BlankLoop
OutErg:         ld      b,e             ; jetzt die Zeichen ausgeben
OutLoop:        pop     af
                add     a,'0'
                call    CONOUT
                djnz    OutLoop
                ifdef   BigTime         ; bei grosser Aufloesung Nachkommastellen
                 ld     a,'.'
                 call   CONOUT
                 ld     l,d             ; Zehner/Einerzerlegung
                 ld     h,0
                 div    hl,c
                 add    hl,'00'
                 ld     a,l
                 call   CONOUT
                 ld     a,h
                 call   CONOUT
                 call   BLAUS
                 ld     a,'m'
                elseif
                 ld     a,' '
                 call   CONOUT
                 ld     a,'u'
                endif
                call    CONOUT          ; Ausgabe Einheit
                ld      a,'s'
                call    CONOUT

                pop     bc              ; Register zurueck
                pop     de
                pop     hl
                pop     af

                ret
                endp

;------------------------------------------------------------------------------
; gemeinsames Ende

                endsection
