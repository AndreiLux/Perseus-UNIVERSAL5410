/*
   'aes_xcbc.c' Obfuscated by COBF (Version 1.06 2006-01-07 by BB) at Mon Jun 10 09:48:00 2013
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
#ifdef __cplusplus
bbr"\x43"{
#endif
#ifdef __cplusplus
bbr"\x43"{
#endif
bba bbi{bbq bb447;bbd bb417[4 * (14 +1 )];}bb364;bbb bb1099(bb364*bbj,
bbh bbb*bb71,bbq bb143);bbb bb1736(bb364*bbj,bbh bbb*bb71,bbq bb143);
bbb bb1035(bb364*bbj,bbb*bb14,bbh bbb*bb5);bbb bb1775(bb364*bbj,bbb*
bb14,bbh bbb*bb5);
#ifdef __cplusplus
}
#endif
bba bbi{bb364 bb2117;bbq bb9;bbf bb102[16 ];bbf bb1926[16 ];bbf bb1928[
16 ];bbf bb1842[16 ];}bb939;bbb bb2042(bb939*bbj,bbh bbb*bb71,bbq bb143
);bbb bb2092(bb939*bbj,bbh bbb*bb5,bbq bb9);bbb bb2103(bb939*bbj,bbb*
bb14);
#ifdef __cplusplus
}
#endif
bbb bb2042(bb939*bbj,bbh bbb*bb71,bbq bb143){bb364 bb2147;bbf bb2181[
16 ];bbj->bb9=0 ;bb32(bb143==16 );bb1099(&bb2147,bb71,bb143);bb998(bbj->
bb1842,0 ,16 );bb998(bb2181,1 ,16 );bb1035(&bb2147,bb2181,bb2181);bb998(
bbj->bb1926,2 ,16 );bb1035(&bb2147,bbj->bb1926,bbj->bb1926);bb998(bbj->
bb1928,3 ,16 );bb1035(&bb2147,bbj->bb1928,bbj->bb1928);bb1099(&bbj->
bb2117,bb2181,bb143);}bb41 bbb bb1254(bb939*bbj,bbh bbf*bb5){bbq bbz;
bb90(bbz=0 ;bbz<16 ;bbz++)bbj->bb1842[bbz]^=bb5[bbz];bb1035(&bbj->
bb2117,bbj->bb1842,bbj->bb1842);}bbb bb2092(bb939*bbj,bbh bbb*bb498,
bbq bb9){bbh bbf*bb5=(bbh bbf* )bb498;bbq bb383=bbj->bb9?(bbj->bb9-1 )%
16 +1 :0 ;bbj->bb9+=bb9;bbm(bb383){bbq bb11=16 -bb383;bb81(bbj->bb102+
bb383,bb5,((bb9)<(bb11)?(bb9):(bb11)));bbm(bb9<=bb11)bb2;bb5+=bb11;
bb9-=bb11;bb1254(bbj,bbj->bb102);}bb90(;bb9>16 ;bb9-=16 ,bb5+=16 )bb1254
(bbj,bb5);bb81(bbj->bb102,bb5,bb9);}bbb bb2103(bb939*bbj,bbb*bb14){
bb1 bb3;bbq bbz,bb383=bbj->bb9?(bbj->bb9-1 )%16 +1 :0 ;bbm(bb383<16 ){bbj
->bb102[bb383++]=0x80 ;bb998(bbj->bb102+bb383,0 ,16 -bb383);bb3=bbj->
bb1928;}bb54 bb3=bbj->bb1926;bb90(bbz=0 ;bbz<16 ;bbz++)bbj->bb102[bbz]
^=bb3[bbz];bb1254(bbj,bbj->bb102);bb81(bb14,bbj->bb1842,16 );}
