* Vacuum tube triode 12ax7a
.SUBCKT 12AX7A A G K
Bca	ca  0	V=45+V(A,K)+95.43*V(G,K)
Bre	re  0	V=URAMP(V(A,K)/5)-URAMP(V(A,K)/5-1)
Baa	A   K	I=V(re)*1.147E-6*(URAMP(V(ca))^1.5)
Bgg	G   K	I=5E-6*(URAMP(V(G,K)+0.2)^1.5)
Cgk G  K  1.6P
Cgp G  A  1.7P
Cpk A  K  0.46P
.ENDS 12AX7A
