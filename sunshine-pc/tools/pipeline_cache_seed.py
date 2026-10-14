#!/usr/bin/env python3
"""Materialize Aurora's bundled initial_pipeline_cache.db next to sunshine.exe.

Aurora's seed_pipeline_cache() (extern/aurora/lib/gfx/pipeline_cache.cpp) opens
initial_pipeline_cache.db from SDL_GetBasePath() read-only on a cold user cache, merges the
device-independent GX/Clear PipelineConfig rows into the user's writable pipeline_cache.db, and
rebuild_pipeline_cache() pre-queues them on the background pipeline worker at boot. That eliminates
the stage-start partial-frame burst on a cold pipeline cache (484 rows: 481 GX version 13, 3 Clear
version 5; stage-start burst rows ordered first by first_frame_used).

Usage:
  python tools/pipeline_cache_seed.py <output/path/to/initial_pipeline_cache.db>
"""
import base64
import lzma
import os
import sqlite3
import struct
import sys

SEED_B85 = (
    "{Wp48S^xk9=GL@E0stWa8~^|S5YJf5^nfEcQC$FX06Z5#X9-"
    ";e;=DNs32`!<$;MEbP_Z=C=Cud4B;7v{G5Y5%uv1C#g{l~|#bDWp*M9|q6{zQUnAX<B5?rbKRmGNPa7l*gmFx^1YeD(?i#OG1Y$E"
    "EQN$c9;>a1Qyw*6`vS;^fPLBQE%Jg2tZ<msE<>Y?7x7c>AV4<_rm@zaud(zf+0=)fL4wddolo!Ns%Xm?>#YI*z4a(+J;+Rl&?T(j"
    "siI$Swi&S8%C5*X+2hGi<~SHe%&GccfZw#wCtm>b+^7Mp|a;H3e`EeHR~-OuBz=8XT@D@IXuaBhjWBE|bC`4Pb>-"
    "WiN87&xFZms$ygD#bR*j3^ine~Lq_h*j`ODEPCeaiAw<XelOS_KT$ColONUXRSc!cTQTYAirZe_4)jMA<0&kHY=m(pHe6!y9?YgO"
    "wkw3Wd}|&lp4b+A%pey2ncrXa>?U;7M|9m>E;?SD7aWQ2RN7sR1JE{_qX)p4acaciT2;skNMNyz5UddB?Q!$%K3N%Jnq0SjAS;WB"
    "tuinHwzofP;3F8|1>C0@c*G%CigpQ1-"
    "Btxfb2JyX2&ZWS0krzVMH0AYW}{WsMT%_IL^}nzf54rTtSEI$}XrjBAwZJ{+O==PdAi;YAMv$-"
    "GBtCLUjZ$hB}jJ+(BP(ysXN$`GAC!M2s5d29fG-LQf=BGG|G)66|`o6EY9`1+T<#@F@yc<SfCuq*;0()wZ097dVTW?ww;wA{Tgfk"
    "hBseN2xPq|I{?<k#r18Q5YKR?(I9AtI!4<d%x9dxATyt2-OUv{^sY8hk5S~$0qOkTckn%A+??@C0pY;I2J>j8Sh}qB_D@7Q8twV-"
    "FNOh^h1;vC4iE#xRX`u$1v8~6638?*k9kIz5Ut5C-"
    "6%aF)VChvm^Y4PH2k;nX=ziEgi0<hU#qZ&RjoU@>LsHQ3eC9e5eVp<IC2zBg>CNo<{h28;s&77V;N|+eA2Uoa`(BgjlK(gkl&WG>"
    "HbjmC{+iQ894ixwX2q!M$EB{FOZ6RNlI50F@YLGV@h_n-"
    "`7U(1$pyMxZz`0BKY<{og7S&cf)R0Sh?55deN2K)9YH%qk$2JAS0L%q0$bAiu%wsBG{lcgYb%Onto0{&(-"
    "$Gc>UoicXl`8TFjhaHsNYtm<KMZ723@1k4S1>qh4|&Ns8{kt&s>NH)X2F1zw>ipIh1kL&o1=Ouz>rSaQe@CsBJaL(pcBJX|uG?Pz"
    "FVtKW{842A$-490-?^f#I+6jEr9OgF_P5n@bXkc4pM@`BpurkvN>zxouAg0AZs{Q)zFqb9s&$zRCOTB05I9-"
    "E&j^O6zA4Xv%@Aa!Rc&PAsY6wKX%%1J4H@wRSCi@>TRWEetrxNUO{&}Lh-a2Dc-"
    "0hDP>Lv_l$%MGyN;b<jO&=Mdo2;A}*Y%bZx`(p}%jXVuDA;ssZ~<4QXUC0t^dhowL4S?X7vYotXG8bzCAx)gY&D~-"
    "GE(e<DwL)jx_uZHz13xC$dSZ*{X07QTRgaA;#%lO+Cx_5G4Eo4doz7x5LX|0tEH5^6dtXOfikYM3ule{+7q^YgJz7xu|e#Rr>r@w"
    "GlA^kSS2&F|LPU%tUgA$7>f{&8913})V6EJ*UxluTv`ZuVV5u+BKXc=mM(bjdq98Q^k3$!y&hY?tS`|XjeJWdWgW6qe%^kuH1akf"
    "F+?7Bd_JRmLmmyn8!)x3@_ai^*)speAN8k$D!uoFxYNU(DM1XHy82*vlAX5h-"
    "k{LN5Ex3XNvsJ_@(YiQoNrTI&Wbq!vTA_GK!aIr`e@9$sAd9J<1MoZ(WyHzNv*1ZCUQD|Xl?J;#+)GrZo-MpfH(~ln_GkjSj#%@i"
    "<{P|n_eB;jbVK$xT!F1qJ17KfpGL$clLwk^Y)**>0_Pc7j)fTw%|}p04YPZJw;A}_|jpE6;B9&rECXZ|0Gt_jWzuIz_WhP2>=HNB"
    "+DHMpR(Qh5FO+z&h611eNSL3y|PPG#z(|;pXs}rO$Q~t6#!F(j(lzREz?Ocwi<rAFpnuU`*)G^ahA*}*xUPQ(?763J?5-"
    "pX*TghvSa{>EPUh7$eFY?NI)<nC6}|sVUpgQ(nt6hZ&*l;h}!}Il{X-DH%9az2RV7@SQXqBVOz^-"
    "PyZ+D;mU5)4DaQHg*CG_JjC4%b#-"
    "7qOu&#8hmTz)pb?QpVy1g@i`rLH5F={@UgX}fY@EwO6)fG{fM`(bEvyr!{dc5QLiQs|VB$gIPk8nPMQ-{N$H?zGH=8{6A#Gzq3-"
    "Wv)J%b#+^HxNudx*^h;4hpvx)OyKm4nH_$fB?TKgLN8kb}ZfXu=cri9ssDSe-h=WaD*Bu10eOBlHGAqF>4aC3V@#ADGII<$-"
    "sDwXkwA>JbZ6&=QuQjCef;Qe{W+b=mVhFFF-{|IA240u(9kF~i-XO;2$dyCw5`ZRCK=sNI%+H$%w+;jZAdOm&2)jG_R4s{ks-je="
    "nS;L84{$Ee0|m_K4C*Im)nOakngAJqE|F0bTgzFg5C>}E|4%K~c}(e}#K&(S5_Q!yi0+Z@_!Cp4M2BOS#-"
    "4WAy%D)>55?qy;J_&qTCLcit^b7YmuFK19oQ(nkp{US*<qzwa-"
    "f%t@g^cck$s?|@Cj8&?)=q)nobkGj(tbDq&y)IdU?=UyjHAF02>R};lu@A_k*HHFymjC+?mtSpO!Y``LJkt-bug&oHLrpTwC~`(q"
    "p{3;k>na}b?L|U>2N71#%0|hGVrx7v_zNpN4(osE0zWoMnBAapY^_V?Otl7d|C<DVYt>Glj2m%~4^C4G3`?*<VImfHZ2Ie#Pv^vo"
    "E*X-"
    "P5%Tt88YP?Yp;B+vm%~;GgtTIb`CD=NPW30Hor1Jc9lE4Cs&_O`r_ZV$tAVQG;7D1XfnjUH&c;SXj4=minzyNBW3@X}y<~o+>{>d"
    "Et%HIX)I4YmxtXjQ?<mKTTG@~k_eqLvkn7ZOZz1%<c5}gQ3oY;2yHhXI@<<MQS!v@#Te<D;^PE^aofPpgr4KqgKmmShcz2VxbvYu"
    "X^HlfVcoq4^!2KJDDseK(671VWj<~ar_8zP9#mDspMs8}NJV6zd2Q`9Q<_riM^C<Z=HXuqSvEI7nke9Wa*Fqz#Auv*tir}bAx9il"
    "q3yVv^3G8Ls|2T&iNWbC{op~NLXn>2zqH^-"
    "!IMg{IjZcl%4&=_41^tsCc1@nBP3M($(O$omcx5o4+8`pvRb&f#LUyDXP*XFq`^3Zf>fODBlQj)szpfd<XgF4Ojfk|p;Z#Vc^?<?"
    "%2Y-;4gOwl91<Teu%2nA;njuKu6N0?i(H@IuPGnJB8fgl$x4%$Gk%30FKr#t>f?*t(qb7-"
    "4r^|l(sK}@jIc1B1TsY9x%#c+XFIa#bP{J4jI+{`Et1ZZX1+$7f=uChS2J|3?fqfCO?8Ijw=SAvggYTw9#0%WzRGKM~bDr%Ku(zY"
    "pIiK#UCF|sxM=;vFe@c;{u4&3H(P%ysi3f6=2t>W_YH-"
    "ncg6ru&SX1D}f|pr#dxVZHkFxSwAJ;3|>1Srzftd{<+Wk+r9MhfWIwW@A(ievy`LSHJ=!PlR-sdj7!1XXCC5`H}ddISkrL<9IKXS"
    "9U3Of45o;3d=hJ%j(J_V(f(rgW59eFv+N4%msGK*3yM*}&7px~yc@kzzdkdNP2Jb)m2d9X)Es#b#Wi2_UCAiRSr0!qMDRZDYo#Uk"
    "38n`=0*>+&prcW5`-z=4_=VSk0v^hsOl&eHtkb6$<8SJx&_#56pHn_|&W|3*b%%VwqOLl>zSBH+$Ym5y>x!agBF6;8kKJj-rcU+8"
    "+=41$b^hXi4kbk>kdMcw)yMu9w&w^G}mmL!blMygO_0k$3<xr9%&eD7t^H?vs%NFE9*EiD<4BJxrE`Fw4<X6$BZt6rl2b6#Tqi(j"
    "hZ?kgPv6YQvpjC|Dfk(3~rV^r{9Zp`Q51v|mV?hQ_SRE)mP=5kteyA}ODC~<}?$;C^eOG>S!FawV~i3~>tp-"
    "IoBD7E^oz8O@`#aS`qDre{jrBAN8LWQH3G;7U;=aSkUf$_Kp*iu|B>p0dM@JU(l00a5%jg<GCugVbl-"
    "Chb~($b##nc*sf7<Tsn5dPQC{!S3i11W(s%aGQRk$-ck17#6=`H=qwL6}01&zhzjG%6DGL64z@lvi-"
    "aCZl>G|3D*TQ%y?+;|8EOvfB`*xB&+~wIfQV39c$;3bpl!@BOI|!?@QZT7i<64nckX-"
    "raY5y6nW@=<qx#@gjFwYa(I6g(Oj_><3Dw*qoPiSVinR|KGd;o+r#2@kxIi{?#46pO#tyW#ZuP27>J-BgAM-VGFvqw}SI#m(G{xR"
    "E)^7j8lfB4YWyx8Aag96OD+NXdw3ZvFt#Vm^V?6#ZVk3cI|ZooRIs(VOKAO&7{(}r7+?Z^w}PPo;9KjMn2Sr-"
    "uM?*VPN$JQ5Zk^Y7e@Y<M$C}g8)^D3V*0515KS05T3L+k?%5}%S6gBn-rKmm^P8-"
    "zoVh@tm4Z^hz+3pU^$N&{4EG}Ubo2)<(dN0VU7Z$q#Ah&&o(j#9hEz?gwdP0)!=QI;UG7PEgMwfcU!tv(8hGraOJ2vpHk5;GxWrF"
    "vN!BlZdCWB!BgDFGBndEuyjUsI(q0^2G7FoKVnRl4f>lIPXoHj^p8JQgH1BCn~?GGnbu3S<j+G0L&eAU(Le*?o7k4%_7ivezV$;$"
    "5OJ$d7|B{9HS;Ka^XUs`?kRg;E{`@?w(Z8~EvEtvLy1FJn10MSKPb#QGw0NEhM{$q!_#mO4?19+q%`4%QXR`)gE@J%#R-"
    "~Q423E$`N5p6xjnV({it&5RU%7JFi1l6$~X_9g@v%hwckdq>?$PBYtG?^8`VWr3)ZHOp5bbPe(8{f=wNVru~lEl`%0F6<ocGek1I"
    "gTA<2(}?-J)JWq+@>4K(GZ04V^H;v28U?3iUZDkKY7gNohB(C>A%m-Ai-bE`t^GDq&xJMa;lyAg>nSjV;()Tt>D*B=jiIxB_AchK"
    "J+dSSl*7Kip9d=nRL0(R7hMt>}On_e&^Ulxjh@!<^NP=Go2O$9^kVi_IBpg<#32+n1Z@vonTz5Fbl33ra^hXG=v2C|VE+IdZ=3)T"
    "j0f|s|)WHVYdFMRMQ(}0akhcFHGYHlu!kPyRz#UY-x_(kRogiS&c{2nNhdPuD+X&>UFW-hD7<hUg$uk3W4j68_5#wJNO;Y4byS0~"
    "EMHFVgxH{YzGSkRTD!1`C0nd?#PdV`ZcIS6B&>)_nB2xO`6yr~E8>fri=B{J=Td26LHz^X#{1QqtR^G-"
    "4lnI{RJw?8T!GA!f{g1CXTf$K7vhc$7VjSJO^GFGs=hgl?0?>(yCPfw1=(;ew&?VL{csB!}gL^5P>ZM1F!bMsIcPGj&<OSKogJR)"
    "oD_MK8?(nJeTy!96)nWb>pP2n{@`WPv0zSH;7#~x91{&~%D_-_w3LOmJ?csIjBo`{*%CQ044`!H5Oo|iJH3VTavaq6V`YzCAx4l9"
    "}NN37i5@H>^oqUuN}S=?;28K4&!dpT%{<uiz$wGz89AJ%B&>|4BVYH=m(E5s)s=zXBqeRc!&OaIxb2bp!CjsDvjzgp(yl+`)o`9M"
    "J$^CaKTk<YEWJ-K;sO}O-{5lGBRqmLE#@qHM-"
    "^ev@z2+6jZT^H+wf(D;^fP?7B!POuS*?(wp0q^jnGD>g$CMlj*7t1`Vd&JJa(_J>fnwAT6LLHyQ^%aV9y04dIDch$&|Kn02#s=d6"
    ">Cc5TkY2uecIYant33Hpft4IqhJhU)Z9~EZ)7%D8j%>PJS%8vkwT(ST6628-;oFt;vvws%M%Z}IHic@oT96-^?|nzbgV|j2%`jB-"
    "jj;bdQ0vxFk^KMJyT02j(w5KRI)6aBN{BG&21u6x0O0P{mVuf4z?mQd`}0|(OMOZKg0wop151*yDHEsfCJk}F9{5hG3#RDL|HGoO"
    "Fha4-;5dv;YE~FX8}8y-"
    "<uowo+R~<mt@1~59)bKQE{WQh`)jD#Q=QnU`c@Bjh5R&6m3dHJSTNsn{1t7e0snDW$D1LA%14g`X=~|QhdWRHHObgLSVdJ_YWWHu"
    "VAXZ7bI+IuCLI&W))Sd~v%ElB3CJDi&18){m-wRdcv;dmBc`s%+|k(m9Gc&wD0UIPi)-"
    "ZnnnJLHbpAKTf_PPyz1oDwse$?NnbTVlj>`EB?8Y@pt`QJ9SAz_&7-"
    "N=DiTkfM;ZPS&#Q}=LoQq`2WO}{HCWZ+e3dDR85Kw$@d0BNMVzR4+{q(l$1o-"
    "$$zP$3$rm@uF<P$e{&*B>vm!JWl<dvpQzv|Fo$Z~k^kB~goF8a(%&}UM8aJzVIY<wHlB<QKyI1lKRnJtc8>nfp_ex>?tZbJ!gQYg"
    "&hf$tG@Ca*{Y3w!!9BexoQ22_!&z_{3@?GASK=$fP*LSO3zs4r-"
    "m797__MF^0ofWm#pzRl)6C6JlyNb)VV+^ad@CNGSr^{Wf<%{Pwhl#phYI2UP=7I1jOK0e5>n8kD{S$?K{f_hipMtUq9qDHlF3ZkK"
    "?aT$FWV(J6CCP%hw*^iHE*e#URHIIq<xL570quH9A(q0zIu94b5Tg}rF(zxB1u`*U|&yZiM@t=Gp;NzI<ux+vm)ja2YWDkqFeK5M"
    "rWJH%+9$~Uy9u8HL51jE(UzqzU3L4hrt;hMtdxD?l<$!p5r^ZF?A<3bkpcAb3i?Pm6!UCIi8J<6sauf=9^ah<^SZRysk6+MO&~pD"
    "PZ#lxiD*ESZ#F!Y^l#$)E-OMJJ{i)|EkBhmURFr7D#WReZpm(zQC<>~k>EBC)C^Qs?{=I_YR-"
    "Z{$hs7Gu!nU;u3y#LUjwNz5mb6T`ZpD5Hru>k4yecq<XC2N9o0bj?GmaK>uK@ZP)W)lVAmuCtj$C{|?@vb`LEi3i9ys5E*$Oh!oR"
    "|v;l*EgcrYY&$Yl^{RD`nYo%r!jKLUaTe;9&*MOSTt2sDD0_BRAXrY#W!Gnxl=5AC7)i#_3_*tXU+7@3JUhi&RQSU;B7ITH`4jM8"
    "m&)l)dp!BR-xAKJe{f=r3?o5=pM=Q4P?&lQ<COf#dD?+UxEPUPF3}Rc#I+Rz@4HilR&rag_g+^q9+bwyrEYkN~kaBbhoY|M`Va%x"
    "ambTS&Mf`l%!oug33@Yeqh48^YhK+a0JPTW;r=ds;EcY54R%h7QKtiboS%+W<_MDa<ekYbf$W^r(d|N8?My@5A|=jdeKPHrDBZXD"
    "t@e^|x+-"
    "vHDekal*L`S{zfYe~V%ZG^Y=H2{1`>BBfV*j8q)}VUS>Y{Z|VtwZP0NT4(~zdVA+mugdoYqXu%$HNE8UGdb7*yyWq3AMQP<1pZ?Q"
    "tRN)0Y!u7KOB`-7Bk7reJ6)yHaXbu7&$y3E77;^#k-"
    "%Wm^7)ZraJij3?GGnH3auSVqiiXgEi2AndzYie$Ls{rL}wzwssMsRd~xclzc*1+oSgoZ+v+ibjHTtlu8?MC|I<hb^xETUe#ygKga"
    "QcZi_|yxPw`k|27Lw;--xPo5WrE$j~KGJ0z_9pWI2WiA^0COzR-jsC|Z)p;ljB=m#=NbFgKUiQi_Lg!kdBXap@*r&2Re-EmXfp)("
    "9YW1$%-mM{b6VF*Uz(?I;|t{{+D&6GC0bSL4rbWAxSR;b^+7FJVWii^r$(PDf+R@-"
    "q)KmU;9qwCF^@>^MD4#zN*BlPdf}(xHW2seMY0n8Fq9if!^vC+_rb0uk)_HSsB#!TN{9L>|2|Dd!s7>VFw@4;e!YXz@uX&A8r0hS"
    "1-h+SndQ7yA^W6q#`FHSFaSj&>^nw)wt(^<Fncd)EU6!*SCqT1d)04Tw+m28)pwdQEKa;O0uP3Cx%hG~~3@5tf!Xnq7B4k@|d7-"
    "+(?zZYl^ToPxK!|LOl2TTDR^?hp)jEdOQ+j4kibU&dG*uS)m!vdh;Oo;09!>_s}W=fm-LbbHFq#1{P$TIZf^C~Sq+{7M8Z1HhQIV"
    "nTP#D+adt|LQ@p#b7dRD(a+gdV4d{ONR<BaT`SxNv?HH8Q{hWtt<@v(P{^%`LA75(dSD<x?UY`=5g(Bgqx`3xT^|$&Xl@HSA9wF@"
    "#baD^LeXm&Lbcze)?dVj}c%@?aBM*UmrNQwjMGFxj=H-"
    "VQZZ=0w@c!owvbji0UO|uIRgQg3EXr42XC+P&w{0oRYC+n&Gx}W_KaNBFvYLx6UHJ5P0sOMp*3Sz#VUj9Nq%l#*1BFBe(K(s8mOF"
    "@d&yeR(I&FSSHSH`!?8=h}A@eF`+Evq|>#|6Px+2-n@g;-rc8Z;WE-"
    "|PKw^{&~B6I_~?}~4jO&DZ8GMyTEK`+Do>G8H{M}Dtkxahc^6Ku)AmynYK_FFOs!qwmnL`^{8@AUe*F!-"
    ";2R)i=dd_gCrrh{a1XM49mWX>gSx|tPhX`Ux&T3rWvr~Z5&KiNpJ`$0*}USAhj9aX)R3!WfX$20Qd!EbYr$Pp1ia#81@<!pt7|Dn"
    "jAq3R$7SwLazo?W#xdu6SGhnb+4SF1%L`-kS^{u+`hpg3z^_i!lK?<KB)wkK>G7-Z+Y#PMf-OI5C2j1UHw|;zpV$8bAZ<3gAMSaM"
    "^MKFSnP~DQJdl5WwXlqrwt~_(=dTEG+xMNu83-"
    "pq*P3Yifj1QOe0u+`<`Xvo`;<bTl*XaFa}5qouQrcg_r4R}F_S+BoqRCNgirB+*xvHp^f-"
    "wVx%h18LfRSM7t)b0F!B{O{9E@42m@&%&u{)@bEd^YqtV}F%nAIkhM@dv*S9WHgQ073J$Kv6uJY1+IX!r%i&D~uh7KfB#NM+c#8o"
    "gKkS1^;V(v*s53tGXYc~(|{scBD4AazrzS1!Tegid?pF;=5yxCggHht_HM3aD#5PiUb@R(pfYw3wMIMek$m1bc2@Fg@|!5g5WG21"
    "sJlkR(4p!~tk${1AtOx~B3hF0b4R+CP6FYuB@3s`DSs|G{Z*-&oPJV5K@7&#8L#uxMKQhL6%yKzyh_FBO!0-"
    "zPF1PxAz$HbnenD8d7O~{s!=*}ctjIR3}jB-"
    "u$o|LFO_WsO!A*;Ls!u3`<l+zgUawU>!Su^hUduAK0|0L?p`wzm2+C(#59<bU*mtcX)6A;UWPwglor#sl@<l``NN&MDGCaY;G6}s"
    "^jqmkbqhG@UWK+mnDM>(j)^VaT=#}|qotDx)#0qIK6=!=W*bHst0CnRP9n!J$i=>&C-Xca?^XS-WTE$z}49zpqkJp!r2or4jV*lq"
    "?Rs&j%9K#xjZRIJ3p;EOvS!Icez>i#<Cj}+gCT!xw)pr%{V`qp+YyaQH3Qmo!aPzX9*?0}F#<ru5{x(-Oxu_g-"
    "%wf35r<CD@gP+0OP&Bs)$m<Di8ukqfgJ^X-qf)_(fCFU{mi#r^=g)|0mE4TrQmvCBYfpwA8b>ZQc331Odd4hR~@^xi-"
    "(cVB^;T|7!2wwOmhS;4!&iiSC7Wa&!1Oezj(fc(BRHjH{G=nusJ#uMEKp;Nb{5(aN5>QNKY1Yeis99Gd6q?UfDCTSl_>Wh$kYcAT"
    "FQ#ajMsB&ptBPtii=v8ekL+e;A?jl~Q~GVWiL{!(fSjJ$2o@d{g_p_B4eequy#`F*`YNYOeA`1l+DBy;H8__TjcnwK^=GA@`yk|#"
    "g}|ww<0?MFGRW13kT>lcH}OQ_Cmc=@q<9>T6tb%9mZ$Z$FhVbk>!g0PJv9y72qZ?C)+1<DG!Qp8%dP>FZY=9W1OeK<#x)%R9&6L|"
    "hnVAdrdeVWofn7hH#=?iShvIt(&noRA}BImYn7Xek8Gv=oQqkS);Z6OQg;aMWB|tQH>OdzFijZqofPQ2;TP!o(Ua57<4@2^A2%9W"
    "Ue%nXlma^z)=fk8KS&$_@1s;nU8vspW-"
    "q&_1w6trb+l0)`vDG&h9L6UASJgXie3atv$XcvxshqR*vMAuC;!qB65D)+`kFvDAMEd+4HTP)_3b1NJW`(DUk4|VHrk7kHbq~R11"
    "r730@iNX@G(d0FuGc(==ak$B6?JcI~L2D2cK{Wj*N~?<4LBvLL+2E@=(nX?0j&n%8{JB4OWLq>?cK%_kMSYA^d)3axOZvOwNigPM"
    "=n#l8*D?J6U##kMxtrO5k03a*`NR*dVoHmq`gSU^r_8X9YZZUA9l;!&RBoEmv>SqY78mAmc^JWoL#B__1krJLcP_X)M7yx#%{4eu"
    "32M>`M;Y_<12xp~z?wNjrBXW&=m6w{+dRy^cx7Ft@{?+P|#r(SEnKLc-"
    "E`(lAJ{F>1iXprH+P|514=w5n{j#2@lZ%}Al%_DL;svFgp1u&^S7IUV47tda8$F=WTDKTZu9UE?-"
    "+VUd#N!>(fZ?+~<D4B5cs*D$x~ole6baiQ|`C4_kbpt1?tXo-"
    "6fRBEFDkV?gkn2_FSIAQoq4#pTmcBXEFt?MHpD#Ie|^VGUZB{9w3iwq2-"
    "#B}#A<_cn*0MSSP(>(+aN2@GJdYn_)Qi>e(Er$gwm<Heqd!isUo57Q5^-"
    "VQaQw{UkM9k$!^fno8^2y`HE*cFdN3$u4i}|Ni7zd=WJ-#sxyS?6mnU?LccM82RRzytb(EKL-"
    "gAC1Sb!}Mfe39gcC5q0U+k;Z<>=a|`>pVxhdzoQ^hp6NygRaCaUk?DxQ-YqjnK?w!I0jGPNTa{yK7vtjtv+)@RT1TxQ4F$I7v+mL"
    "TyCB%UzSC$(jl~Tqf>rLXbp%bcSD@YST^rL|EeQ)*gBPA_xYU-"
    ")osFSGD1Q65UztLxybkyy?8^9>8;x~0($nBn57j1Eb7Gx47x?DbaN(rJrct^TENE@Pb^uW0AfTY1|5ZvoA1FoP;o61owRZFJ~duG"
    "Zg;L|k3~S$ciZ-R^(sMjb*y=#{f;jfwwhZbFcQ{85_u`5WUR}{$xMreQ*xtg>fU*@5Naq+-"
    "Zc7{V7**@x2#8S`pYFID3!)7Y3QKL<RcH=(4V3@5h^q2)TsReX4xZ0K<UR=0Z=*rfm0JQ`$(wGhA8Q$JcE*j)R)^-"
    "Y<bU|Wnzayge-cBywMPvk=S8GJZ;)GY*5)$PbGlRy>qbSztiBJB%>B4;s>;4D-P+fk-uH8A%&8AlOJhbA%JX0BeSxPjj&Rj-"
    "~;z}KY(@luDcYT!CBx*4=17pq29YZrJjRbG`Pa-3-empjySK-5&-"
    "Qet7)`>G1(e&p+nE)A}sSCY}<7Z?5Hv%C)S&fekm2Q2Fa@f0V9=SP*JqkyOW3pP~9uW>^vJ7;V%p*sF7`I>jFrQGA4eaeyKlb#=h"
    "vGSisJ?5tjL#Ci{y-HmJitRLJYOVMJB%^Ngqs5O%fh0eu~b9aC!Rxc3%JP+O;hec<I(*gjRFUgKGYoDNaW)-"
    "=BmCkD!e+DNkOKj|=`rpp#y-rIiZ8EVZDv66qznR_OI+^Owuyos(-7!JJ+^LKz}eZBoHZF>@%qz-"
    "{t%DsWGpqjQ)N@BclSPS4c#a^$u(b}7%FZo0qR`hjlOOZ76_i{o0FYkDm7;A;zx&CE#TGj<Ut1OA@8E3MM!8rJ=wt&|cnwbV8{VH"
    "iN-qPRbcO0Xv7_T0BKf%7sgDw=Tzv(io4>>J@iw=_<UW}71XzCRJO>$sb?{=d!qPXtqk1~i1h8%JFCH|_X9n+w-"
    "<mvKWCW_)e%pG`)^zbvAm`sSPSn_QX&}2|qD%M}y6KthA6XHiGhJfygWlefR;nR`#7yApmSZP|{_SV|YLi5^wi$zBx>+dYKzI1ku"
    "gNHGTalO!hICPY?6XY@M9y{Nu@Xt3M(SW^eY8KZhx1JrHppMC9mey|bqGX*HzgYfq=9BuWoo#yHUVGd^cgjam>{W3SEHmWU+Wlc("
    "CB-|jUY+)0S#6;EHo+v($=~v}Dh*8Kv&UoTa0_H<*4W8fRS-rPfeu0hd|#yYrV`Sw%(b=A##w^A+}EVoQQgn9N$`Vc-Pj*qa0!aW"
    "Ej%feD8+R4=03y^FD8&w)rh-"
    "q@6ib~<0Ka6dF!d_SUb`HIPBYbX6VZ>;GH?9_E;)01eL4X{1+(;qc>L_CM_lzngWh@k;ih%8DR`h?>xwHx;!AFV!f^6k(GTWFugI"
    "~6Dr)QSaOmYA@iO#a^oL6P>XS>Y}TYDRSrArM+t^kudPifB`jy%HEkcK%tQ59AXGEy!CNGLe|ulyO#{HN!Uk8!vryZi$(Dj>T;G4"
    "%9E1MJNaq1cUXn(GIc$70o!Y;ViZS%j(uGQffbX;>v!{c_ovU?mL>z3rw#97)t0D5*MBj@}s5g3@(L}2h>x3=&<haBxg&tg(lis3"
    "4jo4Ilx8&7VWm*@Y@1Ld9_XIC4evj+V5r2^=`U|c)>Tf7sYR)LY+x<^fiIalkO1pj|h0S7QaN!on&@ury4c2N*UXe5(=%dKCMhwE"
    "^+l&@v-"
    "pTX<L~jGW=h=*jpt0olxpv4ac^kAVj=L{h;N|+R5C=;AR0y8=5$Uy1TJ|~Pqf%or;o2|ve`eq^A5YY@SnaCiIZjIh>5#@#hAn6wh"
    "l&s7?KOCO#ThUwRl60X$y!hVoPj`(u0p&mzcBJ*1cao&U2KCb-IX5vPxO;eZVyFU@Sa$YnWb2gZWAo}gOu374Zcq5;>m$5jI>)s*"
    "OAvX<Pv8dE?mKK9Cxq=5Ird}j~y@py9&x0N-gQ3u2D%#ni+wL1vILYv-VIqb^TP$aZF`CCU`zW=XS=*DY%M-"
    "z>Hk;lQn7$>C*bN_H@snem&GYWD&Kn#E{^OgYP&c4}<Slp^~z}U^0%$Htvz4gomcl|C+Q+b%_(BM9TG^#Gu3w5|FPYd46q!GCFV-"
    "fNE@4lDk^T++yV~<f_&+j9lTUx}Wx_DR{x4Hq6D0&Pr!zh>)nkP)lsaNc%U-"
    "JD5De2OeO7kQrqkWeMO)<W#=RdoEHhC({UJd(bOHppyJvQ;8H-"
    "G4`YAv6jBomo_n&iTe+d7@qPSoI$XAg7f)lE7eKHrKQ4N>WbC@w9H?T_JKu6`&Hm#jm(z`Y{Vfmzusk<zJVovzEQ+oX|3`wjBHe3"
    "*lRgqN`)jNkKwf?3ESJ3qNGmuiY^%Tca(*5d&f)pSk~{xM_q>vQUV%G`^g+Ik7%cR5@Ds`1KHe;m+-"
    "m7=g+4KiCH)TbwcL*s?B*01b^?C;qqCf>dSnWV0|UC0R(QDw@7gV3lk0SnA-Qc*T$7jG&$+<3Bn&Ly0v%O<be^%Pq>Z0uTdbk7=1"
    "PB>IChbEY&V#Ea4FD-N<D;nohL_NBuV}+wA%F2DDUD;B>_e$lrz5OB=jg1_Ox91H(e5Ni7`?V?QbOYTc8Qjp><O-"
    "9O*4w&R{9OdoH+4d1}hx9H4*+_;Ir#s9EZ*WD#zj`NS(EglDrF%=T1g*V^6Vjp{>{D0GPtR!_J;EuAITv6Gr)%2MUZ9K@Xd}<{8d"
    "Sc9N%#fajNfB%v>UOGhg!e=*eeZ#9Xw?NKgSDw*Z}<zImm0;n&Wc?1JVsJiq>Ov53n@bK<}34@*VU)>$er^hod2U2PqN%uf^h$A6"
    "IbJ2(xyw*^?@;pVt2L;lq9VXfnQRTGlYDjvy~nkiHV>Cz$*%JLz+kP^APH)u&cmbHlDkl%WK~?3?C+rul`=lI|9~Uf*UcQuG(f9$"
    "`o^$sXGxj)54t0%Mh~d&k<1C6+A#X=Y&*$yjC&5N)cI{UF_H?Y97pz3uM!;d|xn(@R^?Ab>QE_-yeFApNAGvuB(eR5&?vJUTu+l?"
    "sGjFaeZ9RUxJ)~;{r2B@;+|Mc^2{=KG;Wq0$;z&<uiCwcLJQkafhd6J7ha^qzZw0Bhz1w4V1Pn(Po>w;w%yf-"
    "k%>X0~msHQZ1rKbkGN1Iy{yAx(>FJy)FxtFfUCgs4FYbZe+s+$I`ZTI-"
    "`(;wk*w>w}z_!(vvnOs4Bkw{;j669dmL{C#AKdYPd2hp}VvfIb7fy(7Y_b!alo8xP|jQ(;7F8ta40Ysbz$+vKo{b$bhx`gP~f5M^"
    "TkVzHew9O^|=dusRuC;{8_LJ;2R4-"
    "9;EMa61g?9NnHA>4jI=yJuSQ9l4|v>bq1BJ{r%&Wr@pdMjb%uHH#=iUsu>1JG_`Z31?=oMvJiDm+Dm6RHaNc%Y?I7#gZ8}7;G;)j"
    "cg#)J|`gAu_e0~qdr$o6IH9_LnTLXu1v%Fo;G+}0~Z7w(KDWmx;F1M_7)6c{ihsI@_M7v2S<P|zziBi@mSjoV1B};jDP?-"
    "cdz4V`)P02Ju?%V;CTjmA~Wo=8_D_pHBNo6lPMFeu42<~b#<UuIPj^t;orZg(X-"
    "C|&N_@5d;cf;@DHD8K9c8g;$v{eLjY$g9YtUsd`G3O+)9DD)_ixEKT}`q$GkvyZ>W3hMsWl2GXkB|77u4*WWrgo6+6it+W8c>SNH"
    "5);msG8AuN8cg0O>z&6PkhHqSRn`quzA9MZ=OJJy@>Ut<ma+p}xToW!oLW7`zs^wgps2kj%hDpD`KIe-"
    "v@SzOF^3@^H)py2Zz$3wj601geXC0-Rb06Zw!yU-GZp7J*hvXPOPjrlZQFNa0-^YUWRA?tZCL@DAmRKt!*S=~1-"
    "Msc4T58y$*)h4ksW$MHxobEnl&3?Q|H)vB(Fw7#GDp7hjN6MN-"
    "#ow@|YUbdO9x*JjiyA#&NS(lG>Vzn_<S5|Z&@C010)BTrpk1ToRsA7>s~05@Xvd8!WoLKGb(2RI@#X=~x|I=|a%0cQ(P5D2a-"
    "W686-K{~O9>)Y+Q^E9_i%-Z`Ve{g*F{OFj75_sNkb;go-nLVl?+0qo0M0tb@^o_6;cm?ja%_<ZfJZ4zyG>X__DcK)brXj7&}K2t@"
    "LaUls^+|UikZidK7nX+e`~Y_;9aq)#i!gL{VlojQ~f+w(A-"
    "5|FElO8>ZnOMpc?qiBUR&#S&fhyjoK|>gb2s2|f{UkEtwLWpqZO;c5cGdxLsJwKH!|ExUWT9B_38zi{@K0P4yZ1X~m>4?v9torJo"
    "MT1FxDM!Vd@heiW*i61C4jFUOh(5$o<Kqh;ypL_Jy=^fH|kx7S<&rGVP{9wRu3BZ3B%MowNH?DYLS=V&E3rNdKw-pU%#B_-HDAhg"
    "<do_0B(T4XZbfMGp{}Z{qh9YgAT|5|%QxbYx1QNklJ8U=s^QQ!u5i10$nd&(IOT^0`i%;q}xgU7EYQ91kTWkbCa$-"
    "m>OebWXV$fc!@&~lzTj;6mk)Fo!@n=@&Y0F9#vL)$Yp%?CL#20HXEZy@i4yI7xSog;{R~qot-@uz(V-"
    "4@)7l39*l`}((4^8)=&%$l!MN02P-f!*zyIYZzR$T!`+>J^Vxq5nd&wCK=ggY`d2gEzPpFgp`tZ6r6h}Zw_<Z-"
    "PzHu}*f4gE=*NIu<w+P+B|&W~vG#ks?w+k)9`j|;Iu2e`1;UH)UDP5R^ME8IY1E9xE%e5k}<+RK1kA~aqR1{@`PV)Dfr>U=IE0(D"
    "W@%f+5av`BbU4Ee8yTLg*8{|SqO)R(>69{fL>q|Rdd;s2gz`|_1<gBnGQt;|Q2fc$&>HW@DEkl52K^2BNoCJMJ6D?XYb64DtcBEv"
    "}<*7j;rcO&r!>3{2Zr>&BEM=*5BDfHp`y^ig?hGA3iA#MJA(TPfRb+U_tBsdzM{Dh|%AH}25j#7x@tKD%?8r2WrMSgdVBpEcH>+Q"
    "{(nz+bdrZ8MOvN)ip{(?j(IS^WKYI`yuVR+DUKZ79os&b@+wnyeT1BJmq6s!Ld4P8Rh9GlqXiQfGNd3{157C2P4ZZ%W}sTeGh@to"
    "4QG{=V{ywFI};{S9d!{ar$5N{xIe`=!&C79PkUz>WL=Cm?!o?C&Wo7y(FnR)h`2h*4LmESJfzIZ!8RW{K>BQl@Un}1DN?ci=V<1e"
    "z=H0>mb>=$(CwYFvL<!(fZega^B%Fn9W%88@uIWI6a2SN^t@Fn)}R6q(-"
    "=jiItK80VKth0<d#`4~M$YUAM<qhVPZ5o^DodAn{C*T&-FD`s_SBR)3Kllb;#NarN*SC&>;Yr+?#eZl-EEx;f5pF2CR=sJZ_ucLF"
    "m~zGNXa0m&w!v%n)W23ol_*}g%lyuykzb7J1^`rdKHfYh5c^gKO_tZ5K9b^B6dTlT@;OIr531}16bm+jkfPY7DaNGYy$_BY5qHJ}"
    ";_u?bkQ-"
    ";^2zmHV^FcoqKta=lSeZM!Y=G)$BF~>RgnSt~%vSIQG`Y?n?k{*1hDEHxLSsUHDyBZ9e^I;mxQRr@x}zc>(XXvXfO4N}O%*8gKir"
    "uS^ZHqc72&O&>`a`Gdd;LR&2vz)vF7Aou%K%6y%6u^H-wBO-"
    "f{S{lJUTP(6363X5TfTQ1^<W^T}q^#J6V&U6gn%U>zm98+p@vW$XLH&beLqpA(=eOlXu1*p<@fNqJn_Fh2_F^G?k0GYShL$_>loN"
    "vV(^b2L`=$D+3qx3l|dhl*{S8q4&U7FG{Jy!zlC{gYostk~3>D&XU*4l@Se(|$#H0fv0gx-"
    "3E$IBGqb!1g%qVkA8Rn`<@OtGlXoNWo#F`Wr!=ty4MLWw5exN5$KF>%TK?IvPTtl38}4eLcSOESNo)xpTW-"
    "h0LMWQyd=umOX2cGXX9ySQ1A5p;U=KA@PiA-V@+z^df~?vCqsWaPrwe^CFJoYhQ>sSxGqld+yZ!TdfcjnU`YaM-QTdNLVEZU`{V%"
    "_@RAQ%hk+Idb<UoTr!3YAQ+Xa-"
    "m(Q4VAU18qS*^Lywv=>W<dlK&qO&Hd%)KMMAQp>iG2g;J+`)IG#ttI=x17jNv>gt?~Y2$9X58Kh2Gq+t2G~?4L_bB@kOMT4_GBE@"
    "YSsK)O-a_Z@Q9F(g<=1PcBmJED+31z!Q#+&fC--3R5d?B6hfjUUNP7@^<9N)aYK4srW{2(#B?)DOG5P{M<YF^ubbpf9Jm)@!@|}-"
    "S^h{n{J{!aPw4J*^cIVPjcX=+Xa)?{13u=4rWjd-apgxE6~sOuLkVX0!l0nc788+XQ0-TWAyz!-"
    "z&`J{McO9<vUrX!3btr+Y{Wj9qBy^vNB2sd|_#g-"
    "eV@=ys0u(bQFgRfSO$rvTTfws(U!@7}_Tt=HEB|J1@HI$_^iGN<(T3QAk+mboLDw;(C0^Jw5vZ)#KewQegt{sdgSVo2}ORQ9@qJ+"
    "*W{hhcviYiisxZHU}_CUKGPqnS#dht)rO|w-Q$=Otuf6T7C&cRW?cE=VU}g-n-"
    "R<b$4|nPVt<0{L|hpR*b|Y0t{o2XMN_Q$uUFmWdpB%r<{-^<Ek8q&RGJY-i@9-epTtENMbz?@-"
    "jhlooJCI$}Do+4;)8sEdOyL?cVL3r)zu7t^xKSNG6Y3^f%kzYI>pqr=$&xuwhxb18azJdT&p;a*XUng-"
    "%9;^#N{&Vp&f>ZXZCoe|=h3Z|`=P{_|ED50dGb3h=7pRln3;I+xf}VO|9pl(OXYT3&>ez;f_<ry^NxNW@QVjs2JMjd_yAT0Hlt^J"
    "0Hi)rW_`i_8$j10!k#M(wwjWG7Q!W4YOgq>eRTw@s2Ujh`6Byur19`NpgPd@Bc}W{LV!te+v~ikR)Xx*moeZR=NkX$4;tV}sVY3Q"
    "rRo58?OZ5o26x><OP1iL{=s`~VZnit<v&99Fh29&g<E{I7H0e+kBln`nk1iCS@i8w{Gx8gdGe;<0f2+h|LeK4vZAEPMtBxg(QH8I"
    "vvB>WP?M3NE?9R@_)mwhz1wiTQiTxMeE-"
    "F0&Bsk2pkJ08x<ErMvc*z2&a}5XsMsaDAFA;tj*VtNRCFaaceX^T54#p@LUF>yeQ0NGYm!7jb*syI%K<@weI`kH?H7E~J@6_Akqf"
    "ThVgrX$9qmIQv?B`_IzBl(P_zo1DTyzk;x))x=@aXRPpXsLro|Z=^en+GOw3cF93Bca63uUCxMvq<*-eyxi(lQ-"
    "*dhHh)yUUr_itDce96pP>D%f&5!Y4Q^euEDo`LS`ZK$s7W`@Mk;qHvv^xz!OTYJlS-"
    "X3@p|~P8_Q@Mp@ny(l@@y_hBVb<6I>`uAr@(j?IZhFRV;4fA36wsp@7KzOd48{QcgBV#25LSo0Fw5aZTbMxFB^YtB44niFx&{qAV"
    "9dsT0V>G>oWQV{HyOqM&S{CRhN$y^76S;2g$EUk<5bL-(F2V72}Fw4CGSe*v}IO-RX(4^v#Vd&*kB73W)0Hyo{MCcOG4kk2;gTz{"
    "I8Fx=A6p3vH1Y0Rl#{BE>NyUzP6n!{_faLV1@heR}i;*z{pd~W}YbbXlDqHmA^P5tV9^xl0~bv3`j47m(|L2|P=XAA=ZyclMV`W;"
    "LF-"
    "3_uaSI3>=jxqTvB^XN>cmsJS*WP*fGNMVpr#ElXuHX5RdV|06871rY515T;=bID(qSaDIw0>=>H;8*^#J2cg!6U6OYM`Xr(~HYa4"
    ";A5iSH)&PX#W>Bvb~bJXRe$&uL2DxXg!wG;vp53I}BmVk-YaMpmIg#vraq@(xU{Qz*`=P_RE0~@kjC^C3r}rh33JGm*k?`B8X%<3"
    "-H^4Zbp=sSDfjA`_^{`?N|A_?zO#eaXCw7ME%cudIY`00%PA#gI~Fa?dDg$g%s2#?xZwwJI7R_Sq);tBS?Ne3y1yx@=+5j29J>zK"
    "v!eqw3YvcziuQB6O7b(_+g0e>6~qxMm2CfE_9D)4w}O3%)kEN!>8oupn!7SJ0qzTm$H?MQITyD%e}cH8nWBjQO12wTS;G66k@xGQ"
    "9+?wyPS!>nP4EuGCHNP=u_!-&E~*_G}vG6Z`U^HC}P0Ld?Yk4!ec-VBfdOF^Mmgh3TNoIpQF_%OKoJ^_BDaJqg=gR%qL?u!y_V$u"
    "L*Y(d3?u0=q9egLYe5}sZVI{l6*x`J+#0800000s6-4>(S#`500HfAq<~TYN|x+9vBYQl0ssI200dcD"
)


