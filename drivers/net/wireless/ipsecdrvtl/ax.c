/*
   'src_compress_deflate_inffast.c' Obfuscated by COBF (Version 1.06 2006-01-07 by BB) at Mon Jun 10 09:48:00 2013
*/
#include"cobf.h"
#ifdef _WIN32
#if defined( UNDER_CE) && defined( bb344) || ! defined( bb338)
#define bb340 1
#define bb336 1
#else
#define bb351 bb357
#define bb330 1
#define bb352 1
#endif
#define bb353 1
#include"uncobf.h"
#include<ndis.h>
#include"cobf.h"
#ifdef UNDER_CE
#include"uncobf.h"
#include<ndiswan.h>
#include"cobf.h"
#endif
#include"uncobf.h"
#include<stdio.h>
#include<basetsd.h>
#include"cobf.h"
bba bbs bbl bbf, *bb1;bba bbs bbe bbq, *bb94;bba bb135 bb124, *bb337;
bba bbs bbl bb39, *bb72;bba bbs bb135 bbk, *bb59;bba bbe bbu, *bb133;
bba bbh bbf*bb89;
#ifdef bb308
bba bbd bb60, *bb122;
#endif
#else
#include"uncobf.h"
#include<linux/module.h>
#include<linux/ctype.h>
#include<linux/time.h>
#include<linux/slab.h>
#include"cobf.h"
#ifndef bb116
#define bb116
#ifdef _WIN32
#include"uncobf.h"
#include<wtypes.h>
#include"cobf.h"
#else
#ifdef bb117
#include"uncobf.h"
#include<linux/types.h>
#include"cobf.h"
#else
#include"uncobf.h"
#include<stddef.h>
#include<sys/types.h>
#include"cobf.h"
#endif
#endif
#ifdef _WIN32
bba bb111 bb256;
#else
bba bbe bbu, *bb133, *bb279;
#define bb201 1
#define bb202 0
bba bb284 bb227, *bb217, *bb229;bba bbe bb263, *bb250, *bb287;bba bbs
bbq, *bb94, *bb289;bba bb6 bb223, *bb285;bba bbs bb6 bb214, *bb259;
bba bb6 bb118, *bb238;bba bbs bb6 bb64, *bb241;bba bb64 bb258, *bb228
;bba bb64 bb276, *bb255;bba bb118 bb111, *bb249;bba bb290 bb264;bba
bb208 bb124;bba bb271 bb83;bba bb115 bb114;bba bb115 bb274;
#ifdef bb226
bba bb235 bb39, *bb72;bba bb254 bbk, *bb59;bba bb248 bbd, *bb29;bba
bb270 bb56, *bb113;
#else
bba bb246 bb39, *bb72;bba bb257 bbk, *bb59;bba bb278 bbd, *bb29;bba
bb206 bb56, *bb113;
#endif
bba bb39 bbf, *bb1, *bb224;bba bbk bb243, *bb209, *bb221;bba bbk bb275
, *bb210, *bb245;bba bbd bb60, *bb122, *bb252;bba bb83 bb37, *bb267, *
bb240;bba bbd bb234, *bb211, *bb222;bba bb114 bb251, *bb269, *bb231;
bba bb56 bb225, *bb280, *bb273;
#define bb141 bbb
bba bbb*bb212, *bb77;bba bbh bbb*bb230;bba bbl bb207;bba bbl*bb232;
bba bbh bbl*bb82;
#if defined( bb117)
bba bbe bb112;
#endif
bba bb112 bb19;bba bb19*bb233;bba bbh bb19*bb188;
#if defined( bb283) || defined( bb236)
bba bb19 bb36;bba bb19 bb120;
#else
bba bbl bb36;bba bbs bbl bb120;
#endif
bba bbh bb36*bb262;bba bb36*bb268;bba bb60 bb266, *bb216;bba bbb*
bb107;bba bb107*bb237;
#define bb215( bb35) bbi bb35##__ { bbe bb219; }; bba bbi bb35##__  * \
 bb35
