/*
   'src_compress_deflate_inftrees.c' Obfuscated by COBF (Version 1.06 2006-01-07 by BB) at Mon Jun 10 09:48:00 2013
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
((bb165* ,bb165* ,bb153* * ,bb153* * ,bb17));
#if ! defined( bb2249) && ! defined( bb136)
#define bb2249
#endif
bbi bb381{bbe bb442;};bb41 bbe bb1974 bbp((bb165* ,bb7,bb7,bbh bb165*
,bbh bb165* ,bb153* * ,bb165* ,bb153* ,bb7* ,bb165* ));bb41 bbh bb7
bb2372[31 ]={3 ,4 ,5 ,6 ,7 ,8 ,9 ,10 ,11 ,13 ,15 ,17 ,19 ,23 ,27 ,31 ,35 ,43 ,51 ,59 ,67 ,
83 ,99 ,115 ,131 ,163 ,195 ,227 ,258 ,0 ,0 };bb41 bbh bb7 bb2361[31 ]={0 ,0 ,0 ,0 ,0
,0 ,0 ,0 ,1 ,1 ,1 ,1 ,2 ,2 ,2 ,2 ,3 ,3 ,3 ,3 ,4 ,4 ,4 ,4 ,5 ,5 ,5 ,5 ,0 ,112 ,112 };bb41 bbh bb7
bb2407[30 ]={1 ,2 ,3 ,4 ,5 ,7 ,9 ,13 ,17 ,25 ,33 ,49 ,65 ,97 ,129 ,193 ,257 ,385 ,513 ,
769 ,1025 ,1537 ,2049 ,3073 ,4097 ,6145 ,8193 ,12289 ,16385 ,24577 };bb41 bbh bb7
bb2408[30 ]={0 ,0 ,0 ,0 ,1 ,1 ,2 ,2 ,3 ,3 ,4 ,4 ,5 ,5 ,6 ,6 ,7 ,7 ,8 ,8 ,9 ,9 ,10 ,10 ,11 ,11 ,
12 ,12 ,13 ,13 };bb41 bbe bb1974(bbn,bb11,bbg,bbt,bbw,bb45,bb97,bb1786,
bb1787,bb441)bb165*bbn;bb7 bb11;bb7 bbg;bbh bb165*bbt;bbh bb165*bbw;
bb153* *bb45;bb165*bb97;bb153*bb1786;bb7*bb1787;bb165*bb441;{bb7 bbc;
bb7 bbo[15 +1 ];bb7 bb20;bbe bb52;bbe bb42;bb930 bb7 bbz;bb930 bb7 bb76
;bb930 bbe bb3;bbe bb197;bb7 bb1167;bb930 bb165*bb28;bb153*bb84;bbi
bb1376 bb26={{{0 }},0 };bb153*bb315[15 ];bb930 bbe bbv;bb7 bb8[15 +1 ];
bb165*bb2085;bbe bb193;bb7 bby;bb28=bbo; *bb28++=0 ; *bb28++=0 ; *bb28
++=0 ; *bb28++=0 ; *bb28++=0 ; *bb28++=0 ; *bb28++=0 ; *bb28++=0 ; *bb28++=
0 ; *bb28++=0 ; *bb28++=0 ; *bb28++=0 ; *bb28++=0 ; *bb28++=0 ; *bb28++=0 ; *
bb28++=0 ;bb28=bbn;bbz=bb11;bb565{bbo[ *bb28++]++;}bb106(--bbz);bbm(
bbo[0 ]==bb11){ *bb45=(bb153* )0 ; *bb97=0 ;bb2 0 ;}bb197= *bb97;bb90(
bb76=1 ;bb76<=15 ;bb76++)bbm(bbo[bb76])bb22;bb3=bb76;bbm((bb7)bb197<
bb76)bb197=bb76;bb90(bbz=15 ;bbz;bbz--)bbm(bbo[bbz])bb22;bb52=bbz;bbm(
(bb7)bb197>bbz)bb197=bbz; *bb97=bb197;bb90(bb193=1 <<bb76;bb76<bbz;
bb76++,bb193<<=1 )bbm((bb193-=bbo[bb76])<0 )bb2(-3 );bbm((bb193-=bbo[bbz
])<0 )bb2(-3 );bbo[bbz]+=bb193;bb8[1 ]=bb76=0 ;bb28=bbo+1 ;bb2085=bb8+2 ;
bb106(--bbz){ *bb2085++=(bb76+= *bb28++);}bb28=bbn;bbz=0 ;bb565{bbm((
bb76= *bb28++)!=0 )bb441[bb8[bb76]++]=bbz;}bb106(++bbz<bb11);bb11=bb8[
bb52];bb8[0 ]=bbz=0 ;bb28=bb441;bb42=-1 ;bbv=-bb197;bb315[0 ]=(bb153* )0 ;
bb84=(bb153* )0 ;bby=0 ;bb90(;bb3<=bb52;bb3++){bbc=bbo[bb3];bb106(bbc--
){bb106(bb3>bbv+bb197){bb42++;bbv+=bb197;bby=bb52-bbv;bby=bby>(bb7)bb197
?(bb7)bb197:bby;bbm((bb20=1 <<(bb76=bb3-bbv))>bbc+1 ){bb20-=bbc+1 ;
bb2085=bbo+bb3;bbm(bb76<bby)bb106(++bb76<bby){bbm((bb20<<=1 )<= * ++
bb2085)bb22;bb20-= *bb2085;}}bby=1 <<bb76;bbm( *bb1787+bby>1440 )bb2(-4
);bb315[bb42]=bb84=bb1786+ *bb1787; *bb1787+=bby;bbm(bb42){bb8[bb42]=
bbz;bb26.bb508.bb509.bb959=(bb152)bb197;bb26.bb508.bb509.bb1171=(
bb152)bb76;bb76=bbz>>(bbv-bb197);bb26.bb603=(bb7)(bb84-bb315[bb42-1 ]-
bb76);bb315[bb42-1 ][bb76]=bb26;}bb54*bb45=bb84;}bb26.bb508.bb509.
bb959=(bb152)(bb3-bbv);bbm(bb28>=bb441+bb11)bb26.bb508.bb509.bb1171=
128 +64 ;bb54 bbm( *bb28<bbg){bb26.bb508.bb509.bb1171=(bb152)( *bb28<
256 ?0 :32 +64 );bb26.bb603= *bb28++;}bb54{bb26.bb508.bb509.bb1171=(bb152
)(bbw[ *bb28-bbg]+16 +64 );bb26.bb603=bbt[ *bb28++-bbg];}bb20=1 <<(bb3-
bbv);bb90(bb76=bbz>>bbv;bb76<bby;bb76+=bb20)bb84[bb76]=bb26;bb90(bb76
=1 <<(bb3-1 );bbz&bb76;bb76>>=1 )bbz^=bb76;bbz^=bb76;bb1167=(1 <<bbv)-1 ;
bb106((bbz&bb1167)!=bb8[bb42]){bb42--;bbv-=bb197;bb1167=(1 <<bbv)-1 ;}}
}bb2 bb193!=0 &&bb52!=1 ?(-5 ):0 ;}bbe bb2014(bbo,bb1723,bb1770,bb1786,
bby)bb165*bbo;bb165*bb1723;bb153* *bb1770;bb153*bb1786;bb17 bby;{bbe
bb26;bb7 bb1787=0 ;bb165*bb441;bbm((bb441=(bb165* )( * ((bby)->bb401))(
(bby)->bb110,(19 ),(bb12(bb7))))==0 )bb2(-4 );bb26=bb1974(bbo,19 ,19 ,(
bb165* )0 ,(bb165* )0 ,bb1770,bb1723,bb1786,&bb1787,bb441);bbm(bb26==(-
3 ))bby->bb320=(bbl* )"";bb54 bbm(bb26==(-5 )|| *bb1723==0 ){bby->bb320=
(bbl* )"";bb26=(-3 );}( * ((bby)->bb371))((bby)->bb110,(bb70)(bb441));
bb2 bb26;}bbe bb1997(bb2176,bb2393,bbo,bb55,bb961,bb1013,bb1026,
bb1786,bby)bb7 bb2176;bb7 bb2393;bb165*bbo;bb165*bb55;bb165*bb961;
bb153* *bb1013;bb153* *bb1026;bb153*bb1786;bb17 bby;{bbe bb26;bb7
bb1787=0 ;bb165*bb441;bbm((bb441=(bb165* )( * ((bby)->bb401))((bby)->
bb110,(288 ),(bb12(bb7))))==0 )bb2(-4 );bb26=bb1974(bbo,bb2176,257 ,
bb2372,bb2361,bb1013,bb55,bb1786,&bb1787,bb441);bbm(bb26!=0 || *bb55==
0 ){bbm(bb26==(-3 ))bby->bb320=(bbl* )"";bb54 bbm(bb26!=(-4 )){bby->
bb320=(bbl* )"";bb26=(-3 );}( * ((bby)->bb371))((bby)->bb110,(bb70)(
bb441));bb2 bb26;}bb26=bb1974(bbo+bb2176,bb2393,0 ,bb2407,bb2408,
bb1026,bb961,bb1786,&bb1787,bb441);bbm(bb26!=0 ||( *bb961==0 &&bb2176>
257 )){bbm(bb26==(-3 ))bby->bb320=(bbl* )"";bb54 bbm(bb26==(-5 )){bby->
bb320=(bbl* )"";bb26=(-3 );}bb54 bbm(bb26!=(-4 )){bby->bb320=(bbl* )"";
bb26=(-3 );}( * ((bby)->bb371))((bby)->bb110,(bb70)(bb441));bb2 bb26;}
( * ((bby)->bb371))((bby)->bb110,(bb70)(bb441));bb2 0 ;}
#ifdef bb2249
bb41 bbe bb2336=0 ;
#define bb2541 544
bb41 bb153 bb2428[bb2541];bb41 bb7 bb2129;bb41 bb7 bb2131;bb41 bb153*
bb2265;bb41 bb153*bb2268;
#else
bb41 bb7 bb2129=9 ;bb41 bb7 bb2131=5 ;bb41 bb153 bb2265[]={{{{96 ,7 }},
256 },{{{0 ,8 }},80 },{{{0 ,8 }},16 },{{{84 ,8 }},115 },{{{82 ,7 }},31 },{{{0 ,8 }},
112 },{{{0 ,8 }},48 },{{{0 ,9 }},192 },{{{80 ,7 }},10 },{{{0 ,8 }},96 },{{{0 ,8 }},
32 },{{{0 ,9 }},160 },{{{0 ,8 }},0 },{{{0 ,8 }},128 },{{{0 ,8 }},64 },{{{0 ,9 }},224
},{{{80 ,7 }},6 },{{{0 ,8 }},88 },{{{0 ,8 }},24 },{{{0 ,9 }},144 },{{{83 ,7 }},59 },
{{{0 ,8 }},120 },{{{0 ,8 }},56 },{{{0 ,9 }},208 },{{{81 ,7 }},17 },{{{0 ,8 }},104 },
{{{0 ,8 }},40 },{{{0 ,9 }},176 },{{{0 ,8 }},8 },{{{0 ,8 }},136 },{{{0 ,8 }},72 },{{{
0 ,9 }},240 },{{{80 ,7 }},4 },{{{0 ,8 }},84 },{{{0 ,8 }},20 },{{{85 ,8 }},227 },{{{
83 ,7 }},43 },{{{0 ,8 }},116 },{{{0 ,8 }},52 },{{{0 ,9 }},200 },{{{81 ,7 }},13 },{{{
0 ,8 }},100 },{{{0 ,8 }},36 },{{{0 ,9 }},168 },{{{0 ,8 }},4 },{{{0 ,8 }},132 },{{{0 ,
8 }},68 },{{{0 ,9 }},232 },{{{80 ,7 }},8 },{{{0 ,8 }},92 },{{{0 ,8 }},28 },{{{0 ,9 }}
,152 },{{{84 ,7 }},83 },{{{0 ,8 }},124 },{{{0 ,8 }},60 },{{{0 ,9 }},216 },{{{82 ,7 }
},23 },{{{0 ,8 }},108 },{{{0 ,8 }},44 },{{{0 ,9 }},184 },{{{0 ,8 }},12 },{{{0 ,8 }},
140 },{{{0 ,8 }},76 },{{{0 ,9 }},248 },{{{80 ,7 }},3 },{{{0 ,8 }},82 },{{{0 ,8 }},18
},{{{85 ,8 }},163 },{{{83 ,7 }},35 },{{{0 ,8 }},114 },{{{0 ,8 }},50 },{{{0 ,9 }},
196 },{{{81 ,7 }},11 },{{{0 ,8 }},98 },{{{0 ,8 }},34 },{{{0 ,9 }},164 },{{{0 ,8 }},2
},{{{0 ,8 }},130 },{{{0 ,8 }},66 },{{{0 ,9 }},228 },{{{80 ,7 }},7 },{{{0 ,8 }},90 },
{{{0 ,8 }},26 },{{{0 ,9 }},148 },{{{84 ,7 }},67 },{{{0 ,8 }},122 },{{{0 ,8 }},58 },{
{{0 ,9 }},212 },{{{82 ,7 }},19 },{{{0 ,8 }},106 },{{{0 ,8 }},42 },{{{0 ,9 }},180 },{
{{0 ,8 }},10 },{{{0 ,8 }},138 },{{{0 ,8 }},74 },{{{0 ,9 }},244 },{{{80 ,7 }},5 },{{{
0 ,8 }},86 },{{{0 ,8 }},22 },{{{192 ,8 }},0 },{{{83 ,7 }},51 },{{{0 ,8 }},118 },{{{0
,8 }},54 },{{{0 ,9 }},204 },{{{81 ,7 }},15 },{{{0 ,8 }},102 },{{{0 ,8 }},38 },{{{0 ,
9 }},172 },{{{0 ,8 }},6 },{{{0 ,8 }},134 },{{{0 ,8 }},70 },{{{0 ,9 }},236 },{{{80 ,7
}},9 },{{{0 ,8 }},94 },{{{0 ,8 }},30 },{{{0 ,9 }},156 },{{{84 ,7 }},99 },{{{0 ,8 }},
126 },{{{0 ,8 }},62 },{{{0 ,9 }},220 },{{{82 ,7 }},27 },{{{0 ,8 }},110 },{{{0 ,8 }},
46 },{{{0 ,9 }},188 },{{{0 ,8 }},14 },{{{0 ,8 }},142 },{{{0 ,8 }},78 },{{{0 ,9 }},
252 },{{{96 ,7 }},256 },{{{0 ,8 }},81 },{{{0 ,8 }},17 },{{{85 ,8 }},131 },{{{82 ,7 }
},31 },{{{0 ,8 }},113 },{{{0 ,8 }},49 },{{{0 ,9 }},194 },{{{80 ,7 }},10 },{{{0 ,8 }}
,97 },{{{0 ,8 }},33 },{{{0 ,9 }},162 },{{{0 ,8 }},1 },{{{0 ,8 }},129 },{{{0 ,8 }},65
},{{{0 ,9 }},226 },{{{80 ,7 }},6 },{{{0 ,8 }},89 },{{{0 ,8 }},25 },{{{0 ,9 }},146 },
{{{83 ,7 }},59 },{{{0 ,8 }},121 },{{{0 ,8 }},57 },{{{0 ,9 }},210 },{{{81 ,7 }},17 },
{{{0 ,8 }},105 },{{{0 ,8 }},41 },{{{0 ,9 }},178 },{{{0 ,8 }},9 },{{{0 ,8 }},137 },{{
{0 ,8 }},73 },{{{0 ,9 }},242 },{{{80 ,7 }},4 },{{{0 ,8 }},85 },{{{0 ,8 }},21 },{{{80
,8 }},258 },{{{83 ,7 }},43 },{{{0 ,8 }},117 },{{{0 ,8 }},53 },{{{0 ,9 }},202 },{{{
81 ,7 }},13 },{{{0 ,8 }},101 },{{{0 ,8 }},37 },{{{0 ,9 }},170 },{{{0 ,8 }},5 },{{{0 ,
8 }},133 },{{{0 ,8 }},69 },{{{0 ,9 }},234 },{{{80 ,7 }},8 },{{{0 ,8 }},93 },{{{0 ,8 }
},29 },{{{0 ,9 }},154 },{{{84 ,7 }},83 },{{{0 ,8 }},125 },{{{0 ,8 }},61 },{{{0 ,9 }}
,218 },{{{82 ,7 }},23 },{{{0 ,8 }},109 },{{{0 ,8 }},45 },{{{0 ,9 }},186 },{{{0 ,8 }}
,13 },{{{0 ,8 }},141 },{{{0 ,8 }},77 },{{{0 ,9 }},250 },{{{80 ,7 }},3 },{{{0 ,8 }},
83 },{{{0 ,8 }},19 },{{{85 ,8 }},195 },{{{83 ,7 }},35 },{{{0 ,8 }},115 },{{{0 ,8 }},
51 },{{{0 ,9 }},198 },{{{81 ,7 }},11 },{{{0 ,8 }},99 },{{{0 ,8 }},35 },{{{0 ,9 }},
166 },{{{0 ,8 }},3 },{{{0 ,8 }},131 },{{{0 ,8 }},67 },{{{0 ,9 }},230 },{{{80 ,7 }},7
},{{{0 ,8 }},91 },{{{0 ,8 }},27 },{{{0 ,9 }},150 },{{{84 ,7 }},67 },{{{0 ,8 }},123 }
,{{{0 ,8 }},59 },{{{0 ,9 }},214 },{{{82 ,7 }},19 },{{{0 ,8 }},107 },{{{0 ,8 }},43 },
{{{0 ,9 }},182 },{{{0 ,8 }},11 },{{{0 ,8 }},139 },{{{0 ,8 }},75 },{{{0 ,9 }},246 },{
{{80 ,7 }},5 },{{{0 ,8 }},87 },{{{0 ,8 }},23 },{{{192 ,8 }},0 },{{{83 ,7 }},51 },{{{
0 ,8 }},119 },{{{0 ,8 }},55 },{{{0 ,9 }},206 },{{{81 ,7 }},15 },{{{0 ,8 }},103 },{{{
0 ,8 }},39 },{{{0 ,9 }},174 },{{{0 ,8 }},7 },{{{0 ,8 }},135 },{{{0 ,8 }},71 },{{{0 ,9
}},238 },{{{80 ,7 }},9 },{{{0 ,8 }},95 },{{{0 ,8 }},31 },{{{0 ,9 }},158 },{{{84 ,7 }
},99 },{{{0 ,8 }},127 },{{{0 ,8 }},63 },{{{0 ,9 }},222 },{{{82 ,7 }},27 },{{{0 ,8 }}
,111 },{{{0 ,8 }},47 },{{{0 ,9 }},190 },{{{0 ,8 }},15 },{{{0 ,8 }},143 },{{{0 ,8 }},
79 },{{{0 ,9 }},254 },{{{96 ,7 }},256 },{{{0 ,8 }},80 },{{{0 ,8 }},16 },{{{84 ,8 }},
115 },{{{82 ,7 }},31 },{{{0 ,8 }},112 },{{{0 ,8 }},48 },{{{0 ,9 }},193 },{{{80 ,7 }}
,10 },{{{0 ,8 }},96 },{{{0 ,8 }},32 },{{{0 ,9 }},161 },{{{0 ,8 }},0 },{{{0 ,8 }},128
},{{{0 ,8 }},64 },{{{0 ,9 }},225 },{{{80 ,7 }},6 },{{{0 ,8 }},88 },{{{0 ,8 }},24 },{
{{0 ,9 }},145 },{{{83 ,7 }},59 },{{{0 ,8 }},120 },{{{0 ,8 }},56 },{{{0 ,9 }},209 },{
{{81 ,7 }},17 },{{{0 ,8 }},104 },{{{0 ,8 }},40 },{{{0 ,9 }},177 },{{{0 ,8 }},8 },{{{
0 ,8 }},136 },{{{0 ,8 }},72 },{{{0 ,9 }},241 },{{{80 ,7 }},4 },{{{0 ,8 }},84 },{{{0 ,
8 }},20 },{{{85 ,8 }},227 },{{{83 ,7 }},43 },{{{0 ,8 }},116 },{{{0 ,8 }},52 },{{{0 ,
9 }},201 },{{{81 ,7 }},13 },{{{0 ,8 }},100 },{{{0 ,8 }},36 },{{{0 ,9 }},169 },{{{0 ,
8 }},4 },{{{0 ,8 }},132 },{{{0 ,8 }},68 },{{{0 ,9 }},233 },{{{80 ,7 }},8 },{{{0 ,8 }}
,92 },{{{0 ,8 }},28 },{{{0 ,9 }},153 },{{{84 ,7 }},83 },{{{0 ,8 }},124 },{{{0 ,8 }},
60 },{{{0 ,9 }},217 },{{{82 ,7 }},23 },{{{0 ,8 }},108 },{{{0 ,8 }},44 },{{{0 ,9 }},
185 },{{{0 ,8 }},12 },{{{0 ,8 }},140 },{{{0 ,8 }},76 },{{{0 ,9 }},249 },{{{80 ,7 }},
3 },{{{0 ,8 }},82 },{{{0 ,8 }},18 },{{{85 ,8 }},163 },{{{83 ,7 }},35 },{{{0 ,8 }},
114 },{{{0 ,8 }},50 },{{{0 ,9 }},197 },{{{81 ,7 }},11 },{{{0 ,8 }},98 },{{{0 ,8 }},
34 },{{{0 ,9 }},165 },{{{0 ,8 }},2 },{{{0 ,8 }},130 },{{{0 ,8 }},66 },{{{0 ,9 }},229
},{{{80 ,7 }},7 },{{{0 ,8 }},90 },{{{0 ,8 }},26 },{{{0 ,9 }},149 },{{{84 ,7 }},67 },
{{{0 ,8 }},122 },{{{0 ,8 }},58 },{{{0 ,9 }},213 },{{{82 ,7 }},19 },{{{0 ,8 }},106 },
{{{0 ,8 }},42 },{{{0 ,9 }},181 },{{{0 ,8 }},10 },{{{0 ,8 }},138 },{{{0 ,8 }},74 },{{
{0 ,9 }},245 },{{{80 ,7 }},5 },{{{0 ,8 }},86 },{{{0 ,8 }},22 },{{{192 ,8 }},0 },{{{
83 ,7 }},51 },{{{0 ,8 }},118 },{{{0 ,8 }},54 },{{{0 ,9 }},205 },{{{81 ,7 }},15 },{{{
0 ,8 }},102 },{{{0 ,8 }},38 },{{{0 ,9 }},173 },{{{0 ,8 }},6 },{{{0 ,8 }},134 },{{{0 ,
8 }},70 },{{{0 ,9 }},237 },{{{80 ,7 }},9 },{{{0 ,8 }},94 },{{{0 ,8 }},30 },{{{0 ,9 }}
,157 },{{{84 ,7 }},99 },{{{0 ,8 }},126 },{{{0 ,8 }},62 },{{{0 ,9 }},221 },{{{82 ,7 }
},27 },{{{0 ,8 }},110 },{{{0 ,8 }},46 },{{{0 ,9 }},189 },{{{0 ,8 }},14 },{{{0 ,8 }},
142 },{{{0 ,8 }},78 },{{{0 ,9 }},253 },{{{96 ,7 }},256 },{{{0 ,8 }},81 },{{{0 ,8 }},
17 },{{{85 ,8 }},131 },{{{82 ,7 }},31 },{{{0 ,8 }},113 },{{{0 ,8 }},49 },{{{0 ,9 }},
195 },{{{80 ,7 }},10 },{{{0 ,8 }},97 },{{{0 ,8 }},33 },{{{0 ,9 }},163 },{{{0 ,8 }},1
},{{{0 ,8 }},129 },{{{0 ,8 }},65 },{{{0 ,9 }},227 },{{{80 ,7 }},6 },{{{0 ,8 }},89 },
{{{0 ,8 }},25 },{{{0 ,9 }},147 },{{{83 ,7 }},59 },{{{0 ,8 }},121 },{{{0 ,8 }},57 },{
{{0 ,9 }},211 },{{{81 ,7 }},17 },{{{0 ,8 }},105 },{{{0 ,8 }},41 },{{{0 ,9 }},179 },{
{{0 ,8 }},9 },{{{0 ,8 }},137 },{{{0 ,8 }},73 },{{{0 ,9 }},243 },{{{80 ,7 }},4 },{{{0
,8 }},85 },{{{0 ,8 }},21 },{{{80 ,8 }},258 },{{{83 ,7 }},43 },{{{0 ,8 }},117 },{{{0
,8 }},53 },{{{0 ,9 }},203 },{{{81 ,7 }},13 },{{{0 ,8 }},101 },{{{0 ,8 }},37 },{{{0 ,
9 }},171 },{{{0 ,8 }},5 },{{{0 ,8 }},133 },{{{0 ,8 }},69 },{{{0 ,9 }},235 },{{{80 ,7
}},8 },{{{0 ,8 }},93 },{{{0 ,8 }},29 },{{{0 ,9 }},155 },{{{84 ,7 }},83 },{{{0 ,8 }},
125 },{{{0 ,8 }},61 },{{{0 ,9 }},219 },{{{82 ,7 }},23 },{{{0 ,8 }},109 },{{{0 ,8 }},
45 },{{{0 ,9 }},187 },{{{0 ,8 }},13 },{{{0 ,8 }},141 },{{{0 ,8 }},77 },{{{0 ,9 }},
251 },{{{80 ,7 }},3 },{{{0 ,8 }},83 },{{{0 ,8 }},19 },{{{85 ,8 }},195 },{{{83 ,7 }},
35 },{{{0 ,8 }},115 },{{{0 ,8 }},51 },{{{0 ,9 }},199 },{{{81 ,7 }},11 },{{{0 ,8 }},
99 },{{{0 ,8 }},35 },{{{0 ,9 }},167 },{{{0 ,8 }},3 },{{{0 ,8 }},131 },{{{0 ,8 }},67 }
,{{{0 ,9 }},231 },{{{80 ,7 }},7 },{{{0 ,8 }},91 },{{{0 ,8 }},27 },{{{0 ,9 }},151 },{
{{84 ,7 }},67 },{{{0 ,8 }},123 },{{{0 ,8 }},59 },{{{0 ,9 }},215 },{{{82 ,7 }},19 },{
{{0 ,8 }},107 },{{{0 ,8 }},43 },{{{0 ,9 }},183 },{{{0 ,8 }},11 },{{{0 ,8 }},139 },{{
{0 ,8 }},75 },{{{0 ,9 }},247 },{{{80 ,7 }},5 },{{{0 ,8 }},87 },{{{0 ,8 }},23 },{{{
192 ,8 }},0 },{{{83 ,7 }},51 },{{{0 ,8 }},119 },{{{0 ,8 }},55 },{{{0 ,9 }},207 },{{{
81 ,7 }},15 },{{{0 ,8 }},103 },{{{0 ,8 }},39 },{{{0 ,9 }},175 },{{{0 ,8 }},7 },{{{0 ,
8 }},135 },{{{0 ,8 }},71 },{{{0 ,9 }},239 },{{{80 ,7 }},9 },{{{0 ,8 }},95 },{{{0 ,8 }
},31 },{{{0 ,9 }},159 },{{{84 ,7 }},99 },{{{0 ,8 }},127 },{{{0 ,8 }},63 },{{{0 ,9 }}
,223 },{{{82 ,7 }},27 },{{{0 ,8 }},111 },{{{0 ,8 }},47 },{{{0 ,9 }},191 },{{{0 ,8 }}
,15 },{{{0 ,8 }},143 },{{{0 ,8 }},79 },{{{0 ,9 }},255 }};bb41 bb153 bb2268[]={{
{{80 ,5 }},1 },{{{87 ,5 }},257 },{{{83 ,5 }},17 },{{{91 ,5 }},4097 },{{{81 ,5 }},5 }
,{{{89 ,5 }},1025 },{{{85 ,5 }},65 },{{{93 ,5 }},16385 },{{{80 ,5 }},3 },{{{88 ,5 }
},513 },{{{84 ,5 }},33 },{{{92 ,5 }},8193 },{{{82 ,5 }},9 },{{{90 ,5 }},2049 },{{{
86 ,5 }},129 },{{{192 ,5 }},24577 },{{{80 ,5 }},2 },{{{87 ,5 }},385 },{{{83 ,5 }},
25 },{{{91 ,5 }},6145 },{{{81 ,5 }},7 },{{{89 ,5 }},1537 },{{{85 ,5 }},97 },{{{93 ,
5 }},24577 },{{{80 ,5 }},4 },{{{88 ,5 }},769 },{{{84 ,5 }},49 },{{{92 ,5 }},12289 }
,{{{82 ,5 }},13 },{{{90 ,5 }},3073 },{{{86 ,5 }},193 },{{{192 ,5 }},24577 }};
#endif
bbe bb1967(bb55,bb961,bb1013,bb1026,bby)bb165*bb55;bb165*bb961;bb153*
 *bb1013;bb153* *bb1026;bb17 bby;{(bbb)bby;
#ifdef bb2249
bbm(!bb2336){bbe bb3;bb7 bb20=0 ;bb165*bbo;bb165*bb441;bbm((bbo=(bb165
 * )( * ((bby)->bb401))((bby)->bb110,(288 ),(bb12(bb7))))==0 )bb2(-4 );
bbm((bb441=(bb165* )( * ((bby)->bb401))((bby)->bb110,(288 ),(bb12(bb7))))==
0 ){( * ((bby)->bb371))((bby)->bb110,(bb70)(bbo));bb2(-4 );}bb90(bb3=0 ;
bb3<144 ;bb3++)bbo[bb3]=8 ;bb90(;bb3<256 ;bb3++)bbo[bb3]=9 ;bb90(;bb3<280
;bb3++)bbo[bb3]=7 ;bb90(;bb3<288 ;bb3++)bbo[bb3]=8 ;bb2129=9 ;bb1974(bbo,
288 ,257 ,bb2372,bb2361,&bb2265,&bb2129,bb2428,&bb20,bb441);bb90(bb3=0 ;
bb3<30 ;bb3++)bbo[bb3]=5 ;bb2131=5 ;bb1974(bbo,30 ,0 ,bb2407,bb2408,&
bb2268,&bb2131,bb2428,&bb20,bb441);( * ((bby)->bb371))((bby)->bb110,(
bb70)(bb441));( * ((bby)->bb371))((bby)->bb110,(bb70)(bbo));bb2336=1 ;
}
#endif
 *bb55=bb2129; *bb961=bb2131; *bb1013=bb2265; *bb1026=bb2268;bb2 0 ;}