def write_seed_db(out_path: str) -> int:
    raw = lzma.decompress(base64.b85decode(SEED_B85))
    (count,) = struct.unpack_from("<I", raw, 0)
    pos = 4
    rows = []
    for _ in range(count):
        t, h, cv, cs, ffu = struct.unpack_from("<BqIII", raw, pos)
        pos += 21
        cfg = raw[pos : pos + cs]
        pos += cs
        rows.append((t, h, cv, cs, cfg, ffu))
    assert pos == len(raw), (pos, len(raw))

    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    tmp_path = out_path + ".tmp"
    if os.path.exists(tmp_path):
        os.remove(tmp_path)
    con = sqlite3.connect(tmp_path)
    con.executescript(
        """
        PRAGMA journal_mode=DELETE;
        CREATE TABLE aurora_schema(value INTEGER);
        INSERT INTO aurora_schema VALUES (1);
        CREATE TABLE pipeline_cache (
          type INTEGER NOT NULL,
          hash INTEGER NOT NULL,
          config_version INTEGER NOT NULL,
          config_size INTEGER NOT NULL,
          config BLOB NOT NULL,
          first_frame_used INTEGER NOT NULL,
          PRIMARY KEY (type, hash)
        );
        CREATE INDEX pipeline_cache_load_order_idx
          ON pipeline_cache(type, config_version, first_frame_used);
        """
    )
    con.executemany("INSERT INTO pipeline_cache VALUES (?, ?, ?, ?, ?, ?)", rows)
    con.commit()
    con.execute("VACUUM")
    con.close()
    os.replace(tmp_path, out_path)
    return len(rows)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: python tools/pipeline_cache_seed.py <out.db>")
    n = write_seed_db(sys.argv[1])
    print(f"initial_pipeline_cache.db: {n} rows ({os.path.getsize(sys.argv[1])} bytes) -> {sys.argv[1]}")
