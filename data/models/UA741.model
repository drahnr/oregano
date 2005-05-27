.subckt UA741         1 2 3 4 5
* connections:        | | | | |
*                     | | | | |
*   non-inverting input | | | |
*         inverting input | | |
*     positive power supply | |
*       negative power supply |
*                        output
*
*
  c1   11 12 8.661E-12
  c2    6  7 30.00E-12
  dc    5 53 dx
  de   54  5 dx
  dlp  90 91 dx
  dln  92 90 dx
  dp    4  3 dx

*SPICE2:
*  egnd 99  0 poly(2) (3,0) (4,0) 0  .5  .5
*SPICE3:
  Begnd 99 0 V = .5*V(3,0) + .5*V(4,0) 

*SPICE2:
*  fb    7 99 poly(5) vb vc ve vlp vln 0 10.61E6 -10E6 10E6 10E6 -10E6
*SPICE3:
  Bfb 7 99 I = 10.61E6*I(vb) + -10E6*I(vc) + 10E6*I(ve) + 10E6*I(vlp) + -10E6*I(vln) 

  ga    6  0 11 12 188.5E-6
  gcm   0  6 10 99 5.961E-9
  iee  10  4 dc 15.16E-6
  hlim 90  0 vlim 1K
  q1   11  2 13 qx
  q2   12  1 14 qx
  r2    6  9 100.0E3
  rc1   3 11 5.305E3
  rc2   3 12 5.305E3
  re1  13 10 1.836E3
  re2  14 10 1.836E3
  ree  10 99 13.19E6
  ro1   8  5 50
  ro2   7 99 100
  rp    3  4 18.16E3
  vb    9  0 dc 0
  vc    3 53 dc 1
  ve   54  4 dc 1
  vlim  7  8 dc 0
  vlp  91  0 dc 40
  vln   0 92 dc 40
.model dx D(Is=800.0E-18 Rs=1)
.model qx NPN(Is=800.0E-18 Bf=93.75)
.ends UA741
