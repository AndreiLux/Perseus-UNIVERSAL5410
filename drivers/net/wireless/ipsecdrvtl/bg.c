/*
   'src_ipsec_pgpIPheader.c' Obfuscated by COBF (Version 1.06 2006-01-07 by BB) at Mon Jun 10 09:48:00 2013
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
#pragma pack(push, 1)
bba bbi{bbf bb370[6 ];bbf bb1008[6 ];bbk bb375;}bb363, *bb378;bba bbi{
bbf bb442[6 ];bbk bb375;}bb1084, *bb1093;bba bbi{bbf bb936:4 ;bbf bb1090
:4 ;bbf bb1053;bbk bb372;bbk bb886;bbk bb573;bbf bb1003;bbf bb253;bbk
bb611;bbd bb311;bbd bb204;}bb325, *bb316;bba bbi{bbk bb1034;bbk bb1042
;bbf bb1055;bbf bb1043;bbk bb1064;bbf bb1083[6 ];bbd bb1038;bbf bb1037
[6 ];bbd bb1066;}bb1091, *bb1068;
#pragma pack(pop)
bba bbi{bbk bb291;bbk bb423;bbk bb1007;bbk bb319;}bb414, *bb347;bba
bbi{bbk bb291;bbk bb578;bbd bb540;bbd bb919;bbf bb92;bbf bb170;bbk
bb158;bbk bb319;bbk bb1021;}bb485, *bb318;bba bbi{bbf bb1080;bbf
bb1073;bbf bb1089;bbf bb1039;bbd bb1065;bbk bb1085;bbk bb374;bbd
bb1032;bbd bb1051;bbd bb1067;bbd bb1063;bbf bb1081[16 ];bbf bb1050[64 ]
;bbf bb25[128 ];bbf bb1033[64 ];}bb1082, *bb1076;bba bbi{bbd bb311;bbd
bb204;bbf bb917;bbf bb253;bbk bb913;}bb610, *bb571;
#if defined( _WIN32)
#define bb53( bbc) (((( bbc) & 0XFF00) >> 8) | ((( bbc) & 0X00FF) <<  \
8))
#define bb194( bbc) ( bb53( bbc))
#define bb445( bbc) (((( bbc) & 0XFF000000) >> 24) | ((( bbc) &  \
0X00FF0000) >> 8) | ((( bbc) & 0X0000FF00) << 8) | ((( bbc) &  \
0X000000FF) << 24))
#define bb493( bbc) ( bb445( bbc))
#endif
bbk bb928(bbh bbb*bb301);bbk bb849(bbh bbb*bb503,bbe bb21);bba bb83
bb4;bb10{bb98=0 ,bb361=-12000 ,bb345=-11999 ,bb377=-11998 ,bb674=-11997 ,
bb722=-11996 ,bb738=-11995 ,bb874=-11994 ,bb854=-11992 ,bb779=-11991 ,
bb804=-11990 ,bb750=-11989 ,bb838=-11988 ,bb639=-11987 ,bb683=-11986 ,
bb763=-11985 ,bb848=-11984 ,bb711=-11983 ,bb622=-11982 ,bb773=-11981 ,
bb895=-11980 ,bb684=-11979 ,bb714=-11978 ,bb844=-11977 ,bb586=-11976 ,
bb897=-11975 ,bb637=-11960 ,bb885=-11959 ,bb876=-11500 ,bb723=-11499 ,
bb670=-11498 ,bb810=-11497 ,bb696=-11496 ,bb755=-11495 ,bb720=-11494 ,
bb783=-11493 ,bb668=-11492 ,bb632=-11491 ,bb635=-11490 ,bb676=-11489 ,
bb680=-11488 ,bb700=-11487 ,bb840=-11486 ,bb889=-11485 ,bb649=-11484 ,
bb900=-11483 ,bb716=-11482 ,bb634=-11481 ,bb708=-11480 ,bb768=-11479 ,
bb648=-11478 ,bb730=-11477 ,bb774=-11476 ,bb769=-11475 ,bb866=-11474 ,
bb775=-11473 ,bb841=-11472 ,bb757=-11460 ,bb843=-11450 ,bb751=-11449 ,
bb717=-11448 ,bb726=-11447 ,bb864=-11446 ,bb629=-11445 ,bb826=-11444 ,
bb734=-11443 ,bb713=-11440 ,bb789=-11439 ,bb791=-11438 ,bb697=-11437 ,
bb707=-11436 ,bb673=-11435 ,bb630=-11420 ,bb531=-11419 ,bb567=-11418 ,
bb693=-11417 ,bb631=-11416 ,bb792=-11415 ,bb808=-11414 ,bb748=-11413 ,
bb633=-11412 ,bb830=-11411 ,bb681=-11410 ,bb781=-11409 ,bb715=-11408 ,
bb905=-11407 ,bb873=-11406 ,bb814=-11405 ,bb894=-11404 ,bb654=-11403 ,
bb770=-11402 ,bb846=-11401 ,bb642=-11400 ,bb879=-11399 ,bb855=-11398 ,
bb659=-11397 ,bb687=-11396 ,bb898=-11395 ,bb705=-11394 ,bb860=-11393 ,
bb811=-11392 ,bb878=-11391 ,bb888=-11390 ,bb743=-11389 ,bb790=-11388 ,
bb813=-11387 ,bb903=-11386 ,bb758=-11385 ,bb706=-11384 ,bb899=-11383 ,
bb645=-11382 ,bb908=-11381 ,bb640=-11380 ,bb795=-11379 ,bb623=-11378 ,
bb760=-11377 ,bb815=-11376 ,bb870=-11375 ,bb788=-11374 ,bb816=-11373 ,
bb688=-11372 ,bb865=-11371 ,bb812=-11370 ,bb620=-11369 ,bb712=-11368 ,
bb765=-11367 ,bb858=-11366 ,bb710=-11365 ,bb636=-11364 ,bb653=-11363 ,
bb387=-11350 ,bb884=bb387,bb721=-11349 ,bb662=-11348 ,bb835=-11347 ,bb646
=-11346 ,bb651=-11345 ,bb877=-11344 ,bb818=-11343 ,bb754=-11342 ,bb824=-
11341 ,bb764=-11340 ,bb902=-11339 ,bb397=-11338 ,bb724=-11337 ,bb701=bb397
,bb819=-11330 ,bb619=-11329 ,bb799=-11328 ,bb801=-11327 ,bb731=-11326 ,
bb762=-11325 ,bb772=-11324 ,bb672=-11320 ,bb638=-11319 ,bb678=-11318 ,
bb752=-11317 ,bb644=-11316 ,bb667=-11315 ,bb831=-11314 ,bb736=-11313 ,
bb776=-11312 ,bb881=-11300 ,bb787=-11299 ,bb749=-11298 ,bb709=-11297 ,
bb704=-11296 ,bb867=-11295 ,bb892=-11294 ,bb656=-11293 ,bb890=-11292 ,
bb906=-11291 ,bb618=-11290 ,bb727=-11289 ,bb809=-11288 ,bb725=-11287 ,
bb785=-11286 ,bb616=-11285 ,bb806=-11284 ,bb850=-11283 ,bb728=-11282 ,
bb679=-11281 ,bb807=-11280 ,bb685=-11279 ,bb753=-11250 ,bb832=-11249 ,
bb663=-11248 ,bb677=-11247 ,bb641=-11246 ,bb650=-11245 ,bb719=-11244 ,
bb761=-11243 ,bb621=-11242 ,bb820=-11240 ,bb655=-11239 ,bb745=-11238 ,
bb825=-11237 ,bb702=-11150 ,bb703=-11100 ,bb836=-11099 ,bb786=-11098 ,
bb871=-11097 ,bb735=-11096 ,bb802=-11095 ,bb643=-11094 ,bb617=-11093 ,
bb834=-11092 ,bb686=-11091 ,bb805=-11090 ,bb628=-11089 ,bb868=-11088 ,
bb661=-11087 ,bb901=-11086 ,bb767=-11085 ,bb793=-11050 ,bb759=-11049 ,
bb695=-10999 ,bb771=-10998 ,bb658=-10997 ,bb669=-10996 ,bb904=-10995 ,
bb893=-10994 ,bb699=-10993 ,bb817=-10992 ,bb780=-10991 ,bb627=-10990 ,
bb626=-10989 ,bb822=-10988 ,bb694=-10979 ,bb660=-10978 ,bb880=-10977 ,
bb875=-10976 ,bb744=-10975 ,bb742=-10974 ,};bba bbi bb449{bb1 bb74;bbd
bb125;bbd bb181;bbi bb449*bb93;}bbx;bb4 bb467(bbx*bb689,bbd bb909,bbx
 *bb837,bbd bb896,bbd bb536);bb4 bb526(bbx*bbj,bbd bb92,bbh bbb*bb95,
bbd bb47);bb4 bb563(bbx*bbj,bbd bb92,bbb*bb130,bbd bb47);bbu bb833(
bbx*bbj,bbd bb92,bbh bbb*bb95,bbd bb47);bb4 bb595(bbx*bb85,bbf bb103,
bbx*bb58);bb4 bb690(bbx*bb85,bbu bb177,bbf*bb404);bb4 bb951(bbx*bb58,
bbf*bb391);bb4 bb952(bbh bbf*bb391,bbx*bb58);bb4 bb537(bbx*bb51,bbf
bb103,bbd*bb941);bb4 bb924(bbx*bb85,bbf bb103,bbf bb404,bbx*bb58);bbd
bb518(bbx*bb51);bbk bb547(bbx*bb51);bbb bb522(bbk bb151,bbx*bb51);bbb
bb541(bbx*bb51);bbb bb968(bbx*bb51,bbd*bb27);bbb bb1000(bbx*bb51,bbd*
bb27);bbb bb994(bbx*bb51,bbd bb27);bbb bb921(bbx*bb51,bbd bb27);bbb
bb985(bbx*bb51);bbu bb1012(bbf*bb51);bb10{bb1129=-5000 ,bb1111=-4000 ,
bb999=-4999 ,bb1020=-4998 ,bb1031=-4997 ,bb983=-4996 ,bb1095=-4995 ,bb1088
=-4994 ,bb1102=-4993 ,bb1027=-4992 ,bb1086=-4991 };bb4 bb1134(bb4 bb1137,
bbd bb1120,bbl*bb1105);bba bb10{bb403,bb1505,}bb297;bbk bb1207(bb297
bb859,bbh bbf*bb453);bbd bb542(bb297 bb859,bbh bbf*bb453);bbb bb1166(
bbk bb159,bb297 bb550,bbf bb438[2 ]);bbb bb976(bbd bb159,bb297 bb550,
bbf bb438[4 ]);bbk bb928(bbh bbb*bb1840){bb59 bb301=(bb59)bb1840;bbd
bb589=0 ;bbk bbz=0 ;bb106(bbz++<(bb12(bb325)/2 )){bb589+= *bb301++;}
bb589=(bb589>>16 )+(bb589&0xffff );bb589+=(bb589>>16 );bb589=~bb589;bb2(
bbk)bb589;}bbk bb849(bbh bbb*bb503,bbe bb21){bb59 bb428=(bb59)bb503;
bbd bb589=0 ;bb106(bb21>1 ){bb589+= *bb428++;bbm(bb589&0x80000000 )bb589
=(bb589&0xffff )+(bb589>>16 );bb21-=2 ;}bbm(bb21)bb589+= * (bb1)bb428;
bb106(bb589>>16 )bb589=(bb589&0xffff )+(bb589>>16 );bb589=~bb589;bb2(bbk
)bb589;}bb4 bb595(bbx*bb85,bbf bb103,bbx*bb58){bbd bb1101;bb4 bb18;
bb1101=bb518(bb85);bb18=bb467(bb85,0 ,bb58,0 ,bb1101);bbm(((bb18)==bb98
))bb58->bb74[9 ]=bb103;bb2 bb18;}bb4 bb690(bbx*bb85,bbu bb177,bbf*
bb404){bb4 bb18=bb98;bbm(bb177) *bb404=4 ;bb54*bb404=bb85->bb74[9 ];bb2
bb18;}bb4 bb951(bbx*bb58,bbf*bb391){bbd bb1101;bbd bb163;bbd bb2128;
bb4 bb18=bb98;bb1101=bb518(bb58);bb81(bb391,bb58->bb74,bb1101);bb58->
bb74[1 ]=0 ;bb58->bb74[6 ]=0 ;bb58->bb74[6 +1 ]=0 ;bb58->bb74[8 ]=0 ;bb58->
bb74[10 ]=0 ;bb58->bb74[10 +1 ]=0 ;bbm(bb1101==20 )bb2 bb18;bb163=20 ;bb106(
bb58->bb74[bb163]!=0 ){bb300(bb58->bb74[bb163]){bb15 1 :bb163++;bb22;
bb15 130 :bb15 133 :bb15 134 :bb15 148 :bb15 149 :bb163++;bb163+=bb58->
bb74[bb163]+1 ;bb22;bb419:bb58->bb74[bb163]=0 ;bb2128=bb163+1 ;bb163+=2 ;
bb90(;bb163<=(bb2128+bb58->bb74[bb2128]);bb163++)bb58->bb74[bb163]=0 ;
bb58->bb74[bb2128]=0 ;bb22;}}bb2 bb18;}bb4 bb952(bbh bbf*bb391,bbx*
bb58){bbd bb1101;bb4 bb18=bb98;bb1101=bb518(bb58);bb81(bb58->bb74,
bb391,bb1101);bb2 bb18;}bb4 bb537(bbx*bb51,bbf bb103,bbd*bb941){bb4
bb18=bb98; *bb941=0 ;bbm(bb51->bb74[9 ]!=bb103)bb2 bb983; *bb941=bb518(
bb51);bb2 bb18;}bb4 bb924(bbx*bb85,bbf bb103,bbf bb404,bbx*bb58){bbd
bb1101;bb4 bb18;bbm(bb85->bb74[9 ]!=bb103)bb2 bb983;bb1101=bb518(bb85);
bb18=bb467(bb85,0 ,bb58,0 ,bb1101);bbm(((bb18)==bb98))bb58->bb74[9 ]=
bb404;bb2 bb18;}bbd bb518(bbx*bb51){bb2(bb51->bb74[0 ]&0x0F ) *4 ;}bbk
bb547(bbx*bb51){bb2 bb1207(bb403,&bb51->bb74[2 ]);}bbb bb522(bbk bb151
,bbx*bb51){bb1166(bb151,bb403,&bb51->bb74[2 ]);}bbb bb541(bbx*bb51){
bbd bb157;bbk bb319;bbd bbz;bbd bb1101;bb157=0 ;bb1101=bb518(bb51);
bb51->bb74[10 ]=0 ;bb51->bb74[10 +1 ]=0 ;bb90(bbz=0 ;bbz<bb1101;bbz+=2 ){
bb157+=(bb51->bb74[bbz]<<8 )|(bb51->bb74[bbz+1 ]);}bb319=~((bbk)bb157+(
bbk)(bb157>>16 ));bb1166(bb319,bb403,&bb51->bb74[10 ]);}bbb bb968(bbx*
bb51,bbd*bb27){bb81(bb27,&bb51->bb74[12 ],4 );}bbb bb1000(bbx*bb51,bbd*
bb27){bb81(bb27,&bb51->bb74[16 ],4 );}bbb bb994(bbx*bb51,bbd bb27){bb81
(&bb51->bb74[12 ],&bb27,4 );}bbb bb921(bbx*bb51,bbd bb27){bb81(&bb51->
bb74[16 ],&bb27,4 );}bbb bb985(bbx*bb51){bb51->bb74[6 ]&=0xBF ;}bbu bb1012
(bbf*bb51){bbm(bb51[6 ]&0x20 )bb2 1 ;bb54{bbm((bb51[6 ]&0x1F )||(bb51[6 +1 ]
))bb2 1 ;bb54 bb2 0 ;}}