bba bbi{bb37 bb190,bb265,bb239,bb244;}bb272, *bb281, *bb261;bba bbi{
bb37 bb8,bb193;}bb292, *bb242, *bb277;bba bbi{bb37 bb218,bb247;}bb220
, *bb213, *bb260;
#endif
bba bbh bbf*bb89;
#endif
bba bbf bb101;
#define IN
#define OUT
#ifdef _DEBUG
#define bb146( bbc) bb32( bbc)
#else
#define bb146( bbc) ( bbb)( bbc)
#endif
bba bbe bb161, *bb173;
#define bb288 0
#define bb312 1
#define bb296 2
#define bb323 3
#define bb343 4
bba bbe bb349;bba bbb*bb121;
#endif
#ifdef _WIN32
#ifndef UNDER_CE
#define bb31 bb341
#define bb43 bb346
bba bbs bb6 bb31;bba bb6 bb43;
#endif
#else
#endif
#ifdef _WIN32
bbb*bb128(bb31 bb47);bbb bb109(bbb* );bbb*bb137(bb31 bb159,bb31 bb47);
#else
#define bb128( bbc) bb147(1, bbc, bb140)
#define bb109( bbc) bb331( bbc)
#define bb137( bbc, bbn) bb147( bbc, bbn, bb140)
#endif
#ifdef _WIN32
#define bb32( bbc) bb339( bbc)
#else
#ifdef _DEBUG
bbe bb144(bbh bbl*bb95,bbh bbl*bb25,bbs bb286);
#define bb32( bbc) ( bbb)(( bbc) || ( bb144(# bbc, __FILE__, __LINE__ \
)))
#else
#define bb32( bbc) (( bbb)0)
#endif
#endif
bb43 bb302(bb43*bb324);
#ifndef _WIN32
bbe bb328(bbh bbl*bbg);bbe bb321(bbh bbl*bb20,...);
#endif
#ifdef _WIN32
bba bb342 bb96;
#define bb139( bbc) bb354( bbc)
#define bb142( bbc) bb329( bbc)
#define bb134( bbc) bb348( bbc)
#define bb132( bbc) bb332( bbc)
#else
bba bb334 bb96;
#define bb139( bbc) ( bbb)(  *  bbc = bb356( bbc))
#define bb142( bbc) (( bbb)0)
#define bb134( bbc) bb333( bbc)
#define bb132( bbc) bb358( bbc)
#endif
#if ( defined( _WIN32) || defined( __WIN32__)) && ! defined( WIN32)
#define WIN32
#endif
#if defined( __GNUC__) || defined( WIN32) || defined( bb1226) ||  \
defined( bb1217)
#ifndef bb398
#define bb398
#endif
#endif
#if defined( __MSDOS__) && ! defined( bb169)
#define bb169
#endif
#if defined( bb169) && ! defined( bb398)
#define bb510
#endif
#ifdef bb169
#define bb1040
#endif
#if ( defined( bb169) || defined( bb1190) || defined( WIN32)) && !  \
defined( bb136)
#define bb136
#endif
#if defined( __STDC__) || defined( __cplusplus) || defined( bb1212)
#ifndef bb136
#define bb136
#endif
#endif
#ifndef bb136
#ifndef bbh
#define bbh
#endif
#endif
#if defined( __BORLANDC__) && ( __BORLANDC__ < 0x500)
#define bb1146
#endif
#ifndef bb205
#ifdef bb510
#define bb205 8
#else
#define bb205 9
#endif
#endif
#ifndef bbp
#ifdef bb136
#define bbp( bb406) bb406
#else
#define bbp( bb406) ()
#endif
#endif
bba bbf bb152;bba bbs bbe bb7;bba bbs bb6 bb24;bba bb152 bb30;bba bbl
bb439;bba bbe bb1141;bba bb7 bb165;bba bb24 bb167;
#ifdef bb136
bba bbb*bb70;bba bbb*bb182;
#else
bba bb152*bb70;bba bb152*bb182;
#endif
#ifdef __cplusplus
bbr"\x43"{
#endif
bba bb70( *bb505)bbp((bb70 bb110,bb7 bb496,bb7 bb47));bba bbb( *bb504
)bbp((bb70 bb110,bb70 bb1103));bbi bb381;bba bbi bb1187{bb30*bb127;
bb7 bb148;bb24 bb187;bb30*bb596;bb7 bb384;bb24 bb604;bbl*bb320;bbi
bb381*bb23;bb505 bb401;bb504 bb371;bb70 bb110;bbe bb969;bb24 bb367;
bb24 bb1152;}bb435;bba bb435*bb17;bbr bbh bbl*bb1158 bbp((bbb));bbr
bbe bb506 bbp((bb17 bb16,bbe bb174));bbr bbe bb943 bbp((bb17 bb16));
bbr bbe bb1054 bbp((bb17 bb16,bbe bb174));bbr bbe bb956 bbp((bb17 bb16
));bbr bbe bb1177 bbp((bb17 bb16,bbh bb30*bb424,bb7 bb431));bbr bbe
bb1150 bbp((bb17 bb130,bb17 bb184));bbr bbe bb1058 bbp((bb17 bb16));
bbr bbe bb1178 bbp((bb17 bb16,bbe bb123,bbe bb295));bbr bbe bb1149 bbp
((bb17 bb16,bbh bb30*bb424,bb7 bb431));bbr bbe bb1172 bbp((bb17 bb16));
bbr bbe bb1009 bbp((bb17 bb16));bbr bbe bb1151 bbp((bb30*bb130,bb167*
bb317,bbh bb30*bb184,bb24 bb326));bbr bbe bb1108 bbp((bb30*bb130,
bb167*bb317,bbh bb30*bb184,bb24 bb326,bbe bb123));bbr bbe bb1162 bbp(
(bb30*bb130,bb167*bb317,bbh bb30*bb184,bb24 bb326));bba bb182 bb34;
bbr bb34 bb1206 bbp((bbh bbl*bb1052,bbh bbl*bb57));bbr bb34 bb1208 bbp
((bbe bb472,bbh bbl*bb57));bbr bbe bb1214 bbp((bb34 bb25,bbe bb123,
bbe bb295));bbr bbe bb1191 bbp((bb34 bb25,bb182 bb40,bbs bb21));bbr
bbe bb1188 bbp((bb34 bb25,bbh bb182 bb40,bbs bb21));bbr bbe bb1218 bbp
((bb34 bb25,bbh bbl*bb1230,...));bbr bbe bb1194 bbp((bb34 bb25,bbh bbl
 *bbg));bbr bbl*bb1182 bbp((bb34 bb25,bbl*bb40,bbe bb21));bbr bbe
bb1192 bbp((bb34 bb25,bbe bbo));bbr bbe bb1183 bbp((bb34 bb25));bbr
bbe bb1185 bbp((bb34 bb25,bbe bb174));bbr bb6 bb1195 bbp((bb34 bb25,
bb6 bb92,bbe bb1202));bbr bbe bb1181 bbp((bb34 bb25));bbr bb6 bb1193
bbp((bb34 bb25));bbr bbe bb1198 bbp((bb34 bb25));bbr bbe bb1203 bbp((
bb34 bb25));bbr bbh bbl*bb1223 bbp((bb34 bb25,bbe*bb1180));bbr bb24
bb986 bbp((bb24 bb367,bbh bb30*bb40,bb7 bb21));bbr bb24 bb1174 bbp((
bb24 bb380,bbh bb30*bb40,bb7 bb21));bbr bbe bb1116 bbp((bb17 bb16,bbe
bb123,bbh bbl*bb189,bbe bb196));bbr bbe bb1127 bbp((bb17 bb16,bbh bbl
 *bb189,bbe bb196));bbr bbe bb1047 bbp((bb17 bb16,bbe bb123,bbe bb569
,bbe bb452,bbe bb958,bbe bb295,bbh bbl*bb189,bbe bb196));bbr bbe
bb1049 bbp((bb17 bb16,bbe bb452,bbh bbl*bb189,bbe bb196));bbr bbh bbl
 *bb1165 bbp((bbe bb18));bbr bbe bb1173 bbp((bb17 bby));bbr bbh bb167
 *bb1163 bbp((bbb));
#ifdef __cplusplus
}
#endif
#define bb963 1
#ifdef bb136
#if defined( bb1742)
#else
#endif
#endif
bba bbs bbl bb155;bba bb155 bb1205;bba bbs bb135 bb126;bba bb126 bb500
;bba bbs bb6 bb389;bbr bbh bbl*bb1071[10 ];
#if bb205 >= 8
#define bb803 8
#else
#define bb803 bb205
#endif
#ifdef bb169
#define bb421 0x00
#if defined( __TURBOC__) || defined( __BORLANDC__)
#if( __STDC__ == 1) && ( defined( bb1792) || defined( bb1771))
bbb bb938 bb1310(bbb*bb102);bbb*bb938 bb1344(bbs bb6 bb1740);
#else
#include"uncobf.h"
#include<alloc.h>
#include"cobf.h"
#endif
#else
#include"uncobf.h"
#include<malloc.h>
#include"cobf.h"
#endif
#endif
#ifdef WIN32
#define bb421 0x0b
#endif
#if ( defined( _MSC_VER) && ( _MSC_VER > 600))
#define bb1752( bb472, bb129) bb1788( bb472, bb129)
#endif
#ifndef bb421
#define bb421 0x03
#endif
#if defined( bb1549) && ! defined( _MSC_VER) && ! defined( bb1773)
#define bb963
#endif
bba bb24( *bb955)bbp((bb24 bb487,bbh bb30*bb40,bb7 bb21));bb70 bb1175
bbp((bb70 bb110,bbs bb496,bbs bb47));bbb bb1160 bbp((bb70 bb110,bb70
bb911));bba bbi bb1376 bb153;bbi bb1376{bb559{bbi{bb152 bb1171;bb152
bb959;}bb509;bb7 bb1551;}bb508;bb7 bb603;};bbr bbe bb2014 bbp((bb165*
,bb165* ,bb153* * ,bb153* ,bb17));bbr bbe bb1997 bbp((bb7,bb7,bb165* ,
bb165* ,bb165* ,bb153* * ,bb153* * ,bb153* ,bb17));bbr bbe bb1967 bbp
((bb165* ,bb165* ,bb153* * ,bb153* * ,bb17));bbi bb1106;bba bbi bb1106
bb304;bbr bb304*bb2011 bbp((bb17 bby,bb955 bbo,bb7 bbv));bbr bbe
bb1981 bbp((bb304* ,bb17,bbe));bbr bbb bb1791 bbp((bb304* ,bb17,bb167
 * ));bbr bbe bb1996 bbp((bb304* ,bb17));bbr bbb bb2024 bbp((bb304*
bbg,bbh bb30*bbt,bb7 bb11));bbr bbe bb1979 bbp((bb304*bbg));bbi bb1233
;bba bbi bb1233 bb741;bbr bb741*bb1968 bbp((bb7,bb7,bb153* ,bb153* ,
bb17));bbr bbe bb2069 bbp((bb304* ,bb17,bbe));bbr bbb bb1990 bbp((
bb741* ,bb17));bba bb10{bb1767,bb2026,bb2037,bb2074,bb2017,bb1978,
bb1959,bb1883,bb1785,bb920}bb1900;bbi bb1106{bb1900 bb57;bb559{bb7
bb190;bbi{bb7 bb989;bb7 bb163;bb165*bb1130;bb7 bb1723;bb153*bb1770;}
bb444;bbi{bb741*bb1761;}bb1751;}bb149;bb7 bb1885;bb7 bb365;bb24 bb366
;bb153*bb1806;bb30*bb158;bb30*bb443;bb30*bb369;bb30*bb199;bb955 bb1561
;bb24 bb487;};bb41 bbh bb7 bb1145[17 ]={0x0000 ,0x0001 ,0x0003 ,0x0007 ,
0x000f ,0x001f ,0x003f ,0x007f ,0x00ff ,0x01ff ,0x03ff ,0x07ff ,0x0fff ,0x1fff
,0x3fff ,0x7fff ,0xffff };bbr bbe bb388 bbp((bb304* ,bb17,bbe));bbi bb381
{bbe bb442;};bbr bbe bb2200 bbp((bb7,bb7,bb153* ,bb153* ,bb304* ,bb17
));bbi bb1233{bbe bb442;};bbe bb2200(bb55,bb961,bb1013,bb1026,bbg,bby
)bb7 bb55,bb961;bb153*bb1013;bb153*bb1026;bb304*bbg;bb17 bby;{bb153*
bb45;bb7 bbw;bb24 bbn;bb7 bb3;bb30*bb28;bb7 bb11;bb30*bb84;bb7 bb97;
bb7 bb2395;bb7 bb2397;bb7 bbo;bb7 bbt;bb30*bb26;{{bb28=bby->bb127;
bb11=bby->bb148;bbn=bbg->bb366;bb3=bbg->bb365;}{bb84=bbg->bb199;bb97=
(bb7)(bb7)(bb84<bbg->bb369?bbg->bb369-bb84-1 :bbg->bb443-bb84);}}
bb2395=bb1145[bb55];bb2397=bb1145[bb961];bb565{{bb106(bb3<(20 )){bbn|=
((bb24)(bb11--, *bb28++))<<bb3;bb3+=8 ;}}bbm((bbw=(bb45=bb1013+((bb7)bbn
&bb2395))->bb508.bb509.bb1171)==0 ){{bbn>>=(bb45->bb508.bb509.bb959);
bb3-=(bb45->bb508.bb509.bb959);}; *bb84++=(bb152)bb45->bb603;bb97--;
bb1690;}bb565{{bbn>>=(bb45->bb508.bb509.bb959);bb3-=(bb45->bb508.
bb509.bb959);}bbm(bbw&16 ){bbw&=15 ;bbo=bb45->bb603+((bb7)bbn&bb1145[
bbw]);{bbn>>=(bbw);bb3-=(bbw);};{bb106(bb3<(15 )){bbn|=((bb24)(bb11--,
 *bb28++))<<bb3;bb3+=8 ;}};bbw=(bb45=bb1026+((bb7)bbn&bb2397))->bb508.
bb509.bb1171;bb565{{bbn>>=(bb45->bb508.bb509.bb959);bb3-=(bb45->bb508
.bb509.bb959);}bbm(bbw&16 ){bbw&=15 ;{bb106(bb3<(bbw)){bbn|=((bb24)(
bb11--, *bb28++))<<bb3;bb3+=8 ;}}bbt=bb45->bb603+((bb7)bbn&bb1145[bbw]
);{bbn>>=(bbw);bb3-=(bbw);};bb97-=bbo;bbm((bb7)(bb84-bbg->bb158)>=bbt
){bb26=bb84-bbt; *bb84++= *bb26++;bbo--; *bb84++= *bb26++;bbo--;}bb54
{bbw=bbt-(bb7)(bb84-bbg->bb158);bb26=bbg->bb443-bbw;bbm(bbo>bbw){bbo
-=bbw;bb565{ *bb84++= *bb26++;}bb106(--bbw);bb26=bbg->bb158;}}bb565{ *
bb84++= *bb26++;}bb106(--bbo);bb22;}bb54 bbm((bbw&64 )==0 ){bb45+=bb45
->bb603;bbw=(bb45+=((bb7)bbn&bb1145[bbw]))->bb508.bb509.bb1171;}bb54{
bby->bb320=(bbl* )"";{bbo=bby->bb148-bb11;bbo=(bb3>>3 )<bbo?bb3>>3 :bbo
;bb11+=bbo;bb28-=bbo;bb3-=bbo<<3 ;}{{bbg->bb366=bbn;bbg->bb365=bb3;}{
bby->bb148=bb11;bby->bb187+=(bb24)(bb28-bby->bb127);bby->bb127=bb28;}
{bbg->bb199=bb84;}}bb2(-3 );}}bb106(1 );bb22;}bbm((bbw&64 )==0 ){bb45+=
bb45->bb603;bbm((bbw=(bb45+=((bb7)bbn&bb1145[bbw]))->bb508.bb509.
bb1171)==0 ){{bbn>>=(bb45->bb508.bb509.bb959);bb3-=(bb45->bb508.bb509.
bb959);}; *bb84++=(bb152)bb45->bb603;bb97--;bb22;}}bb54 bbm(bbw&32 ){;
{bbo=bby->bb148-bb11;bbo=(bb3>>3 )<bbo?bb3>>3 :bbo;bb11+=bbo;bb28-=bbo;
bb3-=bbo<<3 ;}{{bbg->bb366=bbn;bbg->bb365=bb3;}{bby->bb148=bb11;bby->
bb187+=(bb24)(bb28-bby->bb127);bby->bb127=bb28;}{bbg->bb199=bb84;}}
bb2 1 ;}bb54{bby->bb320=(bbl* )"";{bbo=bby->bb148-bb11;bbo=(bb3>>3 )<
bbo?bb3>>3 :bbo;bb11+=bbo;bb28-=bbo;bb3-=bbo<<3 ;}{{bbg->bb366=bbn;bbg
->bb365=bb3;}{bby->bb148=bb11;bby->bb187+=(bb24)(bb28-bby->bb127);bby
->bb127=bb28;}{bbg->bb199=bb84;}}bb2(-3 );}}bb106(1 );}bb106(bb97>=258
&&bb11>=10 );{bbo=bby->bb148-bb11;bbo=(bb3>>3 )<bbo?bb3>>3 :bbo;bb11+=
bbo;bb28-=bbo;bb3-=bbo<<3 ;}{{bbg->bb366=bbn;bbg->bb365=bb3;}{bby->
bb148=bb11;bby->bb187+=(bb24)(bb28-bby->bb127);bby->bb127=bb28;}{bbg
->bb199=bb84;}}bb2 0 ;}
